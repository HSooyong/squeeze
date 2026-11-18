#include "Ui/Widgets.h"

#include "Parameters.h"
#include "Ui/Strings.h"
#include "Ui/Theme.h"

namespace squeeze::ui
{

void Pill::setTexts(juce::String primaryText, juce::String secondaryText)
{
    primary = std::move(primaryText);
    secondary = std::move(secondaryText);
    repaint();
}

void Pill::setSelected(bool shouldSelect)
{
    if (selected == shouldSelect)
        return;

    selected = shouldSelect;
    repaint();
}

void Pill::setAccent(juce::Colour colour)
{
    accent = colour;
    repaint();
}

bool Pill::contentFits() const
{
    const auto first = fitLine(primary, 13.0f, 11.0f, getWidth(), 12);
    const auto second = fitLine(secondary, 9.0f, 8.0f, getWidth(), 12);
    const float used = secondary.isEmpty() ? first.size : first.size + second.size + 2.0f;
    return first.ok && second.ok && used + 6.0f <= static_cast<float>(getHeight());
}

void Pill::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    const float radius = bounds.getHeight() * 0.5f;
    g.setColour(selected ? accent : juce::Colours::transparentBlack);
    g.fillRoundedRectangle(bounds, radius);
    g.setColour(selected ? accent : kLine);
    g.drawRoundedRectangle(bounds, radius, selected ? 0.0f : 1.2f);

    const auto first = fitLine(primary, 13.0f, 11.0f, getWidth(), 12);
    const auto second = fitLine(secondary, 9.0f, 8.0f, getWidth(), 12);

    auto area = getLocalBounds().reduced(8, 0);

    if (selected)
    {
        g.setColour(juce::Colour(0xff1a140c));
        g.setFont(interfaceFont(first.size));
    }
    else
    {
        g.setColour(kText);
        g.setFont(interfaceFont(first.size));
    }

    if (secondary.isEmpty())
    {
        g.drawText(primary, area, juce::Justification::centred, false);
        return;
    }

    const int primaryWidth = juce::GlyphArrangement::getStringWidthInt(interfaceFont(first.size), primary);
    const int secondaryWidth = juce::GlyphArrangement::getStringWidthInt(interfaceFont(second.size), secondary);
    const int gap = 6;
    const int total = primaryWidth + gap + secondaryWidth;
    auto row = area.withSizeKeepingCentre(juce::jmin(area.getWidth(), total), area.getHeight());
    g.drawText(primary, row.removeFromLeft(primaryWidth), juce::Justification::centred, false);
    row.removeFromLeft(gap);
    g.setColour(selected ? juce::Colour(0xff1a140c).withAlpha(0.75f) : kMuted);
    g.setFont(interfaceFont(second.size));
    g.drawText(secondary, row, juce::Justification::centredLeft, false);
}

void Pill::mouseUp(const juce::MouseEvent& event)
{
    if (event.mouseWasClicked() && getLocalBounds().contains(event.getPosition()) && onClick != nullptr)
        onClick();
}

bool Logo::contentFits() const
{
    const auto first = fitLine(strings::wordmark(), 16.0f, 13.0f, getWidth(), 4);
    const auto second = fitLine(strings::brand(), 9.0f, 8.0f, getWidth(), 4);
    return first.ok && second.ok && first.size + second.size + 4.0f <= static_cast<float>(getHeight());
}

void Logo::paint(juce::Graphics& g)
{
    const auto word = strings::wordmark();
    const auto brand = strings::brand();
    const float wordSize = 15.0f;
    const float brandSize = 9.0f;
    const int wordWidth = juce::GlyphArrangement::getStringWidthInt(interfaceFont(wordSize), word);
    const int brandWidth = juce::GlyphArrangement::getStringWidthInt(interfaceFont(brandSize), brand);

    if (wordWidth + brandWidth + 10 < getWidth())
    {
        auto area = getLocalBounds();
        g.setColour(kText);
        g.setFont(interfaceFont(wordSize));
        g.drawText(word, area.removeFromLeft(wordWidth + 8), juce::Justification::centredLeft, false);
        g.setColour(kMuted);
        g.setFont(interfaceFont(brandSize));
        g.drawText(brand, area, juce::Justification::centredLeft, false);
        return;
    }

    const auto first = fitLine(word, 16.0f, 13.0f, getWidth(), 4);
    const auto second = fitLine(brand, 9.0f, 8.0f, getWidth(), 4);
    auto area = getLocalBounds();
    g.setColour(kText);
    g.setFont(interfaceFont(first.size));
    g.drawText(word, area.removeFromTop(juce::roundToInt(first.size + 2.0f)), juce::Justification::centredLeft, false);
    g.setColour(kMuted);
    g.setFont(interfaceFont(second.size));
    g.drawText(brand, area, juce::Justification::centredLeft, false);
}

void MenuButton::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(kLine);
    g.drawRoundedRectangle(bounds, bounds.getHeight() * 0.5f, 1.2f);
    g.setColour(kText);

    const auto centre = bounds.getCentre();

    for (int index = -1; index <= 1; ++index)
        g.fillEllipse(centre.x - 1.6f + static_cast<float>(index) * 6.0f, centre.y - 1.6f, 3.2f, 3.2f);
}

void MenuButton::mouseUp(const juce::MouseEvent& event)
{
    if (event.mouseWasClicked() && getLocalBounds().contains(event.getPosition()) && onClick != nullptr)
        onClick();
}

namespace
{

juce::String reductionText(float grDb)
{
    if (! std::isfinite(grDb) || grDb < 0.05f)
        return "0.0 dB";

    return "-" + juce::String(grDb, 1) + " dB";
}

juce::String valueText(const SqueezeProcessor& processor, const char* id)
{
    if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(processor.getApvts().getParameter(id)))
        return parameter->getText(parameter->getValue(), 16);

    return {};
}

float rawValue(const SqueezeProcessor& processor, const char* id)
{
    if (auto* raw = processor.getApvts().getRawParameterValue(id))
        return raw->load();

    return 0.0f;
}

} // namespace

GainReadout::GainReadout(SqueezeProcessor& processorIn)
    : processor(processorIn)
{
}

juce::String detailLine(const SqueezeProcessor& processor)
{
    const int mode = juce::jlimit(0, 3, juce::roundToInt(rawValue(processor, params::mode)));

    if (mode == 0)
        return "Peak Red. " + juce::String(juce::roundToInt(rawValue(processor, params::optoPeakRed)))
               + " · " + valueText(processor, params::optoLimit);

    if (mode == 1)
        return valueText(processor, params::vcaRatio) + " · " + valueText(processor, params::vcaThreshold);

    if (mode == 2)
        return "Input " + valueText(processor, params::fetInput) + " · Ratio " + valueText(processor, params::fetRatio);

    return "Threshold " + valueText(processor, params::vmuThreshold) + " · " + valueText(processor, params::vmuTime);
}

bool GainReadout::contentFits() const
{
    const auto title = fitLine(strings::gainTitle(), 12.0f, 10.0f, getWidth(), 4);
    const auto number = fitLine(reductionText(std::abs(processor.lastVisualFrame().grDb)), 42.0f, 22.0f, getWidth(), 4);
    const auto detail = fitLine(detailLine(processor), 13.0f, 10.0f, getWidth(), 4);
    const float used = title.size + number.size + detail.size + 18.0f;
    return title.ok && number.ok && detail.ok && used <= static_cast<float>(getHeight());
}

void GainReadout::paint(juce::Graphics& g)
{
    const auto title = fitLine(strings::gainTitle(), 12.0f, 10.0f, getWidth(), 4);
    const auto numberText = reductionText(std::abs(processor.lastVisualFrame().grDb));
    const auto number = fitLine(numberText, 42.0f, 22.0f, getWidth(), 4);
    const auto detail = detailLine(processor);
    const auto detailFit = fitLine(detail, 13.0f, 10.0f, getWidth(), 4);
    auto area = getLocalBounds();

    g.setColour(kMuted);
    g.setFont(interfaceFont(title.size));
    g.drawText(strings::gainTitle(), area.removeFromTop(juce::roundToInt(title.size + 4.0f)), juce::Justification::centredLeft, false);

    g.setColour(kText);
    g.setFont(interfaceFont(number.size));
    g.drawText(numberText, area.removeFromTop(juce::roundToInt(number.size + 2.0f)), juce::Justification::centredLeft, false);

    g.setColour(kMuted);
    g.setFont(interfaceFont(detailFit.size));
    g.drawText(detail, area.removeFromTop(juce::roundToInt(detailFit.size + 2.0f)), juce::Justification::centredLeft, false);
}

OutputSlider::OutputSlider(SqueezeProcessor& processorIn)
    : processor(processorIn)
{
}

OutputSlider::~OutputSlider()
{
    end();
}

OutputSlider::Spec OutputSlider::spec() const
{
    const int mode = juce::jlimit(0, 3, juce::roundToInt(rawValue(processor, params::mode)));

    if (mode == 0)
        return { params::optoGain, strings::gainSlider(), {} };

    const char* id = params::vcaOutput;

    if (mode == 2)
        id = params::fetOutput;
    else if (mode == 3)
        id = params::vmuOutput;

    return { id, strings::outputSlider(), {} };
}

juce::RangedAudioParameter* OutputSlider::parameter() const
{
    return dynamic_cast<juce::RangedAudioParameter*>(processor.getApvts().getParameter(spec().id));
}

void OutputSlider::begin(juce::RangedAudioParameter* next)
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

void OutputSlider::writeNorm(float norm)
{
    if (gesture != nullptr)
        gesture->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, norm));
}

void OutputSlider::end()
{
    if (gesture != nullptr)
    {
        gesture->endChangeGesture();
        processor.commitGesture();
    }

    gesture = nullptr;
}

juce::Rectangle<int> OutputSlider::titleArea() const
{
    return getLocalBounds().removeFromTop(36);
}

juce::Rectangle<int> OutputSlider::valueArea() const
{
    return getLocalBounds().removeFromBottom(32);
}

juce::Rectangle<int> OutputSlider::trackArea() const
{
    auto area = getLocalBounds();
    area.removeFromTop(40);
    area.removeFromBottom(36);
    return area.withSizeKeepingCentre(8, juce::jmax(40, area.getHeight()));
}

bool OutputSlider::contentFits() const
{
    const auto current = spec();
    const auto title = fitLine(current.primary, 12.0f, 10.0f, getWidth(), 2);
    const auto titleEn = fitLine(current.secondary, 8.0f, 8.0f, getWidth(), 2);
    const auto value = fitLine(valueText(processor, current.id), 12.0f, 9.0f, getWidth(), 2);
    const float titleUsed = title.size + (current.secondary.isEmpty() ? 0.0f : titleEn.size + 1.0f);
    return title.ok && (current.secondary.isEmpty() || titleEn.ok) && value.ok
           && titleUsed + 4.0f <= static_cast<float>(titleArea().getHeight())
           && value.size + 4.0f <= static_cast<float>(valueArea().getHeight());
}

void OutputSlider::nudge(float deltaPixels)
{
    auto* parameter = this->parameter();

    if (parameter == nullptr)
        return;

    begin(parameter);
    const float span = static_cast<float>(juce::jmax(1, trackArea().getHeight()));
    writeNorm(parameter->getValue() + deltaPixels / span);
    end();
}

void OutputSlider::paint(juce::Graphics& g)
{
    const int mode = juce::jlimit(0, 3, juce::roundToInt(rawValue(processor, params::mode)));
    const auto current = spec();
    const auto title = fitLine(current.primary, 12.0f, 10.0f, getWidth(), 2);
    const auto valueString = valueText(processor, current.id);
    const auto value = fitLine(valueString, 12.0f, 9.0f, getWidth(), 2);
    auto heading = titleArea();

    g.setColour(kMuted);
    g.setFont(interfaceFont(title.size));
    g.drawText(current.primary, heading, juce::Justification::centred, false);

    auto track = trackArea().toFloat();
    g.setColour(kTrack);
    g.fillRoundedRectangle(track, 4.0f);

    const float norm = parameter() != nullptr ? parameter()->getValue() : 0.0f;
    const float filled = track.getHeight() * juce::jlimit(0.0f, 1.0f, norm);
    auto fill = track;
    fill.setY(track.getBottom() - filled);
    fill.setHeight(filled);
    g.setColour(kModeColour[mode]);
    g.fillRoundedRectangle(fill, 4.0f);

    const float thumbY = track.getBottom() - filled;
    g.setColour(kText);
    g.fillEllipse(track.getCentreX() - 8.0f, thumbY - 8.0f, 16.0f, 16.0f);

    g.setColour(kText);
    g.setFont(interfaceFont(value.size));
    g.drawText(valueString, valueArea(), juce::Justification::centred, false);
}

void OutputSlider::mouseDown(const juce::MouseEvent& event)
{
    auto* parameter = this->parameter();

    if (parameter == nullptr)
        return;

    startNorm = parameter->getValue();
    startY = event.position.y;
    begin(parameter);
    const auto track = trackArea().toFloat();
    const float norm = 1.0f - (event.position.y - track.getY()) / juce::jmax(1.0f, track.getHeight());
    writeNorm(norm);
}

void OutputSlider::mouseDrag(const juce::MouseEvent& event)
{
    if (gesture == nullptr)
        return;

    const float scale = event.mods.isShiftDown() ? 0.2f : 1.0f;
    const float span = static_cast<float>(juce::jmax(1, trackArea().getHeight()));
    writeNorm(startNorm + (startY - event.position.y) * scale / span);
}

void OutputSlider::mouseUp(const juce::MouseEvent&)
{
    end();
}

void OutputSlider::mouseDoubleClick(const juce::MouseEvent&)
{
    auto* parameter = this->parameter();

    if (parameter == nullptr)
        return;

    begin(parameter);
    writeNorm(parameter->getDefaultValue());
    end();
}

void OutputSlider::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    nudge(wheel.deltaY * 40.0f);
}

} // namespace squeeze::ui
