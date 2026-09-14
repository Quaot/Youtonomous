#include "mpv_player.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <mpv/client.h>

MpvPlayer::MpvPlayer()
    : mpv_(mpv_create())
{
    if (!mpv_)
        return;

    mpv_set_option_string(mpv_, "config", "no");
    mpv_set_option_string(mpv_, "terminal", "no");
    mpv_set_option_string(mpv_, "idle", "yes");
    mpv_set_option_string(mpv_, "keep-open", "yes");
    mpv_set_option_string(mpv_, "hr-seek", "yes");
    mpv_set_option_string(mpv_, "osc", "no");
    mpv_set_option_string(mpv_, "osd-level", "0");
    mpv_set_option_string(mpv_, "input-default-bindings", "no");
    mpv_set_option_string(mpv_, "input-vo-keyboard", "no");
    mpv_set_option_string(mpv_, "input-cursor", "no");
}

MpvPlayer::~MpvPlayer()
{
    if (mpv_)
        mpv_terminate_destroy(mpv_);
}

void MpvPlayer::initialize()
{
    if (!mpv_ || initialized_)
        return;
    initialized_ = mpv_initialize(mpv_) >= 0;
}

void MpvPlayer::attach(HWND window)
{
    if (!mpv_ || initialized_)
        return;

    int64_t wid = static_cast<int64_t>(reinterpret_cast<intptr_t>(window));
    mpv_set_option(mpv_, "wid", MPV_FORMAT_INT64, &wid);
    initialize();
}

void MpvPlayer::open(const std::string& path, int start)
{
    initialize();
    if (!initialized_)
        return;

    std::string start_text = std::to_string(std::max(start, 0));
    mpv_set_property_string(mpv_, "start", start_text.c_str());
    mpv_set_property_string(mpv_, "pause", "no");
    applyVolumeAndSpeed();
    command({"loadfile", path.c_str(), "replace"});
    loaded_ = true;
}

void MpvPlayer::togglePause()
{
    if (!loaded_)
        return;

    if (flag("eof-reached")) {
        command({"seek", "0", "absolute+exact"});
        mpv_set_property_string(mpv_, "pause", "no");
        return;
    }
    command({"cycle", "pause"});
}

bool MpvPlayer::playing() const
{
    if (!loaded_)
        return false;
    return !flag("pause") && !flag("idle-active") && !flag("eof-reached");
}

int MpvPlayer::time() const
{
    if (!loaded_)
        return 0;
    return static_cast<int>(std::max(0.0, number("time-pos")));
}

int MpvPlayer::length() const
{
    if (!loaded_)
        return 0;
    return static_cast<int>(std::max(0.0, number("duration")));
}

void MpvPlayer::seek(int seconds)
{
    if (!loaded_)
        return;

    int end = length();
    if (end > 0)
        seconds = std::min(seconds, end - 1);
    seconds = std::max(seconds, 0);

    std::string target = std::to_string(seconds);
    command({"seek", target.c_str(), "absolute+exact"});
}

void MpvPlayer::setVolume(int percent)
{
    volume_ = std::clamp(percent, 0, 100);
    applyVolumeAndSpeed();
}

void MpvPlayer::setSpeed(double rate)
{
    if (!std::isfinite(rate))
        return;
    speed_ = std::clamp(rate, 0.5, 2.0);
    applyVolumeAndSpeed();
}

void MpvPlayer::applyVolumeAndSpeed()
{
    if (!mpv_)
        return;
    double volume = volume_;
    double speed = speed_;
    mpv_set_property(mpv_, "volume", MPV_FORMAT_DOUBLE, &volume);
    mpv_set_property(mpv_, "speed", MPV_FORMAT_DOUBLE, &speed);
}

void MpvPlayer::drainEvents() const
{
    while (mpv_wait_event(mpv_, 0)->event_id != MPV_EVENT_NONE) {
    }
}

bool MpvPlayer::flag(const char* name) const
{
    drainEvents();
    int value = 0;
    if (mpv_get_property(mpv_, name, MPV_FORMAT_FLAG, &value) < 0)
        return false;
    return value != 0;
}

double MpvPlayer::number(const char* name) const
{
    drainEvents();
    double value = 0;
    if (mpv_get_property(mpv_, name, MPV_FORMAT_DOUBLE, &value) < 0 || !std::isfinite(value))
        return 0;
    return value;
}

void MpvPlayer::command(std::initializer_list<const char*> args) const
{
    std::vector<const char*> list(args);
    list.push_back(nullptr);
    mpv_command(mpv_, list.data());
}
