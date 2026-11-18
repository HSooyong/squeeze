#pragma once

#include <juce_dsp/juce_dsp.h>

#include <memory>
#include <vector>

namespace squeeze
{

// Delay that matches the oversampler, plus the oversampled colour stage.
// Colour stays out of the path while its amount is zero, so a transparent
// setting is a plain integer delay.
class ColorStage
{
public:
    void prepare(double sampleRate, int maximumBlockSize, int numChannels);
    void reset() noexcept;

    int latencyForChoice(int oversamplingIndex) const noexcept;
    void process(float* const* channels, const float* colorAmount, int numChannels, int numSamples, int oversamplingIndex) noexcept;
    float lastDifferenceRms() const noexcept { return differenceRms; }
    bool hasBadState() const noexcept;

private:
    class Delay
    {
    public:
        void prepare(int channels, int maximumDelay);
        void setLength(int samples) noexcept;
        void reset() noexcept;
        void process(float* const* channels, int numChannels, int numSamples) noexcept;
        bool hasBadState() const noexcept;

    private:
        std::vector<std::vector<float>> buffer;
        int write = 0;
        int length = 0;
    };

    void processChunk(float* const* channels, const float* colorAmount, int numChannels, int numSamples, int oversamplingIndex) noexcept;
    void shapeInPlace(juce::dsp::AudioBlock<float>& block, const float* colorAmount, int numBaseSamples, int factor) noexcept;

    Delay delay;
    std::vector<float> scratch[2];
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling2;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling4;
    int latency2 = 0;
    int latency4 = 0;
    int maximumBlock = 0;
    int channelCount = 2;
    float differenceRms = 0.0f;
};

} // namespace squeeze
