#include "Check.h"
#include "Engine.h"
#include "Parameters.h"
#include "PluginProcessor.h"

#include <cmath>
#include <vector>

namespace
{

void setMode(squeeze::SqueezeProcessor& processor, int index)
{
    auto* choice = dynamic_cast<juce::AudioParameterChoice*>(processor.getApvts().getParameter(squeeze::params::mode));
    choice->setValueNotifyingHost(choice->convertTo0to1(static_cast<float>(index)));
}

void fill(juce::AudioBuffer<float>& buffer, double sampleRate)
{
    const int numSamples = buffer.getNumSamples();

    for (int index = 0; index < numSamples; ++index)
    {
        const auto time = static_cast<float>(index) / static_cast<float>(sampleRate);
        const auto left = std::sin(2.0f * 3.14159265f * 440.0f * time);
        buffer.setSample(0, index, left);

        if (buffer.getNumChannels() > 1)
            buffer.setSample(1, index, left * 0.25f);
    }

    if (numSamples > 0)
        buffer.setSample(0, 0, 0.0f);

    if (numSamples > 1)
        buffer.setSample(0, 1, -1.0f);
}

bool sameAudio(const juce::AudioBuffer<float>& actual, const juce::AudioBuffer<float>& expected)
{
    if (actual.getNumChannels() != expected.getNumChannels() || actual.getNumSamples() != expected.getNumSamples())
        return false;

    for (int channel = 0; channel < actual.getNumChannels(); ++channel)
    {
        for (int index = 0; index < actual.getNumSamples(); ++index)
        {
            const auto sample = actual.getSample(channel, index);

            if (!std::isfinite(sample) || !squeeze::test::bitsEqual(sample, expected.getSample(channel, index)))
                return false;
        }
    }

    return true;
}

void runCore()
{
    squeeze::Engine engine;
    engine.prepare(96000.0, 8);

    float left[4] = { 0.1f, -0.2f, 0.3f, -0.4f };
    float right[4] = { 0.5f, 0.6f, -0.7f, 0.8f };
    const float leftCopy[4] = { 0.1f, -0.2f, 0.3f, -0.4f };
    const float rightCopy[4] = { 0.5f, 0.6f, -0.7f, 0.8f };
    float* channels[] = { left, right };

    for (int index = 0; index < squeeze::kModeCount; ++index)
    {
        engine.setMode(squeeze::modeFromIndex(index));
        EXPECT(static_cast<int>(engine.getMode()) == index);
        engine.process(channels, 2, 4);
    }

    engine.process(nullptr, 2, 4);
    engine.process(channels, 0, 4);
    engine.reset();

    for (int index = 0; index < 4; ++index)
    {
        EXPECT(squeeze::test::bitsEqual(left[index], leftCopy[index]));
        EXPECT(squeeze::test::bitsEqual(right[index], rightCopy[index]));
        EXPECT(std::isfinite(left[index]));
        EXPECT(std::isfinite(right[index]));
    }
}

} // namespace

namespace squeeze::test
{

void runProcess()
{
    runCore();

    const double sampleRates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };
    const int blockSizes[] = { 1, 128, 4096 };

    for (const double sampleRate : sampleRates)
    {
        for (const int blockSize : blockSizes)
        {
            SqueezeProcessor processor;
            processor.prepareToPlay(sampleRate, blockSize);
            EXPECT(processor.getLatencySamples() == 0);

            for (int mode = 0; mode < 4; ++mode)
            {
                setMode(processor, mode);

                juce::AudioBuffer<float> buffer(2, blockSize);
                fill(buffer, sampleRate);
                const auto expected = buffer;
                juce::MidiBuffer midi;
                processor.processBlock(buffer, midi);

                EXPECT(sameAudio(buffer, expected));
                EXPECT(processor.getLatencySamples() == 0);
            }

            juce::AudioBuffer<float> bypassed(2, blockSize);
            fill(bypassed, sampleRate);
            const auto bypassExpected = bypassed;
            juce::MidiBuffer midi;
            processor.processBlockBypassed(bypassed, midi);
            EXPECT(sameAudio(bypassed, bypassExpected));
            EXPECT(processor.getLatencySamples() == 0);

            processor.releaseResources();
        }
    }

    SqueezeProcessor empty;
    empty.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> noSamples(2, 0);
    juce::MidiBuffer midi;
    empty.processBlock(noSamples, midi);
    EXPECT(empty.getLatencySamples() == 0);

    SqueezeProcessor mono;
    mono.setPlayConfigDetails(1, 1, 44100.0, 64);
    EXPECT(mono.getTotalNumInputChannels() == 1);
    EXPECT(mono.getTotalNumOutputChannels() == 1);
    mono.prepareToPlay(44100.0, 64);

    juce::AudioBuffer<float> monoBuffer(1, 64);
    fill(monoBuffer, 44100.0);
    const auto monoExpected = monoBuffer;
    mono.processBlock(monoBuffer, midi);
    EXPECT(sameAudio(monoBuffer, monoExpected));

    for (int mode = 0; mode < 4; ++mode)
    {
        setMode(mono, mode);
        mono.processBlock(monoBuffer, midi);
        EXPECT(std::isfinite(monoBuffer.getSample(0, 0)));
    }
}

} // namespace squeeze::test
