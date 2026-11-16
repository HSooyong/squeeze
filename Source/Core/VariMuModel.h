#pragma once

namespace squeeze
{

// Variable-mu model with a six-step time constant. Pass-through until the DSP milestone.
class VariMuModel
{
public:
    void prepare(double sampleRate, int maxBlockSize) noexcept;
    void reset() noexcept;
    void process(float* const* channels, int numChannels, int numSamples) noexcept;
};

} // namespace squeeze
