#include <climits>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
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
        && a.duration == b.duration && a.start == b.start && sameMarks(a.marks, b.marks);
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

    fs::remove_all(root, error);
    return report();
}
