#include "Parameters.h"

#include <cmath>

namespace squeeze::params
{
namespace
{

juce::String fit(juce::String text, int maximumLength)
{
    if (maximumLength > 0 && text.length() > maximumLength)
        return text.substring(0, maximumLength);

    return text;
}

float parseNumber(juce::String text, float fallback)
{
    text = text.trim().replaceCharacter(juce::juce_wchar(0x2212), '-');

    if (text.containsChar(juce::juce_wchar(0x221E)) || text.containsIgnoreCase("inf"))
        return kRatioInfinity;

    if (text.isEmpty())
        return fallback;

    return text.getFloatValue();
}

juce::String formatDecibels(float value)
{
    if (value > -0.05f && value < 0.05f)
        value = 0.0f;

    auto text = juce::String(value, 1);

    if (value > 0.0f)
        text = "+" + text;

    return text + " dB";
}

juce::String formatSigned(float value)
{
    if (value > -0.05f && value < 0.05f)
        return "0.0";

    auto text = juce::String(value, 1);

    if (value > 0.0f)
        text = "+" + text;

    return text;
}

juce::String formatRatio(float value)
{
    if (value >= 999.0f)
        return juce::String::fromUTF8("\xE2\x88\x9E : 1");

    const auto tenths = std::round(static_cast<double>(value) * 10.0) / 10.0;
    const auto nearest = std::round(tenths);
    const bool whole = std::abs(tenths - nearest) < 0.01;
    const auto number = whole ? juce::String(static_cast<int>(nearest)) : juce::String(static_cast<float>(tenths), 1);
    return number + " : 1";
}

juce::String formatMicroseconds(float value)
{
    return juce::String(juce::roundToInt(value)) + juce::String::fromUTF8(" \xC2\xB5s");
}

juce::String formatMilliseconds(float value)
{
    return juce::String(juce::roundToInt(value)) + " ms";
}

juce::AudioParameterFloatAttributes decibelAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float value, int length) { return fit(formatDecibels(value), length); })
        .withValueFromStringFunction([](const juce::String& text) { return parseNumber(text, 0.0f); });
}

juce::AudioParameterFloatAttributes signedAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float value, int length) { return fit(formatSigned(value), length); })
        .withValueFromStringFunction([](const juce::String& text) { return parseNumber(text, 0.0f); });
}

juce::AudioParameterFloatAttributes plainAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float value, int length) { return fit(juce::String(value, 1), length); })
        .withValueFromStringFunction([](const juce::String& text) { return parseNumber(text, 0.0f); });
}

juce::AudioParameterFloatAttributes ratioAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float value, int length) { return fit(formatRatio(value), length); })
        .withValueFromStringFunction([](const juce::String& text) { return parseNumber(text, 1.0f); });
}

juce::AudioParameterFloatAttributes microsecondAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float value, int length) { return fit(formatMicroseconds(value), length); })
        .withValueFromStringFunction([](const juce::String& text) { return parseNumber(text, 300.0f); });
}

juce::AudioParameterFloatAttributes millisecondAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float value, int length) { return fit(formatMilliseconds(value), length); })
        .withValueFromStringFunction([](const juce::String& text) { return parseNumber(text, 250.0f); });
}

juce::AudioParameterBoolAttributes onOffAttributes()
{
    return juce::AudioParameterBoolAttributes()
        .withStringFromValueFunction([](bool value, int length) {
            return fit(value ? "On" : "Off", length);
        })
        .withValueFromStringFunction([](const juce::String& text) {
            if (text.containsIgnoreCase("off") || text.equalsIgnoreCase("false") || text == "0")
                return false;

            if (text.containsIgnoreCase("on") || text.equalsIgnoreCase("true") || text == "1")
                return true;

            return false;
        });
}

juce::AudioParameterBoolAttributes limitAttributes()
{
    return juce::AudioParameterBoolAttributes()
        .withStringFromValueFunction([](bool value, int length) {
            return fit(value ? "Limit" : "Compress", length);
        })
        .withValueFromStringFunction([](const juce::String& text) {
            if (text.containsIgnoreCase("limit"))
                return true;

            if (text.containsIgnoreCase("compress"))
                return false;

            return text.getIntValue() != 0;
        });
}

// Logarithmic skew: equal knob travel is a constant ratio of the time value.
juce::NormalisableRange<float> logarithmicRange(float start, float end)
{
    return {
        start,
        end,
        [](float rangeStart, float rangeEnd, float proportion) {
            const auto p = juce::jlimit(0.0f, 1.0f, proportion);
            return rangeStart * std::pow(rangeEnd / rangeStart, p);
        },
        [](float rangeStart, float rangeEnd, float value) {
            const auto clamped = juce::jlimit(rangeStart, rangeEnd, value);
            return std::log(clamped / rangeStart) / std::log(rangeEnd / rangeStart);
        },
        [](float rangeStart, float rangeEnd, float value) {
            return juce::jlimit(rangeStart, rangeEnd, value);
        }
    };
}

juce::ParameterID id(const char* parameterId)
{
    return { parameterId, kParameterVersionHint };
}

template <typename Parameter, typename... Args>
std::unique_ptr<Parameter> make(Args&&... args)
{
    return std::make_unique<Parameter>(std::forward<Args>(args)...);
}

} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(make<juce::AudioParameterChoice>(
        id(mode),
        "Mode",
        juce::StringArray { "LAMP", "NEEDLE", "SPARK", "TUBE" },
        0));

    layout.add(make<juce::AudioParameterChoice>(
        id(osFactor),
        "Oversampling",
        juce::StringArray { "1x", "2x", "4x" },
        1));

    layout.add(make<juce::AudioParameterBool>(id(abLevelMatch), "Level match", true, onOffAttributes()));

    layout.add(make<juce::AudioParameterFloat>(
        id(optoPeakRed), "Peak Reduction", juce::NormalisableRange<float>(0.0f, 100.0f), 40.0f, plainAttributes()));
    layout.add(make<juce::AudioParameterFloat>(
        id(optoGain), "GAIN", juce::NormalisableRange<float>(-12.0f, 30.0f), 0.0f, decibelAttributes()));
    layout.add(make<juce::AudioParameterBool>(id(optoLimit), "Compress / Limit", false, limitAttributes()));

    layout.add(make<juce::AudioParameterFloat>(
        id(vcaThreshold), "THRESHOLD", juce::NormalisableRange<float>(-40.0f, 0.0f), -18.0f, decibelAttributes()));
    layout.add(make<juce::AudioParameterFloat>(
        id(vcaRatio), "RATIO", juce::NormalisableRange<float>(1.0f, kRatioInfinity), 4.0f, ratioAttributes()));
    layout.add(make<juce::AudioParameterFloat>(
        id(vcaOutput), "OUTPUT", juce::NormalisableRange<float>(-20.0f, 20.0f), 0.0f, decibelAttributes()));

    layout.add(make<juce::AudioParameterFloat>(
        id(fetInput), "INPUT", juce::NormalisableRange<float>(0.0f, 48.0f), 24.0f, signedAttributes()));
    layout.add(make<juce::AudioParameterFloat>(
        id(fetOutput), "OUTPUT", juce::NormalisableRange<float>(-24.0f, 24.0f), 0.0f, decibelAttributes()));
    layout.add(make<juce::AudioParameterFloat>(
        id(fetAttack), "ATTACK", logarithmicRange(20.0f, 800.0f), 300.0f, microsecondAttributes()));
    layout.add(make<juce::AudioParameterFloat>(
        id(fetRelease), "RELEASE", logarithmicRange(50.0f, 1100.0f), 250.0f, millisecondAttributes()));
    layout.add(make<juce::AudioParameterChoice>(
        id(fetRatio),
        "RATIO",
        juce::StringArray { "4", "8", "12", "20", "ALL" },
        0));
    layout.add(make<juce::AudioParameterBool>(id(fetAllEnabled), "Allow ALL", false, onOffAttributes()));

    layout.add(make<juce::AudioParameterFloat>(
        id(vmuInput), "INPUT", juce::NormalisableRange<float>(-20.0f, 20.0f), 0.0f, decibelAttributes()));
    layout.add(make<juce::AudioParameterFloat>(
        id(vmuThreshold), "THRESHOLD", juce::NormalisableRange<float>(-40.0f, 0.0f), -22.0f, decibelAttributes()));
    layout.add(make<juce::AudioParameterChoice>(
        id(vmuTime),
        "TIME",
        juce::StringArray { "T1", "T2", "T3", "T4", "T5", "T6" },
        2));
    layout.add(make<juce::AudioParameterFloat>(
        id(vmuOutput), "OUTPUT", juce::NormalisableRange<float>(-20.0f, 20.0f), 0.0f, decibelAttributes()));

    return layout;
}

} // namespace squeeze::params
