#pragma once

#include "Check.h"
#include "Parameters.h"
#include "PluginProcessor.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace squeeze::test
{

struct Point
{
    double time = 0.0;
    float gr = 0.0f;
};

class Session
{
public:
    void set(const char* id, float value)
    {
        auto* parameter = processor.getApvts().getParameter(id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    }

    void silenceCompression()
    {
        set(params::abLevelMatch, 0.0f);
        set(params::optoPeakRed, 0.0f);
        set(params::optoGain, 0.0f);
        set(params::optoLimit, 0.0f);
        set(params::vcaThreshold, 0.0f);
        set(params::vcaRatio, 1.0f);
        set(params::vcaOutput, 0.0f);
        set(params::fetInput, 24.0f);
        set(params::fetOutput, 0.0f);
        set(params::fetRatio, 0.0f);
        set(params::vmuInput, 0.0f);
        set(params::vmuThreshold, 0.0f);
        set(params::vmuOutput, 0.0f);
    }

    void prepare(double newSampleRate, int blockSize)
    {
        sampleRate = newSampleRate;
        processor.prepareToPlay(sampleRate, std::max(1, blockSize));
    }

    std::vector<float> render(const std::vector<float>& input, int blockSize)
    {
        std::vector<float> output(input.size(), 0.0f);
        std::size_t offset = 0;
        while (offset < input.size())
        {
            const int count = static_cast<int>(std::min<std::size_t>(static_cast<std::size_t>(std::max(1, blockSize)), input.size() - offset));
            juce::AudioBuffer<float> buffer(2, count);
            for (int index = 0; index < count; ++index)
            {
                const float sample = input[offset + static_cast<std::size_t>(index)];
                buffer.setSample(0, index, sample);
                buffer.setSample(1, index, sample);
            }

            juce::MidiBuffer midi;
            processor.processBlock(buffer, midi);
            for (int index = 0; index < count; ++index)
                output[offset + static_cast<std::size_t>(index)] = buffer.getSample(0, index);
            offset += static_cast<std::size_t>(count);
        }
        return output;
    }

    std::vector<Point> recordGr(const std::vector<float>& input, int blockSize)
    {
        std::vector<Point> points;
        points.reserve(input.size() / static_cast<std::size_t>(std::max(1, blockSize)) + 1);
        std::size_t offset = 0;
        while (offset < input.size())
        {
            const int count = static_cast<int>(std::min<std::size_t>(static_cast<std::size_t>(std::max(1, blockSize)), input.size() - offset));
            juce::AudioBuffer<float> buffer(2, count);
            for (int index = 0; index < count; ++index)
            {
                const float sample = input[offset + static_cast<std::size_t>(index)];
                buffer.setSample(0, index, sample);
                buffer.setSample(1, index, sample);
            }
            juce::MidiBuffer midi;
            processor.processBlock(buffer, midi);
            offset += static_cast<std::size_t>(count);
            points.push_back({ static_cast<double>(offset) / sampleRate, processor.currentGrDb() });
        }
        return points;
    }

    SqueezeProcessor processor;
    double sampleRate = 48000.0;
};

inline void report(const std::string& name, double value, const std::string& unit)
{
    std::cout << "MEASURE " << name << " " << value << " " << unit << "\n";
}

inline double crossingTime(const std::vector<Point>& points, float level, bool rising, double afterTime)
{
    for (std::size_t index = 1; index < points.size(); ++index)
    {
        if (points[index].time < afterTime)
            continue;

        const float previous = points[index - 1].gr;
        const float current = points[index].gr;
        const bool crossed = rising ? (previous < level && current >= level) : (previous > level && current <= level);
        if (!crossed || std::fabs(current - previous) < 1.0e-8f)
            continue;

        const float fraction = (level - previous) / (current - previous);
        return points[index - 1].time + static_cast<double>(fraction) * (points[index].time - points[index - 1].time);
    }
    return -1.0;
}

inline std::vector<float> tone(double sampleRate, double seconds, double frequency, float amplitude, double startAt = 0.0)
{
    const int count = static_cast<int>(std::llround(sampleRate * seconds));
    std::vector<float> samples(static_cast<std::size_t>(count), 0.0f);
    const int start = static_cast<int>(std::llround(sampleRate * startAt));
    for (int index = start; index < count; ++index)
    {
        const double time = static_cast<double>(index) / sampleRate;
        samples[static_cast<std::size_t>(index)] = amplitude * static_cast<float>(std::sin(2.0 * 3.14159265358979323846 * frequency * time));
    }
    return samples;
}

inline std::vector<float> step(double sampleRate, double seconds, float amplitude, double startAt)
{
    const int count = static_cast<int>(std::llround(sampleRate * seconds));
    std::vector<float> samples(static_cast<std::size_t>(count), 0.0f);
    const int start = static_cast<int>(std::llround(sampleRate * startAt));
    for (int index = std::max(0, start); index < count; ++index)
        samples[static_cast<std::size_t>(index)] = amplitude;
    return samples;
}

inline double rms(const std::vector<float>& samples, int begin, int end)
{
    double energy = 0.0;
    const int first = std::max(0, begin);
    const int last = std::min(end, static_cast<int>(samples.size()));
    for (int index = first; index < last; ++index)
        energy += static_cast<double>(samples[static_cast<std::size_t>(index)]) * static_cast<double>(samples[static_cast<std::size_t>(index)]);
    const int count = std::max(1, last - first);
    return std::sqrt(energy / static_cast<double>(count));
}

inline float amplitudeForDb(float db) noexcept
{
    return std::pow(10.0f, db / 20.0f);
}

} // namespace squeeze::test
