#pragma once

#include "EditorLayout.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <vector>

namespace squeeze
{

// Root attribute written by every save. Older documents (missing or lower) still load.
inline constexpr int kStateVersion = 1;
inline constexpr int kMaxHistoryEntries = 50;

// The build spec lists Always / Lesson only / Off and does not assign a default.
// Lesson only is the initial value.
enum class Captions
{
    always = 0,
    lessonOnly = 1,
    off = 2
};

struct HistoryEntry
{
    juce::String time;
    juce::String mode;
    juce::String change;
    juce::String sentence;
};

struct ParameterSnapshot
{
    juce::String id;
    float value = 0.0f;
};

struct UiState
{
    int windowWidth = ui::kDefaultWidth;
    bool learnOpen = false;
    bool meterOpen = false;
    Captions captions = Captions::lessonOnly;
    juce::String selectedScene;
    int abSlot = 0;
    std::vector<HistoryEntry> history;
    std::array<std::vector<ParameterSnapshot>, 2> slots;
};

std::vector<ParameterSnapshot> snapshotParameters(const juce::AudioProcessorValueTreeState& state);

std::unique_ptr<juce::XmlElement> parametersToXml(juce::AudioProcessorValueTreeState& state);

// Unknown parameter ids are ignored. Missing ids are written back as defaults.
void applyParameters(const juce::XmlElement& xml, juce::AudioProcessorValueTreeState& state);

std::unique_ptr<juce::XmlElement> uiToXml(const UiState& ui);

// A null element yields a default UiState. Unknown attributes and children are ignored.
UiState uiFromXml(const juce::XmlElement* ui, const juce::AudioProcessorValueTreeState& state);

} // namespace squeeze
