#include "library.h"

#include <algorithm>
#include <climits>
#include <fstream>

#include <nlohmann/json.hpp>

using nlohmann::json;

static void sortMarks(Video& video)
{
    std::stable_sort(video.marks.begin(), video.marks.end(),
                     [](const Bookmark& a, const Bookmark& b) { return a.time < b.time; });
}

static json toJson(const Video& video)
{
    json marks = json::array();
    for (const Bookmark& mark : video.marks)
        marks.push_back({{"time", mark.time}, {"label", mark.label}, {"chapter", mark.chapter}});

    return {
        {"id", video.id},
        {"url", video.url},
        {"title", video.title},
        {"file", video.file},
        {"duration", video.duration},
        {"start", video.start},
        {"position", video.position},
        {"marks", marks},
    };
}

static int readPosition(const json& item)
{
    if (!item.contains("position") || !item["position"].is_number_integer())
        return 0;
    long long position = item["position"].get<long long>();
    return position > 0 && position <= INT_MAX ? static_cast<int>(position) : 0;
}

static Video fromJson(const json& item)
{
    Video video;
    video.id = item.value("id", "");
    video.url = item.value("url", "");
    video.title = item.value("title", "");
    video.file = item.value("file", "");
    video.duration = item.value("duration", 0);
    video.start = item.value("start", 0);
    video.position = readPosition(item);

    for (const json& mark : item.value("marks", json::array()))
        video.marks.push_back({mark.value("time", 0), mark.value("label", ""), mark.value("chapter", false)});

    sortMarks(video);
    return video;
}

Library::Library(std::filesystem::path file)
    : file_(std::move(file))
{
}

bool Library::load()
{
    videos_.clear();

    std::ifstream in(file_);
    if (!in)
        return false;

    json items = json::parse(in, nullptr, false);
    if (items.is_discarded() || !items.is_array())
        return false;

    for (const json& item : items) {
        if (item.is_object())
            videos_.push_back(fromJson(item));
    }
    return true;
}

bool Library::save() const
{
    std::error_code error;
    std::filesystem::create_directories(file_.parent_path(), error);

    json items = json::array();
    for (const Video& video : videos_)
        items.push_back(toJson(video));

    std::filesystem::path temp = file_;
    temp += ".tmp";
    {
        std::ofstream out(temp);
        out << items.dump(2);
        if (!out)
            return false;
    }

    std::filesystem::rename(temp, file_, error);
    return !error;
}

Video* Library::find(const std::string& id)
{
    for (Video& video : videos_) {
        if (video.id == id)
            return &video;
    }
    return nullptr;
}

Video* Library::findByUrl(const std::string& url)
{
    for (Video& video : videos_) {
        if (!url.empty() && video.url == url)
            return &video;
    }
    return nullptr;
}

void Library::add(Video video)
{
    sortMarks(video);
    if (Video* existing = find(video.id))
        *existing = std::move(video);
    else
        videos_.push_back(std::move(video));
}

void Library::remove(const std::string& id)
{
    videos_.erase(std::remove_if(videos_.begin(), videos_.end(),
                                 [&](const Video& video) { return video.id == id; }),
                  videos_.end());
}

void Library::addMark(Video& video, Bookmark mark)
{
    video.marks.push_back(std::move(mark));
    sortMarks(video);
}

void Library::importChapters(Video& video, const std::vector<Bookmark>& chapters)
{
    video.marks.erase(std::remove_if(video.marks.begin(), video.marks.end(),
                                     [](const Bookmark& mark) { return mark.chapter; }),
                      video.marks.end());

    for (Bookmark chapter : chapters) {
        chapter.chapter = true;
        video.marks.push_back(std::move(chapter));
    }
    sortMarks(video);
}
