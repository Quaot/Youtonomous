#pragma once

#include <vector>

#include <windows.h>

#include "controls.h"
#include "library.h"

class PlayerPanel {
public:
    void create(const ui::Context& context);
    void layout(int width, int height);

    HWND videoWindow() const { return video_; }
    void show(int time, int length, bool playing);
    void showMarks(const std::vector<Bookmark>& marks, int start);

private:
    HWND video_ = nullptr;
    HWND seekbar_ = nullptr;
    HWND prev_mark_ = nullptr;
    HWND back30_ = nullptr;
    HWND back10_ = nullptr;
    HWND play_button_ = nullptr;
    HWND forward10_ = nullptr;
    HWND forward30_ = nullptr;
    HWND next_mark_ = nullptr;
    HWND time_label_ = nullptr;
};
