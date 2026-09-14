#include "seekbar.h"

#include <algorithm>

#include <windowsx.h>

namespace seekbar {

namespace {

const wchar_t kClassName[] = L"YoutonomousSeekBar";

const COLORREF kTrack = RGB(200, 200, 200);
const COLORREF kPlayed = RGB(220, 40, 40);
const COLORREF kChapter = RGB(60, 110, 200);
const COLORREF kMark = RGB(240, 150, 0);
const COLORREF kStart = RGB(40, 170, 80);

struct State {
    int length = 0;
    int position = 0;
    int start = 0;
    std::vector<Bookmark> marks;
    bool dragging = false;
};

State* stateOf(HWND bar)
{
    return reinterpret_cast<State*>(GetWindowLongPtrW(bar, GWLP_USERDATA));
}

int widthOf(HWND bar)
{
    RECT client;
    GetClientRect(bar, &client);
    return client.right;
}

int xFor(const State& state, int seconds, int width)
{
    if (state.length <= 0)
        return 0;
    return static_cast<int>(static_cast<long long>(seconds) * width / state.length);
}

int secondsFor(const State& state, int x, int width)
{
    if (width <= 0)
        return 0;
    x = std::clamp(x, 0, width);
    return static_cast<int>(static_cast<long long>(x) * state.length / width);
}

void fill(HDC dc, const RECT& rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void drawTick(HDC dc, int x, int height, COLORREF color)
{
    RECT rect{x - 1, 2, x + 2, height - 2};
    fill(dc, rect, color);
}

void draw(HDC dc, const State& state, int width, int height)
{
    RECT all{0, 0, width, height};
    fill(dc, all, GetSysColor(COLOR_BTNFACE));

    RECT track{0, height / 2 - 3, width, height / 2 + 3};
    fill(dc, track, kTrack);

    track.right = xFor(state, state.position, width);
    fill(dc, track, kPlayed);

    if (state.length <= 0)
        return;

    for (const Bookmark& mark : state.marks)
        drawTick(dc, xFor(state, mark.time, width), height, mark.chapter ? kChapter : kMark);

    if (state.start > 0)
        drawTick(dc, xFor(state, state.start, width), height, kStart);
}

void paint(HWND bar, const State& state)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(bar, &ps);

    RECT client;
    GetClientRect(bar, &client);

    HDC memory = CreateCompatibleDC(dc);
    HBITMAP bitmap = CreateCompatibleBitmap(dc, client.right, client.bottom);
    HGDIOBJ old = SelectObject(memory, bitmap);

    draw(memory, state, client.right, client.bottom);
    BitBlt(dc, 0, 0, client.right, client.bottom, memory, 0, 0, SRCCOPY);

    SelectObject(memory, old);
    DeleteObject(bitmap);
    DeleteDC(memory);
    EndPaint(bar, &ps);
}

void dragTo(HWND bar, State& state, int x)
{
    state.position = secondsFor(state, x, widthOf(bar));
    InvalidateRect(bar, nullptr, FALSE);
}

LRESULT CALLBACK proc(HWND bar, UINT msg, WPARAM w_param, LPARAM l_param)
{
    State* state = stateOf(bar);

    switch (msg) {
    case WM_NCCREATE:
        SetWindowLongPtrW(bar, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(new State));
        break;
    case WM_NCDESTROY:
        delete state;
        SetWindowLongPtrW(bar, GWLP_USERDATA, 0);
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        paint(bar, *state);
        return 0;
    case WM_LBUTTONDOWN:
        SetFocus(GetParent(bar));
        if (state->length > 0) {
            state->dragging = true;
            SetCapture(bar);
            dragTo(bar, *state, GET_X_LPARAM(l_param));
        }
        return 0;
    case WM_MOUSEMOVE:
        if (state->dragging)
            dragTo(bar, *state, GET_X_LPARAM(l_param));
        return 0;
    case WM_LBUTTONUP:
        if (state->dragging) {
            state->dragging = false;
            ReleaseCapture();
            PostMessageW(GetParent(bar), WM_SEEKBAR_SEEK, state->position, 0);
        }
        return 0;
    case WM_CAPTURECHANGED:
        state->dragging = false;
        return 0;
    }
    return DefWindowProcW(bar, msg, w_param, l_param);
}

}

HWND create(HWND parent, HINSTANCE instance)
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_HAND);
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);

    return CreateWindowExW(0, kClassName, L"", WS_CHILD | WS_VISIBLE,
                           0, 0, 0, 0, parent, nullptr, instance, nullptr);
}

void setRange(HWND bar, int length)
{
    State* state = stateOf(bar);
    if (state->length == length)
        return;
    state->length = length;
    InvalidateRect(bar, nullptr, FALSE);
}

void setPosition(HWND bar, int position)
{
    State* state = stateOf(bar);
    if (state->dragging || state->position == position)
        return;
    state->position = position;
    InvalidateRect(bar, nullptr, FALSE);
}

void setMarks(HWND bar, const std::vector<Bookmark>& marks, int start)
{
    State* state = stateOf(bar);
    state->marks = marks;
    state->start = start;
    InvalidateRect(bar, nullptr, FALSE);
}

}
