#pragma once

#include <cstdio>
#include <string>

inline int check_count = 0;
inline int fail_count = 0;

inline void check(bool ok, const std::string& name)
{
    ++check_count;
    if (!ok) {
        ++fail_count;
        std::printf("FAIL: %s\n", name.c_str());
        std::fflush(stdout);
    }
}

inline int report()
{
    std::printf("%d checks, %d failed\n", check_count, fail_count);
    std::fflush(stdout);
    return fail_count == 0 ? 0 : 1;
}
