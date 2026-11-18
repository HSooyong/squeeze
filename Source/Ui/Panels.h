#pragma once

#include "PluginProcessor.h"

#include <functional>
#include <vector>

namespace squeeze::ui
{

struct Trace
{
    std::vector<float> gr;
    std::vector<float> inDb;
    std::vector<float> outDb;

    void push(const VisualFrame& frame)
    {
        gr.push_back(std::abs(frame.grDb));
        inDb.push_back(frame.inDb);
        outDb.push_back(frame.outDb);

        if (gr.size() > 240)
        {
            gr.erase(gr.begin());
            inDb.erase(inDb.begin());
            outDb.erase(outDb.begin());
        }
    }
};

class LearnColumn : public juce::Component
{
public:
    explicit LearnColumn(SqueezeProcessor&);

    std::function<void()> onHear;
    std::function<void()> onUndo;
    std::function<void()> onWhy;

    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    SqueezeProcessor& processor;
    juce::Rectangle<int> hearBounds;
    juce::Rectangle<int> undoBounds;
    juce::Rectangle<int> whyBounds;
};

class LearnDock : public juce::Component
{
public:
    LearnDock(SqueezeProcessor&, const Trace&);

    void paint(juce::Graphics&) override;

private:
    SqueezeProcessor& processor;
    const Trace& trace;
};

class MeterDrawer : public juce::Component
{
public:
    MeterDrawer(SqueezeProcessor&, const Trace&);

    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    SqueezeProcessor& processor;
    const Trace& trace;
    std::array<juce::Rectangle<int>, 4> sceneBounds {};
    juce::Rectangle<int> undoBounds;
};

} // namespace squeeze::ui
