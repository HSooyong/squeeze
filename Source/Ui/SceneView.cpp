#include "Ui/SceneView.h"

#include "BinaryData.h"
#include "Parameters.h"
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
    float outLevelDb = -160.0f;
    float fetInput = 24.0f;
    float attack = 300.0f;
    float release = 250.0f;
    int ratioIndex = 0;
    bool allEnabled = false;
    float arc = 0.0f;
    float grit = 0.0f;
    float ratioNow = 1.0f;
    float glow = 0.0f;
    int timeIndex = 2;
    float cell = 0.0f;
    float tubeInput = 0.0f;
    float tubeThreshold = -22.0f;
    float phase = 0.0f;
    bool learn = false;
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
    bool framed = true;

    juce::Rectangle<int> content() const
    {
        const int padX = framed ? 10 : 4;
        const int padY = framed ? 7 : 2;
        auto inside = bounds.reduced(padX, padY);
        return inside.getWidth() > 4 && inside.getHeight() > 4 ? inside : bounds;
    }

    bool fits() const
    {
        if (primary.isEmpty() && secondary.isEmpty())
            return true;

        const auto inside = content();
        const auto first = fitLine(primary, preferred, minimum, inside.getWidth(), 0);
        const auto second = fitLine(secondary, preferredSecondary, minimumSecondary, inside.getWidth(), 0);
        const float used = secondary.isEmpty() ? first.size : first.size + second.size + 3.0f;
        return first.ok && second.ok && used <= static_cast<float>(inside.getHeight());
    }
};

struct SceneMap
{
    std::vector<Item> items;
    std::vector<juce::Rectangle<float>> solids;
};

juce::Rectangle<int> enclose(juce::Rectangle<float> solid)
{
    const int x = static_cast<int>(std::floor(solid.getX()));
    const int y = static_cast<int>(std::floor(solid.getY()));
    const int right = static_cast<int>(std::ceil(solid.getRight()));
    const int bottom = static_cast<int>(std::ceil(solid.getBottom()));
    return { x, y, juce::jmax(0, right - x), juce::jmax(0, bottom - y) };
}

juce::Rectangle<float> frac(juce::Rectangle<int> local, float x, float y, float w, float h)
{
    return { local.getX() + local.getWidth() * x,
             local.getY() + local.getHeight() * y,
             local.getWidth() * w,
             local.getHeight() * h };
}

juce::Rectangle<int> fracInt(juce::Rectangle<int> local, float x, float y, float w, float h)
{
    return frac(local, x, y, w, h).toNearestInt();
}

float rawOf(const SqueezeProcessor& processor, const char* id)
{
    if (auto* raw = processor.getApvts().getRawParameterValue(id))
        return raw->load();

    return 0.0f;
}

juce::RangedAudioParameter* parameter(SqueezeProcessor& processor, const char* id)
{
    return dynamic_cast<juce::RangedAudioParameter*>(processor.getApvts().getParameter(id));
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

juce::String numberDb(float value)
{
    if (! std::isfinite(value) || value < -80.0f)
        return "--";

    return juce::String(value, 1) + " dB";
}

juce::String signedNumber(float value)
{
    if (value > 0.05f)
        return "+" + juce::String(value, 0);

    return juce::String(juce::roundToInt(value));
}

juce::String ratioText(float value)
{
    if (value >= 999.0f)
        return "inf : 1";

    const auto nearest = std::round(value);
    if (std::abs(value - nearest) < 0.05f)
        return juce::String(static_cast<int>(nearest)) + " : 1";

    return juce::String(value, 1) + " : 1";
}

void addSolid(SceneMap& map, juce::Rectangle<float> solid)
{
    if (! solid.isEmpty())
        map.solids.push_back(solid);
}

SceneMap buildMap(juce::Rectangle<int> local, const Readings& readings)
{
    SceneMap map;

    if (local.getWidth() < 32 || local.getHeight() < 32)
        return map;

    if (readings.mode == 0)
    {
        map.items.push_back(makeItem(fracInt(local, 0.014f, 0.026f, 0.167f, 0.104f),
                                     readings.limit ? "Limit" : "Compress",
                                     "shade",
                                     2,
                                     false));
        map.items.push_back(makeItem(fracInt(local, 0.452f, 0.251f, 0.096f, 0.139f), {}, {}, 1, false));
        addSolid(map, frac(local, 0.431f, 0.216f, 0.167f, 0.238f));
        addSolid(map, frac(local, 0.580f, 0.519f, 0.260f, 0.240f));
        addSolid(map, frac(local, 0.179f, 0.721f, 0.160f, 0.119f));
    }
    else if (readings.mode == 1)
    {
        const int open = juce::roundToInt(juce::jlimit(0.0f, 1.0f, readings.valveOpen) * 100.0f);
        auto opened = makeItem(fracInt(local, 0.514f, 0.017f, 0.156f, 0.087f),
                               juce::String(open) + "% open",
                               "OPEN",
                               0,
                               false);
        opened.framed = false;
        map.items.push_back(opened);
        map.items.push_back(makeItem(fracInt(local, 0.359f, 0.017f, 0.091f, 0.165f), {}, {}, 3, false));
        map.items.push_back(makeItem(fracInt(local, 0.014f, 0.190f, 0.201f, 0.113f), "Valve speed", "AUTO", 5, true));
        map.items.push_back(makeItem(fracInt(local, 0.120f, 0.455f, 0.211f, 0.113f),
                                     "Threshold",
                                     numberDb(readings.threshold),
                                     4,
                                     false));
        auto inLevel = makeItem(fracInt(local, 0.802f, 0.026f, 0.179f, 0.087f), numberDb(readings.rmsDb), "IN", 0, false);
        auto outLevel = makeItem(fracInt(local, 0.802f, 0.150f, 0.179f, 0.087f), numberDb(readings.outLevelDb), "OUT", 0, false);
        inLevel.framed = false;
        outLevel.framed = false;
        map.items.push_back(inLevel);
        map.items.push_back(outLevel);
        addSolid(map, frac(local, 0.359f, 0.017f, 0.091f, 0.165f));
        addSolid(map, frac(local, 0.431f, 0.152f, 0.048f, 0.195f));
        addSolid(map, frac(local, 0.359f, 0.368f, 0.299f, 0.498f));
    }
    else if (readings.mode == 2)
    {
        map.items.push_back(makeItem(fracInt(local, 0.203f, 0.303f, 0.108f, 0.433f), {}, {}, 6, false));
        map.items.push_back(makeItem(fracInt(local, 0.014f, 0.866f, 0.179f, 0.113f),
                                     "Input " + signedNumber(readings.fetInput),
                                     "INPUT",
                                     0,
                                     false));
        map.items.push_back(makeItem(fracInt(local, 0.395f, 0.035f, 0.179f, 0.104f), "Fixed line", "FIXED", 14, true));
        map.items.push_back(makeItem(fracInt(local, 0.813f, 0.152f, 0.167f, 0.113f),
                                     juce::String(juce::roundToInt(readings.release)) + " ms",
                                     "RELEASE",
                                     8,
                                     false));
        map.items.push_back(makeItem(fracInt(local, 0.359f, 0.779f, 0.216f, 0.113f),
                                     juce::String(juce::roundToInt(readings.attack)) + juce::String::fromUTF8(" \xC2\xB5s"),
                                     "ATTACK",
                                     7,
                                     false));

        const char* names[] = { "4", "8", "12", "20", "ALL" };

        for (int index = 0; index < 5; ++index)
        {
            const float x = 0.598f + static_cast<float>(index) * 0.0502f;
            auto dot = makeItem(fracInt(local, x, 0.918f, 0.0407f, 0.0736f), names[index], {}, 9 + index, index == 4 && ! readings.allEnabled);
            dot.preferred = 12.0f;
            dot.minimum = 9.0f;
            dot.framed = false;
            map.items.push_back(dot);
        }

        addSolid(map, frac(local, 0.167f, 0.195f, 0.179f, 0.095f));
        addSolid(map, frac(local, 0.203f, 0.303f, 0.108f, 0.433f));
        addSolid(map, frac(local, 0.742f, 0.216f, 0.048f, 0.087f));
        addSolid(map, frac(local, 0.756f, 0.303f, 0.019f, 0.390f));
    }
    else
    {
        map.items.push_back(makeItem(fracInt(local, 0.048f, 0.260f, 0.038f, 0.390f), {}, {}, 15, false));
        map.items.push_back(makeItem(fracInt(local, 0.014f, 0.026f, 0.203f, 0.104f),
                                     "Input " + numberDb(readings.tubeInput),
                                     "HEAT",
                                     0,
                                     false));
        map.items.push_back(makeItem(fracInt(local, 0.014f, 0.866f, 0.216f, 0.113f),
                                     "Threshold",
                                     numberDb(readings.tubeThreshold),
                                     16,
                                     false));
        map.items.push_back(makeItem(fracInt(local, 0.395f, 0.866f, 0.192f, 0.113f),
                                     "Time T" + juce::String(readings.timeIndex + 1),
                                     "TIME",
                                     17,
                                     false));
        map.items.push_back(makeItem(fracInt(local, 0.766f, 0.026f, 0.215f, 0.121f),
                                     "Ratio " + juce::String(readings.ratioNow, 1) + " : 1",
                                     "RATIO",
                                     18,
                                     true));
        addSolid(map, frac(local, 0.359f, 0.108f, 0.239f, 0.606f));
        addSolid(map, frac(local, 0.048f, 0.260f, 0.038f, 0.390f));
        addSolid(map, frac(local, 0.909f, 0.260f, 0.033f, 0.390f));
    }

    return map;
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

juce::Image& sceneArt(int mode)
{
    static juce::Image images[4];
    static bool loaded = false;

    if (! loaded)
    {
        images[0] = juce::ImageFileFormat::loadFrom(SqueezeArt::scene_lamp_jpg, SqueezeArt::scene_lamp_jpgSize);
        images[1] = juce::ImageFileFormat::loadFrom(SqueezeArt::scene_needle_jpg, SqueezeArt::scene_needle_jpgSize);
        images[2] = juce::ImageFileFormat::loadFrom(SqueezeArt::scene_spark_jpg, SqueezeArt::scene_spark_jpgSize);
        images[3] = juce::ImageFileFormat::loadFrom(SqueezeArt::scene_tube_jpg, SqueezeArt::scene_tube_jpgSize);
        loaded = true;
    }

    return images[juce::jlimit(0, 3, mode)];
}

void paintPlate(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& text, juce::Colour colour, float size, bool emphasize)
{
    if (text.isEmpty() || bounds.isEmpty())
        return;

    auto plate = bounds.toFloat();
    g.setColour(juce::Colour(0xff121318).withAlpha(emphasize ? 0.88f : 0.72f));
    g.fillRoundedRectangle(plate, 7.0f);
    g.setColour(colour.withAlpha(0.9f));
    g.drawRoundedRectangle(plate, 7.0f, 1.0f);
    g.setColour(juce::Colours::white);
    g.setFont(interfaceFont(size));
    g.drawText(text, bounds, juce::Justification::centred, false);
}

void drawLampOverlay(juce::Graphics& g, juce::Rectangle<int> local, const Readings& readings)
{
    const float gr = readings.grDb;
    const auto accent = kModeColour[0];
    const float calm = juce::jlimit(0.0f, 0.5f, (6.0f - gr) / 14.0f);
    juce::ColourGradient shade(juce::Colours::black.withAlpha(calm),
                               local.getWidth() * 0.70f,
                               local.getHeight() * 0.62f,
                               juce::Colours::transparentBlack,
                               local.getWidth() * 0.52f,
                               local.getHeight() * 0.32f,
                               true);
    g.setGradientFill(shade);
    g.fillEllipse(frac(local, 0.48f, 0.28f, 0.42f, 0.48f));

    const float hot = juce::jlimit(0.0f, 0.55f, readings.cell * 0.2f + juce::jmax(0.0f, gr - 5.0f) / 16.0f);
    const float pulse = 0.85f + 0.15f * std::sin(readings.phase);
    g.setColour(juce::Colour(0xffffe2a8).withAlpha(hot * pulse));
    g.fillEllipse(frac(local, 0.50f, 0.30f, 0.07f, 0.07f));
    paintPlate(g, fracInt(local, 0.58f, 0.46f, 0.16f, 0.07f), juce::String(-gr, 1) + " dB", accent, 15.0f, true);
}

void drawNeedleOverlay(juce::Graphics& g, juce::Rectangle<int> local, const Readings& readings)
{
    const auto accent = kModeColour[1];
    const float cupTop = 0.42f;
    const float cupBottom = 0.76f;
    const auto yFor = [&](float db) {
        const float clamped = juce::jlimit(-40.0f, 0.0f, db);
        const float t = (0.0f - clamped) / 40.0f;
        return local.getHeight() * (cupTop + (cupBottom - cupTop) * t);
    };

    const float open = juce::jlimit(0.04f, 1.0f, readings.valveOpen);
    const float streamX = local.getWidth() * 0.505f;
    const float streamTop = local.getHeight() * 0.30f;
    const float streamBottom = local.getHeight() * 0.52f;
    g.setColour(juce::Colour(0xffd7f3ff).withAlpha(0.18f + 0.55f * open));
    g.drawLine(streamX, streamTop, streamX + std::sin(readings.phase) * 3.0f, streamBottom, 1.2f + open * 9.0f);

    if (readings.threshold > -80.0f)
    {
        const float thresholdY = yFor(readings.threshold);
        g.setColour(juce::Colour(0xffff5a4a).withAlpha(0.9f));
        g.drawLine(local.getWidth() * 0.40f, thresholdY, local.getWidth() * 0.62f, thresholdY, 2.0f);
    }

    const int openPercent = juce::roundToInt(open * 100.0f);
    paintPlate(g, fracInt(local, 0.52f, 0.145f, 0.14f, 0.055f), juce::String(openPercent) + "% open", accent, 12.0f, true);
    paintPlate(g, fracInt(local, 0.62f, 0.44f, 0.14f, 0.06f), juce::String(-readings.grDb, 1) + " dB", accent, 14.0f, true);

    const float over = readings.rmsDb > -80.0f ? juce::jmax(0.0f, readings.rmsDb - readings.threshold) : 0.0f;
    const float kept = readings.ratio >= 999.0f ? 0.0f : over / juce::jmax(1.0f, readings.ratio);
    auto card = fracInt(local, 0.63f, 0.56f, 0.28f, 0.20f);
    g.setColour(juce::Colour(0xff121318).withAlpha(0.94f));
    g.fillRoundedRectangle(card.toFloat(), 8.0f);
    g.setColour(accent.withAlpha(0.75f));
    g.drawRoundedRectangle(card.toFloat(), 8.0f, 1.0f);
    g.setColour(kMuted);
    g.setFont(interfaceFont(10.0f));
    g.drawText("ABOVE THE LINE", card.removeFromTop(16), juce::Justification::centred, false);
    g.setColour(kText);
    g.setFont(interfaceFont(12.0f));
    g.drawText("IN  " + juce::String(over, 1) + " dB", card.removeFromTop(16), juce::Justification::centred, false);
    g.drawText(ratioText(readings.ratio), card.removeFromTop(16), juce::Justification::centred, false);
    g.drawText("OUT  " + juce::String(kept, 1) + " dB", card, juce::Justification::centred, false);
}

void drawSparkOverlay(juce::Graphics& g, juce::Rectangle<int> local, const Readings& readings)
{
    const auto accent = kModeColour[2];
    const float cover = juce::jlimit(0.0f, 0.82f, 0.82f - readings.arc * 1.8f);
    g.setColour(juce::Colour(0xff120c18).withAlpha(cover));
    g.fillRoundedRectangle(frac(local, 0.38f, 0.18f, 0.40f, 0.28f), 20.0f);

    const auto from = juce::Point<float>(local.getWidth() * 0.40f, local.getHeight() * 0.30f);
    const auto to = juce::Point<float>(local.getWidth() * 0.74f, local.getHeight() * 0.34f);
    const int strands = 1 + juce::jlimit(0, 4, readings.ratioIndex);
    const float alpha = juce::jlimit(0.0f, 0.85f, readings.arc);

    if (readings.arc < 0.08f)
    {
        g.setColour(accent.withAlpha(0.45f + 0.2f * std::sin(readings.phase)));
        g.drawEllipse(frac(local, 0.30f, 0.20f, 0.16f, 0.10f), 2.0f);
    }
    else
    {
        juce::Path arc;
        arc.startNewSubPath(from);
        arc.cubicTo(from.x + 40.0f, from.y - 16.0f - static_cast<float>(strands) * 3.0f, to.x - 30.0f, to.y + 10.0f, to.x, to.y);
        g.setColour(juce::Colours::white.withAlpha(alpha));
        g.strokePath(arc, juce::PathStrokeType(1.5f + static_cast<float>(strands) * 1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const int sparks = juce::jlimit(0, 10, strands * 2);
        g.setColour(juce::Colour(0xffffb15a).withAlpha(0.85f));

        for (int spark = 0; spark < sparks; ++spark)
        {
            const float t = static_cast<float>(spark + 1) / static_cast<float>(sparks + 1);
            const float x = from.x + (to.x - from.x) * t;
            const float y = from.y + std::sin(readings.phase + static_cast<float>(spark)) * (4.0f + static_cast<float>(strands));
            g.fillEllipse(x, y, 2.5f, 2.5f);
        }
    }

    paintPlate(g, fracInt(local, 0.46f, 0.14f, 0.15f, 0.065f), juce::String(-readings.grDb, 1) + " dB", accent, 14.0f, true);

    if (readings.ratioIndex >= 0 && readings.ratioIndex < 5)
    {
        const float x = 0.62f + static_cast<float>(readings.ratioIndex) * 0.045f;
        g.setColour(accent);
        g.drawEllipse(frac(local, x, 0.90f, 0.035f, 0.055f), 2.0f);
    }
}

void drawTubeOverlay(juce::Graphics& g, juce::Rectangle<int> local, const Readings& readings)
{
    const auto accent = kModeColour[3];
    const float amplitude = juce::jmap(juce::jlimit(-20.0f, 20.0f, readings.tubeInput), -20.0f, 20.0f, 6.0f, local.getHeight() * 0.09f);
    const auto wave = [&](float x, float width, float amount) {
        juce::Path path;
        const float mid = local.getHeight() * 0.48f;

        for (int step = 0; step <= 24; ++step)
        {
            const float t = static_cast<float>(step) / 24.0f;
            const float px = x + width * t;
            const float py = mid + std::sin(t * 18.0f + readings.phase) * amount * std::sin(t * juce::MathConstants<float>::pi);
            if (step == 0)
                path.startNewSubPath(px, py);
            else
                path.lineTo(px, py);
        }

        return path;
    };

    g.setColour(accent.withAlpha(0.55f));
    g.strokePath(wave(local.getWidth() * 0.12f, local.getWidth() * 0.20f, amplitude), juce::PathStrokeType(2.0f));
    const float outAmount = amplitude / juce::jmax(1.2f, readings.ratioNow);
    g.setColour(accent.withAlpha(0.4f));
    g.strokePath(wave(local.getWidth() * 0.66f, local.getWidth() * 0.20f, outAmount), juce::PathStrokeType(1.8f));

    const float glow = juce::jlimit(0.05f, 0.55f, 0.05f + readings.glow * 0.5f);
    g.setColour(juce::Colour(0xffffc27a).withAlpha(glow));
    g.fillEllipse(frac(local, 0.46f, 0.34f, 0.08f, 0.24f));

    paintPlate(g, fracInt(local, 0.41f, 0.22f, 0.16f, 0.065f), juce::String(-readings.grDb, 1) + " dB", accent, 14.0f, true);

    const float ratioT = juce::jlimit(0.0f, 1.0f, (readings.ratioNow - 1.0f) / 5.0f);
    const auto bar = frac(local, 0.90f, 0.26f, 0.03f, 0.40f);
    const float marker = bar.getBottom() - bar.getHeight() * ratioT;
    g.setColour(juce::Colours::white);
    g.fillEllipse(bar.getX() - 4.0f, marker - 4.0f, 8.0f, 8.0f);
}

void drawLearnMarks(juce::Graphics& g, juce::Rectangle<int> local, int mode)
{
    const auto accent = kModeColour[mode];
    const juce::Point<float> marks[4][4] = {
        { { 0.22f, 0.78f }, { 0.52f, 0.30f }, { 0.70f, 0.62f }, { 0.78f, 0.42f } },
        { { 0.20f, 0.16f }, { 0.28f, 0.50f }, { 0.46f, 0.10f }, { 0.55f, 0.55f } },
        { { 0.24f, 0.78f }, { 0.28f, 0.42f }, { 0.55f, 0.28f }, { 0.76f, 0.36f } },
        { { 0.10f, 0.40f }, { 0.48f, 0.36f }, { 0.90f, 0.40f }, { 0.48f, 0.90f } },
    };

    for (int index = 0; index < 4; ++index)
    {
        const auto mark = marks[mode][index];
        auto bubble = juce::Rectangle<float>(local.getWidth() * mark.x, local.getHeight() * mark.y, 22.0f, 22.0f);
        g.setColour(accent);
        g.fillEllipse(bubble);
        g.setColour(juce::Colour(0xff16120c));
        g.setFont(interfaceFont(12.0f));
        g.drawText(juce::String(index + 1), bubble.toNearestInt(), juce::Justification::centred, false);
    }
}

Readings readReadings(const SqueezeProcessor& processor, float phase)
{
    Readings readings;
    const auto& frame = processor.lastVisualFrame();
    readings.mode = juce::jlimit(0, 3, juce::roundToInt(rawOf(processor, params::mode)));
    readings.grDb = std::abs(frame.grDb);
    readings.peak = rawOf(processor, params::optoPeakRed);
    readings.limit = rawOf(processor, params::optoLimit) >= 0.5f;
    readings.threshold = rawOf(processor, params::vcaThreshold);
    readings.ratio = rawOf(processor, params::vcaRatio);
    readings.valveOpen = frame.vcaValveOpen;
    readings.rmsDb = frame.vcaRmsDb;
    readings.outLevelDb = frame.vcaRmsDb - std::abs(frame.grDb);
    readings.fetInput = rawOf(processor, params::fetInput);
    readings.attack = rawOf(processor, params::fetAttack);
    readings.release = rawOf(processor, params::fetRelease);
    readings.ratioIndex = juce::roundToInt(rawOf(processor, params::fetRatio));
    readings.allEnabled = rawOf(processor, params::fetAllEnabled) >= 0.5f;
    readings.arc = frame.fetArc;
    readings.grit = frame.fetGrit;
    readings.ratioNow = frame.vmuRatioNow > 0.0f ? frame.vmuRatioNow : 1.0f;
    readings.glow = frame.vmuGlow;
    readings.tubeInput = rawOf(processor, params::vmuInput);
    readings.tubeThreshold = rawOf(processor, params::vmuThreshold);
    readings.timeIndex = juce::roundToInt(rawOf(processor, params::vmuTime));
    readings.cell = frame.optoCell;
    readings.phase = phase;
    readings.learn = processor.getUiState().learnOpen;
    return readings;
}

int hitRole(juce::Rectangle<int> local, juce::Point<float> position, const Readings& readings)
{
    const auto map = buildMap(local, readings);

    for (const auto& item : map.items)
        if (item.role > 0 && item.bounds.contains(position.toInt()))
            return item.role;

    return 0;
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
    const auto readings = readReadings(processor, phase);
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

        const bool hasText = item.primary.isNotEmpty() || item.secondary.isNotEmpty();

        if (hasText)
            for (const auto& solid : map.solids)
                if (item.bounds.intersects(enclose(solid)))
                    problems += "label on object " + item.primary.toStdString() + "\n";

        for (size_t j = i + 1; j < map.items.size(); ++j)
        {
            const bool otherText = map.items[j].primary.isNotEmpty() || map.items[j].secondary.isNotEmpty();

            if (hasText && otherText && item.bounds.intersects(map.items[j].bounds))
                problems += "overlap " + item.primary.toStdString() + " / " + map.items[j].primary.toStdString() + "\n";
        }
    }

    for (const auto& solid : map.solids)
        if (! getLocalBounds().contains(enclose(solid)))
            problems += "object outside\n";

    return problems;
}

void SceneView::begin(juce::RangedAudioParameter* next)
{
    if (gesture == next)
        return;

    end();
    gesture = next;

    if (gesture != nullptr)
    {
        processor.beginGestureUndo();
        gesture->beginChangeGesture();
    }
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
    {
        gesture->endChangeGesture();
        processor.commitGesture();
    }

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
    const auto readings = readReadings(processor, phase);
    auto local = getLocalBounds();
    g.fillAll(kBackground);

    if (auto art = sceneArt(readings.mode); art.isValid())
        g.drawImage(art, local.toFloat(), juce::RectanglePlacement::stretchToFit);
    else
        g.fillAll(kBackground);

    if (readings.mode == 0)
        drawLampOverlay(g, local, readings);
    else if (readings.mode == 1)
        drawNeedleOverlay(g, local, readings);
    else if (readings.mode == 2)
        drawSparkOverlay(g, local, readings);
    else
        drawTubeOverlay(g, local, readings);

    if (readings.learn)
        drawLearnMarks(g, local, readings.mode);
}

void SceneView::mouseDown(const juce::MouseEvent& event)
{
    const auto readings = readReadings(processor, phase);
    activeRole = hitRole(getLocalBounds(), event.position, readings);

    if (activeRole == 0 || activeRole == 5 || activeRole == 14 || activeRole == 18)
    {
        if (activeRole != 0)
            processor.notifyLockedControl(activeRole);

        activeRole = 0;
        return;
    }

    if (activeRole == 13 && ! readings.allEnabled)
    {
        processor.notifyLockedControl(activeRole);
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
    const auto readings = readReadings(processor, phase);
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
    const auto readings = readReadings(processor, phase);
    const int role = hitRole(getLocalBounds(), event.position, readings);

    if (role == 0)
        return;

    applyRole(role, wheel.deltaX * 40.0f, wheel.deltaY * 40.0f, false);
    end();
}

} // namespace squeeze::ui
