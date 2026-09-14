#include "marks_panel.h"

#include <algorithm>

#include <commctrl.h>

#include "text.h"
#include "timecode.h"

using namespace ui;

void MarksPanel::create(const Context& context)
{
    const DWORD listStyle = WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT;

    startLabel_ = addControl(context, L"STATIC", L"Start at", SS_LEFT, 0);
    startEdit_ = addControl(context, L"EDIT", L"", ES_AUTOHSCROLL, kStart, WS_EX_CLIENTEDGE);
    setStartButton_ = addControl(context, L"BUTTON", L"Set", BS_PUSHBUTTON, kSetStart);
    goStartButton_ = addControl(context, L"BUTTON", L"Go", BS_PUSHBUTTON, kGoStart);

    marksLabel_ = addControl(context, L"STATIC", L"Bookmarks", SS_LEFT, 0);
    list_ = addControl(context, L"LISTBOX", L"", listStyle, kMarks, WS_EX_CLIENTEDGE);
    labelEdit_ = addControl(context, L"EDIT", L"", ES_AUTOHSCROLL, kMarkLabel, WS_EX_CLIENTEDGE);
    addButton_ = addControl(context, L"BUTTON", L"Add", BS_PUSHBUTTON, kAddMark);
    deleteButton_ = addControl(context, L"BUTTON", L"Delete", BS_PUSHBUTTON, kDeleteMark);

    SendMessageW(startEdit_, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"0:00"));
    SendMessageW(labelEdit_, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Label (optional)"));
}

void MarksPanel::layout(int width, int height)
{
    int bottom = height - kPad - kRow;
    int left = width - kPad - kSide;
    int labelRowY = bottom - kGap - kRow;

    int y = kPad;
    MoveWindow(startLabel_, left, y, kSide, kLabel, TRUE);
    y += kLabel;
    MoveWindow(startEdit_, left, y, kSide - kButton * 2 - kGap * 2, kRow, TRUE);
    MoveWindow(setStartButton_, left + kSide - kButton * 2 - kGap, y, kButton, kRow, TRUE);
    MoveWindow(goStartButton_, left + kSide - kButton, y, kButton, kRow, TRUE);
    y += kRow + kPad;
    MoveWindow(marksLabel_, left, y, kSide, kLabel, TRUE);
    y += kLabel;
    MoveWindow(list_, left, y, kSide, std::max(0, labelRowY - kGap - y), TRUE);
    MoveWindow(labelEdit_, left, labelRowY, kSide - kSmallButton - kGap, kRow, TRUE);
    MoveWindow(addButton_, left + kSide - kSmallButton, labelRowY, kSmallButton, kRow, TRUE);
    MoveWindow(deleteButton_, left, bottom, kSide, kRow, TRUE);
}

void MarksPanel::show(const Video* video)
{
    SendMessageW(list_, LB_RESETCONTENT, 0, 0);
    if (!video) {
        setText(startEdit_, L"");
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
    setText(startEdit_, widen(timecode::format(seconds)));
}

std::string MarksPanel::startText() const
{
    return narrow(textOf(startEdit_));
}

std::string MarksPanel::takeLabel()
{
    std::string label = trim(narrow(textOf(labelEdit_)));
    SetWindowTextW(labelEdit_, L"");
    return label;
}

int MarksPanel::selected(size_t count) const
{
    return selectedIndex(list_, count);
}
