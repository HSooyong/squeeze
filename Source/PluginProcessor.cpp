#include "PluginProcessor.h"

#include "MathUtil.h"
#include "Parameters.h"
#include "PluginEditor.h"
#include "Tunables.h"
#include "Ui/Scenes.h"

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

namespace
{

bool sameSnapshot(const std::vector<ParameterSnapshot>& before, const std::vector<ParameterSnapshot>& after)
{
    if (before.size() != after.size())
        return false;

    for (size_t index = 0; index < before.size(); ++index)
        if (before[index].id != after[index].id || std::abs(before[index].value - after[index].value) > 1.0e-3f)
            return false;

    return true;
}

juce::String modeName(int mode)
{
    switch (mode)
    {
        case 0: return "LAMP";
        case 1: return "NEEDLE";
        case 2: return "SPARK";
        default: return "TUBE";
    }
}

juce::String explainChange(const juce::String& id)
{
    if (id == params::optoPeakRed)
        return "The lamp drives the cell harder, so more of the level is held back.";
    if (id == params::optoGain || id == params::vcaOutput || id == params::fetOutput || id == params::vmuOutput)
        return "Output makeup changed. Gain reduction stays with the input.";
    if (id == params::optoLimit)
        return "Compress holds a gentle curve. Limit holds the level much harder.";
    if (id == params::vcaThreshold)
        return "The line moved, so a different amount of the signal crosses it.";
    if (id == params::vcaRatio)
        return "Only 1 dB of every extra dB above the line is kept, divided by the ratio.";
    if (id == params::fetInput)
        return "Push the signal into the fixed line. The bolt catches what crosses it.";
    if (id == params::fetAttack)
        return "This is how long the bolt takes to grab the peak.";
    if (id == params::fetRelease)
        return "This is how long the bolt takes to let go.";
    if (id == params::fetRatio)
        return "A higher ratio makes the bolt thicker and holds peaks flatter.";
    if (id == params::vmuInput)
        return "More input heat drives the tube harder.";
    if (id == params::vmuThreshold)
        return "The tube starts easing the level at this point.";
    if (id == params::vmuTime)
        return "The time constant sets how quickly the tube warms and cools.";
    if (id == params::mode)
        return "Each mode is a different classic way of holding level back.";

    return "The picture follows the engine.";
}

} // namespace

void SqueezeProcessor::applySnapshot(const std::vector<ParameterSnapshot>& snapshot)
{
    for (const auto& item : snapshot)
        if (auto* parameter = apvts.getParameter(item.id))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(item.value));
}

void SqueezeProcessor::showToast(juce::String text)
{
    toastTextValue = std::move(text);
    toastAtMs = juce::Time::currentTimeMillis();
}

void SqueezeProcessor::rememberHistory(juce::String change, juce::String sentence)
{
    auto state = getUiState();
    HistoryEntry entry;
    entry.time = juce::Time::getCurrentTime().formatted("%H:%M:%S");
    const int mode = modeParameter != nullptr ? modeParameter->getIndex() : 0;
    entry.mode = modeName(mode);
    entry.change = std::move(change);
    entry.sentence = std::move(sentence);
    state.history.push_back(std::move(entry));
    setUiState(std::move(state));
}

void SqueezeProcessor::beginGestureUndo()
{
    if (gestureOpen)
        return;

    gestureOpen = true;
    gestureStart = snapshotParameters(apvts);
}

void SqueezeProcessor::commitGesture()
{
    if (! gestureOpen)
        return;

    gestureOpen = false;
    const auto now = snapshotParameters(apvts);

    if (sameSnapshot(gestureStart, now))
        return;

    undoStack.push_back(gestureStart);
    if (undoStack.size() > 50)
        undoStack.erase(undoStack.begin());

    redoStack.clear();

    juce::String changedId;
    juce::String change;

    for (size_t index = 0; index < now.size() && index < gestureStart.size(); ++index)
    {
        if (std::abs(now[index].value - gestureStart[index].value) <= 1.0e-3f)
            continue;

        changedId = now[index].id;
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(changedId)))
            change = parameter->getName(40) + " " + parameter->getText(parameter->convertTo0to1(gestureStart[index].value), 16)
                     + " -> " + parameter->getText(parameter->getValue(), 16);
        else
            change = changedId;
    }

    const auto sentence = explainChange(changedId);
    rememberHistory(change, sentence);
    showToast(change + "  " + sentence);
}

void SqueezeProcessor::notifyLockedControl(int role)
{
    juce::String sentence = "This part is fixed for this method.";

    if (role == 5)
        sentence = "Valve speed is automatic. The further the level crosses the line, the faster it closes.";
    else if (role == 14)
        sentence = "The line is fixed. Raise INPUT to push the signal into it.";
    else if (role == 18)
        sentence = "The ratio rises on its own as the level climbs.";
    else if (role == 13)
        sentence = "ALL is off until Allow ALL is enabled in the menu.";

    showToast(sentence);
}

bool SqueezeProcessor::undoGesture()
{
    if (undoStack.empty())
        return false;

    redoStack.push_back(snapshotParameters(apvts));
    const auto snapshot = undoStack.back();
    undoStack.pop_back();
    applySnapshot(snapshot);

    auto state = getUiState();
    if (! state.history.empty())
        state.history.pop_back();
    setUiState(std::move(state));
    showToast("Undo");
    return true;
}

bool SqueezeProcessor::redoGesture()
{
    if (redoStack.empty())
        return false;

    undoStack.push_back(snapshotParameters(apvts));
    const auto snapshot = redoStack.back();
    redoStack.pop_back();
    applySnapshot(snapshot);
    showToast("Redo");
    return true;
}

void SqueezeProcessor::selectAbSlot(int slot)
{
    slot = std::clamp(slot, 0, 1);
    auto state = getUiState();

    if (state.abSlot == slot)
        return;

    state.slots[static_cast<size_t>(state.abSlot)] = snapshotParameters(apvts);

    if (state.slots[static_cast<size_t>(slot)].empty())
        state.slots[static_cast<size_t>(slot)] = state.slots[static_cast<size_t>(state.abSlot)];

    const auto next = state.slots[static_cast<size_t>(slot)];
    state.abSlot = slot;
    setUiState(std::move(state));
    applySnapshot(next);
    showToast(slot == 0 ? "Listening to A" : "Listening to B");
}

void SqueezeProcessor::applyScene(int mode, int index)
{
    int count = 0;
    const auto* scenes = ui::scenesForMode(mode, count);

    if (scenes == nullptr || index < 0 || index >= count)
        return;

    const auto& scene = scenes[index];
    beginGestureUndo();

    if (auto* parameter = apvts.getParameter(params::mode))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(mode)));

    for (int item = 0; item < scene.count; ++item)
        if (auto* parameter = apvts.getParameter(scene.ids[item]))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(scene.values[item]));

    auto state = getUiState();
    state.selectedScene = scene.name;
    setUiState(std::move(state));
    commitGesture();
}

juce::String SqueezeProcessor::toastText() const
{
    return toastTextValue;
}

} // namespace squeeze

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new squeeze::SqueezeProcessor();
}
