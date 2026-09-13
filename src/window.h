#pragma once

#include <windows.h>

class MainWindow {
public:
    bool create(HINSTANCE instance, int show);

private:
    static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handle(UINT msg, WPARAM wParam, LPARAM lParam);

    HWND hwnd_ = nullptr;
};
