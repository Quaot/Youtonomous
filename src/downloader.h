#pragma once

#include <filesystem>
#include <string>

#include <windows.h>

#include "library.h"

const UINT WM_DOWNLOAD_PROGRESS = WM_APP + 1;
const UINT WM_DOWNLOAD_DONE = WM_APP + 2;

struct DownloadResult {
    bool ok = false;
    std::string error;
    Video video;
};

// Posts WM_DOWNLOAD_PROGRESS (wParam = percent) while running, then
// WM_DOWNLOAD_DONE (lParam = DownloadResult*, which the receiver deletes).
void startDownload(HWND notify, const std::string& url, const std::filesystem::path& folder);
