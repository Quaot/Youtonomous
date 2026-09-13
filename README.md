# Youtonomous

Long YouTube videos often open with minutes of chatter. Youtonomous downloads a video once, remembers where the real content starts and opens it there every time. Bookmarks let you jump around.

It is a small Windows app written in C++. yt-dlp downloads the videos and VLC plays them.

## Features

- Download a video from a YouTube link.
- Give each video a start point. Playback skips straight to it.
- YouTube chapters become bookmarks.
- Bookmark any moment with a label.
- Jump between bookmarks with a click or a key.

## Setup

Install [MSYS2](https://www.msys2.org). In the UCRT64 shell, run:

```
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja \
          mingw-w64-ucrt-x86_64-vlc mingw-w64-ucrt-x86_64-nlohmann-json
```

Then install yt-dlp:

```
winget install yt-dlp.yt-dlp
```

## Build

```
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
```

Run `build/youtonomous.exe` from the same shell so it finds the VLC libraries. Pass a file path to open a local video.

## Use

1. Paste a link and press Download.
2. Double-click a video in the library to play it.
3. Type a time under "Start at" and press Set. The video opens there from now on.
4. Type a label and press Add to bookmark the current moment.

Times can be written as `1:02:03`, `4:30`, `270` or `4m30s`.

On the seek bar, blue ticks are chapters, orange ticks are your bookmarks and the green tick is the start point.

## Keys

| Key | Action |
|---|---|
| Space | Play or pause |
| Left / Right | Back or forward 10 seconds |
| Shift + Left / Right | Back or forward 60 seconds |
| `[` / `]` | Previous or next bookmark |
| B | Bookmark this moment |
| S | Set the start point to this moment |
| Home | Go to the start point |

Keys do nothing while you type in a text box. Enter in a box does what its button does.

## Files

The library lives in `%APPDATA%\Youtonomous\library.json`. Videos go to `%USERPROFILE%\Videos\Youtonomous`.

## License

MIT
