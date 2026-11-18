#pragma once

#include <juce_core/juce_core.h>

namespace squeeze::ui::strings
{

// Every string the plugin draws. English only, and no hardware model names.
inline juce::String wordmark() { return "SQUEEZE"; }
inline juce::String brand() { return "BEARFIELD"; }

inline juce::String gainTitle() { return "GAIN REDUCTION"; }

inline juce::String modePrimary(int mode)
{
    switch (mode)
    {
        case 0: return "LAMP";
        case 1: return "NEEDLE";
        case 2: return "SPARK";
        default: return "TUBE";
    }
}

inline juce::String modeDetail(int mode)
{
    switch (mode)
    {
        case 0: return "Opto";
        case 1: return "VCA";
        case 2: return "FET";
        default: return "Vari-Mu";
    }
}

inline juce::String learnLabel() { return "LEARN"; }
inline juce::String meterLabel() { return "METER"; }
inline juce::String gainSlider() { return "GAIN"; }
inline juce::String outputSlider() { return "OUTPUT"; }

} // namespace squeeze::ui::strings
