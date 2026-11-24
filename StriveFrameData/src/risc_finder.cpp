#include "risc_finder.h"

#include "arcsys.h"

#include <cstdio>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace {
  // Memory scanned, in bytes. The engine area ends with GameFrame (0x3E7C);
  // the player area stops before the static move list (0x10900).
  constexpr unsigned ENGINE_SCAN_SIZE = 0x3E80;
  constexpr unsigned PLAYER_SCAN_SIZE = 0x10900;
  constexpr int MAX_PLAUSIBLE_VALUE = 1000000;
  constexpr size_t MAX_LISTED = 20;

  std::string hex(unsigned value) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "0x%X", value);
    return buffer;
  }
} // namespace

bool RiscFinder::read(bool engine_area, unsigned offset, int &value) const {
  auto *engine = asw_engine::get();
  if (!engine) return false;
  const void *base = engine_area ? static_cast<const void *>(engine) : static_cast<const void *>(engine->players[1].entity);
  if (!base) return false;
  value = *reinterpret_cast<const int *>(reinterpret_cast<const char *>(base) + offset);
  return true;
}

void RiscFinder::poll() {
  const int keys[3] = {VK_F6, VK_F7, VK_F8};
  for (int i = 0; i < 3; ++i) {
    const bool down = (GetAsyncKeyState(keys[i]) & 0x8000) != 0;
    const bool pressed = down && !key_was_down_[i];
    key_was_down_[i] = down;
    if (!pressed) continue;
    if (i == 0) start();
    else filter(i == 1);
  }
}

void RiscFinder::start() {
  candidates_.clear();
  int value = 0;
  for (unsigned offset = 0; offset < ENGINE_SCAN_SIZE; offset += 4) {
    if (read(true, offset, value) && value == 0) candidates_.push_back({true, offset, value});
  }
  for (unsigned offset = 0; offset < PLAYER_SCAN_SIZE; offset += 4) {
    if (read(false, offset, value) && value == 0) candidates_.push_back({false, offset, value});
  }
  started_ = true;
  log_("RISC finder: started, " + std::to_string(candidates_.size()) + " candidates at 0. Make the dummy block, then press F7.");
}

void RiscFinder::filter(bool went_up) {
  if (!started_) {
    log_("RISC finder: press F6 first (after a training reset).");
    return;
  }
  std::vector<Candidate> kept;
  int value = 0;
  for (const auto &candidate : candidates_) {
    if (!read(candidate.engine, candidate.offset, value)) continue;
    const bool ok = went_up ? (value > candidate.value && value <= MAX_PLAUSIBLE_VALUE)
                            : (value < candidate.value && value >= 0);
    if (ok) kept.push_back({candidate.engine, candidate.offset, value});
  }
  candidates_ = std::move(kept);
  log_(std::string("RISC finder: ") + (went_up ? "went up" : "went down") + ", " + std::to_string(candidates_.size()) + " candidates left.");
  report();
}

void RiscFinder::report() {
  if (candidates_.empty()) {
    log_("RISC finder: no candidate left. Press F6 after a training reset to start again.");
    return;
  }
  if (candidates_.size() > MAX_LISTED) {
    log_("RISC finder: keep alternating F7 (after blocking) and F8 (after the gauge drained).");
    return;
  }
  for (const auto &candidate : candidates_) {
    log_(std::string("  candidate ") + (candidate.engine ? "engine:" : "player:") + hex(candidate.offset) + " = " + std::to_string(candidate.value));
  }
}
