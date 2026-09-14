#pragma once

#include <filesystem>
#include <string>

struct Settings {
    std::string backend = "vlc";
    std::string videos_folder;
    int window_x = -1;
    int window_y = -1;
    int window_width = 1280;
    int window_height = 760;
    bool maximized = false;
    int volume = 100;
    double speed = 1.0;
};

Settings loadSettings(const std::filesystem::path& file);
bool saveSettings(const Settings& settings, const std::filesystem::path& file);
