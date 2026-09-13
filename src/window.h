#pragma once

#include <string>

#include <windows.h>

#include "player.h"

class MainWindow {
public:
    bool create(HINSTANCE instance, int show);
    void openPath(const std::wstring& path);

private:
    static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handle(UINT msg, WPARAM wParam, LPARAM lParam);

    HWND addControl(const wchar_t* type, const wchar_t* text, DWORD style, int id);
    void createControls();
    void layout(int width, int height);
    void onCommand(int id);

    void openFile();

    HWND hwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    HFONT font_ = nullptr;

    HWND openButton_ = nullptr;
    HWND video_ = nullptr;

    Player player_;
};
