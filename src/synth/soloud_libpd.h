#ifndef SOLOUD_LIBPD_H
#define SOLOUD_LIBPD_H

#include "soloud.h"

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
    LibPDAudioSource(unsigned int sampleRate, unsigned int channels);
    virtual ~LibPDAudioSource();
    virtual SoLoud::AudioSourceInstance *createInstance();
};

#endif // SOLOUD_LIBPD_H
