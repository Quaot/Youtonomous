#pragma once

#include <filesystem>
#include <string>

#include <windows.h>

#include "downloader.h"
#include "library.h"
#include "player.h"

class MainWindow {
public:
    MainWindow();

    bool create(HINSTANCE instance, int show);
    void openPath(const std::wstring& path);

private:
    static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handle(UINT msg, WPARAM wParam, LPARAM lParam);

    HWND addControl(const wchar_t* type, const wchar_t* text, DWORD style, int id, DWORD exStyle = 0);
    void createControls();
    void layout(int width, int height);
    void onCommand(int id, int code);
    void tick();

    void openFile();
    void download();
    void onDownloadDone(DownloadResult* result);
    void refreshLibrary();
    void openVideo(const std::string& id);
    void removeSelected();

    HWND hwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    HFONT font_ = nullptr;

    HWND urlEdit_ = nullptr;
    HWND downloadButton_ = nullptr;
    HWND progress_ = nullptr;
    HWND status_ = nullptr;
    HWND libraryLabel_ = nullptr;
    HWND libraryList_ = nullptr;
    HWND openButton_ = nullptr;
    HWND removeButton_ = nullptr;

    HWND video_ = nullptr;
    HWND seekbar_ = nullptr;
    HWND back30_ = nullptr;
    HWND back10_ = nullptr;
    HWND playButton_ = nullptr;
    HWND forward10_ = nullptr;
    HWND forward30_ = nullptr;
    HWND timeLabel_ = nullptr;

    std::filesystem::path videosFolder_;
    Library library_;
    Player player_;
    std::string currentId_;
    bool downloading_ = false;
};
