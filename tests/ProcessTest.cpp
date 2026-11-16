#include "Check.h"
#include "Parameters.h"
#include "PluginProcessor.h"

#include <cmath>

namespace
{

void setChoice(squeeze::SqueezeProcessor& processor, const char* id, float index)
{
    auto* parameter = processor.getApvts().getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(index));
}

bool finiteBuffer(const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int index = 0; index < buffer.getNumSamples(); ++index)
            if (!std::isfinite(buffer.getSample(channel, index)) || std::fabs(buffer.getSample(channel, index)) > 4.0f)
                return false;
    return true;
}

} // namespace

namespace squeeze::test
{

void runProcess()
{
    const double sampleRates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };
    const int blockSizes[] = { 1, 128, 4096 };

    for (const double sampleRate : sampleRates)
    {
        for (const int blockSize : blockSizes)
        {
            SqueezeProcessor processor;
            processor.prepareToPlay(sampleRate, blockSize);
            const int latency = processor.getLatencySamples();

            for (int mode = 0; mode < 4; ++mode)
            {
                setChoice(processor, params::mode, static_cast<float>(mode));
                juce::AudioBuffer<float> buffer(2, blockSize);
                for (int index = 0; index < blockSize; ++index)
                {
                    const float sample = 0.2f * std::sin(2.0f * 3.14159265f * 440.0f * static_cast<float>(index) / static_cast<float>(sampleRate));
                    buffer.setSample(0, index, sample);
                    buffer.setSample(1, index, sample * 0.5f);
                }

                juce::MidiBuffer midi;
                processor.processBlock(buffer, midi);
                EXPECT(finiteBuffer(buffer));
                EXPECT(std::isfinite(processor.currentGrDb()));
                EXPECT(processor.getLatencySamples() == latency);
            }

            processor.releaseResources();
        }
    }

    SqueezeProcessor bypassed;
    bypassed.prepareToPlay(48000.0, 128);
    setChoice(bypassed, params::osFactor, 0.0f);
    juce::AudioBuffer<float> ones(2, 32);
    for (int index = 0; index < ones.getNumSamples(); ++index)
    {
        ones.setSample(0, index, 0.2f);
        ones.setSample(1, index, 0.2f);
    }
    juce::MidiBuffer midi;
    bypassed.processBlock(ones, midi);
    EXPECT(bypassed.getLatencySamples() == 0);
    EXPECT(finiteBuffer(ones));
}

} // namespace squeeze::test
