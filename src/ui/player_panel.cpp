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
    prevMark_ = addControl(context, L"BUTTON", L"Prev", BS_PUSHBUTTON, kPrevMark);
    back30_ = addControl(context, L"BUTTON", L"-30", BS_PUSHBUTTON, kBack30);
    back10_ = addControl(context, L"BUTTON", L"-10", BS_PUSHBUTTON, kBack10);
    playButton_ = addControl(context, L"BUTTON", L"Play", BS_PUSHBUTTON, kPlay);
    forward10_ = addControl(context, L"BUTTON", L"+10", BS_PUSHBUTTON, kForward10);
    forward30_ = addControl(context, L"BUTTON", L"+30", BS_PUSHBUTTON, kForward30);
    nextMark_ = addControl(context, L"BUTTON", L"Next", BS_PUSHBUTTON, kNextMark);
    timeLabel_ = addControl(context, L"STATIC", L"0:00 / 0:00", SS_CENTERIMAGE, 0);
}

void PlayerPanel::layout(int width, int height)
{
    int bottom = height - kPad - kRow;
    int left = kSide + kPad * 2;
    int centerWidth = std::max(0, width - kSide * 2 - kPad * 4);
    int seekY = bottom - kPad - kSeekHeight;

    MoveWindow(video_, left, kPad, centerWidth, std::max(0, seekY - kPad * 2), TRUE);
    MoveWindow(seekbar_, left, seekY, centerWidth, kSeekHeight, TRUE);

    int x = left;
    for (HWND button : {prevMark_, back30_, back10_, playButton_, forward10_, forward30_, nextMark_}) {
        MoveWindow(button, x, bottom, kButton, kRow, TRUE);
        x += kButton + kGap;
    }
    MoveWindow(timeLabel_, x + kPad, bottom, 160, kRow, TRUE);
}

void PlayerPanel::show(int time, int length, bool playing)
{
    seekbar::setRange(seekbar_, length);
    seekbar::setPosition(seekbar_, time);

    setText(timeLabel_, widen(timecode::format(time) + " / " + timecode::format(length)));
    setText(playButton_, playing ? L"Pause" : L"Play");
}

void PlayerPanel::showMarks(const std::vector<Bookmark>& marks, int start)
{
    seekbar::setMarks(seekbar_, marks, start);
}
