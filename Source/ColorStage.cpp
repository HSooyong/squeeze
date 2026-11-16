#include "ColorStage.h"

#include "MathUtil.h"

#include <algorithm>
#include <cmath>

namespace squeeze
{
namespace
{

constexpr int kMaximumDelay = 8192;

float shapeSample(float sample, float amount) noexcept
{
    if (amount <= 1.0e-6f)
        return sample;

    const float drive = 1.0f + 4.0f * amount;
    const float bent = sample >= 0.0f ? std::tanh(drive * sample) : std::tanh(sample);
    return sample + amount * (bent - sample);
}

int roundedLatency(const juce::dsp::Oversampling<float>& oversampling) noexcept
{
    return std::max(0, static_cast<int>(std::lround(static_cast<double>(oversampling.getLatencyInSamples()))));
}

} // namespace

void ColorStage::Delay::prepare(int channelCount, int maximumDelay)
{
    buffer.assign(static_cast<size_t>(std::max(1, channelCount)), std::vector<float>(static_cast<size_t>(std::max(1, maximumDelay)), 0.0f));
    write = 0;
    length = 0;
}

void ColorStage::Delay::setLength(int samples) noexcept
{
    length = std::clamp(samples, 0, buffer.empty() ? 0 : static_cast<int>(buffer.front().size()) - 1);
}

void ColorStage::Delay::reset() noexcept
{
    for (auto& channel : buffer)
        std::fill(channel.begin(), channel.end(), 0.0f);
    write = 0;
}

void ColorStage::Delay::process(float* const* channels, int numChannels, int numSamples) noexcept
{
    if (length <= 0 || buffer.empty())
        return;

    const int capacity = static_cast<int>(buffer.front().size());
    const int used = std::min(numChannels, static_cast<int>(buffer.size()));

    for (int index = 0; index < numSamples; ++index)
    {
        int read = write - length;
        if (read < 0)
            read += capacity;

        for (int channel = 0; channel < used; ++channel)
        {
            const float input = channels[channel][index];
            channels[channel][index] = buffer[static_cast<size_t>(channel)][static_cast<size_t>(read)];
            buffer[static_cast<size_t>(channel)][static_cast<size_t>(write)] = flushDenormal(input);
        }

        write = (write + 1) % capacity;
    }
}

bool ColorStage::Delay::hasBadState() const noexcept
{
    for (const auto& channel : buffer)
        for (const float sample : channel)
            if (isBadFloat(sample))
                return true;
    return false;
}

void ColorStage::prepare(double, int maximumBlockSize, int numChannels)
{
    channelCount = std::max(1, numChannels);
    maximumBlock = std::max(maximumBlockSize, 1);
    oversampling2 = std::make_unique<juce::dsp::Oversampling<float>>(static_cast<size_t>(std::max(channelCount, 2)),
                                                                      1,
                                                                      juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                                                                      true,
                                                                      true);
    oversampling4 = std::make_unique<juce::dsp::Oversampling<float>>(static_cast<size_t>(std::max(channelCount, 2)),
                                                                      2,
                                                                      juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                                                                      true,
                                                                      true);
    oversampling2->initProcessing(static_cast<size_t>(maximumBlock));
    oversampling4->initProcessing(static_cast<size_t>(maximumBlock));
    latency2 = roundedLatency(*oversampling2);
    latency4 = roundedLatency(*oversampling4);
    delay.prepare(std::max(channelCount, 2), kMaximumDelay);
    scratch[0].assign(static_cast<size_t>(maximumBlock), 0.0f);
    scratch[1].assign(static_cast<size_t>(maximumBlock), 0.0f);
    reset();
}

void ColorStage::reset() noexcept
{
    delay.reset();
    differenceRms = 0.0f;
    if (oversampling2 != nullptr)
        oversampling2->reset();
    if (oversampling4 != nullptr)
        oversampling4->reset();
}

int ColorStage::latencyForChoice(int oversamplingIndex) const noexcept
{
    if (oversamplingIndex <= 0)
        return 0;
    if (oversamplingIndex == 1)
        return latency2;
    return latency4;
}

void ColorStage::shapeInPlace(juce::dsp::AudioBlock<float>& block, const float* colorAmount, int numBaseSamples, int factor) noexcept
{
    double energy = 0.0;
    const auto numSamples = static_cast<int>(block.getNumSamples());
    const auto numChannels = static_cast<int>(block.getNumChannels());

    for (int index = 0; index < numSamples; ++index)
    {
        const int base = std::min(numBaseSamples - 1, index / std::max(1, factor));
        const float amount = colorAmount != nullptr ? colorAmount[base] : 0.0f;
        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float dry = block.getSample(static_cast<size_t>(channel), static_cast<size_t>(index));
            const float wet = shapeSample(dry, amount);
            energy += static_cast<double>(wet - dry) * static_cast<double>(wet - dry);
            block.setSample(static_cast<size_t>(channel), static_cast<size_t>(index), wet);
        }
    }

    const double count = std::max(1, numSamples * numChannels);
    differenceRms = static_cast<float>(std::sqrt(energy / count));
}

void ColorStage::processChunk(float* const* channels, const float* colorAmount, int numChannels, int numSamples, int oversamplingIndex) noexcept
{
    const int latency = latencyForChoice(oversamplingIndex);
    delay.setLength(latency);

    if (oversamplingIndex <= 0 || oversampling2 == nullptr || latency <= 0)
    {
        differenceRms = 0.0f;
        return;
    }

    const int used = std::min(numChannels, 2);
    for (int channel = 0; channel < used; ++channel)
        std::copy(channels[channel], channels[channel] + numSamples, scratch[channel].begin());

    delay.process(channels, numChannels, numSamples);

    float maximum = 0.0f;
    if (colorAmount != nullptr)
        for (int index = 0; index < numSamples; ++index)
            maximum = std::max(maximum, colorAmount[index]);

    if (maximum <= 1.0e-6f)
    {
        differenceRms = 0.0f;
        return;
    }

    float* colored[2] = { scratch[0].data(), used > 1 ? scratch[1].data() : scratch[0].data() };
    auto* oversampling = oversamplingIndex >= 2 ? oversampling4.get() : oversampling2.get();
    juce::dsp::AudioBlock<float> block(colored, static_cast<size_t>(used), static_cast<size_t>(numSamples));
    auto upsampled = oversampling->processSamplesUp(block);
    const int factor = std::max(1, static_cast<int>(upsampled.getNumSamples() / static_cast<size_t>(std::max(1, numSamples))));
    shapeInPlace(upsampled, colorAmount, numSamples, factor);
    oversampling->processSamplesDown(block);

    for (int index = 0; index < numSamples; ++index)
    {
        const float amount = colorAmount != nullptr ? clampFloat(colorAmount[index], 0.0f, 1.0f) : 0.0f;
        for (int channel = 0; channel < used; ++channel)
            channels[channel][index] = channels[channel][index] * (1.0f - amount) + colored[channel][index] * amount;
    }
}

void ColorStage::process(float* const* channels, const float* colorAmount, int numChannels, int numSamples, int oversamplingIndex) noexcept
{
    if (channels == nullptr || numChannels <= 0 || numSamples <= 0)
        return;

    int offset = 0;
    while (offset < numSamples)
    {
        const int count = std::min(maximumBlock, numSamples - offset);
        float* pointers[2] = { channels[0] + offset, numChannels > 1 ? channels[1] + offset : channels[0] + offset };
        const float* drive = colorAmount != nullptr ? colorAmount + offset : nullptr;
        processChunk(pointers, drive, numChannels, count, oversamplingIndex);
        offset += count;
    }
}

bool ColorStage::hasBadState() const noexcept
{
    return delay.hasBadState() || isBadFloat(differenceRms);
}

} // namespace squeeze
