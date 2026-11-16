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
