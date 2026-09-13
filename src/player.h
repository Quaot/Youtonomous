#pragma once

#include <string>

#include <windows.h>

struct libvlc_instance_t;
struct libvlc_media_player_t;

class Player {
public:
    Player();
    ~Player();

    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;

    bool ready() const { return player_ != nullptr; }
    bool loaded() const { return loaded_; }

    void attach(HWND window);
    void open(const std::string& path, int start);
    void togglePause();

    bool playing() const;
    int time() const;
    int length() const;

    void seek(int seconds);
    void skip(int seconds);

private:
    libvlc_instance_t* vlc_ = nullptr;
    libvlc_media_player_t* player_ = nullptr;
    bool loaded_ = false;
};
