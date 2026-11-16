#pragma once

#include <cmath>
#include <cstring>
#include <string>

namespace squeeze::test
{

int& failureCount();

void expect(bool condition, const std::string& message, const char* file, int line);

inline bool near(float actual, float expected, float tolerance = 1.0e-4f)
{
    return std::fabs(actual - expected) <= tolerance;
}

inline bool bitsEqual(float actual, float expected)
{
    return std::memcmp(&actual, &expected, sizeof(float)) == 0;
}

} // namespace squeeze::test

#define EXPECT(...) ::squeeze::test::expect((__VA_ARGS__), #__VA_ARGS__, __FILE__, __LINE__)
