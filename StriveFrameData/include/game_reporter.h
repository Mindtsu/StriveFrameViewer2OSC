#pragma once

// Turns per-frame snapshots of the battle into OSC messages.
// Pure logic: no game memory access here (see game_reader.h), so the
// behaviour can be tested outside the game.

#include "osc_config.h"
#include "osc_sender.h"

#include <array>
#include <cstddef>
#include <functional>
#include <string>

// True when the game's registered move name (m_SkillName, a char[capacity]
// that cuts longer names) designates the given script state.
bool skill_name_matches_state(const char *skill_name, size_t capacity, const std::string &state);

struct PlayerSnapshot {
  bool valid = false;
  std::string state;       // current BBScript state, e.g. "NmlAtk5A", "CmnActFDash"
  int action_time = 0;     // frames spent in the current state
  int skill_id = -1;       // index of the registered move the current state is, -1 if none
  std::string skill_name;  // that move's name (the state name), empty if none
  int hp = 0;
  int tension_raw = 0;     // 0..10000
  int burst_raw = 0;       // 0..15000
  bool has_risc = false;
  int risc = 0;
  int cmn_action = -1;     // ID_CMNACT
};

struct FrameSnapshot {
  std::array<PlayerSnapshot, 2> players;
  int round_count = 0;
  bool battle_start = false; // BOM_EVENT_BATTLE_START seen this frame
  bool round_end = false;    // BOM_EVENT_DECISION or BOM_EVENT_RESET
  bool rematch = false;      // BOM_EVENT_RESET_REMATCH
};

class GameReporter {
public:
  using SendFn = std::function<void(const OscMessage &)>;
  using LogFn = std::function<void(const std::string &)>;

  static constexpr int CMN_ROUND_WIN = 0x6A;  // ID_CmnActRoundWin
  static constexpr int CMN_MATCH_WIN = 0x6B;  // ID_CmnActMatchWin

  GameReporter(const OscConfig &config, SendFn send, LogFn log);

  void on_match_start();                      // from the MatchStart hook
  void on_frame(const FrameSnapshot &frame);  // once per battle frame

private:
  struct Track {
    bool valid = false;
    std::string state;
    int action_time = 0;
    int skill_id = -1;
    int hp = 0;
    int hp_max = 0;
    int tension_raw = -1;
    int burst_raw = -1;
    int risc = 0;
    int cmn_action = -1;
    int wins = 0;
    std::string last_move;
    long long last_move_frame = -1000;
  };

  void process_player(int index, const PlayerSnapshot &snap);
  void send_rounds(int index);
  std::string path(int player, const char *leaf) const;
  std::string path(const char *leaf) const;

  const OscConfig &config_;
  SendFn send_;
  LogFn log_;
  std::array<Track, 2> tracks_{};
  long long frame_ = 0;
  bool round_active_ = false;
  bool match_start_pending_ = true;
  bool force_all_ = true;
};
