#include "window.h"

#include <algorithm>
#include <cstdlib>
#include <memory>

#include <commctrl.h>
#include <commdlg.h>

#include "seekbar.h"
#include "text.h"
#include "timecode.h"

namespace {

enum ControlId {
    kOpen = 100,
    kRemove,
    kUrl,
    kDownload,
    kLibrary,
    kPrevMark,
    kBack30,
    kBack10,
    kPlay,
    kForward10,
    kForward30,
    kNextMark,
    kStart,
    kSetStart,
    kGoStart,
    kMarks,
    kMarkLabel,
    kAddMark,
    kDeleteMark,
};

const UINT_PTR kTimer = 1;

const int kPad = 8;
const int kGap = 4;
const int kSide = 270;
const int kRow = 26;
const int kLabel = 20;
const int kButton = 56;
const int kSmallButton = 80;
const int kSeekHeight = 20;

std::filesystem::path folderFromEnv(const wchar_t* name)
{
    const wchar_t* value = _wgetenv(name);
    return value ? std::filesystem::path(value) : std::filesystem::current_path();
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

MainWindow::MainWindow()
    : videosFolder_(folderFromEnv(L"USERPROFILE") / L"Videos" / L"Youtonomous")
    , library_(folderFromEnv(L"APPDATA") / L"Youtonomous" / L"library.json")
{
}

bool MainWindow::create(HINSTANCE instance, int show)
{
    instance_ = instance;

    INITCOMMONCONTROLSEX controls{sizeof controls, ICC_PROGRESS_CLASS};
    InitCommonControlsEx(&controls);

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
        library_.load();
        refreshLibrary();
        SetTimer(hwnd_, kTimer, 250, nullptr);
        if (!player_.ready())
            MessageBoxW(hwnd_, L"Could not start VLC.", L"Youtonomous", MB_ICONERROR);
        return 0;
    case WM_SIZE:
        layout(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_COMMAND:
        onCommand(LOWORD(wParam), HIWORD(wParam));
        return 0;
    case WM_TIMER:
        tick();
        return 0;
    case WM_SEEKBAR_SEEK:
        player_.seek(static_cast<int>(wParam));
        tick();
        return 0;
    case WM_DOWNLOAD_PROGRESS:
        SendMessageW(progress_, PBM_SETPOS, wParam, 0);
        setText(status_, L"Downloading " + std::to_wstring(wParam) + L"%");
        return 0;
    case WM_DOWNLOAD_DONE:
        onDownloadDone(reinterpret_cast<DownloadResult*>(lParam));
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd_, kTimer);
        DeleteObject(font_);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

HWND MainWindow::addControl(const wchar_t* type, const wchar_t* text, DWORD style, int id, DWORD exStyle)
{
    HWND control = CreateWindowExW(exStyle, type, text, WS_CHILD | WS_VISIBLE | style,
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

    const DWORD listStyle = WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT;

    urlEdit_ = addControl(L"EDIT", L"", ES_AUTOHSCROLL, kUrl, WS_EX_CLIENTEDGE);
    downloadButton_ = addControl(L"BUTTON", L"Download", BS_PUSHBUTTON, kDownload);
    progress_ = addControl(PROGRESS_CLASSW, L"", 0, 0);
    status_ = addControl(L"STATIC", L"Paste a YouTube link above.", SS_LEFT, 0);
    libraryLabel_ = addControl(L"STATIC", L"Library", SS_LEFT, 0);
    libraryList_ = addControl(L"LISTBOX", L"", listStyle, kLibrary, WS_EX_CLIENTEDGE);
    openButton_ = addControl(L"BUTTON", L"Open file...", BS_PUSHBUTTON, kOpen);
    removeButton_ = addControl(L"BUTTON", L"Remove", BS_PUSHBUTTON, kRemove);

    video_ = addControl(L"STATIC", L"", SS_BLACKRECT, 0);
    seekbar_ = seekbar::create(hwnd_, instance_);
    prevMark_ = addControl(L"BUTTON", L"Prev", BS_PUSHBUTTON, kPrevMark);
    back30_ = addControl(L"BUTTON", L"-30", BS_PUSHBUTTON, kBack30);
    back10_ = addControl(L"BUTTON", L"-10", BS_PUSHBUTTON, kBack10);
    playButton_ = addControl(L"BUTTON", L"Play", BS_PUSHBUTTON, kPlay);
    forward10_ = addControl(L"BUTTON", L"+10", BS_PUSHBUTTON, kForward10);
    forward30_ = addControl(L"BUTTON", L"+30", BS_PUSHBUTTON, kForward30);
    nextMark_ = addControl(L"BUTTON", L"Next", BS_PUSHBUTTON, kNextMark);
    timeLabel_ = addControl(L"STATIC", L"0:00 / 0:00", SS_CENTERIMAGE, 0);

    startLabel_ = addControl(L"STATIC", L"Start at", SS_LEFT, 0);
    startEdit_ = addControl(L"EDIT", L"", ES_AUTOHSCROLL, kStart, WS_EX_CLIENTEDGE);
    setStartButton_ = addControl(L"BUTTON", L"Set", BS_PUSHBUTTON, kSetStart);
    goStartButton_ = addControl(L"BUTTON", L"Go", BS_PUSHBUTTON, kGoStart);

    marksLabel_ = addControl(L"STATIC", L"Bookmarks", SS_LEFT, 0);
    marksList_ = addControl(L"LISTBOX", L"", listStyle, kMarks, WS_EX_CLIENTEDGE);
    markLabelEdit_ = addControl(L"EDIT", L"", ES_AUTOHSCROLL, kMarkLabel, WS_EX_CLIENTEDGE);
    addMarkButton_ = addControl(L"BUTTON", L"Add", BS_PUSHBUTTON, kAddMark);
    deleteMarkButton_ = addControl(L"BUTTON", L"Delete", BS_PUSHBUTTON, kDeleteMark);

    SendMessageW(progress_, PBM_SETRANGE32, 0, 100);
    SendMessageW(startEdit_, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"0:00"));
    SendMessageW(markLabelEdit_, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Label (optional)"));
    player_.attach(video_);
}

void MainWindow::layout(int width, int height)
{
    int controlsY = height - kPad - kRow;
    int half = (kSide - kGap) / 2;

    int y = kPad;
    MoveWindow(urlEdit_, kPad, y, kSide - kSmallButton - kGap, kRow, TRUE);
    MoveWindow(downloadButton_, kPad + kSide - kSmallButton, y, kSmallButton, kRow, TRUE);
    y += kRow + kGap;
    MoveWindow(progress_, kPad, y, kSide, 14, TRUE);
    y += 14 + kGap;
    MoveWindow(status_, kPad, y, kSide, kLabel * 2, TRUE);
    y += kLabel * 2 + kPad;
    MoveWindow(libraryLabel_, kPad, y, kSide, kLabel, TRUE);
    y += kLabel;
    MoveWindow(libraryList_, kPad, y, kSide, std::max(0, controlsY - kGap - y), TRUE);
    MoveWindow(openButton_, kPad, controlsY, half, kRow, TRUE);
    MoveWindow(removeButton_, kPad + half + kGap, controlsY, half, kRow, TRUE);

    int centerX = kSide + kPad * 2;
    int centerWidth = std::max(0, width - kSide * 2 - kPad * 4);
    int seekY = controlsY - kPad - kSeekHeight;

    MoveWindow(video_, centerX, kPad, centerWidth, std::max(0, seekY - kPad * 2), TRUE);
    MoveWindow(seekbar_, centerX, seekY, centerWidth, kSeekHeight, TRUE);

    int x = centerX;
    for (HWND button : {prevMark_, back30_, back10_, playButton_, forward10_, forward30_, nextMark_}) {
        MoveWindow(button, x, controlsY, kButton, kRow, TRUE);
        x += kButton + kGap;
    }
    MoveWindow(timeLabel_, x + kPad, controlsY, 160, kRow, TRUE);

    int rightX = width - kPad - kSide;
    int labelRowY = controlsY - kGap - kRow;

    y = kPad;
    MoveWindow(startLabel_, rightX, y, kSide, kLabel, TRUE);
    y += kLabel;
    MoveWindow(startEdit_, rightX, y, kSide - kButton * 2 - kGap * 2, kRow, TRUE);
    MoveWindow(setStartButton_, rightX + kSide - kButton * 2 - kGap, y, kButton, kRow, TRUE);
    MoveWindow(goStartButton_, rightX + kSide - kButton, y, kButton, kRow, TRUE);
    y += kRow + kPad;
    MoveWindow(marksLabel_, rightX, y, kSide, kLabel, TRUE);
    y += kLabel;
    MoveWindow(marksList_, rightX, y, kSide, std::max(0, labelRowY - kGap - y), TRUE);
    MoveWindow(markLabelEdit_, rightX, labelRowY, kSide - kSmallButton - kGap, kRow, TRUE);
    MoveWindow(addMarkButton_, rightX + kSide - kSmallButton, labelRowY, kSmallButton, kRow, TRUE);
    MoveWindow(deleteMarkButton_, rightX, controlsY, kSide, kRow, TRUE);
}

void MainWindow::onCommand(int id, int code)
{
    switch (id) {
    case kOpen: openFile(); break;
    case kRemove: removeSelected(); break;
    case kDownload: download(); break;
    case kPrevMark: jumpMark(-1); break;
    case kBack30: player_.skip(-30); break;
    case kBack10: player_.skip(-10); break;
    case kPlay: player_.togglePause(); break;
    case kForward10: player_.skip(10); break;
    case kForward30: player_.skip(30); break;
    case kNextMark: jumpMark(1); break;
    case kSetStart: saveStartFromBox(); break;
    case kGoStart: goToStart(); break;
    case kAddMark: addMark(); break;
    case kDeleteMark: deleteSelectedMark(); break;
    case kLibrary:
        if (code == LBN_DBLCLK) {
            int index = selectedIndex(libraryList_, library_.videos().size());
            if (index >= 0)
                openVideo(library_.videos()[index].id);
        }
        break;
    case kMarks:
        if (code == LBN_SELCHANGE)
            jumpToSelectedMark();
        break;
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
    for (const Video& video : library_.videos()) {
        std::error_code error;
        if (std::filesystem::equivalent(std::filesystem::u8path(video.file), path, error)) {
            openVideo(video.id);
            return;
        }
    }

    Video video;
    video.id = narrow(path);
    video.title = narrow(std::filesystem::path(path).stem().wstring());
    video.file = video.id;
    library_.add(video);
    library_.save();
    refreshLibrary();
    openVideo(video.id);
}

void MainWindow::download()
{
    std::string url = trim(narrow(textOf(urlEdit_)));
    if (url.empty() || downloading_)
        return;

    if (url.find('"') != std::string::npos) {
        setText(status_, L"That link has a quote mark in it.");
        return;
    }

    if (Video* existing = library_.findByUrl(url)) {
        setText(status_, L"Already in the library.");
        openVideo(existing->id);
        return;
    }

    downloading_ = true;
    EnableWindow(downloadButton_, FALSE);
    SendMessageW(progress_, PBM_SETPOS, 0, 0);
    setText(status_, L"Starting...");
    startDownload(hwnd_, url, videosFolder_);
}

void MainWindow::onDownloadDone(DownloadResult* raw)
{
    std::unique_ptr<DownloadResult> result(raw);
    downloading_ = false;
    EnableWindow(downloadButton_, TRUE);

    if (!result->ok) {
        SendMessageW(progress_, PBM_SETPOS, 0, 0);
        setText(status_, widen(result->error));
        return;
    }

    Video& downloaded = result->video;
    std::string id = downloaded.id;

    if (Video* existing = library_.find(id)) {
        existing->url = downloaded.url;
        existing->file = downloaded.file;
        existing->duration = downloaded.duration;
        Library::importChapters(*existing, downloaded.marks);
    } else {
        library_.add(downloaded);
    }
    library_.save();
    refreshLibrary();

    SendMessageW(progress_, PBM_SETPOS, 100, 0);
    setText(status_, L"Done: " + widen(downloaded.title));
    SetWindowTextW(urlEdit_, L"");
    openVideo(id);
}

void MainWindow::refreshLibrary()
{
    SendMessageW(libraryList_, LB_RESETCONTENT, 0, 0);
    for (const Video& video : library_.videos())
        SendMessageW(libraryList_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(widen(video.title).c_str()));
}

void MainWindow::openVideo(const std::string& id)
{
    const auto& videos = library_.videos();
    auto found = std::find_if(videos.begin(), videos.end(), [&](const Video& video) { return video.id == id; });
    if (found == videos.end())
        return;

    std::error_code error;
    if (!std::filesystem::exists(std::filesystem::u8path(found->file), error)) {
        std::wstring message = L"File not found:\n" + widen(found->file);
        MessageBoxW(hwnd_, message.c_str(), L"Youtonomous", MB_ICONWARNING);
        return;
    }

    currentId_ = id;
    player_.open(found->file, found->start);
    SendMessageW(libraryList_, LB_SETCURSEL, found - videos.begin(), 0);
    SetWindowTextW(hwnd_, (L"Youtonomous - " + widen(found->title)).c_str());
    refreshMarks();
}

void MainWindow::removeSelected()
{
    int index = selectedIndex(libraryList_, library_.videos().size());
    if (index < 0)
        return;

    const Video& video = library_.videos()[index];
    std::wstring question = L"Remove \"" + widen(video.title) + L"\" from the library?\nThe file stays on disk.";
    if (MessageBoxW(hwnd_, question.c_str(), L"Youtonomous", MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    std::string id = video.id;
    library_.remove(id);
    library_.save();
    refreshLibrary();
    refreshMarks();
}

Video* MainWindow::current()
{
    return currentId_.empty() ? nullptr : library_.find(currentId_);
}

void MainWindow::refreshMarks()
{
    SendMessageW(marksList_, LB_RESETCONTENT, 0, 0);

    Video* video = current();
    if (!video) {
        seekbar::setMarks(seekbar_, {}, 0);
        setText(startEdit_, L"");
        return;
    }

    for (const Bookmark& mark : video->marks) {
        std::wstring line = widen(timecode::format(mark.time) + "  " + mark.label);
        SendMessageW(marksList_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(line.c_str()));
    }
    seekbar::setMarks(seekbar_, video->marks, video->start);
    setText(startEdit_, widen(timecode::format(video->start)));
}

void MainWindow::addMark()
{
    Video* video = current();
    if (!video || !player_.loaded())
        return;

    std::string label = trim(narrow(textOf(markLabelEdit_)));
    if (label.empty())
        label = "Bookmark";

    Library::addMark(*video, {player_.time(), label, false});
    library_.save();
    SetWindowTextW(markLabelEdit_, L"");
    refreshMarks();
}

void MainWindow::deleteSelectedMark()
{
    Video* video = current();
    if (!video)
        return;

    int index = selectedIndex(marksList_, video->marks.size());
    if (index < 0)
        return;

    video->marks.erase(video->marks.begin() + index);
    library_.save();
    refreshMarks();
}

void MainWindow::jumpToSelectedMark()
{
    Video* video = current();
    if (!video)
        return;

    int index = selectedIndex(marksList_, video->marks.size());
    if (index >= 0)
        player_.seek(video->marks[index].time);
}

void MainWindow::jumpMark(int direction)
{
    Video* video = current();
    if (!video)
        return;

    const auto& marks = video->marks;
    int now = player_.time();

    if (direction > 0) {
        auto next = std::find_if(marks.begin(), marks.end(),
                                 [&](const Bookmark& mark) { return mark.time > now; });
        if (next != marks.end())
            player_.seek(next->time);
        return;
    }

    // The grace period lets repeated presses walk back past the mark just reached.
    auto previous = std::find_if(marks.rbegin(), marks.rend(),
                                 [&](const Bookmark& mark) { return mark.time < now - 2; });
    if (previous != marks.rend())
        player_.seek(previous->time);
}

void MainWindow::saveStartFromBox()
{
    Video* video = current();
    if (!video)
        return;

    std::optional<int> start = timecode::parse(narrow(textOf(startEdit_)));
    if (!start) {
        MessageBeep(MB_ICONWARNING);
        setText(startEdit_, widen(timecode::format(video->start)));
        return;
    }
    setStart(*start);
}

void MainWindow::setStart(int seconds)
{
    Video* video = current();
    if (!video)
        return;

    int length = player_.length();
    if (length > 0)
        seconds = std::min(seconds, length - 1);

    video->start = std::max(seconds, 0);
    library_.save();
    refreshMarks();
}

void MainWindow::goToStart()
{
    if (Video* video = current())
        player_.seek(video->start);
}
