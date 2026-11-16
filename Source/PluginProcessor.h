#pragma once

#include "Engine.h"
#include "State.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <mutex>

namespace squeeze
{

class SqueezeProcessor : public juce::AudioProcessor
{
public:
    SqueezeProcessor();
    ~SqueezeProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    using juce::AudioProcessor::processBlock;
    using juce::AudioProcessor::processBlockBypassed;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getApvts() noexcept { return apvts; }
    const juce::AudioProcessorValueTreeState& getApvts() const noexcept { return apvts; }

    UiState getUiState() const;
    void setUiState(UiState state);

    std::unique_ptr<juce::XmlElement> createStateXml();
    void applyStateXml(const juce::XmlElement& xml);

private:
    void sanitise(UiState& state) const;

    juce::AudioProcessorValueTreeState apvts;
    juce::AudioParameterChoice* modeParameter = nullptr;
    Engine engine;

    // Touched from the message thread and from state save/restore. Never from the audio thread.
    mutable std::mutex uiStateMutex;
    UiState uiState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SqueezeProcessor)
};

} // namespace squeeze
