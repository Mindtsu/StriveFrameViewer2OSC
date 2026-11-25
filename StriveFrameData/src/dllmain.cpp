// StriveOSC: sends what happens in a Guilty Gear -Strive- battle as OSC
// messages on the local machine (moves interpreted by the game, health,
// tension, burst, R.I.S.C., rounds).
//
// Based on StriveFrameViewer (pbozai, Sevoi, Procdox). The analysis tools
// (frame bar, hitboxes, pause, menu) were removed: this mod draws nothing.

#include <polyhook2/Detour/x64Detour.hpp>

#include "arcsys.h"
#include "game_reader.h"
#include "game_reporter.h"
#include "osc_config.h"
#include "osc_sender.h"
#include "risc_finder.h"
#include "sigscan.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>

#include <DynamicOutput/DynamicOutput.hpp>
#include <Mod/CppUserModBase.hpp>
#include <UE4SSProgram.hpp>
#include <Unreal/World.hpp>
#include <UnrealDef.hpp>

#define WIN32_LEAN_AND_MEAN
#include "Windows.h"

#define STRIVEOSC_API __declspec(dllexport)

/* Definitions */

class UREDGameCommon : public Unreal::UObject {};

using funcMatchStart_t = void (*)(AREDGameState_Battle *);
using funcGetGameMode_t = int (*)(UREDGameCommon *);
using funcUpdateBattle_t = void (*)(AREDGameState_Battle *, float);

void hook_MatchStart(AREDGameState_Battle *);
void hook_UpdateBattle(AREDGameState_Battle *, float);

// For reference, the values used in osc_config.ini "modes".
enum GAME_MODE : int32_t {
  GAME_MODE_DEBUG_BATTLE = 0x0,
  GAME_MODE_ADVERTISE = 0x1,
  GAME_MODE_MAINTENANCEVS = 0x2,
  GAME_MODE_ARCADE = 0x3,
  GAME_MODE_MOM = 0x4,
  GAME_MODE_SPARRING = 0x5,
  GAME_MODE_VERSUS = 0x6,
  GAME_MODE_VERSUS_PREINSTALL = 0x7,
  GAME_MODE_TRAINING = 0x8,
  GAME_MODE_TOURNAMENT = 0x9,
  GAME_MODE_RANNYU_VERSUS = 0xA,
  GAME_MODE_EVENT = 0xB,
  GAME_MODE_SURVIVAL = 0xC,
  GAME_MODE_STORY = 0xD,
  GAME_MODE_MAINMENU = 0xE,
  GAME_MODE_TUTORIAL = 0xF,
  GAME_MODE_LOBBYTUTORIAL = 0x10,
  GAME_MODE_CHALLENGE = 0x11,
  GAME_MODE_KENTEI = 0x12,
  GAME_MODE_MISSION = 0x13,
  GAME_MODE_GALLERY = 0x14,
  GAME_MODE_LIBRARY = 0x15,
  GAME_MODE_NETWORK = 0x16,
  GAME_MODE_REPLAY = 0x17,
  GAME_MODE_LOBBYSUB = 0x18,
  GAME_MODE_MAINMENU_QUICK_BATTLE = 0x19,
  GAME_MODE_UNDECIDED = 0x1A,
  GAME_MODE_INVALID = 0x1B,
};

/* Utilities */

namespace {
  std::wstring to_wide(const std::string &text) {
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
  }

  void log_line(const std::string &text) {
    RC::Output::send<LogLevel::Verbose>(STR("[StriveOSC] {}\n"), to_wide(text));
  }

  void log_warning(const std::string &text) {
    RC::Output::send<LogLevel::Warning>(STR("[StriveOSC] {}\n"), to_wide(text));
  }

  // <game>/.../Mods/StriveOSC/dlls/main.dll -> <game>/.../Mods/StriveOSC/osc_config.ini
  std::filesystem::path config_path() {
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&config_path), &module);
    wchar_t buffer[MAX_PATH * 2] = {};
    GetModuleFileNameW(module, buffer, static_cast<DWORD>(std::size(buffer)));
    return std::filesystem::path(buffer).parent_path().parent_path() / L"osc_config.ini";
  }

  OscConfig load_config() {
    OscConfig config;
    const auto path = config_path();
    std::ifstream file(path, std::ios::binary);
    if (!file) {
      std::ofstream created(path, std::ios::binary);
      if (created) created << OscConfig::default_file_contents();
      log_line("no osc_config.ini found next to the dlls folder, created one with the default settings");
      return config;
    }
    std::stringstream text;
    text << file.rdbuf();
    std::vector<std::string> warnings;
    config.load_from_text(text.str(), warnings);
    for (const auto &warning : warnings) log_warning("osc_config.ini " + warning);
    return config;
  }
} // namespace

/* State */

funcGetGameMode_t orig_GetGameMode = nullptr;
funcMatchStart_t orig_MatchStart = nullptr;
funcUpdateBattle_t orig_UpdateBattle = nullptr;

OscConfig g_config;
OscSender g_sender;
std::unique_ptr<GameReporter> g_reporter;
std::unique_ptr<RiscFinder> g_risc_finder;

class ModeWatcher {
  UREDGameCommon *game_common_ = nullptr;
  int last_mode_ = -1;
  bool allowed_ = false;

public:
  // Returns true when the current game mode is listed in "modes".
  bool is_mode_allowed() {
    if (!game_common_) {
      game_common_ = reinterpret_cast<UREDGameCommon *>(UObjectGlobals::FindFirstOf(FName(STR("REDGameCommon"))));
    }
    if (!game_common_ || !orig_GetGameMode) return false;

    const int mode = orig_GetGameMode(game_common_);
    if (mode != last_mode_) {
      last_mode_ = mode;
      allowed_ = std::find(g_config.modes.begin(), g_config.modes.end(), mode) != g_config.modes.end();
      log_line("game mode " + std::to_string(mode) + (allowed_ ? ": OSC active" : ": OSC inactive"));
      if (allowed_) g_sender.send(OscMessage(g_config.prefix + "/mode").add(static_cast<int32_t>(mode)));
    }
    return allowed_;
  }
} g_mode;

/* Hooks */

void hook_MatchStart(AREDGameState_Battle *game_state) {
  if (g_reporter) g_reporter->on_match_start();
  orig_MatchStart(game_state);
}

struct HookStats {
  unsigned long calls = 0;       // UpdateBattle calls seen
  unsigned long mode_off = 0;    // ... ignored because the game mode is not listed
  unsigned long resim = 0;       // ... where the game reported a rollback replay
  unsigned long reported = 0;    // ... that reached the reporter
} g_stats;

// Logs the registered move names when they change (new match, new character).
void log_registered_moves_if_changed() {
  static size_t last_count[2] = {0, 0};
  for (int i = 0; i < 2; ++i) {
    const auto names = registered_move_names(i);
    if (names.empty() || names.size() == last_count[i]) continue;
    last_count[i] = names.size();
    std::string text = "P" + std::to_string(i + 1) + " registered moves (" + std::to_string(names.size()) + "):";
    for (const auto &name : names) text += " " + name;
    log_warning(text);
  }
}

void hook_UpdateBattle(AREDGameState_Battle *game_state, float delta_time) {
  ++g_stats.calls;
  if (!g_mode.is_mode_allowed() || !g_reporter) {
    ++g_stats.mode_off;
    orig_UpdateBattle(game_state, delta_time);
    return;
  }

  const BattleEvents events = read_battle_events();
  orig_UpdateBattle(game_state, delta_time);

  // Rollback only exists online. The two flags below are not proven to stay
  // false in local play, so the frames are skipped only if asked for.
  const bool resimulating = is_resimulating();
  if (resimulating) ++g_stats.resim;

  if (!(resimulating && g_config.skip_rollback_frames)) {
    FrameSnapshot frame;
    if (read_frame(g_config, events, frame)) {
      g_reporter->on_frame(frame);
      ++g_stats.reported;
    }
    if (g_risc_finder) g_risc_finder->poll();
    if ((g_config.log_moves || g_config.debug) && g_stats.calls % 60 == 0) log_registered_moves_if_changed();
  }

  if (g_config.debug && g_stats.calls % 300 == 0) {
    log_warning("debug: hook calls=" + std::to_string(g_stats.calls) + " mode_off=" + std::to_string(g_stats.mode_off) +
                " resim_flag=" + std::to_string(g_stats.resim) + " reported=" + std::to_string(g_stats.reported));
  }
}

/* Mod Definition */

class StriveOSC : public CppUserModBase {
public:
  PLH::x64Detour *UpdateBattle_Detour = nullptr;
  PLH::x64Detour *MatchStart_Detour = nullptr;

  StriveOSC() : CppUserModBase() {
    ModName = STR("StriveOSC");
    ModVersion = STR("0.1");
    ModDescription = STR("Sends battle events (moves, health, tension, burst) as OSC on the local machine.");
    ModAuthors = STR("Mindtsu; based on StriveFrameViewer by pbozai, Sevoi, Procdox");
    Output::send<LogLevel::Verbose>(STR("StriveOSC started\n"));
  }

  ~StriveOSC() override {}

  auto on_update() -> void override {}

  auto on_unreal_init() -> void override {
    g_config = load_config();

    std::string error;
    if (!g_sender.open(g_config.host, g_config.port, error)) {
      log_warning("OSC disabled: " + error);
    } else {
      log_line("sending OSC to " + g_config.host + ":" + std::to_string(g_config.port) + " with prefix " + g_config.prefix);
    }

    g_reporter = std::make_unique<GameReporter>(
        g_config, [](const OscMessage &message) { g_sender.send(message); }, log_line);

    if (g_config.risc_finder) {
      g_risc_finder = std::make_unique<RiscFinder>(log_warning);
      log_warning("RISC finder enabled (F6 start, F7 went up, F8 went down).");
    }
    log_line("RISC p1 = " + g_config.risc_p1.to_string() + ", p2 = " + g_config.risc_p2.to_string());

    const uint64_t UpdateBattle_Addr = sigscan::get().scan("\x40\x53\x57\x41\x54\x41\x55\x48\x81\xEC\x88\x00\x00\x00", "xxxxxxxxxxxxxx");
    UpdateBattle_Detour = new PLH::x64Detour(UpdateBattle_Addr, reinterpret_cast<uint64_t>(&hook_UpdateBattle), reinterpret_cast<uint64_t *>(&orig_UpdateBattle));
    UpdateBattle_Detour->hook();

    // https://github.com/TheLettuceClub/StriveSAMMI/blob/7292cc225bd4de2b389e92b227c1b2793609c4e1/GGSTSammi/src/dllmain.cpp#L435
    const uint64_t MatchStart_Addr = sigscan::get().scan("\x48\x89\x5c\x24\x10\x55\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8d\x6c\x24\xf0\x48\x81\xec\x10\x01\x00\x00\x0f\x29\xb4\x24", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    MatchStart_Detour = new PLH::x64Detour(MatchStart_Addr, reinterpret_cast<uint64_t>(&hook_MatchStart), reinterpret_cast<uint64_t *>(&orig_MatchStart));
    MatchStart_Detour->hook();

    const uintptr_t GetGameMode_Addr = sigscan::get().scan("\x0F\xB6\x81\xF0\x02\x00\x00\xC3", "xxxxxxxx");
    orig_GetGameMode = reinterpret_cast<funcGetGameMode_t>(GetGameMode_Addr);

    ASWInitFunctions();
  }
};

extern "C" {
STRIVEOSC_API CppUserModBase *start_mod() {
  return new StriveOSC();
}

STRIVEOSC_API void uninstall_mod(CppUserModBase *mod) {
  delete mod;
}
}
