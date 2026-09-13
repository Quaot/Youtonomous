#include <cstdio>
#include <string>

#include "timecode.h"

static int failures = 0;

static void check(bool ok, const char* name)
{
    if (!ok) {
        std::printf("FAIL: %s\n", name);
        ++failures;
    }
}

static void testParse()
{
    check(timecode::parse("270") == 270, "plain seconds");
    check(timecode::parse("4:30") == 270, "minutes and seconds");
    check(timecode::parse("1:02:03") == 3723, "hours");
    check(timecode::parse(" 0:05 ") == 5, "spaces");
    check(timecode::parse("4m30s") == 270, "units");
    check(timecode::parse("1h") == 3600, "hours unit");
    check(timecode::parse("4m30") == 270, "trailing seconds");

    check(!timecode::parse(""), "empty");
    check(!timecode::parse("1:75"), "seconds over 59");
    check(!timecode::parse("1::2"), "double colon");
    check(!timecode::parse("1:2:3:4"), "too many parts");
    check(!timecode::parse("abc"), "letters");
    check(!timecode::parse("-5"), "negative");
}

static void testFormat()
{
    check(timecode::format(0) == "0:00", "zero");
    check(timecode::format(270) == "4:30", "under an hour");
    check(timecode::format(3723) == "1:02:03", "over an hour");
    check(timecode::format(-3) == "0:00", "negative");
}

int main()
{
    testParse();
    testFormat();

    if (failures == 0)
        std::printf("All tests passed\n");
    return failures == 0 ? 0 : 1;
}
