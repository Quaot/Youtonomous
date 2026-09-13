#include "window.h"

#include <algorithm>

#include <commdlg.h>

#include "text.h"

namespace {

enum ControlId {
    kOpen = 100,
};

const int kPad = 8;
const int kSide = 270;
const int kRow = 26;

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
        if (!player_.ready())
            MessageBoxW(hwnd_, L"Could not start VLC.", L"Youtonomous", MB_ICONERROR);
        return 0;
    case WM_SIZE:
        layout(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_COMMAND:
        onCommand(LOWORD(wParam));
        return 0;
    case WM_DESTROY:
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

    player_.attach(video_);
}

void MainWindow::layout(int width, int height)
{
    int centerX = kSide + kPad * 2;
    int centerWidth = std::max(0, width - kSide * 2 - kPad * 4);

    MoveWindow(openButton_, kPad, height - kPad - kRow, kSide, kRow, TRUE);
    MoveWindow(video_, centerX, kPad, centerWidth, std::max(0, height - kPad * 2), TRUE);
}

void MainWindow::onCommand(int id)
{
    switch (id) {
    case kOpen:
        openFile();
        break;
    }
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
