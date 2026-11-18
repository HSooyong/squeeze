#include "Check.h"

#include <iostream>

namespace squeeze::test
{

int& failureCount()
{
    static int count = 0;
    return count;
}

void expect(bool condition, const std::string& message, const char* file, int line)
{
    if (condition)
        return;

    std::cerr << file << ":" << line << ": " << message << "\n";
    ++failureCount();
}

} // namespace squeeze::test
