#pragma once

#include "FetModel.h"
#include "Mode.h"
#include "OptoModel.h"
#include "VariMuModel.h"
#include "VcaModel.h"

namespace squeeze
{

// Switches between the four models. M1 processing does not alter samples.
class Engine
{
public:
    void prepare(double sampleRate, int maxBlockSize) noexcept;
    void reset() noexcept;
    void setMode(Mode newMode) noexcept;
    Mode getMode() const noexcept;
    void process(float* const* channels, int numChannels, int numSamples) noexcept;

private:
    struct Prepared
    {
        double sampleRate = 44100.0;
        int maxBlockSize = 0;
    };

    Prepared prepared;
    Mode mode = Mode::opto;
    OptoModel opto;
    VcaModel vca;
    FetModel fet;
    VariMuModel variMu;
};

} // namespace squeeze
