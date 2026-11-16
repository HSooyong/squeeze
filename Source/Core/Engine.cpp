#include "Engine.h"

namespace squeeze
{

void Engine::prepare(double sampleRate, int maxBlockSize) noexcept
{
    prepared.sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    prepared.maxBlockSize = maxBlockSize > 0 ? maxBlockSize : 0;

    opto.prepare(prepared.sampleRate, prepared.maxBlockSize);
    vca.prepare(prepared.sampleRate, prepared.maxBlockSize);
    fet.prepare(prepared.sampleRate, prepared.maxBlockSize);
    variMu.prepare(prepared.sampleRate, prepared.maxBlockSize);
}

void Engine::reset() noexcept
{
    opto.reset();
    vca.reset();
    fet.reset();
    variMu.reset();
}

void Engine::setMode(Mode newMode) noexcept
{
    mode = newMode;
}

Mode Engine::getMode() const noexcept
{
    return mode;
}

void Engine::process(float* const* channels, int numChannels, int numSamples) noexcept
{
    if (channels == nullptr || numChannels <= 0 || numSamples <= 0)
        return;

    switch (mode)
    {
        case Mode::opto:
            opto.process(channels, numChannels, numSamples);
            break;
        case Mode::vca:
            vca.process(channels, numChannels, numSamples);
            break;
        case Mode::fet:
            fet.process(channels, numChannels, numSamples);
            break;
        case Mode::variMu:
            variMu.process(channels, numChannels, numSamples);
            break;
    }
}

} // namespace squeeze
