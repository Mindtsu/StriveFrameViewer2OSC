#pragma once

// Settings read from osc_config.ini (next to the mod's "dlls" folder).
// No Windows / UE4SS dependency, so it can be tested on its own.

#include <cstdint>
#include <string>
#include <vector>

// Where a value lives in the game's memory, e.g. "player:0x1234" or "engine:0x170".
struct MemoryRef {
  enum class Source { None, Engine, Player };
  Source source = Source::None;
  uint32_t offset = 0;

  bool valid() const { return source != Source::None; }
  static bool parse(const std::string &text, MemoryRef &out);
  std::string to_string() const;
};

struct OscConfig {
  std::string host = "127.0.0.1";
  uint16_t port = 9000;
  std::string prefix = "/ggst";

  // Game modes where the mod is active (see GAME_MODE in dllmain.cpp).
  // 6 = local versus, 7 = versus (preinstall variant), 8 = training.
  std::vector<int> modes = {6, 7, 8};

  bool send_states = true;         // /pN/state on every script state change
  bool send_frame = false;         // /frame every battle frame
  bool log_moves = false;          // print moves in the UE4SS console
  bool skip_rollback_frames = false; // ignore frames replayed by online rollback
  bool debug = false;              // periodic counters in UE4SS.log
  int rounds_to_win = 2;           // to compute /pN/lives

  MemoryRef risc_p1;               // unset until found with the RISC finder
  MemoryRef risc_p2;
  bool risc_finder = false;        // F6/F7/F8 helper to locate the RISC gauge

  // Parses "key = value" lines. Unknown keys are reported in `warnings`.
  void load_from_text(const std::string &text, std::vector<std::string> &warnings);

  static const char *default_file_contents();
};
