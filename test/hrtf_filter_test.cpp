#include "filters/soloud_hrtf_filter.h"
#include "soloud_bus.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <thread>
#include <vector>

using namespace SoLoud;

static void closeTo(float actual, float expected, float tolerance = 1e-5f)
{
    if (!std::isfinite(actual) || std::fabs(actual - expected) > tolerance) {
        std::fprintf(stderr, "Expected %.9g, got %.9g (tolerance %.9g)\n",
                     expected, actual, tolerance);
        std::abort();
    }
}

static void writeData(const char *path)
{
    std::ofstream file(path, std::ios::binary);
    assert(file);
    for (int a = 0; a < 27; ++a) {
        for (int e = 0; e < 52; ++e) {
            std::array<float, HRTF_IR_SIZE> left{}, right{};
            const int delay = e == 51 ? 20 : 20 + e;
            left[delay] = 2;
            right[delay + 8] = e == 51 ? 0.5f : 1.0f;
            file.write(reinterpret_cast<const char *>(left.data()), sizeof(left));
            file.write(reinterpret_cast<const char *>(right.data()), sizeof(right));
        }
    }
    assert(file);
}

static int peakAt(const std::array<float, HRTF_IR_SIZE> &ir)
{
    return (int)(std::max_element(ir.begin(), ir.end(),
        [](float a, float b) { return std::fabs(a) < std::fabs(b); }) - ir.begin());
}

static void positions(HrtfFilter &filter)
{
    const float xyz[][5] = {
        {0, 1, 0, 0, 0}, {0, -1, 0, 0, 180},
        {0, 0, 1, 0, 90}, {0, 0, -1, 0, -90},
        {0, -1, -1, 0, 225}, {1, 0, 0, 90, 0},
        {-1, 0, 0, -90, 0},
        {0, 0.00001f, -1, 0, -89.99943f},
        {0, -0.00001f, -1, 0, 269.99943f}
    };
    for (const auto &p : xyz) {
        float az, el;
        assert(hrtfPositionAngles(p[0], p[1], p[2], az, el));
        closeTo(az, p[3]);
        closeTo(el, p[4], 0.0001f);
    }
    float az = 12, el = 34;
    assert(!hrtfPositionAngles(0, 0, 0, az, el));
    closeTo(az, 12); closeTo(el, 34);
    std::array<float, HRTF_IR_SIZE> l{}, r{}, seamL{}, seamR{};
    const float directions[][2] = {{0, 29}, {180, 61}, {90, 45}, {-90, 20}};
    for (const auto &direction : directions) {
        filter.interpolate(0, direction[0], l.data(), r.data());
        assert(peakAt(l) == (int)direction[1]);
    }
    filter.interpolate(0, -90, l.data(), r.data());
    filter.interpolate(0, 270, seamL.data(), seamR.data());
    assert(l == seamL && r == seamR);
    closeTo(r[28], 0.375f);
    for (float offset : {0.001f, 0.1f, 1.0f}) {
        filter.interpolate(0, -90 + offset, l.data(), r.data());
        filter.interpolate(0, 270 - offset, seamL.data(), seamR.data());
        closeTo(l[20], seamL[20], 0.002f);
        if (offset == 0.001f) {
            for (int k = 0; k < HRTF_IR_SIZE; ++k) {
                closeTo(l[k], seamL[k], 1e-5f);
                closeTo(r[k], seamR[k], 1e-5f);
            }
        }
    }
    filter.interpolate(100, 0, l.data(), r.data());
    filter.interpolate(90, 0, seamL.data(), seamR.data());
    assert(l == seamL && r == seamR);
    closeTo(filter.getParamMax(HrtfFilter::ELEVATION), 270);
    closeTo(filter.getParamMin(HrtfFilter::ELEVATION), -90);
}

static void resampling(HrtfFilter &filter)
{
    const auto original = filter.tableForRate();
    assert(original->entries.size() == 27 * 52);
    for (float rate : {22050.0f, 48000.0f, 88200.0f}) {
        const auto table = filter.tableForRate(rate);
        assert(table == filter.tableForRate(rate));
        closeTo(table->sampleRate, rate);
        for (const auto &entry : table->entries) {
            assert(entry.left.size() == 200 && entry.right.size() == 200);
            float peak = 0;
            for (int k = 0; k < HRTF_IR_SIZE; ++k) {
                assert(std::isfinite(entry.left[k]) && std::isfinite(entry.right[k]));
                peak = std::max({peak, std::fabs(entry.left[k]), std::fabs(entry.right[k])});
                if (rate == 22050 && k >= 100) {
                    closeTo(entry.left[k], 0); closeTo(entry.right[k], 0);
                }
            }
            closeTo(peak, 1);
        }
        const auto &entry = table->entries[13 * 52 + 9];
        closeTo((float)peakAt(entry.left), 29 * rate / 44100, 0.51f);
        closeTo((float)peakAt(entry.right), 37 * rate / 44100, 0.51f);
        if (rate == 88200) closeTo(entry.right[74], 0.5f);
    }
    assert(!filter.tableForRate(-1));
    assert(!filter.tableForRate(std::numeric_limits<float>::quiet_NaN()));
}

static void cache(const char *path, HrtfFilter &first)
{
    HrtfFilter second;
    auto alias = (std::filesystem::path(path).parent_path() / "." /
                  std::filesystem::path(path).filename()).string();
    assert(second.loadKemarBinary(alias.c_str(), 44100, 48000) == 0);
    assert(second.tableForRate() == first.tableForRate());
    assert(second.tableForRate(48000) == first.tableForRate(48000));
    assert(second.tableForRate(48000) != second.tableForRate());
    std::array<std::shared_ptr<const HrirTable>, 6> results;
    std::vector<std::thread> workers;
    for (size_t i = 0; i < results.size(); ++i) workers.emplace_back([&, i] {
        HrtfFilter f;
        assert(f.loadKemarBinary(path) == 0);
        results[i] = f.tableForRate(32000);
    });
    for (auto &worker : workers) worker.join();
    for (const auto &result : results) assert(result == results[0]);
    HrtfFilter differentRate;
    assert(differentRate.loadKemarBinary(path, 48000) == 0);
    assert(differentRate.tableForRate() != first.tableForRate());
    assert(differentRate.tableForRate(32000) != results[0]);
    auto retained = first.tableForRate();
    assert(second.loadKemarBinary("nonexistent-hrtf-test.bin") != 0);
    assert(second.tableForRate() == retained);
    std::filesystem::remove(path);
    HrtfFilter afterRemoval;
    assert(afterRemoval.loadKemarBinary(path) == 0);
    assert(afterRemoval.tableForRate() == retained);
}

static void crossfades(HrtfFilter &filter)
{
    std::unique_ptr<FilterInstance> a(filter.createInstance()), b(filter.createInstance()),
        c(filter.createInstance()), moving(filter.createInstance());
    b->setFilterParameter(HrtfFilter::ELEVATION, 180);
    c->setFilterParameter(HrtfFilter::ELEVATION, 90);
    std::array<float, 2> ba{}, bb{}, bc{}, bm{};
    for (int n = 0; n < 1100; ++n) {
        if (n == 256) moving->setFilterParameter(HrtfFilter::ELEVATION, 180);
        if (n == 384) moving->setFilterParameter(HrtfFilter::ELEVATION, 90);
        const float input = std::sin(n * 0.37f);
        ba[0] = bb[0] = bc[0] = bm[0] = input;
        a->filter(ba.data(), 1, 1, 1, 44100, n / 44100.0);
        b->filter(bb.data(), 1, 1, 1, 44100, n / 44100.0);
        c->filter(bc.data(), 1, 1, 1, 44100, n / 44100.0);
        moving->filter(bm.data(), 1, 1, 1, 44100, n / 44100.0);
        for (int ear = 0; ear < 2; ++ear) {
            float expected = ba[ear];
            if (n >= 256 && n < 384) {
                const float t = (n - 255) / 512.0f;
                expected = (1 - t) * ba[ear] + t * bb[ear];
            } else if (n >= 384) {
                const float t = std::min(1.0f, (n - 383) / 512.0f);
                expected = (1 - t) * (0.75f * ba[ear] + 0.25f * bb[ear]) + t * bc[ear];
            }
            closeTo(bm[ear], expected, 2e-5f);
        }
    }
    std::unique_ptr<FilterInstance> seam(filter.createInstance()), below(filter.createInstance());
    seam->setFilterParameter(HrtfFilter::ELEVATION, -90);
    below->setFilterParameter(HrtfFilter::ELEVATION, -90);
    for (int n = 0; n < 600; ++n) {
        if (n == 256) seam->setFilterParameter(HrtfFilter::ELEVATION, 270);
        ba[0] = bb[0] = std::sin(n * 0.21f);
        seam->filter(ba.data(), 1, 1, 1, 44100, n / 44100.0);
        below->filter(bb.data(), 1, 1, 1, 44100, n / 44100.0);
        closeTo(ba[0], bb[0]); closeTo(ba[1], bb[1]);
    }
    std::unique_ptr<FilterInstance> rateVoice(filter.createInstance());
    HrtfFilter prepared;
    std::atomic_store(&prepared.mDataState->source, filter.tableForRate(48000));
    std::unique_ptr<FilterInstance> rateReference(prepared.createInstance());
    for (int n = 0; n < 256; ++n) {
        ba[0] = bb[0] = n == 0 ? 1 : 0;
        rateVoice->filter(ba.data(), 1, 1, 1, 48000, n / 48000.0);
        rateReference->filter(bb.data(), 1, 1, 1, 48000, n / 48000.0);
        closeTo(ba[0], bb[0]); closeTo(ba[1], bb[1]);
    }
    std::unique_ptr<FilterInstance> surviving;
    {
        HrtfFilter temporary;
        std::atomic_store(&temporary.mDataState->source, filter.tableForRate());
        surviving.reset(temporary.createInstance());
    }
    surviving->filter(ba.data(), 1, 1, 1, 44100, 0);
    assert(std::isfinite(ba[0]) && std::isfinite(ba[1]));
}

class SilentInstance : public AudioSourceInstance {
public:
    unsigned int getAudio(float *buffer, unsigned int samples, unsigned int) override {
        std::fill(buffer, buffer + samples, 0.0f); return samples;
    }
    bool hasEnded() override { return false; }
};
class SilentSource : public AudioSource {
public:
    AudioSourceInstance *createInstance() override { return new SilentInstance; }
};
class HalfCollider : public AudioCollider {
public:
    float collide(Soloud *, AudioSourceInstance3dData *, int) override { return 0.5f; }
};

static void panning()
{
    for (unsigned int channels : {2u, 6u, 8u}) {
        auto engine = std::make_unique<Soloud>();
        assert(engine->init(0, Soloud::NULLDRIVER, 48000, 2048, channels) == 0);
        SilentSource source;
        HalfCollider collider;
        source.set3dMinMaxDistance(1, 100);
        source.set3dAttenuation(AudioSource::INVERSE_DISTANCE, 1);
        source.set3dCollider(&collider);
        Bus bus;
        const handle busHandle = engine->play(bus);
        engine->set3dListenerParameters(2, 3, 4, 1, 0, 0, 0, 1, 0);
        engine->setSpeakerPosition(channels - 1, 0, 0, 0);
        for (handle routing : {0u, busHandle}) {
            const handle front = engine->play3d(source, 6, 3, 4, 0, 0, 0, 1, true, routing);
            const handle side = engine->play3d(source, 2, 3, 8, 0, 0, 0, 1, true, routing);
            const int f = engine->getVoiceFromHandle_internal(front);
            const int s = engine->getVoiceFromHandle_internal(side);
            assert(f >= 0 && s >= 0);
            assert(!(engine->mVoice[s]->mFlags & AudioSourceInstance::PAN_3D_DISABLED));
            engine->update3dAudio();
            std::array<float, MAX_CHANNELS> enabled{};
            std::copy_n(engine->mVoice[s]->mChannelVolume, MAX_CHANNELS, enabled.begin());
            engine->set3dSourcePanEnabled(side, false);
            engine->update3dAudio();
            closeTo(engine->m3dData[s].m3dVolume, 0.125f);
            bool changed = false;
            for (unsigned int j = 0; j < channels; ++j) {
                closeTo(engine->mVoice[s]->mChannelVolume[j], engine->mVoice[f]->mChannelVolume[j]);
                const float *speaker = engine->m3dSpeakerPosition + 3 * j;
                const float length = std::sqrt(speaker[0]*speaker[0] + speaker[1]*speaker[1] + speaker[2]*speaker[2]);
                const float gain = length == 0 ? 1 : (speaker[2] / length + 1) / 2;
                closeTo(engine->mVoice[s]->mChannelVolume[j], 0.125f * gain);
                changed |= std::fabs(enabled[j] - engine->mVoice[s]->mChannelVolume[j]) > 1e-5f;
            }
            assert(changed);
            engine->set3dSourceVelocity(side, 0, 0, -10);
            engine->update3dAudio();
            const float doppler = engine->m3dData[s].mDopplerValue;
            assert(std::fabs(doppler - 1.0f) > 1e-5f);
            engine->set3dSourcePanEnabled(side, true);
            engine->update3dAudio();
            closeTo(engine->m3dData[s].mDopplerValue, doppler);
            for (unsigned int j = 0; j < channels; ++j)
                closeTo(engine->mVoice[s]->mChannelVolume[j], enabled[j]);
            engine->set3dSourcePanEnabled(side, false);
            engine->stop(front); engine->stop(side);
        }
        engine->deinit();
    }
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    writeData(argv[1]);
    HrtfFilter filter;
    assert(filter.loadKemarBinary(argv[1], 44100, 48000) == 0);
    positions(filter);
    resampling(filter);
    cache(argv[1], filter);
    crossfades(filter);
    panning();
    std::puts("HRTF tests passed");
}
