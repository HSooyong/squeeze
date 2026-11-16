#pragma once

namespace squeeze
{

// RMS-detector VCA model. Pass-through until the DSP milestone.
class VcaModel
{
public:
    void prepare(double sampleRate, int maxBlockSize) noexcept;
    void reset() noexcept;
    void process(float* const* channels, int numChannels, int numSamples) noexcept;
};

} // namespace squeeze
