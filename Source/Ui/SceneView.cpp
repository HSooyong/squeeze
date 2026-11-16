#include "Ui/SceneView.h"

#include "Parameters.h"
#include "Ui/Strings.h"
#include "Ui/Theme.h"

#include <cmath>
#include <vector>

namespace squeeze::ui
{
namespace
{

struct Readings
{
    int mode = 0;
    float grDb = 0.0f;
    float peak = 40.0f;
    bool limit = false;
    float threshold = -18.0f;
    float ratio = 4.0f;
    float valveOpen = 1.0f;
    float rmsDb = -160.0f;
    float fetInput = 24.0f;
    float attack = 300.0f;
    float release = 250.0f;
    int ratioIndex = 0;
    bool allEnabled = false;
    float arc = 0.0f;
    float heat = -160.0f;
    float ratioNow = 1.0f;
    float glow = 0.0f;
    int timeIndex = 2;
    float cell = 0.0f;
    float tubeInput = 0.0f;
    float tubeThreshold = -22.0f;
    float phase = 0.0f;
};

struct Item
{
    juce::Rectangle<int> bounds;
    juce::String primary;
    juce::String secondary;
    float preferred = 13.0f;
    float minimum = 10.0f;
    float preferredSecondary = 9.0f;
    float minimumSecondary = 8.0f;
    int role = 0;
    bool locked = false;

    bool fits() const
    {
        const auto first = fitLine(primary, preferred, minimum, bounds.getWidth(), 8);
        const auto second = fitLine(secondary, preferredSecondary, minimumSecondary, bounds.getWidth(), 8);
        const float used = secondary.isEmpty() ? first.size + 4.0f : first.size + second.size + 6.0f;
        return first.ok && second.ok && used <= static_cast<float>(bounds.getHeight());
    }
};

struct SceneMap
{
    std::vector<Item> items;
    juce::Rectangle<float> dome;
    juce::Rectangle<float> cup;
    juce::Point<float> lampBase;
    juce::Point<float> lampElbow;
    juce::Point<float> lampHead;
    float lampAngle = 0.0f;
};

float rawOf(SqueezeProcessor& processor, const char* id)
{
    if (auto* raw = processor.getApvts().getRawParameterValue(id))
        return raw->load();

    return 0.0f;
}

juce::RangedAudioParameter* parameter(SqueezeProcessor& processor, const char* id)
{
    return dynamic_cast<juce::RangedAudioParameter*>(processor.getApvts().getParameter(id));
}

juce::String reductionText(float grDb)
{
    if (! std::isfinite(grDb) || std::abs(grDb) < 0.05f)
        return "0.0 dB";

    return "-" + juce::String(std::abs(grDb), 1) + " dB";
}

juce::String dbOne(float value)
{
    if (! std::isfinite(value) || value < -80.0f)
        return juce::String::fromUTF8(u8"—");

    return juce::String(value, 1);
}

Item makeItem(juce::Rectangle<int> bounds, juce::String primary, juce::String secondary, int role, bool locked)
{
    Item item;
    item.bounds = bounds;
    item.primary = std::move(primary);
    item.secondary = std::move(secondary);
    item.role = role;
    item.locked = locked;
    return item;
}

juce::String shownFrom(float db)
{
    return juce::String(db, 1) + " dB";
}

juce::String shownInput(float raw)
{
    const float display = raw - 24.0f;
    return (display > 0.0f ? "+" : "") + juce::String(display, 1);
}

juce::Rectangle<int> box(int x, int y, int w, int h)
{
    return { x, y, w, h };
}

SceneMap buildMap(juce::Rectangle<int> local, const Readings& readings)
{
    SceneMap map;
    const int width = local.getWidth();
    const int height = local.getHeight();
    const auto gr = box(width - 166, 12, 150, 44);
    map.items.push_back(makeItem(gr, reductionText(readings.grDb), {}, 0, false));
    map.items.back().preferred = 18.0f;
    map.items.back().minimum = 12.0f;

    if (readings.mode == 0)
    {
        const float peak01 = juce::jlimit(0.0f, 1.0f, readings.peak / 100.0f);
        map.lampAngle = juce::degreesToRadians(-62.0f + peak01 * 46.0f);
        map.lampBase = { width * 0.22f, height * 0.76f };
        map.lampElbow = { width * 0.36f, height * 0.50f };
        const float length = width * 0.16f;
        map.lampHead = map.lampElbow + juce::Point<float>(std::cos(map.lampAngle), std::sin(map.lampAngle)) * length;
        auto head = box(juce::roundToInt(map.lampHead.x) - 36, juce::roundToInt(map.lampHead.y) - 30, 72, 60);
        map.items.push_back(makeItem(head, {}, {}, 1, false));
        map.items.push_back(makeItem(box(20, height - 56, 104, 40),
                                     readings.limit ? strings::limit() : strings::compress(),
                                     readings.limit ? "LIMIT" : "COMPRESS",
                                     2,
                                     false));
        map.dome = { width * 0.60f, height * 0.56f, width * 0.28f, height * 0.30f };
    }
    else if (readings.mode == 1)
    {
        auto valve = box(juce::roundToInt(width * 0.24f), 28, 72, 72);
        auto opened = box(20, 32, 108, 40);
        auto locked = box(valve.getX(), valve.getBottom() + 12, 132, 40);
        auto threshold = box(juce::roundToInt(width * 0.40f), juce::roundToInt(height * 0.46f), 120, 48);
        auto inLevel = box(width - 166, 68, 150, 40);
        auto outLevel = box(width - 166, 114, 150, 40);
        const float outDb = readings.rmsDb - readings.grDb;
        map.items.push_back(makeItem(opened,
                                     strings::openLabel() + " " + juce::String(juce::roundToInt(readings.valveOpen * 100.0f)) + "%",
                                     strings::openLabelEn(),
                                     0,
                                     false));
        map.items.push_back(makeItem(valve, {}, {}, 3, false));
        map.items.push_back(makeItem(locked, strings::valveAuto(), strings::valveAutoEn(), 5, true));
        map.items.push_back(makeItem(threshold,
                                     strings::thresholdLabel() + " " + shownFrom(readings.threshold),
                                     strings::thresholdLabelEn(),
                                     4,
                                     false));
        map.items.push_back(makeItem(inLevel, dbOne(readings.rmsDb), "IN", 0, false));
        map.items.push_back(makeItem(outLevel, dbOne(outDb), "OUT", 0, false));
        map.cup = { static_cast<float>(threshold.getRight() + 18),
                    height * 0.30f,
                    juce::jmax(120.0f, width * 0.18f),
                    height * 0.56f };
    }
    else if (readings.mode == 2)
    {
        auto charge = box(juce::roundToInt(width * 0.24f) - 44, juce::roundToInt(height * 0.62f) - 70, 96, 132);
        auto fixed = box(charge.getRight() + 16, juce::roundToInt(height * 0.30f), 120, 40);
        auto release = box(width - 250, juce::roundToInt(height * 0.36f), 112, 48);
        auto attack = box(juce::roundToInt(width * 0.42f), juce::roundToInt(height * 0.72f), 168, 44);
        map.items.push_back(makeItem(charge,
                                     strings::inputLabel() + " " + shownInput(readings.fetInput),
                                     strings::inputLabelEn(),
                                     6,
                                     false));
        map.items.push_back(makeItem(fixed, strings::fixedLine(), strings::fixedLineEn(), 14, true));
        map.items.push_back(makeItem(release, juce::String(juce::roundToInt(readings.release)) + " ms", "RELEASE", 8, false));
        map.items.push_back(makeItem(attack, juce::String(juce::roundToInt(readings.attack)) + juce::String::fromUTF8(u8" µs"), "ATTACK", 7, false));

        const char* names[] = { "4", "8", "12", "20", "ALL" };
        const int dot = 32;
        const int gap = 8;
        const int rowWidth = 5 * dot + 4 * gap;
        int x = width - rowWidth - 28;
        const int y = height - dot - 18;

        for (int index = 0; index < 5; ++index)
        {
            const bool locked = index == 4 && ! readings.allEnabled;
            map.items.push_back(makeItem(box(x, y, dot, dot), names[index], {}, 9 + index, locked));
            map.items.back().preferred = 11.0f;
            map.items.back().minimum = 9.0f;
            x += dot + gap;
        }
    }
    else
    {
        auto heat = box(28, juce::roundToInt(height * 0.24f), 116, 168);
        auto threshold = box(heat.getRight() + 16, height - 72, 132, 48);
        auto dial = box(juce::roundToInt(width * 0.46f), height - 76, 120, 52);
        auto ratio = box(width - 188, juce::roundToInt(height * 0.28f), 120, 150);
        map.items.push_back(makeItem(heat,
                                     strings::heat() + " " + dbOne(readings.tubeInput),
                                     strings::heatEn(),
                                     15,
                                     false));
        map.items.push_back(makeItem(threshold,
                                     strings::thresholdLabel() + " " + shownFrom(readings.tubeThreshold),
                                     strings::thresholdLabelEn(),
                                     16,
                                     false));
        map.items.push_back(makeItem(dial,
                                     strings::timeLabel() + " T" + juce::String(readings.timeIndex + 1),
                                     strings::timeLabelEn(),
                                     17,
                                     false));
        map.items.push_back(makeItem(ratio,
                                     strings::ratioNow() + " " + juce::String(readings.ratioNow, 1) + " : 1",
                                     strings::ratioNowEn(),
                                     18,
                                     true));
    }

    return map;
}

juce::Rectangle<int> textBoundsFor(const Item& item)
{
    auto bounds = item.bounds;

    if (item.role == 6)
        return bounds.removeFromBottom(46);

    if (item.role == 15 || item.role == 18)
        return bounds.removeFromTop(46);

    return bounds;
}

void paintText(juce::Graphics& g, const Item& item, juce::Colour accent)
{
    if (item.primary.isEmpty() && item.secondary.isEmpty())
        return;

    const auto plate = textBoundsFor(item);
    const bool chip = item.role > 1 && item.role != 6 && item.role != 15 && item.role != 18;
    const bool dot = item.role >= 9 && item.role <= 13;

    if (dot)
    {
        auto circle = plate.toFloat().reduced(1.0f);
        g.setColour(item.locked ? kTrack : accent.withAlpha(0.16f));
        g.fillEllipse(circle);
        g.setColour(item.locked ? kLine : accent);
        g.drawEllipse(circle, 1.4f);
    }
    else if (chip)
    {
        auto rounded = plate.toFloat().reduced(0.5f);
        g.setColour(item.locked ? juce::Colours::black.withAlpha(0.25f) : accent.withAlpha(0.10f));
        g.fillRoundedRectangle(rounded, 8.0f);
        g.setColour(item.locked ? kLine : accent.withAlpha(0.9f));
        g.drawRoundedRectangle(rounded, 8.0f, 1.3f);
    }

    const auto first = fitLine(item.primary, item.preferred, item.minimum, plate.getWidth(), 8);
    const auto second = fitLine(item.secondary, item.preferredSecondary, item.minimumSecondary, plate.getWidth(), 8);

    if (item.secondary.isEmpty())
    {
        g.setColour(item.locked ? kMuted : kText);
        g.setFont(interfaceFont(first.size));
        g.drawText(item.primary, plate, juce::Justification::centred, false);
        return;
    }

    auto area = plate;
    auto top = area.removeFromTop(juce::roundToInt(first.size + 3.0f));
    g.setColour(item.locked ? kMuted : kText);
    g.setFont(interfaceFont(first.size));
    g.drawText(item.primary, top, juce::Justification::centred, false);
    g.setColour(item.locked ? kMuted : accent);
    g.setFont(interfaceFont(second.size));
    g.drawText(item.secondary, area, juce::Justification::centred, false);
}

void drawFloor(juce::Graphics& g, juce::Rectangle<int> local)
{
    juce::ColourGradient glow(juce::Colours::black.withAlpha(0.0f),
                              local.getCentreX(),
                              local.getCentreY(),
                              juce::Colours::black.withAlpha(0.45f),
                              static_cast<float>(local.getX()),
                              static_cast<float>(local.getBottom()),
                              true);
    g.setGradientFill(glow);
    g.fillRect(local);
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.fillEllipse(local.getWidth() * 0.18f, local.getHeight() * 0.78f, local.getWidth() * 0.64f, local.getHeight() * 0.16f);
}

void drawLamp(juce::Graphics& g, const SceneMap& map, const Readings& readings)
{
    const auto accent = kModeColour[0];
    const float brightness = juce::jlimit(0.2f, 1.0f, 0.28f + readings.cell * 0.55f + juce::jmin(0.45f, readings.grDb / 14.0f));
    const auto head = map.lampHead;
    const auto dome = map.dome;
    const auto aim = dome.getCentre() - head;
    const float towardDome = std::atan2(aim.y, aim.x);
    const float down = juce::MathConstants<float>::halfPi;
    const float facing = towardDome * 0.25f + down * 0.75f;
    const juce::Point<float> along(std::cos(facing), std::sin(facing));
    const juce::Point<float> side(-along.y, along.x);
    const auto neck = head - along * 8.0f;
    const auto mouth = head + along * 86.0f;

    juce::Path cone;
    cone.startNewSubPath(mouth + side * 36.0f);
    cone.lineTo(mouth - side * 36.0f);
    cone.lineTo(dome.getX() - 10.0f, dome.getBottom() - 8.0f);
    cone.lineTo(dome.getRight() + 16.0f, dome.getY() + dome.getHeight() * 0.35f);
    cone.closeSubPath();
    g.setColour(accent.withAlpha(0.16f + 0.20f * brightness));
    g.fillPath(cone);
    juce::ColourGradient beam(juce::Colour(0xfffff1c9).withAlpha(0.55f * brightness),
                              mouth.x,
                              mouth.y,
                              accent.withAlpha(0.0f),
                              dome.getCentreX(),
                              dome.getCentreY(),
                              true);
    g.setGradientFill(beam);
    g.fillPath(cone);

    juce::Path arm;
    arm.startNewSubPath(map.lampBase);
    arm.lineTo(map.lampElbow);
    arm.lineTo(neck);
    g.setColour(juce::Colour(0xfff4f1ea));
    g.strokePath(arm, juce::PathStrokeType(8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(juce::Colour(0xffc8c2b8));
    g.strokePath(arm, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour(juce::Colour(0xffb7b1a6));
    g.fillEllipse(map.lampBase.x - 38.0f, map.lampBase.y - 10.0f, 76.0f, 24.0f);
    g.setColour(juce::Colour(0xfff7f4ee));
    g.fillEllipse(map.lampBase.x - 32.0f, map.lampBase.y - 16.0f, 64.0f, 22.0f);
    g.setColour(juce::Colour(0xff2a241c));
    g.fillEllipse(map.lampBase.x + 10.0f, map.lampBase.y - 8.0f, 10.0f, 10.0f);
    g.setColour(juce::Colour(0xfff7f4ee));
    g.fillEllipse(map.lampElbow.x - 8.0f, map.lampElbow.y - 8.0f, 16.0f, 16.0f);
    g.setColour(juce::Colour(0xff8d877e));
    g.fillEllipse(map.lampElbow.x - 3.0f, map.lampElbow.y - 3.0f, 6.0f, 6.0f);

    juce::Path shade;
    shade.startNewSubPath(neck + side * 18.0f);
    shade.lineTo(neck - side * 18.0f);
    shade.lineTo(mouth - side * 48.0f);
    shade.lineTo(mouth + side * 48.0f);
    shade.closeSubPath();
    juce::ColourGradient metal(juce::Colour(0xffffffff), neck.x - side.x * 20.0f, neck.y - side.y * 20.0f, juce::Colour(0xffc9c3b8), mouth.x + side.x * 20.0f, mouth.y + side.y * 20.0f, false);
    g.setGradientFill(metal);
    g.fillPath(shade);
    g.setColour(juce::Colour(0xff8a847a));
    g.strokePath(shade, juce::PathStrokeType(1.4f));
    juce::Path cavity;
    const auto cavityStart = mouth - along * 22.0f;
    cavity.startNewSubPath(cavityStart + side * 10.0f);
    cavity.lineTo(cavityStart - side * 10.0f);
    cavity.lineTo(mouth - side * 28.0f);
    cavity.lineTo(mouth + side * 28.0f);
    cavity.closeSubPath();
    g.setColour(juce::Colour(0xff2c261f));
    g.fillPath(cavity);
    g.setColour(accent.withAlpha(0.95f));
    g.fillEllipse(mouth.x - 11.0f, mouth.y - 11.0f, 22.0f, 22.0f);
    g.setColour(juce::Colours::white.withAlpha(0.85f * brightness));
    g.fillEllipse(mouth.x - 5.0f, mouth.y - 5.0f, 10.0f, 10.0f);

    g.setColour(accent.withAlpha(0.85f));
    for (int ring = 1; ring <= 3; ++ring)
    {
        const float radius = 16.0f + static_cast<float>(ring) * 11.0f;
        juce::Path arc;
        arc.addCentredArc(neck.x, neck.y, radius, radius, 0.0f, facing - 2.4f, facing - 1.15f, true);
        const float dashes[] = { 3.0f, 4.0f };
        juce::Path dashed;
        juce::PathStrokeType(1.3f).createDashedStroke(dashed, arc, dashes, 2, {});
        g.strokePath(dashed, juce::PathStrokeType(1.3f));
    }

    g.setColour(juce::Colours::black.withAlpha(0.28f));
    g.fillEllipse(dome.translated(0.0f, 10.0f));
    juce::ColourGradient glass(juce::Colour(0xfffff6e4), dome.getCentreX(), dome.getY(), accent.darker(0.15f), dome.getCentreX(), dome.getBottom(), false);
    g.setGradientFill(glass);
    g.fillEllipse(dome);
    g.setColour(accent.darker(0.35f).withAlpha(0.9f));
    g.drawEllipse(dome, 1.6f);

    juce::Path waves;
    for (int line = 0; line < 4; ++line)
    {
        const float y = dome.getY() + dome.getHeight() * (0.32f + 0.12f * static_cast<float>(line));
        const float inset = 18.0f + static_cast<float>(line % 2) * 8.0f;
        waves.startNewSubPath(dome.getX() + inset, y);

        for (int step = 1; step <= 18; ++step)
        {
            const float t = static_cast<float>(step) / 18.0f;
            const float x = dome.getX() + inset + (dome.getWidth() - inset * 2.0f) * t;
            const float wobble = std::sin(t * 12.0f + readings.phase + static_cast<float>(line)) * (1.6f + brightness);
            waves.lineTo(x, y + wobble);
        }
    }

    g.setColour(juce::Colour(0xff7a4e12).withAlpha(0.55f));
    g.strokePath(waves, juce::PathStrokeType(1.7f));

    g.setColour(accent.darker(0.45f).withAlpha(0.7f));
    const float meshY = dome.getBottom() - 16.0f;
    for (int cell = 0; cell < 9; ++cell)
    {
        const float x = dome.getX() + 16.0f + static_cast<float>(cell) * (dome.getWidth() - 32.0f) / 8.0f;
        g.drawEllipse(x - 4.0f, meshY, 8.0f, 6.0f, 1.0f);
    }
}

void drawNeedle(juce::Graphics& g, const SceneMap& map, const Readings& readings)
{
    const auto accent = kModeColour[1];
    auto cup = map.cup;
    const float inY = juce::jmap(juce::jlimit(-40.0f, 0.0f, readings.rmsDb), 0.0f, -40.0f, cup.getY() + 18.0f, cup.getBottom() - 16.0f);
    const float outDb = readings.rmsDb - readings.grDb;
    const float outY = juce::jmap(juce::jlimit(-40.0f, 0.0f, outDb), 0.0f, -40.0f, cup.getY() + 18.0f, cup.getBottom() - 16.0f);
    const float markY = juce::jmap(juce::jlimit(-40.0f, 0.0f, readings.threshold), 0.0f, -40.0f, cup.getY() + 18.0f, cup.getBottom() - 16.0f);
    juce::Rectangle<float> valve;

    for (const auto& item : map.items)
        if (item.role == 3)
            valve = item.bounds.toFloat();

    const float pipeY = valve.getCentreY();
    const float spoutX = cup.getCentreX();
    juce::ColourGradient chrome(juce::Colour(0xfff7f9fb), pipeY, pipeY, juce::Colour(0xff8e98a3), spoutX, pipeY, false);
    g.setGradientFill(chrome);
    g.fillRoundedRectangle(valve.getX() - 78.0f, pipeY - 9.0f, spoutX - valve.getX() + 10.0f, 18.0f, 9.0f);
    g.fillRoundedRectangle(spoutX - 10.0f, pipeY - 4.0f, 20.0f, cup.getY() - pipeY + 16.0f, 9.0f);
    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.drawLine(valve.getX() - 60.0f, pipeY - 4.0f, spoutX - 16.0f, pipeY - 4.0f, 1.5f);
    g.fillEllipse(spoutX - 14.0f, pipeY - 14.0f, 28.0f, 28.0f);

    const float steps[] = { 1.0f, 1.5f, 2.0f, 4.0f, 8.0f, 1000.0f };
    float position = 5.0f;

    for (int index = 0; index < 5; ++index)
    {
        if (readings.ratio <= steps[index] + 0.01f)
        {
            position = static_cast<float>(index);
            break;
        }

        if (readings.ratio < steps[index + 1])
        {
            position = static_cast<float>(index) + (readings.ratio - steps[index]) / (steps[index + 1] - steps[index]);
            break;
        }
    }

    const float rotation = juce::degreesToRadians(-50.0f + position * 22.0f);
    const auto hub = valve.getCentre() + juce::Point<float>(0.0f, -16.0f);
    g.setColour(juce::Colour(0xffd5dbe2));
    g.fillEllipse(hub.x - 6.0f, valve.getCentreY() - 6.0f, 12.0f, 12.0f);

    for (int arm = 0; arm < 4; ++arm)
    {
        const float angle = rotation + juce::degreesToRadians(static_cast<float>(arm) * 90.0f);
        const auto end = hub + juce::Point<float>(std::cos(angle), std::sin(angle)) * 22.0f;
        g.setColour(juce::Colour(0xfff4f7fa));
        g.drawLine(hub.x, hub.y, end.x, end.y, 5.0f);
        g.fillEllipse(end.x - 5.0f, end.y - 5.0f, 10.0f, 10.0f);
    }

    g.setColour(juce::Colour(0xff9aa3ad));
    g.fillEllipse(hub.x - 4.0f, hub.y - 4.0f, 8.0f, 8.0f);

    g.setColour(juce::Colours::black.withAlpha(0.25f));
    g.fillEllipse(cup.getX() + 8.0f, cup.getBottom() - 8.0f, cup.getWidth() - 16.0f, 18.0f);
    juce::ColourGradient glass(juce::Colour(0xffe7eef5).withAlpha(0.20f), cup.getCentreX(), cup.getY(), juce::Colour(0xff9eacba).withAlpha(0.28f), cup.getCentreX(), cup.getBottom(), false);
    g.setGradientFill(glass);
    g.fillRoundedRectangle(cup, 16.0f);
    g.setColour(juce::Colours::white.withAlpha(0.75f));
    g.drawRoundedRectangle(cup, 16.0f, 2.0f);
    g.fillEllipse(cup.getX() + 8.0f, cup.getY() - 6.0f, cup.getWidth() - 16.0f, 16.0f);

    for (int tick = 0; tick <= 4; ++tick)
    {
        const float y = juce::jmap(static_cast<float>(tick), 0.0f, 4.0f, cup.getY() + 18.0f, cup.getBottom() - 16.0f);
        g.setColour(kMuted.withAlpha(0.85f));
        g.drawLine(cup.getRight() - 18.0f, y, cup.getRight() - 7.0f, y, 1.2f);
    }

    const float dashes[] = { 5.0f, 4.0f };
    juce::Path mark;
    mark.startNewSubPath(cup.getX() + 4.0f, markY);
    mark.lineTo(cup.getRight() - 4.0f, markY);
    juce::Path dashed;
    juce::PathStrokeType(1.3f).createDashedStroke(dashed, mark, dashes, 2, {});
    g.setColour(accent.withAlpha(0.9f));
    g.strokePath(dashed, juce::PathStrokeType(1.3f));

    g.saveState();
    g.reduceClipRegion(cup.toNearestInt().reduced(2));
    juce::ColourGradient water(juce::Colour(0xffb9e6fb), cup.getCentreX(), outY, juce::Colour(0xff1a6eb8), cup.getCentreX(), cup.getBottom(), false);
    g.setGradientFill(water);
    g.fillRect(cup.getX(), outY, cup.getWidth(), cup.getBottom() - outY + 2.0f);

    if (inY < outY - 1.0f)
    {
        g.setColour(juce::Colours::white.withAlpha(0.28f));

        for (float y = inY; y < outY; y += 7.0f)
            g.drawLine(cup.getX() + 8.0f, y, cup.getRight() - 8.0f, y + 12.0f, 1.0f);
    }

    g.setColour(juce::Colours::white.withAlpha(0.40f));
    g.fillEllipse(cup.getX() + 12.0f, outY - 7.0f + std::sin(readings.phase) * 1.4f, cup.getWidth() - 24.0f, 14.0f);
    g.restoreState();

    juce::Path stream;
    stream.startNewSubPath(spoutX, pipeY + 18.0f);

    for (int step = 1; step <= 14; ++step)
    {
        const float t = static_cast<float>(step) / 14.0f;
        stream.lineTo(spoutX + std::sin(t * 10.0f + readings.phase) * 2.2f, juce::jmap(t, pipeY + 18.0f, outY));
    }

    g.setColour(juce::Colour(0xffd7f1ff).withAlpha(0.35f + 0.55f * juce::jlimit(0.0f, 1.0f, readings.valveOpen)));
    g.strokePath(stream, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void drawSpark(juce::Graphics& g, juce::Rectangle<int> local, const SceneMap& map, const Readings& readings)
{
    const auto accent = kModeColour[2];
    juce::Rectangle<float> charge;
    juce::Rectangle<float> release;

    for (const auto& item : map.items)
    {
        if (item.role == 6)
            charge = item.bounds.toFloat();
        else if (item.role == 8)
            release = item.bounds.toFloat();
    }

    const float lift = juce::jmap(readings.fetInput, 0.0f, 48.0f, 0.0f, 26.0f);
    auto coil = charge.withTrimmedBottom(50.0f).translated(0.0f, -lift).reduced(6.0f, 0.0f);
    g.setColour(juce::Colour(0xff241c28));
    g.fillRoundedRectangle(charge.removeFromBottom(48.0f).translated(0.0f, 0.0f).reduced(4.0f, 2.0f), 8.0f);

    for (int turn = 0; turn < 9; ++turn)
    {
        const float y = coil.getBottom() - 8.0f - static_cast<float>(turn) * (coil.getHeight() / 9.0f);
        const float widen = 1.0f - std::abs(static_cast<float>(turn) - 4.0f) * 0.07f;
        const float turnWidth = (coil.getWidth() + 20.0f) * widen;
        auto turnRect = juce::Rectangle<float>(coil.getCentreX() - turnWidth * 0.5f, y, turnWidth, 16.0f);
        juce::ColourGradient copper(juce::Colour(0xfff0c09a), turnRect.getX(), turnRect.getY(), juce::Colour(0xff7a3e24), turnRect.getRight(), turnRect.getBottom(), false);
        g.setGradientFill(copper);
        g.fillEllipse(turnRect);
        g.setColour(juce::Colour(0xff4d2616).withAlpha(0.8f));
        g.drawEllipse(turnRect.reduced(3.0f, 3.0f), 1.2f);
    }

    auto toroid = juce::Rectangle<float>(coil.getX() - 22.0f, coil.getY() - 30.0f, coil.getWidth() + 44.0f, 36.0f);
    juce::ColourGradient silver(juce::Colour(0xfff8fbff), toroid.getX(), toroid.getY(), juce::Colour(0xff7f8b99), toroid.getCentreX(), toroid.getBottom(), false);
    g.setGradientFill(silver);
    g.fillEllipse(toroid);
    g.setColour(kBackground);
    g.fillEllipse(toroid.reduced(toroid.getWidth() * 0.30f, toroid.getHeight() * 0.18f));
    g.setColour(juce::Colour(0xffd5dde6));
    g.drawEllipse(toroid.reduced(4.0f, 3.0f), 2.0f);

    const float poleX = release.getX() - 18.0f;
    g.setColour(juce::Colour(0xffd5dbe3));
    g.fillRoundedRectangle(poleX - 5.0f, toroid.getCentreY(), 10.0f, release.getBottom() - toroid.getCentreY(), 4.0f);
    g.setGradientFill(silver);
    g.fillEllipse(poleX - 16.0f, toroid.getCentreY() - 16.0f, 32.0f, 32.0f);

    for (int ball = 0; ball < 4; ++ball)
    {
        const float x = poleX - 28.0f + static_cast<float>(ball) * 16.0f;
        g.fillEllipse(x, release.getBottom() + 6.0f, 12.0f, 12.0f);
    }

    juce::Random random(19 + juce::roundToInt(readings.phase * 8.0f));
    juce::Point<float> point(toroid.getRight() - 8.0f, toroid.getCentreY());
    juce::Path bolt;
    bolt.startNewSubPath(point);

    while (point.x < poleX - 18.0f)
    {
        point.x += 18.0f + random.nextFloat() * 12.0f;
        point.y += random.nextFloat() * 36.0f - 18.0f;
        bolt.lineTo(point);
    }

    bolt.lineTo(poleX, toroid.getCentreY());
    const float energy = juce::jlimit(0.35f, 1.0f, 0.35f + readings.arc + juce::jmin(0.5f, readings.grDb / 18.0f));
    g.setColour(accent.withAlpha(0.45f * energy));
    g.strokePath(bolt, juce::PathStrokeType(10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(juce::Colour(0xfff7f3ff).withAlpha(0.95f));
    g.strokePath(bolt, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::ignoreUnused(local);
}

void drawSlimMeter(juce::Graphics& g, juce::Rectangle<float> area, float norm, juce::Colour accent)
{
    auto bar = area.withTrimmedTop(50.0f).withSizeKeepingCentre(16.0f, juce::jmax(40.0f, area.getHeight() - 58.0f));
    g.setColour(kTrack);
    g.fillRoundedRectangle(bar, 7.0f);
    const float clamped = juce::jlimit(0.0f, 1.0f, norm);
    auto fill = bar;
    const float filled = bar.getHeight() * clamped;
    fill.setY(bar.getBottom() - filled);
    fill.setHeight(filled);
    juce::ColourGradient glow(accent.brighter(0.4f), fill.getCentreX(), fill.getY(), accent.darker(0.2f), fill.getCentreX(), fill.getBottom(), false);
    g.setGradientFill(glow);
    g.fillRoundedRectangle(fill, 7.0f);
}

void drawTube(juce::Graphics& g, juce::Rectangle<int> local, const SceneMap& map, const Readings& readings)
{
    const auto accent = kModeColour[3];
    juce::Rectangle<float> heat;
    juce::Rectangle<float> ratio;

    for (const auto& item : map.items)
    {
        if (item.role == 15)
            heat = item.bounds.toFloat();
        else if (item.role == 18)
            ratio = item.bounds.toFloat();
    }

    drawSlimMeter(g, heat, juce::jmap(juce::jlimit(-20.0f, 20.0f, readings.tubeInput), -20.0f, 20.0f, 0.08f, 1.0f), accent);
    drawSlimMeter(g, ratio, juce::jmap(juce::jlimit(1.0f, 6.0f, readings.ratioNow), 1.0f, 6.0f, 0.12f, 1.0f), accent);

    auto glass = juce::Rectangle<float>(local.getWidth() * 0.40f, local.getHeight() * 0.10f, local.getWidth() * 0.20f, local.getHeight() * 0.64f);
    const float glow = juce::jlimit(0.25f, 1.0f, 0.3f + readings.glow + juce::jmin(0.45f, std::abs(readings.grDb) / 12.0f));
    g.setColour(accent.withAlpha(0.22f * glow));
    g.fillEllipse(glass.expanded(48.0f, 28.0f));

    juce::ColourGradient bulb(juce::Colour(0xfff8f4ee).withAlpha(0.22f),
                              glass.getCentreX() - 10.0f,
                              glass.getY() + 10.0f,
                              juce::Colour(0xff6e655e).withAlpha(0.18f),
                              glass.getRight(),
                              glass.getBottom(),
                              false);
    g.setGradientFill(bulb);
    g.fillRoundedRectangle(glass, glass.getWidth() * 0.42f);
    g.setColour(juce::Colours::white.withAlpha(0.45f));
    g.drawRoundedRectangle(glass, glass.getWidth() * 0.42f, 2.0f);
    g.drawLine(glass.getX() + 12.0f, glass.getY() + 24.0f, glass.getX() + 12.0f, glass.getBottom() - 28.0f, 2.0f);

    auto filament = glass.reduced(glass.getWidth() * 0.40f, 36.0f);
    juce::ColourGradient filamentGlow(juce::Colour(0xfffff6df), filament.getCentreX(), filament.getY(), accent, filament.getCentreX(), filament.getBottom(), false);
    g.setGradientFill(filamentGlow);
    g.fillRoundedRectangle(filament, 5.0f);
    g.setColour(juce::Colour(0xfffff6df).withAlpha(0.8f));

    for (int rib = 0; rib < 6; ++rib)
    {
        const float y = filament.getY() + 8.0f + static_cast<float>(rib) * (filament.getHeight() - 16.0f) / 5.0f;
        g.drawLine(filament.getX() - 4.0f, y, filament.getRight() + 4.0f, y, 1.2f);
    }

    g.setColour(juce::Colour(0xffd9d3cb));

    for (int pin = 0; pin < 5; ++pin)
    {
        const float x = glass.getX() + 20.0f + static_cast<float>(pin) * (glass.getWidth() - 40.0f) / 4.0f;
        g.fillRoundedRectangle(x, glass.getBottom() - 2.0f, 5.0f, 14.0f, 1.5f);
    }

    auto wave = [&](float x, float width, float amplitude) {
        juce::Path path;
        const float y = glass.getCentreY();

        for (int step = 0; step <= 56; ++step)
        {
            const float t = static_cast<float>(step) / 56.0f;
            const float px = x + width * t;
            const float env = std::sin(t * juce::MathConstants<float>::pi);
            const float py = y + std::sin(t * juce::MathConstants<float>::twoPi * 3.0f + readings.phase) * amplitude * env;
            if (step == 0)
                path.startNewSubPath(px, py);
            else
                path.lineTo(px, py);
        }

        return path;
    };

    const float amplitude = juce::jmap(juce::jlimit(-20.0f, 20.0f, readings.heat), -20.0f, 20.0f, 10.0f, 46.0f);
    g.setColour(accent.withAlpha(0.95f));
    g.strokePath(wave(heat.getRight() + 8.0f, glass.getX() - heat.getRight() - 16.0f, amplitude), juce::PathStrokeType(2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(accent.withAlpha(0.55f));
    g.strokePath(wave(glass.getRight() + 10.0f, ratio.getX() - glass.getRight() - 18.0f, amplitude / juce::jmax(1.0f, readings.ratioNow)), juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::ignoreUnused(map);
}

const char* roleParameter(int role)
{
    switch (role)
    {
        case 1: return params::optoPeakRed;
        case 2: return params::optoLimit;
        case 3: return params::vcaRatio;
        case 4: return params::vcaThreshold;
        case 6: return params::fetInput;
        case 7: return params::fetAttack;
        case 8: return params::fetRelease;
        case 9:
        case 10:
        case 11:
        case 12:
        case 13: return params::fetRatio;
        case 15: return params::vmuInput;
        case 16: return params::vmuThreshold;
        case 17: return params::vmuTime;
        default: return nullptr;
    }
}

} // namespace

SceneView::SceneView(SqueezeProcessor& processorIn)
    : processor(processorIn)
{
}

SceneView::~SceneView()
{
    end();
}

void SceneView::advance()
{
    phase += 0.18f;
}

std::string SceneView::layoutProblems() const
{
    Readings readings;
    readings.mode = juce::jlimit(0, 3, juce::roundToInt(rawOf(processor, params::mode)));
    readings.grDb = std::abs(processor.lastVisualFrame().grDb);
    readings.peak = rawOf(processor, params::optoPeakRed);
    readings.limit = rawOf(processor, params::optoLimit) >= 0.5f;
    readings.threshold = rawOf(processor, params::vcaThreshold);
    readings.ratio = rawOf(processor, params::vcaRatio);
    readings.valveOpen = processor.lastVisualFrame().vcaValveOpen;
    readings.rmsDb = processor.lastVisualFrame().vcaRmsDb;
    readings.fetInput = rawOf(processor, params::fetInput);
    readings.attack = rawOf(processor, params::fetAttack);
    readings.release = rawOf(processor, params::fetRelease);
    readings.ratioIndex = juce::roundToInt(rawOf(processor, params::fetRatio));
    readings.allEnabled = rawOf(processor, params::fetAllEnabled) >= 0.5f;
    readings.tubeInput = rawOf(processor, params::vmuInput);
    readings.tubeThreshold = rawOf(processor, params::vmuThreshold);
    readings.timeIndex = juce::roundToInt(rawOf(processor, params::vmuTime));
    readings.ratioNow = processor.lastVisualFrame().vmuRatioNow;
    const auto map = buildMap(getLocalBounds(), readings);
    std::string problems;

    for (size_t i = 0; i < map.items.size(); ++i)
    {
        const auto& item = map.items[i];

        if (! getLocalBounds().contains(item.bounds))
            problems += "outside " + std::to_string(item.role) + "\n";

        if (item.role > 0 && (item.bounds.getWidth() < 28 || item.bounds.getHeight() < 28))
            problems += "small " + std::to_string(item.role) + "\n";

        if (! item.fits())
            problems += "text " + item.primary.toStdString() + " / " + item.secondary.toStdString() + "\n";

        for (size_t j = i + 1; j < map.items.size(); ++j)
            if (item.bounds.intersects(map.items[j].bounds))
                problems += "overlap " + std::to_string(item.role) + " " + std::to_string(map.items[j].role) + "\n";
    }

    return problems;
}

void SceneView::begin(juce::RangedAudioParameter* next)
{
    if (gesture == next)
        return;

    end();
    gesture = next;

    if (gesture != nullptr)
        gesture->beginChangeGesture();
}

void SceneView::writeNorm(float norm)
{
    if (gesture != nullptr)
        gesture->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, norm));
}

void SceneView::writeDenorm(float value)
{
    if (gesture != nullptr)
        writeNorm(gesture->convertTo0to1(value));
}

void SceneView::end()
{
    if (gesture != nullptr)
        gesture->endChangeGesture();

    gesture = nullptr;
}

void SceneView::applyRole(int role, float dx, float dy, bool fromStart)
{
    if (role == 5 || role == 14 || role == 18)
        return;

    if (role == 13 && rawOf(processor, params::fetAllEnabled) < 0.5f)
        return;

    const char* id = roleParameter(role);
    auto* ranged = id != nullptr ? parameter(processor, id) : nullptr;

    if (ranged == nullptr)
        return;

    begin(ranged);
    const float baseNorm = fromStart ? startNorm : ranged->getValue();
    const float base = ranged->convertFrom0to1(baseNorm);

    if (role == 1)
        writeDenorm(juce::jlimit(0.0f, 100.0f, base + dy * 0.45f));
    else if (role == 2)
        writeDenorm(rawOf(processor, params::optoLimit) >= 0.5f ? 0.0f : 1.0f);
    else if (role == 3)
    {
        const float steps[] = { 1.0f, 1.5f, 2.0f, 4.0f, 8.0f, 1000.0f };
        int nearest = 0;
        float best = 100000.0f;

        for (int index = 0; index < 6; ++index)
        {
            const float distance = std::abs(base - steps[index]);

            if (distance < best)
            {
                best = distance;
                nearest = index;
            }
        }

        const int next = juce::jlimit(0, 5, nearest + juce::roundToInt(dy / 28.0f));
        writeDenorm(steps[next]);
    }
    else if (role == 4 || role == 16)
        writeDenorm(juce::jlimit(-40.0f, 0.0f, base + dy * 0.15f));
    else if (role == 6)
        writeDenorm(juce::jlimit(0.0f, 48.0f, base + dy * 0.22f));
    else if (role == 7 || role == 8)
        writeNorm(baseNorm + (role == 7 ? dx : dy) / 220.0f);
    else if (role >= 9 && role <= 13)
        writeDenorm(static_cast<float>(role - 9));
    else if (role == 15)
        writeDenorm(juce::jlimit(-20.0f, 20.0f, base + dy * 0.15f));
    else if (role == 17)
        writeDenorm(static_cast<float>(juce::jlimit(0, 5, juce::roundToInt(base) + juce::roundToInt(dy / 24.0f))));
}

void SceneView::nudge(bool locked, float delta)
{
    Readings readings;
    readings.mode = juce::jlimit(0, 3, juce::roundToInt(rawOf(processor, params::mode)));
    readings.peak = rawOf(processor, params::optoPeakRed);
    readings.allEnabled = rawOf(processor, params::fetAllEnabled) >= 0.5f;
    readings.timeIndex = juce::roundToInt(rawOf(processor, params::vmuTime));
    const auto map = buildMap(getLocalBounds(), readings);

    for (const auto& item : map.items)
    {
        if (item.role <= 0 || item.locked != locked)
            continue;

        applyRole(item.role, 0.0f, delta, false);
        end();
        return;
    }
}

void SceneView::selectAllRatio()
{
    applyRole(13, 0.0f, 0.0f, false);
    end();
}

void SceneView::paint(juce::Graphics& g)
{
    Readings readings;
    readings.mode = juce::jlimit(0, 3, juce::roundToInt(rawOf(processor, params::mode)));
    const auto& frame = processor.lastVisualFrame();
    readings.grDb = std::abs(frame.grDb);
    readings.peak = rawOf(processor, params::optoPeakRed);
    readings.limit = rawOf(processor, params::optoLimit) >= 0.5f;
    readings.threshold = rawOf(processor, params::vcaThreshold);
    readings.ratio = rawOf(processor, params::vcaRatio);
    readings.valveOpen = frame.vcaValveOpen;
    readings.rmsDb = frame.vcaRmsDb;
    readings.fetInput = rawOf(processor, params::fetInput);
    readings.attack = rawOf(processor, params::fetAttack);
    readings.release = rawOf(processor, params::fetRelease);
    readings.ratioIndex = juce::roundToInt(rawOf(processor, params::fetRatio));
    readings.allEnabled = rawOf(processor, params::fetAllEnabled) >= 0.5f;
    readings.arc = frame.fetArc;
    readings.heat = frame.vmuHeatDb;
    readings.ratioNow = frame.vmuRatioNow;
    readings.glow = frame.vmuGlow;
    readings.tubeInput = rawOf(processor, params::vmuInput);
    readings.tubeThreshold = rawOf(processor, params::vmuThreshold);
    readings.timeIndex = juce::roundToInt(rawOf(processor, params::vmuTime));
    readings.cell = frame.optoCell;
    readings.phase = phase;

    auto local = getLocalBounds();
    const auto map = buildMap(local, readings);
    drawFloor(g, local);

    if (readings.mode == 0)
        drawLamp(g, map, readings);
    else if (readings.mode == 1)
        drawNeedle(g, map, readings);
    else if (readings.mode == 2)
        drawSpark(g, local, map, readings);
    else
        drawTube(g, local, map, readings);

    for (const auto& item : map.items)
        paintText(g, item, kModeColour[readings.mode]);
}

int hitRole(juce::Rectangle<int> local, juce::Point<float> position, const Readings& readings)
{
    const auto map = buildMap(local, readings);

    for (const auto& item : map.items)
        if (item.role > 0 && item.bounds.contains(position.toInt()))
            return item.role;

    return 0;
}

void SceneView::mouseDown(const juce::MouseEvent& event)
{
    Readings readings;
    readings.mode = juce::jlimit(0, 3, juce::roundToInt(rawOf(processor, params::mode)));
    readings.peak = rawOf(processor, params::optoPeakRed);
    readings.allEnabled = rawOf(processor, params::fetAllEnabled) >= 0.5f;
    activeRole = hitRole(getLocalBounds(), event.position, readings);

    if (activeRole == 0 || activeRole == 5 || activeRole == 14 || activeRole == 18)
    {
        activeRole = 0;
        return;
    }

    if (activeRole == 13 && ! readings.allEnabled)
    {
        activeRole = 0;
        return;
    }

    if (activeRole == 2 || (activeRole >= 9 && activeRole <= 13))
    {
        applyRole(activeRole, 0.0f, 0.0f, false);
        end();
        activeRole = 0;
        return;
    }

    if (const char* id = roleParameter(activeRole))
        if (auto* ranged = parameter(processor, id))
            startNorm = ranged->getValue();

    startPos = event.position;
    applyRole(activeRole, 0.0f, 0.0f, true);
}

void SceneView::mouseDrag(const juce::MouseEvent& event)
{
    if (activeRole == 0)
        return;

    applyRole(activeRole, event.position.x - startPos.x, startPos.y - event.position.y, true);
}

void SceneView::mouseUp(const juce::MouseEvent&)
{
    end();
    activeRole = 0;
}

void SceneView::mouseDoubleClick(const juce::MouseEvent& event)
{
    Readings readings;
    readings.mode = juce::jlimit(0, 3, juce::roundToInt(rawOf(processor, params::mode)));
    readings.peak = rawOf(processor, params::optoPeakRed);
    readings.allEnabled = true;
    const int role = hitRole(getLocalBounds(), event.position, readings);

    if (const char* id = roleParameter(role))
    {
        if (auto* ranged = parameter(processor, id))
        {
            begin(ranged);
            writeNorm(ranged->getDefaultValue());
            end();
        }
    }
}

void SceneView::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    Readings readings;
    readings.mode = juce::jlimit(0, 3, juce::roundToInt(rawOf(processor, params::mode)));
    readings.allEnabled = rawOf(processor, params::fetAllEnabled) >= 0.5f;
    const int role = hitRole(getLocalBounds(), event.position, readings);

    if (role == 0)
        return;

    applyRole(role, wheel.deltaX * 40.0f, wheel.deltaY * 40.0f, false);
    end();
}

} // namespace squeeze::ui
