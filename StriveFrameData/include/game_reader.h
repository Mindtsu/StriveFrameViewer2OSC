#pragma once

#include "game_reporter.h"
#include "osc_config.h"

struct BattleEvents {
  bool battle_start = false;
  bool round_end = false;
  bool rematch = false;
};

// Reads the battle event queue. Call it BEFORE the original UpdateBattle,
// like the original mod did, so the events of the previous update are seen.
BattleEvents read_battle_events();

// True while the game is re-simulating frames for rollback (online only).
bool is_resimulating();

// Copies what the reporter needs from the game's memory. Returns false when
// no battle is loaded.
bool read_frame(const OscConfig &config, const BattleEvents &events, FrameSnapshot &out);
