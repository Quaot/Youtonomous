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

    std::string url() const;
    void clearUrl();
    bool isUrlBox(HWND window) const { return window == urlEdit_; }

    void showProgress(int percent);
    void showStatus(const std::wstring& text);
    void setDownloading(bool downloading);

    void showVideos(const std::vector<Video>& videos);
    void select(int index);
    int selected(size_t count) const;

private:
    HWND urlEdit_ = nullptr;
    HWND downloadButton_ = nullptr;
    HWND progress_ = nullptr;
    HWND status_ = nullptr;
    HWND label_ = nullptr;
    HWND list_ = nullptr;
    HWND openButton_ = nullptr;
    HWND removeButton_ = nullptr;
};
