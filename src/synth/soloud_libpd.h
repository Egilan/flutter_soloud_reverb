#ifndef SOLOUD_LIBPD_H
#define SOLOUD_LIBPD_H

#include "soloud.h"
#include "pd_renderer.h"

class LibPDAudioSource;

class LibPDInstance : public SoLoud::AudioSourceInstance
{
    LibPDAudioSource *mParent;

public:
    LibPDInstance(LibPDAudioSource *aParent);
    virtual unsigned int getAudio(float *aBuffer, unsigned int aSamplesToRead, unsigned int aBufferSize);
    virtual bool hasEnded();
};

class LibPDAudioSource : public SoLoud::AudioSource
{
public:
    LibPDAudioSource(unsigned int sampleRate, unsigned int channels,
                     unsigned int firstChannel = 0);
    LibPDAudioSource(const LibPDAudioSource &) = delete;
    LibPDAudioSource &operator=(const LibPDAudioSource &) = delete;
    virtual ~LibPDAudioSource();
    virtual SoLoud::AudioSourceInstance *createInstance();
    bool isValid() const { return mSourceId != 0; }

private:
    friend class LibPDInstance;
    PdRenderer *mRenderer = nullptr;
    PdRenderer::SourceId mSourceId = 0;
};

#endif // SOLOUD_LIBPD_H
