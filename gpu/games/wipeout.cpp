// SPDX-License-Identifier: GPL-2.0-or-later
// WipEout Omega Collection (update 1.07): what the translator knows about its code. Nothing yet:
// the game runs on the generic paths, and this profile only names it, so that what is learned
// about its code (hooks, per-frame writers, known shaders) has its place. Addresses will be those
// of the 1.07 executable (scripts/games.py makes sure that is the one running).
#include "game_profile.h"

namespace Game {
namespace {
/// The releases whose 1.07 executable is known (scripts/games.py WIPEOUT): the European one.
constexpr const char* Serials[] = {"CUSA05670"};
} // namespace

extern const Profile WipeoutProfile = {
    .name = "WipEout Omega Collection",
    .serials = Serials,
};

} // namespace Game
