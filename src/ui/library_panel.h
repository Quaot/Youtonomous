#pragma once

#include <string>
#include <vector>

#include <windows.h>

#include "controls.h"
#include "library.h"

class LibraryPanel {
public:
    void create(const ui::Context& context);
    void layout(int height);
    void setVisible(bool visible);

    std::string url() const;
    void clearUrl();
    bool isUrlBox(HWND window) const { return window == url_edit_; }

    void showProgress(int percent);
    void showStatus(const std::wstring& text);
    void setDownloading(bool downloading);

    void showVideos(const std::vector<Video>& videos);
    void select(int index);
    int selected(size_t count) const;

private:
    HWND url_edit_ = nullptr;
    HWND download_button_ = nullptr;
    HWND progress_ = nullptr;
    HWND status_ = nullptr;
    HWND label_ = nullptr;
    HWND list_ = nullptr;
    HWND open_button_ = nullptr;
    HWND remove_button_ = nullptr;
};
