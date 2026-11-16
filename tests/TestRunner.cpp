#include "Check.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <exception>
#include <iostream>
#include <string>

namespace squeeze::test
{
void runParams();
void runState();
void runProcess();
void runStaticCurve();
void runTimingOpto();
void runTimingVca();
void runTimingFet();
void runTimingVariMu();
void runNullBypass();
void runLevelMatch();
void runStability();
void runModeSwitch();
void runVisualContract();
void runPerf();
}

namespace
{

int runNamed(const std::string& name)
{
    const int before = squeeze::test::failureCount();

    try
    {
        if (name == "params")
            squeeze::test::runParams();
        else if (name == "state")
            squeeze::test::runState();
        else if (name == "process")
            squeeze::test::runProcess();
        else if (name == "static_curve")
            squeeze::test::runStaticCurve();
        else if (name == "timing_opto")
            squeeze::test::runTimingOpto();
        else if (name == "timing_vca")
            squeeze::test::runTimingVca();
        else if (name == "timing_fet")
            squeeze::test::runTimingFet();
        else if (name == "timing_varimu")
            squeeze::test::runTimingVariMu();
        else if (name == "null_bypass")
            squeeze::test::runNullBypass();
        else if (name == "level_match")
            squeeze::test::runLevelMatch();
        else if (name == "stability")
            squeeze::test::runStability();
        else if (name == "mode_switch")
            squeeze::test::runModeSwitch();
        else if (name == "visual_contract")
            squeeze::test::runVisualContract();
        else if (name == "perf")
            squeeze::test::runPerf();
        else
        {
            std::cerr << "Unknown test \"" << name << "\"\n";
            return 2;
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << name << " exception: " << error.what() << "\n";
        return 1;
    }

    const int added = squeeze::test::failureCount() - before;

    if (added > 0)
    {
        std::cerr << added << " assertion(s) failed in " << name << "\n";
        return 1;
    }

    std::cout << "ok " << name << "\n";
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const std::string requested = argc > 1 ? argv[1] : "all";

    if (requested == "all")
    {
        int status = runNamed("params");

        if (status == 0)
            status = runNamed("state");

        if (status == 0)
            status = runNamed("process");

        return status;
    }

    return runNamed(requested);
}
