#pragma once

namespace squeeze
{

// Optical leveling model. Pass-through until the DSP milestone.
class OptoModel
{
public:
    void prepare(double sampleRate, int maxBlockSize) noexcept;
    void reset() noexcept;
    void process(float* const* channels, int numChannels, int numSamples) noexcept;
};

} // namespace squeeze
