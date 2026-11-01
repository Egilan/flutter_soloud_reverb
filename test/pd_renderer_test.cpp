#include "../src/synth/pd_renderer.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <thread>

static void check(bool condition, const char *message)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

struct Fixture {
    std::mutex mutex;
    unsigned int ticks = 0;
    unsigned int channels;
    PdRenderer renderer;

    explicit Fixture(unsigned int channelCount)
        : channels(channelCount), renderer(channels, 64, [this](float *output) {
            for (unsigned int i = 0; i < 64; ++i)
                for (unsigned int ch = 0; ch < channels; ++ch)
                    output[i * channels + ch] = static_cast<float>(ch * 1000000 + ticks * 64 + i);
            ++ticks;
        }, mutex)
    {
    }

    void read(PdRenderer::SourceId id, unsigned int firstChannel,
              unsigned int count, unsigned int frames, unsigned int start)
    {
        const unsigned int stride = frames + 17;
        std::vector<float> output(static_cast<size_t>(count) * stride + 2, -1.0f);
        check(renderer.read(id, output.data() + 1, frames, stride) == frames,
              "read fills requested frame count");
        check(output.front() == -1 && output.back() == -1, "output guards preserved");
        for (unsigned int ch = 0; ch < count; ++ch) {
            for (unsigned int i = 0; i < frames; ++i)
                check(output[1 + ch * stride + i] ==
                          static_cast<float>((firstChannel + ch) * 1000000 + start + i),
                      "channel slice and frame continuity");
            for (unsigned int i = frames; i < stride; ++i)
                check(output[1 + ch * stride + i] == -1, "channel stride padding preserved");
        }
    }
};

static void alternatingRanges()
{
    Fixture f(6);
    const auto dry = f.renderer.registerSource(0, 2);
    const auto wet = f.renderer.registerSource(2, 2);
    check(dry && wet, "two disjoint stereo ranges");
    unsigned int dryFrames = 0, wetFrames = 0;
    for (unsigned int i = 0; i < 80; ++i) {
        const auto count = (i % 3 + 1) * 64;
        if (i % 2 == 0) {
            f.read(dry, 0, 2, count, dryFrames);
            dryFrames += count;
        } else {
            f.read(wet, 2, 2, count, wetFrames);
            wetFrames += count;
        }
        check(f.ticks == (std::max)(dryFrames, wetFrames) / 64,
              "only the furthest reader advances the engine");
    }
}

static void partialRequestsAndMono()
{
    Fixture f(12);
    const auto stereo = f.renderer.registerSource(8, 2);
    const auto mono = f.renderer.registerSource(11, 1);
    unsigned int consumed = 0;
    for (const auto frames : {1u, 63u, 65u, 127u, 7u, 513u, 2049u}) {
        f.read(stereo, 8, 2, frames, consumed);
        const auto ticks = f.ticks;
        f.read(mono, 11, 1, frames, consumed);
        consumed += frames;
        check(f.ticks == ticks && ticks == (consumed + 63) / 64,
              "partial tick retained for both ranges");
    }
    check(f.renderer.read(stereo, nullptr, 0, 0) == 0, "zero length read");
}

static void lagAndLargeRequests()
{
    Fixture f(4);
    const auto leader = f.renderer.registerSource(0, 2);
    const auto lagger = f.renderer.registerSource(3, 1);
    f.read(leader, 0, 2, 12001, 0);
    check(f.ticks == (12001 + 63) / 64, "large request has no skipped leader frames");
    const auto oldest = f.ticks * 64 - PdRenderer::ringCapacity;
    f.read(lagger, 3, 1, 513, oldest);
    f.read(leader, 0, 2, 321, 12001);
    check(f.ticks == (12001 + 321 + 63) / 64, "lagger leaves leader unchanged");
    const auto newestOldest = f.ticks * 64 - PdRenderer::ringCapacity;
    f.read(lagger, 3, 1, 7000, (std::max)(oldest + 513, newestOldest));
}

static void validationAndRegistration()
{
    Fixture f(8);
    check(!f.renderer.registerSource(0, 0), "zero channels rejected");
    check(!f.renderer.registerSource(8, 1), "first channel outside outputs rejected");
    check(!f.renderer.registerSource(7, 2), "range exceeding outputs rejected");
    check(!f.renderer.registerSource(std::numeric_limits<unsigned int>::max(), 2),
          "offset overflow rejected");
    check(!f.renderer.registerSource(1, std::numeric_limits<unsigned int>::max()),
          "count overflow rejected");
    const auto range = f.renderer.registerSource(2, 2);
    check(range != 0, "valid range accepted");
    for (const auto first : {1u, 2u, 3u})
        check(!f.renderer.registerSource(first, 2), "overlap rejected");
    check(!f.renderer.registerSource(0, 8), "enclosing overlap rejected");
    const auto left = f.renderer.registerSource(0, 2);
    check(left && f.renderer.registerSource(4, 4), "adjacent ranges accepted");
    f.read(left, 0, 2, 65, 0);
    f.renderer.unregisterSource(range);
    float untouched = -1;
    check(f.renderer.read(range, &untouched, 1, 1) == 0 && untouched == -1,
          "unregistered source cannot read");
    const auto replacement = f.renderer.registerSource(2, 2);
    check(replacement && replacement != range, "unregister releases range with fresh id");
    f.read(replacement, 2, 2, 100, 128);
}

static void sharedMutexAndConcurrentRegistration()
{
    std::mutex mutex;
    std::atomic<unsigned int> entered{0};
    std::atomic<bool> start{false};
    PdRenderer renderer(2, 64, [&](float *output) {
        check(entered.fetch_add(1) == 0, "processing excludes bridge control");
        std::fill(output, output + 128, 0.5f);
        check(entered.fetch_sub(1) == 1, "processing remains exclusive");
    }, mutex);
    const auto source = renderer.registerSource(0, 1);
    std::thread control([&]() {
        while (!start.load()) std::this_thread::yield();
        for (unsigned int i = 0; i < 1000; ++i) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                check(entered.fetch_add(1) == 0, "bridge control excludes processing");
                std::this_thread::yield();
                check(entered.fetch_sub(1) == 1, "bridge control remains exclusive");
            }
            const auto other = renderer.registerSource(1, 1);
            check(other != 0, "concurrent registration");
            renderer.unregisterSource(other);
        }
    });
    start = true;
    for (unsigned int i = 0; i < 1000; ++i) {
        float output[127];
        check(renderer.read(source, output, 127, 127) == 127, "concurrent full read");
        for (float sample : output) check(sample == 0.5f, "concurrent output");
    }
    control.join();
}

int main()
{
    alternatingRanges();
    partialRequestsAndMono();
    lagAndLargeRequests();
    validationAndRegistration();
    sharedMutexAndConcurrentRegistration();
    std::puts("PASS: PD renderer (alternation, partials, mono, lag, ranges, threading)");
}
