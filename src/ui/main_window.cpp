#include "main_window.h"

#include <algorithm>
#include <cstdlib>
#include <memory>

#include <commctrl.h>
#include <commdlg.h>

#include "seekbar.h"
#include "text.h"
#include "timecode.h"

using namespace ui;

namespace {

const UINT_PTR kTimer = 1;

std::filesystem::path folderFromEnv(const wchar_t* name)
{
    const wchar_t* value = _wgetenv(name);
    return value ? std::filesystem::path(value) : std::filesystem::current_path();
}

}

MainWindow::MainWindow()
    : videos_folder_(folderFromEnv(L"USERPROFILE") / L"Videos" / L"Youtonomous")
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
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    wc.hIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON,
                                               GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    wc.lpszClassName = L"Youtonomous";
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(0, wc.lpszClassName, L"Youtonomous", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                            CW_USEDEFAULT, CW_USEDEFAULT, 1280, 760, nullptr, nullptr, instance, this);
    if (!hwnd_)
        return false;

    ShowWindow(hwnd_, show);
    return true;
}

LRESULT CALLBACK MainWindow::proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param)
{
    if (msg == WM_NCCREATE) {
        auto* self = static_cast<MainWindow*>(reinterpret_cast<CREATESTRUCTW*>(l_param)->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    auto* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self)
        return DefWindowProcW(hwnd, msg, w_param, l_param);
    return self->handle(msg, w_param, l_param);
}

LRESULT MainWindow::handle(UINT msg, WPARAM w_param, LPARAM l_param)
{
    switch (msg) {
    case WM_CREATE:
        createControls();
        library_.load();
        refreshLibrary();
        SetTimer(hwnd_, kTimer, 250, nullptr);
        if (!player_->ready())
            MessageBoxW(hwnd_, L"Could not start VLC.", L"Youtonomous", MB_ICONERROR);
        return 0;
    case WM_SIZE:
        layout(LOWORD(l_param), HIWORD(l_param));
        return 0;
    case WM_COMMAND:
        onCommand(LOWORD(w_param), HIWORD(w_param));
        return 0;
    case WM_TIMER:
        tick();
        return 0;
    case WM_SEEKBAR_SEEK:
        player_->seek(static_cast<int>(w_param));
        tick();
        return 0;
    case WM_LBUTTONDOWN:
        SetFocus(hwnd_);
        return 0;
    case WM_DOWNLOAD_PROGRESS:
        library_panel_.showProgress(static_cast<int>(w_param));
        library_panel_.showStatus(L"Downloading " + std::to_wstring(w_param) + L"%");
        return 0;
    case WM_DOWNLOAD_DONE:
        onDownloadDone(reinterpret_cast<DownloadResult*>(l_param));
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd_, kTimer);
        DeleteObject(font_);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd_, msg, w_param, l_param);
}

void MainWindow::createControls()
{
    NONCLIENTMETRICSW metrics{};
    metrics.cbSize = sizeof metrics;
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof metrics, &metrics, 0);
    font_ = CreateFontIndirectW(&metrics.lfMessageFont);

    Context context{hwnd_, instance_, font_};
    library_panel_.create(context);
    player_panel_.create(context);
    marks_panel_.create(context);

    player_->attach(player_panel_.videoWindow());
}

void MainWindow::layout(int width, int height)
{
    library_panel_.layout(height);
    player_panel_.layout(width, height);
    marks_panel_.layout(width, height);
}

void MainWindow::onCommand(int id, int code)
{
    switch (id) {
    case kOpen: openFile(); break;
    case kRemove: removeSelected(); break;
    case kDownload: download(); break;
    case kPrevMark: jumpMark(-1); break;
    case kBack30: player_->skip(-30); break;
    case kBack10: player_->skip(-10); break;
    case kPlay: player_->togglePause(); break;
    case kForward10: player_->skip(10); break;
    case kForward30: player_->skip(30); break;
    case kNextMark: jumpMark(1); break;
    case kSetStart: saveStartFromBox(); break;
    case kGoStart: goToStart(); break;
    case kAddMark: addMark(); break;
    case kDeleteMark: deleteSelectedMark(); break;
    case kLibrary:
        if (code == LBN_DBLCLK) {
            int index = library_panel_.selected(library_.videos().size());
            if (index >= 0)
                openVideo(library_.videos()[index].id);
        }
        break;
    case kMarks:
        if (code == LBN_SELCHANGE)
            jumpToSelectedMark();
        break;
    }

    if (code == BN_CLICKED || code == LBN_SELCHANGE || code == LBN_DBLCLK)
        SetFocus(hwnd_);
    tick();
}

void MainWindow::tick()
{
    player_panel_.show(player_->time(), player_->length(), player_->playing());
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
    std::string url = library_panel_.url();
    if (url.empty() || downloading_)
        return;

    if (url.find('"') != std::string::npos) {
        library_panel_.showStatus(L"That link has a quote mark in it.");
        return;
    }

    if (Video* existing = library_.findByUrl(url)) {
        library_panel_.showStatus(L"Already in the library.");
        openVideo(existing->id);
        return;
    }

    downloading_ = true;
    library_panel_.setDownloading(true);
    library_panel_.showProgress(0);
    library_panel_.showStatus(L"Starting...");
    startDownload(hwnd_, url, videos_folder_);
}

void MainWindow::onDownloadDone(DownloadResult* raw)
{
    std::unique_ptr<DownloadResult> result(raw);
    downloading_ = false;
    library_panel_.setDownloading(false);

    if (!result->ok) {
        library_panel_.showProgress(0);
        library_panel_.showStatus(widen(result->error));
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

    library_panel_.showProgress(100);
    library_panel_.showStatus(L"Done: " + widen(downloaded.title));
    library_panel_.clearUrl();
    openVideo(id);
}

void MainWindow::refreshLibrary()
{
    library_panel_.showVideos(library_.videos());
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

    current_id_ = id;
    player_->open(found->file, found->start);
    library_panel_.select(static_cast<int>(found - videos.begin()));
    SetWindowTextW(hwnd_, (L"Youtonomous - " + widen(found->title)).c_str());
    refreshMarks();
}

void MainWindow::removeSelected()
{
    int index = library_panel_.selected(library_.videos().size());
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
    return current_id_.empty() ? nullptr : library_.find(current_id_);
}

void MainWindow::refreshMarks()
{
    Video* video = current();
    marks_panel_.show(video);
    if (video)
        player_panel_.showMarks(video->marks, video->start);
    else
        player_panel_.showMarks({}, 0);
}

void MainWindow::addMark()
{
    Video* video = current();
    if (!video || !player_->loaded())
        return;

    std::string label = marks_panel_.takeLabel();
    if (label.empty())
        label = "Bookmark";

    Library::addMark(*video, {player_->time(), label, false});
    library_.save();
    refreshMarks();
}

void MainWindow::deleteSelectedMark()
{
    Video* video = current();
    if (!video)
        return;

    int index = marks_panel_.selected(video->marks.size());
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

    int index = marks_panel_.selected(video->marks.size());
    if (index >= 0)
        player_->seek(video->marks[index].time);
}

void MainWindow::jumpMark(int direction)
{
    Video* video = current();
    if (!video)
        return;

    const auto& marks = video->marks;
    int now = player_->time();

    if (direction > 0) {
        auto next = std::find_if(marks.begin(), marks.end(), [&](const Bookmark& mark) { return mark.time > now; });
        if (next != marks.end())
            player_->seek(next->time);
        return;
    }

    // The grace period lets repeated presses walk back past the mark just reached.
    auto previous = std::find_if(marks.rbegin(), marks.rend(), [&](const Bookmark& mark) { return mark.time < now - 2; });
    if (previous != marks.rend())
        player_->seek(previous->time);
}

void MainWindow::saveStartFromBox()
{
    Video* video = current();
    if (!video)
        return;

    std::optional<int> start = timecode::parse(marks_panel_.startText());
    if (!start) {
        MessageBeep(MB_ICONWARNING);
        marks_panel_.showStart(video->start);
        return;
    }
    setStart(*start);
}

void MainWindow::setStart(int seconds)
{
    Video* video = current();
    if (!video)
        return;

    int length = player_->length();
    if (length > 0)
        seconds = std::min(seconds, length - 1);

    video->start = std::max(seconds, 0);
    library_.save();
    refreshMarks();
}

void MainWindow::goToStart()
{
    if (Video* video = current())
        player_->seek(video->start);
}

bool MainWindow::handleKey(const MSG& msg)
{
    if (msg.message != WM_KEYDOWN || (msg.hwnd != hwnd_ && !IsChild(hwnd_, msg.hwnd)))
        return false;

    HWND focus = GetFocus();
    bool in_url = library_panel_.isUrlBox(focus);
    bool in_start = marks_panel_.isStartBox(focus);
    bool in_label = marks_panel_.isLabelBox(focus);
    bool typing = in_url || in_start || in_label;

    if (msg.wParam == VK_RETURN && typing) {
        if (in_url)
            download();
        else if (in_start)
            saveStartFromBox();
        else
            addMark();
        return true;
    }
    if (typing)
        return false;

    bool shift = GetKeyState(VK_SHIFT) < 0;

    switch (msg.wParam) {
    case VK_SPACE: player_->togglePause(); break;
    case VK_LEFT: player_->skip(shift ? -60 : -10); break;
    case VK_RIGHT: player_->skip(shift ? 60 : 10); break;
    case VK_OEM_4: jumpMark(-1); break;
    case VK_OEM_6: jumpMark(1); break;
    case 'B': addMark(); break;
    case 'S': setStart(player_->time()); break;
    case VK_HOME: goToStart(); break;
    default: return false;
    }

    tick();
    return true;
}
