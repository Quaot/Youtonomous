#pragma once

#include "player.h"

struct libvlc_instance_t;
struct libvlc_media_player_t;

class VlcPlayer : public Player {
public:
    VlcPlayer();
    ~VlcPlayer() override;

    VlcPlayer(const VlcPlayer&) = delete;
    VlcPlayer& operator=(const VlcPlayer&) = delete;

    bool ready() const override { return player_ != nullptr; }
    bool loaded() const override { return loaded_; }

    void attach(HWND window) override;
    void open(const std::string& path, int start) override;
    void togglePause() override;

    bool playing() const override;
    int time() const override;
    int length() const override;

    void seek(int seconds) override;

private:
    libvlc_instance_t* vlc_ = nullptr;
    libvlc_media_player_t* player_ = nullptr;
    bool loaded_ = false;
};
