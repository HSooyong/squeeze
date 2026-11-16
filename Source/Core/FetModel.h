#pragma once

namespace squeeze
{

// Fixed-threshold FET peak model. Pass-through until the DSP milestone.
class FetModel
{
public:
    void prepare(double sampleRate, int maxBlockSize) noexcept;
    void reset() noexcept;
    void process(float* const* channels, int numChannels, int numSamples) noexcept;
};

} // namespace squeeze
