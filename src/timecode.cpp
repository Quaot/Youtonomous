#include "timecode.h"

#include <cctype>
#include <cstdio>
#include <vector>

namespace timecode {

static const int kMaxNumber = 1000000;

static bool isDigit(char c)
{
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

static std::string trim(const std::string& text)
{
    size_t first = text.find_first_not_of(" \t");
    if (first == std::string::npos)
        return "";
    size_t last = text.find_last_not_of(" \t");
    return text.substr(first, last - first + 1);
}

static std::optional<int> parseUnits(const std::string& text)
{
    int total = 0;
    int number = 0;
    bool haveNumber = false;

    for (char c : text) {
        if (isDigit(c)) {
            number = number * 10 + (c - '0');
            if (number > kMaxNumber)
                return std::nullopt;
            haveNumber = true;
            continue;
        }
        if (!haveNumber)
            return std::nullopt;

        switch (std::tolower(static_cast<unsigned char>(c))) {
        case 'h': total += number * 3600; break;
        case 'm': total += number * 60; break;
        case 's': total += number; break;
        default: return std::nullopt;
        }
        number = 0;
        haveNumber = false;
    }
    return total + number;
}

static std::optional<int> parseColons(const std::string& text)
{
    std::vector<int> parts{0};
    bool haveNumber = false;

    for (char c : text) {
        if (c == ':') {
            if (!haveNumber)
                return std::nullopt;
            parts.push_back(0);
            haveNumber = false;
        } else if (isDigit(c)) {
            parts.back() = parts.back() * 10 + (c - '0');
            if (parts.back() > kMaxNumber)
                return std::nullopt;
            haveNumber = true;
        } else {
            return std::nullopt;
        }
    }
    if (!haveNumber || parts.size() > 3)
        return std::nullopt;

    int total = 0;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0 && parts[i] >= 60)
            return std::nullopt;
        total = total * 60 + parts[i];
    }
    return total;
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
