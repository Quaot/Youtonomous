#include "timecode.h"

#include <cctype>
#include <climits>
#include <cstdio>
#include <vector>

namespace timecode {

static bool isDigit(char c)
{
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

static bool isSpace(char c)
{
    return std::isspace(static_cast<unsigned char>(c)) != 0;
}

static std::string trim(const std::string& text)
{
    size_t first = 0;
    while (first < text.size() && isSpace(text[first]))
        ++first;
    size_t last = text.size();
    while (last > first && isSpace(text[last - 1]))
        --last;
    return text.substr(first, last - first);
}

static bool addDigit(long long& number, char c)
{
    number = number * 10 + (c - '0');
    return number <= INT_MAX;
}

static std::optional<int> parseUnits(const std::string& text)
{
    long long total = 0;
    long long number = 0;
    bool have_number = false;

    for (char c : text) {
        if (isDigit(c)) {
            if (!addDigit(number, c))
                return std::nullopt;
            have_number = true;
            continue;
        }
        if (!have_number)
            return std::nullopt;

        switch (std::tolower(static_cast<unsigned char>(c))) {
        case 'h': total += number * 3600; break;
        case 'm': total += number * 60; break;
        case 's': total += number; break;
        default: return std::nullopt;
        }
        if (total > INT_MAX)
            return std::nullopt;
        number = 0;
        have_number = false;
    }

    total += number;
    if (total > INT_MAX)
        return std::nullopt;
    return static_cast<int>(total);
}

static std::optional<int> parseColons(const std::string& text)
{
    std::vector<long long> parts{0};
    bool have_number = false;

    for (char c : text) {
        if (c == ':') {
            if (!have_number)
                return std::nullopt;
            parts.push_back(0);
            have_number = false;
        } else if (isDigit(c)) {
            if (!addDigit(parts.back(), c))
                return std::nullopt;
            have_number = true;
        } else {
            return std::nullopt;
        }
    }
    if (!have_number || parts.size() > 3)
        return std::nullopt;

    long long total = 0;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0 && parts[i] >= 60)
            return std::nullopt;
        total = total * 60 + parts[i];
        if (total > INT_MAX)
            return std::nullopt;
    }
    return static_cast<int>(total);
}

std::optional<int> parse(const std::string& text)
{
    std::string clean = trim(text);
    if (clean.empty())
        return std::nullopt;

    for (char c : clean) {
        if (std::isalpha(static_cast<unsigned char>(c)))
            return parseUnits(clean);
    }
    return parseColons(clean);
}

std::string format(int seconds)
{
    if (seconds < 0)
        seconds = 0;

    int h = seconds / 3600;
    int m = seconds / 60 % 60;
    int s = seconds % 60;

    char buffer[32];
    if (h > 0)
        std::snprintf(buffer, sizeof buffer, "%d:%02d:%02d", h, m, s);
    else
        std::snprintf(buffer, sizeof buffer, "%d:%02d", m, s);
    return buffer;
}

}
