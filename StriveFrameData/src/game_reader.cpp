#include "game_reader.h"

#include "arcsys.h"

#include <cstring>

namespace {
  std::string bounded_string(const char *text, size_t max_length) {
    if (!text) return {};
    return std::string(text, strnlen(text, max_length));
  }

  int read_int(const void *base, uint32_t offset) {
    if (!base) return 0;
    return *reinterpret_cast<const int *>(reinterpret_cast<const char *>(base) + offset);
  }

  void read_player(const OscConfig &config, asw_engine *engine, int index, PlayerSnapshot &out) {
    asw_player *player = engine->players[index].entity;
    if (!player) {
      out.valid = false;
      return;
    }
    out.valid = true;

    out.state = bounded_string(player->get_BB_state(), 32);
    out.action_time = player->action_time;

    // A move is reported when the character's current script state is one of
    // the moves the game registered for it (m_SkillInfo): the game has then
    // accepted the command. The pending-request field (action_info_reg) is
    // consumed during the update, so it is already empty when read here.
    out.skill_id = -1;
    out.skill_name.clear();
    long move_count = player->move_datas.move_count;
    if (move_count > 180) move_count = 180;
    for (long i = 0; i < move_count; ++i) {
      if (skill_name_matches_state(player->move_datas.moves[i].get_name(), 20, out.state)) {
        out.skill_id = static_cast<int>(i);
        out.skill_name = out.state;
        break;
      }
    }

    out.hp = player->hp;
    out.tension_raw = index == 0 ? engine->P1Tension : engine->P2Tension;
    out.burst_raw = index == 0 ? engine->P1BurstRaw : engine->P2BurstRaw;
    out.cmn_action = static_cast<int>(player->cur_cmn_action_id);

    const MemoryRef &risc = index == 0 ? config.risc_p1 : config.risc_p2;
    out.has_risc = risc.valid();
    if (risc.source == MemoryRef::Source::Engine) out.risc = read_int(engine, risc.offset);
    else if (risc.source == MemoryRef::Source::Player) out.risc = read_int(player, risc.offset);
  }
} // namespace

std::vector<std::string> registered_move_names(int player_index) {
  std::vector<std::string> names;
  auto *engine = asw_engine::get();
  if (!engine || player_index < 0 || player_index > 1) return names;
  asw_player *player = engine->players[player_index].entity;
  if (!player) return names;
  long move_count = player->move_datas.move_count;
  if (move_count > 180) move_count = 180;
  for (long i = 0; i < move_count; ++i) names.push_back(bounded_string(player->move_datas.moves[i].get_name(), 20));
  return names;
}

BattleEvents read_battle_events() {
  BattleEvents result;
  auto *events = asw_events::get();
  if (!events) return result;

  auto count = events->event_count;
  if (count > 10) count = 10;
  for (unsigned int idx = 0; idx < count; ++idx) {
    switch (events->events[idx].type) {
      case BOM_EVENT_BATTLE_START: result.battle_start = true; break;
      case BOM_EVENT_DECISION:
      case BOM_EVENT_RESET: result.round_end = true; break;
      case BOM_EVENT_RESET_REMATCH: result.rematch = true; break;
      default: break;
    }
  }
  return result;
}

bool is_resimulating() {
  auto *rollback = asw_rollback::get();
  return rollback && (rollback->bDoingRollbackProcess || rollback->IsRestoreAndResim);
}

bool read_frame(const OscConfig &config, const BattleEvents &events, FrameSnapshot &out) {
  auto *engine = asw_engine::get();
  if (!engine) return false;

  for (int i = 0; i < 2; ++i) read_player(config, engine, i, out.players[i]);

  auto *state = asw_state::get();
  out.round_count = state ? state->RoundCount : 0;
  out.battle_start = events.battle_start;
  out.round_end = events.round_end;
  out.rematch = events.rematch;
  return true;
}
