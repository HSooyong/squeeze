#include "Check.h"
#include "Parameters.h"
#include "PluginProcessor.h"

#include <utility>
#include <vector>

namespace
{

float currentValue(squeeze::SqueezeProcessor& processor, const char* id)
{
    auto* parameter = processor.getApvts().getParameter(id);
    return parameter->convertFrom0to1(parameter->getValue());
}

void setValue(squeeze::SqueezeProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.getApvts().getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

bool hasSnapshot(const std::vector<squeeze::ParameterSnapshot>& slot, const char* id, float expected)
{
    for (const auto& item : slot)
    {
        if (item.id == id)
            return squeeze::test::near(item.value, expected, 1.0e-3f);
    }

    return false;
}

void setNonDefaults(squeeze::SqueezeProcessor& processor)
{
    setValue(processor, squeeze::params::mode, 2.0f);
    setValue(processor, squeeze::params::osFactor, 2.0f);
    setValue(processor, squeeze::params::abLevelMatch, 0.0f);
    setValue(processor, squeeze::params::optoPeakRed, 75.0f);
    setValue(processor, squeeze::params::optoGain, 6.0f);
    setValue(processor, squeeze::params::optoLimit, 1.0f);
    setValue(processor, squeeze::params::vcaThreshold, -24.0f);
    setValue(processor, squeeze::params::vcaRatio, 8.0f);
    setValue(processor, squeeze::params::vcaOutput, -3.0f);
    setValue(processor, squeeze::params::fetInput, 36.0f);
    setValue(processor, squeeze::params::fetOutput, 4.0f);
    setValue(processor, squeeze::params::fetAttack, 100.0f);
    setValue(processor, squeeze::params::fetRelease, 400.0f);
    setValue(processor, squeeze::params::fetRatio, 3.0f);
    setValue(processor, squeeze::params::fetAllEnabled, 1.0f);
    setValue(processor, squeeze::params::vmuInput, -6.0f);
    setValue(processor, squeeze::params::vmuThreshold, -30.0f);
    setValue(processor, squeeze::params::vmuTime, 5.0f);
    setValue(processor, squeeze::params::vmuOutput, 2.0f);
}

void expectMatches(squeeze::SqueezeProcessor& actual, squeeze::SqueezeProcessor& expected)
{
    const char* ids[] = {
        squeeze::params::mode,
        squeeze::params::osFactor,
        squeeze::params::abLevelMatch,
        squeeze::params::optoPeakRed,
        squeeze::params::optoGain,
        squeeze::params::optoLimit,
        squeeze::params::vcaThreshold,
        squeeze::params::vcaRatio,
        squeeze::params::vcaOutput,
        squeeze::params::fetInput,
        squeeze::params::fetOutput,
        squeeze::params::fetAttack,
        squeeze::params::fetRelease,
        squeeze::params::fetRatio,
        squeeze::params::fetAllEnabled,
        squeeze::params::vmuInput,
        squeeze::params::vmuThreshold,
        squeeze::params::vmuTime,
        squeeze::params::vmuOutput,
    };

    for (const auto* id : ids)
        EXPECT(squeeze::test::near(currentValue(actual, id), currentValue(expected, id), 1.0e-3f));
}

squeeze::UiState populatedUi()
{
    squeeze::UiState ui;
    ui.windowWidth = 1500;
    ui.learnOpen = true;
    ui.meterOpen = true;
    ui.captions = squeeze::Captions::off;
    ui.selectedScene = "Vocal steady";
    ui.abSlot = 0;
    ui.history.push_back({ "12:00", "LAMP", "Peak Reduction 40 -> 75", juce::String::fromUTF8("선을 내리면 더 많이 잠깁니다.") });
    ui.history.push_back({ "12:01", "SPARK", "Attack 300 -> 100", "Faster" });
    ui.slots[0].push_back({ squeeze::params::mode, 0.0f });
    ui.slots[1].push_back({ squeeze::params::vcaThreshold, -12.0f });
    return ui;
}

} // namespace

namespace squeeze::test
{

void runState()
{
    SqueezeProcessor original;
    setNonDefaults(original);
    original.setUiState(populatedUi());

    juce::MemoryBlock block;
    original.getStateInformation(block);
    EXPECT(block.getSize() > 0);

    SqueezeProcessor restored;
    setValue(restored, params::optoPeakRed, 1.0f);
    restored.setStateInformation(block.getData(), static_cast<int>(block.getSize()));
    expectMatches(restored, original);

    const auto restoredUi = restored.getUiState();
    EXPECT(restoredUi.windowWidth == 1500);
    EXPECT(restoredUi.learnOpen);
    EXPECT(restoredUi.meterOpen);
    EXPECT(restoredUi.captions == Captions::off);
    EXPECT(restoredUi.selectedScene == "Vocal steady");
    EXPECT(restoredUi.abSlot == 0);
    EXPECT(restoredUi.history.size() == 2);
    EXPECT(restoredUi.history[0].sentence == juce::String::fromUTF8("선을 내리면 더 많이 잠깁니다."));
    EXPECT(restoredUi.slots[0].size() == 19);
    EXPECT(hasSnapshot(restoredUi.slots[0], params::mode, 2.0f));
    EXPECT(restoredUi.slots[1].size() == 1);
    EXPECT(hasSnapshot(restoredUi.slots[1], params::vcaThreshold, -12.0f));

    auto xml = original.createStateXml();
    EXPECT(xml != nullptr);
    EXPECT(xml->getIntAttribute("version") == kStateVersion);

    xml->setAttribute("mystery", 5);
    xml->createNewChildElement("future");
    auto* unknown = xml->createNewChildElement("PARAM");
    unknown->setAttribute("id", "not_a_parameter");
    unknown->setAttribute("value", 9.0);

    for (auto* child = xml->getFirstChildElement(); child != nullptr; child = child->getNextElement())
    {
        if (child->hasTagName("PARAM") && child->getStringAttribute("id") == params::vcaThreshold)
        {
            xml->removeChildElement(child, true);
            break;
        }
    }

    if (auto* ui = xml->getChildByName("ui"))
    {
        ui->setAttribute("futureFlag", 1);
        if (auto* history = ui->getChildByName("history"))
            history->createNewChildElement("note");
    }

    SqueezeProcessor partial;
    setValue(partial, params::vcaThreshold, -10.0f);
    partial.applyStateXml(*xml);
    EXPECT(near(currentValue(partial, params::vcaThreshold), -18.0f, 1.0e-3f));
    EXPECT(near(currentValue(partial, params::optoPeakRed), currentValue(original, params::optoPeakRed), 1.0e-3f));
    EXPECT(partial.getUiState().windowWidth == 1500);

    auto rewritten = partial.createStateXml();
    bool mysterySurvived = false;

    for (auto* child = rewritten->getFirstChildElement(); child != nullptr; child = child->getNextElement())
    {
        if (child->hasTagName("PARAM") && child->getStringAttribute("id") == "not_a_parameter")
            mysterySurvived = true;
    }

    EXPECT(!mysterySurvived);
    EXPECT(!rewritten->hasAttribute("mystery"));

    juce::XmlElement legacy("SQUEEZE");
    legacy.setAttribute("version", 0);
    auto* mode = legacy.createNewChildElement("PARAM");
    mode->setAttribute("id", params::mode);
    mode->setAttribute("value", 3.0);
    auto* time = legacy.createNewChildElement("PARAM");
    time->setAttribute("id", params::vmuTime);
    time->setAttribute("value", 5.0);

    SqueezeProcessor oldDocument;
    setValue(oldDocument, params::optoGain, 9.0f);
    oldDocument.applyStateXml(legacy);
    EXPECT(near(currentValue(oldDocument, params::mode), 3.0f));
    EXPECT(near(currentValue(oldDocument, params::vmuTime), 5.0f));
    EXPECT(near(currentValue(oldDocument, params::optoGain), 0.0f));
    EXPECT(near(currentValue(oldDocument, params::vcaThreshold), -18.0f));
    EXPECT(oldDocument.getUiState().windowWidth == ui::kDefaultWidth);
    EXPECT(oldDocument.getUiState().captions == Captions::lessonOnly);
    EXPECT(oldDocument.getUiState().slots[0].size() == 19);
    EXPECT(oldDocument.getUiState().slots[1].size() == 19);
    EXPECT(oldDocument.createStateXml()->getIntAttribute("version") == kStateVersion);

    juce::XmlElement unversioned("SQUEEZE");
    auto* peak = unversioned.createNewChildElement("PARAM");
    peak->setAttribute("id", params::optoPeakRed);
    peak->setAttribute("value", 70.0);

    SqueezeProcessor missingVersion;
    missingVersion.applyStateXml(unversioned);
    EXPECT(near(currentValue(missingVersion, params::optoPeakRed), 70.0f));
    EXPECT(near(currentValue(missingVersion, params::mode), 0.0f));

    juce::XmlElement future("SQUEEZE");
    future.setAttribute("version", 99);
    auto* gain = future.createNewChildElement("PARAM");
    gain->setAttribute("id", params::optoGain);
    gain->setAttribute("value", 3.0);
    future.setAttribute("not_a_real_key", 12);

    SqueezeProcessor futureDocument;
    futureDocument.applyStateXml(future);
    EXPECT(near(currentValue(futureDocument, params::optoGain), 3.0f));
    EXPECT(near(currentValue(futureDocument, params::optoPeakRed), 40.0f));

    UiState longHistory;
    longHistory.history.resize(55);

    for (int index = 0; index < 55; ++index)
        longHistory.history[static_cast<size_t>(index)].change = juce::String(index);

    SqueezeProcessor trimmed;
    trimmed.setUiState(longHistory);
    const auto kept = trimmed.getUiState().history;
    EXPECT(kept.size() == 50);
    EXPECT(kept.front().change == "5");
    EXPECT(kept.back().change == "54");

    juce::XmlElement clamped("SQUEEZE");
    auto* ui = clamped.createNewChildElement("ui");
    ui->setAttribute("windowWidth", 50);
    ui->setAttribute("abSlot", "nope");
    ui->setAttribute("captions", "sometimes");

    SqueezeProcessor clampedProcessor;
    clampedProcessor.applyStateXml(clamped);
    EXPECT(clampedProcessor.getUiState().windowWidth == ui::kMinWidth);
    EXPECT(clampedProcessor.getUiState().abSlot == 0);
    EXPECT(clampedProcessor.getUiState().captions == Captions::lessonOnly);

    ui->setAttribute("windowWidth", 9000);
    ui->setAttribute("abSlot", "B");
    clampedProcessor.applyStateXml(clamped);
    EXPECT(clampedProcessor.getUiState().windowWidth == ui::kMaxWidth);
    EXPECT(clampedProcessor.getUiState().abSlot == 1);

    SqueezeProcessor untouched;
    setValue(untouched, params::fetInput, 12.0f);
    const char junk[] = "this is not state";
    untouched.setStateInformation(junk, static_cast<int>(sizeof(junk)));
    untouched.setStateInformation(nullptr, 0);
    EXPECT(near(currentValue(untouched, params::fetInput), 12.0f, 1.0e-3f));

    restored.prepareToPlay(48000.0, 32);
    juce::AudioBuffer<float> buffer(2, 32);
    buffer.clear();
    buffer.setSample(0, 0, 0.25f);
    auto expected = buffer;
    juce::MidiBuffer midi;
    restored.processBlock(buffer, midi);
    EXPECT(bitsEqual(buffer.getSample(0, 0), expected.getSample(0, 0)));
    EXPECT(restored.getLatencySamples() == 0);
}

} // namespace squeeze::test
