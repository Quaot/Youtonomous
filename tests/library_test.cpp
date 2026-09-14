#include <climits>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <windows.h>

#include <nlohmann/json.hpp>

#include "check.h"
#include "library.h"

namespace fs = std::filesystem;

static fs::path root;

static void writeFile(const fs::path& path, const std::string& content)
{
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << content;
}

static std::string readFile(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

static bool tryLoad(Library& library, const std::string& name)
{
    try {
        return library.load();
    } catch (...) {
        check(false, "load() does not throw: " + name);
        return false;
    }
}

static bool trySave(const Library& library, const std::string& name)
{
    try {
        return library.save();
    } catch (...) {
        check(false, "save() does not throw: " + name);
        return false;
    }
}

static Bookmark mark(int time, const std::string& label, bool chapter)
{
    Bookmark result;
    result.time = time;
    result.label = label;
    result.chapter = chapter;
    return result;
}

static Video makeVideo(const std::string& id, const std::string& title)
{
    Video video;
    video.id = id;
    video.url = "https://www.youtube.com/watch?v=" + id;
    video.title = title;
    video.file = "C:\\Videos\\Youtonomous\\" + id + ".mp4";
    video.duration = 600;
    video.start = 30;
    return video;
}

static bool sameMarks(const std::vector<Bookmark>& a, const std::vector<Bookmark>& b)
{
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].time != b[i].time || a[i].label != b[i].label || a[i].chapter != b[i].chapter)
            return false;
    return true;
}

static bool sameVideo(const Video& a, const Video& b)
{
    return a.id == b.id && a.url == b.url && a.title == b.title && a.file == b.file
        && a.duration == b.duration && a.start == b.start && a.position == b.position && sameMarks(a.marks, b.marks);
}

static std::string ids(const Library& library)
{
    std::string result;
    for (const Video& video : library.videos())
        result += (result.empty() ? "" : ",") + video.id;
    return result;
}

static std::string describe(const std::vector<Bookmark>& marks)
{
    std::string result;
    for (const Bookmark& m : marks)
        result += std::to_string(m.time) + (m.chapter ? "[chapter]" : "[user]") + m.label + " ";
    return result;
}

static bool sortedByTime(const std::vector<Bookmark>& marks)
{
    for (size_t i = 1; i < marks.size(); ++i)
        if (marks[i].time < marks[i - 1].time)
            return false;
    return true;
}

static int countLabel(const std::vector<Bookmark>& marks, const std::string& label, bool chapter)
{
    int count = 0;
    for (const Bookmark& m : marks)
        if (m.label == label && m.chapter == chapter)
            ++count;
    return count;
}

static void testMissingFile()
{
    Library library(root / "missing" / "library.json");
    check(library.videos().empty(), "new library is empty");
    check(!tryLoad(library, "missing file"), "load() of a missing file returns false");
    check(library.videos().empty(), "library is empty after loading a missing file");
    check(library.find("anything") == nullptr, "find() on an empty library returns nullptr");
    check(library.findByUrl("https://example.com") == nullptr, "findByUrl() on an empty library returns nullptr");
    check(library.findByUrl("") == nullptr, "findByUrl(\"\") on an empty library returns nullptr");

    Library filled(root / "missing2" / "library.json");
    filled.add(makeVideo("a", "Alpha"));
    check(!tryLoad(filled, "missing file with videos in memory"), "load() of a missing file returns false with videos in memory");
    check(filled.videos().empty(), "failed load() of a missing file empties the in-memory videos");
}

static void testAddAndFind()
{
    Library library(root / "unused" / "library.json");
    library.add(makeVideo("zulu", "Zulu"));
    library.add(makeVideo("alpha", "Alpha"));
    library.add(makeVideo("mike", "Mike"));
    check(ids(library) == "zulu,alpha,mike", "videos() keeps insertion order, got " + ids(library));

    Video* alpha = library.find("alpha");
    check(alpha && alpha->title == "Alpha", "find(\"alpha\") returns that video");
    check(library.find("bravo") == nullptr, "find() of an unknown id returns nullptr");
    check(library.find("") == nullptr, "find(\"\") returns nullptr");

    Video* mike = library.findByUrl("https://www.youtube.com/watch?v=mike");
    check(mike && mike->id == "mike", "findByUrl() returns the video with that url");
    check(library.findByUrl("https://www.youtube.com/watch?v=nope") == nullptr, "findByUrl() of an unknown url returns nullptr");

    Video no_url = makeVideo("local", "Local file");
    no_url.url = "";
    library.add(no_url);
    check(library.findByUrl("") == nullptr, "findByUrl(\"\") never matches, even when a video has an empty url");
    check(library.find("local") != nullptr, "a video without url can be found by id");

    Video* found = library.find("alpha");
    if (found)
        found->title = "Changed";
    check(library.videos().size() == 4 && library.videos()[1].title == "Changed", "find() returns a pointer to the stored video");
}

static void testReplace()
{
    Library library(root / "unused" / "library.json");
    library.add(makeVideo("a", "Old"));
    library.add(makeVideo("b", "Bravo"));

    Video replacement = makeVideo("a", "New");
    replacement.start = 99;
    replacement.marks = {mark(5, "x", false)};
    library.add(replacement);

    check(library.videos().size() == 2, "add() with an existing id does not add a duplicate, size " + std::to_string(library.videos().size()));
    int copies = 0;
    for (const Video& video : library.videos())
        if (video.id == "a")
            ++copies;
    check(copies == 1, "exactly one video with id a after replacing, got " + std::to_string(copies));
    Video* a = library.find("a");
    check(a && sameVideo(*a, replacement), "add() with an existing id replaces the stored video");
    Video* b = library.find("b");
    check(b && b->title == "Bravo", "replacing a does not change b");

    library.add(replacement);
    library.add(replacement);
    check(library.videos().size() == 2, "adding the same video repeatedly keeps one copy");
}

static void testRemove()
{
    Library library(root / "unused" / "library.json");
    library.add(makeVideo("a", "A"));
    library.add(makeVideo("b", "B"));
    library.add(makeVideo("c", "C"));
    library.add(makeVideo("d", "D"));

    library.remove("b");
    check(ids(library) == "a,c,d", "remove(\"b\") leaves a,c,d in order, got " + ids(library));
    check(library.find("b") == nullptr, "removed video can no longer be found");
    check(library.findByUrl("https://www.youtube.com/watch?v=b") == nullptr, "removed video can no longer be found by url");

    library.remove("unknown");
    check(ids(library) == "a,c,d", "remove() of an unknown id changes nothing, got " + ids(library));
    library.remove("b");
    check(ids(library) == "a,c,d", "removing the same id twice changes nothing, got " + ids(library));
    library.remove("");
    check(ids(library) == "a,c,d", "remove(\"\") changes nothing, got " + ids(library));

    library.remove("a");
    library.remove("d");
    check(ids(library) == "c", "removing first and last leaves c, got " + ids(library));
    library.remove("c");
    check(library.videos().empty(), "removing every video empties the library");
    library.remove("c");
    check(library.videos().empty(), "remove() on an empty library is harmless");

    library.add(makeVideo("b", "B again"));
    check(ids(library) == "b", "a removed id can be added again, got " + ids(library));
}

static void testAddMark()
{
    Video video = makeVideo("v", "V");
    Library::addMark(video, mark(30, "thirty", false));
    Library::addMark(video, mark(10, "ten", false));
    Library::addMark(video, mark(20, "twenty", false));
    Library::addMark(video, mark(0, "zero", false));
    std::vector<Bookmark> expected = {mark(0, "zero", false), mark(10, "ten", false), mark(20, "twenty", false), mark(30, "thirty", false)};
    check(sameMarks(video.marks, expected), "addMark() keeps marks ordered by time, got " + describe(video.marks));

    Library::addMark(video, mark(20, "twenty again", false));
    check(video.marks.size() == 5, "addMark() at an existing time adds another mark, got " + describe(video.marks));
    check(sortedByTime(video.marks), "marks still ordered after adding at an existing time, got " + describe(video.marks));
    check(countLabel(video.marks, "twenty", false) == 1 && countLabel(video.marks, "twenty again", false) == 1,
          "both marks at the same time are kept, got " + describe(video.marks));

    Library::addMark(video, mark(5000, "late", false));
    check(!video.marks.empty() && video.marks.back().label == "late", "a later mark goes last, got " + describe(video.marks));

    Video empty = makeVideo("e", "E");
    Library::addMark(empty, mark(42, "only", false));
    check(empty.marks.size() == 1 && empty.marks[0].time == 42 && empty.marks[0].label == "only" && !empty.marks[0].chapter,
          "addMark() on a video without marks stores the mark as given, got " + describe(empty.marks));

    Video many = makeVideo("m", "M");
    for (int i = 0; i < 200; ++i)
        Library::addMark(many, mark((i * 7919) % 200, "m" + std::to_string(i), false));
    check(many.marks.size() == 200 && sortedByTime(many.marks), "200 marks added in scrambled order are kept ordered by time");
}

static void testImportChapters()
{
    Video video = makeVideo("v", "V");
    Library::addMark(video, mark(5, "mine early", false));
    Library::addMark(video, mark(45, "mine late", false));

    Library::importChapters(video, {mark(60, "End", false), mark(0, "Intro", false), mark(30, "Middle", false)});
    std::vector<Bookmark> expected = {
        mark(0, "Intro", true), mark(5, "mine early", false), mark(30, "Middle", true),
        mark(45, "mine late", false), mark(60, "End", true)};
    check(sameMarks(video.marks, expected), "importChapters() adds chapters flagged as chapters, keeps user marks, orders by time, got " + describe(video.marks));

    Library::importChapters(video, {mark(10, "New", true)});
    expected = {mark(5, "mine early", false), mark(10, "New", true), mark(45, "mine late", false)};
    check(sameMarks(video.marks, expected), "importChapters() replaces the old chapters, got " + describe(video.marks));

    Library::importChapters(video, {mark(10, "New", true)});
    check(sameMarks(video.marks, expected), "importing the same chapters again changes nothing, got " + describe(video.marks));

    Library::importChapters(video, {});
    expected = {mark(5, "mine early", false), mark(45, "mine late", false)};
    check(sameMarks(video.marks, expected), "importChapters() with no chapters removes all chapters and keeps user marks, got " + describe(video.marks));

    Library::importChapters(video, {mark(5, "Same time", false)});
    check(video.marks.size() == 3, "a chapter at the same time as a user mark keeps both, got " + describe(video.marks));
    check(countLabel(video.marks, "mine early", false) == 1 && countLabel(video.marks, "Same time", true) == 1,
          "user mark and chapter at the same time both present with the right flags, got " + describe(video.marks));
    check(sortedByTime(video.marks), "marks ordered after importing a chapter at a user mark time, got " + describe(video.marks));

    Video bare = makeVideo("b", "B");
    Library::importChapters(bare, {mark(120, "Two", false), mark(0, "One", false)});
    expected = {mark(0, "One", true), mark(120, "Two", true)};
    check(sameMarks(bare.marks, expected), "importChapters() on a video without marks stores just the chapters, got " + describe(bare.marks));

    Video named = makeVideo("n", "N");
    Library::addMark(named, mark(30, "Intro", false));
    Library::importChapters(named, {mark(0, "Intro", false)});
    Library::importChapters(named, {});
    check(named.marks.size() == 1 && named.marks[0].label == "Intro" && !named.marks[0].chapter && named.marks[0].time == 30,
          "a user mark whose label matches a chapter survives chapter replacement, got " + describe(named.marks));

    Video after = makeVideo("a", "A");
    Library::importChapters(after, {mark(0, "Intro", false), mark(100, "Outro", false)});
    Library::addMark(after, mark(50, "mine", false));
    Library::importChapters(after, {mark(20, "Only", false)});
    expected = {mark(20, "Only", true), mark(50, "mine", false)};
    check(sameMarks(after.marks, expected), "user mark added after chapters survives a re-import, got " + describe(after.marks));
}

static std::vector<Video> sampleVideos()
{
    Video full;
    full.id = "abc123XYZ_-";
    full.url = "https://www.youtube.com/watch?v=abc123XYZ_-&t=10s";
    full.title = "Caf\xC3\xA9 \xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E \xF0\x9F\x98\x80 \"quoted\" back\\slash";
    full.file = "C:\\Users\\Test\\Videos\\Youtonomous\\caf\xC3\xA9 \xE6\x97\xA5\xE6\x9C\xAC.mp4";
    full.duration = 5025;
    full.start = 97;
    full.position = 1800;
    full.marks = {
        mark(0, "Intro", true),
        mark(97, "Real start \"quoted\" \\ back/slash", false),
        mark(1800, "Line1\nLine2\tTab", false),
        mark(3000, "\xE3\x83\x81\xE3\x83\xA3\xE3\x83\x97\xE3\x82\xBF\xE3\x83\xBC", true),
        mark(3000, "", false),
        mark(5025, "\xC3\x9C" "ber \xF0\x9F\x8E\xB5", false),
    };

    Video minimal;
    minimal.id = "x";

    Video large = makeVideo("large", "\xF0\x9F\x8E\xAC Large");
    large.duration = INT_MAX;
    large.start = INT_MAX - 1;
    large.position = INT_MAX;
    large.marks = {mark(INT_MAX, "end", true)};

    return {full, minimal, large};
}

static void testRoundTrip()
{
    fs::path file = root / "roundtrip" / "library.json";
    std::vector<Video> videos = sampleVideos();
    Library library(file);
    for (const Video& video : videos)
        library.add(video);
    check(trySave(library, "round trip"), "save() returns true");
    check(fs::exists(file), "save() writes the library file");

    check(nlohmann::json::accept(readFile(file)), "saved library file is valid JSON");

    Library loaded(file);
    check(tryLoad(loaded, "round trip"), "load() of a saved library returns true");
    check(loaded.videos().size() == videos.size(), "load() restores every video, got " + std::to_string(loaded.videos().size()));
    for (size_t i = 0; i < videos.size() && i < loaded.videos().size(); ++i) {
        std::string name = "video " + std::to_string(i) + " (" + videos[i].id + ") restored with every field";
        check(sameVideo(loaded.videos()[i], videos[i]), name + ", marks " + describe(loaded.videos()[i].marks));
    }

    Video* found = loaded.find("abc123XYZ_-");
    check(found && found->title == videos[0].title, "find() works after load()");
    Video* by_url = loaded.findByUrl(videos[0].url);
    check(by_url && by_url->id == videos[0].id, "findByUrl() works after load()");
    check(loaded.findByUrl("") == nullptr, "findByUrl(\"\") does not match the loaded video with an empty url");

    check(tryLoad(loaded, "second load"), "load() twice returns true");
    check(loaded.videos().size() == videos.size(), "load() twice does not duplicate videos, got " + std::to_string(loaded.videos().size()));

    check(trySave(loaded, "resave"), "save() of a loaded library returns true");
    Library again(file);
    check(tryLoad(again, "reload after resave"), "load() after saving a loaded library returns true");
    bool same = again.videos().size() == videos.size();
    for (size_t i = 0; same && i < videos.size(); ++i)
        same = sameVideo(again.videos()[i], videos[i]);
    check(same, "load, save, load gives the same videos");
}

static void testEditAfterLoad()
{
    fs::path file = root / "edit" / "library.json";
    Library library(file);
    library.add(makeVideo("a", "A"));
    library.add(makeVideo("b", "B"));
    trySave(library, "edit setup");

    Library loaded(file);
    tryLoad(loaded, "edit");
    Video* a = loaded.find("a");
    if (a) {
        a->start = 123;
        Library::addMark(*a, mark(200, "later", false));
        Library::addMark(*a, mark(100, "earlier", false));
    }
    loaded.remove("b");
    loaded.add(makeVideo("c", "C"));
    check(trySave(loaded, "edit save"), "save() after editing returns true");

    Library check2(file);
    check(tryLoad(check2, "edit reload"), "load() after editing returns true");
    check(ids(check2) == "a,c", "edited library reloads with a,c, got " + ids(check2));
    Video* reloaded = check2.find("a");
    std::vector<Bookmark> expected = {mark(100, "earlier", false), mark(200, "later", false)};
    check(reloaded && reloaded->start == 123 && sameMarks(reloaded->marks, expected),
          "changes made through find() are saved, marks " + (reloaded ? describe(reloaded->marks) : std::string("missing")));
}

static void testNestedFolders()
{
    fs::path file = root / "deep" / "er" / "still" / "library.json";
    Library library(file);
    library.add(makeVideo("a", "A"));
    check(trySave(library, "nested folders"), "save() creates missing parent folders and returns true");
    check(fs::exists(file), "save() into missing folders writes the file");
    Library loaded(file);
    check(tryLoad(loaded, "nested folders") && ids(loaded) == "a", "library saved into new folders loads back");
}

static void testUnicodePath()
{
    fs::path file = root / fs::u8path("caf\xC3\xA9 \xE6\x97\xA5\xE6\x9C\xAC \xF0\x9F\x98\x80") / "library.json";
    Library library(file);
    library.add(makeVideo("a", "A"));
    check(trySave(library, "unicode path"), "save() to a folder with a non-ASCII name returns true");
    check(fs::exists(file), "save() to a folder with a non-ASCII name writes the file");
    Library loaded(file);
    check(tryLoad(loaded, "unicode path") && ids(loaded) == "a", "library in a folder with a non-ASCII name loads back");
}

static void testOverwrite()
{
    fs::path file = root / "overwrite" / "library.json";
    Library big(file);
    for (const Video& video : sampleVideos())
        big.add(video);
    trySave(big, "overwrite big");

    Library small(file);
    small.add(makeVideo("only", "Only"));
    check(trySave(small, "overwrite small"), "save() over an existing file returns true");
    Library loaded(file);
    check(tryLoad(loaded, "overwrite"), "load() after overwrite returns true");
    check(ids(loaded) == "only", "save() replaces the previous file contents, got " + ids(loaded));

    Library empty(file);
    check(trySave(empty, "overwrite with empty"), "saving an empty library over an existing file returns true");
    Library reloaded(file);
    reloaded.add(makeVideo("stale", "Stale"));
    check(tryLoad(reloaded, "overwrite with empty"), "load() of an overwritten empty library returns true");
    check(reloaded.videos().empty(), "overwriting with an empty library leaves no videos, got " + ids(reloaded));
}

static void testEmptyLibrary()
{
    fs::path file = root / "empty" / "library.json";
    Library library(file);
    check(trySave(library, "empty"), "save() of an empty library returns true");
    check(fs::exists(file), "save() of an empty library writes a file");
    Library loaded(file);
    loaded.add(makeVideo("stale", "Stale"));
    check(tryLoad(loaded, "empty"), "load() of a saved empty library returns true");
    check(loaded.videos().empty(), "load() of a saved empty library gives no videos");
}

static void testLoadReplaces()
{
    fs::path file = root / "replace" / "library.json";
    Library saved(file);
    saved.add(makeVideo("c", "C"));
    trySave(saved, "replace setup");

    Library library(file);
    library.add(makeVideo("a", "A"));
    library.add(makeVideo("b", "B"));
    check(tryLoad(library, "replace"), "load() with videos in memory returns true");
    check(ids(library) == "c", "load() replaces in-memory videos with the file contents, got " + ids(library));
}

static void testDeletedFile()
{
    fs::path file = root / "deleted" / "library.json";
    Library library(file);
    library.add(makeVideo("a", "A"));
    trySave(library, "deleted setup");
    check(tryLoad(library, "before delete"), "load() before deleting the file returns true");
    std::error_code error;
    fs::remove(file, error);
    check(!tryLoad(library, "after delete"), "load() after the file is deleted returns false");
    check(library.videos().empty(), "library is empty after the file is deleted and reloaded");
}

static void loadRejects(const std::string& name, const std::string& content)
{
    fs::path file = root / "invalid" / "library.json";
    writeFile(file, content);
    Library library(file);
    library.add(makeVideo("stale", "Stale"));
    check(!tryLoad(library, name), "load() returns false for " + name);
    check(library.videos().empty(), "library is empty after loading " + name + ", got " + ids(library));
}

static void testInvalidFiles()
{
    loadRejects("an empty file", "");
    loadRejects("a whitespace-only file", "  \r\n\t ");
    loadRejects("corrupt JSON", "{not json");
    loadRejects("truncated JSON", "[{\"id\": \"a\", \"title\": ");
    loadRejects("a JSON number", "42");
    loadRejects("a JSON string", "\"library\"");
    loadRejects("JSON null", "null");
    loadRejects("JSON true", "true");
    loadRejects("binary garbage", std::string("\xFF\xFE\x00\x01garbage", 11));

    fs::path file = root / "invalid" / "library.json";
    writeFile(file, "[1, 2, 3]");
    Library numbers(file);
    tryLoad(numbers, "array of numbers");
    check(numbers.videos().empty(), "no videos are created from an array of numbers, got " + std::to_string(numbers.videos().size()));

    fs::path probe = root / "probe" / "library.json";
    Library probe_library(probe);
    probe_library.add(makeVideo("a", "A"));
    trySave(probe_library, "probe");
    bool saved_as_array = false;
    try {
        saved_as_array = nlohmann::json::parse(readFile(probe)).is_array();
    } catch (...) {
    }
    if (saved_as_array) {
        loadRejects("a JSON object", "{\"id\": \"a\", \"title\": \"A\"}");
        loadRejects("an empty JSON object", "{}");
        writeFile(file, "[]");
        Library empty_array(file);
        empty_array.add(makeVideo("stale", "Stale"));
        check(tryLoad(empty_array, "empty array"), "load() of an empty JSON array returns true");
        check(empty_array.videos().empty(), "load() of an empty JSON array gives no videos");
    }

    Library recovered(file);
    writeFile(file, "{not json");
    tryLoad(recovered, "corrupt before recovery");
    recovered.add(makeVideo("new", "New"));
    check(trySave(recovered, "recovery"), "save() over a corrupt file returns true");
    Library reloaded(file);
    check(tryLoad(reloaded, "after recovery") && ids(reloaded) == "new", "a corrupt file is replaced by a good save, got " + ids(reloaded));
}

static void testSaveFailure()
{
    fs::path blocker = root / "blocker";
    writeFile(blocker, "this is a file, not a folder");
    Library library(blocker / "library.json");
    library.add(makeVideo("a", "A"));
    check(!trySave(library, "folder blocked by a file"), "save() returns false when the folder cannot be created");
    check(readFile(blocker) == "this is a file, not a folder", "failed save() leaves the blocking file alone");
}

static void testManyVideos()
{
    fs::path file = root / "many" / "library.json";
    Library library(file);
    std::vector<Video> added;
    for (int i = 0; i < 300; ++i) {
        Video video = makeVideo("v" + std::to_string((i * 7919) % 300), "Video " + std::to_string(i));
        video.duration = i * 13;
        video.start = i;
        video.position = i * 11;
        for (int j = 0; j < 20; ++j)
            Library::addMark(video, mark((j * 37) % 20 * 10, "m" + std::to_string(j), j % 3 == 0));
        library.add(video);
        added.push_back(video);
    }
    check(library.videos().size() == 300, "300 distinct videos are all kept");
    check(trySave(library, "many"), "save() of 300 videos returns true");

    Library loaded(file);
    check(tryLoad(loaded, "many"), "load() of 300 videos returns true");
    bool same = loaded.videos().size() == added.size();
    for (size_t i = 0; same && i < added.size(); ++i)
        same = sameVideo(loaded.videos()[i], added[i]);
    check(same, "300 videos with 20 marks each round-trip in insertion order");
}

static std::string positions(const Library& library)
{
    std::string result;
    for (const Video& video : library.videos())
        result += (result.empty() ? "" : ",") + video.id + "=" + std::to_string(video.position);
    return result;
}

static std::string describeVideo(const Video& video)
{
    return "id " + video.id + ", url " + video.url + ", title " + video.title + ", file " + video.file
         + ", duration " + std::to_string(video.duration) + ", start " + std::to_string(video.start)
         + ", position " + std::to_string(video.position) + ", marks " + describe(video.marks);
}

static void testPositionDefault()
{
    Video video;
    check(video.position == 0, "a new Video has position 0, got " + std::to_string(video.position));
    Video made = makeVideo("d", "D");
    check(made.position == 0, "a video without a set position has position 0, got " + std::to_string(made.position));
}

static void testPositionRoundTrip()
{
    fs::path file = root / "position-roundtrip" / "library.json";
    std::vector<int> values = {0, 1, 59, 599, 600, INT_MAX - 1, INT_MAX};
    std::vector<Video> added;
    Library library(file);
    for (size_t i = 0; i < values.size(); ++i) {
        Video video = makeVideo("p" + std::to_string(i), "Position " + std::to_string(values[i]));
        video.position = values[i];
        library.add(video);
        added.push_back(video);
    }
    check(trySave(library, "position round trip"), "save() of videos with positions returns true");

    Library loaded(file);
    check(tryLoad(loaded, "position round trip"), "load() of videos with positions returns true");
    check(loaded.videos().size() == added.size(), "load() restores every video with a position, got " + ids(loaded));
    for (size_t i = 0; i < added.size() && i < loaded.videos().size(); ++i) {
        const Video& got = loaded.videos()[i];
        check(got.position == added[i].position, "position " + std::to_string(added[i].position) + " round-trips exactly, got " + std::to_string(got.position));
        check(sameVideo(got, added[i]), "video with position " + std::to_string(added[i].position) + " round-trips every field, got " + describeVideo(got));
    }

    check(trySave(loaded, "position resave"), "save() of a loaded library with positions returns true");
    Library again(file);
    tryLoad(again, "position reload");
    bool same = again.videos().size() == added.size();
    for (size_t i = 0; same && i < added.size(); ++i)
        same = sameVideo(again.videos()[i], added[i]);
    check(same, "load, save, load keeps every position, got " + positions(again));
}

static nlohmann::json savedJson(const fs::path& file, const std::vector<Video>& videos, const std::string& name)
{
    Library library(file);
    for (const Video& video : videos)
        library.add(video);
    check(trySave(library, name), "save() returns true for " + name);
    nlohmann::json result;
    try {
        result = nlohmann::json::parse(readFile(file));
    } catch (...) {
        result = nlohmann::json();
    }
    bool shaped = result.is_array() && result.size() == videos.size();
    for (size_t i = 0; shaped && i < result.size(); ++i)
        shaped = result[i].is_object() && result[i].contains("position");
    check(shaped, "saved file is an array of video objects with a \"position\" key for " + name + ", got " + result.dump());
    return shaped ? result : nlohmann::json();
}

static void loadExpects(const fs::path& file, const nlohmann::json& content, const std::vector<Video>& expected, const std::string& name)
{
    writeFile(file, content.dump());
    Library library(file);
    check(tryLoad(library, name), "load() returns true for " + name);
    check(library.videos().size() == expected.size(), "load() keeps every video for " + name + ", got " + ids(library));
    for (size_t i = 0; i < expected.size() && i < library.videos().size(); ++i) {
        const Video& got = library.videos()[i];
        check(sameVideo(got, expected[i]), name + ": video " + expected[i].id + " loads with position " + std::to_string(expected[i].position)
                                               + " and every other field, got " + describeVideo(got));
    }
}

static Video positionedVideo(const std::string& id, int position)
{
    Video video = makeVideo(id, "Title " + id);
    video.duration = 900;
    video.start = 12;
    video.position = position;
    video.marks = {mark(0, "Intro", true), mark(45, "mine", false)};
    return video;
}

static void testPositionMissingKey()
{
    fs::path file = root / "position-missing" / "library.json";
    Video first = positionedVideo("first", 250);
    Video second = positionedVideo("second", 77);
    nlohmann::json saved = savedJson(file, {first, second}, "missing position setup");
    if (saved.is_null())
        return;

    nlohmann::json old_style = saved;
    for (nlohmann::json& entry : old_style)
        entry.erase("position");
    Video old_first = first;
    old_first.position = 0;
    Video old_second = second;
    old_second.position = 0;
    loadExpects(file, old_style, {old_first, old_second}, "a file without \"position\" keys");

    nlohmann::json mixed = saved;
    mixed[0].erase("position");
    loadExpects(file, mixed, {old_first, second}, "a file where only one video lacks \"position\"");
}

static void testPositionWrongType()
{
    fs::path file = root / "position-type" / "library.json";
    Video first = positionedVideo("first", 250);
    Video second = positionedVideo("second", 77);
    nlohmann::json saved = savedJson(file, {first, second}, "wrong type position setup");
    if (saved.is_null())
        return;

    Video zeroed = first;
    zeroed.position = 0;
    std::vector<std::pair<std::string, nlohmann::json>> cases = {
        {"a string position \"12\"", nlohmann::json("12")},
        {"an empty string position", nlohmann::json("")},
        {"a null position", nlohmann::json(nullptr)},
        {"a true position", nlohmann::json(true)},
        {"a false position", nlohmann::json(false)},
        {"an array position [12]", nlohmann::json::array({12})},
        {"an empty array position", nlohmann::json::array()},
        {"an object position", nlohmann::json::object({{"seconds", 12}})},
        {"a position of -1", nlohmann::json(-1)},
        {"a position of -600", nlohmann::json(-600)},
        {"a position of INT_MIN", nlohmann::json(INT_MIN)},
    };
    for (const auto& [name, value] : cases) {
        nlohmann::json edited = saved;
        edited[0]["position"] = value;
        loadExpects(file, edited, {zeroed, second}, name);
    }

    nlohmann::json floating = saved;
    floating[0]["position"] = 12.5;
    writeFile(file, floating.dump());
    Library library(file);
    check(tryLoad(library, "a float position"), "load() returns true for a float position 12.5");
    check(library.videos().size() == 2, "load() keeps every video for a float position, got " + ids(library));
    if (library.videos().size() == 2) {
        Video got = library.videos()[0];
        check(got.position == 0 || got.position == 12, "float position 12.5 loads as 0 or 12, got " + std::to_string(got.position));
        got.position = 0;
        check(sameVideo(got, zeroed), "float position 12.5 leaves the other fields loaded, got " + describeVideo(library.videos()[0]));
        check(sameVideo(library.videos()[1], second), "float position in one video leaves the next video alone, got " + describeVideo(library.videos()[1]));
    }
}

static void testPositionReplace()
{
    Library library(root / "unused" / "library.json");
    library.add(positionedVideo("a", 300));
    library.add(positionedVideo("b", 40));

    Video cleared = positionedVideo("a", 0);
    cleared.title = "Cleared";
    library.add(cleared);
    Video* a = library.find("a");
    check(a && a->position == 0, "add() with an existing id replaces a position of 300 with 0, got " + (a ? std::to_string(a->position) : std::string("missing")));
    check(a && sameVideo(*a, cleared), "add() with an existing id replaces the whole video when position is 0");

    Video moved = positionedVideo("a", 45);
    library.add(moved);
    a = library.find("a");
    check(a && a->position == 45, "add() with an existing id replaces position 0 with 45, got " + (a ? std::to_string(a->position) : std::string("missing")));

    Video* b = library.find("b");
    check(b && b->position == 40, "replacing a does not change the position of b, got " + (b ? std::to_string(b->position) : std::string("missing")));
    check(library.videos().size() == 2, "replacing by id keeps two videos, got " + positions(library));
}

static void testPositionMarks()
{
    Video video = makeVideo("v", "V");
    video.position = 321;
    Library::addMark(video, mark(10, "ten", false));
    check(video.position == 321, "addMark() leaves position 321 alone, got " + std::to_string(video.position));
    Library::addMark(video, mark(500, "past position", false));
    check(video.position == 321, "addMark() after the position leaves position 321 alone, got " + std::to_string(video.position));
    Library::importChapters(video, {mark(0, "Intro", false), mark(400, "Later", false)});
    check(video.position == 321, "importChapters() leaves position 321 alone, got " + std::to_string(video.position));
    Library::importChapters(video, {});
    check(video.position == 321, "importChapters() with no chapters leaves position 321 alone, got " + std::to_string(video.position));

    Video unwatched = makeVideo("u", "U");
    Library::addMark(unwatched, mark(90, "mark", false));
    Library::importChapters(unwatched, {mark(60, "Chapter", false)});
    check(unwatched.position == 0, "addMark() and importChapters() leave position 0 alone, got " + std::to_string(unwatched.position));
}

static void testPositionThroughFind()
{
    fs::path file = root / "position-find" / "library.json";
    Library library(file);
    library.add(makeVideo("a", "A"));
    library.add(positionedVideo("b", 700));
    trySave(library, "position find setup");

    Library loaded(file);
    tryLoad(loaded, "position find");
    Video* a = loaded.find("a");
    if (a)
        a->position = 480;
    Video* b = loaded.find("b");
    if (b)
        b->position = 0;
    check(trySave(loaded, "position find save"), "save() after setting positions through find() returns true");

    Library reloaded(file);
    check(tryLoad(reloaded, "position find reload"), "load() after setting positions through find() returns true");
    Video* reloaded_a = reloaded.find("a");
    check(reloaded_a && reloaded_a->position == 480, "position 480 set through find() is saved, got " + positions(reloaded));
    Video* reloaded_b = reloaded.find("b");
    check(reloaded_b && reloaded_b->position == 0, "position reset to 0 through find() is saved, got " + positions(reloaded));
    check(reloaded_b && reloaded_b->start == 12 && reloaded_b->duration == 900 && reloaded_b->marks.size() == 2,
          "setting position through find() leaves the other saved fields alone, got " + (reloaded_b ? describeVideo(*reloaded_b) : std::string("missing")));
}

int main()
{
    root = fs::temp_directory_path() / ("youtonomous-library-test-" + std::to_string(GetCurrentProcessId()));
    std::error_code error;
    fs::remove_all(root, error);
    fs::create_directories(root);

    testMissingFile();
    testAddAndFind();
    testReplace();
    testRemove();
    testAddMark();
    testImportChapters();
    testRoundTrip();
    testEditAfterLoad();
    testNestedFolders();
    testUnicodePath();
    testOverwrite();
    testEmptyLibrary();
    testLoadReplaces();
    testDeletedFile();
    testInvalidFiles();
    testSaveFailure();
    testManyVideos();
    testPositionDefault();
    testPositionRoundTrip();
    testPositionMissingKey();
    testPositionWrongType();
    testPositionReplace();
    testPositionMarks();
    testPositionThroughFind();

    fs::remove_all(root, error);
    return report();
}
