#include "player_panel.h"

#include <algorithm>

#include "seekbar.h"
#include "text.h"
#include "timecode.h"

using namespace ui;

void PlayerPanel::create(const Context& context)
{
    video_ = addControl(context, L"STATIC", L"", SS_BLACKRECT, 0);
    seekbar_ = seekbar::create(context.parent, context.instance);
    prev_mark_ = addControl(context, L"BUTTON", L"Prev", BS_PUSHBUTTON, kPrevMark);
    back30_ = addControl(context, L"BUTTON", L"-30", BS_PUSHBUTTON, kBack30);
    back10_ = addControl(context, L"BUTTON", L"-10", BS_PUSHBUTTON, kBack10);
    play_button_ = addControl(context, L"BUTTON", L"Play", BS_PUSHBUTTON, kPlay);
    forward10_ = addControl(context, L"BUTTON", L"+10", BS_PUSHBUTTON, kForward10);
    forward30_ = addControl(context, L"BUTTON", L"+30", BS_PUSHBUTTON, kForward30);
    next_mark_ = addControl(context, L"BUTTON", L"Next", BS_PUSHBUTTON, kNextMark);
    time_label_ = addControl(context, L"STATIC", L"0:00 / 0:00", SS_CENTERIMAGE, 0);
}

void PlayerPanel::layout(int width, int height)
{
    int bottom = height - kPad - kRow;
    int left = kSide + kPad * 2;
    int center_width = std::max(0, width - kSide * 2 - kPad * 4);
    int seek_y = bottom - kPad - kSeekHeight;

    MoveWindow(video_, left, kPad, center_width, std::max(0, seek_y - kPad * 2), TRUE);
    MoveWindow(seekbar_, left, seek_y, center_width, kSeekHeight, TRUE);

    int x = left;
    for (HWND button : {prev_mark_, back30_, back10_, play_button_, forward10_, forward30_, next_mark_}) {
        MoveWindow(button, x, bottom, kButton, kRow, TRUE);
        x += kButton + kGap;
    }
    MoveWindow(time_label_, x + kPad, bottom, 160, kRow, TRUE);
}

void PlayerPanel::show(int time, int length, bool playing)
{
    seekbar::setRange(seekbar_, length);
    seekbar::setPosition(seekbar_, time);

    setText(time_label_, widen(timecode::format(time) + " / " + timecode::format(length)));
    setText(play_button_, playing ? L"Pause" : L"Play");
}

void PlayerPanel::showMarks(const std::vector<Bookmark>& marks, int start)
{
    seekbar::setMarks(seekbar_, marks, start);
}
