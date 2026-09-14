#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

#include <windows.h>

#include <nlohmann/json.hpp>

#include "check.h"
#include "timecode.h"

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

struct Search {
    DWORD pid = 0;
    const wchar_t* className = nullptr;
    std::vector<HWND> found;
};

static bool waitFor(const std::function<bool()>& condition, int milliseconds)
{
    Clock::time_point end = Clock::now() + std::chrono::milliseconds(milliseconds);
    while (!condition()) {
        if (Clock::now() >= end)
            return false;
        Sleep(100);
    }
    return true;
}

static std::wstring environmentWith(const fs::path& home)
{
    std::wstring block;
    wchar_t* strings = GetEnvironmentStringsW();
    for (wchar_t* entry = strings; *entry; entry += wcslen(entry) + 1) {
        bool replaced = _wcsnicmp(entry, L"APPDATA=", 8) == 0 || _wcsnicmp(entry, L"USERPROFILE=", 12) == 0;
        if (!replaced)
            block += std::wstring(entry) + L'\0';
    }
    FreeEnvironmentStringsW(strings);
    block += L"APPDATA=" + (home / "AppData").wstring() + L'\0';
    block += L"USERPROFILE=" + home.wstring() + L'\0';
    return block + L'\0';
}

static bool launch(std::wstring commandLine, std::wstring* environment, PROCESS_INFORMATION& process)
{
    STARTUPINFOW startup{};
    startup.cb = sizeof startup;
    DWORD flags = CREATE_UNICODE_ENVIRONMENT;
    if (!environment)
        flags |= CREATE_NO_WINDOW;
    LPVOID block = environment ? environment->data() : nullptr;
    BOOL started = CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE, flags, block, nullptr, &startup, &process);
    return started != 0;
}

static bool runToEnd(const std::wstring& commandLine)
{
    PROCESS_INFORMATION process{};
    if (!launch(commandLine, nullptr, process))
        return false;
    bool finished = WaitForSingleObject(process.hProcess, 120000) == WAIT_OBJECT_0;
    DWORD code = 1;
    if (finished)
        GetExitCodeProcess(process.hProcess, &code);
    else
        TerminateProcess(process.hProcess, 1);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return finished && code == 0;
}

static bool makeClip(const fs::path& file)
{
    wchar_t ffmpeg[MAX_PATH];
    if (SearchPathW(nullptr, L"ffmpeg.exe", nullptr, MAX_PATH, ffmpeg, nullptr) == 0)
        return false;

    std::wstring command = L"\"" + std::wstring(ffmpeg) + L"\" -hide_banner -loglevel error -y";
    command += L" -f lavfi -i testsrc=duration=40:size=320x240:rate=25";
    command += L" -f lavfi -i sine=frequency=440:duration=40";
    command += L" -af volume=0 -c:v mpeg4 -c:a aac -shortest";
    command += L" \"" + file.wstring() + L"\"";
    return runToEnd(command) && fs::exists(file);
}

static BOOL CALLBACK findApp(HWND hwnd, LPARAM param)
{
    auto* search = reinterpret_cast<Search*>(param);
    wchar_t name[64];
    DWORD owner = 0;
    GetWindowThreadProcessId(hwnd, &owner);
    if (owner == search->pid && GetClassNameW(hwnd, name, 64) && wcscmp(name, L"Youtonomous") == 0) {
        search->found.push_back(hwnd);
        return FALSE;
    }
    return TRUE;
}

static BOOL CALLBACK findChild(HWND hwnd, LPARAM param)
{
    auto* search = reinterpret_cast<Search*>(param);
    wchar_t name[64];
    if (GetClassNameW(hwnd, name, 64) && _wcsicmp(name, search->className) == 0)
        search->found.push_back(hwnd);
    return TRUE;
}

static HWND appWindow(DWORD pid)
{
    Search search;
    search.pid = pid;
    EnumWindows(findApp, reinterpret_cast<LPARAM>(&search));
    return search.found.empty() ? nullptr : search.found[0];
}

static std::vector<HWND> children(HWND top, const wchar_t* className)
{
    Search search;
    search.className = className;
    EnumChildWindows(top, findChild, reinterpret_cast<LPARAM>(&search));
    return search.found;
}

static std::wstring textOf(HWND hwnd)
{
    wchar_t buffer[512] = L"";
    SendMessageTimeoutW(hwnd, WM_GETTEXT, 512, reinterpret_cast<LPARAM>(buffer), SMTO_ABORTIFHUNG, 2000, nullptr);
    return buffer;
}

static HWND button(HWND top, const std::wstring& caption)
{
    for (HWND hwnd : children(top, L"Button")) {
        if (textOf(hwnd) == caption)
            return hwnd;
    }
    return nullptr;
}

static bool click(HWND top, const std::wstring& caption)
{
    HWND hwnd = button(top, caption);
    if (!hwnd)
        return false;
    SendMessageTimeoutW(hwnd, BM_CLICK, 0, 0, SMTO_ABORTIFHUNG, 2000, nullptr);
    return true;
}

static void setText(HWND hwnd, const std::wstring& text)
{
    SendMessageTimeoutW(hwnd, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(text.c_str()), SMTO_ABORTIFHUNG, 2000, nullptr);
}

static void key(HWND top, WPARAM vk)
{
    PostMessageW(top, WM_KEYDOWN, vk, 0);
}

static int playbackTime(HWND top)
{
    for (HWND hwnd : children(top, L"Static")) {
        std::wstring text = textOf(hwnd);
        size_t slash = text.find(L" / ");
        if (slash == std::wstring::npos)
            continue;
        std::optional<int> seconds = timecode::parse(std::string(text.begin(), text.begin() + slash));
        return seconds ? *seconds : -1;
    }
    return -1;
}

static int markCount(HWND top)
{
    std::vector<HWND> lists = children(top, L"ListBox");
    if (lists.size() < 2)
        return -1;
    return static_cast<int>(SendMessageW(lists[1], LB_GETCOUNT, 0, 0));
}

static nlohmann::json savedVideo(const fs::path& libraryFile)
{
    std::ifstream in(libraryFile);
    nlohmann::json items = nlohmann::json::parse(in, nullptr, false);
    if (items.is_array() && !items.empty())
        return items[0];
    return nlohmann::json::object();
}

static bool hasMark(const nlohmann::json& video, const std::string& label)
{
    for (const nlohmann::json& mark : video.value("marks", nlohmann::json::array())) {
        if (mark.value("label", "") == label)
            return true;
    }
    return false;
}

static bool closeApp(PROCESS_INFORMATION& process, HWND top)
{
    PostMessageW(top, WM_CLOSE, 0, 0);
    bool closed = WaitForSingleObject(process.hProcess, 10000) == WAIT_OBJECT_0;
    if (!closed)
        TerminateProcess(process.hProcess, 1);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return closed;
}

static bool within(int value, int low, int high)
{
    return value >= low && value <= high;
}

static std::string got(int value)
{
    return ", got " + std::to_string(value);
}

static void clickSeekBar(HWND top, int numerator, int denominator)
{
    std::vector<HWND> bars = children(top, L"YoutonomousSeekBar");
    if (bars.empty())
        return;
    RECT rect;
    GetClientRect(bars[0], &rect);
    LPARAM point = MAKELPARAM(rect.right * numerator / denominator, rect.bottom / 2);
    SendMessageW(bars[0], WM_LBUTTONDOWN, MK_LBUTTON, point);
    SendMessageW(bars[0], WM_LBUTTONUP, 0, point);
}

static void firstRun(const std::wstring& commandLine, std::wstring& environment, const fs::path& libraryFile)
{
    PROCESS_INFORMATION process{};
    check(launch(commandLine, &environment, process), "app starts");
    HWND top = nullptr;
    check(waitFor([&] { return (top = appWindow(process.dwProcessId)) != nullptr; }, 10000), "main window appears");
    if (!top)
        return;

    check(waitFor([&] { return playbackTime(top) >= 1; }, 15000), "the clip plays");
    check(waitFor([&] { return click(top, L"Pause"); }, 5000), "Pause button is shown while playing");
    check(waitFor([&] { return button(top, L"Play") != nullptr; }, 5000), "Play button is shown after pausing");

    int paused = playbackTime(top);
    click(top, L"+10");
    check(waitFor([&] { return within(playbackTime(top), paused + 9, paused + 11); }, 5000), "+10 moves forward 10 s" + got(playbackTime(top)));
    click(top, L"-10");
    check(waitFor([&] { return within(playbackTime(top), paused - 1, paused + 1); }, 5000), "-10 moves back 10 s" + got(playbackTime(top)));

    check(children(top, L"YoutonomousSeekBar").size() == 1, "one seek bar is shown");
    clickSeekBar(top, 3, 4);
    check(waitFor([&] { return within(playbackTime(top), 28, 32); }, 5000), "clicking three quarters along the bar seeks to about 30 s" + got(playbackTime(top)));

    int before = markCount(top);
    key(top, 'B');
    check(waitFor([&] { return markCount(top) == before + 1; }, 3000), "B adds a bookmark" + got(markCount(top)));

    std::vector<HWND> edits = children(top, L"Edit");
    check(edits.size() >= 3, "URL, start and bookmark label boxes exist");
    if (edits.size() >= 3) {
        setText(edits[2], L"Smoke mark");
        click(top, L"Add");
        check(waitFor([&] { return hasMark(savedVideo(libraryFile), "Smoke mark"); }, 3000), "Add saves a labelled bookmark");

        setText(edits[1], L"0:20");
        click(top, L"Set");
        check(waitFor([&] { return savedVideo(libraryFile).value("start", -1) == 20; }, 3000), "Set saves the start point");
    }

    key(top, VK_HOME);
    check(waitFor([&] { return within(playbackTime(top), 20, 21); }, 5000), "Home goes to the start point" + got(playbackTime(top)));
    key(top, VK_RIGHT);
    check(waitFor([&] { return within(playbackTime(top), 30, 31); }, 5000), "Right arrow moves forward 10 s" + got(playbackTime(top)));

    check(closeApp(process, top), "app closes cleanly");
}

static void secondRun(const std::wstring& commandLine, std::wstring& environment)
{
    PROCESS_INFORMATION process{};
    check(launch(commandLine, &environment, process), "app starts again");
    HWND top = nullptr;
    check(waitFor([&] { return (top = appWindow(process.dwProcessId)) != nullptr; }, 10000), "main window appears again");
    if (!top)
        return;

    int first = -1;
    waitFor([&] { return (first = playbackTime(top)) >= 1; }, 15000);
    check(within(first, 19, 25), "reopened video starts at the saved start point" + got(first));
    check(waitFor([&] { return markCount(top) >= 2; }, 3000), "bookmarks survive a restart" + got(markCount(top)));
    check(closeApp(process, top), "app closes cleanly again");
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: ui_smoke_test <path to youtonomous.exe>\n");
        return 2;
    }

    fs::path root = fs::temp_directory_path() / ("youtonomous-ui-test-" + std::to_string(GetCurrentProcessId()));
    std::error_code error;
    fs::remove_all(root, error);
    fs::create_directories(root / "AppData");

    fs::path clip = root / "clip.mp4";
    if (!makeClip(clip)) {
        std::printf("SKIP: ffmpeg not found or could not make a test clip\n");
        fs::remove_all(root, error);
        return 77;
    }

    std::wstring commandLine = L"\"" + fs::u8path(argv[1]).wstring() + L"\" \"" + clip.wstring() + L"\"";
    std::wstring environment = environmentWith(root);
    fs::path libraryFile = root / "AppData" / "Youtonomous" / "library.json";

    firstRun(commandLine, environment, libraryFile);
    secondRun(commandLine, environment);

    fs::remove_all(root, error);
    return report();
}
