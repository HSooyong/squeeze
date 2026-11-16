#pragma once

namespace squeeze::tunables
{

// Values marked "Proposal" come from the build spec as starting points.
// They are deliberately not baked into call sites so listening tests can retune them.
// Targets marked "Target" are the acceptance numbers the tests check.

// --- Optical leveling (LAMP) ---
inline constexpr float optoSidechainAtZeroDb = -30.0f;   // Proposal. Peak Reduction 0.
inline constexpr float optoSidechainAtFullDb = 10.0f;    // Proposal. Peak Reduction 100.
inline constexpr float optoThresholdDb = -20.0f;         // Fixed internal threshold.
inline constexpr float optoCompressRatio = 3.0f;         // Proposal.
inline constexpr float optoCompressKneeDb = 12.0f;       // Proposal.
inline constexpr float optoLimitRatio = 10.0f;           // Proposal.
inline constexpr float optoLimitKneeDb = 6.0f;           // Proposal.
inline constexpr float optoDetectorSeconds = 0.001f;     // Rectified smoother. Describes the peak/RMS blend.
inline constexpr float optoAttackSeconds = 0.010f;       // Target: about 10 ms, ±20%.
inline constexpr float optoFastReleaseSeconds = 0.022f;  // Chosen so about half the GR recovers inside 40–120 ms.
inline constexpr float optoSlowReleaseMinSeconds = 0.5f; // Proposal formula: 0.5 s + 4.5 s * memory.
inline constexpr float optoSlowReleaseSpanSeconds = 4.5f;
inline constexpr float optoMemorySeconds = 5.0f;         // Proposal formula.
inline constexpr float optoMemoryReferenceDb = 12.0f;    // GR that counts as full program memory.

// --- RMS VCA (NEEDLE) ---
// The 5 ms proposal makes the 20 dB / 30 dB attack targets late, so the window is shorter.
inline constexpr float vcaRmsSeconds = 0.0015f;         // Proposal was 5 ms; tuned so the 15/5/3 ms attacks still pass.
inline constexpr float vcaReleaseDbPerSecond = 120.0f;   // Target: 120 dB/s, ±10%.
inline constexpr float vcaAttackOverDb[3] = { 10.0f, 20.0f, 30.0f };
inline constexpr float vcaAttackSeconds[3] = { 0.015f, 0.005f, 0.003f }; // Targets at infinite ratio, ±25%.

// --- FET peak (SPARK) ---
inline constexpr float fetThresholdDb = -18.0f;          // Proposal.
inline constexpr float fetKneeDb[4] = { 6.0f, 4.0f, 3.0f, 2.0f }; // Proposal for ratios 4, 8, 12, 20.
inline constexpr float fetRatios[4] = { 4.0f, 8.0f, 12.0f, 20.0f };
inline constexpr float fetAllRatio = 20.0f;              // Proposal approximation: at least 20:1.
inline constexpr float fetAllKneeDb = 10.0f;             // Proposal approximation: wider knee than 20:1.
inline constexpr float fetAllOvershoot = 0.35f;          // Proposal approximation: release overshoot amount.
inline constexpr float fetPeakHoldSeconds = 0.0015f;     // Keeps a sine on its peak without moving the release targets.
inline constexpr float fetColorPerInputDb = 0.004f;     // Proposal. Zero at input display 24.
inline constexpr float fetColorPerGrDb = 0.003f;         // Proposal. Small so a steady tone stays near the gain law.

// --- Variable-mu (TUBE) ---
// The six-step table is a proposal. It has not been checked against a published source.
inline constexpr float vmuKneeDb = 14.0f;                // Proposal.
inline constexpr float vmuRatioBase = 1.4f;              // Proposal: R = min(6, 1.4 + 0.28 * over).
inline constexpr float vmuRatioSlope = 0.28f;            // Proposal.
inline constexpr float vmuRatioMax = 6.0f;               // Proposal.
inline constexpr float vmuPeakHoldSeconds = 0.0015f;
inline constexpr float vmuMemorySeconds = 12.0f;
inline constexpr float vmuMemoryReferenceDb = 8.0f;
inline constexpr float vmuColorPerInputDb = 0.003f;      // Proposal.
inline constexpr float vmuColorPerGrDb = 0.002f;         // Proposal. Small so a steady tone stays near the gain law.

struct VariMuTiming
{
    float attackSeconds;
    float releaseSeconds;
    float slowReleaseSeconds; // 0 means a single release. T5 and T6 use the slow value.
    bool programDependent;
};

// Proposal table, unverified.
inline constexpr VariMuTiming vmuTiming[6] = {
    { 0.0002f, 0.3f, 0.0f, false },
    { 0.0002f, 0.8f, 0.0f, false },
    { 0.0004f, 2.0f, 0.0f, false },
    { 0.0004f, 5.0f, 0.0f, false },
    { 0.0004f, 2.0f, 10.0f, true },
    { 0.0002f, 0.3f, 10.0f, true },
};

// Shared behaviour from the build spec (not proposals).
inline constexpr float parameterSmoothSeconds = 0.020f;
inline constexpr float timeSmoothSeconds = 0.050f;
inline constexpr float stepSmoothSeconds = 0.020f;
inline constexpr float modeCrossfadeSeconds = 0.030f;
inline constexpr float levelMatchWindowSeconds = 3.0f;
inline constexpr float levelMatchGlideSeconds = 1.0f;
inline constexpr float outputSafety = 4.0f;

inline float optoSidechainDb(float peakReduction) noexcept
{
    const float amount = peakReduction / 100.0f;
    return optoSidechainAtZeroDb + (optoSidechainAtFullDb - optoSidechainAtZeroDb) * amount;
}

} // namespace squeeze::tunables
