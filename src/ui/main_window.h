#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include <windows.h>

#include "downloader.h"
#include "library.h"
#include "library_panel.h"
#include "make_player.h"
#include "marks_panel.h"
#include "player_panel.h"

class MainWindow {
public:
    MainWindow();

    bool create(HINSTANCE instance, int show);
    void openPath(const std::wstring& path);
    bool handleKey(const MSG& msg);

private:
    static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handle(UINT msg, WPARAM wParam, LPARAM lParam);

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

    Video* current();
    void refreshMarks();
    void addMark();
    void deleteSelectedMark();
    void jumpToSelectedMark();
    void jumpMark(int direction);

    void saveStartFromBox();
    void setStart(int seconds);
    void goToStart();

    HWND hwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    HFONT font_ = nullptr;

    LibraryPanel libraryPanel_;
    PlayerPanel playerPanel_;
    MarksPanel marksPanel_;

    std::filesystem::path videosFolder_;
    Library library_;
    std::unique_ptr<Player> player_ = makePlayer("vlc");
    std::string currentId_;
    bool downloading_ = false;
};
