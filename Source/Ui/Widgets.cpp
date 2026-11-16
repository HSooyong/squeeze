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
    g.setColour(selected ? accent.withAlpha(0.18f) : juce::Colours::transparentBlack);
    g.fillRoundedRectangle(bounds, radius);
    g.setColour(selected ? accent : kLine);
    g.drawRoundedRectangle(bounds, radius, 1.2f);

    const auto first = fitLine(primary, 13.0f, 11.0f, getWidth(), 12);
    const auto second = fitLine(secondary, 9.0f, 8.0f, getWidth(), 12);

    if (secondary.isEmpty())
    {
        g.setColour(selected ? kText : kMuted);
        g.setFont(interfaceFont(first.size));
        g.drawText(primary, getLocalBounds(), juce::Justification::centred, false);
        return;
    }

    auto area = getLocalBounds();
    const int primaryHeight = juce::roundToInt(first.size + 1.0f);
    auto top = area.removeFromTop(primaryHeight + 2);
    g.setColour(kText);
    g.setFont(interfaceFont(first.size));
    g.drawText(primary, top, juce::Justification::centred, false);
    g.setColour(selected ? accent : kMuted);
    g.setFont(interfaceFont(second.size));
    g.drawText(secondary, area, juce::Justification::centred, false);
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
    const auto first = fitLine(strings::wordmark(), 16.0f, 13.0f, getWidth(), 4);
    const auto second = fitLine(strings::brand(), 9.0f, 8.0f, getWidth(), 4);
    auto area = getLocalBounds();
    g.setColour(kText);
    g.setFont(interfaceFont(first.size));
    g.drawText(strings::wordmark(), area.removeFromTop(juce::roundToInt(first.size + 2.0f)), juce::Justification::centredLeft, false);
    g.setColour(kMuted);
    g.setFont(interfaceFont(second.size));
    g.drawText(strings::brand(), area, juce::Justification::centredLeft, false);
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

juce::String valueText(SqueezeProcessor& processor, const char* id)
{
    if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(processor.getApvts().getParameter(id)))
        return parameter->getText(parameter->getValue(), 16);

    return {};
}

float rawValue(SqueezeProcessor& processor, const char* id)
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

bool GainReadout::contentFits() const
{
    const auto title = fitLine(strings::gainTitle(), 13.0f, 11.0f, getWidth(), 4);
    const auto titleEn = fitLine(strings::gainTitleEn(), 9.0f, 8.0f, getWidth(), 4);
    const auto number = fitLine(reductionText(std::abs(processor.lastVisualFrame().grDb)), 40.0f, 22.0f, getWidth(), 4);
    const int mode = juce::jlimit(0, 3, juce::roundToInt(rawValue(processor, params::mode)));
    juce::String detail;

    if (mode == 0)
        detail = strings::peakWord() + " " + juce::String(juce::roundToInt(rawValue(processor, params::optoPeakRed)))
                 + " · " + (rawValue(processor, params::optoLimit) >= 0.5f ? strings::limit() : strings::compress());
    else if (mode == 1)
        detail = valueText(processor, params::vcaRatio) + " · " + valueText(processor, params::vcaThreshold);
    else if (mode == 2)
        detail = valueText(processor, params::fetInput) + " · " + valueText(processor, params::fetRatio);
    else
        detail = valueText(processor, params::vmuTime) + " · " + valueText(processor, params::vmuThreshold);

    const auto detailFit = fitLine(detail, 12.0f, 10.0f, getWidth(), 4);
    const float used = title.size + titleEn.size + number.size + detailFit.size + 16.0f;
    return title.ok && titleEn.ok && number.ok && detailFit.ok && used <= static_cast<float>(getHeight());
}

void GainReadout::paint(juce::Graphics& g)
{
    const int mode = juce::jlimit(0, 3, juce::roundToInt(rawValue(processor, params::mode)));
    const auto title = fitLine(strings::gainTitle(), 13.0f, 11.0f, getWidth(), 4);
    const auto titleEn = fitLine(strings::gainTitleEn(), 9.0f, 8.0f, getWidth(), 4);
    const auto numberText = reductionText(std::abs(processor.lastVisualFrame().grDb));
    const auto number = fitLine(numberText, 40.0f, 22.0f, getWidth(), 4);
    juce::String detail;

    if (mode == 0)
        detail = strings::peakWord() + " " + juce::String(juce::roundToInt(rawValue(processor, params::optoPeakRed)))
                 + " · " + (rawValue(processor, params::optoLimit) >= 0.5f ? strings::limit() : strings::compress());
    else if (mode == 1)
        detail = valueText(processor, params::vcaRatio) + " · " + valueText(processor, params::vcaThreshold);
    else if (mode == 2)
        detail = valueText(processor, params::fetInput) + " · " + valueText(processor, params::fetRatio);
    else
        detail = valueText(processor, params::vmuTime) + " · " + valueText(processor, params::vmuThreshold);

    const auto detailFit = fitLine(detail, 12.0f, 10.0f, getWidth(), 4);
    auto area = getLocalBounds();

    g.setColour(kMuted);
    g.setFont(interfaceFont(title.size));
    g.drawText(strings::gainTitle(), area.removeFromTop(juce::roundToInt(title.size + 2.0f)), juce::Justification::centredLeft, false);
    g.setFont(interfaceFont(titleEn.size));
    g.drawText(strings::gainTitleEn(), area.removeFromTop(juce::roundToInt(titleEn.size + 4.0f)), juce::Justification::centredLeft, false);

    g.setColour(kModeColour[mode]);
    g.setFont(interfaceFont(number.size));
    g.drawText(numberText, area.removeFromTop(juce::roundToInt(number.size + 4.0f)), juce::Justification::centredLeft, false);

    g.setColour(kText);
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
        return { params::optoGain, strings::gainSlider(), strings::gainSliderEn() };

    const char* id = params::vcaOutput;

    if (mode == 2)
        id = params::fetOutput;
    else if (mode == 3)
        id = params::vmuOutput;

    return { id, strings::outputSlider(), strings::outputSliderEn() };
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
        gesture->beginChangeGesture();
}

void OutputSlider::writeNorm(float norm)
{
    if (gesture != nullptr)
        gesture->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, norm));
}

void OutputSlider::end()
{
    if (gesture != nullptr)
        gesture->endChangeGesture();

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
    const auto value = fitLine(valueText(processor, current.id), 11.0f, 9.0f, getWidth(), 2);
    return title.ok && titleEn.ok && value.ok
           && title.size + titleEn.size + 4.0f <= static_cast<float>(titleArea().getHeight())
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
    const auto titleEn = fitLine(current.secondary, 8.0f, 8.0f, getWidth(), 2);
    const auto valueString = valueText(processor, current.id);
    const auto value = fitLine(valueString, 11.0f, 9.0f, getWidth(), 2);
    auto heading = titleArea();

    g.setColour(kText);
    g.setFont(interfaceFont(title.size));
    g.drawText(current.primary, heading.removeFromTop(juce::roundToInt(title.size + 1.0f)), juce::Justification::centred, false);
    g.setColour(kMuted);
    g.setFont(interfaceFont(titleEn.size));
    g.drawText(current.secondary, heading, juce::Justification::centred, false);

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
