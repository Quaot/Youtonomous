#pragma once

#include <cstdio>
#include <string>

inline int checkCount = 0;
inline int failCount = 0;

inline void check(bool ok, const std::string& name)
{
    ++checkCount;
    if (!ok) {
        ++failCount;
        std::printf("FAIL: %s\n", name.c_str());
        std::fflush(stdout);
    }
}

inline int report()
{
    std::printf("%d checks, %d failed\n", checkCount, failCount);
    std::fflush(stdout);
    return failCount == 0 ? 0 : 1;
}
