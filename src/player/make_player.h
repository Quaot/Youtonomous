#pragma once

#include <memory>
#include <string>

#include "player.h"

std::unique_ptr<Player> makePlayer(const std::string& backend);
