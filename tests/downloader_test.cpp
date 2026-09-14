#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include <windows.h>

#include "check.h"
#include "downloader.h"

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

struct Outcome {
    int done = 0;
    int nullResults = 0;
    std::vector<DownloadResult> results;
    std::vector<WPARAM> progress;
    int progressAfterAllDone = 0;
};

static Outcome collect(HWND window, int expectedDone)
{
    Outcome outcome;
    Clock::time_point end = Clock::now() + std::chrono::seconds(60);
    while (Clock::now() < end) {
        DWORD remaining = static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(end - Clock::now()).count());
        MsgWaitForMultipleObjects(0, nullptr, FALSE, remaining, QS_ALLINPUT);
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.hwnd == window && msg.message == WM_DOWNLOAD_PROGRESS) {
                outcome.progress.push_back(msg.wParam);
                if (outcome.done >= expectedDone)
                    ++outcome.progressAfterAllDone;
            } else if (msg.hwnd == window && msg.message == WM_DOWNLOAD_DONE) {
                DownloadResult* result = reinterpret_cast<DownloadResult*>(msg.lParam);
                ++outcome.done;
                if (result)
                    outcome.results.push_back(*result);
                else
                    ++outcome.nullResults;
                delete result;
                if (outcome.done == expectedDone)
                    end = Clock::now() + std::chrono::seconds(3);
            } else {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
    }
    return outcome;
}

static void expectFailures(const Outcome& outcome, int expectedDone, const std::string& name)
{
    check(outcome.done == expectedDone, name + ": exactly " + std::to_string(expectedDone) + " WM_DOWNLOAD_DONE within 60 s, got " + std::to_string(outcome.done));
    check(outcome.nullResults == 0, name + ": WM_DOWNLOAD_DONE carries a DownloadResult");
    for (const DownloadResult& result : outcome.results) {
        check(!result.ok, name + ": result is not ok");
        check(!result.error.empty(), name + ": result has an error message");
    }
    WPARAM highest = 0;
    for (WPARAM percent : outcome.progress)
        if (percent > highest)
            highest = percent;
    check(highest <= 100, name + ": progress stays within 0..100, highest " + std::to_string(static_cast<long long>(highest)));
    check(outcome.progressAfterAllDone == 0, name + ": no progress after WM_DOWNLOAD_DONE, got " + std::to_string(outcome.progressAfterAllDone));
}

static void testFailure(HWND window, const fs::path& folder, const std::string& url, const std::string& name)
{
    startDownload(window, url, folder);
    expectFailures(collect(window, 1), 1, name);
}

int main()
{
    wchar_t found[MAX_PATH];
    if (SearchPathW(nullptr, L"yt-dlp.exe", nullptr, MAX_PATH, found, nullptr) == 0) {
        std::printf("SKIP: yt-dlp.exe not found on PATH\n");
        return 77;
    }

    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW windowClass = {};
    windowClass.lpfnWndProc = DefWindowProcW;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = L"DownloaderTestWindow";
    RegisterClassW(&windowClass);
    HWND window = CreateWindowExW(0, L"DownloaderTestWindow", L"downloader test", WS_OVERLAPPEDWINDOW,
                                  0, 0, 100, 100, nullptr, nullptr, instance, nullptr);
    check(window != nullptr, "hidden window is created");
    if (!window)
        return report();

    fs::path root = fs::temp_directory_path() / ("youtonomous-downloader-test-" + std::to_string(GetCurrentProcessId()));
    fs::path folder = root / "videos";
    std::error_code error;
    fs::remove_all(root, error);
    fs::create_directories(folder);

    testFailure(window, folder, "not a url", "\"not a url\"");
    testFailure(window, folder, "https://invalid.invalid/video", "\"https://invalid.invalid/video\"");
    testFailure(window, folder, "", "empty url");

    fs::path marker = root / "injected.txt";
    std::string injection = "https://invalid.invalid/\" & echo injected > \"" + marker.u8string() + "\" & \"";
    testFailure(window, folder, injection, "url with shell metacharacters");
    Sleep(1000);
    check(!fs::exists(marker), "a url with shell metacharacters does not run a command");

    startDownload(window, "not a url", folder);
    startDownload(window, "https://invalid.invalid/other", folder);
    expectFailures(collect(window, 2), 2, "two downloads at once");

    DestroyWindow(window);
    fs::remove_all(root, error);
    return report();
}
