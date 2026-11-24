#include "osc_config.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>

namespace {
  std::string trim(const std::string &text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
  }

  std::string lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
  }

  bool parse_bool(const std::string &value, bool &out) {
    const auto v = lower(value);
    if (v == "1" || v == "true" || v == "yes" || v == "on" || v == "oui") { out = true; return true; }
    if (v == "0" || v == "false" || v == "no" || v == "off" || v == "non") { out = false; return true; }
    return false;
  }

  bool parse_int(const std::string &value, long long &out) {
    try {
      size_t used = 0;
      out = std::stoll(value, &used, 0); // base 0: accepts 0x prefix
      return used == value.size();
    } catch (...) {
      return false;
    }
  }
} // namespace

bool MemoryRef::parse(const std::string &text, MemoryRef &out) {
  const auto t = lower(trim(text));
  if (t.empty() || t == "none" || t == "0") {
    out = MemoryRef{};
    return true;
  }
  const auto colon = t.find(':');
  if (colon == std::string::npos) return false;
  const auto source = t.substr(0, colon);
  long long offset = 0;
  if (!parse_int(trim(t.substr(colon + 1)), offset) || offset < 0 || offset > 0x100000 || offset % 4 != 0) return false;

  MemoryRef ref;
  if (source == "engine") ref.source = Source::Engine;
  else if (source == "player") ref.source = Source::Player;
  else return false;
  ref.offset = static_cast<uint32_t>(offset);
  out = ref;
  return true;
}

std::string MemoryRef::to_string() const {
  if (source == Source::None) return "none";
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%s:0x%X", source == Source::Engine ? "engine" : "player", offset);
  return buffer;
}

void OscConfig::load_from_text(const std::string &text, std::vector<std::string> &warnings) {
  std::istringstream stream(text);
  std::string line;
  int line_number = 0;
  while (std::getline(stream, line)) {
    ++line_number;
    // Strip UTF-8 BOM on the first line.
    if (line_number == 1 && line.size() >= 3 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);
    line = trim(line);
    if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[') continue;

    const auto equals = line.find('=');
    if (equals == std::string::npos) {
      warnings.push_back("line " + std::to_string(line_number) + ": missing '='");
      continue;
    }
    const auto key = lower(trim(line.substr(0, equals)));
    auto value = trim(line.substr(equals + 1));
    // Allow trailing comments: "port = 9000 ; comment"
    if (const auto comment = value.find_first_of("#;"); comment != std::string::npos) value = trim(value.substr(0, comment));

    const auto bad = [&] { warnings.push_back("line " + std::to_string(line_number) + ": invalid value for '" + key + "'"); };
    long long number = 0;
    bool flag = false;

    if (key == "host") {
      host = value;
    } else if (key == "port") {
      if (parse_int(value, number) && number > 0 && number < 65536) port = static_cast<uint16_t>(number);
      else bad();
    } else if (key == "prefix") {
      if (!value.empty() && value[0] == '/') {
        while (value.size() > 1 && value.back() == '/') value.pop_back();
        prefix = value == "/" ? "" : value;
      } else bad();
    } else if (key == "modes") {
      std::vector<int> parsed;
      std::istringstream list(value);
      std::string item;
      bool ok = true;
      while (std::getline(list, item, ',')) {
        if (parse_int(trim(item), number) && number >= 0 && number < 64) parsed.push_back(static_cast<int>(number));
        else ok = false;
      }
      if (ok && !parsed.empty()) modes = parsed;
      else bad();
    } else if (key == "send_states") {
      if (parse_bool(value, flag)) send_states = flag; else bad();
    } else if (key == "send_frame") {
      if (parse_bool(value, flag)) send_frame = flag; else bad();
    } else if (key == "log_moves") {
      if (parse_bool(value, flag)) log_moves = flag; else bad();
    } else if (key == "rounds_to_win") {
      if (parse_int(value, number) && number >= 1 && number <= 9) rounds_to_win = static_cast<int>(number);
      else bad();
    } else if (key == "risc_p1") {
      if (!MemoryRef::parse(value, risc_p1)) bad();
    } else if (key == "risc_p2") {
      if (!MemoryRef::parse(value, risc_p2)) bad();
    } else if (key == "risc_finder") {
      if (parse_bool(value, flag)) risc_finder = flag; else bad();
    } else {
      warnings.push_back("line " + std::to_string(line_number) + ": unknown key '" + key + "'");
    }
  }
}

const char *OscConfig::default_file_contents() {
  // Written only when the file is missing. The packaged osc_config.ini
  // has the same keys with more detailed (French) explanations.
  return "; StriveOSC settings. Restart the game after editing.\n"
         "host = 127.0.0.1        ; loopback only (127.x.x.x)\n"
         "port = 9000\n"
         "prefix = /ggst\n"
         "modes = 6,7,8           ; 6 = local versus, 8 = training\n"
         "send_states = 1\n"
         "send_frame = 0\n"
         "log_moves = 0\n"
         "rounds_to_win = 2\n"
         "risc_p1 = none\n"
         "risc_p2 = none\n"
         "risc_finder = 0\n";
}
