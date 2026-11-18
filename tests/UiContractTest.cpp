#include "Check.h"
#include "EditorLayout.h"
#include "Parameters.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <cmath>
#include <string>

namespace squeeze::test
{
namespace
{

void setRaw(SqueezeProcessor& processor, const char* id, float value)
{
    if (auto* parameter = processor.getApvts().getParameter(id))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

float raw(SqueezeProcessor& processor, const char* id)
{
    return processor.getApvts().getRawParameterValue(id)->load();
}

void fillSine(juce::AudioBuffer<float>& buffer, int index, float amplitude)
{
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const float value = amplitude * std::sin(0.13f * static_cast<float>(index * buffer.getNumSamples() + sample));
        buffer.setSample(0, sample, value);
        buffer.setSample(1, sample, value * 0.8f);
    }
}

int countPixels(const juce::Image& image, bool red)
{
    int count = 0;

    for (int y = 80; y < image.getHeight() - 40; y += 6)
    {
        for (int x = 220; x < image.getWidth() - 80; x += 6)
        {
            const auto pixel = image.getPixelAt(x, y);

            if (red)
            {
                if (pixel.getRed() > 220 && pixel.getGreen() < 40 && pixel.getBlue() < 40)
                    ++count;
            }
            else if (pixel.getRed() > 70 || pixel.getGreen() > 70 || pixel.getBlue() > 80)
            {
                ++count;
            }
        }
    }

    return count;
}

} // namespace

void runUiContract()
{
    SqueezeProcessor processor;
    setRaw(processor, params::mode, 1.0f);
    setRaw(processor, params::vcaThreshold, -30.0f);
    setRaw(processor, params::vcaRatio, 8.0f);
    processor.prepareToPlay(48000.0, 256);

    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;

    for (int index = 0; index < 180; ++index)
    {
        fillSine(buffer, index, 0.95f);
        processor.processBlock(buffer, midi);
    }

    EXPECT(std::abs(processor.currentGrDb()) > 0.5f);
    EXPECT(std::isfinite(processor.lastVisualFrame().grDb));
    EXPECT(processor.lastVisualFrame().mode == static_cast<int>(Mode::vca) || processor.lastVisualFrame().mode == 1);

    std::unique_ptr<SqueezeEditor> editor(dynamic_cast<SqueezeEditor*>(processor.createEditor()));
    EXPECT(editor != nullptr);
    editor->setSize(ui::kDefaultWidth, ui::kDesignHeight);

    for (int tick = 0; tick < 30; ++tick)
        editor->serviceTimerForTest();

    EXPECT(near(editor->displayedReductionDb(), std::abs(processor.currentGrDb()), 0.05f));
    EXPECT(! editor->learnShown());
    EXPECT(! editor->meterShown());

    editor->setDisclosure(true, true);
    EXPECT(editor->learnShown());
    EXPECT(editor->meterShown());
    EXPECT(processor.getUiState().learnOpen);
    EXPECT(processor.getUiState().meterOpen);

    editor->setSize(1400, ui::heightForWidth(1400));
    EXPECT(processor.getUiState().windowWidth == 1400);

    processor.selectAbSlot(1);
    setRaw(processor, params::vcaThreshold, -12.0f);
    EXPECT(processor.getUiState().abSlot == 1);
    processor.selectAbSlot(0);
    EXPECT(near(raw(processor, params::vcaThreshold), -30.0f, 0.05f));
    EXPECT(processor.getUiState().abSlot == 0);

    processor.applyScene(1, 2);
    EXPECT(processor.getUiState().selectedScene == "Bass even");
    EXPECT(near(raw(processor, params::vcaThreshold), -24.0f, 0.05f));
    EXPECT(! processor.getUiState().history.empty());
    EXPECT(processor.undoGesture());
    EXPECT(near(raw(processor, params::vcaThreshold), -30.0f, 0.05f));

    editor->setDisclosure(true, false);
    juce::MemoryBlock block;
    processor.getStateInformation(block);

    SqueezeProcessor restored;
    restored.setStateInformation(block.getData(), static_cast<int>(block.getSize()));
    EXPECT(restored.getUiState().learnOpen);
    EXPECT(! restored.getUiState().meterOpen);
    EXPECT(restored.getUiState().windowWidth == 1400);
    EXPECT(restored.getUiState().selectedScene == "Bass even");
    EXPECT(near(raw(restored, params::vcaThreshold), -30.0f, 0.05f));
    EXPECT(near(raw(restored, params::mode), 1.0f, 0.01f));

    std::unique_ptr<SqueezeEditor> restoredEditor(dynamic_cast<SqueezeEditor*>(restored.createEditor()));
    EXPECT(restoredEditor != nullptr);
    EXPECT(restoredEditor->learnShown());
    EXPECT(! restoredEditor->meterShown());
    EXPECT(restoredEditor->getWidth() == 1400);

    for (int mode = 0; mode < 4; ++mode)
    {
        SqueezeProcessor shot;
        setRaw(shot, params::mode, static_cast<float>(mode));
        setRaw(shot, params::optoPeakRed, 62.0f);
        setRaw(shot, params::vcaThreshold, -18.0f);
        setRaw(shot, params::vcaRatio, 4.0f);
        setRaw(shot, params::fetInput, 34.0f);
        setRaw(shot, params::fetRatio, 0.0f);
        setRaw(shot, params::vmuThreshold, -22.0f);
        setRaw(shot, params::vmuTime, 2.0f);
        shot.prepareToPlay(48000.0, 128);
        juce::AudioBuffer<float> tone(2, 128);

        for (int index = 0; index < 40; ++index)
        {
            fillSine(tone, index, 0.5f);
            shot.processBlock(tone, midi);
        }

        std::unique_ptr<juce::AudioProcessorEditor> view(shot.createEditor());
        view->setSize(ui::kDefaultWidth, ui::kDesignHeight);
        // Windows defaults to a Direct2D image, which does not read back in headless CI.
        juce::Image image(juce::Image::ARGB, ui::kDefaultWidth, ui::kDesignHeight, true, juce::SoftwareImageType{});
        juce::Graphics graphics(image);
        view->paintEntireComponent(graphics, true);
        EXPECT(countPixels(image, true) < 30);
        EXPECT(countPixels(image, false) > 80);
    }
}

} // namespace squeeze::test
