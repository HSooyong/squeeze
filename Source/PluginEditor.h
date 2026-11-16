#pragma once

#include "PluginProcessor.h"

namespace squeeze
{

class SqueezeEditor : public juce::AudioProcessorEditor,
                      private juce::AudioProcessorValueTreeState::Listener,
                      private juce::AsyncUpdater
{
public:
    explicit SqueezeEditor(SqueezeProcessor&);
    ~SqueezeEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;

    SqueezeProcessor& processor;
    bool suppressStateWrite = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SqueezeEditor)
};

} // namespace squeeze
