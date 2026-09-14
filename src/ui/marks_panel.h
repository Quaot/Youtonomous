#pragma once

#include <string>

#include <windows.h>

#include "controls.h"
#include "library.h"

class MarksPanel {
public:
    void create(const ui::Context& context);
    void layout(int width, int height);

    void show(const Video* video);
    void showStart(int seconds);
    std::string startText() const;
    std::string takeLabel();
    int selected(size_t count) const;

    bool isStartBox(HWND window) const { return window == startEdit_; }
    bool isLabelBox(HWND window) const { return window == labelEdit_; }

private:
    HWND startLabel_ = nullptr;
    HWND startEdit_ = nullptr;
    HWND setStartButton_ = nullptr;
    HWND goStartButton_ = nullptr;

    HWND marksLabel_ = nullptr;
    HWND list_ = nullptr;
    HWND labelEdit_ = nullptr;
    HWND addButton_ = nullptr;
    HWND deleteButton_ = nullptr;
};
