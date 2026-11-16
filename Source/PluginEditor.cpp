#include "PluginEditor.h"

#include "EditorLayout.h"
#include "Parameters.h"
#include "Ui/SqueezeBoxes.h"
#include "Ui/Strings.h"
#include "Ui/Theme.h"

#include <algorithm>
#include <cstdio>

namespace squeeze
{

SqueezeEditor::SqueezeEditor(SqueezeProcessor& processorIn)
    : juce::AudioProcessorEditor(processorIn),
      processor(processorIn),
      gain(processorIn),
      scene(processorIn),
      output(processorIn)
{
    const auto width = std::clamp(processor.getUiState().windowWidth, ui::kMinWidth, ui::kMaxWidth);

    setOpaque(true);
    setName("SQUEEZE");

    addAndMakeVisible(logo);
    addAndMakeVisible(gain);
    addAndMakeVisible(scene);
    addAndMakeVisible(output);
    addAndMakeVisible(meterHandle);

    for (int index = 0; index < 4; ++index)
    {
        modeTabs[static_cast<size_t>(index)].setTexts(ui::strings::modePrimary(index), ui::strings::modeEnglish(index));
        addAndMakeVisible(modeTabs[static_cast<size_t>(index)]);
        modeTabs[static_cast<size_t>(index)].onClick = [this, index] { selectMode(index); };
    }

    slotA.setTexts("A", {});
    slotB.setTexts("B", {});
    learn.setTexts(ui::strings::learnPrimary(), ui::strings::learnEn());
    meter.setTexts(ui::strings::meterPrimary(), ui::strings::meterEn());
    meterHandle.setTexts(ui::strings::meterPrimary(), ui::strings::meterEn());
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

    // setResizeLimits applies the constrainer to the current (empty) bounds and
    // lands on the minimum size. Ignore that callback so it does not replace the saved width.
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
    const auto boxes = ui::squeezeBoxes(getWidth(), getHeight());
    g.fillAll(ui::kBackground);
    g.setColour(ui::kHeader);
    g.fillRect(boxes.header);
    g.setColour(ui::kLine);
    g.drawLine(0.0f, static_cast<float>(boxes.header.getBottom()), static_cast<float>(getWidth()), static_cast<float>(boxes.header.getBottom()), 1.0f);
    std::fprintf(stderr, "editor %d x %d scene %d %d %d %d\n", getWidth(), getHeight(), scene.getX(), scene.getY(), scene.getWidth(), scene.getHeight());
}

void SqueezeEditor::resized()
{
    if (getWidth() < ui::kMinWidth)
        return;

    const auto boxes = ui::squeezeBoxes(getWidth(), getHeight());
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

    if (suppressStateWrite)
        return;

    const int width = getWidth();

    if (width < ui::kMinWidth || width > ui::kMaxWidth)
        return;

    auto state = processor.getUiState();

    if (state.windowWidth == width)
        return;

    state.windowWidth = width;
    processor.setUiState(std::move(state));
}

bool SqueezeEditor::layoutIsValid() const
{
    return layoutReport().empty();
}

std::string SqueezeEditor::layoutReport() const
{
    std::string problems;
    const auto boxes = ui::squeezeBoxes(getWidth(), getHeight());
    const juce::Rectangle<int> parent = getLocalBounds();

    struct Named
    {
        const char* name;
        juce::Rectangle<int> bounds;
        bool textOk;
    };

    const Named named[] = {
        { "logo", logo.getBounds(), logo.contentFits() },
        { "tab0", modeTabs[0].getBounds(), modeTabs[0].contentFits() },
        { "tab1", modeTabs[1].getBounds(), modeTabs[1].contentFits() },
        { "tab2", modeTabs[2].getBounds(), modeTabs[2].contentFits() },
        { "tab3", modeTabs[3].getBounds(), modeTabs[3].contentFits() },
        { "slotA", slotA.getBounds(), slotA.contentFits() },
        { "slotB", slotB.getBounds(), slotB.contentFits() },
        { "learn", learn.getBounds(), learn.contentFits() },
        { "meter", meter.getBounds(), meter.contentFits() },
        { "menu", menu.getBounds(), menu.contentFits() },
        { "gain", gain.getBounds(), gain.contentFits() },
        { "scene", scene.getBounds(), true },
        { "output", output.getBounds(), output.contentFits() },
        { "meterHandle", meterHandle.getBounds(), meterHandle.contentFits() },
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
    };

    for (size_t index = 0; index < std::size(named); ++index)
    {
        if (named[index].bounds != expected[index])
            problems += std::string(named[index].name) + " bounds\n";

        if (! parent.contains(named[index].bounds))
            problems += std::string(named[index].name) + " outside\n";

        if (! named[index].textOk)
            problems += std::string(named[index].name) + " text\n";

        if (named[index].bounds.getWidth() < 28 || named[index].bounds.getHeight() < 28)
            problems += std::string(named[index].name) + " small\n";

        for (size_t other = index + 1; other < std::size(named); ++other)
            if (named[index].bounds.intersects(named[other].bounds))
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
    VisualFrame frame;
    bool got = false;

    while (processor.popVisualFrame(frame))
        got = true;

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
}

void SqueezeEditor::toggleMeter()
{
    auto state = processor.getUiState();
    state.meterOpen = ! state.meterOpen;
    processor.setUiState(std::move(state));
    refreshHeader();
}

void SqueezeEditor::selectSlot(int slot)
{
    auto state = processor.getUiState();
    state.abSlot = slot == 0 ? 0 : 1;
    processor.setUiState(std::move(state));
    refreshHeader();
}

void SqueezeEditor::selectMode(int mode)
{
    if (auto* parameter = processor.getApvts().getParameter(params::mode))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(mode)));
        parameter->endChangeGesture();
    }
}

} // namespace squeeze
