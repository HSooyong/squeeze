#pragma once

#include "PluginProcessor.h"
#include "Ui/SceneView.h"
#include "Ui/Widgets.h"

#include <array>
#include <string>

namespace squeeze
{

class SqueezeEditor : public juce::AudioProcessorEditor,
                      private juce::AudioProcessorValueTreeState::Listener,
                      private juce::AsyncUpdater,
                      private juce::Timer
{
public:
    explicit SqueezeEditor(SqueezeProcessor&);
    ~SqueezeEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    bool layoutIsValid() const;
    std::string layoutReport() const;
    int paintRequestCount() const noexcept { return paintRequests; }
    void resetPaintRequestCount() noexcept { paintRequests = 0; }
    void serviceTimerForTest();
    void nudgeScene(bool locked, float delta);
    void nudgeOutput(float delta);
    void selectAllRatio();

private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    void timerCallback() override;
    void requestPaint();
    void listen(bool add);
    void refreshHeader();
    void toggleLearn();
    void toggleMeter();
    void selectSlot(int slot);
    void selectMode(int mode);

    SqueezeProcessor& processor;
    bool suppressStateWrite = false;
    bool fastTimer = false;
    int quietTicks = 0;
    int paintRequests = 0;

    ui::Logo logo;
    std::array<ui::Pill, 4> modeTabs;
    ui::Pill slotA;
    ui::Pill slotB;
    ui::Pill learn;
    ui::Pill meter;
    ui::MenuButton menu;
    ui::GainReadout gain;
    ui::SceneView scene;
    ui::OutputSlider output;
    ui::Pill meterHandle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SqueezeEditor)
};

} // namespace squeeze
