#include <climits>
#include <cstdio>
#include <optional>
#include <string>

#include "check.h"
#include "timecode.h"

static std::string show(const std::string& text)
{
    std::string result = "\"";
    for (char c : text) {
        if (static_cast<unsigned char>(c) < 32) {
            char buffer[8];
            std::snprintf(buffer, sizeof buffer, "\\x%02X", static_cast<unsigned char>(c));
            result += buffer;
        } else {
            result += c;
        }
    }
    return result + "\"";
}

static std::string show(const std::optional<int>& value)
{
    return value ? std::to_string(*value) : "nothing";
}

static void parses(const std::string& text, int seconds)
{
    std::optional<int> result = timecode::parse(text);
    check(result && *result == seconds,
          "parse(" + show(text) + ") == " + std::to_string(seconds) + ", got " + show(result));
}

static void rejects(const std::string& text)
{
    std::optional<int> result = timecode::parse(text);
    check(!result, "parse(" + show(text) + ") is invalid, got " + show(result));
}

static void doesNotOverflow(const std::string& text)
{
    std::optional<int> result = timecode::parse(text);
    check(!result || *result == INT_MAX, "parse(" + show(text) + ") does not overflow, got " + show(result));
}

static void formats(int seconds, const std::string& text)
{
    std::string result = timecode::format(seconds);
    check(result == text, "format(" + std::to_string(seconds) + ") == " + show(text) + ", got " + show(result));
}

static bool digits(const std::string& text)
{
    if (text.empty())
        return false;
    for (char c : text)
        if (c < '0' || c > '9')
            return false;
    return true;
}

static bool wellFormed(int seconds, const std::string& text)
{
    size_t first = text.find(':');
    size_t last = text.rfind(':');
    if (first == std::string::npos)
        return false;
    std::string lead = text.substr(0, first);
    if (!digits(lead) || (lead.size() > 1 && lead[0] == '0'))
        return false;
    std::string secs = text.substr(last + 1);
    if (secs.size() != 2 || !digits(secs) || secs[0] > '5')
        return false;
    if (seconds < 3600)
        return first == last;
    std::string mins = text.substr(first + 1, last - first - 1);
    return first != last && mins.size() == 2 && digits(mins) && mins[0] <= '5';
}

static void testColonForms()
{
    parses("1:02:03", 3723);
    parses("4:30", 270);
    parses("0:00", 0);
    parses("00:00", 0);
    parses("0:00:00", 0);
    parses("0:01", 1);
    parses("0:59", 59);
    parses("1:00", 60);
    parses("04:30", 270);
    parses("10:00", 600);
    parses("59:59", 3599);
    parses("60:00", 3600);
    parses("90:00", 5400);
    parses("0:59:59", 3599);
    parses("1:00:00", 3600);
    parses("01:02:03", 3723);
    parses("1:59:59", 7199);
    parses("10:00:00", 36000);
    parses("23:59:59", 86399);
    parses("100:00:00", 360000);
    parses("596523:14:07", INT_MAX);
}

static void testPlainSeconds()
{
    parses("0", 0);
    parses("5", 5);
    parses("59", 59);
    parses("60", 60);
    parses("270", 270);
    parses("3599", 3599);
    parses("3600", 3600);
    parses("86400", 86400);
    parses("2147483647", INT_MAX);
}

static void testUnitForms()
{
    parses("4m30s", 270);
    parses("1h", 3600);
    parses("90s", 90);
    parses("0s", 0);
    parses("2m", 120);
    parses("90m", 5400);
    parses("1h30m", 5400);
    parses("1h2m3s", 3723);
    parses("1h30s", 3630);
    parses("2h0m0s", 7200);
    parses("0h0m0s", 0);
    parses("10h", 36000);
    parses("3600s", 3600);
}

static void testWhitespace()
{
    parses("  4:30  ", 270);
    parses("\t270\n", 270);
    parses(" 1:02:03", 3723);
    parses("4m30s ", 270);
    parses("\r\n90s\r\n", 90);
}

static void testInvalid()
{
    const char* inputs[] = {
        "", " ", "\t\n",
        "-5", "-1", "-0:30", "-4:30", "-1:00:00", "-4m30s", "4m-30s", "-1h",
        "abc", "x", "four", "4:30x", "x4:30", "270x", "4m30sx", "4:3o",
        "4::30", ":30", "4:", ":", "::", "1:02:", ":02:03", "1::03", "1:02:03:04",
        "0:60", "4:60", "1:60", "1:02:60", "1:60:00", "1:00:60", "1:02:99", "4:-30", "1:-1:00",
        "4.5", "4,30", "4;30", "4.30", "4 30", "4:30 5", "1 h",
        "m", "h", "s", "ms", "hms", "4q", "4x30", "s30", "m4", "4.5m", "1e3", "0x10"};
    for (const char* input : inputs)
        rejects(input);
}

static void testOverflow()
{
    doesNotOverflow("2147483648");
    doesNotOverflow("99999999999999999999");
    doesNotOverflow("596524:00:00");
    doesNotOverflow("99999999999:00:00");
    doesNotOverflow("35791395m");
    doesNotOverflow("596524h");
    doesNotOverflow("9999999999s");
}

static void testFormat()
{
    formats(0, "0:00");
    formats(1, "0:01");
    formats(5, "0:05");
    formats(9, "0:09");
    formats(10, "0:10");
    formats(59, "0:59");
    formats(60, "1:00");
    formats(61, "1:01");
    formats(270, "4:30");
    formats(599, "9:59");
    formats(600, "10:00");
    formats(3599, "59:59");
    formats(3600, "1:00:00");
    formats(3601, "1:00:01");
    formats(3660, "1:01:00");
    formats(3723, "1:02:03");
    formats(7199, "1:59:59");
    formats(36000, "10:00:00");
    formats(86399, "23:59:59");
    formats(86400, "24:00:00");
    formats(360000, "100:00:00");
    formats(INT_MAX, "596523:14:07");
    formats(-1, "0:00");
    formats(-59, "0:00");
    formats(-3600, "0:00");
    formats(INT_MIN, "0:00");
}

static void testRoundTrip()
{
    int bad_round_trip = -1;
    int bad_shape = -1;
    for (int n = 0; n <= 400000; ++n) {
        std::string text = timecode::format(n);
        std::optional<int> result = timecode::parse(text);
        if (bad_round_trip < 0 && (!result || *result != n))
            bad_round_trip = n;
        if (bad_shape < 0 && !wellFormed(n, text))
            bad_shape = n;
    }
    std::string round_trip_name = "parse(format(n)) == n for n in 0..400000, first bad n = " + std::to_string(bad_round_trip);
    if (bad_round_trip >= 0)
        round_trip_name += " (format gives " + show(timecode::format(bad_round_trip)) + ")";
    check(bad_round_trip < 0, round_trip_name);

    std::string shape_name = "format(n) is M:SS below an hour and H:MM:SS from an hour, first bad n = " + std::to_string(bad_shape);
    if (bad_shape >= 0)
        shape_name += " (format gives " + show(timecode::format(bad_shape)) + ")";
    check(bad_shape < 0, shape_name);

    for (int n : {359999, 360000, 3599999, 86400 * 365, 1000000000, INT_MAX - 1, INT_MAX}) {
        std::optional<int> result = timecode::parse(timecode::format(n));
        std::string name = "parse(format(" + std::to_string(n) + ")) == n, format gives " + show(timecode::format(n));
        check(result && *result == n, name + ", parse gives " + show(result));
    }
}

int main()
{
    testColonForms();
    testPlainSeconds();
    testUnitForms();
    testWhitespace();
    testInvalid();
    testOverflow();
    testFormat();
    testRoundTrip();
    return report();
}
