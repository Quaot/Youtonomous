#include "make_player.h"

#include "vlc_player.h"

std::unique_ptr<Player> makePlayer(const std::string& backend)
{
    if (backend == "vlc")
        return std::make_unique<VlcPlayer>();
    return nullptr;
}
