#pragma once

// Helper to locate the R.I.S.C. gauge, whose offset is not documented.
// Enabled with risc_finder = 1 in osc_config.ini, used in training mode on
// the dummy (player 2):
//   F6  start: training reset, the dummy's RISC is empty
//   F7  "it went up": after the dummy blocked a few attacks
//   F8  "it went down": after the gauge drained
// Each key narrows the list of candidate memory locations; the survivors are
// printed in the UE4SS console.

#include <functional>
#include <string>
#include <utility>
#include <vector>

class RiscFinder {
public:
  using LogFn = std::function<void(const std::string &)>;
  explicit RiscFinder(LogFn log) : log_(std::move(log)) {}

  void poll(); // call once per battle frame

private:
  struct Candidate {
    bool engine; // true: asw_engine, false: P2 player entity
    unsigned offset;
    int value;
  };

  void start();
  void filter(bool went_up);
  void report();
  bool read(bool engine, unsigned offset, int &value) const;

  LogFn log_;
  std::vector<Candidate> candidates_;
  bool started_ = false;
  bool key_was_down_[3] = {};
};
