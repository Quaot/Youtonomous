#include "controls.h"

namespace ui {

HWND addControl(const Context& context, const wchar_t* type, const wchar_t* text, DWORD style, int id, DWORD exStyle)
{
    HMENU menu = reinterpret_cast<HMENU>(static_cast<INT_PTR>(id));
    HWND control = CreateWindowExW(exStyle, type, text, WS_CHILD | WS_VISIBLE | style,
                                   0, 0, 0, 0, context.parent, menu, context.instance, nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(context.font), TRUE);
    return control;
}

std::wstring textOf(HWND control)
{
    int length = GetWindowTextLengthW(control);
    std::wstring text(length + 1, L'\0');
    GetWindowTextW(control, text.data(), length + 1);
    text.resize(length);
    return text;
}

void setText(HWND control, const std::wstring& text)
{
    if (textOf(control) != text)
        SetWindowTextW(control, text.c_str());
}

std::string trim(const std::string& text)
{
    size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return "";
    size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

int selectedIndex(HWND list, size_t count)
{
    int index = static_cast<int>(SendMessageW(list, LB_GETCURSEL, 0, 0));
    return index >= 0 && static_cast<size_t>(index) < count ? index : -1;
}

}
