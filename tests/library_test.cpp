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

static Bookmark notedMark(int time, const std::string& label, bool chapter, const std::string& note)
{
    Bookmark result = mark(time, label, chapter);
    result.note = note;
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
        if (a[i].time != b[i].time || a[i].label != b[i].label || a[i].chapter != b[i].chapter || a[i].note != b[i].note)
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
        result += std::to_string(m.time) + (m.chapter ? "[chapter]" : "[user]") + m.label + (m.note.empty() ? std::string() : "{" + m.note + "}") + " ";
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

static Video notedVideo(const std::string& id)
{
    Video video = makeVideo(id, "Notes " + id);
    video.marks = {
        notedMark(0, "Intro", true, "Chapter note"),
        notedMark(15, "empty", false, ""),
        notedMark(30, "lines", false, "First line\nSecond line\r\nThird line"),
        notedMark(45, "tabs", false, "col1\tcol2\t\t"),
        notedMark(60, "quotes", false, "She said \"hi\" and 'bye'"),
        notedMark(75, "slashes", false, "C:\\path\\to\\file and \\n literal"),
        notedMark(90, "utf8", true, "Caf\xC3\xA9 \xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E \xF0\x9F\x98\x80"),
        notedMark(105, "mixed", false, "\"\\\n\r\n\t\xC3\xBC \xF0\x9F\x8E\xB5"),
    };
    return video;
}

static int eraseNotes(nlohmann::json& video)
{
    int erased = 0;
    for (nlohmann::json& value : video)
        if (value.is_array())
            for (nlohmann::json& entry : value)
                if (entry.is_object() && entry.erase("note") > 0)
                    ++erased;
    return erased;
}

static nlohmann::json* findNote(nlohmann::json& videos, const std::string& note)
{
    for (nlohmann::json& video : videos)
        if (video.is_object())
            for (nlohmann::json& value : video)
                if (value.is_array())
                    for (nlohmann::json& entry : value)
                        if (entry.is_object() && entry.contains("note") && entry["note"] == note)
                            return &entry;
    return nullptr;
}

static void checkNotes(const Video& got, const Video& expected, const std::string& name)
{
    check(got.marks.size() == expected.marks.size(), name + ": video " + expected.id + " keeps " + std::to_string(expected.marks.size())
                                                         + " marks, got " + describe(got.marks));
    for (size_t i = 0; i < got.marks.size() && i < expected.marks.size(); ++i)
        check(got.marks[i].note == expected.marks[i].note, name + ": note at time " + std::to_string(expected.marks[i].time) + " of video " + expected.id
                                                               + " is [" + expected.marks[i].note + "], got [" + got.marks[i].note + "]");
    check(sameVideo(got, expected), name + ": video " + expected.id + " keeps every field, got " + describeVideo(got));
}

static void testNoteDefault()
{
    Bookmark bookmark;
    check(bookmark.note.empty(), "a new Bookmark has an empty note, got [" + bookmark.note + "]");
    Bookmark made = mark(10, "label", true);
    check(made.note.empty(), "a bookmark without a set note has an empty note, got [" + made.note + "]");
}

static void testNoteRoundTrip()
{
    fs::path file = root / "note-roundtrip" / "library.json";
    std::vector<Video> added = {notedVideo("n1"), notedVideo("n2"), makeVideo("plain", "Plain")};
    added[1].marks[0].note = "Only in n2";
    added[2].marks = {mark(5, "no note", false), mark(9, "no note chapter", true)};
    Library library(file);
    for (const Video& video : added)
        library.add(video);
    check(trySave(library, "note round trip"), "save() of bookmarks with notes returns true");
    check(nlohmann::json::accept(readFile(file)), "saved library file with notes is valid JSON");

    Library loaded(file);
    check(tryLoad(loaded, "note round trip"), "load() of bookmarks with notes returns true");
    check(loaded.videos().size() == added.size(), "load() restores every video with notes, got " + ids(loaded));
    for (size_t i = 0; i < added.size() && i < loaded.videos().size(); ++i)
        checkNotes(loaded.videos()[i], added[i], "note round trip");

    check(trySave(loaded, "note resave"), "save() of a loaded library with notes returns true");
    Library again(file);
    check(tryLoad(again, "note reload"), "load() after saving a loaded library with notes returns true");
    check(again.videos().size() == added.size(), "load, save, load keeps every video with notes, got " + ids(again));
    for (size_t i = 0; i < added.size() && i < again.videos().size(); ++i)
        checkNotes(again.videos()[i], added[i], "note load, save, load");
}

static void testNoteLong()
{
    fs::path file = root / "note-long" / "library.json";
    std::string plain(10000, 'x');
    std::string mixed;
    for (int i = 0; i < 2000; ++i)
        mixed += "Line " + std::to_string(i) + "\t\"q\" \\ caf\xC3\xA9 \xF0\x9F\x98\x80\r\n";
    std::string single(20000, 'y');
    single[0] = '"';
    single[9999] = '\\';

    Video video = makeVideo("long", "Long");
    video.marks = {notedMark(1, "plain", false, plain), notedMark(2, "mixed", false, mixed), notedMark(3, "single", true, single)};
    Library library(file);
    library.add(video);
    check(trySave(library, "long notes"), "save() of bookmarks with long notes returns true");

    Library loaded(file);
    check(tryLoad(loaded, "long notes"), "load() of bookmarks with long notes returns true");
    Video* got = loaded.find("long");
    check(got && got->marks.size() == 3, "video with long notes loads with 3 marks, got " + (got ? std::to_string(got->marks.size()) : std::string("missing")));
    if (!got || got->marks.size() != 3)
        return;
    for (size_t i = 0; i < 3; ++i) {
        const std::string& expected = video.marks[i].note;
        const std::string& actual = got->marks[i].note;
        check(actual == expected, "note \"" + video.marks[i].label + "\" of " + std::to_string(expected.size()) + " bytes round-trips exactly, got "
                                      + std::to_string(actual.size()) + " bytes" + (actual.size() == expected.size() ? " with different content" : ""));
    }
    check(sameVideo(*got, video), "video with long notes keeps every field");
}

static void testNoteMissingKey()
{
    fs::path file = root / "note-missing" / "library.json";
    Video first = notedVideo("first");
    Video second = notedVideo("second");
    nlohmann::json saved = savedJson(file, {first, second}, "missing note setup");
    if (saved.is_null())
        return;

    Video old_first = first;
    for (Bookmark& m : old_first.marks)
        m.note = "";
    Video old_second = second;
    for (Bookmark& m : old_second.marks)
        m.note = "";

    nlohmann::json old_style = saved;
    int erased = 0;
    for (nlohmann::json& entry : old_style)
        erased += eraseNotes(entry);
    check(erased > 0, "saved file has \"note\" keys on its marks, found " + std::to_string(erased) + " in " + saved.dump());
    loadExpects(file, old_style, {old_first, old_second}, "a file without \"note\" keys");

    nlohmann::json one_video = saved;
    eraseNotes(one_video[0]);
    loadExpects(file, one_video, {old_first, second}, "a file where only one video's marks lack \"note\"");

    nlohmann::json one_mark = saved;
    nlohmann::json* entry = findNote(one_mark, first.marks[2].note);
    check(entry != nullptr, "saved file contains the note [" + first.marks[2].note + "], got " + saved.dump());
    if (!entry)
        return;
    entry->erase("note");
    Video partial = first;
    partial.marks[2].note = "";
    loadExpects(file, one_mark, {partial, second}, "a file where only one mark lacks \"note\"");
}

static void testNoteWrongType()
{
    fs::path file = root / "note-type" / "library.json";
    Video first = notedVideo("first");
    first.marks[3].note = "Target note";
    Video second = notedVideo("second");
    nlohmann::json saved = savedJson(file, {first, second}, "wrong type note setup");
    if (saved.is_null())
        return;
    nlohmann::json probe = saved;
    bool has_target = findNote(probe, "Target note") != nullptr;
    check(has_target, "saved file contains the note \"Target note\", got " + saved.dump());
    if (!has_target)
        return;

    Video cleared = first;
    cleared.marks[3].note = "";
    std::vector<std::pair<std::string, nlohmann::json>> cases = {
        {"a number note 42", nlohmann::json(42)},
        {"a number note 0", nlohmann::json(0)},
        {"a number note -7", nlohmann::json(-7)},
        {"a number note 1.5", nlohmann::json(1.5)},
        {"a null note", nlohmann::json(nullptr)},
        {"a true note", nlohmann::json(true)},
        {"a false note", nlohmann::json(false)},
        {"an array note [\"Target note\"]", nlohmann::json::array({"Target note"})},
        {"an empty array note", nlohmann::json::array()},
        {"an object note", nlohmann::json::object({{"text", "Target note"}})},
        {"an empty object note", nlohmann::json::object()},
    };
    for (const auto& [name, value] : cases) {
        nlohmann::json edited = saved;
        nlohmann::json* entry = findNote(edited, "Target note");
        if (!entry)
            continue;
        (*entry)["note"] = value;
        loadExpects(file, edited, {cleared, second}, name);
    }
}

static void testNoteAddMark()
{
    Video video = makeVideo("v", "V");
    Library::addMark(video, notedMark(30, "thirty", false, "note 30"));
    Library::addMark(video, notedMark(10, "ten", false, "note 10\nsecond line"));
    Library::addMark(video, notedMark(20, "twenty", false, ""));
    Library::addMark(video, notedMark(0, "zero", false, "note \xF0\x9F\x98\x80 0"));
    std::vector<Bookmark> expected = {
        notedMark(0, "zero", false, "note \xF0\x9F\x98\x80 0"), notedMark(10, "ten", false, "note 10\nsecond line"),
        notedMark(20, "twenty", false, ""), notedMark(30, "thirty", false, "note 30")};
    check(sameMarks(video.marks, expected), "addMark() keeps each note with its own bookmark after sorting, got " + describe(video.marks));

    Video single = makeVideo("s", "S");
    Library::addMark(single, notedMark(42, "only", false, "kept \"note\"\r\n"));
    check(single.marks.size() == 1 && single.marks[0].note == "kept \"note\"\r\n",
          "addMark() keeps the note of the mark as given, got " + describe(single.marks));

    Video many = makeVideo("m", "M");
    for (int i = 0; i < 100; ++i) {
        int time = (i * 7919) % 100;
        Library::addMark(many, notedMark(time, "m" + std::to_string(time), false, "note " + std::to_string(time)));
    }
    std::string mismatch;
    for (const Bookmark& m : many.marks)
        if (mismatch.empty() && (m.note != "note " + std::to_string(m.time) || m.label != "m" + std::to_string(m.time)))
            mismatch = std::to_string(m.time) + " has label " + m.label + " and note [" + m.note + "]";
    check(many.marks.size() == 100 && sortedByTime(many.marks) && mismatch.empty(),
          "100 marks with notes added in scrambled order keep their notes after sorting, size " + std::to_string(many.marks.size())
              + (mismatch.empty() ? std::string() : ", mark at " + mismatch));
}

static void testNoteImportChapters()
{
    Video video = makeVideo("v", "V");
    Library::addMark(video, notedMark(5, "mine early", false, "early note"));
    Library::addMark(video, notedMark(45, "mine late", false, "late\r\nnote"));
    Library::addMark(video, notedMark(70, "mine plain", false, ""));

    Library::importChapters(video, {notedMark(60, "End", false, "end note"), notedMark(0, "Intro", false, "intro \xC3\xA9"), notedMark(30, "Middle", false, "")});
    std::vector<Bookmark> expected = {
        notedMark(0, "Intro", true, "intro \xC3\xA9"), notedMark(5, "mine early", false, "early note"), notedMark(30, "Middle", true, ""),
        notedMark(45, "mine late", false, "late\r\nnote"), notedMark(60, "End", true, "end note"), notedMark(70, "mine plain", false, "")};
    check(sameMarks(video.marks, expected), "importChapters() keeps user notes and the notes given on chapters, got " + describe(video.marks));

    Library::importChapters(video, {notedMark(30, "Middle", true, "new middle note"), notedMark(10, "New", true, "")});
    expected = {
        notedMark(5, "mine early", false, "early note"), notedMark(10, "New", true, ""), notedMark(30, "Middle", true, "new middle note"),
        notedMark(45, "mine late", false, "late\r\nnote"), notedMark(70, "mine plain", false, "")};
    check(sameMarks(video.marks, expected), "importChapters() replaces chapters and their notes with the new chapters, got " + describe(video.marks));

    Library::importChapters(video, {mark(30, "Middle", true)});
    expected = {
        notedMark(5, "mine early", false, "early note"), mark(30, "Middle", true),
        notedMark(45, "mine late", false, "late\r\nnote"), notedMark(70, "mine plain", false, "")};
    check(sameMarks(video.marks, expected), "a re-imported chapter without a note does not keep the old chapter note, got " + describe(video.marks));

    Library::importChapters(video, {});
    expected = {notedMark(5, "mine early", false, "early note"), notedMark(45, "mine late", false, "late\r\nnote"), notedMark(70, "mine plain", false, "")};
    check(sameMarks(video.marks, expected), "importChapters() with no chapters keeps user marks with their notes, got " + describe(video.marks));
}

static void testNoteThroughFind()
{
    fs::path file = root / "note-find" / "library.json";
    Video a = makeVideo("a", "A");
    a.marks = {notedMark(0, "Intro", true, "intro"), notedMark(40, "old label", false, "old note"), notedMark(80, "keep", false, "keep note")};
    Video b = notedVideo("b");
    Library library(file);
    library.add(a);
    library.add(b);
    trySave(library, "note find setup");

    Library loaded(file);
    tryLoad(loaded, "note find");
    Video* found = loaded.find("a");
    check(found != nullptr, "find(\"a\") returns the video after load()");
    if (found) {
        for (Bookmark& m : found->marks) {
            if (m.time == 0)
                m.note = "changed chapter note";
            if (m.time == 40) {
                m.label = "new label";
                m.note = "new note\nline two";
            }
            if (m.time == 80)
                m.note = "";
        }
    }
    check(trySave(loaded, "note find save"), "save() after changing notes through find() returns true");

    Video expected = a;
    expected.marks[0].note = "changed chapter note";
    expected.marks[1].label = "new label";
    expected.marks[1].note = "new note\nline two";
    expected.marks[2].note = "";
    Library reloaded(file);
    check(tryLoad(reloaded, "note find reload"), "load() after changing notes through find() returns true");
    Video* got = reloaded.find("a");
    check(got && sameVideo(*got, expected), "label and note changes made through find() are saved, got " + (got ? describeVideo(*got) : std::string("missing")));
    Video* got_b = reloaded.find("b");
    check(got_b && sameVideo(*got_b, b), "changing notes of a leaves the notes of b alone, got " + (got_b ? describeVideo(*got_b) : std::string("missing")));
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
    testNoteDefault();
    testNoteRoundTrip();
    testNoteLong();
    testNoteMissingKey();
    testNoteWrongType();
    testNoteAddMark();
    testNoteImportChapters();
    testNoteThroughFind();

    fs::remove_all(root, error);
    return report();
}
