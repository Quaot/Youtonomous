#include "player.h"

#include <algorithm>

#include <vlc/vlc.h>

Player::Player()
{
    const char* args[] = {"--no-video-title-show"};
    vlc_ = libvlc_new(1, args);
    if (vlc_)
        player_ = libvlc_media_player_new(vlc_);
}

Player::~Player()
{
    if (player_) {
        libvlc_media_player_stop(player_);
        libvlc_media_player_release(player_);
    }
    if (vlc_)
        libvlc_release(vlc_);
}

void Player::attach(HWND window)
{
    if (!ready())
        return;
    libvlc_media_player_set_hwnd(player_, window);
    libvlc_video_set_mouse_input(player_, false);
    libvlc_video_set_key_input(player_, false);
}

void Player::open(const std::string& path, int start)
{
    if (!ready())
        return;

    libvlc_media_t* media = libvlc_media_new_path(vlc_, path.c_str());
    if (!media)
        return;

    std::string option = ":start-time=" + std::to_string(start);
    libvlc_media_add_option(media, option.c_str());
    libvlc_media_player_set_media(player_, media);
    libvlc_media_release(media);

    libvlc_media_player_play(player_);
    loaded_ = true;
}

void Player::togglePause()
{
    if (!loaded_)
        return;

    if (libvlc_media_player_get_state(player_) == libvlc_Ended) {
        libvlc_media_player_stop(player_);
        libvlc_media_player_play(player_);
        return;
    }
    libvlc_media_player_pause(player_);
}

bool Player::playing() const
{
    return loaded_ && libvlc_media_player_is_playing(player_);
}

int Player::time() const
{
    if (!loaded_)
        return 0;
    return static_cast<int>(std::max<libvlc_time_t>(0, libvlc_media_player_get_time(player_)) / 1000);
}

int Player::length() const
{
    if (!loaded_)
        return 0;
    return static_cast<int>(std::max<libvlc_time_t>(0, libvlc_media_player_get_length(player_)) / 1000);
}

void Player::seek(int seconds)
{
    if (!loaded_)
        return;

    int end = length();
    if (end > 0)
        seconds = std::min(seconds, end - 1);
    seconds = std::max(seconds, 0);
    libvlc_media_player_set_time(player_, static_cast<libvlc_time_t>(seconds) * 1000);
}

void Player::skip(int seconds)
{
    seek(time() + seconds);
}
