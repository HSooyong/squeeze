#include "LevelMatch.h"

#include "MathUtil.h"
#include "Tunables.h"

#include <algorithm>
#include <cmath>

namespace squeeze
{
namespace
{

constexpr double kShelfFrequency = 1681.974450955533;
constexpr double kShelfQ = 0.7071752369554196;
constexpr double kShelfGainDb = 3.999843853973347;
constexpr double kHighPassFrequency = 38.13547087602444;
constexpr double kHighPassQ = 0.5003270373238773;

} // namespace

void LevelMatch::Biquad::setHighShelf(double sampleRate, double frequency, double q, double gainDb) noexcept
{
    const double k = std::tan(3.14159265358979323846 * frequency / sampleRate);
    const double vh = std::pow(10.0, gainDb / 20.0);
    const double vb = std::pow(vh, 0.4996667741545416);
    const double a0 = 1.0 + k / q + k * k;
    b0 = static_cast<float>((vh + vb * k / q + k * k) / a0);
    b1 = static_cast<float>(2.0 * (k * k - vh) / a0);
    b2 = static_cast<float>((vh - vb * k / q + k * k) / a0);
    a1 = static_cast<float>(2.0 * (k * k - 1.0) / a0);
    a2 = static_cast<float>((1.0 - k / q + k * k) / a0);
    reset();
}

void LevelMatch::Biquad::setHighPass(double sampleRate, double frequency, double q) noexcept
{
    const double k = std::tan(3.14159265358979323846 * frequency / sampleRate);
    const double a0 = 1.0 + k / q + k * k;
    b0 = static_cast<float>(1.0 / a0);
    b1 = static_cast<float>(-2.0 / a0);
    b2 = static_cast<float>(1.0 / a0);
    a1 = static_cast<float>(2.0 * (k * k - 1.0) / a0);
    a2 = static_cast<float>((1.0 - k / q + k * k) / a0);
    reset();
}

float LevelMatch::Biquad::process(float sample) noexcept
{
    const float output = b0 * sample + z1;
    z1 = flushDenormal(b1 * sample - a1 * output + z2);
    z2 = flushDenormal(b2 * sample - a2 * output);
    return output;
}

bool LevelMatch::Biquad::hasBadState() const noexcept
{
    return isBadFloat(z1) || isBadFloat(z2);
}

void LevelMatch::prepare(double sampleRate, int)
{
    const double rate = sampleRate > 0.0 ? sampleRate : 44100.0;
    for (int channel = 0; channel < 2; ++channel)
    {
        inputShelf[channel].setHighShelf(rate, kShelfFrequency, kShelfQ, kShelfGainDb);
        outputShelf[channel].setHighShelf(rate, kShelfFrequency, kShelfQ, kShelfGainDb);
        inputHighPass[channel].setHighPass(rate, kHighPassFrequency, kHighPassQ);
        outputHighPass[channel].setHighPass(rate, kHighPassFrequency, kHighPassQ);
    }

    energyCoeff = onePoleCoeff(rate, tunables::levelMatchWindowSeconds);
    glideCoeff = onePoleCoeff(rate, tunables::levelMatchGlideSeconds);
    reset();
}

void LevelMatch::reset() noexcept
{
    for (int channel = 0; channel < 2; ++channel)
    {
        inputShelf[channel].reset();
        outputShelf[channel].reset();
        inputHighPass[channel].reset();
        outputHighPass[channel].reset();
    }

    inputEnergy = 0.0f;
    outputEnergy = 0.0f;
    trim = 0.0f;
}

void LevelMatch::process(const float* const* dry, float* const* wet, int numChannels, int numSamples) noexcept
{
    if (!enabled || dry == nullptr || wet == nullptr || numChannels <= 0 || numSamples <= 0)
        return;

    const int channels = std::min(numChannels, 2);

    for (int index = 0; index < numSamples; ++index)
    {
        float dryPower = 0.0f;
        float wetPower = 0.0f;

        for (int channel = 0; channel < channels; ++channel)
        {
            const float drySample = dry[channel][index];
            const float wetSample = wet[channel][index];
            const float weightedDry = inputHighPass[channel].process(inputShelf[channel].process(drySample));
            const float weightedWet = outputHighPass[channel].process(outputShelf[channel].process(wetSample));
            dryPower += weightedDry * weightedDry;
            wetPower += weightedWet * weightedWet;
        }

        dryPower /= static_cast<float>(channels);
        wetPower /= static_cast<float>(channels);
        inputEnergy += energyCoeff * (dryPower - inputEnergy);
        outputEnergy += energyCoeff * (wetPower - outputEnergy);
        inputEnergy = flushDenormal(std::max(inputEnergy, 0.0f));
        outputEnergy = flushDenormal(std::max(outputEnergy, 0.0f));

        float target = 0.0f;
        if (inputEnergy > 1.0e-10f && outputEnergy > 1.0e-10f)
            target = clampFloat(10.0f * std::log10(inputEnergy / outputEnergy), -24.0f, 24.0f);

        trim += glideCoeff * (target - trim);
        trim = flushDenormal(trim);
        const float gain = dbToGain(trim);

        for (int channel = 0; channel < numChannels; ++channel)
            wet[channel][index] *= gain;
    }
}

float LevelMatch::measureLoudnessDb(const float* samples, int numSamples, double sampleRate)
{
    if (samples == nullptr || numSamples <= 0)
        return -160.0f;

    Biquad shelf;
    Biquad highPass;
    shelf.setHighShelf(sampleRate, kShelfFrequency, kShelfQ, kShelfGainDb);
    highPass.setHighPass(sampleRate, kHighPassFrequency, kHighPassQ);

    double energy = 0.0;
    for (int index = 0; index < numSamples; ++index)
    {
        const float weighted = highPass.process(shelf.process(samples[index]));
        energy += static_cast<double>(weighted) * static_cast<double>(weighted);
    }

    const double mean = energy / static_cast<double>(numSamples);
    if (mean <= 1.0e-16)
        return -160.0f;
    return static_cast<float>(10.0 * std::log10(mean));
}

bool LevelMatch::hasBadState() const noexcept
{
    if (isBadFloat(inputEnergy) || isBadFloat(outputEnergy) || isBadFloat(trim))
        return true;

    for (int channel = 0; channel < 2; ++channel)
    {
        if (inputShelf[channel].hasBadState() || outputShelf[channel].hasBadState()
            || inputHighPass[channel].hasBadState() || outputHighPass[channel].hasBadState())
            return true;
    }

    return false;
}

} // namespace squeeze
