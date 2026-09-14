#include <cstdio>
#include <string>

#include "check.h"
#include "text.h"

static std::string hex(const std::string& bytes)
{
    std::string result;
    for (char c : bytes) {
        char buffer[4];
        std::snprintf(buffer, sizeof buffer, "%02X", static_cast<unsigned char>(c));
        result += buffer;
    }
    return result;
}

static std::string hex(const std::wstring& text)
{
    std::string result;
    for (wchar_t c : text) {
        char buffer[8];
        std::snprintf(buffer, sizeof buffer, "%04X ", static_cast<unsigned>(c));
        result += buffer;
    }
    return result;
}

static std::string utf8(char32_t code)
{
    std::string result;
    if (code < 0x80) {
        result += static_cast<char>(code);
    } else if (code < 0x800) {
        result += static_cast<char>(0xC0 | (code >> 6));
        result += static_cast<char>(0x80 | (code & 0x3F));
    } else if (code < 0x10000) {
        result += static_cast<char>(0xE0 | (code >> 12));
        result += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (code & 0x3F));
    } else {
        result += static_cast<char>(0xF0 | (code >> 18));
        result += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
        result += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (code & 0x3F));
    }
    return result;
}

static std::wstring utf16(char32_t code)
{
    if (code < 0x10000)
        return std::wstring(1, static_cast<wchar_t>(code));
    code -= 0x10000;
    return {static_cast<wchar_t>(0xD800 + (code >> 10)), static_cast<wchar_t>(0xDC00 + (code & 0x3FF))};
}

static void converts(const std::string& bytes, const std::wstring& wide, const std::string& name)
{
    std::wstring widened = widen(bytes);
    std::string narrowed = narrow(wide);
    check(widened == wide, "widen(" + name + ") == " + hex(wide) + ", got " + hex(widened));
    check(narrowed == bytes, "narrow(" + name + ") == " + hex(bytes) + ", got " + hex(narrowed));
}

static void roundTrips(const std::string& bytes, const std::string& name)
{
    std::string result = narrow(widen(bytes));
    check(result == bytes, "narrow(widen(" + name + ")) round-trips, got " + hex(result));
}

static void testEmpty()
{
    check(widen("").empty(), "widen(\"\") is empty");
    check(narrow(L"").empty(), "narrow(L\"\") is empty");
    check(widen(std::string()).empty(), "widen of a default string is empty");
    check(narrow(std::wstring()).empty(), "narrow of a default wstring is empty");
}

static void testKnownValues()
{
    converts("a", L"a", "a");
    converts("hello", L"hello", "hello");
    converts("C:\\Videos\\clip one.mp4", L"C:\\Videos\\clip one.mp4", "a Windows path");
    converts("line\nbreak\ttab", L"line\nbreak\ttab", "control characters");
    converts("caf\xC3\xA9", L"caf\u00E9", "cafe with acute accent");
    converts("\xC3\x9C" "ber", L"\u00DCber", "Uber with umlaut");
    converts("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E", L"\u65E5\u672C\u8A9E", "Japanese");
    converts("\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82", L"\u041F\u0440\u0438\u0432\u0435\u0442", "Russian");
    converts("\xF0\x9F\x98\x80", std::wstring{static_cast<wchar_t>(0xD83D), static_cast<wchar_t>(0xDE00)}, "grinning face emoji");
    converts("a\xF0\x9F\x8E\xB5z", std::wstring{L'a', static_cast<wchar_t>(0xD83C), static_cast<wchar_t>(0xDFB5), L'z'}, "emoji between letters");
    converts("\xEF\xBF\xBF", L"\uFFFF", "U+FFFF");
    converts("\xF4\x8F\xBF\xBF", std::wstring{static_cast<wchar_t>(0xDBFF), static_cast<wchar_t>(0xDFFF)}, "U+10FFFF");

    check(widen("caf\xC3\xA9").size() == 4, "widen of cafe with accent has 4 UTF-16 units");
    check(narrow(L"\u65E5").size() == 3, "narrow of one CJK character gives 3 bytes");
    check(widen("\xF0\x9F\x98\x80").size() == 2, "widen of an emoji gives a surrogate pair");
    check(narrow(std::wstring{static_cast<wchar_t>(0xD83D), static_cast<wchar_t>(0xDE00)}).size() == 4, "narrow of a surrogate pair gives 4 bytes");
}

static void testEmbeddedNull()
{
    std::string bytes("a\0b", 3);
    std::wstring wide(L"a\0b", 3);
    converts(bytes, wide, "a NUL b");
    roundTrips(std::string("\0", 1), "a single NUL");
}

static void testEveryCodePoint()
{
    char32_t firstBad = 0;
    std::string all;
    std::wstring allWide;
    for (char32_t code = 1; code <= 0x10FFFF; ++code) {
        if (code >= 0xD800 && code <= 0xDFFF)
            continue;
        std::string bytes = utf8(code);
        std::wstring wide = utf16(code);
        all += bytes;
        allWide += wide;
        if (firstBad == 0 && (widen(bytes) != wide || narrow(wide) != bytes))
            firstBad = code;
    }
    char buffer[16];
    std::snprintf(buffer, sizeof buffer, "U+%04X", static_cast<unsigned>(firstBad));
    check(firstBad == 0, std::string("widen and narrow convert every code point on its own, first bad ") + buffer);
    check(widen(all) == allWide, "widen of all code points in one string matches UTF-16");
    check(narrow(allWide) == all, "narrow of all code points in one string matches UTF-8");
}

static void testLongText()
{
    std::string bytes;
    for (int i = 0; i < 100000; ++i)
        bytes += "\xE6\x97\xA5" "a\xF0\x9F\x98\x80";
    std::wstring wide = widen(bytes);
    check(wide.size() == 400000, "widen of 100000 repeats gives 400000 UTF-16 units, got " + std::to_string(wide.size()));
    roundTrips(bytes, "a long mixed string");
}

static void testInvalidInputDoesNotCrash()
{
    widen(std::string("\xFF\xFE\xC3", 3));
    widen(std::string("\xE6\x97", 2));
    narrow(std::wstring(1, static_cast<wchar_t>(0xD800)));
    narrow(std::wstring{L'a', static_cast<wchar_t>(0xDC00), L'b'});
    check(true, "invalid input does not crash");
}

int main()
{
    testEmpty();
    testKnownValues();
    testEmbeddedNull();
    testEveryCodePoint();
    testLongText();
    testInvalidInputDoesNotCrash();
    return report();
}
