#include "soloud_libpd.h"
#include "pd_runtime.h"

LibPDInstance::LibPDInstance(LibPDAudioSource *aParent)
    : mParent(aParent)
{
}

unsigned int LibPDInstance::getAudio(float *aBuffer, unsigned int aSamplesToRead,
                                    unsigned int aBufferSize)
{
    // No audio input is needed for DSP-only patches; pd_bridge_init configures zero inputs.
    // PdRenderer sizes scratch for blockSize * outChannels.
    // libpd writes interleaved output per block into the shared renderer.
    return mParent->mRenderer->read(mParent->mSourceId, aBuffer,
                                    aSamplesToRead, aBufferSize);
}

bool LibPDInstance::hasEnded()
{
    return false;
}

LibPDAudioSource::LibPDAudioSource(unsigned int sampleRate, unsigned int channels,
                                 unsigned int firstChannel)
{
    // Audio initialization and DSP enablement happen only in pd_bridge_init.
    auto &runtime = pdRuntime();
    {
        std::lock_guard<std::mutex> lock(runtime.mutex);
        if (sampleRate != runtime.sampleRate || !runtime.renderer ||
            (channels != 1 && channels != 2 && channels != 4 &&
             channels != 6 && channels != 8) || channels > MAX_CHANNELS)
            return;
        mRenderer = runtime.renderer.get();
    }
    mSourceId = mRenderer->registerSource(firstChannel, channels);
    mBaseSamplerate = static_cast<float>(sampleRate);
    mChannels = channels;
    mVolume = 1.0f;
    setSingleInstance(true);
}

LibPDAudioSource::~LibPDAudioSource()
{
    stop();
    if (mRenderer && mSourceId)
        mRenderer->unregisterSource(mSourceId);
}

SoLoud::AudioSourceInstance *LibPDAudioSource::createInstance()
{
    if (!isValid())
        return nullptr;
    return new LibPDInstance(this);
}
