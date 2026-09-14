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
#include "settings.h"

class MainWindow {
public:
    MainWindow();

    bool create(HINSTANCE instance, int show);
    void openPath(const std::wstring& path);
    bool handleKey(const MSG& msg);

private:
    static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param);
    LRESULT handle(UINT msg, WPARAM w_param, LPARAM l_param);

    void createControls();
    void layout(int width, int height);
    void onCommand(int id, int code);
    void tick();
    void saveSettingsOnClose();
    void setFullscreen(bool fullscreen);

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

    void changeSpeed(double step);
    void rememberPosition();
    void resume();

    HWND hwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    HFONT font_ = nullptr;

    LibraryPanel library_panel_;
    PlayerPanel player_panel_;
    MarksPanel marks_panel_;

    bool fullscreen_ = false;
    WINDOWPLACEMENT saved_placement_{};
    LONG_PTR saved_style_ = 0;

    std::filesystem::path settings_file_;
    Settings settings_;
    std::filesystem::path videos_folder_;
    Library library_;
    std::unique_ptr<Player> player_;
    std::string current_id_;
    bool downloading_ = false;
};
