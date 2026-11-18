#include "Check.h"
#include "EditorLayout.h"
#include "Parameters.h"
#include "PluginProcessor.h"

#include <cmath>
#include <initializer_list>
#include <memory>
#include <string>

namespace
{

juce::RangedAudioParameter* parameter(squeeze::SqueezeProcessor& processor, const char* id)
{
    return processor.getApvts().getParameter(id);
}

float currentValue(squeeze::SqueezeProcessor& processor, const char* id)
{
    return parameter(processor, id)->convertFrom0to1(parameter(processor, id)->getValue());
}

void expectFloat(squeeze::SqueezeProcessor& processor,
                 const char* id,
                 const char* name,
                 float minimum,
                 float maximum,
                 float defaultValue,
                 bool logarithmic)
{
    auto* ranged = parameter(processor, id);
    EXPECT(ranged != nullptr);

    if (ranged == nullptr)
        return;

    EXPECT(ranged->getName(64) == name);

    const auto& range = ranged->getNormalisableRange();
    EXPECT(squeeze::test::near(range.start, minimum));
    EXPECT(squeeze::test::near(range.end, maximum));
    EXPECT(squeeze::test::near(ranged->convertFrom0to1(ranged->getDefaultValue()), defaultValue, 1.0e-3f));
    EXPECT(squeeze::test::near(currentValue(processor, id), defaultValue, 1.0e-3f));

    if (logarithmic)
    {
        const auto midpoint = range.convertFrom0to1(0.5f);
        const auto geometric = std::sqrt(range.start * range.end);
        EXPECT(squeeze::test::near(midpoint, geometric, 0.05f));
    }
}

void expectChoice(squeeze::SqueezeProcessor& processor,
                  const char* id,
                  const char* name,
                  std::initializer_list<const char*> choices,
                  int defaultIndex)
{
    auto* choice = dynamic_cast<juce::AudioParameterChoice*>(parameter(processor, id));
    EXPECT(choice != nullptr);

    if (choice == nullptr)
        return;

    EXPECT(choice->getName(64) == name);
    EXPECT(choice->choices.size() == static_cast<int>(choices.size()));

    int index = 0;

    for (const auto* text : choices)
    {
        EXPECT(choice->choices[index] == text);
        ++index;
    }

    EXPECT(choice->getIndex() == defaultIndex);
}

void expectBool(squeeze::SqueezeProcessor& processor, const char* id, const char* name, bool defaultValue)
{
    auto* toggle = dynamic_cast<juce::AudioParameterBool*>(parameter(processor, id));
    EXPECT(toggle != nullptr);

    if (toggle == nullptr)
        return;

    EXPECT(toggle->getName(64) == name);
    EXPECT(toggle->get() == defaultValue);
}

} // namespace

namespace squeeze::test
{

void runParams()
{
    SqueezeProcessor processor;

    EXPECT(processor.getName() == "SQUEEZE");
    EXPECT(!processor.acceptsMidi());
    EXPECT(!processor.producesMidi());
    EXPECT(!processor.isMidiEffect());
    EXPECT(squeeze::test::near(static_cast<float>(processor.getTailLengthSeconds()), 0.0f));
    EXPECT(processor.hasEditor());
    EXPECT(processor.getNumPrograms() == 1);
    EXPECT(processor.getProgramName(0) == "Default");

    const char* order[] = {
        params::mode,
        params::osFactor,
        params::abLevelMatch,
        params::optoPeakRed,
        params::optoGain,
        params::optoLimit,
        params::vcaThreshold,
        params::vcaRatio,
        params::vcaOutput,
        params::fetInput,
        params::fetOutput,
        params::fetAttack,
        params::fetRelease,
        params::fetRatio,
        params::fetAllEnabled,
        params::vmuInput,
        params::vmuThreshold,
        params::vmuTime,
        params::vmuOutput,
    };

    const auto& list = processor.getParameters();
    EXPECT(list.size() == 19);

    for (int index = 0; index < 19 && index < list.size(); ++index)
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(list[index]);
        EXPECT(ranged != nullptr);

        if (ranged != nullptr)
            EXPECT(ranged->getParameterID() == order[index]);
    }

    expectChoice(processor, params::mode, "Mode", { "LAMP", "NEEDLE", "SPARK", "TUBE" }, 0);
    expectChoice(processor, params::osFactor, "Oversampling", { "1x", "2x", "4x" }, 1);
    expectBool(processor, params::abLevelMatch, "Level match", true);

    expectFloat(processor, params::optoPeakRed, "Peak Reduction", 0.0f, 100.0f, 40.0f, false);
    expectFloat(processor, params::optoGain, "GAIN", -12.0f, 30.0f, 0.0f, false);
    expectBool(processor, params::optoLimit, "Compress / Limit", false);

    expectFloat(processor, params::vcaThreshold, "THRESHOLD", -40.0f, 0.0f, -18.0f, false);
    expectFloat(processor, params::vcaRatio, "RATIO", 1.0f, params::kRatioInfinity, 4.0f, false);
    expectFloat(processor, params::vcaOutput, "OUTPUT", -20.0f, 20.0f, 0.0f, false);

    expectFloat(processor, params::fetInput, "INPUT", 0.0f, 48.0f, 24.0f, false);
    expectFloat(processor, params::fetOutput, "OUTPUT", -24.0f, 24.0f, 0.0f, false);
    expectFloat(processor, params::fetAttack, "ATTACK", 20.0f, 800.0f, 300.0f, true);
    expectFloat(processor, params::fetRelease, "RELEASE", 50.0f, 1100.0f, 250.0f, true);
    expectChoice(processor, params::fetRatio, "RATIO", { "4", "8", "12", "20", "ALL" }, 0);
    expectBool(processor, params::fetAllEnabled, "Allow ALL", false);

    expectFloat(processor, params::vmuInput, "INPUT", -20.0f, 20.0f, 0.0f, false);
    expectFloat(processor, params::vmuThreshold, "THRESHOLD", -40.0f, 0.0f, -22.0f, false);
    expectChoice(processor, params::vmuTime, "TIME", { "T1", "T2", "T3", "T4", "T5", "T6" }, 2);
    expectFloat(processor, params::vmuOutput, "OUTPUT", -20.0f, 20.0f, 0.0f, false);

    EXPECT(parameter(processor, params::vcaThreshold)->getCurrentValueAsText() == "-18.0 dB");
    EXPECT(parameter(processor, params::vcaRatio)->getCurrentValueAsText() == "4 : 1");
    EXPECT(parameter(processor, params::fetAttack)->getCurrentValueAsText() == juce::String::fromUTF8("300 \xC2\xB5s"));
    EXPECT(parameter(processor, params::fetRelease)->getCurrentValueAsText() == "250 ms");
    EXPECT(parameter(processor, params::fetInput)->getCurrentValueAsText() == "+24.0");
    EXPECT(parameter(processor, params::optoGain)->getCurrentValueAsText() == "0.0 dB");
    EXPECT(parameter(processor, params::optoLimit)->getCurrentValueAsText() == "Compress");
    EXPECT(parameter(processor, params::abLevelMatch)->getCurrentValueAsText() == "On");

    auto* ratio = parameter(processor, params::vcaRatio);
    ratio->setValueNotifyingHost(ratio->convertTo0to1(1.5f));
    EXPECT(ratio->getCurrentValueAsText() == "1.5 : 1");
    ratio->setValueNotifyingHost(ratio->convertTo0to1(params::kRatioInfinity));
    EXPECT(ratio->getCurrentValueAsText() == juce::String::fromUTF8("\xE2\x88\x9E : 1"));
    EXPECT(squeeze::test::near(ratio->getValueForText(juce::String::fromUTF8("\xE2\x88\x9E : 1")), 1.0f));

    auto* threshold = parameter(processor, params::vcaThreshold);
    const auto parsed = threshold->convertFrom0to1(threshold->getValueForText("-18.0 dB"));
    EXPECT(squeeze::test::near(parsed, -18.0f, 1.0e-3f));

    EXPECT(ui::kDefaultWidth == 1280);
    EXPECT(ui::kMinWidth == 1100);
    EXPECT(ui::kMaxWidth == 1760);
    EXPECT(ui::kDesignHeight == 660);
    EXPECT(ui::kHeaderHeight == 56);
    EXPECT(ui::kLeftColumnWidth == 200);
    EXPECT(ui::kIdleTimerHz == 4);
    EXPECT(ui::kActiveTimerHz == 60);
    EXPECT(ui::heightForWidth(1280) == 660);
    EXPECT(ui::heightForWidth(1100) == 567);
    EXPECT(ui::heightForWidth(1760) == 908);

    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
        EXPECT(editor != nullptr);
        EXPECT(editor->getWidth() == ui::kDefaultWidth);
        EXPECT(editor->getHeight() == ui::heightForWidth(ui::kDefaultWidth));
    }

    squeeze::UiState savedWidth;
    savedWidth.windowWidth = 1500;
    processor.setUiState(savedWidth);

    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
        EXPECT(editor->getWidth() == 1500);
        EXPECT(editor->getHeight() == ui::heightForWidth(1500));
    }

    juce::AudioProcessor::BusesLayout stereo;
    stereo.inputBuses.add(juce::AudioChannelSet::stereo());
    stereo.outputBuses.add(juce::AudioChannelSet::stereo());
    EXPECT(processor.isBusesLayoutSupported(stereo));

    juce::AudioProcessor::BusesLayout mono;
    mono.inputBuses.add(juce::AudioChannelSet::mono());
    mono.outputBuses.add(juce::AudioChannelSet::mono());
    EXPECT(processor.isBusesLayoutSupported(mono));

    juce::AudioProcessor::BusesLayout mismatch;
    mismatch.inputBuses.add(juce::AudioChannelSet::stereo());
    mismatch.outputBuses.add(juce::AudioChannelSet::mono());
    EXPECT(!processor.isBusesLayoutSupported(mismatch));

    juce::AudioProcessor::BusesLayout surround;
    surround.inputBuses.add(juce::AudioChannelSet::create5point1());
    surround.outputBuses.add(juce::AudioChannelSet::create5point1());
    EXPECT(!processor.isBusesLayoutSupported(surround));
}

} // namespace squeeze::test
