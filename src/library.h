#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct Bookmark {
    int time = 0;
    std::string label;
    bool chapter = false;
};

struct Video {
    std::string id;
    std::string url;
    std::string title;
    std::string file;
    int duration = 0;
    int start = 0;
    std::vector<Bookmark> marks;
};

class Library {
public:
    explicit Library(std::filesystem::path file);

    bool load();
    bool save() const;

    const std::vector<Video>& videos() const { return videos_; }
    Video* find(const std::string& id);
    Video* findByUrl(const std::string& url);

    void add(Video video);
    void remove(const std::string& id);

    static void addMark(Video& video, Bookmark mark);
    static void importChapters(Video& video, const std::vector<Bookmark>& chapters);

private:
    std::filesystem::path file_;
    std::vector<Video> videos_;
};
