#include "PluginProcessor.h"

#include "Latency.h"
#include "Parameters.h"
#include "PluginEditor.h"

#include <algorithm>

namespace squeeze
{

SqueezeProcessor::SqueezeProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "SQUEEZE", params::createParameterLayout())
{
    modeParameter = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(params::mode));
    jassert(modeParameter != nullptr);
}

SqueezeProcessor::~SqueezeProcessor() = default;

void SqueezeProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    engine.prepare(sampleRate, samplesPerBlock);
    engine.reset();

    const int modeIndex = modeParameter != nullptr ? modeParameter->getIndex() : 0;
    engine.setMode(modeFromIndex(modeIndex));
    setLatencySamples(kReportedLatencySamples);
}

void SqueezeProcessor::releaseResources()
{
    engine.reset();
}

bool SqueezeProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& input = layouts.getMainInputChannelSet();
    const auto& output = layouts.getMainOutputChannelSet();

    if (input != output)
        return false;

    return output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo();
}

void SqueezeProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midi);

    const int modeIndex = modeParameter != nullptr ? modeParameter->getIndex() : 0;
    engine.setMode(modeFromIndex(modeIndex));

    const auto numInputChannels = getTotalNumInputChannels();
    const auto numOutputChannels = getTotalNumOutputChannels();
    const auto numSamples = buffer.getNumSamples();

    for (int channel = numInputChannels; channel < numOutputChannels; ++channel)
        buffer.clear(channel, 0, numSamples);

    engine.process(buffer.getArrayOfWritePointers(), numInputChannels, numSamples);
}

void SqueezeProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();

    // Bypass keeps the same reported latency as processBlock. M1 latency is zero,
    // so the samples already in the buffer stay aligned.
    for (int channel = getMainBusNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());
}

juce::AudioProcessorEditor* SqueezeProcessor::createEditor()
{
    return new SqueezeEditor(*this);
}

bool SqueezeProcessor::hasEditor() const
{
    return true;
}

const juce::String SqueezeProcessor::getName() const
{
    return "SQUEEZE";
}

bool SqueezeProcessor::acceptsMidi() const
{
    return false;
}

bool SqueezeProcessor::producesMidi() const
{
    return false;
}

bool SqueezeProcessor::isMidiEffect() const
{
    return false;
}

double SqueezeProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SqueezeProcessor::getNumPrograms()
{
    return 1;
}

int SqueezeProcessor::getCurrentProgram()
{
    return 0;
}

void SqueezeProcessor::setCurrentProgram(int)
{
}

const juce::String SqueezeProcessor::getProgramName(int)
{
    return "Default";
}

void SqueezeProcessor::changeProgramName(int, const juce::String&)
{
}

void SqueezeProcessor::sanitise(UiState& state) const
{
    state.windowWidth = std::clamp(state.windowWidth, ui::kMinWidth, ui::kMaxWidth);
    state.abSlot = std::clamp(state.abSlot, 0, 1);

    if (state.selectedScene.length() > 128)
        state.selectedScene = state.selectedScene.substring(0, 128);

    if (static_cast<int>(state.history.size()) > kMaxHistoryEntries)
        state.history.erase(state.history.begin(),
                            state.history.end() - static_cast<std::ptrdiff_t>(kMaxHistoryEntries));

    for (auto& entry : state.history)
    {
        if (entry.time.length() > 512)
            entry.time = entry.time.substring(0, 512);
        if (entry.mode.length() > 512)
            entry.mode = entry.mode.substring(0, 512);
        if (entry.change.length() > 512)
            entry.change = entry.change.substring(0, 512);
        if (entry.sentence.length() > 512)
            entry.sentence = entry.sentence.substring(0, 512);
    }
}

UiState SqueezeProcessor::getUiState() const
{
    std::lock_guard<std::mutex> lock(uiStateMutex);
    return uiState;
}

void SqueezeProcessor::setUiState(UiState state)
{
    sanitise(state);
    std::lock_guard<std::mutex> lock(uiStateMutex);
    uiState = std::move(state);
}

std::unique_ptr<juce::XmlElement> SqueezeProcessor::createStateXml()
{
    auto xml = parametersToXml(apvts);

    if (xml == nullptr)
        xml = std::make_unique<juce::XmlElement>("SQUEEZE");

    xml->setAttribute("version", kStateVersion);

    UiState copy;

    {
        std::lock_guard<std::mutex> lock(uiStateMutex);
        sanitise(uiState);
        const auto slot = static_cast<size_t>(uiState.abSlot);
        uiState.slots[slot] = snapshotParameters(apvts);
        copy = uiState;
    }

    xml->addChildElement(uiToXml(copy).release());
    return xml;
}

void SqueezeProcessor::applyStateXml(const juce::XmlElement& xml)
{
    applyParameters(xml, apvts);

    auto loaded = uiFromXml(xml.getChildByName("ui"), apvts);
    const auto current = snapshotParameters(apvts);

    if (loaded.slots[0].empty())
        loaded.slots[0] = current;

    if (loaded.slots[1].empty())
        loaded.slots[1] = current;

    setUiState(std::move(loaded));
}

void SqueezeProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = createStateXml())
        copyXmlToBinary(*xml, destData);
}

void SqueezeProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        applyStateXml(*xml);
}

} // namespace squeeze

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new squeeze::SqueezeProcessor();
}
