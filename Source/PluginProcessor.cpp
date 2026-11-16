#include "PluginProcessor.h"

#include "MathUtil.h"
#include "Parameters.h"
#include "PluginEditor.h"
#include "Tunables.h"

#include <algorithm>
#include <cmath>

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

    auto raw = [this](const char* id) { return apvts.getRawParameterValue(id); };
    modeRaw = raw(params::mode);
    osRaw = raw(params::osFactor);
    levelMatchRaw = raw(params::abLevelMatch);
    optoPeakRaw = raw(params::optoPeakRed);
    optoGainRaw = raw(params::optoGain);
    optoLimitRaw = raw(params::optoLimit);
    vcaThresholdRaw = raw(params::vcaThreshold);
    vcaRatioRaw = raw(params::vcaRatio);
    vcaOutputRaw = raw(params::vcaOutput);
    fetInputRaw = raw(params::fetInput);
    fetOutputRaw = raw(params::fetOutput);
    fetAttackRaw = raw(params::fetAttack);
    fetReleaseRaw = raw(params::fetRelease);
    fetRatioRaw = raw(params::fetRatio);
    fetAllRaw = raw(params::fetAllEnabled);
    vmuInputRaw = raw(params::vmuInput);
    vmuThresholdRaw = raw(params::vmuThreshold);
    vmuTimeRaw = raw(params::vmuTime);
    vmuOutputRaw = raw(params::vmuOutput);
}

SqueezeProcessor::~SqueezeProcessor() = default;

EngineTargets SqueezeProcessor::readTargets() const noexcept
{
    EngineTargets targets;
    targets.mode = modeFromIndex(juce::roundToInt(modeRaw->load()));
    targets.levelMatch = levelMatchRaw->load() >= 0.5f;
    targets.optoPeakReduction = optoPeakRaw->load();
    targets.optoOutputDb = optoGainRaw->load();
    targets.optoLimit = optoLimitRaw->load();
    targets.vcaThresholdDb = vcaThresholdRaw->load();
    targets.vcaRatio = vcaRatioRaw->load();
    targets.vcaOutputDb = vcaOutputRaw->load();
    targets.fetInputDb = fetInputRaw->load() - 24.0f;
    targets.fetOutputDb = fetOutputRaw->load();
    targets.fetAttackSeconds = fetAttackRaw->load() * 1.0e-6f;
    targets.fetReleaseSeconds = fetReleaseRaw->load() * 1.0e-3f;
    targets.fetRatioIndex = fetRatioRaw->load();
    targets.fetAllEnabled = fetAllRaw->load() >= 0.5f;
    targets.vmuInputDb = vmuInputRaw->load();
    targets.vmuThresholdDb = vmuThresholdRaw->load();
    targets.vmuOutputDb = vmuOutputRaw->load();
    targets.vmuTimeIndex = vmuTimeRaw->load();
    return targets;
}

void SqueezeProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    preparedSamples = std::max(samplesPerBlock, 8192);
    const int channels = std::max(1, getTotalNumOutputChannels());
    dryLeft.assign(static_cast<size_t>(preparedSamples), 0.0f);
    dryRight.assign(static_cast<size_t>(preparedSamples), 0.0f);
    colorDrive.assign(static_cast<size_t>(preparedSamples), 0.0f);

    engine.prepare(sampleRate, channels);
    colorStage.prepare(sampleRate, preparedSamples, channels);
    engine.snapTo(readTargets());
    activeOversampling = std::clamp(juce::roundToInt(osRaw->load()), 0, 2);
    setLatencySamples(colorStage.latencyForChoice(activeOversampling));
}

void SqueezeProcessor::releaseResources()
{
    engine.reset();
    colorStage.reset();
}

void SqueezeProcessor::publishFrame(const VisualFrame& frame) noexcept
{
    latestFrame = frame;
    publishedGr.store(frame.grDb);
    publishedIn.store(frame.inDb);
    publishedOut.store(frame.outDb);

    int start1 = 0;
    int size1 = 0;
    int start2 = 0;
    int size2 = 0;
    visualFifo.prepareToWrite(1, start1, size1, start2, size2);
    if (size1 <= 0)
        return;

    visualFrames[static_cast<size_t>(start1)] = frame;
    visualFifo.finishedWrite(1);
}

bool SqueezeProcessor::popVisualFrame(VisualFrame& frame) noexcept
{
    int start1 = 0;
    int size1 = 0;
    int start2 = 0;
    int size2 = 0;
    visualFifo.prepareToRead(1, start1, size1, start2, size2);
    if (size1 <= 0)
        return false;

    frame = visualFrames[static_cast<size_t>(start1)];
    visualFifo.finishedRead(1);
    return true;
}

bool SqueezeProcessor::hasBadAudioState() const noexcept
{
    return engine.hasBadState() || colorStage.hasBadState();
}

int SqueezeProcessor::oversamplingLatencySamples() const noexcept
{
    return colorStage.latencyForChoice(activeOversampling);
}

void SqueezeProcessor::processChunk(float* const* channels, int numChannels, int numSamples, const EngineTargets& targets, int oversamplingIndex)
{
    for (int index = 0; index < numSamples; ++index)
    {
        dryLeft[static_cast<size_t>(index)] = channels[0][index];
        if (numChannels > 1)
            dryRight[static_cast<size_t>(index)] = channels[1][index];
    }

    VisualFrame frame;
    engine.process(channels, colorDrive.data(), numChannels, numSamples, targets, frame);
    colorStage.process(channels, colorDrive.data(), numChannels, numSamples, oversamplingIndex);

    float* dryPointers[2] = { dryLeft.data(), numChannels > 1 ? dryRight.data() : dryLeft.data() };
    engine.applyLevelMatch(dryPointers, channels, numChannels, numSamples, targets.levelMatch);

    float inPeak = 0.0f;
    float outPeak = 0.0f;
    for (int channel = 0; channel < numChannels; ++channel)
    {
        for (int index = 0; index < numSamples; ++index)
        {
            float sample = channels[channel][index];
            sample = clampFloat(sample, -tunables::outputSafety, tunables::outputSafety);
            channels[channel][index] = sample;
            inPeak = std::max(inPeak, std::fabs(dryPointers[channel][index]));
            outPeak = std::max(outPeak, std::fabs(sample));
        }
    }

    frame.inDb = linearToDb(inPeak);
    frame.outDb = linearToDb(outPeak);
    frame.lmTrimDb = engine.levelMatchTrimDb();
    const float difference = colorStage.lastDifferenceRms();
    if (targets.mode == Mode::fet)
        frame.fetGrit = difference;
    else if (targets.mode == Mode::variMu)
        frame.vmuHarmonics = difference;

    publishFrame(frame);
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

    const auto numInputChannels = getTotalNumInputChannels();
    const auto numOutputChannels = getTotalNumOutputChannels();
    const auto numSamples = buffer.getNumSamples();

    for (int channel = numInputChannels; channel < numOutputChannels; ++channel)
        buffer.clear(channel, 0, numSamples);

    const int oversamplingIndex = std::clamp(juce::roundToInt(osRaw->load()), 0, 2);
    if (oversamplingIndex != activeOversampling)
    {
        activeOversampling = oversamplingIndex;
        setLatencySamples(colorStage.latencyForChoice(activeOversampling));
    }

    const EngineTargets targets = readTargets();
    const int channels = std::max(1, numInputChannels);
    int offset = 0;
    while (offset < numSamples)
    {
        const int count = std::min(preparedSamples > 0 ? preparedSamples : numSamples, numSamples - offset);
        float* pointers[2] = {
            buffer.getWritePointer(0) + offset,
            channels > 1 ? buffer.getWritePointer(1) + offset : buffer.getWritePointer(0) + offset,
        };
        processChunk(pointers, channels, count, targets, oversamplingIndex);
        offset += count;
    }
}

void SqueezeProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();

    const int channels = std::max(1, getTotalNumInputChannels());
    const int oversamplingIndex = std::clamp(juce::roundToInt(osRaw->load()), 0, 2);
    if (oversamplingIndex != activeOversampling)
    {
        activeOversampling = oversamplingIndex;
        setLatencySamples(colorStage.latencyForChoice(activeOversampling));
    }

    colorStage.process(buffer.getArrayOfWritePointers(), nullptr, channels, buffer.getNumSamples(), oversamplingIndex);

    for (int channel = 0; channel < channels; ++channel)
    {
        float* samples = buffer.getWritePointer(channel);
        for (int index = 0; index < buffer.getNumSamples(); ++index)
            samples[index] = clampFloat(samples[index], -tunables::outputSafety, tunables::outputSafety);
    }
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
