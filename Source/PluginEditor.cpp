#include "PluginEditor.h"

#include "EditorLayout.h"
#include "Parameters.h"
#include "Ui/SqueezeBoxes.h"
#include "Ui/Strings.h"
#include "Ui/Theme.h"

#include <algorithm>

namespace squeeze
{

SqueezeEditor::SqueezeEditor(SqueezeProcessor& processorIn)
    : juce::AudioProcessorEditor(processorIn),
      processor(processorIn),
      gain(processorIn),
      scene(processorIn),
      output(processorIn),
      learnColumn(processorIn),
      learnDock(processorIn, trace),
      meterDrawer(processorIn, trace)
{
    const auto width = std::clamp(processor.getUiState().windowWidth, ui::kMinWidth, ui::kMaxWidth);

    setOpaque(true);
    setName("SQUEEZE");
    setWantsKeyboardFocus(true);

    addAndMakeVisible(logo);
    addAndMakeVisible(gain);
    addAndMakeVisible(scene);
    addAndMakeVisible(output);
    addAndMakeVisible(meterHandle);
    addChildComponent(learnColumn);
    addChildComponent(learnDock);
    addChildComponent(meterDrawer);

    for (int index = 0; index < 4; ++index)
    {
        modeTabs[static_cast<size_t>(index)].setTexts(ui::strings::modePrimary(index), ui::strings::modeDetail(index));
        addAndMakeVisible(modeTabs[static_cast<size_t>(index)]);
        modeTabs[static_cast<size_t>(index)].onClick = [this, index] { selectMode(index); };
    }

    slotA.setTexts("A", {});
    slotB.setTexts("B", {});
    learn.setTexts(ui::strings::learnLabel(), {});
    meter.setTexts(ui::strings::meterLabel(), {});
    meterHandle.setTexts(ui::strings::meterLabel(), {});
    addAndMakeVisible(slotA);
    addAndMakeVisible(slotB);
    addAndMakeVisible(learn);
    addAndMakeVisible(meter);
    addAndMakeVisible(menu);

    slotA.onClick = [this] { selectSlot(0); };
    slotB.onClick = [this] { selectSlot(1); };
    learn.onClick = [this] { toggleLearn(); };
    meter.onClick = [this] { toggleMeter(); };
    meterHandle.onClick = [this] { toggleMeter(); };
    menu.onClick = [this] { showMenu(); };
    learnColumn.onHear = [this] {
        const int slot = processor.getUiState().abSlot == 0 ? 1 : 0;
        selectSlot(slot);
    };
    learnColumn.onUndo = [this] { processor.undoGesture(); };
    learnColumn.onWhy = [this] { processor.notifyLockedControl(0); };

    suppressStateWrite = true;
    setResizable(true, true);
    setResizeLimits(ui::kMinWidth,
                    ui::heightForWidth(ui::kMinWidth),
                    ui::kMaxWidth,
                    ui::heightForWidth(ui::kMaxWidth));
    getConstrainer()->setFixedAspectRatio(static_cast<double>(ui::kDefaultWidth) / static_cast<double>(ui::kDesignHeight));
    suppressStateWrite = false;

    setSize(width, ui::heightForWidth(width));
    refreshHeader();
    listen(true);
    startTimerHz(ui::kIdleTimerHz);
}

SqueezeEditor::~SqueezeEditor()
{
    stopTimer();
    listen(false);
    cancelPendingUpdate();
}

void SqueezeEditor::paint(juce::Graphics& g)
{
    const auto boxes = ui::squeezeBoxes(getWidth(), getHeight(), appliedLearn, appliedMeter);
    g.fillAll(ui::kBackground);
    g.setColour(ui::kHeader);
    g.fillRect(boxes.header);
    g.setColour(ui::kLine);
    g.drawLine(0.0f, static_cast<float>(boxes.header.getBottom()), static_cast<float>(getWidth()), static_cast<float>(boxes.header.getBottom()), 1.0f);
}

void SqueezeEditor::paintOverChildren(juce::Graphics& g)
{
    if (! toastVisible())
        return;

    const auto text = processor.toastText();
    const auto width = juce::jmin(420, getWidth() - 48);
    auto bounds = juce::Rectangle<int>(24, getHeight() - 78, width, 36);
    g.setColour(juce::Colour(0xff1c1f26).withAlpha(0.94f));
    g.fillRoundedRectangle(bounds.toFloat(), 8.0f);
    g.setColour(ui::kText);
    g.setFont(ui::interfaceFont(13.0f));
    g.drawText(text, bounds.reduced(10, 0), juce::Justification::centredLeft, true);
}

void SqueezeEditor::resized()
{
    if (getWidth() < ui::kMinWidth)
        return;

    const auto state = processor.getUiState();
    appliedLearn = state.learnOpen;
    appliedMeter = state.meterOpen;
    const auto boxes = ui::squeezeBoxes(getWidth(), getHeight(), appliedLearn, appliedMeter);
    logo.setBounds(boxes.logo);

    for (int index = 0; index < 4; ++index)
        modeTabs[static_cast<size_t>(index)].setBounds(boxes.modeTabs[static_cast<size_t>(index)]);

    slotA.setBounds(boxes.slotA);
    slotB.setBounds(boxes.slotB);
    learn.setBounds(boxes.learn);
    meter.setBounds(boxes.meter);
    menu.setBounds(boxes.menu);
    gain.setBounds(boxes.gain);
    scene.setBounds(boxes.scene);
    output.setBounds(boxes.output);
    meterHandle.setBounds(boxes.meterHandle);
    learnColumn.setBounds(boxes.learnBody);
    learnDock.setBounds(boxes.dock);
    meterDrawer.setBounds(boxes.meterDrawer);
    learnColumn.setVisible(appliedLearn && ! boxes.learnBody.isEmpty());
    learnDock.setVisible(appliedLearn && ! boxes.dock.isEmpty());
    meterDrawer.setVisible(appliedMeter && ! boxes.meterDrawer.isEmpty());

    if (suppressStateWrite)
        return;

    const int width = getWidth();

    if (width < ui::kMinWidth || width > ui::kMaxWidth)
        return;

    auto stored = processor.getUiState();

    if (stored.windowWidth == width)
        return;

    stored.windowWidth = width;
    processor.setUiState(std::move(stored));
}

bool SqueezeEditor::keyPressed(const juce::KeyPress& key)
{
    const auto character = juce::CharacterFunctions::toLowerCase(key.getTextCharacter());

    if (key.getModifiers().isCommandDown() && character == 'z')
    {
        if (key.getModifiers().isShiftDown())
            processor.redoGesture();
        else
            processor.undoGesture();
        return true;
    }

    if (character == 'l')
    {
        toggleLearn();
        return true;
    }

    if (character == 'm')
    {
        toggleMeter();
        return true;
    }

    if (character == 'a')
    {
        selectSlot(0);
        return true;
    }

    if (character == 'b')
    {
        selectSlot(1);
        return true;
    }

    return false;
}

bool SqueezeEditor::layoutIsValid() const
{
    return layoutReport().empty();
}

std::string SqueezeEditor::layoutReport() const
{
    std::string problems;
    const auto state = processor.getUiState();
    const auto boxes = ui::squeezeBoxes(getWidth(), getHeight(), state.learnOpen, state.meterOpen);
    const juce::Rectangle<int> parent = getLocalBounds();

    struct Named
    {
        const char* name;
        juce::Rectangle<int> bounds;
        bool textOk;
        bool active;
    };

    Named named[] = {
        { "logo", logo.getBounds(), logo.contentFits(), true },
        { "tab0", modeTabs[0].getBounds(), modeTabs[0].contentFits(), true },
        { "tab1", modeTabs[1].getBounds(), modeTabs[1].contentFits(), true },
        { "tab2", modeTabs[2].getBounds(), modeTabs[2].contentFits(), true },
        { "tab3", modeTabs[3].getBounds(), modeTabs[3].contentFits(), true },
        { "slotA", slotA.getBounds(), slotA.contentFits(), true },
        { "slotB", slotB.getBounds(), slotB.contentFits(), true },
        { "learn", learn.getBounds(), learn.contentFits(), true },
        { "meter", meter.getBounds(), meter.contentFits(), true },
        { "menu", menu.getBounds(), menu.contentFits(), true },
        { "gain", gain.getBounds(), gain.contentFits(), true },
        { "scene", scene.getBounds(), true, true },
        { "output", output.getBounds(), output.contentFits(), true },
        { "meterHandle", meterHandle.getBounds(), meterHandle.contentFits(), true },
        { "learnColumn", learnColumn.getBounds(), true, learnColumn.isVisible() },
        { "learnDock", learnDock.getBounds(), true, learnDock.isVisible() },
        { "meterDrawer", meterDrawer.getBounds(), true, meterDrawer.isVisible() },
    };

    const juce::Rectangle<int> expected[] = {
        boxes.logo,
        boxes.modeTabs[0],
        boxes.modeTabs[1],
        boxes.modeTabs[2],
        boxes.modeTabs[3],
        boxes.slotA,
        boxes.slotB,
        boxes.learn,
        boxes.meter,
        boxes.menu,
        boxes.gain,
        boxes.scene,
        boxes.output,
        boxes.meterHandle,
        boxes.learnBody,
        boxes.dock,
        boxes.meterDrawer,
    };

    for (size_t index = 0; index < std::size(named); ++index)
    {
        if (! named[index].active)
            continue;

        if (named[index].bounds != expected[index])
            problems += std::string(named[index].name) + " bounds\n";

        if (! parent.contains(named[index].bounds))
            problems += std::string(named[index].name) + " outside\n";

        if (! named[index].textOk)
            problems += std::string(named[index].name) + " text\n";

        if (named[index].bounds.getWidth() < 28 || named[index].bounds.getHeight() < 28)
            problems += std::string(named[index].name) + " small\n";

        for (size_t other = index + 1; other < std::size(named); ++other)
            if (named[other].active && named[index].bounds.intersects(named[other].bounds))
                problems += std::string(named[index].name) + " overlaps " + named[other].name + "\n";
    }

    problems += scene.layoutProblems();
    return problems;
}

void SqueezeEditor::serviceTimerForTest()
{
    timerCallback();
}

void SqueezeEditor::nudgeScene(bool locked, float delta)
{
    scene.nudge(locked, delta);
}

void SqueezeEditor::nudgeOutput(float delta)
{
    output.nudge(delta);
}

void SqueezeEditor::selectAllRatio()
{
    scene.selectAllRatio();
}

void SqueezeEditor::setDisclosure(bool learnOpen, bool meterOpen)
{
    auto state = processor.getUiState();
    state.learnOpen = learnOpen;
    state.meterOpen = meterOpen;
    processor.setUiState(std::move(state));
    refreshHeader();
    resized();
}

bool SqueezeEditor::learnShown() const
{
    return learnColumn.isVisible();
}

bool SqueezeEditor::meterShown() const
{
    return meterDrawer.isVisible();
}

float SqueezeEditor::displayedReductionDb() const
{
    return std::abs(processor.lastVisualFrame().grDb);
}

void SqueezeEditor::parameterChanged(const juce::String&, float)
{
    triggerAsyncUpdate();
}

void SqueezeEditor::handleAsyncUpdate()
{
    refreshHeader();
    requestPaint();
}

void SqueezeEditor::timerCallback()
{
    const auto state = processor.getUiState();

    if (state.learnOpen != appliedLearn || state.meterOpen != appliedMeter)
        resized();

    VisualFrame frame;
    bool got = false;

    while (processor.popVisualFrame(frame))
    {
        trace.push(frame);
        got = true;
    }

    if (! got)
    {
        if (fastTimer)
        {
            if (++quietTicks > 30)
            {
                fastTimer = false;
                quietTicks = 0;
                startTimerHz(ui::kIdleTimerHz);
            }
            else
            {
                scene.advance();
                requestPaint();
            }
        }

        return;
    }

    if (! fastTimer)
    {
        fastTimer = true;
        startTimerHz(ui::kActiveTimerHz);
    }

    quietTicks = 0;
    scene.advance();
    requestPaint();
}

void SqueezeEditor::requestPaint()
{
    ++paintRequests;
    repaint();
}

void SqueezeEditor::listen(bool add)
{
    for (auto* parameter : processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);

        if (ranged == nullptr)
            continue;

        if (add)
            processor.getApvts().addParameterListener(ranged->getParameterID(), this);
        else
            processor.getApvts().removeParameterListener(ranged->getParameterID(), this);
    }
}

void SqueezeEditor::refreshHeader()
{
    const int mode = juce::jlimit(0, 3, juce::roundToInt(processor.getApvts().getRawParameterValue(params::mode)->load()));
    const auto state = processor.getUiState();

    for (int index = 0; index < 4; ++index)
    {
        modeTabs[static_cast<size_t>(index)].setAccent(ui::kModeColour[index]);
        modeTabs[static_cast<size_t>(index)].setSelected(index == mode);
    }

    const auto accent = ui::kModeColour[mode];
    slotA.setAccent(accent);
    slotB.setAccent(accent);
    learn.setAccent(accent);
    meter.setAccent(accent);
    meterHandle.setAccent(accent);
    slotA.setSelected(state.abSlot == 0);
    slotB.setSelected(state.abSlot == 1);
    learn.setSelected(state.learnOpen);
    meter.setSelected(state.meterOpen);
    meterHandle.setSelected(state.meterOpen);
}

void SqueezeEditor::toggleLearn()
{
    auto state = processor.getUiState();
    state.learnOpen = ! state.learnOpen;
    processor.setUiState(std::move(state));
    refreshHeader();
    resized();
}

void SqueezeEditor::toggleMeter()
{
    auto state = processor.getUiState();
    state.meterOpen = ! state.meterOpen;
    processor.setUiState(std::move(state));
    refreshHeader();
    resized();
}

void SqueezeEditor::selectSlot(int slot)
{
    processor.selectAbSlot(slot);
    refreshHeader();
}

void SqueezeEditor::selectMode(int mode)
{
    if (auto* parameter = processor.getApvts().getParameter(params::mode))
    {
        processor.beginGestureUndo();
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(mode)));
        parameter->endChangeGesture();
        processor.commitGesture();
    }
}

void SqueezeEditor::setChoice(const char* id, int index)
{
    if (auto* parameter = processor.getApvts().getParameter(id))
    {
        processor.beginGestureUndo();
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(index)));
        parameter->endChangeGesture();
        processor.commitGesture();
    }
}

void SqueezeEditor::setBool(const char* id, bool enabled)
{
    if (auto* parameter = processor.getApvts().getParameter(id))
    {
        processor.beginGestureUndo();
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(enabled ? 1.0f : 0.0f);
        parameter->endChangeGesture();
        processor.commitGesture();
    }
}

void SqueezeEditor::resetCurrentMode()
{
    const int mode = juce::jlimit(0, 3, juce::roundToInt(processor.getApvts().getRawParameterValue(params::mode)->load()));
    const char* ids[6] = {};
    int count = 0;

    if (mode == 0)
    {
        ids[0] = params::optoPeakRed;
        ids[1] = params::optoGain;
        ids[2] = params::optoLimit;
        count = 3;
    }
    else if (mode == 1)
    {
        ids[0] = params::vcaThreshold;
        ids[1] = params::vcaRatio;
        ids[2] = params::vcaOutput;
        count = 3;
    }
    else if (mode == 2)
    {
        ids[0] = params::fetInput;
        ids[1] = params::fetOutput;
        ids[2] = params::fetAttack;
        ids[3] = params::fetRelease;
        ids[4] = params::fetRatio;
        count = 5;
    }
    else
    {
        ids[0] = params::vmuInput;
        ids[1] = params::vmuThreshold;
        ids[2] = params::vmuTime;
        ids[3] = params::vmuOutput;
        count = 4;
    }

    processor.beginGestureUndo();

    for (int index = 0; index < count; ++index)
        if (auto* parameter = processor.getApvts().getParameter(ids[index]))
            parameter->setValueNotifyingHost(parameter->getDefaultValue());

    processor.commitGesture();
}

void SqueezeEditor::showMenu()
{
    const int oversampling = juce::roundToInt(processor.getApvts().getRawParameterValue(params::osFactor)->load());
    const bool levelMatch = processor.getApvts().getRawParameterValue(params::abLevelMatch)->load() >= 0.5f;
    const bool allowAll = processor.getApvts().getRawParameterValue(params::fetAllEnabled)->load() >= 0.5f;
    const auto captions = processor.getUiState().captions;

    juce::PopupMenu popup;
    popup.addItem(1, "Oversampling 1x", true, oversampling == 0);
    popup.addItem(2, "Oversampling 2x", true, oversampling == 1);
    popup.addItem(3, "Oversampling 4x", true, oversampling == 2);
    popup.addSeparator();
    popup.addItem(10, "Level match", true, levelMatch);
    popup.addItem(11, "Allow ALL", true, allowAll);
    popup.addSeparator();
    popup.addItem(20, "Captions always", true, captions == Captions::always);
    popup.addItem(21, "Captions with LEARN", true, captions == Captions::lessonOnly);
    popup.addItem(22, "Captions off", true, captions == Captions::off);
    popup.addSeparator();
    popup.addItem(30, "Reset this mode");

    popup.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&menu), [this](int result) {
        if (result >= 1 && result <= 3)
            setChoice(params::osFactor, result - 1);
        else if (result == 10)
            setBool(params::abLevelMatch, processor.getApvts().getRawParameterValue(params::abLevelMatch)->load() < 0.5f);
        else if (result == 11)
            setBool(params::fetAllEnabled, processor.getApvts().getRawParameterValue(params::fetAllEnabled)->load() < 0.5f);
        else if (result >= 20 && result <= 22)
        {
            auto state = processor.getUiState();
            state.captions = result == 20 ? Captions::always : result == 22 ? Captions::off : Captions::lessonOnly;
            processor.setUiState(std::move(state));
        }
        else if (result == 30)
            resetCurrentMode();
    });
}

bool SqueezeEditor::toastVisible() const
{
    if (processor.toastText().isEmpty())
        return false;

    const auto captions = processor.getUiState().captions;

    if (captions == Captions::off)
        return false;

    if (captions == Captions::lessonOnly && ! processor.getUiState().learnOpen)
        return false;

    const auto age = juce::Time::currentTimeMillis() - processor.toastShownAtMs();
    return age >= 300 && age <= 2800;
}

} // namespace squeeze
