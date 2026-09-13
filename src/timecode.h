#pragma once

#include <optional>
#include <string>

namespace timecode {

std::optional<int> parse(const std::string& text);
std::string format(int seconds);

}
