#include "library_panel.h"

#include <algorithm>

#include <commctrl.h>

#include "text.h"

using namespace ui;

void LibraryPanel::create(const Context& context)
{
    const DWORD list_style = WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT;

    url_edit_ = addControl(context, L"EDIT", L"", ES_AUTOHSCROLL, kUrl, WS_EX_CLIENTEDGE);
    download_button_ = addControl(context, L"BUTTON", L"Download", BS_PUSHBUTTON, kDownload);
    progress_ = addControl(context, PROGRESS_CLASSW, L"", 0, 0);
    status_ = addControl(context, L"STATIC", L"Paste a YouTube link above.", SS_LEFT, 0);
    label_ = addControl(context, L"STATIC", L"Library", SS_LEFT, 0);
    list_ = addControl(context, L"LISTBOX", L"", list_style, kLibrary, WS_EX_CLIENTEDGE);
    open_button_ = addControl(context, L"BUTTON", L"Open file...", BS_PUSHBUTTON, kOpen);
    remove_button_ = addControl(context, L"BUTTON", L"Remove", BS_PUSHBUTTON, kRemove);

    SendMessageW(progress_, PBM_SETRANGE32, 0, 100);
}

void LibraryPanel::layout(int height)
{
    int bottom = height - kPad - kRow;
    int half = (kSide - kGap) / 2;

    int y = kPad;
    MoveWindow(url_edit_, kPad, y, kSide - kSmallButton - kGap, kRow, TRUE);
    MoveWindow(download_button_, kPad + kSide - kSmallButton, y, kSmallButton, kRow, TRUE);
    y += kRow + kGap;
    MoveWindow(progress_, kPad, y, kSide, 14, TRUE);
    y += 14 + kGap;
    MoveWindow(status_, kPad, y, kSide, kLabel * 2, TRUE);
    y += kLabel * 2 + kPad;
    MoveWindow(label_, kPad, y, kSide, kLabel, TRUE);
    y += kLabel;
    MoveWindow(list_, kPad, y, kSide, std::max(0, bottom - kGap - y), TRUE);
    MoveWindow(open_button_, kPad, bottom, half, kRow, TRUE);
    MoveWindow(remove_button_, kPad + half + kGap, bottom, half, kRow, TRUE);
}

std::string LibraryPanel::url() const
{
    return trim(narrow(textOf(url_edit_)));
}

void LibraryPanel::clearUrl()
{
    SetWindowTextW(url_edit_, L"");
}

void LibraryPanel::showProgress(int percent)
{
    SendMessageW(progress_, PBM_SETPOS, percent, 0);
}

void LibraryPanel::showStatus(const std::wstring& text)
{
    setText(status_, text);
}

void LibraryPanel::setDownloading(bool downloading)
{
    EnableWindow(download_button_, downloading ? FALSE : TRUE);
}

void LibraryPanel::showVideos(const std::vector<Video>& videos)
{
    SendMessageW(list_, LB_RESETCONTENT, 0, 0);
    for (const Video& video : videos)
        SendMessageW(list_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(widen(video.title).c_str()));
}

void LibraryPanel::select(int index)
{
    SendMessageW(list_, LB_SETCURSEL, index, 0);
}

int LibraryPanel::selected(size_t count) const
{
    return selectedIndex(list_, count);
}
