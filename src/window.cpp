#include "window.h"

#include <algorithm>

#include <commdlg.h>

#include "seekbar.h"
#include "text.h"
#include "timecode.h"

namespace {

enum ControlId {
    kOpen = 100,
    kBack30,
    kBack10,
    kPlay,
    kForward10,
    kForward30,
};

const UINT_PTR kTimer = 1;

const int kPad = 8;
const int kGap = 4;
const int kSide = 270;
const int kRow = 26;
const int kButton = 56;
const int kSeekHeight = 20;

void setText(HWND control, const std::wstring& text)
{
    wchar_t current[256];
    GetWindowTextW(control, current, 256);
    if (text != current)
        SetWindowTextW(control, text.c_str());
}

}

bool MainWindow::create(HINSTANCE instance, int show)
{
    instance_ = instance;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    wc.lpszClassName = L"Youtonomous";
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(0, wc.lpszClassName, L"Youtonomous", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                            CW_USEDEFAULT, CW_USEDEFAULT, 1280, 760,
                            nullptr, nullptr, instance, this);
    if (!hwnd_)
        return false;

    ShowWindow(hwnd_, show);
    return true;
}

LRESULT CALLBACK MainWindow::proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCREATE) {
        auto* self = static_cast<MainWindow*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    auto* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self)
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    return self->handle(msg, wParam, lParam);
}

LRESULT MainWindow::handle(UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        createControls();
        SetTimer(hwnd_, kTimer, 250, nullptr);
        if (!player_.ready())
            MessageBoxW(hwnd_, L"Could not start VLC.", L"Youtonomous", MB_ICONERROR);
        return 0;
    case WM_SIZE:
        layout(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_COMMAND:
        onCommand(LOWORD(wParam));
        return 0;
    case WM_TIMER:
        tick();
        return 0;
    case WM_SEEKBAR_SEEK:
        player_.seek(static_cast<int>(wParam));
        tick();
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd_, kTimer);
        DeleteObject(font_);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

HWND MainWindow::addControl(const wchar_t* type, const wchar_t* text, DWORD style, int id)
{
    HWND control = CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style,
                                   0, 0, 0, 0, hwnd_,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    return control;
}

void MainWindow::createControls()
{
    NONCLIENTMETRICSW metrics{};
    metrics.cbSize = sizeof metrics;
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof metrics, &metrics, 0);
    font_ = CreateFontIndirectW(&metrics.lfMessageFont);

    openButton_ = addControl(L"BUTTON", L"Open file...", BS_PUSHBUTTON, kOpen);
    video_ = addControl(L"STATIC", L"", SS_BLACKRECT, 0);
    seekbar_ = seekbar::create(hwnd_, instance_);

    back30_ = addControl(L"BUTTON", L"-30", BS_PUSHBUTTON, kBack30);
    back10_ = addControl(L"BUTTON", L"-10", BS_PUSHBUTTON, kBack10);
    playButton_ = addControl(L"BUTTON", L"Play", BS_PUSHBUTTON, kPlay);
    forward10_ = addControl(L"BUTTON", L"+10", BS_PUSHBUTTON, kForward10);
    forward30_ = addControl(L"BUTTON", L"+30", BS_PUSHBUTTON, kForward30);
    timeLabel_ = addControl(L"STATIC", L"0:00 / 0:00", SS_CENTERIMAGE, 0);

    player_.attach(video_);
}

void MainWindow::layout(int width, int height)
{
    int centerX = kSide + kPad * 2;
    int centerWidth = std::max(0, width - kSide * 2 - kPad * 4);
    int controlsY = height - kPad - kRow;
    int seekY = controlsY - kPad - kSeekHeight;

    MoveWindow(openButton_, kPad, controlsY, kSide, kRow, TRUE);
    MoveWindow(video_, centerX, kPad, centerWidth, std::max(0, seekY - kPad * 2), TRUE);
    MoveWindow(seekbar_, centerX, seekY, centerWidth, kSeekHeight, TRUE);

    int x = centerX;
    for (HWND button : {back30_, back10_, playButton_, forward10_, forward30_}) {
        MoveWindow(button, x, controlsY, kButton, kRow, TRUE);
        x += kButton + kGap;
    }
    MoveWindow(timeLabel_, x + kPad, controlsY, 160, kRow, TRUE);
}

void MainWindow::onCommand(int id)
{
    switch (id) {
    case kOpen: openFile(); break;
    case kBack30: player_.skip(-30); break;
    case kBack10: player_.skip(-10); break;
    case kPlay: player_.togglePause(); break;
    case kForward10: player_.skip(10); break;
    case kForward30: player_.skip(30); break;
    }
    tick();
}

void MainWindow::tick()
{
    int time = player_.time();
    int length = player_.length();

    seekbar::setRange(seekbar_, length);
    seekbar::setPosition(seekbar_, time);

    setText(timeLabel_, widen(timecode::format(time) + " / " + timecode::format(length)));
    setText(playButton_, player_.playing() ? L"Pause" : L"Play");
}

void MainWindow::openFile()
{
    wchar_t path[MAX_PATH] = L"";

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof dialog;
    dialog.hwndOwner = hwnd_;
    dialog.lpstrFilter = L"Videos\0*.mp4;*.mkv;*.webm;*.mov;*.avi\0All files\0*.*\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = MAX_PATH;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&dialog))
        openPath(path);
}

void MainWindow::openPath(const std::wstring& path)
{
    player_.open(narrow(path), 0);
}
