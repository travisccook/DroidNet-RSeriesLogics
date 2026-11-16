// S10: the logics' native allow-list, kept as a test. A MODEL of the stock sequences, not a
// measurement: nothing here has been seen on a display (the bench confirms it, plan step 27).
//
// The spec's test, S9's own measure applied to a stock look as the contract starts it (default
// colour, the speed the core gives it, no microphone source): by brightness and by luminance, no
// two ONSETS closer than 340 ms, over the whole field and over any region of a quarter of the
// field or more. An onset is counted the way the gate counts: a rise of more than a tenth above
// the last trough. The tenth is taken of the series' own peak, which is the strictest reading
// (the gate's ceiling is the owner's brightness, and a stock look is at or under it).
// A look whose lit area never reaches a quarter of the field passes without the count (S10: "or
// what moves is a lit area smaller than a quarter of the field").
//
// What is modelled, from lib/Reeltwo/src/dome/LogicEngine.h and effects/*.h:
//   * the palette as calculateAllColors() bakes it (LE:1427-1486), every LED walking it
//     (LE:1493-1519): the field mean is the time-weighted mean of the palette. A change of hue or
//     scale is taken as reaching every LED at once; the display smears it over a few frames, so
//     this is the worst case;
//   * FastLED's hsv2rgb_rainbow (the value is squared);
//   * the effect delay: a sequence that clears the engine's flip flag after it has acted (the
//     scan lines, fire, pulse, the roaming pixel, the colour wipe) acts once every delay; one
//     that only reads the flag (flash colour, march, the flip flops, colour swap, rainbow)
//     changes once every delay;
//   * the sequences that draw pixel by pixel, frame by frame, on each display size the engine
//     knows: the front 8 x 10, and the rear 16 x 4, 24 x 4, 28 x 4 and 32 x 8.
// The test: every code on the core's list passes at the speed the core gives it, on every
// display size, and that speed is the lowest one that does. The table it prints is the one in
// DROIDNET_FORK.md.
#include "../../src/contract/contract_core.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <cmath>
#include <functional>

static int g_checks = 0, g_fail = 0;
#define CHECK(c) do { g_checks++; if (!(c)) { g_fail++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

static uint32_t g_rng = 0x51F15EEDu;
static uint32_t rnd(uint32_t n) { g_rng = g_rng * 1664525u + 1013904223u; return n ? (g_rng >> 8) % n : 0; }

// ---- the onset count -------------------------------------------------------------------------
static const int FRAME_MS = 10;
static const int kNever = 1000000;
struct Count { int onsets = 0; int gap = kNever; double low = 100; };   // closest two onsets (ms); the trough, % of the peak
static Count onsets(const std::vector<double>& v) {
  Count c; double top = 0;
  for (double x : v) if (x > top) top = x;
  if (top <= 0) return c;
  const double step = top / 10.0;
  bool lit = false; double ref = v[0]; int last = -1; double low = top;
  for (size_t i = 0; i < v.size(); i++) {
    const double x = v[i];
    if (x < low) low = x;
    if (lit) { if (x > ref) ref = x; else if (ref - x > step) { lit = false; ref = x; } }
    else if (x < ref) ref = x;
    else if (x - ref > step) {
      lit = true; ref = x; c.onsets++;
      const int t = (int)i * FRAME_MS;
      if (last >= 0 && t - last < c.gap) c.gap = t - last;
      last = t;
    }
  }
  c.low = low * 100.0 / top;
  return c;
}

// ---- FastLED hsv2rgb_rainbow, and the engine's palette ---------------------------------------
static int scale8(int i, int s) { return (i * s) >> 8; }
static int scale8v(int i, int s) { return ((i * s) >> 8) + ((i && s) ? 1 : 0); }
static int map8(int i, int lo, int hi) { return lo + scale8(i, hi - lo); }
static void hsv(int h, int s, int v, int& r, int& g, int& b) {
  h &= 255;
  const int off8 = (h & 31) << 3, third = scale8(off8, 85), two = scale8(off8, 170);
  switch (h >> 5) {
    case 0: r = 255 - third; g = third; b = 0; break;
    case 1: r = 171; g = 85 + third; b = 0; break;
    case 2: r = 171 - two; g = 170 + third; b = 0; break;
    case 3: r = 0; g = 255 - third; b = third; break;
    case 4: r = 0; g = 171 - two; b = 85 + two; break;
    case 5: r = third; g = 0; b = 255 - third; break;
    case 6: r = 85 + third; g = 0; b = 171 - third; break;
    default: r = 170 + third; g = 0; b = 85 - third; break;
  }
  if (s != 255) {
    if (s == 0) r = g = b = 255;
    else { int d = 255 - s; d = scale8v(d, d); const int sc = 255 - d; r = scale8(r, sc) + d; g = scale8(g, sc) + d; b = scale8(b, sc) + d; }
  }
  if (v != 255) {
    v = scale8v(v, v);
    if (v == 0) r = g = b = 0; else { r = scale8(r, v); g = scale8(g, v); b = scale8(b, v); }
  }
}
static double lumOf(int r, int g, int b) { return 0.2126 * r + 0.7152 * g + 0.0722 * b; }
static const int kKeys[3][4][3] = {
  { {170,255,0}, {170,255,85}, {170,255,170}, {170,0,170} },   // front
  { {90,235,0}, {75,255,250}, {30,255,184}, {0,255,250} },     // rear
  { {0,255,0}, {0,255,0}, {0,255,100}, {0,255,250} } };        // palette 2: one hue, black to full
struct Disp { const char* name; int pal, bri, fade, delay; };  // LE:90-105
static const Disp kFront = { "front", 0, 160, 1, 10 }, kRear = { "rear", 1, 140, 3, 40 };
// The field's mean (level, luminance) for one palette, hue shift and display scale.
static void field(const Disp& d, int pal, int hue, int scale, double& level, double& lum) {
  const int tw = 14, n = 4 + tw * 3;
  int p[64][3];
  for (int k = 0; k < 4; k++) {
    const int t = k + tw * k;
    p[t][0] = kKeys[pal][k][0]; p[t][1] = kKeys[pal][k][1]; p[t][2] = map8(kKeys[pal][k][2], 1, d.bri);
    if (t + 1 != n)
      for (int j = 1; j <= tw; j++)
        for (int el = 0; el < 3; el++) {
          const int per = (kKeys[pal][k + 1][el] - kKeys[pal][k][el]) / (tw + 1);
          const int val = per ? ((kKeys[pal][k][el] + j * per) & 255) : kKeys[pal][k][el];
          p[t + j][el] = (el == 2) ? map8(val, 1, d.bri) : val;
        }
  }
  double sl = 0, sy = 0, sw = 0;
  for (int i = 0; i < 2 * n - 2; i++) {
    const int* c = p[i < n ? i : (n - 2) - (i - n)];
    const double w = ((i % 5 == 0) ? d.delay / 2.0 : d.fade) + 1;
    const int v = (scale == 255) ? c[2] : scale8(scale, c[2]);
    int r, g, b; hsv(c[0] + hue, c[1], v, r, g, b);
    sl += w * (r > g ? (r > b ? r : b) : (g > b ? g : b)); sy += w * lumOf(r, g, b); sw += w;
  }
  level = sl / sw; lum = sy / sw;
}

// ---- what one sequence does, as series -------------------------------------------------------
struct Verdict {
  bool modelled = false, pass = true, smallArea = false;
  int gapLevel = kNever, gapLum = kNever, gapRegion = kNever;   // closest two onsets: whole field by level, by luminance; any region
  double low = 100, lowY = 100;                                 // the whole field's trough, % of its peak (level; luminance)
  std::string note;
  void take(const Count& whole, const Count& wholeLum) {
    if (whole.gap < gapLevel) gapLevel = whole.gap;
    if (wholeLum.gap < gapLum) gapLum = wholeLum.gap;
    if (whole.low < low) low = whole.low;
    if (wholeLum.low < lowY) lowY = wholeLum.low;
  }
  void region(const Count& c) { if (c.gap < gapRegion) gapRegion = c.gap; }
  void settle() { pass = smallArea || (gapLevel >= 340 && gapLum >= 340 && gapRegion >= 340); }
};

// A sequence the palette draws: the whole field moves together (or, for march and the flip
// flops, in two halves that take turns).
static void paletteLook(int code, int speed, const Disp& d, Verdict& v) {
  const int T = 6000;                                  // frames: a minute
  std::vector<double> L, Y, RL, RY;                    // whole field; one half
  double l, y, l2, y2;
  auto steady = [&](int pal) { field(d, pal, 0, d.bri, l, y); L.assign(100, l); Y.assign(100, y); };
  switch (code) {
    case 0: case 12: steady(d.pal); break;             // normal; mic bright restores its settings every frame
    case 1: case 5: case 11: case 13: steady(2); break;   // palette 2 at one hue, whatever the flip flag says
    case 2: {                                          // failure: LE:2014-2044
      int hue = 0;
      for (int f = 0; f < 1900; f++) {
        const int ms = f * FRAME_MS;
        if (ms > 1800 && ms < 6000) hue = (hue + 1) & 255;
        int sc = d.bri;
        if (ms > 6000 && ms < 8000) sc = (2000 - (ms - 6000)) * 255 / 2000;
        else if (ms >= 8000 && ms < 18000) sc = 0;
        if (ms >= 18000) field(d, d.pal, 0, d.bri, l, y);
        else if (sc == 0) l = y = 0;
        else field(d, d.pal, hue, sc, l, y);
        L.push_back(l); Y.push_back(y);
      }
      break;
    }
    case 6: case 4: case 7: case 8: {                  // full and 50/255: the whole field (6), or half of it by turns
      const int flip = (code == 6 ? 250 : code == 4 ? 150 : 200) * (speed + 1) / FRAME_MS;
      field(d, 2, 0, 255, l, y); field(d, 2, 0, 50, l2, y2);
      for (int f = 0; f < T; f++) {
        // march makes its last 3 s faster whatever the speed (175 ms, LE:2004): the tail of the series
        const bool tail = code == 4 && f >= T - 300;
        const bool hi = tail ? (((f - (T - 300)) * FRAME_MS / 175) % 2 == 0) : ((f / flip) % 2 == 0);
        if (code == 6) { L.push_back(hi ? l : l2); Y.push_back(hi ? y : y2); }
        else { L.push_back((l + l2) / 2); Y.push_back((y + y2) / 2); RL.push_back(hi ? l : l2); RY.push_back(hi ? y : y2); }
      }
      break;
    }
    case 9: {                                          // colour swap: the hue jumps half a turn
      const int flip = 350 * (speed + 1) / FRAME_MS;
      field(d, 2, 0, d.bri, l, y); field(d, 2, 128, d.bri, l2, y2);
      for (int f = 0; f < T; f++) { const bool a = (f / flip) % 2 == 0; L.push_back(a ? l : l2); Y.push_back(a ? y : y2); }
      break;
    }
    case 10: {                                         // rainbow: the hue steps by 20
      const int flip = 200 * (speed + 1) / FRAME_MS;
      for (int f = 0; f < T; f++) { field(d, 2, ((f / flip) * 20) & 255, d.bri, l, y); L.push_back(l); Y.push_back(y); }
      break;
    }
    default: return;
  }
  v.modelled = true;
  v.take(onsets(L), onsets(Y));
  if (!RL.empty()) { v.region(onsets(RL)); v.region(onsets(RY)); }
}

// A sequence that draws pixel by pixel: frames of W x H, level and luminance per pixel.
struct Frames {
  int w, h; std::vector<std::vector<float>> L, Y;
  Frames(int W, int H) : w(W), h(H) {}
  void push(const std::vector<float>& l, const std::vector<float>& y) { L.push_back(l); Y.push_back(y); }
};
static void judge(const Frames& fr, Verdict& v) {
  const int w = fr.w, h = fr.h, n = w * h; const size_t F = fr.L.size();
  // a small lit area?
  int most = 0;
  for (const auto& f : fr.L) { int lit = 0; for (float x : f) if (x > 0) lit++; if (lit > most) most = lit; }
  if (most * 4 < n) v.smallArea = true;
  // per frame: the sum of every row, of every column and of the four quarters, for both measures
  for (int m = 0; m < 2; m++) {
    const auto& src = m ? fr.Y : fr.L;
    std::vector<std::vector<double>> row(h, std::vector<double>(F)), col(w, std::vector<double>(F)), quad(4, std::vector<double>(F));
    std::vector<double> whole(F);
    for (size_t f = 0; f < F; f++) {
      const float* p = src[f].data(); double all = 0;
      for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        const double val = p[y * w + x];
        row[y][f] += val; col[x][f] += val; all += val;
        quad[(x >= w / 2 ? 1 : 0) + (y >= h / 2 ? 2 : 0)][f] += val;
      }
      whole[f] = all;
    }
    const Count c = onsets(whole);
    if (m == 0) { if (c.gap < v.gapLevel) v.gapLevel = c.gap; if (c.low < v.low) v.low = c.low; }
    else { if (c.gap < v.gapLum) v.gapLum = c.gap; if (c.low < v.lowY) v.lowY = c.low; }
    // regions of a quarter of the field or more: every band of rows, every band of columns, the four quarters
    std::vector<double> band(F);
    for (int y0 = 0; y0 < h; y0++) {
      std::fill(band.begin(), band.end(), 0.0);
      for (int y1 = y0 + 1; y1 <= h; y1++) {
        for (size_t f = 0; f < F; f++) band[f] += row[y1 - 1][f];
        if ((y1 - y0) * 4 >= h && !(y0 == 0 && y1 == h)) v.region(onsets(band));
      }
    }
    for (int x0 = 0; x0 < w; x0++) {
      std::fill(band.begin(), band.end(), 0.0);
      for (int x1 = x0 + 1; x1 <= w; x1++) {
        for (size_t f = 0; f < F; f++) band[f] += col[x1 - 1][f];
        if ((x1 - x0) * 4 >= w && !(x0 == 0 && x1 == w)) v.region(onsets(band));
      }
    }
    for (int q = 0; q < 4; q++) v.region(onsets(quad[q]));
  }
}
static const unsigned char kFireMask[10][8] = {
  {32,0,0,0,0,0,0,32}, {64,0,0,0,0,0,0,64}, {96,32,0,0,0,0,32,96}, {128,64,32,0,0,32,64,128}, {160,96,64,32,32,64,96,160},
  {160,96,64,32,32,64,96,160}, {192,128,96,64,64,96,128,192}, {255,160,128,96,96,128,160,255}, {255,192,160,128,128,160,192,255},
  {255,192,160,128,128,160,192,255} };
static const unsigned char kFireHue[10][8] = {
  {1,11,19,25,25,22,11,1}, {1,8,13,19,25,19,8,1}, {1,8,13,16,19,16,8,1}, {1,5,11,13,13,13,5,1}, {1,5,11,11,11,11,5,1},
  {1,5,11,11,11,11,5,1}, {0,1,5,8,8,5,1,0}, {0,0,1,5,5,1,0,0}, {0,0,0,1,1,0,0,0}, {0,0,0,1,1,0,0,0} };
static void pixelLook(int code, int speed, int w, int h, Verdict& v) {
  const int n = w * h, T = 6000;
  Frames fr(w, h);
  std::vector<float> L(n, 0), Y(n, 0);
  auto put = [&](int x, int y, int val) { if (x >= 0 && x < w && y >= 0 && y < h) { L[y * w + x] = (float)val; Y[y * w + x] = (float)val; } };
  auto act = [&](int f, int delayMs) { const int d = delayMs < FRAME_MS ? 1 : delayMs / FRAME_MS; return f % d == 0; };
  switch (code) {
    case 19: {                                         // roaming pixel: LE:2404-2441
      int x = 0, y = 0;
      for (int f = 0; f < T; f++) {
        if (act(f, 50 * speed)) {
          put(x, y, 200); x++;
          if (x > w) { for (int i = 0; i < w; i++) put(i, y, 0); x = 0; y++; }
          if (y >= h) { x = 0; y = 0; }
        }
        fr.push(L, Y);
      }
      break;
    }
    case 20: case 21: {                                // the scan lines: LE:2445-2535
      const bool hor = code == 20; const int span = hor ? h : w;
      int p = 0, pp = 0, dir = 0;
      for (int f = 0; f < T; f++) {
        if (act(f, 50 * (speed + 1))) {
          if (pp != p) for (int i = 0; i < (hor ? w : h); i++) hor ? put(i, pp, 0) : put(pp, i, 0);
          for (int i = 0; i < (hor ? w : h); i++) hor ? put(i, p, 200) : put(p, i, 200);
          pp = p; p += dir ? -1 : 1;
          if (p >= span) { dir = 1; p--; } else if (p < 0) { dir = 0; p = 0; }
        }
        fr.push(L, Y);
      }
      break;
    }
    case 22: {                                         // fire: LE:2537-2636. Its blend counter is never set back.
      std::vector<int> cur(n, 0), line(w);
      for (int& x : line) x = 64 + (int)rnd(191);
      long pcnt = 0;
      for (int f = 0; f < T; f++) {
        if (act(f, 100 * (speed + 1))) {
          if (pcnt >= 100) {
            for (int y = h - 1; y > 0; y--) for (int x = 0; x < w; x++) cur[y * w + x] = cur[(y - 1) * w + x];
            for (int x = 0; x < w; x++) cur[x] = line[x];
            for (int& x : line) x = 64 + (int)rnd(191);
          }
          for (int y = h - 1; y > 0; y--) for (int x = 0; x < w; x++) {
            long nv = (long)(((100.0 - pcnt) * cur[y * w + x] + pcnt * cur[(y - 1) * w + x]) / 100.0) - kFireMask[y % 10][x % 8];
            const int val = (int)((nv < 0 ? 0 : nv) & 255);
            int r, g, b; hsv(kFireHue[y % 10][x % 8], 255, val, r, g, b);
            L[(h - 1 - y) * w + x] = (float)(r > g ? (r > b ? r : b) : (g > b ? g : b)); Y[(h - 1 - y) * w + x] = (float)lumOf(r, g, b);
          }
          for (int x = 0; x < w; x++) {
            const int val = (int)((long)(((100.0 - pcnt) * cur[x] + pcnt * line[x]) / 100.0) & 255);
            int r, g, b; hsv(kFireHue[0][x % 8], 255, val, r, g, b);
            L[(h - 1) * w + x] = (float)(r > g ? (r > b ? r : b) : (g > b ? g : b)); Y[(h - 1) * w + x] = (float)lumOf(r, g, b);
          }
          pcnt += 20;
        }
        fr.push(L, Y);
      }
      break;
    }
    case 23: {                                         // PSI colour wipe, default colour: one colour, a column at a time, then still
      int x = 0;
      for (int f = 0; f < 1500; f++) {
        if (x < w && act(f, 50 * (speed + 1))) { for (int y = 0; y < h; y++) put(x, y, 200); x++; }
        fr.push(L, Y);
      }
      break;
    }
    case 24: {                                         // pulse: a box from the centre and back, LE:2655-2700
      int px = 0, dir = 0; const int xm = w / 2, ym = h / 2;
      for (int f = 0; f < T; f++) {
        if (act(f, 70 * (speed + 1))) {
          for (int y = 0; y < h; y++) for (int x = 0; x < w; x++)
            put(x, y, ((y + px >= ym && y - px <= ym) && (x + px >= xm && x - px <= xm)) ? 150 : 0);
          px += dir ? -1 : 1;
          if (px > w / 2 || px < 1) dir = !dir;
        }
        fr.push(L, Y);
      }
      break;
    }
    case 101: {                                        // plasma: effects/PlasmaEffect.h
      static unsigned char lut[1537][3]; static bool made = false;
      if (!made) {
        made = true;
        for (int i = 0; i < 256; i++) {
          lut[i][0] = 255; lut[i][1] = (unsigned char)i; lut[i][2] = 0;
          lut[i + 256][0] = (unsigned char)(255 - i); lut[i + 256][1] = 255; lut[i + 256][2] = 0;
          lut[i + 512][0] = 0; lut[i + 512][1] = 255; lut[i + 512][2] = (unsigned char)i;
          lut[i + 768][0] = 0; lut[i + 768][1] = (unsigned char)(255 - i); lut[i + 768][2] = 255;
          lut[i + 1024][0] = (unsigned char)i; lut[i + 1024][1] = 0; lut[i + 1024][2] = 255;
          lut[i + 1280][0] = 255; lut[i + 1280][1] = 0; lut[i + 1280][2] = (unsigned char)(255 - i);
        }
        lut[1536][0] = 255; lut[1536][1] = 0; lut[1536][2] = 0;
      }
      double counter = 0;
      for (int f = 0; f < 12000; f++) {
        counter += 0.1;
        const double c1 = sin(counter * 0.006), c2 = sin(counter * -0.06);
        double xc = 25.0;
        for (int x = 0; x < w; x++) {
          xc += 0.3; double yc = 25.0; const double s1 = 768.0 + 768.0 * sin(xc) * c1;
          for (int y = 0; y < h; y++) {
            yc += 0.3;
            const double s2 = 768.0 + 768.0 * sin(yc) * c2, s3 = 768.0 + 768.0 * sin((xc + yc + counter / 10.0) / 2.0);
            const unsigned char* c = lut[(int)((s1 + s2 + s3) / 3.0)];
            L[y * w + x] = (float)(c[0] > c[1] ? (c[0] > c[2] ? c[0] : c[2]) : (c[1] > c[2] ? c[1] : c[2])); Y[y * w + x] = (float)lumOf(c[0], c[1], c[2]);
          }
        }
        if (f % 3 == 0) fr.push(L, Y);                 // (every third frame: it moves slowly, and the count below scales the times)
      }
      break;
    }
    case 102: {                                        // metaballs: effects/MeatBallsEffect.h
      const int dia = 1 + (int)rnd(24), nb = 1 + (int)rnd(4), sp = 1 + (int)rnd(4);
      int bx[4], by[4], dx[4], dy[4]; int rs = 255, gs = 0, bs = 255, cnt = 0;
      for (int i = 0; i < nb; i++) { bx[i] = (int)rnd((uint32_t)w); by[i] = (int)rnd((uint32_t)h); dx[i] = rnd(2) ? 1 : -1; dy[i] = rnd(2) ? 1 : -1; }
      for (int f = 0; f < 3000; f++) {
        if (cnt > 25) { rs = (int)rnd(127); gs = (int)rnd(127); bs = (int)rnd(127); cnt = 0; } else cnt++;
        for (int i = 0; i < nb; i++) {
          bx[i] += sp * dx[i]; by[i] += sp * dy[i];
          if (bx[i] < 0) dx[i] = 1; else if (bx[i] > w) dx[i] = -1;
          if (by[i] < 0) dy[i] = 1; else if (by[i] > h) dy[i] = -1;
        }
        for (int x = 0; x < w; x++) for (int y = 0; y < h; y++) {
          int R = 0, G = 0, B = 0;
          for (int i = 0; i < nb; i++) { const double d = (bx[i] - x) * (bx[i] - x) + (by[i] - y) * (by[i] - y) + 1; R += (int)(dia / d * rs); G += (int)(dia / d * gs); B += (int)(dia / d * bs); }
          R = (R > 255 ? 255 : R) / 2; G = (G > 255 ? 255 : G) / 2; B = (B > 255 ? 255 : B) / 2;
          L[y * w + x] = (float)(R > G ? (R > B ? R : B) : (G > B ? G : B)); Y[y * w + x] = (float)lumOf(R, G, B);
        }
        fr.push(L, Y);
      }
      break;
    }
    case 103: {                                        // fractal: effects/FractalEffect.h (three colours that eat each other)
      std::vector<int> col(n, 0), lev(n, 0); const int grow = 1 + (int)rnd(3), dist = 10 + (int)rnd(30);
      for (int x = 0; x < w; x++) { col[x + (h / 3) * w] = 1; col[x + (h / 3 * 2) * w] = 3; }
      for (int y = 0; y < h; y++) { col[w / 3 + y * w] = 2; col[w / 3 * 2 + y * w] = 3; }
      static const int beats[4] = { 0, 2, 3, 1 }, loses[4] = { 0, 3, 1, 2 };
      for (int f = 0; f < 3000; f++) {
        for (int x = 0; x < w; x++) for (int y = 0; y < h; y++) {
          int px = 0, py = 0;
          while (px == 0 && py == 0) { px = (int)rnd(3); py = (int)rnd(3); }
          px = x - 1 + px; py = y - 1 + py;
          if (px < 0) px = w - 1; if (py < 0) py = h - 1; if (px > w - 1) px = 0; if (py > h - 1) py = 0;
          const int a = x + y * w, b = px + py * w;
          if (col[a] == 0 && col[b] != 0 && lev[b] < grow) { col[a] = col[b]; lev[a] = lev[b] + 1; }
          else if (col[a]) {
            if (col[b] == loses[col[a]]) { lev[a]--; lev[b]++; }
            if (col[b] == beats[col[a]]) { lev[a]++; lev[b]--; }
          }
          if (lev[a] < 0) lev[a] = 0;
          if (lev[a] > grow) { col[a] = col[b]; lev[a] = 0; }
        }
        for (int i = 0; i < n; i++) {
          if ((int)rnd(10000) < dist) col[i] = (int)rnd(3) + 1;
          L[i] = col[i] ? 255.f : 0.f;
          Y[i] = col[i] == 1 ? (float)lumOf(255, 0, 0) : col[i] == 2 ? (float)lumOf(0, 255, 0) : col[i] == 3 ? (float)lumOf(0, 0, 255) : 0.f;
        }
        fr.push(L, Y);
      }
      break;
    }
    case 104: {                                        // fade and scroll, its worst start: one colour, every pixel in phase, the given speed
      int hgt = 0; const int sp = 1 + speed;           // (here `speed` is the effect's own random 1..10, less one)
      for (int f = 0; f < 3000; f++) {
        hgt += sp; if (hgt > 511) hgt = 0;
        const float val = (float)(hgt < 256 ? hgt : 511 - hgt);
        for (int i = 0; i < n; i++) { L[i] = val; Y[i] = val; }
        fr.push(L, Y);
      }
      break;
    }
    default: return;
  }
  v.modelled = true;
  Verdict one; judge(fr, one);
  const int k = code == 101 ? 3 : 1;                   // plasma kept every third frame
  if (one.gapLevel < kNever) one.gapLevel *= k;
  if (one.gapLum < kNever) one.gapLum *= k;
  if (one.gapRegion < kNever) one.gapRegion *= k;
  // a display where the look is a small lit area is not counted; the others decide
  if (one.smallArea) { if (!v.note.empty()) v.note += ", "; v.note += std::to_string(w) + "x" + std::to_string(h) + " small area"; return; }
  if (one.gapLevel < v.gapLevel) v.gapLevel = one.gapLevel;
  if (one.gapLum < v.gapLum) v.gapLum = one.gapLum;
  if (one.gapRegion < v.gapRegion) v.gapRegion = one.gapRegion;
  if (one.low < v.low) v.low = one.low;
  if (one.lowY < v.lowY) v.lowY = one.lowY;
}

static const int kSizes[5][2] = { { 8, 10 }, { 16, 4 }, { 24, 4 }, { 28, 4 }, { 32, 8 } };
static Verdict classify(int code, int speed) {
  Verdict v;
  g_rng = 0x51F15EEDu + (uint32_t)code * 977u;
  if (code <= 13) { paletteLook(code, speed, kFront, v); paletteLook(code, speed, kRear, v); }
  else if (code == 14) { v.modelled = true; v.note = "dark"; }
  else if (code == 105) { v.modelled = true; v.note = "every LED fades by itself between four colours: texture"; }
  else {
    const int runs = (code == 22 || code == 102 || code == 103) ? 4 : 1;
    for (int r = 0; r < runs; r++) for (const auto& s : kSizes) pixelLook(code, speed, s[0], s[1], v);
    if (v.modelled && v.gapLevel == kNever && v.gapLum == kNever && v.gapRegion == kNever && !v.note.empty()) v.smallArea = true;
  }
  v.settle();
  return v;
}
static std::string gapText(int g) { return g >= kNever ? std::string("none") : std::to_string(g) + " ms"; }

int main() {
  static const char* const kName[] = { "normal", "alarm", "failure", "Leia", "march", "solid colour", "flash colour", "flip flop", "flip flop alt",
    "colour swap", "rainbow", "red alert", "mic bright", "mic rainbow", "lights out" };
  auto name = [&](int c) -> std::string {
    if (c <= 14) return kName[c];
    switch (c) { case 19: return "roaming pixel"; case 20: return "horizontal scan line"; case 21: return "vertical scan line"; case 22: return "fire";
                 case 23: return "PSI colour wipe"; case 24: return "pulse"; case 101: return "plasma"; case 102: return "metaballs";
                 case 103: return "fractal"; case 104: return "fade and scroll"; case 105: return "colour wheel"; default: return "?"; }
  };
  printf("S10, the logics' native sequences (a model of the stock code; closest two onsets, whole field by level / by luminance / any region of a quarter or more)\n");
  printf("%-4s %-22s %-6s %-5s %-9s %-9s %-9s %s\n", "code", "sequence", "list", "speed", "level", "luminance", "region", "");
  static const int kCodes[] = { 0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 19, 20, 21, 22, 23, 24, 101, 102, 103, 104, 105 };
  for (int code : kCodes) {
    const bool listed = nativeAllowedLogics(code);
    const int speed = nativeSpeedLogics(code);
    // the lowest speed that passes, by the model
    int lowest = -1; Verdict at;
    for (int s = 0; s <= 9 && lowest < 0; s++) { const Verdict v = classify(code, s); if (s == speed || (!listed && s == 0)) at = v; if (v.pass) lowest = s; }
    if (code == 104) at = classify(104, 9);            // its own fastest speed, which nothing sets: it is random at every start
    CHECK(at.modelled);
    if (getenv("S10_LOWEST")) printf("     (lowest speed that passes: %d)\n", lowest);
    printf("%-4d %-22s %-6s %-5d %-9s %-9s %-9s %s%s\n", code, name(code).c_str(), listed ? "on" : "off", listed ? speed : 0,
           gapText(at.gapLevel).c_str(), gapText(at.gapLum).c_str(), gapText(at.gapRegion).c_str(),
           at.smallArea ? "a small lit area on every display" : at.pass ? "passes" : "does not pass", at.note.empty() ? "" : (" (" + at.note + ")").c_str());
    if (listed) {
      CHECK(at.pass);                                  // every code on the list passes at the speed the core gives it...
      CHECK(lowest == speed);                          // ...and that is the fastest speed that passes (S10)
      // "steady": the whole field stays within a tenth of what the gate was told. Only a sequence
      // that draws at the board's brightness can be (the others draw at a level of their own).
      CHECK(nativeSteadyLogics(code) == (code == 14 || (nativeDimsLogics(code) && at.low >= 90.0 && at.lowY >= 90.0)));
    } else CHECK(!nativeSteadyLogics(code));
    if (code == 102 || code == 103) CHECK(!listed && !at.pass);   // off the list because they fail
    if (code == 104) CHECK(!listed);                   // off the list: what it does is drawn at random when it starts
  }
  // what the model does not cover is not on the list for other reasons
  CHECK(!nativeAllowedLogics(3) && !nativeAllowedLogics(15) && !nativeAllowedLogics(18) && !nativeAllowedLogics(25));
  // the speed is a digit the engine takes, and only listed codes have one
  for (int c = -5; c < 300; c++) {
    CHECK(nativeSpeedLogics(c) <= 9);
    if (!nativeAllowedLogics(c)) CHECK(nativeSpeedLogics(c) == 0 && !nativeSteadyLogics(c));
  }
  if (g_fail) { printf("FAILURES  %d/%d checks passed\n", g_checks - g_fail, g_checks); return 1; }
  printf("OK  %d/%d checks passed\n", g_checks, g_checks);
  return 0;
}
