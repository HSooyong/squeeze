#include "DspSupport.h"
#include "LevelMatch.h"
#include "StaticCurve.h"
#include "Tunables.h"

#include <chrono>
#include <cmath>
#include <random>

namespace squeeze::test
{
namespace
{

float settledGr(const std::vector<Point>& points, double from, double to)
{
    double sum = 0.0;
    int count = 0;
    for (const auto& point : points)
    {
        if (point.time < from || point.time > to)
            continue;
        sum += point.gr;
        ++count;
    }
    return count > 0 ? static_cast<float>(sum / count) : 0.0f;
}

void expectRange(const std::string& name, double value, double minimum, double maximum, const std::string& unit)
{
    report(name, value, unit);
    const bool inside = value >= minimum && value <= maximum;
    if (!inside)
        std::cerr << name << " = " << value << " " << unit << " outside " << minimum << ".." << maximum << "\n";
    expect(inside, name, "tests/DspTests.cpp", 0);
}

} // namespace

void runStaticCurve()
{
    const float vcaAtTenOver = StaticCurve::vcaGainDb(-8.0f, -18.0f, 4.0f);
    EXPECT(near(vcaAtTenOver, -7.5f, 1.0e-4f));
    EXPECT(near(StaticCurve::vcaGainDb(-30.0f, -18.0f, 4.0f), 0.0f, 1.0e-4f));
    EXPECT(near(StaticCurve::variMuRatio(-10.0f, -20.0f), std::min(6.0f, 1.4f + 0.28f * 10.0f), 1.0e-4f));

    const double sampleRate = 48000.0;
    const double renderSeconds = 0.45;
    const int block = 256;

    auto sweep = [&](const std::string& label, auto configure, auto expectedGain) {
        float worst = 0.0f;
        for (int level = -60; level <= 0; level += 10)
        {
            Session session;
            session.set(params::abLevelMatch, 0.0f);
            configure(session);
            session.prepare(sampleRate, block);
            const float amplitude = amplitudeForDb(static_cast<float>(level));
            const auto input = tone(sampleRate, renderSeconds, 1000.0, amplitude);
            const auto output = session.render(input, block);
            const int end = static_cast<int>(output.size());
            const int begin = end - static_cast<int>(sampleRate * 0.08);
            const double inRms = rms(input, begin, end);
            const double outRms = rms(output, begin, end);
            const float measured = inRms > 1.0e-9 && outRms > 1.0e-12
                                       ? static_cast<float>(20.0 * std::log10(outRms / inRms))
                                       : 0.0f;
            const float expected = expectedGain(static_cast<float>(level));
            const float error = std::fabs(measured - expected);
            worst = std::max(worst, error);
            EXPECT(error <= 0.5f);
        }
        report(label + " worst", worst, "dB");
    };

    sweep("opto",
          [](Session& session) { session.set(params::optoPeakRed, 100.0f); session.set(params::optoGain, 0.0f); },
          [](float peakDb) {
              const float detector = peakDb + 20.0f * std::log10(2.0f / 3.14159265f) + tunables::optoSidechainDb(100.0f);
              return StaticCurve::optoGainDb(detector, 0.0f);
          });

    sweep("vca",
          [](Session& session) {
              session.set(params::mode, 1.0f);
              session.set(params::vcaThreshold, -18.0f);
              session.set(params::vcaRatio, 4.0f);
              session.set(params::vcaOutput, 0.0f);
          },
          [](float peakDb) {
              const float detector = peakDb - 10.0f * std::log10(2.0f);
              return StaticCurve::vcaGainDb(detector, -18.0f, 4.0f);
          });

    sweep("fet",
          [](Session& session) {
              session.set(params::mode, 2.0f);
              session.set(params::fetInput, 24.0f);
              session.set(params::fetOutput, 0.0f);
              session.set(params::fetRatio, 1.0f);
              session.set(params::fetAttack, 300.0f);
              session.set(params::fetRelease, 100.0f);
          },
          [](float peakDb) { return StaticCurve::fetGainDb(peakDb, 8.0f, tunables::fetKneeDb[1]); });

    sweep("varimu",
          [](Session& session) {
              session.set(params::mode, 3.0f);
              session.set(params::vmuInput, 0.0f);
              session.set(params::vmuThreshold, -22.0f);
              session.set(params::vmuOutput, 0.0f);
              session.set(params::vmuTime, 0.0f);
          },
          [](float peakDb) { return StaticCurve::variMuGainDb(peakDb, -22.0f); });
}

void runTimingOpto()
{
    const double sampleRate = 48000.0;
    Session session;
    session.set(params::abLevelMatch, 0.0f);
    session.set(params::optoPeakRed, 100.0f);
    session.set(params::optoGain, 0.0f);
    session.set(params::optoLimit, 0.0f);
    session.prepare(sampleRate, 1);

    const float amplitude = amplitudeForDb(-10.0f);
    auto attackInput = step(sampleRate, 0.25, amplitude, 0.02);
    auto attack = session.recordGr(attackInput, 1);
    const float finalGr = settledGr(attack, 0.18, 0.24);
    const double t63 = crossingTime(attack, 0.63f * finalGr, true, 0.02);
    expectRange("opto attack", (t63 - 0.02) * 1000.0, 8.0, 12.0, "ms");

    auto releaseInput = step(sampleRate, 0.45, amplitude, 0.0);
    for (std::size_t index = static_cast<std::size_t>(sampleRate * 0.20); index < releaseInput.size(); ++index)
        releaseInput[index] = 0.0f;
    session.prepare(sampleRate, 1);
    auto release = session.recordGr(releaseInput, 1);
    const float held = settledGr(release, 0.12, 0.19);
    const double t50 = crossingTime(release, 0.5f * held, false, 0.20);
    expectRange("opto fast release 50%", (t50 - 0.20) * 1000.0, 40.0, 120.0, "ms");

    auto tail = [&](double holdSeconds) {
        Session holdSession;
        holdSession.set(params::abLevelMatch, 0.0f);
        holdSession.set(params::optoPeakRed, 100.0f);
        holdSession.set(params::optoGain, 0.0f);
        holdSession.prepare(sampleRate, 256);
        const double total = holdSeconds + 8.0;
        auto input = step(sampleRate, total, amplitude, 0.0);
        for (std::size_t index = static_cast<std::size_t>(sampleRate * holdSeconds); index < input.size(); ++index)
            input[index] = 0.0f;
        auto series = holdSession.recordGr(input, 256);
        const float heldGr = settledGr(series, holdSeconds * 0.7, holdSeconds - 0.05);
        const double from = crossingTime(series, 0.40f * heldGr, false, holdSeconds);
        const double to = crossingTime(series, 0.15f * heldGr, false, holdSeconds);
        return to - from;
    };

    const double shortTail = tail(0.5);
    const double longTail = tail(10.0);
    report("opto slow 0.5s", shortTail * 1000.0, "ms");
    report("opto slow 10s", longTail * 1000.0, "ms");
    EXPECT(longTail > shortTail * 2.0);
}

void runTimingVca()
{
    const double sampleRate = 48000.0;
    for (int over : { 10, 20, 30 })
    {
        Session session;
        session.set(params::abLevelMatch, 0.0f);
        session.set(params::mode, 1.0f);
        session.set(params::vcaThreshold, -18.0f);
        session.set(params::vcaRatio, params::kRatioInfinity);
        session.set(params::vcaOutput, 0.0f);
        session.prepare(sampleRate, 1);
        const float amplitude = amplitudeForDb(-18.0f + static_cast<float>(over));
        auto input = step(sampleRate, 0.08, amplitude, 0.005);
        auto series = session.recordGr(input, 1);
        const float finalGr = settledGr(series, 0.05, 0.075);
        const double crossed = crossingTime(series, 0.9f * finalGr, true, 0.005);
        const double targetMs = over == 10 ? 15.0 : over == 20 ? 5.0 : 3.0;
        expectRange("vca attack " + std::to_string(over), (crossed - 0.005) * 1000.0, targetMs * 0.75, targetMs * 1.25, "ms");
    }

    Session release;
    release.set(params::abLevelMatch, 0.0f);
    release.set(params::mode, 1.0f);
    release.set(params::vcaThreshold, -18.0f);
    release.set(params::vcaRatio, params::kRatioInfinity);
    release.set(params::vcaOutput, 0.0f);
    release.prepare(sampleRate, 1);
    auto input = step(sampleRate, 0.25, amplitudeForDb(0.0f), 0.0);
    for (std::size_t index = static_cast<std::size_t>(sampleRate * 0.08); index < input.size(); ++index)
        input[index] = 0.0f;
    auto series = release.recordGr(input, 1);
    const float held = settledGr(series, 0.04, 0.07);
    const double high = crossingTime(series, 0.8f * held, false, 0.08);
    const double low = crossingTime(series, 0.2f * held, false, 0.08);
    const double slope = (0.6 * held) / (low - high);
    expectRange("vca release", slope, 108.0, 132.0, "dB/s");

    Session otherRate;
    otherRate.set(params::abLevelMatch, 0.0f);
    otherRate.set(params::mode, 1.0f);
    otherRate.set(params::vcaThreshold, -18.0f);
    otherRate.set(params::vcaRatio, params::kRatioInfinity);
    otherRate.prepare(192000.0, 1);
    auto fast = otherRate.recordGr(step(192000.0, 0.05, amplitudeForDb(-8.0f), 0.005), 1);
    const float fastFinal = settledGr(fast, 0.03, 0.045);
    const double fastCross = crossingTime(fast, 0.9f * fastFinal, true, 0.005);
    expectRange("vca attack 10dB 192k", (fastCross - 0.005) * 1000.0, 11.25, 18.75, "ms");
}

void runTimingFet()
{
    const float attacks[] = { 20.0f, 300.0f, 800.0f };
    const float releases[] = { 50.0f, 250.0f, 1100.0f };
    const double rates[] = { 96000.0, 192000.0 };

    for (double sampleRate : rates)
    {
        for (float attackUs : attacks)
        {
            Session session;
            session.set(params::abLevelMatch, 0.0f);
            session.set(params::mode, 2.0f);
            session.set(params::fetInput, 24.0f);
            session.set(params::fetOutput, 0.0f);
            session.set(params::fetRatio, 3.0f);
            session.set(params::fetAttack, attackUs);
            session.set(params::fetRelease, 100.0f);
            session.prepare(sampleRate, 1);
            auto input = step(sampleRate, 0.02, amplitudeForDb(2.0f), 0.002);
            auto series = session.recordGr(input, 1);
            const float finalGr = settledGr(series, 0.012, 0.018);
            const double crossed = crossingTime(series, 0.63f * finalGr, true, 0.002);
            const double measuredUs = (crossed - 0.002) * 1.0e6;
            expectRange("fet attack " + std::to_string(static_cast<int>(attackUs)) + " " + std::to_string(static_cast<int>(sampleRate)),
                        measuredUs,
                        attackUs * 0.85,
                        attackUs * 1.15,
                        "us");
        }

        for (float releaseMs : releases)
        {
            Session session;
            session.set(params::abLevelMatch, 0.0f);
            session.set(params::mode, 2.0f);
            session.set(params::fetInput, 24.0f);
            session.set(params::fetOutput, 0.0f);
            session.set(params::fetRatio, 3.0f);
            session.set(params::fetAttack, 20.0f);
            session.set(params::fetRelease, releaseMs);
            session.prepare(sampleRate, 64);
            const double total = 0.05 + releaseMs / 1000.0 * 4.0;
            auto input = step(sampleRate, total, amplitudeForDb(2.0f), 0.0);
            for (std::size_t index = static_cast<std::size_t>(sampleRate * 0.04); index < input.size(); ++index)
                input[index] = 0.0f;
            auto series = session.recordGr(input, 64);
            const float held = settledGr(series, 0.02, 0.035);
            const double crossed = crossingTime(series, held * (1.0f - 0.63f), false, 0.04);
            const double measuredMs = (crossed - 0.04) * 1000.0;
            expectRange("fet release " + std::to_string(static_cast<int>(releaseMs)) + " " + std::to_string(static_cast<int>(sampleRate)),
                        measuredMs,
                        releaseMs * 0.85,
                        releaseMs * 1.15,
                        "ms");
        }
    }
}

void runTimingVariMu()
{
    const double sampleRate = 96000.0;
    const char* names[] = { "T1", "T2", "T3", "T4", "T5", "T6" };
    for (int stepIndex = 0; stepIndex < 6; ++stepIndex)
    {
        const auto timing = tunables::vmuTiming[stepIndex];
        Session session;
        session.set(params::abLevelMatch, 0.0f);
        session.set(params::mode, 3.0f);
        session.set(params::vmuInput, 0.0f);
        session.set(params::vmuThreshold, -22.0f);
        session.set(params::vmuOutput, 0.0f);
        session.set(params::vmuTime, static_cast<float>(stepIndex));
        session.prepare(sampleRate, 1);
        auto input = step(sampleRate, 0.01, amplitudeForDb(-2.0f), 0.001);
        auto series = session.recordGr(input, 1);
        const float finalGr = settledGr(series, 0.006, 0.009);
        const double crossed = crossingTime(series, 0.63f * finalGr, true, 0.001);
        const double measuredUs = (crossed - 0.001) * 1.0e6;
        expectRange(std::string("varimu attack ") + names[stepIndex],
                    measuredUs,
                    timing.attackSeconds * 1.0e6 * 0.8,
                    timing.attackSeconds * 1.0e6 * 1.2,
                    "us");

        Session release;
        release.set(params::abLevelMatch, 0.0f);
        release.set(params::mode, 3.0f);
        release.set(params::vmuInput, 0.0f);
        release.set(params::vmuThreshold, -22.0f);
        release.set(params::vmuOutput, 0.0f);
        release.set(params::vmuTime, static_cast<float>(stepIndex));
        release.prepare(sampleRate, 256);
        const double hold = 0.05;
        const double total = hold + timing.releaseSeconds * 3.0 + 0.2;
        auto releaseInput = step(sampleRate, total, amplitudeForDb(-2.0f), 0.0);
        for (std::size_t index = static_cast<std::size_t>(sampleRate * hold); index < releaseInput.size(); ++index)
            releaseInput[index] = 0.0f;
        auto releaseSeries = release.recordGr(releaseInput, 256);
        const float held = settledGr(releaseSeries, hold * 0.5, hold - 0.01);
        const double releaseCross = crossingTime(releaseSeries, held * (1.0f - 0.63f), false, hold);
        const double measured = releaseCross - hold;
        expectRange(std::string("varimu release ") + names[stepIndex],
                    measured,
                    timing.releaseSeconds * 0.8,
                    timing.releaseSeconds * 1.2,
                    "s");
    }

    auto programRelease = [&](double holdSeconds) {
        Session session;
        session.set(params::abLevelMatch, 0.0f);
        session.set(params::mode, 3.0f);
        session.set(params::vmuThreshold, -22.0f);
        session.set(params::vmuTime, 4.0f);
        session.prepare(sampleRate, 512);
        const double total = holdSeconds + 12.0;
        auto input = step(sampleRate, total, amplitudeForDb(-2.0f), 0.0);
        for (std::size_t index = static_cast<std::size_t>(sampleRate * holdSeconds); index < input.size(); ++index)
            input[index] = 0.0f;
        auto series = session.recordGr(input, 512);
        const float held = settledGr(series, holdSeconds * 0.6, holdSeconds - 0.05);
        return crossingTime(series, held * (1.0f - 0.63f), false, holdSeconds) - holdSeconds;
    };

    const double light = programRelease(0.15);
    const double heavy = programRelease(8.0);
    report("varimu T5 light", light, "s");
    report("varimu T5 heavy", heavy, "s");
    EXPECT(heavy > light * 1.5);
}

void runNullBypass()
{
    const double sampleRate = 48000.0;
    const auto input = tone(sampleRate, 0.5, 440.0, amplitudeForDb(-26.0f));

    for (int mode = 0; mode < 4; ++mode)
    {
        Session session;
        session.silenceCompression();
        session.set(params::mode, static_cast<float>(mode));
        session.set(params::osFactor, 1.0f);
        session.prepare(sampleRate, 256);
        const int latency = session.processor.getLatencySamples();
        const auto output = session.render(input, 256);
        float worst = 0.0f;
        for (std::size_t index = 0; index < output.size(); ++index)
        {
            const float expected = index >= static_cast<std::size_t>(latency) ? input[index - static_cast<std::size_t>(latency)] : 0.0f;
            worst = std::max(worst, std::fabs(output[index] - expected));
        }
        report("null mode " + std::to_string(mode), 20.0 * std::log10(std::max(worst, 1.0e-12f)), "dBFS");
        EXPECT(worst <= 1.0e-5f);

        Session bypass;
        bypass.silenceCompression();
        bypass.set(params::mode, static_cast<float>(mode));
        bypass.set(params::osFactor, 1.0f);
        bypass.prepare(sampleRate, 256);
        juce::AudioBuffer<float> buffer(2, static_cast<int>(input.size()));
        for (int index = 0; index < buffer.getNumSamples(); ++index)
        {
            buffer.setSample(0, index, input[static_cast<std::size_t>(index)]);
            buffer.setSample(1, index, input[static_cast<std::size_t>(index)]);
        }
        juce::MidiBuffer midi;
        bypass.processor.processBlockBypassed(buffer, midi);
        const int bypassLatency = bypass.processor.getLatencySamples();
        float bypassWorst = 0.0f;
        for (int index = 0; index < buffer.getNumSamples(); ++index)
        {
            const float expected = index >= bypassLatency ? input[static_cast<std::size_t>(index - bypassLatency)] : 0.0f;
            bypassWorst = std::max(bypassWorst, std::fabs(buffer.getSample(0, index) - expected));
        }
        EXPECT(bypassWorst <= 1.0e-5f);
    }
}

void runLevelMatch()
{
    auto renderMatched = [](float threshold, float ratio, float outputDb, const std::vector<float>& input, double sampleRate) {
        Session session;
        session.set(params::mode, 1.0f);
        session.set(params::abLevelMatch, 1.0f);
        session.set(params::vcaThreshold, threshold);
        session.set(params::vcaRatio, ratio);
        session.set(params::vcaOutput, outputDb);
        session.prepare(sampleRate, 256);
        return session.render(input, 256);
    };

    const double sampleRate = 48000.0;
    const int count = static_cast<int>(sampleRate * 12.0);
    std::vector<float> pink(static_cast<std::size_t>(count));
    std::uint32_t seed = 0x1234567u;
    float b0 = 0.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    for (int index = 0; index < count; ++index)
    {
        seed = seed * 1664525u + 1013904223u;
        const float white = (static_cast<float>(seed >> 8) * (1.0f / 16777216.0f)) * 2.0f - 1.0f;
        b0 = 0.99765f * b0 + white * 0.0990460f;
        b1 = 0.96300f * b1 + white * 0.2965164f;
        b2 = 0.57000f * b2 + white * 1.0526913f;
        pink[static_cast<std::size_t>(index)] = (b0 + b1 + b2 + white * 0.1848f) * 0.04f;
    }

    std::vector<float> voice(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index)
    {
        const double time = static_cast<double>(index) / sampleRate;
        const float envelope = std::pow(0.5f + 0.5f * std::sin(2.0 * 3.14159265358979323846 * 3.5 * time), 2.0f);
        float harmonic = 0.0f;
        for (int partial = 1; partial <= 5; ++partial)
            harmonic += std::sin(2.0 * 3.14159265358979323846 * 150.0 * partial * time) / static_cast<float>(partial);
        voice[static_cast<std::size_t>(index)] = 0.12f * envelope * harmonic;
    }

    auto compare = [&](const std::string& name, const std::vector<float>& input) {
        const auto heavy = renderMatched(-30.0f, 8.0f, 0.0f, input, sampleRate);
        const auto light = renderMatched(-12.0f, 2.0f, 6.0f, input, sampleRate);
        const int begin = static_cast<int>(sampleRate * 9.0);
        const int samples = static_cast<int>(input.size()) - begin;
        const float heavyDb = LevelMatch::measureLoudnessDb(heavy.data() + begin, samples, sampleRate);
        const float lightDb = LevelMatch::measureLoudnessDb(light.data() + begin, samples, sampleRate);
        report(name, std::fabs(heavyDb - lightDb), "LU");
        EXPECT(std::fabs(heavyDb - lightDb) <= 0.5f);
    };

    compare("level pink", pink);
    compare("level voice", voice);
}

void runStability()
{
    Session reference;
    reference.silenceCompression();
    reference.set(params::mode, 0.0f);
    reference.set(params::optoPeakRed, 50.0f);
    reference.set(params::osFactor, 0.0f);
    const auto input = tone(48000.0, 0.25, 220.0, 0.2f);
    reference.prepare(48000.0, 1);
    const auto block1 = reference.render(input, 1);
    reference.prepare(48000.0, 1);
    const auto block64 = [&] {
        Session again;
        again.silenceCompression();
        again.set(params::optoPeakRed, 50.0f);
        again.set(params::osFactor, 0.0f);
        again.prepare(48000.0, 64);
        return again.render(input, 64);
    }();
    Session block1024Session;
    block1024Session.silenceCompression();
    block1024Session.set(params::optoPeakRed, 50.0f);
    block1024Session.set(params::osFactor, 0.0f);
    block1024Session.prepare(48000.0, 1024);
    const auto block1024 = block1024Session.render(input, 1024);
    EXPECT(block1.size() == block64.size() && block1.size() == block1024.size());
    for (std::size_t index = 0; index < block1.size(); ++index)
    {
        EXPECT(bitsEqual(block1[index], block64[index]));
        EXPECT(bitsEqual(block1[index], block1024[index]));
    }

    const double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };
    const int blocks[] = { 1, 7, 64, 128, 513, 1024, 4096 };
    std::uint32_t seed = 0x51u;

    for (double sampleRate : rates)
    {
        Session session;
        session.prepare(sampleRate, 4096);
        const int total = static_cast<int>(sampleRate * 60.0);
        int rendered = 0;
        int blockCursor = 0;
        while (rendered < total)
        {
            seed = seed * 1664525u + 1013904223u;
            const int block = blocks[blockCursor % 7];
            blockCursor++;
            const int count = std::min(block, total - rendered);
            if ((rendered / std::max(1, static_cast<int>(sampleRate * 0.1))) != ((rendered + count) / std::max(1, static_cast<int>(sampleRate * 0.1))))
            {
                auto uni = [&seed] {
                    seed = seed * 1664525u + 1013904223u;
                    return static_cast<float>(seed >> 8) * (1.0f / 16777216.0f);
                };
                session.set(params::mode, std::floor(uni() * 4.0f));
                session.set(params::optoPeakRed, uni() * 100.0f);
                session.set(params::optoGain, -12.0f + uni() * 42.0f);
                session.set(params::vcaThreshold, -40.0f + uni() * 40.0f);
                session.set(params::vcaRatio, 1.0f + uni() * 20.0f);
                session.set(params::fetInput, uni() * 48.0f);
                session.set(params::fetAttack, 20.0f + uni() * 780.0f);
                session.set(params::fetRelease, 50.0f + uni() * 1050.0f);
                session.set(params::vmuThreshold, -40.0f + uni() * 40.0f);
                session.set(params::vmuTime, std::floor(uni() * 6.0f));
                session.set(params::abLevelMatch, uni() > 0.5f ? 1.0f : 0.0f);
            }

            juce::AudioBuffer<float> buffer(2, count);
            for (int index = 0; index < count; ++index)
            {
                seed = seed * 1664525u + 1013904223u;
                const float noise = (static_cast<float>(seed >> 8) * (1.0f / 16777216.0f)) * 2.0f - 1.0f;
                const float sample = 0.15f * noise;
                buffer.setSample(0, index, sample);
                buffer.setSample(1, index, sample * 0.8f);
            }
            juce::MidiBuffer midi;
            session.processor.processBlock(buffer, midi);
            for (int channel = 0; channel < 2; ++channel)
                for (int index = 0; index < count; ++index)
                {
                    const float sample = buffer.getSample(channel, index);
                    EXPECT(std::isfinite(sample));
                    EXPECT(std::fabs(sample) <= 4.0f);
                }
            rendered += count;
        }

        const bool checkSilence = std::fabs(sampleRate - 48000.0) < 1.0 || std::fabs(sampleRate - 192000.0) < 1.0;
        if (checkSilence)
        {
            const int silence = static_cast<int>(sampleRate * 10.0);
            int done = 0;
            while (done < silence)
            {
                const int count = std::min(1024, silence - done);
                juce::AudioBuffer<float> buffer(2, count);
                buffer.clear();
                juce::MidiBuffer midi;
                session.processor.processBlock(buffer, midi);
                done += count;
            }
            EXPECT(!session.processor.hasBadAudioState());
        }
    }
}

void runModeSwitch()
{
    const double sampleRate = 48000.0;
    Session session;
    session.set(params::abLevelMatch, 0.0f);
    session.set(params::optoPeakRed, 70.0f);
    session.set(params::vcaThreshold, -24.0f);
    session.set(params::vcaRatio, 4.0f);
    session.set(params::fetInput, 24.0f);
    session.set(params::vmuThreshold, -30.0f);
    session.prepare(sampleRate, 128);
    const int latency = session.processor.getLatencySamples();
    const auto input = tone(sampleRate, 1.2, 180.0, 0.2f);
    std::vector<float> output(input.size(), 0.0f);
    int mode = 0;
    std::size_t offset = 0;
    while (offset < input.size())
    {
        if (offset % static_cast<std::size_t>(sampleRate * 0.1) < 128)
            session.set(params::mode, static_cast<float>(mode++ % 4));

        const int count = 128;
        const int todo = static_cast<int>(std::min<std::size_t>(count, input.size() - offset));
        juce::AudioBuffer<float> buffer(2, todo);
        for (int index = 0; index < todo; ++index)
        {
            buffer.setSample(0, index, input[offset + static_cast<std::size_t>(index)]);
            buffer.setSample(1, index, input[offset + static_cast<std::size_t>(index)]);
        }
        juce::MidiBuffer midi;
        session.processor.processBlock(buffer, midi);
        EXPECT(session.processor.getLatencySamples() == latency);
        for (int index = 0; index < todo; ++index)
            output[offset + static_cast<std::size_t>(index)] = buffer.getSample(0, index);
        offset += static_cast<std::size_t>(todo);
    }

    float maxDelta = 0.0f;
    for (std::size_t index = 1; index < output.size(); ++index)
    {
        EXPECT(std::isfinite(output[index]));
        maxDelta = std::max(maxDelta, std::fabs(output[index] - output[index - 1]));
    }
    report("mode switch max delta", maxDelta, "lin");
    EXPECT(maxDelta < 0.05f);
}

void runVisualContract()
{
    const double sampleRate = 192000.0;
    Session session;
    session.set(params::abLevelMatch, 0.0f);
    session.set(params::mode, 2.0f);
    session.set(params::fetInput, 24.0f);
    session.set(params::fetOutput, 0.0f);
    session.set(params::fetRatio, 3.0f);
    session.set(params::fetAttack, 300.0f);
    session.set(params::fetRelease, 80.0f);
    session.set(params::osFactor, 0.0f);
    session.prepare(sampleRate, 2048);

    juce::AudioBuffer<float> buffer(2, 2048);
    buffer.clear();
    const float amplitude = amplitudeForDb(2.0f);
    for (int index = 128; index < buffer.getNumSamples(); ++index)
    {
        buffer.setSample(0, index, amplitude);
        buffer.setSample(1, index, amplitude);
    }
    juce::MidiBuffer midi;
    session.processor.processBlock(buffer, midi);

    VisualFrame frame;
    bool found = false;
    VisualFrame candidate;
    while (session.processor.popVisualFrame(candidate))
    {
        frame = candidate;
        if (candidate.fetEventValid > 0.5f)
            found = true;
    }
    EXPECT(found);
    const float measured = frame.fetGrStartOffset - frame.fetCrossOffset;
    const float expected = 300.0e-6f * static_cast<float>(sampleRate);
    report("fet attack samples", measured, "samples");
    EXPECT(std::fabs(measured - expected) <= 1.0f);
    EXPECT(near(frame.grDb, session.processor.currentGrDb(), 1.0e-4f));
    EXPECT(near(frame.detDb, session.processor.currentDetectorDb(), 1.0e-3f));
}

void runPerf()
{
    const double sampleRate = 48000.0;
    const int block = 128;
    const int total = static_cast<int>(sampleRate * 1.0);
    for (int mode = 0; mode < 4; ++mode)
    {
        Session session;
        session.set(params::abLevelMatch, 0.0f);
        session.set(params::mode, static_cast<float>(mode));
        session.set(params::osFactor, mode >= 2 ? 1.0f : 0.0f);
        session.prepare(sampleRate, block);
        const auto input = tone(sampleRate, 1.0, 440.0, 0.25f);
        const auto started = std::chrono::steady_clock::now();
        const auto output = session.render(input, block);
        const auto elapsed = std::chrono::steady_clock::now() - started;
        const double ns = std::chrono::duration<double, std::nano>(elapsed).count() / static_cast<double>(total);
        report("perf mode " + std::to_string(mode), ns, "ns/sample");
        EXPECT(output.size() == input.size());
        EXPECT(ns < 100000.0);
    }
}

} // namespace squeeze::test
