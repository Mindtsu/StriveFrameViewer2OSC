#include "game_reporter.h"

#include <algorithm>
#include <utility>

GameReporter::GameReporter(const OscConfig &config, SendFn send, LogFn log)
    : config_(config), send_(std::move(send)), log_(std::move(log)) {}

std::string GameReporter::path(int player, const char *leaf) const {
  return config_.prefix + (player == 0 ? "/p1/" : "/p2/") + leaf;
}

std::string GameReporter::path(const char *leaf) const {
  return config_.prefix + "/" + leaf;
}

void GameReporter::on_match_start() {
  match_start_pending_ = true;
}

void GameReporter::send_rounds(int index) {
  const auto &me = tracks_[index];
  const auto &opponent = tracks_[1 - index];
  send_(OscMessage(path(index, "rounds")).add(static_cast<int32_t>(me.wins)));
  // "Lives": rounds the opponent still has to win to take the match.
  const int lives = std::max(0, config_.rounds_to_win - opponent.wins);
  send_(OscMessage(path(index, "lives")).add(static_cast<int32_t>(lives)));
}

void GameReporter::on_frame(const FrameSnapshot &frame) {
  ++frame_;

  if (match_start_pending_ || frame.rematch) {
    match_start_pending_ = false;
    round_active_ = false;
    for (auto &track : tracks_) {
      const int hp_max = track.hp_max; // keep the full-health reference
      track = Track{};
      track.hp_max = hp_max;
    }
    send_(OscMessage(path("match/start")));
    force_all_ = true;
  }

  if (frame.battle_start && !round_active_) {
    round_active_ = true;
    send_(OscMessage(path("round/start")).add(static_cast<int32_t>(frame.round_count)));
    // Health is full when a round starts: use it as the reference for hp_norm.
    for (int i = 0; i < 2; ++i) {
      if (frame.players[i].valid) tracks_[i].hp_max = frame.players[i].hp;
    }
    force_all_ = true;
  }

  if (frame.round_end && round_active_) {
    round_active_ = false;
    send_(OscMessage(path("round/end")).add(static_cast<int32_t>(frame.round_count)));
  }

  if (config_.send_frame) send_(OscMessage(path("frame")).add(static_cast<int32_t>(frame_ & 0x7FFFFFFF)));

  for (int i = 0; i < 2; ++i) process_player(i, frame.players[i]);

  if (force_all_) {
    send_rounds(0);
    send_rounds(1);
  }
  force_all_ = false;
}

void GameReporter::process_player(int index, const PlayerSnapshot &snap) {
  auto &t = tracks_[index];
  if (!snap.valid) {
    t.valid = false;
    return;
  }
  const bool first = !t.valid;
  const bool force = first || force_all_;

  // ---------- Actions ----------
  // A new action starts when the script state changes, or when the same
  // state restarts (its timer goes back down, e.g. 5P cancelled into 5P).
  const bool new_action = !first && (snap.state != t.state || snap.action_time < t.action_time);

  if (config_.send_states && !snap.state.empty() && (new_action || first)) {
    send_(OscMessage(path(index, "state")).add(snap.state));
    if (config_.log_moves && snap.state.rfind("CmnAct", 0) != 0) {
      log_((index == 0 ? "P1 state: " : "P2 state: ") + snap.state);
    }
  }

  // The move (skill) recognised by the game's command interpreter.
  const bool skill_valid = !snap.skill_name.empty();
  const bool skill_changed = !first && skill_valid && snap.skill_id != t.skill_id;
  const bool skill_restarted = new_action && skill_valid && snap.state == snap.skill_name;
  if (skill_changed || skill_restarted) {
    // The skill request and the state change can land on neighbouring
    // frames: don't report the same move twice.
    const bool duplicate = snap.skill_name == t.last_move && frame_ - t.last_move_frame <= 2;
    if (!duplicate) {
      send_(OscMessage(path(index, "move")).add(snap.skill_name));
      if (config_.log_moves) log_((index == 0 ? "P1 move: " : "P2 move: ") + snap.skill_name + "  (state " + snap.state + ")");
      t.last_move = snap.skill_name;
      t.last_move_frame = frame_;
    }
  }

  // ---------- Health ----------
  t.hp_max = std::max(t.hp_max, snap.hp);
  if (!first && snap.hp < t.hp) {
    send_(OscMessage(path(index, "damage")).add(static_cast<int32_t>(t.hp - snap.hp)));
  }
  if (force || snap.hp != t.hp) {
    send_(OscMessage(path(index, "hp")).add(static_cast<int32_t>(snap.hp)));
    const float norm = t.hp_max > 0 ? static_cast<float>(snap.hp) / static_cast<float>(t.hp_max) : 0.f;
    send_(OscMessage(path(index, "hp_norm")).add(std::clamp(norm, 0.f, 1.f)));
  }

  // ---------- Gauges ----------
  if (force || snap.tension_raw != t.tension_raw) {
    send_(OscMessage(path(index, "tension")).add(static_cast<float>(snap.tension_raw) / 100.f));
  }
  if (force || snap.burst_raw != t.burst_raw) {
    send_(OscMessage(path(index, "burst")).add(static_cast<float>(snap.burst_raw) / 150.f));
  }
  if (snap.has_risc && (force || snap.risc != t.risc)) {
    send_(OscMessage(path(index, "risc")).add(static_cast<int32_t>(snap.risc)));
  }

  // ---------- Rounds ----------
  const bool won_now = !first && snap.cmn_action != t.cmn_action &&
                       (snap.cmn_action == CMN_ROUND_WIN || snap.cmn_action == CMN_MATCH_WIN);

  t.valid = true;
  t.state = snap.state;
  t.action_time = snap.action_time;
  t.skill_id = snap.skill_id;
  t.hp = snap.hp;
  t.tension_raw = snap.tension_raw;
  t.burst_raw = snap.burst_raw;
  t.risc = snap.risc;
  t.cmn_action = snap.cmn_action;

  if (won_now) {
    ++t.wins;
    send_(OscMessage(path(index, "round_win")));
    send_rounds(0);
    send_rounds(1);
  }
}
