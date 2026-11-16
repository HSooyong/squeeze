#include "PluginEditor.h"

#include "EditorLayout.h"
#include "Parameters.h"

#include <algorithm>

namespace squeeze
{

SqueezeEditor::SqueezeEditor(SqueezeProcessor& processorIn)
    : juce::AudioProcessorEditor(processorIn),
      processor(processorIn)
{
    const auto width = std::clamp(processor.getUiState().windowWidth, ui::kMinWidth, ui::kMaxWidth);

    setOpaque(true);
    setName("SQUEEZE");

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

    processor.getApvts().addParameterListener(params::mode, this);
}

SqueezeEditor::~SqueezeEditor()
{
    processor.getApvts().removeParameterListener(params::mode, this);
    cancelPendingUpdate();
}

void SqueezeEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(20, 20, 22));

    auto bounds = getLocalBounds().reduced(24);

    g.setColour(juce::Colours::white);
    g.setFont(28.0f);
    g.drawText("SQUEEZE", bounds.removeFromTop(40), juce::Justification::centredLeft, false);

    g.setColour(juce::Colour::fromRGB(196, 196, 196));
    g.setFont(14.0f);
    g.drawText("Bearfield", bounds.removeFromTop(22), juce::Justification::centredLeft, false);

    auto modeName = juce::String("LAMP");

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(processor.getApvts().getParameter(params::mode)))
        modeName = choice->getCurrentChoiceName();

    g.setColour(juce::Colour::fromRGB(232, 184, 74));
    g.drawText(modeName, bounds.removeFromTop(24), juce::Justification::centredLeft, false);
}

void SqueezeEditor::resized()
{
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

void SqueezeEditor::parameterChanged(const juce::String&, float)
{
    triggerAsyncUpdate();
}

void SqueezeEditor::handleAsyncUpdate()
{
    repaint();
}

} // namespace squeeze
