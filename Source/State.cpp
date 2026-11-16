#include "State.h"

#include <optional>

namespace squeeze
{
namespace
{

constexpr int kMaxSceneChars = 128;
constexpr int kMaxHistoryChars = 512;

juce::String clip(juce::String text, int maxChars)
{
    if (text.length() > maxChars)
        return text.substring(0, maxChars);

    return text;
}

void trimHistory(std::vector<HistoryEntry>& history)
{
    if (static_cast<int>(history.size()) > kMaxHistoryEntries)
        history.erase(history.begin(), history.end() - static_cast<std::ptrdiff_t>(kMaxHistoryEntries));
}

juce::String captionsToString(Captions captions)
{
    switch (captions)
    {
        case Captions::always:
            return "always";
        case Captions::off:
            return "off";
        case Captions::lessonOnly:
            break;
    }

    return "lesson";
}

Captions captionsFromString(const juce::String& text)
{
    if (text.equalsIgnoreCase("always"))
        return Captions::always;

    if (text.equalsIgnoreCase("off"))
        return Captions::off;

    return Captions::lessonOnly;
}

bool readBool(const juce::XmlElement& xml, const char* name, bool fallback)
{
    if (!xml.hasAttribute(name))
        return fallback;

    const auto text = xml.getStringAttribute(name).trim();

    if (text.equalsIgnoreCase("true") || text == "1")
        return true;

    if (text.equalsIgnoreCase("false") || text == "0")
        return false;

    return fallback;
}

std::optional<float> parameterValue(const juce::XmlElement& xml, const juce::String& id)
{
    for (auto* child = xml.getFirstChildElement(); child != nullptr; child = child->getNextElement())
    {
        if (!child->hasTagName("PARAM") || child->getStringAttribute("id") != id || !child->hasAttribute("value"))
            continue;

        return static_cast<float>(child->getDoubleAttribute("value"));
    }

    return std::nullopt;
}

const juce::XmlElement* findSlot(const juce::XmlElement& ui, const char* slotId)
{
    for (auto* child = ui.getFirstChildElement(); child != nullptr; child = child->getNextElement())
    {
        if (child->hasTagName("slot") && child->getStringAttribute("id") == slotId)
            return child;
    }

    return nullptr;
}

std::vector<ParameterSnapshot> readSlot(const juce::XmlElement* slot, const juce::AudioProcessorValueTreeState& state)
{
    std::vector<ParameterSnapshot> snapshot;

    if (slot == nullptr)
        return snapshot;

    for (auto* parameter : state.processor.getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
        {
            const auto id = ranged->getParameterID();

            if (slot->hasAttribute(id))
                snapshot.push_back({ id, static_cast<float>(slot->getDoubleAttribute(id)) });
        }
    }

    return snapshot;
}

void writeSlot(juce::XmlElement& ui, const char* slotId, const std::vector<ParameterSnapshot>& snapshot)
{
    auto* slot = ui.createNewChildElement("slot");
    slot->setAttribute("id", slotId);

    for (const auto& item : snapshot)
        slot->setAttribute(item.id, static_cast<double>(item.value));
}

float defaultDenormalised(const juce::RangedAudioParameter& parameter)
{
    return parameter.convertFrom0to1(parameter.getDefaultValue());
}

} // namespace

std::vector<ParameterSnapshot> snapshotParameters(const juce::AudioProcessorValueTreeState& state)
{
    std::vector<ParameterSnapshot> snapshot;

    for (auto* parameter : state.processor.getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
            snapshot.push_back({ ranged->getParameterID(), ranged->convertFrom0to1(ranged->getValue()) });
    }

    return snapshot;
}

std::unique_ptr<juce::XmlElement> parametersToXml(juce::AudioProcessorValueTreeState& state)
{
    return state.copyState().createXml();
}

void applyParameters(const juce::XmlElement& xml, juce::AudioProcessorValueTreeState& state)
{
    juce::ValueTree tree(state.state.getType());

    for (auto* parameter : state.processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);

        if (ranged == nullptr)
            continue;

        const auto id = ranged->getParameterID();
        const auto incoming = parameterValue(xml, id);
        const float value = incoming.has_value() ? *incoming : defaultDenormalised(*ranged);

        juce::ValueTree child("PARAM");
        child.setProperty("id", id, nullptr);
        child.setProperty("value", value, nullptr);
        tree.appendChild(child, nullptr);
    }

    state.replaceState(tree);
}

std::unique_ptr<juce::XmlElement> uiToXml(const UiState& ui)
{
    auto xml = std::make_unique<juce::XmlElement>("ui");
    xml->setAttribute("windowWidth", ui.windowWidth);
    xml->setAttribute("learnOpen", ui.learnOpen ? 1 : 0);
    xml->setAttribute("meterOpen", ui.meterOpen ? 1 : 0);
    xml->setAttribute("captions", captionsToString(ui.captions));
    xml->setAttribute("selectedScene", ui.selectedScene);
    xml->setAttribute("abSlot", ui.abSlot == 1 ? "B" : "A");

    auto* history = xml->createNewChildElement("history");

    for (const auto& entry : ui.history)
    {
        auto* item = history->createNewChildElement("entry");
        item->setAttribute("time", entry.time);
        item->setAttribute("mode", entry.mode);
        item->setAttribute("change", entry.change);
        item->setAttribute("sentence", entry.sentence);
    }

    writeSlot(*xml, "A", ui.slots[0]);
    writeSlot(*xml, "B", ui.slots[1]);
    return xml;
}

UiState uiFromXml(const juce::XmlElement* ui, const juce::AudioProcessorValueTreeState& state)
{
    UiState loaded;

    if (ui == nullptr)
        return loaded;

    if (ui->hasAttribute("windowWidth"))
    {
        loaded.windowWidth = juce::jlimit(ui::kMinWidth,
                                           ui::kMaxWidth,
                                           ui->getIntAttribute("windowWidth", ui::kDefaultWidth));
    }

    loaded.learnOpen = readBool(*ui, "learnOpen", loaded.learnOpen);
    loaded.meterOpen = readBool(*ui, "meterOpen", loaded.meterOpen);

    if (ui->hasAttribute("captions"))
        loaded.captions = captionsFromString(ui->getStringAttribute("captions"));

    if (ui->hasAttribute("selectedScene"))
        loaded.selectedScene = clip(ui->getStringAttribute("selectedScene"), kMaxSceneChars);

    if (ui->hasAttribute("abSlot"))
    {
        const auto slot = ui->getStringAttribute("abSlot").trim();
        loaded.abSlot = (slot.equalsIgnoreCase("B") || slot == "1") ? 1 : 0;
    }

    if (auto* history = ui->getChildByName("history"))
    {
        for (auto* entry = history->getFirstChildElement(); entry != nullptr; entry = entry->getNextElement())
        {
            if (!entry->hasTagName("entry"))
                continue;

            HistoryEntry item;
            item.time = clip(entry->getStringAttribute("time"), kMaxHistoryChars);
            item.mode = clip(entry->getStringAttribute("mode"), kMaxHistoryChars);
            item.change = clip(entry->getStringAttribute("change"), kMaxHistoryChars);
            item.sentence = clip(entry->getStringAttribute("sentence"), kMaxHistoryChars);
            loaded.history.push_back(std::move(item));
        }

        trimHistory(loaded.history);
    }

    loaded.slots[0] = readSlot(findSlot(*ui, "A"), state);
    loaded.slots[1] = readSlot(findSlot(*ui, "B"), state);
    return loaded;
}

} // namespace squeeze
