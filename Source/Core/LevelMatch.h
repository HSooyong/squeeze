#pragma once

namespace squeeze
{

// Feed-forward loudness trim. K-weighted mean squares over about three seconds,
// then a one-second glide. Disabled leaves the buffer untouched.
class LevelMatch
{
public:
    void prepare(double sampleRate, int numChannels);
    void reset() noexcept;
    void setEnabled(bool shouldEnable) noexcept { enabled = shouldEnable; }
    bool isEnabled() const noexcept { return enabled; }

    // `dry` is the block input. `wet` is overwritten with the trimmed output.
    void process(const float* const* dry, float* const* wet, int numChannels, int numSamples) noexcept;

    float trimDb() const noexcept { return trim; }
    bool hasBadState() const noexcept;

    // Mean-square level of a K-weighted signal, in dB. Used by the processor and the tests.
    static float measureLoudnessDb(const float* samples, int numSamples, double sampleRate);

private:
    struct Biquad
    {
        float b0 = 1.0f;
        float b1 = 0.0f;
        float b2 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
        float z1 = 0.0f;
        float z2 = 0.0f;

        void setHighShelf(double sampleRate, double frequency, double q, double gainDb) noexcept;
        void setHighPass(double sampleRate, double frequency, double q) noexcept;
        float process(float sample) noexcept;
        void reset() noexcept { z1 = 0.0f; z2 = 0.0f; }
        bool hasBadState() const noexcept;
    };

    Biquad inputShelf[2];
    Biquad inputHighPass[2];
    Biquad outputShelf[2];
    Biquad outputHighPass[2];
    float inputEnergy = 0.0f;
    float outputEnergy = 0.0f;
    float trim = 0.0f;
    float energyCoeff = 1.0f;
    float glideCoeff = 1.0f;
    bool enabled = false;
};

} // namespace squeeze
