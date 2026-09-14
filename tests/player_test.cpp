#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <system_error>

#include <windows.h>

#include "check.h"
#include "player.h"

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

static HWND videoWindow;
static std::string clip;

static DWORD WINAPI windowThread(LPVOID created)
{
    videoWindow = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 320, 240, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    SetEvent(static_cast<HANDLE>(created));
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (videoWindow)
        DestroyWindow(videoWindow);
    return 0;
}

static bool waitFor(const std::function<bool()>& condition, int milliseconds)
{
    Clock::time_point end = Clock::now() + std::chrono::milliseconds(milliseconds);
    while (true) {
        if (condition())
            return true;
        if (Clock::now() >= end)
            return false;
        Sleep(50);
    }
}

static void watch(const std::function<void()>& sample, int milliseconds)
{
    waitFor([&] { sample(); return false; }, milliseconds);
}

static std::string num(int value)
{
    return std::to_string(value);
}

static bool runProcess(const std::wstring& commandLine, int milliseconds)
{
    STARTUPINFOW startup = {};
    startup.cb = sizeof startup;
    PROCESS_INFORMATION process = {};
    std::wstring buffer = commandLine;
    if (!CreateProcessW(nullptr, buffer.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process))
        return false;
    bool finished = WaitForSingleObject(process.hProcess, milliseconds) == WAIT_OBJECT_0;
    DWORD code = 1;
    if (finished)
        GetExitCodeProcess(process.hProcess, &code);
    else
        TerminateProcess(process.hProcess, 1);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return finished && code == 0;
}

static bool makeClip(const fs::path& folder)
{
    wchar_t ffmpeg[MAX_PATH];
    if (SearchPathW(nullptr, L"ffmpeg.exe", nullptr, MAX_PATH, ffmpeg, nullptr) == 0)
        return false;
    fs::path file = folder / "clip.mp4";
    std::wstring command = L"\"" + std::wstring(ffmpeg) + L"\" -hide_banner -loglevel error -y"
        L" -f lavfi -i testsrc=duration=30:size=320x240:rate=25"
        L" -f lavfi -i sine=frequency=440:duration=30"
        L" -af volume=0 -c:v mpeg4 -c:a aac -shortest \"" + file.wstring() + L"\"";
    if (!runProcess(command, 120000) || !fs::exists(file))
        return false;
    clip = file.u8string();
    return true;
}

static void testNothingOpen()
{
    Player player;
    player.attach(videoWindow);
    check(!player.loaded(), "loaded() is false before open");
    check(!player.playing(), "playing() is false before open");
    check(player.time() == 0, "time() is 0 before open, got " + num(player.time()));
    check(player.length() == 0, "length() is 0 before open, got " + num(player.length()));

    player.seek(10);
    player.seek(-10);
    player.skip(5);
    player.skip(-5);
    player.togglePause();
    Sleep(300);
    check(!player.loaded(), "loaded() stays false after seek, skip and togglePause with nothing open");
    check(!player.playing(), "playing() stays false after togglePause with nothing open");
    check(player.time() == 0, "time() stays 0 after seek and skip with nothing open, got " + num(player.time()));
    check(player.length() == 0, "length() stays 0 after seek and skip with nothing open, got " + num(player.length()));
    player.togglePause();
    check(!player.playing(), "playing() stays false after a second togglePause with nothing open");
}

static void testMissingFile(const fs::path& folder)
{
    Player player;
    player.attach(videoWindow);
    player.open((folder / "does-not-exist" / "missing.mp4").u8string(), 0);
    check(waitFor([&] { return !player.playing(); }, 5000), "a non-existent file does not end up playing");
    int lowest = 0;
    watch([&] { lowest = std::min(lowest, std::min(player.time(), player.length())); }, 1000);
    check(lowest >= 0, "time() and length() are never negative for a non-existent file, lowest " + num(lowest));
    player.seek(10);
    player.skip(5);
    player.skip(-5);
    player.togglePause();
    player.togglePause();
    Sleep(500);
    check(player.time() >= 0, "seek, skip and togglePause after opening a non-existent file do not crash");

    player.open("", 0);
    Sleep(500);
    check(player.time() >= 0 && player.length() >= 0, "opening an empty path does not crash");
}

static bool startPlaying(Player& player, int start)
{
    player.attach(videoWindow);
    player.open(clip, start);
    return waitFor([&] { return player.playing() && player.length() > 0; }, 10000);
}

static void testOpen()
{
    Player player;
    Clock::time_point opened = Clock::now();
    player.attach(videoWindow);
    player.open(clip, 0);
    check(player.loaded(), "loaded() is true after open");
    check(waitFor([&] { return player.playing(); }, 10000), "playing() becomes true after open");
    check(waitFor([&] { return player.length() > 0; }, 10000), "length() becomes known after open");
    check(player.length() >= 29 && player.length() <= 31, "length() is about 30 for a 30 s clip, got " + num(player.length()));
    int elapsed = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - opened).count());
    int time = player.time();
    check(time >= 0 && time <= elapsed + 1, "open at 0 starts near 0, got time " + num(time) + " after " + num(elapsed) + " s");
    int before = player.time();
    check(waitFor([&] { return player.time() >= before + 1; }, 4000), "time() advances while playing, stuck at " + num(player.time()));
}

static void testOpenAtStart()
{
    Player player;
    Clock::time_point opened = Clock::now();
    player.attach(videoWindow);
    player.open(clip, 10);
    bool reached = waitFor([&] { return player.playing() && player.time() >= 10; }, 10000);
    int elapsed = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - opened).count());
    int time = player.time();
    check(reached, "open at 10 plays with time() >= 10, got " + num(time));
    check(time >= 10 && time < 20, "open at 10 gives a time well below 30, got " + num(time));
    check(time > elapsed, "open at 10 starts at 10 rather than playing up to it, time " + num(time) + " after " + num(elapsed) + " s");
    check(player.loaded(), "loaded() is true after open at 10");
}

static void testSeek()
{
    Player player;
    check(startPlaying(player, 0), "clip plays for the seek tests");

    player.seek(20);
    check(waitFor([&] { int t = player.time(); return t >= 20 && t <= 23; }, 5000), "seek(20) jumps to 20, got " + num(player.time()));
    player.seek(5);
    check(waitFor([&] { int t = player.time(); return t >= 5 && t <= 8; }, 5000), "seek(5) jumps back to 5, got " + num(player.time()));
    player.seek(0);
    check(waitFor([&] { return player.time() <= 3; }, 5000), "seek(0) jumps to the beginning, got " + num(player.time()));

    player.seek(15);
    waitFor([&] { return player.time() >= 15; }, 5000);
    int lowest = player.time();
    player.seek(-5);
    bool back = waitFor([&] { lowest = std::min(lowest, player.time()); return player.time() <= 3; }, 5000);
    watch([&] { lowest = std::min(lowest, player.time()); }, 1000);
    check(back, "seek(-5) goes to the beginning, got " + num(player.time()));
    check(lowest >= 0, "time() is never negative after seek(-5), lowest " + num(lowest));

    int length = player.length();
    int highest = 0;
    lowest = 0;
    player.seek(1000);
    watch([&] { highest = std::max(highest, player.time()); lowest = std::min(lowest, player.time()); }, 3000);
    check(highest <= length && lowest >= 0, "seek(1000) keeps time() inside 0.." + num(length) + ", saw " + num(lowest) + ".." + num(highest));
}

static void testSkip()
{
    Player player;
    check(startPlaying(player, 10), "clip plays for the skip tests");
    waitFor([&] { return player.time() >= 10; }, 5000);

    int before = player.time();
    player.skip(10);
    check(waitFor([&] { int t = player.time(); return t >= before + 9 && t <= before + 14; }, 5000),
          "skip(10) from " + num(before) + " moves forward 10, got " + num(player.time()));

    before = player.time();
    player.skip(-5);
    check(waitFor([&] { int t = player.time(); return t >= before - 6 && t <= before - 2; }, 5000),
          "skip(-5) from " + num(before) + " moves back 5, got " + num(player.time()));

    int lowest = player.time();
    player.skip(-1000);
    bool back = waitFor([&] { lowest = std::min(lowest, player.time()); return player.time() <= 3; }, 5000);
    watch([&] { lowest = std::min(lowest, player.time()); }, 1000);
    check(back, "skip(-1000) goes to the beginning, got " + num(player.time()));
    check(lowest >= 0, "time() is never negative after skip(-1000), lowest " + num(lowest));

    int length = player.length();
    int highest = 0;
    lowest = 0;
    player.skip(1000);
    watch([&] { highest = std::max(highest, player.time()); lowest = std::min(lowest, player.time()); }, 3000);
    check(highest <= length && lowest >= 0, "skip(1000) keeps time() inside 0.." + num(length) + ", saw " + num(lowest) + ".." + num(highest));
}

static void testPause()
{
    Player player;
    check(startPlaying(player, 0), "clip plays for the pause tests");
    waitFor([&] { return player.time() >= 1; }, 5000);

    player.togglePause();
    check(waitFor([&] { return !player.playing(); }, 5000), "togglePause() while playing pauses");
    Sleep(300);
    int paused = player.time();
    bool still = true;
    watch([&] { if (player.time() != paused || player.playing()) still = false; }, 1500);
    check(still, "time() does not advance while paused, was " + num(paused) + " now " + num(player.time()));

    player.togglePause();
    check(waitFor([&] { return player.playing(); }, 5000), "togglePause() while paused resumes");
    check(waitFor([&] { return player.time() >= paused + 1; }, 4000), "time() advances again after resuming, stuck at " + num(player.time()));
}

static void testReopen(const fs::path& folder)
{
    Player player;
    player.attach(videoWindow);
    player.open((folder / "missing.mp4").u8string(), 0);
    Sleep(1000);
    player.open(clip, 0);
    check(waitFor([&] { return player.playing() && player.length() > 0; }, 10000), "a real clip plays after a failed open");

    player.seek(20);
    waitFor([&] { return player.time() >= 20; }, 5000);
    player.open(clip, 5);
    check(waitFor([&] { int t = player.time(); return player.playing() && t >= 5 && t <= 12; }, 10000),
          "opening the clip again at 5 restarts at 5, got " + num(player.time()));
}

int main()
{
    HANDLE created = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    DWORD threadId = 0;
    HANDLE thread = CreateThread(nullptr, 0, windowThread, created, 0, &threadId);
    WaitForSingleObject(created, 10000);
    CloseHandle(created);

    fs::path folder = fs::temp_directory_path() / ("youtonomous-player-test-" + std::to_string(GetCurrentProcessId()));
    std::error_code error;
    fs::remove_all(folder, error);
    fs::create_directories(folder);

    int result = 0;
    testNothingOpen();
    bool ready = false;
    {
        Player player;
        ready = player.ready();
    }
    if (!ready) {
        std::printf("SKIP: VLC did not start\n");
        result = failCount > 0 ? report() : 77;
    } else {
        testMissingFile(folder);
        if (!makeClip(folder)) {
            std::printf("SKIP: ffmpeg not found or could not make a test clip\n");
            result = failCount > 0 ? report() : 77;
        } else {
            testOpen();
            testOpenAtStart();
            testSeek();
            testSkip();
            testPause();
            testReopen(folder);
            result = report();
        }
    }

    PostThreadMessageW(threadId, WM_QUIT, 0, 0);
    WaitForSingleObject(thread, 5000);
    CloseHandle(thread);
    fs::remove_all(folder, error);
    return result;
}
