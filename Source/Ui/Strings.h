#pragma once

#include <juce_core/juce_core.h>

namespace squeeze::ui::strings
{

inline juce::String utf8(const char* text)
{
    return juce::String::fromUTF8(text);
}

// Wordmark stays the product name. Everything else is Korean with a small English sub-label.
inline juce::String wordmark() { return "SQUEEZE"; }
inline juce::String brand() { return "BEARFIELD"; }

inline juce::String gainTitle() { return utf8(u8"게인 감소"); }
inline juce::String gainTitleEn() { return "GAIN REDUCTION"; }

inline juce::String modePrimary(int mode)
{
    switch (mode)
    {
        case 0: return utf8(u8"램프");
        case 1: return utf8(u8"니들");
        case 2: return utf8(u8"스파크");
        default: return utf8(u8"튜브");
    }
}

inline juce::String modeEnglish(int mode)
{
    switch (mode)
    {
        case 0: return "LAMP";
        case 1: return "NEEDLE";
        case 2: return "SPARK";
        default: return "TUBE";
    }
}

inline juce::String learnPrimary() { return utf8(u8"배우기"); }
inline juce::String learnEn() { return "LEARN"; }
inline juce::String meterPrimary() { return utf8(u8"미터"); }
inline juce::String meterEn() { return "METER"; }
inline juce::String gainSlider() { return utf8(u8"게인"); }
inline juce::String gainSliderEn() { return "GAIN"; }
inline juce::String outputSlider() { return utf8(u8"출력"); }
inline juce::String outputSliderEn() { return "OUTPUT"; }
inline juce::String compress() { return utf8(u8"압축"); }
inline juce::String limit() { return utf8(u8"제한"); }
inline juce::String peakWord() { return utf8(u8"피크"); }
inline juce::String valveAuto() { return utf8(u8"밸브 자동"); }
inline juce::String valveAutoEn() { return "AUTO"; }
inline juce::String fixedLine() { return utf8(u8"고정선"); }
inline juce::String fixedLineEn() { return "FIXED"; }
inline juce::String heat() { return utf8(u8"입력 열"); }
inline juce::String heatEn() { return "HEAT"; }
inline juce::String ratioNow() { return utf8(u8"지금 비율"); }
inline juce::String ratioNowEn() { return "RATIO"; }
inline juce::String timeLabel() { return utf8(u8"시간"); }
inline juce::String timeLabelEn() { return "TIME"; }
inline juce::String inputLabel() { return utf8(u8"입력"); }
inline juce::String inputLabelEn() { return "INPUT"; }
inline juce::String thresholdLabel() { return utf8(u8"임계"); }
inline juce::String thresholdLabelEn() { return "THRESHOLD"; }
inline juce::String openLabel() { return utf8(u8"열림"); }
inline juce::String openLabelEn() { return "OPEN"; }

} // namespace squeeze::ui::strings
