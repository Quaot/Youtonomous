#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <limits>
#include <string>
#include <system_error>

#include <windows.h>

#include "check.h"
#include "make_player.h"

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

static HWND video_window;
static std::string clip;
static std::string backend = "vlc";

static DWORD WINAPI windowThread(LPVOID created)
{
    video_window = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 320, 240, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    SetEvent(static_cast<HANDLE>(created));
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (video_window)
        DestroyWindow(video_window);
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

static std::string dec(double value)
{
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.3f", value);
    return buffer;
}

static bool same(double value, double expected)
{
    return std::fabs(value - expected) < 0.001;
}

static bool runProcess(const std::wstring& command_line, int milliseconds)
{
    STARTUPINFOW startup = {};
    startup.cb = sizeof startup;
    PROCESS_INFORMATION process = {};
    std::wstring buffer = command_line;
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
    std::wstring command = L"\"" + std::wstring(ffmpeg) + L"\" -hide_banner -loglevel error -y";
    command += L" -f lavfi -i testsrc=duration=30:size=320x240:rate=25";
    command += L" -f lavfi -i sine=frequency=440:duration=30";
    command += L" -af volume=0 -c:v mpeg4 -c:a aac -shortest \"" + file.wstring() + L"\"";
    if (!runProcess(command, 120000) || !fs::exists(file))
        return false;
    clip = file.u8string();
    return true;
}

static void testNothingOpen()
{
    auto owned = makePlayer(backend);
    Player& player = *owned;
    player.attach(video_window);
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

static void testVolumeAndSpeedWithNothingOpen()
{
    auto owned = makePlayer(backend);
    Player& player = *owned;
    check(player.volume() == 100, "volume() is 100 on a new player, got " + num(player.volume()));
    check(same(player.speed(), 1.0), "speed() is 1.0 on a new player, got " + dec(player.speed()));

    player.setSpeed(std::numeric_limits<double>::quiet_NaN());
    check(same(player.speed(), 1.0), "setSpeed(NaN) on a new player keeps 1.0, got " + dec(player.speed()));
    player.setSpeed(std::numeric_limits<double>::infinity());
    check(same(player.speed(), 1.0), "setSpeed(infinity) on a new player keeps 1.0, got " + dec(player.speed()));
    player.setSpeed(-std::numeric_limits<double>::infinity());
    check(same(player.speed(), 1.0), "setSpeed(-infinity) on a new player keeps 1.0, got " + dec(player.speed()));

    player.attach(video_window);
    player.setVolume(50);
    check(player.volume() == 50, "setVolume(50) with nothing open gives 50, got " + num(player.volume()));
    player.setVolume(0);
    check(player.volume() == 0, "setVolume(0) with nothing open gives 0, got " + num(player.volume()));
    player.setVolume(100);
    check(player.volume() == 100, "setVolume(100) with nothing open gives 100, got " + num(player.volume()));
    player.setVolume(150);
    check(player.volume() == 100, "setVolume(150) clamps to 100, got " + num(player.volume()));
    player.setVolume(-20);
    check(player.volume() == 0, "setVolume(-20) clamps to 0, got " + num(player.volume()));
    player.setVolume(std::numeric_limits<int>::max());
    check(player.volume() == 100, "setVolume(INT_MAX) clamps to 100, got " + num(player.volume()));
    player.setVolume(std::numeric_limits<int>::min());
    check(player.volume() == 0, "setVolume(INT_MIN) clamps to 0, got " + num(player.volume()));

    player.setSpeed(1.5);
    check(same(player.speed(), 1.5), "setSpeed(1.5) with nothing open gives 1.5, got " + dec(player.speed()));
    player.setSpeed(0.5);
    check(same(player.speed(), 0.5), "setSpeed(0.5) with nothing open gives 0.5, got " + dec(player.speed()));
    player.setSpeed(2.0);
    check(same(player.speed(), 2.0), "setSpeed(2.0) with nothing open gives 2.0, got " + dec(player.speed()));
    player.setSpeed(10.0);
    check(same(player.speed(), 2.0), "setSpeed(10.0) clamps to 2.0, got " + dec(player.speed()));
    player.setSpeed(0.1);
    check(same(player.speed(), 0.5), "setSpeed(0.1) clamps to 0.5, got " + dec(player.speed()));
    player.setSpeed(0.0);
    check(same(player.speed(), 0.5), "setSpeed(0.0) clamps to 0.5, got " + dec(player.speed()));
    player.setSpeed(-1.0);
    check(same(player.speed(), 0.5), "setSpeed(-1.0) clamps to 0.5, got " + dec(player.speed()));

    player.setSpeed(1.5);
    player.setSpeed(std::numeric_limits<double>::quiet_NaN());
    check(same(player.speed(), 1.5), "setSpeed(NaN) keeps the previous 1.5, got " + dec(player.speed()));
    player.setSpeed(std::numeric_limits<double>::infinity());
    check(same(player.speed(), 1.5), "setSpeed(infinity) keeps the previous 1.5, got " + dec(player.speed()));
    player.setSpeed(-std::numeric_limits<double>::infinity());
    check(same(player.speed(), 1.5), "setSpeed(-infinity) keeps the previous 1.5, got " + dec(player.speed()));
}

static void testMissingFile(const fs::path& folder)
{
    auto owned = makePlayer(backend);
    Player& player = *owned;
    player.attach(video_window);
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
    player.attach(video_window);
    player.open(clip, start);
    return waitFor([&] { return player.playing() && player.length() > 0; }, 10000);
}

static double playbackRate(Player& player, int milliseconds, bool& stayed_playing)
{
    stayed_playing = true;
    int first = player.time();
    waitFor([&] { if (!player.playing()) stayed_playing = false; return player.time() != first; }, 4500);
    int from = player.time();
    Clock::time_point started = Clock::now();
    watch([&] { if (!player.playing()) stayed_playing = false; }, milliseconds);
    int last = player.time();
    waitFor([&] { if (!player.playing()) stayed_playing = false; return player.time() != last; }, 4500);
    int to = player.time();
    double wall = std::chrono::duration<double>(Clock::now() - started).count();
    return (to - from) / wall;
}

static bool seekForMeasuring(Player& player)
{
    player.seek(2);
    return waitFor([&] { int t = player.time(); return player.playing() && t >= 2 && t <= 4; }, 5000);
}

static void testOpen()
{
    auto owned = makePlayer(backend);
    Player& player = *owned;
    Clock::time_point opened = Clock::now();
    player.attach(video_window);
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
    auto owned = makePlayer(backend);
    Player& player = *owned;
    Clock::time_point opened = Clock::now();
    player.attach(video_window);
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
    auto owned = makePlayer(backend);
    Player& player = *owned;
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
    auto owned = makePlayer(backend);
    Player& player = *owned;
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
    auto owned = makePlayer(backend);
    Player& player = *owned;
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
    auto owned = makePlayer(backend);
    Player& player = *owned;
    player.attach(video_window);
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

static void testVolumeAndSpeedWithMissingFile(const fs::path& folder)
{
    auto owned = makePlayer(backend);
    Player& player = *owned;
    player.attach(video_window);
    player.open((folder / "does-not-exist" / "missing.mp4").u8string(), 0);
    player.setVolume(30);
    player.setSpeed(1.5);
    check(player.volume() == 30, "setVolume(30) right after opening a non-existent file gives 30, got " + num(player.volume()));
    check(same(player.speed(), 1.5), "setSpeed(1.5) right after opening a non-existent file gives 1.5, got " + dec(player.speed()));

    waitFor([&] { return !player.playing(); }, 5000);
    Sleep(500);
    player.setVolume(200);
    player.setSpeed(0.25);
    player.setSpeed(std::numeric_limits<double>::quiet_NaN());
    check(player.volume() == 100, "setVolume(200) after a failed open clamps to 100, got " + num(player.volume()));
    check(same(player.speed(), 0.5), "setSpeed(0.25) then NaN after a failed open gives 0.5, got " + dec(player.speed()));
    Sleep(300);
    check(player.time() >= 0 && player.length() >= 0, "setVolume and setSpeed after opening a non-existent file do not crash");
}

static void testVolumeAndSpeedBeforeOpen()
{
    auto owned = makePlayer(backend);
    Player& player = *owned;
    player.setVolume(40);
    player.setSpeed(2.0);
    player.attach(video_window);
    player.open(clip, 0);
    check(player.volume() == 40, "volume() set to 40 before open is still 40 right after open, got " + num(player.volume()));
    check(same(player.speed(), 2.0), "speed() set to 2.0 before open is still 2.0 right after open, got " + dec(player.speed()));
    check(waitFor([&] { return player.playing() && player.length() > 0; }, 10000), "clip plays after setting volume and speed before open");
    check(player.volume() == 40, "volume() set to 40 before open is still 40 while playing, got " + num(player.volume()));
    check(same(player.speed(), 2.0), "speed() set to 2.0 before open is still 2.0 while playing, got " + dec(player.speed()));

    check(seekForMeasuring(player), "seek(2) lands near 2 before measuring speed 2.0 set before open, got " + num(player.time()));
    bool stayed_playing = true;
    double rate = playbackRate(player, 4000, stayed_playing);
    check(rate >= 1.5, "speed 2.0 set before open plays at least 1.5x, got " + dec(rate) + "x");
    check(stayed_playing, "playing() stays true while playing at speed 2.0 set before open");
}

static void testSpeedPlayback()
{
    auto owned = makePlayer(backend);
    Player& player = *owned;
    check(startPlaying(player, 0), "clip plays for the speed tests");

    check(seekForMeasuring(player), "seek(2) lands near 2 before measuring speed 1.0, got " + num(player.time()));
    bool stayed_playing = true;
    double rate = playbackRate(player, 4000, stayed_playing);
    check(rate >= 0.7 && rate <= 1.3, "speed 1.0 plays between 0.7x and 1.3x, got " + dec(rate) + "x");
    check(stayed_playing, "playing() stays true while playing at speed 1.0");

    player.setSpeed(0.5);
    check(same(player.speed(), 0.5), "setSpeed(0.5) while playing gives 0.5 right away, got " + dec(player.speed()));
    check(player.playing(), "playing() stays true right after setSpeed(0.5)");
    player.setSpeed(std::numeric_limits<double>::quiet_NaN());
    check(same(player.speed(), 0.5), "setSpeed(NaN) while playing keeps 0.5, got " + dec(player.speed()));
    player.setSpeed(5.0);
    check(same(player.speed(), 2.0), "setSpeed(5.0) while playing clamps to 2.0, got " + dec(player.speed()));
    player.setSpeed(0.5);
    player.setVolume(150);
    check(player.volume() == 100, "setVolume(150) while playing clamps to 100 right away, got " + num(player.volume()));
    player.setVolume(-5);
    check(player.volume() == 0, "setVolume(-5) while playing clamps to 0 right away, got " + num(player.volume()));
    player.setVolume(60);
    check(player.volume() == 60, "setVolume(60) while playing gives 60 right away, got " + num(player.volume()));
    check(player.playing(), "playing() stays true right after setVolume while playing");

    player.togglePause();
    check(waitFor([&] { return !player.playing(); }, 5000), "togglePause() at speed 0.5 pauses");
    Sleep(300);
    check(same(player.speed(), 0.5), "speed() stays 0.5 while paused, got " + dec(player.speed()));
    player.togglePause();
    check(waitFor([&] { return player.playing(); }, 5000), "togglePause() at speed 0.5 resumes");
    check(same(player.speed(), 0.5), "speed() stays 0.5 after resuming, got " + dec(player.speed()));
    check(player.volume() == 60, "volume() stays 60 after pausing and resuming, got " + num(player.volume()));

    check(seekForMeasuring(player), "seek(2) lands near 2 before measuring speed 0.5, got " + num(player.time()));
    rate = playbackRate(player, 4000, stayed_playing);
    check(rate > 0 && rate <= 0.75, "speed 0.5 after pausing and resuming plays above 0 and at most 0.75x, got " + dec(rate) + "x");
    check(stayed_playing, "playing() stays true while playing at speed 0.5");
}

static void testVolumeAndSpeedAcrossReopen(const fs::path& folder)
{
    fs::path other = folder / "other.mp4";
    std::error_code error;
    fs::copy_file(fs::u8path(clip), other, fs::copy_options::overwrite_existing, error);
    check(!error && fs::exists(other), "a second copy of the clip is made for the reopen tests");

    auto owned = makePlayer(backend);
    Player& player = *owned;
    check(startPlaying(player, 0), "clip plays for the volume and speed reopen tests");
    player.setVolume(25);
    player.setSpeed(2.0);
    bool stayed_playing = true;
    watch([&] { if (!player.playing()) stayed_playing = false; }, 500);
    check(stayed_playing, "playing() stays true after setVolume(25) and setSpeed(2.0) while playing");

    player.seek(20);
    waitFor([&] { return player.time() >= 20; }, 5000);
    player.open(clip, 0);
    check(player.volume() == 25, "volume() stays 25 right after opening the same clip again, got " + num(player.volume()));
    check(same(player.speed(), 2.0), "speed() stays 2.0 right after opening the same clip again, got " + dec(player.speed()));
    check(waitFor([&] { return player.playing() && player.time() <= 5; }, 10000), "the same clip plays again from the start, got " + num(player.time()));
    check(player.volume() == 25, "volume() stays 25 while the same clip plays again, got " + num(player.volume()));
    check(same(player.speed(), 2.0), "speed() stays 2.0 while the same clip plays again, got " + dec(player.speed()));

    player.seek(20);
    waitFor([&] { return player.time() >= 20; }, 5000);
    player.open(other.u8string(), 0);
    check(player.volume() == 25, "volume() stays 25 right after opening another file, got " + num(player.volume()));
    check(same(player.speed(), 2.0), "speed() stays 2.0 right after opening another file, got " + dec(player.speed()));
    check(waitFor([&] { return player.playing() && player.length() > 0 && player.time() <= 5; }, 10000),
          "another file plays from the start, got " + num(player.time()));
    check(player.volume() == 25, "volume() stays 25 while another file plays, got " + num(player.volume()));
    check(same(player.speed(), 2.0), "speed() stays 2.0 while another file plays, got " + dec(player.speed()));

    check(seekForMeasuring(player), "seek(2) lands near 2 before measuring speed 2.0 after reopening, got " + num(player.time()));
    double rate = playbackRate(player, 4000, stayed_playing);
    check(rate >= 1.5, "speed 2.0 set before opening another file plays at least 1.5x, got " + dec(rate) + "x");
    check(stayed_playing, "playing() stays true while another file plays at speed 2.0");
}

int main(int argc, char** argv)
{
    if (argc > 1)
        backend = argv[1];
    if (!makePlayer(backend)) {
        std::printf("unknown backend: %s\n", backend.c_str());
        return 2;
    }

    HANDLE created = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    DWORD thread_id = 0;
    HANDLE thread = CreateThread(nullptr, 0, windowThread, created, 0, &thread_id);
    WaitForSingleObject(created, 10000);
    CloseHandle(created);

    fs::path folder = fs::temp_directory_path() / ("youtonomous-player-test-" + std::to_string(GetCurrentProcessId()));
    std::error_code error;
    fs::remove_all(folder, error);
    fs::create_directories(folder);

    int result = 0;
    testNothingOpen();
    testVolumeAndSpeedWithNothingOpen();
    bool ready = makePlayer(backend)->ready();
    if (!ready) {
        std::printf("SKIP: VLC did not start\n");
        result = fail_count > 0 ? report() : 77;
    } else {
        testMissingFile(folder);
        if (!makeClip(folder)) {
            std::printf("SKIP: ffmpeg not found or could not make a test clip\n");
            result = fail_count > 0 ? report() : 77;
        } else {
            testOpen();
            testOpenAtStart();
            testSeek();
            testSkip();
            testPause();
            testReopen(folder);
            testVolumeAndSpeedWithMissingFile(folder);
            testVolumeAndSpeedBeforeOpen();
            testSpeedPlayback();
            testVolumeAndSpeedAcrossReopen(folder);
            result = report();
        }
    }

    PostThreadMessageW(thread_id, WM_QUIT, 0, 0);
    WaitForSingleObject(thread, 5000);
    CloseHandle(thread);
    fs::remove_all(folder, error);
    return result;
}
