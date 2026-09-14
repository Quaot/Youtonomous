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
    resume_button_ = addControl(context, L"BUTTON", L"Resume", BS_PUSHBUTTON, kResume);

    marks_label_ = addControl(context, L"STATIC", L"Bookmarks", SS_LEFT, 0);
    list_ = addControl(context, L"LISTBOX", L"", list_style, kMarks, WS_EX_CLIENTEDGE);
    label_edit_ = addControl(context, L"EDIT", L"", ES_AUTOHSCROLL, kMarkLabel, WS_EX_CLIENTEDGE);
    add_button_ = addControl(context, L"BUTTON", L"Add", BS_PUSHBUTTON, kAddMark);
    delete_button_ = addControl(context, L"BUTTON", L"Delete", BS_PUSHBUTTON, kDeleteMark);

    SendMessageW(start_edit_, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"0:00"));
    SendMessageW(label_edit_, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Label (optional)"));
    showStoppedAt(0);
}

void MarksPanel::layout(int width, int height)
{
    int bottom = height - kPad - kRow;
    int left = width - kPad - kSide;
    int label_row_y = bottom - kGap - kRow;
    int buttons_x = left + kSide - kButton * 3 - kGap * 2;

    int y = kPad;
    MoveWindow(start_label_, left, y, kSide, kLabel, TRUE);
    y += kLabel;
    MoveWindow(start_edit_, left, y, buttons_x - kGap - left, kRow, TRUE);
    MoveWindow(set_start_button_, buttons_x, y, kButton, kRow, TRUE);
    MoveWindow(go_start_button_, buttons_x + kButton + kGap, y, kButton, kRow, TRUE);
    MoveWindow(resume_button_, buttons_x + (kButton + kGap) * 2, y, kButton, kRow, TRUE);
    y += kRow + kPad;
    MoveWindow(marks_label_, left, y, kSide, kLabel, TRUE);
    y += kLabel;
    MoveWindow(list_, left, y, kSide, std::max(0, label_row_y - kGap - y), TRUE);
    MoveWindow(label_edit_, left, label_row_y, kSide - kSmallButton - kGap, kRow, TRUE);
    MoveWindow(add_button_, left + kSide - kSmallButton, label_row_y, kSmallButton, kRow, TRUE);
    MoveWindow(delete_button_, left, bottom, kSide, kRow, TRUE);
}

void MarksPanel::setVisible(bool visible)
{
    ui::setVisible({start_label_, start_edit_, set_start_button_, go_start_button_, resume_button_, marks_label_, list_,
                    label_edit_, add_button_, delete_button_},
                   visible);
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

void MarksPanel::showStoppedAt(int seconds)
{
    if (seconds > 0)
        setText(start_label_, widen("Start at  (last stopped at " + timecode::format(seconds) + ")"));
    else
        setText(start_label_, L"Start at");
    EnableWindow(resume_button_, seconds > 0 ? TRUE : FALSE);
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
