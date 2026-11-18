#pragma once

#include "ColorStage.h"
#include "Engine.h"
#include "State.h"
#include "VisualFrame.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <mutex>
#include <vector>

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

    float currentGrDb() const noexcept { return engine.currentGrDb(); }
    float currentDetectorDb() const noexcept { return engine.currentDetectorDb(); }
    const VisualFrame& lastVisualFrame() const noexcept { return latestFrame; }
    bool popVisualFrame(VisualFrame& frame) noexcept;
    bool hasBadAudioState() const noexcept;
    int oversamplingLatencySamples() const noexcept;

    // Message thread only. The audio thread never touches gesture or slot state.
    void beginGestureUndo();
    void commitGesture();
    void notifyLockedControl(int role);
    bool undoGesture();
    bool redoGesture();
    void selectAbSlot(int slot);
    void applyScene(int mode, int index);
    juce::String toastText() const;
    juce::int64 toastShownAtMs() const noexcept { return toastAtMs; }

private:
    void sanitise(UiState& state) const;
    EngineTargets readTargets() const noexcept;
    void applySnapshot(const std::vector<ParameterSnapshot>& snapshot);
    void showToast(juce::String text);
    void rememberHistory(juce::String change, juce::String sentence);
    void publishFrame(const VisualFrame& frame) noexcept;
    void processChunk(float* const* channels, int numChannels, int numSamples, const EngineTargets& targets, int oversamplingIndex);

    juce::AudioProcessorValueTreeState apvts;
    juce::AudioParameterChoice* modeParameter = nullptr;
    Engine engine;
    ColorStage colorStage;
    int preparedSamples = 0;
    int activeOversampling = 1;

    std::atomic<float>* modeRaw = nullptr;
    std::atomic<float>* osRaw = nullptr;
    std::atomic<float>* levelMatchRaw = nullptr;
    std::atomic<float>* optoPeakRaw = nullptr;
    std::atomic<float>* optoGainRaw = nullptr;
    std::atomic<float>* optoLimitRaw = nullptr;
    std::atomic<float>* vcaThresholdRaw = nullptr;
    std::atomic<float>* vcaRatioRaw = nullptr;
    std::atomic<float>* vcaOutputRaw = nullptr;
    std::atomic<float>* fetInputRaw = nullptr;
    std::atomic<float>* fetOutputRaw = nullptr;
    std::atomic<float>* fetAttackRaw = nullptr;
    std::atomic<float>* fetReleaseRaw = nullptr;
    std::atomic<float>* fetRatioRaw = nullptr;
    std::atomic<float>* fetAllRaw = nullptr;
    std::atomic<float>* vmuInputRaw = nullptr;
    std::atomic<float>* vmuThresholdRaw = nullptr;
    std::atomic<float>* vmuTimeRaw = nullptr;
    std::atomic<float>* vmuOutputRaw = nullptr;

    std::vector<float> dryLeft;
    std::vector<float> dryRight;
    std::vector<float> colorDrive;
    juce::AbstractFifo visualFifo { 256 };
    std::array<VisualFrame, 256> visualFrames {};
    VisualFrame latestFrame;
    std::atomic<float> publishedGr { 0.0f };
    std::atomic<float> publishedIn { -160.0f };
    std::atomic<float> publishedOut { -160.0f };

    // Touched from the message thread and from state save/restore. Never from the audio thread.
    mutable std::mutex uiStateMutex;
    UiState uiState;
    bool gestureOpen = false;
    std::vector<ParameterSnapshot> gestureStart;
    std::vector<std::vector<ParameterSnapshot>> undoStack;
    std::vector<std::vector<ParameterSnapshot>> redoStack;
    juce::String toastTextValue;
    juce::int64 toastAtMs = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SqueezeProcessor)
};

} // namespace squeeze
