#include "marks_panel.h"

#include <algorithm>

#include <commctrl.h>

#include "text.h"
#include "timecode.h"

using namespace ui;

void MarksPanel::create(const Context& context)
{
    const DWORD list_style = WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT;

    start_label_ = addControl(context, L"STATIC", L"Start at", SS_LEFT, 0);
    start_edit_ = addControl(context, L"EDIT", L"", ES_AUTOHSCROLL, kStart, WS_EX_CLIENTEDGE);
    set_start_button_ = addControl(context, L"BUTTON", L"Set", BS_PUSHBUTTON, kSetStart);
    go_start_button_ = addControl(context, L"BUTTON", L"Go", BS_PUSHBUTTON, kGoStart);

    marks_label_ = addControl(context, L"STATIC", L"Bookmarks", SS_LEFT, 0);
    list_ = addControl(context, L"LISTBOX", L"", list_style, kMarks, WS_EX_CLIENTEDGE);
    label_edit_ = addControl(context, L"EDIT", L"", ES_AUTOHSCROLL, kMarkLabel, WS_EX_CLIENTEDGE);
    add_button_ = addControl(context, L"BUTTON", L"Add", BS_PUSHBUTTON, kAddMark);
    delete_button_ = addControl(context, L"BUTTON", L"Delete", BS_PUSHBUTTON, kDeleteMark);

    SendMessageW(start_edit_, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"0:00"));
    SendMessageW(label_edit_, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Label (optional)"));
}

void MarksPanel::layout(int width, int height)
{
    int bottom = height - kPad - kRow;
    int left = width - kPad - kSide;
    int label_row_y = bottom - kGap - kRow;

    int y = kPad;
    MoveWindow(start_label_, left, y, kSide, kLabel, TRUE);
    y += kLabel;
    MoveWindow(start_edit_, left, y, kSide - kButton * 2 - kGap * 2, kRow, TRUE);
    MoveWindow(set_start_button_, left + kSide - kButton * 2 - kGap, y, kButton, kRow, TRUE);
    MoveWindow(go_start_button_, left + kSide - kButton, y, kButton, kRow, TRUE);
    y += kRow + kPad;
    MoveWindow(marks_label_, left, y, kSide, kLabel, TRUE);
    y += kLabel;
    MoveWindow(list_, left, y, kSide, std::max(0, label_row_y - kGap - y), TRUE);
    MoveWindow(label_edit_, left, label_row_y, kSide - kSmallButton - kGap, kRow, TRUE);
    MoveWindow(add_button_, left + kSide - kSmallButton, label_row_y, kSmallButton, kRow, TRUE);
    MoveWindow(delete_button_, left, bottom, kSide, kRow, TRUE);
}

void MarksPanel::show(const Video* video)
{
    SendMessageW(list_, LB_RESETCONTENT, 0, 0);
    if (!video) {
        setText(start_edit_, L"");
        return;
    }

    for (const Bookmark& mark : video->marks) {
        std::wstring line = widen(timecode::format(mark.time) + "  " + mark.label);
        SendMessageW(list_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(line.c_str()));
    }
    showStart(video->start);
}

void MarksPanel::showStart(int seconds)
{
    setText(start_edit_, widen(timecode::format(seconds)));
}

std::string MarksPanel::startText() const
{
    return narrow(textOf(start_edit_));
}

std::string MarksPanel::takeLabel()
{
    std::string label = trim(narrow(textOf(label_edit_)));
    SetWindowTextW(label_edit_, L"");
    return label;
}

int MarksPanel::selected(size_t count) const
{
    return selectedIndex(list_, count);
}
