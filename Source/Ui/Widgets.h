#pragma once

#include "PluginProcessor.h"

#include <functional>

namespace squeeze::ui
{

class Pill : public juce::Component
{
public:
    std::function<void()> onClick;

    void setTexts(juce::String primaryText, juce::String secondaryText);
    void setSelected(bool shouldSelect);
    void setAccent(juce::Colour colour);
    bool contentFits() const;

    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    juce::String primary;
    juce::String secondary;
    bool selected = false;
    juce::Colour accent { 0xfff4f1ea };
};

class Logo : public juce::Component
{
public:
    bool contentFits() const;
    void paint(juce::Graphics&) override;
};

class MenuButton : public juce::Component
{
public:
    std::function<void()> onClick;
    bool contentFits() const { return getWidth() >= 28 && getHeight() >= 28; }
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;
};

class GainReadout : public juce::Component
{
public:
    explicit GainReadout(SqueezeProcessor&);
    bool contentFits() const;
    void paint(juce::Graphics&) override;

private:
    SqueezeProcessor& processor;
};

class OutputSlider : public juce::Component
{
public:
    explicit OutputSlider(SqueezeProcessor&);
    ~OutputSlider() override;

    bool contentFits() const;
    void nudge(float deltaPixels);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    struct Spec
    {
        const char* id = nullptr;
        juce::String primary;
        juce::String secondary;
    };

    Spec spec() const;
    juce::RangedAudioParameter* parameter() const;
    void begin(juce::RangedAudioParameter*);
    void writeNorm(float norm);
    void end();
    juce::Rectangle<int> titleArea() const;
    juce::Rectangle<int> valueArea() const;
    juce::Rectangle<int> trackArea() const;

    SqueezeProcessor& processor;
    juce::RangedAudioParameter* gesture = nullptr;
    float startNorm = 0.0f;
    float startY = 0.0f;
};

} // namespace squeeze::ui
