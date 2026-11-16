#pragma once

#include "PluginProcessor.h"

#include <string>

namespace squeeze::ui
{

class SceneView : public juce::Component
{
public:
    explicit SceneView(SqueezeProcessor&);
    ~SceneView() override;

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    void advance();
    std::string layoutProblems() const;
    void nudge(bool locked, float delta);
    void selectAllRatio();

private:
    SqueezeProcessor& processor;
    float phase = 0.0f;
    int activeRole = 0;
    float startNorm = 0.0f;
    juce::Point<float> startPos;
    juce::RangedAudioParameter* gesture = nullptr;

    void begin(juce::RangedAudioParameter*);
    void writeNorm(float);
    void writeDenorm(float);
    void end();
    void applyRole(int role, float dx, float dy, bool fromStart);
};

} // namespace squeeze::ui
