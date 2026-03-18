#include "soloud_libpd.h"
#include "z_libpd.h"

#include <mutex>

LibPDInstance::LibPDInstance(LibPDAudioSource *aParent)
{
    mParent = aParent;
}

unsigned int LibPDInstance::getAudio(float *aBuffer, unsigned int aSamplesToRead, unsigned int aBufferSize)
{
    const int blockSize = libpd_blocksize();
    const int channels = mParent->mChannels;
    const int samplesPerBlock = blockSize * channels;

    // Zero-filled input buffer (no audio input needed for DSP-only patches)
    float inBuf[128] = {}; // blockSize(64) * maxChannels(2)

    unsigned int written = 0;
    while (written + (unsigned int)samplesPerBlock <= aSamplesToRead)
    {
        libpd_process_float(1, inBuf, aBuffer + written);
        written += samplesPerBlock;
    }
    return written;
}

bool LibPDInstance::hasEnded()
{
    return false;
}

LibPDAudioSource::LibPDAudioSource(unsigned int sampleRate, unsigned int channels)
{
    static std::once_flag initFlag;
    std::call_once(initFlag, [&]() {
        libpd_init();
        libpd_init_audio(0, (int)channels, (int)sampleRate);
        // Turn DSP on
        libpd_start_message(1);
        libpd_add_float(1.0f);
        libpd_finish_message("pd", "dsp");
    });

    mBaseSamplerate = (float)sampleRate;
    mChannels = channels;
    mVolume = 1.0f;
}

LibPDAudioSource::~LibPDAudioSource()
{
    stop();
}

SoLoud::AudioSourceInstance *LibPDAudioSource::createInstance()
{
    return new LibPDInstance(this);
}
