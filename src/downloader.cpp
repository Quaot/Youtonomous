#include "downloader.h"

#include <cstdlib>
#include <functional>
#include <memory>
#include <thread>

#include <nlohmann/json.hpp>

#include "text.h"

using nlohmann::json;

static bool runProcess(std::wstring command_line, const std::function<void(const std::string&)>& on_line)
{
    SECURITY_ATTRIBUTES security{sizeof security, nullptr, TRUE};
    HANDLE read_end = nullptr;
    HANDLE write_end = nullptr;
    if (!CreatePipe(&read_end, &write_end, &security, 0))
        return false;
    SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startup{};
    startup.cb = sizeof startup;
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = write_end;
    startup.hStdError = write_end;

    PROCESS_INFORMATION process{};
    BOOL started = CreateProcessW(nullptr, command_line.data(), nullptr, nullptr, TRUE,
                                  CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
    CloseHandle(write_end);
    if (!started) {
        CloseHandle(read_end);
        return false;
    }

    std::string pending;
    char buffer[4096];
    DWORD count = 0;
    while (ReadFile(read_end, buffer, sizeof buffer, &count, nullptr) && count > 0) {
        pending.append(buffer, count);
        size_t end;
        while ((end = pending.find('\n')) != std::string::npos) {
            std::string line = pending.substr(0, end);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            on_line(line);
            pending.erase(0, end + 1);
        }
    }
    if (!pending.empty())
        on_line(pending);
    CloseHandle(read_end);

    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    return exit_code == 0;
}

static std::wstring buildCommand(const std::string& url, const std::filesystem::path& folder)
{
    std::wstring output = (folder / L"%(id)s.%(ext)s").wstring();

    std::wstring command = L"yt-dlp --newline --no-playlist --encoding utf-8";
    command += L" -f \"bv*[height<=1080]+ba/b\" --merge-output-format mp4";
    command += L" --progress --progress-template \"download:progress %(progress._percent_str)s\"";
    command += L" --print \"after_move:%(.{id,title,duration,chapters,filepath})j\"";
    command += L" -o \"" + output + L"\" -- \"" + widen(url) + L"\"";
    return command;
}

static int percentOf(const std::string& line)
{
    size_t digit = line.find_first_of("0123456789");
    if (digit == std::string::npos)
        return 0;
    return static_cast<int>(std::strtod(line.c_str() + digit, nullptr));
}

static bool readVideo(const std::string& line, const std::string& url, Video& video)
{
    json info = json::parse(line, nullptr, false);
    if (info.is_discarded() || !info.is_object())
        return false;

    video.id = info.value("id", "");
    video.url = url;
    video.title = info.value("title", video.id);
    video.file = info.value("filepath", "");

    if (info.contains("duration") && info["duration"].is_number())
        video.duration = static_cast<int>(info["duration"].get<double>());

    if (info.contains("chapters") && info["chapters"].is_array()) {
        for (const json& chapter : info["chapters"]) {
            Bookmark mark;
            if (chapter.contains("start_time") && chapter["start_time"].is_number())
                mark.time = static_cast<int>(chapter["start_time"].get<double>());
            if (chapter.contains("title") && chapter["title"].is_string())
                mark.label = chapter["title"].get<std::string>();
            mark.chapter = true;
            video.marks.push_back(mark);
        }
    }
    return !video.id.empty() && !video.file.empty();
}

void startDownload(HWND notify, const std::string& url, const std::filesystem::path& folder)
{
    std::thread([notify, url, folder] {
        auto result = std::make_unique<DownloadResult>();
        std::string last_error;
        bool got_video = false;

        std::error_code error;
        std::filesystem::create_directories(folder, error);

        bool ran = runProcess(buildCommand(url, folder), [&](const std::string& line) {
            if (line.rfind("progress ", 0) == 0)
                PostMessageW(notify, WM_DOWNLOAD_PROGRESS, percentOf(line), 0);
            else if (line.rfind("{", 0) == 0)
                got_video = readVideo(line, url, result->video);
            else if (line.rfind("ERROR:", 0) == 0)
                last_error = line;
        });

        result->ok = ran && got_video;
        if (!result->ok)
            result->error = last_error.empty() ? "yt-dlp failed. Is it installed and on PATH?" : last_error;

        if (PostMessageW(notify, WM_DOWNLOAD_DONE, 0, reinterpret_cast<LPARAM>(result.get())))
            result.release();
    }).detach();
}
