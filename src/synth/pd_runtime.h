#ifndef PD_RUNTIME_H
#define PD_RUNTIME_H

#include "pd_renderer.h"
#include <memory>

struct PdRuntime {
    std::mutex mutex;
    bool libpdInitialized = false;
    unsigned int sampleRate = 0;
    unsigned int outChannels = 0;
    std::unique_ptr<PdRenderer> renderer;
};

PdRuntime &pdRuntime();

#endif
