// src/contract/contract_core.h — pure, dependency-free contract parsing/logic.
// Host-compilable (no Arduino/FastLED/Reeltwo). The firmware layer includes this too.
#pragma once
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

struct RGB { uint8_t r, g, b; };

enum ContractVerb : uint8_t {
  CV_NONE = 0, CV_ANIMATE, CV_PULSE, CV_CLOCK, CV_BRIGHT, CV_LEVEL, CV_STOP, CV_MODE, CV_QUERY
};
enum ContractEffect : uint8_t {
  CE_NONE = 0, CE_OFF, CE_SOLID, CE_FLASH, CE_PULSE, CE_RAINBOW, CE_SCAN, CE_SPARKLE, CE_METER, CE_NATIVE
};

struct ContractParams {
  bool hasEffect = false;  ContractEffect effect = CE_NONE;  int nativeCode = -1;
  bool hasColor = false;   RGB color{0, 0, 0};
  bool hasSpeed = false;   uint8_t speed = 0;
  bool hasDur = false;     uint32_t durMs = 0;
  bool hasBright = false;  uint8_t bright = 0;
  bool hasBeatMod = false; uint8_t beatMod = 0;
  bool hasAt = false;      int32_t atBeat = -1;
  bool hasAm = false;      uint8_t accentMode = 0;
  // clock params (Plan 2): parsed here but unused in Phase-1
  bool hasBpm = false;     uint16_t bpm = 0;
  bool hasPh = false;      uint32_t phMs = 0;
  bool hasBpb = false;     uint8_t bpb = 4;
  bool hasBeat = false;    int32_t beatAnchor = 0;
  bool hasLevel = false;   uint8_t level = 0;   // verb L value
  char mode = 0;           // verb M: 's'(how) | 'i'(dle)
};

struct ParsedContract {
  bool valid = false;
  char cls = 0;   // 'L' 'P' 'H' '*'
  char unit = 0;  // 'F' 'R' 'T' '*'
  ContractVerb verb = CV_NONE;
  ContractParams params;
};

// ===== PARSE IMPL (Tasks 2/3) =====
inline ContractVerb _verbFromChar(char v) {
  switch (v) {
    case 'A': return CV_ANIMATE; case 'P': return CV_PULSE; case 'C': return CV_CLOCK;
    case 'B': return CV_BRIGHT;  case 'L': return CV_LEVEL;  case 'X': return CV_STOP;
    case 'M': return CV_MODE;    case 'Q': return CV_QUERY;  default: return CV_NONE;
  }
}
inline int _hexNib(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
inline bool _parseHex6(const char* s, size_t len, RGB& out) {
  if (len < 6) return false;
  int v[6];
  for (int i = 0; i < 6; i++) { v[i] = _hexNib(s[i]); if (v[i] < 0) return false; }
  out.r = (uint8_t)(v[0] * 16 + v[1]);
  out.g = (uint8_t)(v[2] * 16 + v[3]);
  out.b = (uint8_t)(v[4] * 16 + v[5]);
  return true;
}
inline ContractEffect _effectFromName(const char* s, size_t len, int& nativeCode) {
  nativeCode = -1;
  auto eq = [&](const char* k) { size_t kl = strlen(k); return kl == len && strncmp(s, k, kl) == 0; };
  if (eq("off")) return CE_OFF;         if (eq("solid")) return CE_SOLID;
  if (eq("flash")) return CE_FLASH;     if (eq("pulse")) return CE_PULSE;
  if (eq("rainbow")) return CE_RAINBOW; if (eq("scan")) return CE_SCAN;
  if (eq("sparkle")) return CE_SPARKLE; if (eq("meter")) return CE_METER;
  if (len > 7 && strncmp(s, "native:", 7) == 0) { nativeCode = atoi(s + 7); return CE_NATIVE; }
  return CE_NONE;
}
// One key=value token. k/kl and v/vl are slices into the (null-terminated) source
// buffer; numeric parses (atoi/strtoul) read v up to the next ',' or '\0'.
inline void _applyParam(ContractParams& pr, const char* k, size_t kl, const char* v, size_t vl) {
  auto key = [&](const char* s) { return kl == strlen(s) && strncmp(k, s, kl) == 0; };
  if (key("i")) {
    int nc; ContractEffect e = _effectFromName(v, vl, nc);
    if (e != CE_NONE) { pr.hasEffect = true; pr.effect = e; pr.nativeCode = nc; }
  } else if (key("c")) {
    RGB rgb; if (_parseHex6(v, vl, rgb)) { pr.hasColor = true; pr.color = rgb; }
  } else if (key("s"))  { pr.hasSpeed = true;   pr.speed = (uint8_t)atoi(v); }
  else if (key("d"))    { pr.hasDur = true;     pr.durMs = (uint32_t)strtoul(v, nullptr, 10); }
  else if (key("b"))    { pr.hasBright = true;  pr.bright = (uint8_t)atoi(v); }
  else if (key("m"))    { pr.hasBeatMod = true; pr.beatMod = (uint8_t)atoi(v); }
  else if (key("at"))   { pr.hasAt = true;      pr.atBeat = atoi(v); }
  else if (key("am"))   { pr.hasAm = true;      pr.accentMode = (uint8_t)atoi(v); }
  else if (key("v"))    { pr.hasLevel = true;   pr.level = (uint8_t)atoi(v);
                          if (vl > 0 && (v[0] == 's' || v[0] == 'i')) pr.mode = v[0]; }
  else if (key("bpm"))  { pr.hasBpm = true;     pr.bpm = (uint16_t)atoi(v); }
  else if (key("ph"))   { pr.hasPh = true;      pr.phMs = (uint32_t)strtoul(v, nullptr, 10); }
  else if (key("bpb"))  { pr.hasBpb = true;     pr.bpb = (uint8_t)atoi(v); }
  else if (key("beat")) { pr.hasBeat = true;    pr.beatAnchor = atoi(v); }
  // "sw" (swing) and any unknown key: parsed-and-ignored (forward-compatible)
}
inline void _parseParams(const char* s, ContractParams& pr) {
  while (*s) {
    const char* kstart = s; while (*s && *s != '=' && *s != ',') s++;
    size_t kl = (size_t)(s - kstart);
    const char* vstart = s; size_t vl = 0;
    if (*s == '=') { s++; vstart = s; while (*s && *s != ',') s++; vl = (size_t)(s - vstart); }
    if (kl) _applyParam(pr, kstart, kl, vstart, vl);
    if (*s == ',') s++;
  }
}
// afterBang = the bytes AFTER the leading '!'. Returns true on a well-formed
// scope+verb. Unknown params are ignored (forward-compatible).
inline bool contractParse(const char* afterBang, ParsedContract& out) {
  out = ParsedContract{};
  if (!afterBang) return false;
  size_t n = strlen(afterBang);
  if (n < 3) return false;                         // need class + unit + verb
  char cls = afterBang[0], unit = afterBang[1];
  if (cls != 'L' && cls != 'P' && cls != 'H' && cls != '*') return false;
  if (unit != 'F' && unit != 'R' && unit != 'T' && unit != '*') return false;
  ContractVerb verb = _verbFromChar(afterBang[2]);
  if (verb == CV_NONE) return false;
  out.cls = cls; out.unit = unit; out.verb = verb;
  if (n > 3 && afterBang[3] == ':') _parseParams(afterBang + 4, out.params);
  out.valid = true;
  return true;
}
// ===== END PARSE IMPL =====
