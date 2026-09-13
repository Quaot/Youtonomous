#include <cstdio>
#include <filesystem>
#include <string>

#include "library.h"
#include "timecode.h"

static int failures = 0;

static void check(bool ok, const char* name)
{
    if (!ok) {
        std::printf("FAIL: %s\n", name);
        ++failures;
    }
}

static void testParse()
{
    check(timecode::parse("270") == 270, "plain seconds");
    check(timecode::parse("4:30") == 270, "minutes and seconds");
    check(timecode::parse("1:02:03") == 3723, "hours");
    check(timecode::parse(" 0:05 ") == 5, "spaces");
    check(timecode::parse("4m30s") == 270, "units");
    check(timecode::parse("1h") == 3600, "hours unit");
    check(timecode::parse("4m30") == 270, "trailing seconds");

    check(!timecode::parse(""), "empty");
    check(!timecode::parse("1:75"), "seconds over 59");
    check(!timecode::parse("1::2"), "double colon");
    check(!timecode::parse("1:2:3:4"), "too many parts");
    check(!timecode::parse("abc"), "letters");
    check(!timecode::parse("-5"), "negative");
}

static void testFormat()
{
    check(timecode::format(0) == "0:00", "zero");
    check(timecode::format(270) == "4:30", "under an hour");
    check(timecode::format(3723) == "1:02:03", "over an hour");
    check(timecode::format(-3) == "0:00", "negative");
}

static void testLibrary()
{
    std::filesystem::path file = std::filesystem::temp_directory_path() / "youtonomous-test" / "library.json";
    std::filesystem::remove_all(file.parent_path());

    Video video;
    video.id = "abc";
    video.url = "https://youtu.be/abc";
    video.title = "Study with me";
    video.file = "C:/videos/abc.mp4";
    video.duration = 10800;
    video.start = 270;
    Library::addMark(video, {600, "Mine", false});
    Library::importChapters(video, {{0, "Intro", false}, {900, "Session 1", false}});

    check(video.marks.size() == 3, "chapters added");
    check(video.marks[0].label == "Intro" && video.marks[0].chapter, "marks sorted");

    Library::importChapters(video, {{60, "New intro", false}});
    check(video.marks.size() == 2, "old chapters replaced");
    check(video.marks[1].label == "Mine", "own marks kept");

    Library library(file);
    library.add(video);
    check(library.save(), "save");

    Library loaded(file);
    check(loaded.load(), "load");
    check(loaded.videos().size() == 1, "one video");

    Video* found = loaded.find("abc");
    check(found != nullptr, "find by id");
    if (found) {
        check(found->title == "Study with me", "title");
        check(found->start == 270, "start");
        check(found->marks.size() == 2, "marks");
    }
    check(loaded.findByUrl("https://youtu.be/abc") == found, "find by url");

    loaded.remove("abc");
    check(loaded.videos().empty(), "remove");

    Library missing(file.parent_path() / "nope.json");
    check(!missing.load(), "missing file");

    std::filesystem::remove_all(file.parent_path());
}

int main()
{
    testParse();
    testFormat();
    testLibrary();

    if (failures == 0)
        std::printf("All tests passed\n");
    return failures == 0 ? 0 : 1;
}
