#pragma once

#include <initializer_list>
#include <string>

#include "player.h"

struct mpv_handle;

class MpvPlayer : public Player {
public:
    MpvPlayer();
    ~MpvPlayer() override;

    MpvPlayer(const MpvPlayer&) = delete;
    MpvPlayer& operator=(const MpvPlayer&) = delete;

    bool ready() const override { return mpv_ != nullptr; }
    bool loaded() const override { return loaded_; }

    void attach(HWND window) override;
    void open(const std::string& path, int start) override;
    void togglePause() override;

    bool playing() const override;
    int time() const override;
    int length() const override;

    void seek(int seconds) override;

private:
    void initialize();
    void drainEvents() const;
    bool flag(const char* name) const;
    double number(const char* name) const;
    void command(std::initializer_list<const char*> args) const;

    mpv_handle* mpv_ = nullptr;
    bool initialized_ = false;
    bool loaded_ = false;
};
