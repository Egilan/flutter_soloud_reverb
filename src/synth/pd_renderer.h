#ifndef PD_RENDERER_H
#define PD_RENDERER_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

/**
 * One shared engine timeline, with zero-based, non-overlapping source ranges.
 * Each output channel retains 4096 frames; a lagging reader drops frames older
 * than that window. New sources start at the current engine write position.
 * Reads copy planar audio using the caller's channel stride, including partial
 * ticks and requests larger than the ring. Processing is injected and writes
 * one interleaved tick. Only the furthest reader causes new engine ticks.
 * The runtime lives for the process lifetime. Only pd_bridge_init configures
 * audio (zero inputs, DSP on); identical parameters are a no-op and changed
 * parameters are rejected even after all sources have been removed.
 *
 * The supplied mutex is also held by every bridge call into libpd, including
 * whole message assembly sequences. This provides the exclusion that sys_lock
 * gives PDTHREADS builds, without relying on PD_MULTI or recursive Pd locks.
 * Source registration/removal uses the same mutex. Audio holds it for at most
 * one tick plus a bounded ring copy, releases it between iterations, and never
 * allocates renderer storage. Control calls must not acquire SoLoud's mixer
 * lock while holding this mutex; source destruction stops voices before
 * unregistering. Synchronous Pd hooks must not re-enter the bridge. Slow patch
 * loads can still block audio.
 */
class PdRenderer {
public:
    using SourceId = std::uint64_t;
    using Process = std::function<void(float *)>;
    static constexpr unsigned int ringCapacity = 4096;

    PdRenderer(unsigned int outChannels, unsigned int blockSize,
               Process process, std::mutex &mutex)
        : mOutChannels(outChannels), mBlockSize(blockSize),
          mProcess(std::move(process)), mMutex(mutex),
          mScratch(static_cast<std::size_t>(outChannels) * blockSize),
          mRing(static_cast<std::size_t>(outChannels) * ringCapacity)
    {
        if (!outChannels || !blockSize || blockSize > ringCapacity || !mProcess)
            throw std::invalid_argument("Invalid PD renderer configuration");
    }

    PdRenderer(const PdRenderer &) = delete;
    PdRenderer &operator=(const PdRenderer &) = delete;

    SourceId registerSource(unsigned int firstChannel, unsigned int channels)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!channels || firstChannel >= mOutChannels ||
            channels > mOutChannels - firstChannel)
            return 0;
        for (const auto &entry : mSources) {
            const auto &source = entry.second;
            if (firstChannel < source.firstChannel + source.channels &&
                source.firstChannel < firstChannel + channels)
                return 0;
        }
        const auto id = mNextId++;
        mSources.emplace(id, Source{firstChannel, channels, mWritten});
        return id;
    }

    void unregisterSource(SourceId id)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mSources.erase(id);
    }

    unsigned int read(SourceId id, float *output, unsigned int frames,
                      unsigned int stride)
    {
        unsigned int filled = 0;
        while (filled < frames) {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mSources.find(id);
            if (it == mSources.end() || stride < frames || !output)
                return filled;
            auto &source = it->second;
            const auto oldest = mWritten > ringCapacity ? mWritten - ringCapacity : 0;
            source.position = (std::max)(source.position, oldest);
            if (source.position == mWritten) {
                mProcess(mScratch.data());
                for (unsigned int ch = 0; ch < mOutChannels; ++ch) {
                    for (unsigned int i = 0; i < mBlockSize; ++i) {
                        mRing[static_cast<std::size_t>(ch) * ringCapacity +
                              (mWritten + i) % ringCapacity] =
                            mScratch[static_cast<std::size_t>(i) * mOutChannels + ch];
                    }
                }
                mWritten += mBlockSize;
            }
            const auto count = static_cast<unsigned int>((std::min)(
                static_cast<std::uint64_t>(frames - filled), mWritten - source.position));
            for (unsigned int ch = 0; ch < source.channels; ++ch) {
                for (unsigned int i = 0; i < count; ++i) {
                    output[static_cast<std::size_t>(ch) * stride + filled + i] =
                        mRing[static_cast<std::size_t>(source.firstChannel + ch) * ringCapacity +
                              (source.position + i) % ringCapacity];
                }
            }
            source.position += count;
            filled += count;
        }
        return filled;
    }

private:
    struct Source {
        unsigned int firstChannel;
        unsigned int channels;
        std::uint64_t position;
    };

    unsigned int mOutChannels;
    unsigned int mBlockSize;
    Process mProcess;
    std::mutex &mMutex;
    std::vector<float> mScratch;
    std::vector<float> mRing;
    std::unordered_map<SourceId, Source> mSources;
    std::uint64_t mWritten = 0;
    SourceId mNextId = 1;
};

#endif
