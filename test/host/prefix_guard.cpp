// Marcduino prefix-collision guard for the RSeries fork.
// Part of the DroidNet RSeries fork. Copyright (c) 2026 Travis Cook. LGPL-2.1-only.
//
// Usage:  prefix_guard [repo root] [catalog snapshot]
//         defaults: ../..  and  catalog_example_lines.tsv  (test/host/run.sh runs it from test/host)
//
// WHY. Marcduino::processCommand (lib/Reeltwo/src/core/Marcduino.h:37-54) runs EVERY registered
// action whose text is a prefix of the line (plus the '@'+digit rule: an action "@1T1" also
// matches a digit-led line "1T1..."), and each match calls player.animateOnce(), which replaces
// the ONE player slot. So when two actions match one line, only the last-registered one runs
// and the other is dropped without a word. This guard reads every registered action text from
// the four files that register them and fails if:
//   (a) "#LEBRI" is related by prefix, in either direction, to any other action;
//   (b) any other prefix pair exists among the actions, except exactly the three stock pairs
//       @0T1->@0T11, @1T1->@1T11, @2T1->@2T11 (MarcduinoLogics.h), which work only because the
//       longer text is registered later; the guard also checks that order;
//   (c) any catalog line whose command id is not rseries.cfg.brightness would run "#LEBRI";
//   (d) any rseries.cfg.brightness line would run an action other than "#LEBRI", or its tail
//       is rejected by nativeBrightnessParse().
// The catalog snapshot is catalog_example_lines.tsv, written by refresh_catalog_examples.py.
#include "../../src/native/native_brightness_parse.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

static int g_fail = 0;
static void fail(const std::string& msg) { g_fail++; printf("FAIL %s\n", msg.c_str()); }

struct Action { std::string text, name, file; int line; };

static const char* const kBrightnessText = "#LEBRI";
static const char* const kBrightnessId = "rseries.cfg.brightness";
static const char* const kSourceFiles[] = {
  "MarcduinoLogics.h", "src/main.cpp", "src/contract/ContractLogics.h", "src/native/NativeBrightness.h",
};
// (shorter, longer): the stock pairs that exist today and are safe only by registration order.
static const std::set<std::pair<std::string, std::string>> kAllowedPairs = {
  {"@0T1", "@0T11"}, {"@1T1", "@1T11"}, {"@2T1", "@2T11"},
};

// Exactly Marcduino.h:40-42: strncmp over the action's length, or the '@'+digit rule, which
// compares the line with the text after '@' over len-1 chars when the line starts with a digit.
static bool lineRuns(const std::string& line, const std::string& action) {
  const size_t len = action.size();
  if (line.compare(0, len, action) == 0) return true;
  return action[0] == '@' && !line.empty() && isdigit(static_cast<unsigned char>(line[0])) &&
         line.compare(0, len - 1, action, 1, len - 1) == 0;
}

// The shortest lines that run an action: its text, and for "@<digit>..." the text without '@'.
static std::vector<std::string> witnesses(const std::string& action) {
  std::vector<std::string> w{action};
  if (action.size() >= 2 && action[0] == '@' && isdigit(static_cast<unsigned char>(action[1])))
    w.push_back(action.substr(1));
  return w;
}

// Two actions collide iff some line runs both. If one does, the longer of the two matched
// forms is itself such a line, so checking every witness of each against the other is exact.
static bool collide(const std::string& a, const std::string& b) {
  for (const std::string& w : witnesses(b)) if (lineRuns(w, a)) return true;
  for (const std::string& w : witnesses(a)) if (lineRuns(w, b)) return true;
  return false;
}

// Blank out comments, keeping string/char literals and line numbers intact.
static std::string stripComments(const std::string& src) {
  std::string out;
  out.reserve(src.size());
  size_t i = 0;
  while (i < src.size()) {
    const char c = src[i];
    if (c == '"' || c == '\'') {
      out += c; i++;
      while (i < src.size() && src[i] != c && src[i] != '\n') {
        if (src[i] == '\\' && i + 1 < src.size()) { out += src[i++]; }
        out += src[i++];
      }
      if (i < src.size()) out += src[i++];
    } else if (c == '/' && i + 1 < src.size() && src[i + 1] == '/') {
      while (i < src.size() && src[i] != '\n') i++;
    } else if (c == '/' && i + 1 < src.size() && src[i + 1] == '*') {
      i += 2;
      while (i + 1 < src.size() && !(src[i] == '*' && src[i + 1] == '/')) { if (src[i] == '\n') out += '\n'; i++; }
      i += 2;
    } else {
      out += c; i++;
    }
  }
  return out;
}

static bool readFile(const std::string& path, std::string& out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  std::stringstream ss; ss << in.rdbuf(); out = ss.str();
  return true;
}

static std::vector<Action> readActions(const std::string& root, const char* rel) {
  std::vector<Action> found;
  std::string src;
  if (!readFile(root + "/" + rel, src)) { fail(std::string("cannot read ") + root + "/" + rel); return found; }
  const std::string code = stripComments(src);
  static const std::regex re(R"(MARCDUINO_(?:ACTION|ANIMATION)\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*,\s*([^,\s]+)\s*,)");
  for (auto it = std::sregex_iterator(code.begin(), code.end(), re); it != std::sregex_iterator(); ++it) {
    const int line = 1 + static_cast<int>(std::count(code.begin(), code.begin() + it->position(0), '\n'));
    found.push_back(Action{(*it)[2].str(), (*it)[1].str(), rel, line});
  }
  return found;
}

// Self-test: pin lineRuns() to the Marcduino.h behaviour it simulates.
static void selfTest() {
  struct Case { const char* line; const char* action; bool runs; };
  static const Case cases[] = {
    {"@1T1", "@1T1", true}, {"1T1", "@1T1", true}, {"@1T11", "@1T1", true}, {"1T11", "@1T1", true},
    {"1T1", "@1T11", false}, {"@1T", "@1T1", false}, {"LE1", "@LE", false}, {"1LE", "@LE", false},
    {"@LE1", "@LE", true}, {"#LEBRI128", "#LEBRI", true}, {"#LEBR", "#LEBRI", false},
    {"#LEBRI", "#LEB", true}, {"!LFA", "!", true}, {"~RTLE1", "~RT", true},
  };
  for (const Case& c : cases)
    if (lineRuns(c.line, c.action) != c.runs)
      fail(std::string("self-test: lineRuns(\"") + c.line + "\", \"" + c.action + "\") != " + (c.runs ? "true" : "false"));
  if (!collide("@1T1", "@1T11") || collide("@1T1", "@2T1") || !collide("#LEB", "#LEBRI") || collide("#LEBRI", "#LEWIFI"))
    fail("self-test: collide()");
}

int main(int argc, char** argv) {
  const std::string root = argc > 1 ? argv[1] : "../..";
  const std::string tsv = argc > 2 ? argv[2] : "catalog_example_lines.tsv";
  selfTest();

  // ---- the registered actions --------------------------------------------------------------
  std::vector<Action> actions;
  std::string counts;
  for (const char* rel : kSourceFiles) {
    const std::vector<Action> got = readActions(root, rel);
    if (got.empty()) fail(std::string("no MARCDUINO_ACTION found in ") + rel + " (extraction broke?)");
    counts += (counts.empty() ? "" : ", ") + std::to_string(got.size()) + " " + rel;
    actions.insert(actions.end(), got.begin(), got.end());
  }
  int brightness = 0;
  for (const Action& a : actions) {
    if (a.text.empty() || a.text == "@") fail("action " + a.name + " has an unusable text \"" + a.text + "\"");
    if (a.text == kBrightnessText) brightness++;
  }
  if (brightness != 1) fail("expected exactly one \"#LEBRI\" action, found " + std::to_string(brightness));

  // ---- (a) and (b): prefix pairs among the actions -------------------------------------------
  std::set<std::pair<std::string, std::string>> seenAllowed;
  for (size_t i = 0; i < actions.size(); i++) {
    for (size_t j = i + 1; j < actions.size(); j++) {
      const Action& x = actions[i];
      const Action& y = actions[j];
      if (!collide(x.text, y.text)) continue;
      const std::string where = x.name + " \"" + x.text + "\" (" + x.file + ":" + std::to_string(x.line) + ") and " +
                                y.name + " \"" + y.text + "\" (" + y.file + ":" + std::to_string(y.line) + ")";
      if (x.text == kBrightnessText || y.text == kBrightnessText) { fail("(a) #LEBRI collides: " + where); continue; }
      const bool xShort = x.text.size() <= y.text.size();
      const std::pair<std::string, std::string> key = xShort ? std::make_pair(x.text, y.text) : std::make_pair(y.text, x.text);
      if (kAllowedPairs.count(key) == 0) { fail("(b) prefix pair not on the allowlist: " + where); continue; }
      seenAllowed.insert(key);
      // Safe only while the longer text is registered later (later animateOnce() wins).
      const Action& shorter = xShort ? x : y;
      const Action& longer = xShort ? y : x;
      if (shorter.file != longer.file || longer.line <= shorter.line)
        fail("(b) allowlisted pair is no longer registered shorter-first: " + where);
    }
  }
  for (const auto& p : kAllowedPairs)
    if (seenAllowed.count(p) == 0) fail("(b) allowlisted pair " + p.first + " -> " + p.second + " not found; update the allowlist");

  // ---- (c) and (d): the catalog snapshot --------------------------------------------------------
  std::string text;
  if (!readFile(tsv, text)) { fail("cannot read " + tsv); text.clear(); }
  std::istringstream rows(text);
  std::string row;
  int lines = 0, brightnessRows = 0, rowNo = 0;
  while (std::getline(rows, row)) {
    rowNo++;
    if (row.empty() || row.rfind("# ", 0) == 0) continue;
    const size_t tab = row.find('\t');
    if (tab == std::string::npos) { fail(tsv + ":" + std::to_string(rowNo) + ": no TAB in row"); continue; }
    const std::string id = row.substr(0, tab);
    const std::string line = row.substr(tab + 1);
    lines++;
    if (id != kBrightnessId) {
      if (lineRuns(line, kBrightnessText)) fail("(c) " + id + " line \"" + line + "\" would run #LEBRI");
      continue;
    }
    brightnessRows++;
    for (const Action& a : actions)
      if (a.text != kBrightnessText && lineRuns(line, a.text))
        fail("(d) " + id + " line \"" + line + "\" also runs " + a.name + " \"" + a.text + "\"");
    uint8_t mask = 0, level = 0;
    if (!lineRuns(line, kBrightnessText) || !nativeBrightnessParse(line.c_str() + 6, mask, level))
      fail("(d) " + id + " line \"" + line + "\" is not a valid #LEBRI line");
  }
  if (lines == 0) fail("no catalog lines in " + tsv);

  printf("%s  %zu actions (%s); %zu allowlisted stock pairs; %d catalog lines, %d %s rows\n",
         g_fail ? "FAILURES" : "OK", actions.size(), counts.c_str(), seenAllowed.size(), lines,
         brightnessRows, kBrightnessId);
  return g_fail ? 1 : 0;
}
