#include "Ui/Panels.h"

#include <cmath>

#include "Parameters.h"
#include "StaticCurve.h"
#include "Tunables.h"
#include "Ui/Scenes.h"
#include "Ui/Theme.h"

namespace squeeze::ui
{
namespace
{

float rawOf(const SqueezeProcessor& processor, const char* id)
{
    if (auto* raw = processor.getApvts().getRawParameterValue(id))
        return raw->load();

    return 0.0f;
}

juce::String valueText(const SqueezeProcessor& processor, const char* id)
{
    if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(processor.getApvts().getParameter(id)))
        return parameter->getText(parameter->getValue(), 16);

    return {};
}

void drawCard(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(juce::Colour(0xff1c1f26));
    g.fillRoundedRectangle(bounds.toFloat(), 10.0f);
    g.setColour(kLine);
    g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 10.0f, 1.0f);
}

float curveGain(const SqueezeProcessor& processor, int mode, float detectorDb)
{
    if (mode == 0)
    {
        const float sidechain = tunables::optoSidechainDb(rawOf(processor, params::optoPeakRed));
        return StaticCurve::optoGainDb(detectorDb + sidechain, rawOf(processor, params::optoLimit));
    }

    if (mode == 1)
        return StaticCurve::vcaGainDb(detectorDb, rawOf(processor, params::vcaThreshold), rawOf(processor, params::vcaRatio));

    if (mode == 2)
    {
        const int index = juce::jlimit(0, 4, juce::roundToInt(rawOf(processor, params::fetRatio)));
        const bool all = index == 4;
        const float ratio = all ? tunables::fetAllRatio : tunables::fetRatios[index];
        const float knee = all ? tunables::fetAllKneeDb : tunables::fetKneeDb[index];
        return StaticCurve::fetGainDb(detectorDb, ratio, knee);
    }

    return StaticCurve::variMuGainDb(detectorDb, rawOf(processor, params::vmuThreshold));
}

} // namespace

LearnColumn::LearnColumn(SqueezeProcessor& processorIn)
    : processor(processorIn)
{
}

void LearnColumn::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().reduced(4);
    const int mode = juce::jlimit(0, 3, juce::roundToInt(rawOf(processor, params::mode)));
    const auto accent = kModeColour[mode];
    const float gr = std::abs(processor.lastVisualFrame().grDb);

    g.setColour(kMuted);
    g.setFont(interfaceFont(11.0f));
    auto title = bounds.removeFromTop(18);
    g.drawText(juce::String(-gr, 1) + " dB now", title, juce::Justification::centredLeft, false);

    const char* steps[][4] = {
        { "Sound reaches the lamp", "The lamp brightens with level", "The cell holds gain back", "Release is slow and has a memory" },
        { "Level arrives as water", "Water that crosses the line is measured", "The valve keeps 1/ratio of the excess", "The close speed follows how far it crossed" },
        { "Input pushes charge up the coil", "The metal ring is a fixed line", "A bolt grabs the peak immediately", "Higher ratios thicken the bolt" },
        { "Input heats the tube", "Gain eases off above the line", "The ratio climbs by itself", "The time constant sets the speed" },
    };
    const char* stories[] = {
        "Peak Reduction feeds the lamp. Attack and release belong to the cell, so they are not knobs.",
        "The line is the threshold. A 4:1 valve keeps one decibel for every four that cross it.",
        "The line does not move. Raise INPUT until the charge crosses it and the bolt fires.",
        "There is no ratio knob. The tube conducts less as it gets hotter, so the ratio rises.",
    };

    g.setColour(accent);
    g.setFont(interfaceFont(11.0f));
    g.drawText("HOW IT SQUEEZES", bounds.removeFromTop(18), juce::Justification::centredLeft, false);

    g.setColour(kText);
    g.setFont(interfaceFont(13.0f));

    for (int step = 0; step < 4; ++step)
    {
        auto row = bounds.removeFromTop(36);
        g.setColour(accent);
        g.drawText(juce::String(step + 1), row.removeFromLeft(18), juce::Justification::centredLeft, false);
        g.setColour(kText);
        g.drawFittedText(steps[mode][step], row, juce::Justification::centredLeft, 2);
    }

    auto card = bounds.removeFromTop(juce::jmin(110, bounds.getHeight() - 40));
    drawCard(g, card);
    g.setColour(kText);
    g.setFont(interfaceFont(13.0f));
    g.drawFittedText(stories[mode], card.reduced(10), juce::Justification::topLeft, 5);

    auto buttons = bounds.removeFromBottom(32);
    const int buttonW = juce::jmax(28, (buttons.getWidth() - 12) / 3);
    hearBounds = buttons.removeFromLeft(buttonW);
    buttons.removeFromLeft(6);
    undoBounds = buttons.removeFromLeft(buttonW);
    buttons.removeFromLeft(6);
    whyBounds = buttons.removeFromLeft(buttonW);

    const juce::Rectangle<int> buttonBoxes[] = { hearBounds, undoBounds, whyBounds };
    const char* labels[] = { "Hear A/B", "Undo", "Why?" };

    for (int index = 0; index < 3; ++index)
    {
        g.setColour(juce::Colour(0xff1c1f26));
        g.fillRoundedRectangle(buttonBoxes[index].toFloat(), 8.0f);
        g.setColour(accent);
        g.drawRoundedRectangle(buttonBoxes[index].toFloat(), 8.0f, 1.0f);
        g.setColour(kText);
        g.setFont(interfaceFont(12.0f));
        g.drawText(labels[index], buttonBoxes[index], juce::Justification::centred, false);
    }
}

void LearnColumn::mouseUp(const juce::MouseEvent& event)
{
    if (! event.mouseWasClicked())
        return;

    if (hearBounds.contains(event.getPosition()) && onHear != nullptr)
        onHear();
    else if (undoBounds.contains(event.getPosition()) && onUndo != nullptr)
        onUndo();
    else if (whyBounds.contains(event.getPosition()) && onWhy != nullptr)
        onWhy();
}

LearnDock::LearnDock(SqueezeProcessor& processorIn, const Trace& traceIn)
    : processor(processorIn),
      trace(traceIn)
{
}

void LearnDock::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().reduced(4);
    const int mode = juce::jlimit(0, 3, juce::roundToInt(rawOf(processor, params::mode)));
    const auto accent = kModeColour[mode];
    const int behaviorH = juce::jmax(90, bounds.getHeight() / 3);
    auto behavior = bounds.removeFromTop(behaviorH);
    bounds.removeFromTop(8);
    auto overTime = bounds.removeFromTop(behaviorH);
    bounds.removeFromTop(8);
    auto controls = bounds;

    drawCard(g, behavior);
    drawCard(g, overTime);
    drawCard(g, controls);

    g.setColour(kMuted);
    g.setFont(interfaceFont(11.0f));
    g.drawText("BEHAVIOR", behavior.removeFromTop(18).reduced(8, 0), juce::Justification::centredLeft, false);
    auto plot = behavior.reduced(8);
    const float detector = processor.lastVisualFrame().detDb;
    juce::Path curve;
    juce::Point<float> dot;

    for (int step = 0; step <= 48; ++step)
    {
        const float xDb = juce::jmap(static_cast<float>(step), 0.0f, 48.0f, -60.0f, 0.0f);
        const float yDb = xDb + curveGain(processor, mode, xDb);
        const float x = plot.getX() + plot.getWidth() * static_cast<float>(step) / 48.0f;
        const float y = juce::jmap(yDb, -60.0f, 0.0f, static_cast<float>(plot.getBottom()), static_cast<float>(plot.getY()));
        if (step == 0)
            curve.startNewSubPath(x, y);
        else
            curve.lineTo(x, y);

        if (std::abs(xDb - detector) < 1.3f)
            dot = { x, y };
    }

    g.setColour(kLine);
    g.drawRect(plot.toFloat(), 1.0f);
    g.setColour(accent);
    g.strokePath(curve, juce::PathStrokeType(1.8f));

    if (detector > -80.0f)
    {
        g.setColour(juce::Colours::white);
        g.fillEllipse(dot.x - 3.0f, dot.y - 3.0f, 6.0f, 6.0f);
    }

    g.setColour(kMuted);
    g.drawText("SQUEEZE OVER TIME", overTime.removeFromTop(18).reduced(8, 0), juce::Justification::centredLeft, false);
    auto historyPlot = overTime.reduced(8);
    g.setColour(kLine);
    g.drawRect(historyPlot.toFloat(), 1.0f);

    if (trace.gr.size() > 1)
    {
        juce::Path path;
        const float maxGr = 24.0f;

        for (size_t index = 0; index < trace.gr.size(); ++index)
        {
            const float x = historyPlot.getX() + historyPlot.getWidth() * static_cast<float>(index) / static_cast<float>(trace.gr.size() - 1);
            const float y = historyPlot.getBottom() - historyPlot.getHeight() * juce::jlimit(0.0f, 1.0f, trace.gr[index] / maxGr);
            if (index == 0)
                path.startNewSubPath(x, y);
            else
                path.lineTo(x, y);
        }

        g.setColour(accent);
        g.strokePath(path, juce::PathStrokeType(1.6f));
    }

    g.setColour(kMuted);
    g.drawText("CONTROLS", controls.removeFromTop(18).reduced(8, 0), juce::Justification::centredLeft, false);
    g.setColour(kText);
    g.setFont(interfaceFont(12.0f));

    struct Row
    {
        const char* name;
        juce::String value;
        bool locked;
    };

    std::vector<Row> rows;

    if (mode == 0)
    {
        rows.push_back({ "Peak Reduction", valueText(processor, params::optoPeakRed), false });
        rows.push_back({ "Curve", valueText(processor, params::optoLimit), false });
        rows.push_back({ "Attack", "cell, about 10 ms", true });
        rows.push_back({ "Release", "two-stage, automatic", true });
    }
    else if (mode == 1)
    {
        rows.push_back({ "Threshold", valueText(processor, params::vcaThreshold), false });
        rows.push_back({ "Ratio", valueText(processor, params::vcaRatio), false });
        rows.push_back({ "Attack", "automatic", true });
        rows.push_back({ "Release", "about 120 dB/s", true });
        rows.push_back({ "Detector", "RMS", true });
    }
    else if (mode == 2)
    {
        rows.push_back({ "Input", valueText(processor, params::fetInput), false });
        rows.push_back({ "Ratio", valueText(processor, params::fetRatio), false });
        rows.push_back({ "Attack", valueText(processor, params::fetAttack), false });
        rows.push_back({ "Release", valueText(processor, params::fetRelease), false });
        rows.push_back({ "Threshold", "fixed", true });
    }
    else
    {
        rows.push_back({ "Input", valueText(processor, params::vmuInput), false });
        rows.push_back({ "Threshold", valueText(processor, params::vmuThreshold), false });
        rows.push_back({ "Time", valueText(processor, params::vmuTime), false });
        rows.push_back({ "Ratio", juce::String(processor.lastVisualFrame().vmuRatioNow, 1) + " : 1", true });
    }

    for (const auto& row : rows)
    {
        auto line = controls.removeFromTop(18).reduced(8, 0);
        g.setColour(row.locked ? kMuted : kText);
        g.drawText(row.name, line.removeFromLeft(line.getWidth() / 2), juce::Justification::centredLeft, false);
        g.drawText(row.locked ? row.value + "  lock" : row.value, line, juce::Justification::centredRight, false);
    }
}

MeterDrawer::MeterDrawer(SqueezeProcessor& processorIn, const Trace& traceIn)
    : processor(processorIn),
      trace(traceIn)
{
}

void MeterDrawer::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff14171c));
    g.fillRect(getLocalBounds());
    g.setColour(kLine);
    g.drawLine(0.0f, 0.0f, static_cast<float>(getWidth()), 0.0f, 1.0f);

    auto bounds = getLocalBounds().reduced(16, 8);
    const int mode = juce::jlimit(0, 3, juce::roundToInt(rawOf(processor, params::mode)));
    const auto accent = kModeColour[mode];
    auto chips = bounds.removeFromTop(28);
    g.setColour(kMuted);
    g.setFont(interfaceFont(12.0f));

    const char* fixed = mode == 0 ? "ATTACK / RELEASE  FIXED" : mode == 1 ? "DETECTOR RMS   KNEE HARD   FIXED"
                                                         : mode == 2 ? "THRESHOLD  FIXED"
                                                                     : "RATIO  FOLLOWS LEVEL";
    g.drawText(fixed, chips.removeFromLeft(chips.getWidth() / 2), juce::Justification::centredLeft, false);

    int count = 0;
    const auto* scenes = scenesForMode(mode, count);
    auto sceneRow = bounds.removeFromTop(32);
    const int sceneW = juce::jmax(28, (sceneRow.getWidth() - 80) / juce::jmax(1, count));

    for (int index = 0; index < count; ++index)
    {
        sceneBounds[static_cast<size_t>(index)] = sceneRow.removeFromLeft(sceneW).reduced(3, 2);
        const bool selected = processor.getUiState().selectedScene == scenes[index].name;
        g.setColour(selected ? accent.withAlpha(0.25f) : juce::Colour(0xff1c1f26));
        g.fillRoundedRectangle(sceneBounds[static_cast<size_t>(index)].toFloat(), 8.0f);
        g.setColour(selected ? accent : kLine);
        g.drawRoundedRectangle(sceneBounds[static_cast<size_t>(index)].toFloat(), 8.0f, 1.0f);
        g.setColour(kText);
        g.setFont(interfaceFont(11.0f));
        g.drawText(scenes[index].name, sceneBounds[static_cast<size_t>(index)], juce::Justification::centred, false);
    }

    undoBounds = sceneRow.removeFromRight(72).reduced(3, 2);
    g.setColour(juce::Colour(0xff1c1f26));
    g.fillRoundedRectangle(undoBounds.toFloat(), 8.0f);
    g.setColour(accent);
    g.drawRoundedRectangle(undoBounds.toFloat(), 8.0f, 1.0f);
    g.setColour(kText);
    g.drawText("Undo", undoBounds, juce::Justification::centred, false);

    auto now = bounds.removeFromRight(160);
    auto wave = bounds.reduced(0, 4);
    g.setColour(kLine);
    g.drawRoundedRectangle(wave.toFloat(), 8.0f, 1.0f);

    if (trace.inDb.size() > 1)
    {
        const auto draw = [&](const std::vector<float>& values, juce::Colour colour) {
            juce::Path path;
            const float top = static_cast<float>(wave.getY() + 6);
            const float bottom = static_cast<float>(wave.getBottom() - 6);

            for (size_t index = 0; index < values.size(); ++index)
            {
                const float x = wave.getX() + wave.getWidth() * static_cast<float>(index) / static_cast<float>(values.size() - 1);
                const float y = juce::jmap(juce::jlimit(-48.0f, 0.0f, values[index]), -48.0f, 0.0f, bottom, top);
                if (index == 0)
                    path.startNewSubPath(x, y);
                else
                    path.lineTo(x, y);
            }

            g.setColour(colour);
            g.strokePath(path, juce::PathStrokeType(1.4f));
        };

        draw(trace.inDb, kMuted);
        draw(trace.outDb, accent);
    }

    g.setColour(kMuted);
    g.setFont(interfaceFont(11.0f));
    g.drawText("NOW", now.removeFromTop(16), juce::Justification::centredLeft, false);
    g.setColour(kText);
    g.setFont(interfaceFont(22.0f));
    const float gr = std::abs(processor.lastVisualFrame().grDb);
    g.drawText(juce::String(-gr, 1) + " dB", now.removeFromTop(28), juce::Justification::centredLeft, false);
    g.setColour(kMuted);
    g.setFont(interfaceFont(12.0f));
    const auto& frame = processor.lastVisualFrame();
    g.drawText("IN " + juce::String(frame.inDb, 1), now.removeFromTop(16), juce::Justification::centredLeft, false);
    g.drawText("OUT " + juce::String(frame.outDb, 1), now.removeFromTop(16), juce::Justification::centredLeft, false);

    const auto history = processor.getUiState().history;
    g.setFont(interfaceFont(11.0f));
    const int start = juce::jmax(0, static_cast<int>(history.size()) - 2);

    for (int index = start; index < static_cast<int>(history.size()); ++index)
        g.drawText(history[static_cast<size_t>(index)].change, now.removeFromTop(14), juce::Justification::centredLeft, true);
}

void MeterDrawer::mouseUp(const juce::MouseEvent& event)
{
    if (! event.mouseWasClicked())
        return;

    if (undoBounds.contains(event.getPosition()))
    {
        processor.undoGesture();
        return;
    }

    const int mode = juce::jlimit(0, 3, juce::roundToInt(rawOf(processor, params::mode)));

    for (int index = 0; index < 4; ++index)
        if (sceneBounds[static_cast<size_t>(index)].contains(event.getPosition()))
            processor.applyScene(mode, index);
}

} // namespace squeeze::ui
