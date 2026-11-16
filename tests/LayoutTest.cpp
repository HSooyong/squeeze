#include "Check.h"
#include "EditorLayout.h"
#include "Parameters.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <iostream>
#include <string>
#include <vector>

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

std::vector<float> capture(SqueezeProcessor& processor)
{
    std::vector<float> values;

    for (auto* parameter : processor.getParameters())
        values.push_back(parameter->getValue());

    return values;
}

void fillSine(juce::AudioBuffer<float>& buffer, double sampleRate, int index, float amplitude, float hz)
{
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const float value = amplitude * std::sin(juce::MathConstants<float>::twoPi * hz * static_cast<float>(index * buffer.getNumSamples() + sample) / static_cast<float>(sampleRate));
        buffer.setSample(0, sample, value);
        buffer.setSample(1, sample, value);
    }
}

void expectLayout(SqueezeEditor& editor, const std::string& label)
{
    const auto report = editor.layoutReport();

    if (! report.empty())
        std::cerr << label << "\n" << report;

    EXPECT(report.empty());
}

} // namespace

void runLayout()
{
    SqueezeProcessor processor;
    std::unique_ptr<SqueezeEditor> editor(dynamic_cast<SqueezeEditor*>(processor.createEditor()));
    EXPECT(editor != nullptr);

    editor->resetPaintRequestCount();

    for (int tick = 0; tick < 8; ++tick)
        editor->serviceTimerForTest();

    EXPECT(editor->paintRequestCount() == 0);

    for (int width = ui::kMinWidth; width <= ui::kMaxWidth; width += 10)
    {
        editor->setSize(width, ui::heightForWidth(width));

        for (int mode = 0; mode < 4; ++mode)
        {
            setRaw(processor, params::mode, static_cast<float>(mode));
            setRaw(processor, params::optoPeakRed, mode == 0 ? 0.0f : 100.0f);
            setRaw(processor, params::vcaRatio, 1000.0f);
            setRaw(processor, params::vcaThreshold, -40.0f);
            setRaw(processor, params::fetAttack, 800.0f);
            setRaw(processor, params::fetRelease, 1100.0f);
            setRaw(processor, params::fetInput, 48.0f);
            setRaw(processor, params::vmuTime, 5.0f);
            setRaw(processor, params::vmuThreshold, 0.0f);
            setRaw(processor, params::vmuInput, 20.0f);
            expectLayout(*editor, "w=" + std::to_string(width) + " mode=" + std::to_string(mode));
        }
    }

    setRaw(processor, params::mode, 0.0f);
    setRaw(processor, params::optoPeakRed, 40.0f);
    const auto beforeLamp = capture(processor);
    editor->nudgeScene(true, 80.0f);
    EXPECT(capture(processor) == beforeLamp);
    editor->nudgeScene(false, 80.0f);
    EXPECT(capture(processor) != beforeLamp);

    setRaw(processor, params::mode, 1.0f);
    setRaw(processor, params::vcaRatio, 4.0f);
    const float ratioBefore = raw(processor, params::vcaRatio);
    editor->nudgeScene(true, -80.0f);
    EXPECT(near(raw(processor, params::vcaRatio), ratioBefore, 0.001f));
    editor->nudgeScene(false, -80.0f);
    EXPECT(! near(raw(processor, params::vcaRatio), ratioBefore, 0.001f));

    setRaw(processor, params::mode, 2.0f);
    setRaw(processor, params::fetAllEnabled, 0.0f);
    setRaw(processor, params::fetRatio, 0.0f);
    editor->selectAllRatio();
    EXPECT(near(raw(processor, params::fetRatio), 0.0f, 0.001f));
    setRaw(processor, params::fetAllEnabled, 1.0f);
    editor->selectAllRatio();
    EXPECT(near(raw(processor, params::fetRatio), 4.0f, 0.001f));

    const auto outputBefore = raw(processor, params::fetOutput);
    editor->nudgeOutput(40.0f);
    EXPECT(! near(raw(processor, params::fetOutput), outputBefore, 0.001f));

    setRaw(processor, params::mode, 3.0f);
    setRaw(processor, params::vmuInput, 0.0f);
    const float timeBefore = raw(processor, params::vmuTime);
    const float thresholdBefore = raw(processor, params::vmuThreshold);
    const float inputBefore = raw(processor, params::vmuInput);
    editor->nudgeScene(true, 60.0f);
    EXPECT(near(raw(processor, params::vmuThreshold), thresholdBefore, 0.001f));
    EXPECT(near(raw(processor, params::vmuTime), timeBefore, 0.001f));
    EXPECT(near(raw(processor, params::vmuInput), inputBefore, 0.001f));
    editor->nudgeScene(false, 60.0f);
    EXPECT(! near(raw(processor, params::vmuInput), inputBefore, 0.001f));
}

void runSnapshot(const std::string& directory)
{
    const juce::File folder(directory);
    folder.createDirectory();

    struct Shot
    {
        const char* name;
        int mode;
        float amplitude;
    };

    const Shot shots[] = {
        { "squeeze-m3-lamp.png", 0, 0.45f },
        { "squeeze-m3-needle.png", 1, 0.50f },
        { "squeeze-m3-spark.png", 2, 0.55f },
        { "squeeze-m3-tube.png", 3, 0.60f },
    };

    for (const auto& shot : shots)
    {
        SqueezeProcessor processor;
        setRaw(processor, params::mode, static_cast<float>(shot.mode));
        setRaw(processor, params::optoPeakRed, 62.0f);
        setRaw(processor, params::optoGain, 5.0f);
        setRaw(processor, params::vcaThreshold, -18.0f);
        setRaw(processor, params::vcaRatio, 4.0f);
        setRaw(processor, params::vcaOutput, 6.0f);
        setRaw(processor, params::fetInput, 34.0f);
        setRaw(processor, params::fetOutput, 3.0f);
        setRaw(processor, params::fetAttack, 300.0f);
        setRaw(processor, params::fetRelease, 250.0f);
        setRaw(processor, params::fetRatio, shot.mode == 2 ? 2.0f : 0.0f);
        setRaw(processor, params::vmuThreshold, -22.0f);
        setRaw(processor, params::vmuTime, 2.0f);
        setRaw(processor, params::vmuOutput, 3.5f);
        processor.prepareToPlay(48000.0, 256);

        juce::AudioBuffer<float> buffer(2, 256);
        juce::MidiBuffer midi;

        for (int index = 0; index < 140; ++index)
        {
            fillSine(buffer, 48000.0, index, shot.amplitude, 220.0f);
            processor.processBlock(buffer, midi);
        }

        std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
        editor->setSize(ui::kDefaultWidth, ui::kDesignHeight);
        juce::Image image(juce::Image::ARGB, ui::kDefaultWidth, ui::kDesignHeight, true);
        juce::Graphics graphics(image);
        editor->paintEntireComponent(graphics, true);

        juce::FileOutputStream stream(folder.getChildFile(shot.name));
        EXPECT(stream.openedOk());
        juce::PNGImageFormat format;
        EXPECT(format.writeImageToStream(image, stream));
        std::cout << "snapshot " << folder.getChildFile(shot.name).getFullPathName() << "\n";
    }
}

} // namespace squeeze::test
