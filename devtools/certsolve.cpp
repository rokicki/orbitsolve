// certsolve: solve every piece of an N x N x N cube from the certificates.  It began as obsolve, which
// solves every oblique centre orbit of an N x N x N cube, pair by pair, from the stage
// certificates alone ("the proof is in the pudding").  It reads only the small certificate
// (smallcert.py: per stage the lines not one stage move from a shorter class) and rebuilds the full
// stage tables in memory at startup: every state of each stage gets its distance by a breadth-first
// pass over the stage moves from the kept lines' states (and their stage-symmetric images), and its
// line is the kept line or a stage move followed by the line of a state one closer.
//
// The cube is brobdicube's xcube (brob/, unmodified); centre orbit (x,y) is
// rows[y-1][x-1].  A certificate line acts on xorbits (1,2)/(2,1) with layer depths 1..3; for the
// chiral pair (a,b)/(b,a) its depths 2 and 3 become a+1 and b+1.  Order: 1ud then 2ud
// for every pair (other obliques keep their U and D faces), then 3ax then 4op for every pair (other
// obliques keep their colours).
//
// Default: every stage plays its table line (the plain sum, P <= 80).  --bound: at each boundary
// (1ud/2ud and 3ax/4op) the first stage's solution is chosen as the boundary finders choose theirs
// (boundx, axbx): the table line, its stage-symmetric copies, an action (outer turn or a pool
// algorithm image) followed by the table line of where it leads, within --slack of the line, and if
// that is not enough two actions; the one with the least |P| + d(second stage) is played.  The bound
// it must meet is --ud X (default 33) and --ax X (default 39).
//
// Checks: the layout and depth substitution are tested on the given N at startup (random sequences
// on the cube against the pair-facelet simulation); after every stage the pair is re-read from the
// cube and must be at that stage's goal; after phase 1 every pair has U and D home, at the end every
// oblique orbit is solved; a chosen alternative must keep the other obliques as its stage requires.
//
// Command line as gullicube's (certsolve with no arguments prints it): certsolve N [options]; the
// scramble from -r/-R/-i/-f (default a random position), -o writes the solution with one line per set
// solved at once (a pair's phase, an orbit, the corners), every line of moves starting with a space;
// --verify replays it on an independent sticker simulator (sim/cube.h, written for gullicube).
#include "certcube.h"
#include "xcube.h"
#include "facelets.h"
#include "cube.h"  // sim/cube.h: the independent sticker simulator (--verify)
#include <iostream>
#if defined(__APPLE__)
#include <libproc.h>
#include <unistd.h>
#endif
#include <algorithm>
#include <atomic>
#include <climits>
#include <chrono>
#include <cstdint>
#include <dirent.h>
#include <random>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <map>

// globals brobdicube's xcube.cpp expects (as gullicube's brobglue.cpp)
int alwaysinmem = 1;
ll cancellations = 0;
ll runningmovecount = 0;
int writemoves = 0;
std::vector<mv> curblock;
ll wingmoves = 0;
ll blocks = 0;
void sendblock() { curblock.clear(); }
double duration() { return 0; }

static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static inline uint8_t mvByte(const cc::Mv &m) { return m.face << 4 | m.depth << 2 | m.amt; }
static inline cc::Mv byteMv(uint8_t b) { return {b >> 4, b >> 2 & 3, b & 3}; }
static std::string text(const std::vector<cc::Mv> &mv) {
  std::string s;
  for (auto &m : mv) { if (!s.empty()) s += ' '; s += cc::mvText(m); }
  return s;
}

// ---- pair geometry on 8x8x8 (certificate frame): k = face * 8 + orbit * 4 + i
static int mp[6][4][4][48];  // dst form: the piece at k goes to mp[f][d][a][k]
static int faceOfK(int k) { return k / 8; }
static int orbitOfK(int k) { return (k % 8) / 4; }
static int groupOf(int k) { return orbitOfK(k) * 6 + faceOfK(k); }
static const int axisOf[6] = {0, 1, 2, 1, 2, 0};
static bool side(int f) { return axisOf[f] != 0; }
struct GSym { int fac[48], gmap[12], face[6]; bool flip; };
static std::vector<GSym> gsyms;  // the 48 cube symmetries
static std::vector<cc::Mv> symMoves(const GSym &y, const std::vector<cc::Mv> &mv) {
  std::vector<cc::Mv> r;
  for (auto &m : mv) r.push_back({y.face[m.face], m.depth, y.flip ? 4 - m.amt : m.amt});
  return r;
}
static void symG(const GSym &y, const int8_t *a, int8_t *b) { for (int k = 0; k < 48; k++) b[y.fac[k]] = y.gmap[a[k]]; }
static void applyG(const cc::Mv &m, const int8_t *a, int8_t *b) { for (int k = 0; k < 48; k++) b[mp[m.face][m.depth][m.amt][k]] = a[k]; }
static void applyG(const std::vector<cc::Mv> &mv, int8_t *a) {
  int8_t b[48];
  for (auto &m : mv) { applyG(m, a, b); memcpy(a, b, 48); }
}
static void mapOf(const std::vector<cc::Mv> &mv, int8_t *D) {
  for (int k = 0; k < 48; k++) {
    int x = k;
    for (auto &m : mv) x = mp[m.face][m.depth][m.amt][x];
    D[k] = x;
  }
}
// the position a line solves: the goal run backwards (from fully solved groups)
static void positionOf(const std::vector<cc::Mv> &mv, int8_t *g) {
  int8_t b[48];
  for (int k = 0; k < 48; k++) g[k] = groupOf(k);
  for (auto it = mv.rbegin(); it != mv.rend(); ++it) {
    for (int k = 0; k < 48; k++) b[mp[it->face][it->depth][4 - it->amt][k]] = g[k];
    memcpy(g, b, 48);
  }
}
// merges (axbx): one layer's turns across turns that commute with them on every non-diagonal centre
// orbit (same axis; outer with outer; inner slices of one depth)
static void simplify(std::vector<cc::Mv> &v) {
  std::vector<cc::Mv> o;
  for (auto m : v) {
    int i = (int)o.size() - 1;
    bool merged = false;
    for (; i >= 0; i--) {
      if (o[i].face == m.face && o[i].depth == m.depth) {
        const int a = (o[i].amt + m.amt) % 4;
        if (a) o[i].amt = a;
        else o.erase(o.begin() + i);
        merged = true;
        break;
      }
      if (!(axisOf[o[i].face] == axisOf[m.face] || o[i].depth == m.depth)) break;
    }
    if (!merged) o.push_back(m);
  }
  v.swap(o);
}

// the other oblique orbits' facelets on 8x8x8 (orbits (2,4), (4,2), (3,4), (4,3), 1-based depths):
// representatives of every oblique orbit a stage line can move.  keepOthers: 1ud/2ud keep their U,
// ring and D sets; 3ax/4op keep their faces.
static std::vector<int> othFac;
static int c8perm[6][4][4][384];
static int c8faceOf[384];
static bool keepOthers(bool ud, const std::vector<cc::Mv> &mv) {
  for (int i : othFac) {
    int x = i;
    for (auto &m : mv) x = c8perm[m.face][m.depth][m.amt][x];
    const int f0 = c8faceOf[i], f1 = c8faceOf[x];
    if (ud ? ((f0 == 0) != (f1 == 0) || (f0 == 5) != (f1 == 5)) : f0 != f1) return false;
  }
  return true;
}

// ---- ranking helpers
static uint64_t binom[49][49];
static int16_t rk16[65536], rk8[256];
static uint16_t ur16[12870];
static uint8_t ur8[70];
static int slots[32], slotOf[48];
static int bitOf(int st, int g, int k) {  // 3ax/4op key bit of group g in side slot k; -1: not a start
  if (g / 6 != orbitOfK(k)) return -1;
  const int f = g % 6, fk = faceOfK(k);
  if (!side(f)) return -1;
  if (st == 0) return axisOf[f] == 2;
  if (axisOf[f] != axisOf[fk]) return -1;
  return f == 2 || f == 1;
}
static uint32_t keyOf(int st, const int8_t *g) {
  uint32_t x = 0;
  for (int k = 0; k < 48; k++) {
    if (!side(faceOfK(k))) { if (g[k] != groupOf(k)) return 0xFFFFFFFFu; continue; }
    const int b = bitOf(st, g[k], k);
    if (b < 0) return 0xFFFFFFFFu;
    x |= (uint32_t)b << slotOf[k];
  }
  return x;
}
static void colOf(int st, uint32_t x, int8_t *g) {
  for (int k = 0; k < 48; k++) {
    if (!side(faceOfK(k))) { g[k] = groupOf(k); continue; }
    const int b = x >> slotOf[k] & 1, o = orbitOfK(k), ax = axisOf[faceOfK(k)];
    if (st == 0) g[k] = o * 6 + (b ? 2 : 1);
    else g[k] = o * 6 + (ax == 2 ? (b ? 2 : 4) : (b ? 1 : 3));
  }
}
static uint32_t orbBits(uint32_t x, int o) {
  uint32_t r = 0;
  for (int fi = 0; fi < 4; fi++) r |= (x >> (fi * 8 + o * 4) & 15) << (fi * 4);
  return r;
}
static uint32_t axBits(uint32_t x, int o, int ax) {
  const int f1 = ax == 2 ? 1 : 0, f2 = ax == 2 ? 3 : 2;
  return (x >> (f1 * 8 + o * 4) & 15) | (x >> (f2 * 8 + o * 4) & 15) << 4;
}

#include "certcache.h"

// ---- a stage: dense table over every state, rebuilt from the small certificate
struct Stage {
  int kind = 0;  // 0 1ud, 1 2ud, 2 3ax, 3 4op
  std::string name;
  uint64_t total = 0;
  std::vector<uint8_t> dist;
  std::vector<uint32_t> src;  // kept line (low 26 bits) and symmetry (high 6) of a state's kept line, or ~0
  std::vector<uint64_t> lineOff;
  std::vector<uint8_t> lineBytes;
  std::vector<int> ss;  // stage symmetries (indices into gsyms)
  std::vector<cc::Mv> moves;  // the stage moves: keep the other obliques as the stage requires
  // actions for the rebuild: the stage moves (cost 1) and, in a concise stage, the lines of actions.txt
  // under the stage symmetries and inverted; D: the piece at k goes to D[k]
  struct SAct { int8_t D[48]; std::vector<cc::Mv> mv; };
  std::vector<SAct> acts;
  size_t nActLines = 0;
  void addAct(const std::vector<cc::Mv> &mv, std::unordered_map<std::string, size_t> &seen) {
    SAct a;
    mapOf(mv, a.D);
    bool id = true;
    for (int k = 0; k < 48; k++) id &= a.D[k] == k;
    if (id) return;
    a.mv = mv;
    const std::string key((const char *)a.D, 48);
    auto it = seen.find(key);
    if (it == seen.end()) { seen[key] = acts.size(); acts.push_back(a); }
    else if (acts[it->second].mv.size() > mv.size()) acts[it->second] = a;
  }
  int W = 0;
  // 1ud / 2ud: S = solved groups (2ud: aU, bU), TG = target groups; per orbit the free positions
  int S = 0, TG = 0;
  int tg[4], ntg = 0, cidx[12];
  int rel[2][24], nrel[2] = {0, 0};
  bool isS[48];
  uint64_t Mo[2] = {1, 1};
  bool ud() const { return kind < 2; }

  void init(int k) {
    kind = k;
    name = k == 0 ? "1ud" : k == 1 ? "2ud" : k == 2 ? "3ax" : "4op";
    if (ud()) {
      S = kind == 0 ? 0 : (1 << 0 | 1 << 6);
      TG = kind == 0 ? (1 << 0 | 1 << 6) : (1 << 5 | 1 << 11);
      for (int g = 0; g < 12; g++) { cidx[g] = 0; if (TG >> g & 1) tg[ntg] = g, cidx[g] = ++ntg; }
      for (int q = 0; q < 48; q++) {
        isS[q] = S >> groupOf(q) & 1;
        if (!isS[q]) rel[orbitOfK(q)][nrel[orbitOfK(q)]++] = q;
      }
      for (int o = 0; o < 2; o++) Mo[o] = binom[nrel[o]][4];
      total = Mo[0] * Mo[1];
      for (size_t s = 0; s < gsyms.size(); s++) {
        int s2 = 0, t2 = 0;
        for (int g = 0; g < 12; g++) { if (S >> g & 1) s2 |= 1 << gsyms[s].gmap[g]; if (TG >> g & 1) t2 |= 1 << gsyms[s].gmap[g]; }
        if (s2 == S && t2 == TG) ss.push_back(s);
      }
    } else {
      total = kind == 2 ? 12870ull * 12870 : 70ull * 70 * 70 * 70;
      for (size_t s = 0; s < gsyms.size(); s++) {
        bool keeps = true;
        for (int g = 0; g < 12; g++) if (side(g % 6) != side(gsyms[s].gmap[g] % 6)) keeps = false;
        if (keeps) ss.push_back(s);
      }
    }
    // stage moves, one per distinct effect on the pair
    std::unordered_set<std::string> seen;
    for (int f = 0; f < 6; f++)
      for (int d = 1; d <= 3; d++)
        for (int a = 1; a <= 3; a++) {
          if (!keepOthers(ud(), {{f, d, a}})) continue;
          int8_t D[48];
          mapOf({{f, d, a}}, D);
          if (seen.insert(std::string((const char *)D, 48)).second) moves.push_back({f, d, a});
        }
  }
  // full groups -> rank; false if not a start of this stage
  bool rankG(const int8_t *g, uint64_t &r) const {
    if (ud()) {
      r = 0;
      for (int o = 0; o < 2; o++) {
        // colex rank of the 4 target-colour positions among rel[o] (target group cidx o+1)
        uint64_t ro = 0;
        int j = 0;
        for (int i = 0; i < nrel[o]; i++) {
          const int q = rel[o][i], c = cidx[g[q]];
          if (c) { if (c != o + 1) return false; j++; ro += binom[i][j]; }
        }
        if (j != 4) return false;
        r = r * Mo[o] + ro;
      }
      for (int q = 0; q < 48; q++) if (isS[q] && g[q] != groupOf(q)) return false;
      return true;
    }
    const uint32_t x = keyOf(kind - 2, g);
    if (x == 0xFFFFFFFFu) return false;
    if (kind == 2) {
      const int a = rk16[orbBits(x, 0)], b = rk16[orbBits(x, 1)];
      if (a < 0 || b < 0) return false;
      r = (uint64_t)a * 12870 + b;
    } else {
      r = 0;
      for (int o = 0; o < 2; o++)
        for (int ax : {2, 1}) {
          const int v = rk8[axBits(x, o, ax)];
          if (v < 0) return false;
          r = r * 70 + v;
        }
    }
    return true;
  }
  // a representative colouring of rank r
  void unrankG(uint64_t r, int8_t *g) const {
    if (ud()) {
      uint64_t ro[2];
      ro[1] = r % Mo[1], ro[0] = r / Mo[1];
      for (int q = 0; q < 48; q++) g[q] = isS[q] ? groupOf(q) : orbitOfK(q) * 6 + 2;  // filler: an F piece
      for (int o = 0; o < 2; o++) {
        uint64_t rest = ro[o];
        int p = nrel[o];
        for (int t = 4; t >= 1; t--) {
          p--;
          while (binom[p][t] > rest) p--;
          rest -= binom[p][t];
          g[rel[o][p]] = tg[o];
        }
      }
      return;
    }
    uint32_t x = 0;
    if (kind == 2) {
      const uint32_t A = ur16[r / 12870], B = ur16[r % 12870];
      for (int o = 0; o < 2; o++) {
        const uint32_t v = o ? B : A;
        for (int fi = 0; fi < 4; fi++) x |= (v >> (fi * 4) & 15) << (fi * 8 + o * 4);
      }
    } else {
      int v[4];
      for (int i = 3; i >= 0; i--) v[i] = r % 70, r /= 70;
      int i = 0;
      for (int o = 0; o < 2; o++)
        for (int ax : {2, 1}) {
          const uint32_t b = ur8[v[i++]];
          const int f1 = ax == 2 ? 1 : 0, f2 = ax == 2 ? 3 : 2;
          x |= (b & 15) << (f1 * 8 + o * 4) | (b >> 4 & 15) << (f2 * 8 + o * 4);
        }
    }
    colOf(kind - 2, x, g);
  }
  std::vector<cc::Mv> lineAt(size_t i) const {
    std::vector<cc::Mv> mv;
    for (uint64_t j = lineOff[i]; j < lineOff[i + 1]; j++) mv.push_back(byteMv(lineBytes[j]));
    return mv;
  }
  // the line of rank r: a kept line, or a stage move to a state one closer, repeatedly
  std::vector<cc::Mv> lineOf(uint64_t r) const {
    std::vector<cc::Mv> out;
    int8_t g[48], h[48];
    unrankG(r, g);
    for (;;) {
      const int d = dist[r];
      if (d == 0) return out;
      if (src[r] != 0xFFFFFFFFu && (int)(lineOff[(src[r] & 0x3FFFFFF) + 1] - lineOff[src[r] & 0x3FFFFFF]) == d) {
        const auto l = symMoves(gsyms[src[r] >> 26], lineAt(src[r] & 0x3FFFFFF));
        out.insert(out.end(), l.begin(), l.end());
        return out;
      }
      bool found = false;
      for (auto &a : acts) {
        const int c = a.mv.size();
        if (c > d) continue;
        for (int k = 0; k < 48; k++) h[a.D[k]] = g[k];
        uint64_t r2;
        if (rankG(h, r2) && dist[r2] == d - c) { out.insert(out.end(), a.mv.begin(), a.mv.end()); memcpy(g, h, 48); r = r2; found = true; break; }
      }
      if (!found) { fprintf(stderr, "%s: no action to a closer state from a state at %d\n", name.c_str(), d); exit(1); }
    }
  }
  bool load(const std::string &dir, int threads) {
    const double t0 = now();
    std::vector<std::string> fs;
    if (DIR *dd = opendir(dir.c_str())) {
      while (dirent *e = readdir(dd)) {
        std::string n = e->d_name;
        if (n.size() > 4 && n.compare(n.size() - 4, 4, ".txt") == 0) fs.push_back(dir + "/" + n);
      }
      closedir(dd);
    }
    if (fs.empty()) { fprintf(stderr, "%s: no line files\n", name.c_str()); return false; }
    std::sort(fs.begin(), fs.end());
    // actions.txt (a concise stage) first: its lines are both lines and actions
    std::stable_partition(fs.begin(), fs.end(), [](const std::string &f) { return f.size() >= 12 && f.compare(f.size() - 12, 12, "/actions.txt") == 0; });
    const bool concise = fs[0].size() >= 12 && fs[0].compare(fs[0].size() - 12, 12, "/actions.txt") == 0;
    for (auto &f : fs) {
      FILE *in = fopen(f.c_str(), "r");
      if (!in) { fprintf(stderr, "%s: cannot open a line file\n", name.c_str()); return false; }
      const bool isAct = concise && &f == &fs[0];
      char buf[4096];
      std::vector<cc::Mv> mv;
      while (fgets(buf, sizeof buf, in)) {
        std::string l(buf);
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
        if (!cc::parseMoves(l, mv)) { fprintf(stderr, "%s: bad line\n", name.c_str()); return false; }
        for (auto &m : mv) if (m.depth > 3) { fprintf(stderr, "%s: bad move depth\n", name.c_str()); return false; }
        lineOff.push_back(lineBytes.size());
        for (auto &m : mv) lineBytes.push_back(mvByte(m));
        if (isAct) nActLines++;
      }
      fclose(in);
    }
    acts.clear();
    {
      std::unordered_map<std::string, size_t> seen;
      for (auto &m : moves) addAct({m}, seen);
      for (size_t i = 0; i < nActLines; i++) {
        lineOff.push_back(lineBytes.size());  // lineAt needs the end offset of i
        const auto mv = lineAt(i);
        lineOff.pop_back();
        for (int s2 : ss) {
          const auto m2 = symMoves(gsyms[s2], mv);
          addAct(m2, seen);
          std::vector<cc::Mv> inv;
          for (auto it = m2.rbegin(); it != m2.rend(); ++it) inv.push_back({it->face, it->depth, 4 - it->amt});
          addAct(inv, seen);
        }
      }
    }
    lineOff.push_back(lineBytes.size());
    const size_t NL = lineOff.size() - 1;
    if (NL >= (1u << 26)) { fprintf(stderr, "%s: too many lines\n", name.c_str()); return false; }
    dist.assign(total, 0xFF), src.assign(total, 0xFFFFFFFFu);
    uint64_t fp = fnv(lineBytes.data(), lineBytes.size(), fnv(lineOff.data(), lineOff.size() * 8, fnv(&total, 8)));
    fp = fnv(&nActLines, sizeof nActLines, fp);
    const std::string cpath = cachePath("obl-" + name, fp);
    const bool cached = readCache(cpath, {{dist.data(), total}, {src.data(), total * 4}});
    long kept = 0;
    std::vector<std::thread> th;
    if (!cached) {
    std::atomic<long> bad{0}, clash{0};
    {
      std::atomic<size_t> ni{0};
      for (int t = 0; t < threads; t++)
        th.emplace_back([&] {
          int8_t g[48], h[48];
          for (size_t i; (i = ni++) < NL;) {
            const auto mv = lineAt(i);
            positionOf(mv, g);
            for (int s : ss) {
              symG(gsyms[s], g, h);
              uint64_t r;
              if (!rankG(h, r)) { bad++; continue; }
              // images of one line may coincide; two lines on one state would be a certificate error
              const uint32_t v = (uint32_t)(i | (size_t)s << 26), old = __atomic_exchange_n(&src[r], v, __ATOMIC_RELAXED);
              if (old != 0xFFFFFFFFu && (old & 0x3FFFFFF) != i) clash++;
              dist[r] = mv.size();
            }
          }
        });
      for (auto &x : th) x.join();
      th.clear();
    }
    if (bad || clash) { fprintf(stderr, "%s: %ld line images not a stage start, %ld states with two lines\n", name.c_str(), bad.load(), clash.load()); return false; }
    for (uint64_t r = 0; r < total; r++) if (dist[r] != 0xFF) kept++;
    // level by level over the actions: a state that an action of c moves takes to a state at d is at
    // most d + c (the stage moves are actions of one move)
    int maxLine = 0;
    for (size_t i = 0; i + 1 < lineOff.size(); i++) maxLine = std::max<int>(maxLine, lineOff[i + 1] - lineOff[i]);
    for (int d = 0;; d++) {
      std::atomic<uint64_t> nr{0};
      std::atomic<long> atD{0};
      const uint64_t CH = 1 << 16;
      for (int t = 0; t < threads; t++)
        th.emplace_back([&] {
          int8_t g[48], h[48];
          long n = 0;
          for (uint64_t lo; (lo = nr.fetch_add(CH)) < total;)
            for (uint64_t r = lo; r < std::min(total, lo + CH); r++) {
              if (dist[r] != d) continue;
              n++;
              unrankG(r, g);
              for (auto &a : acts) {
                for (int k = 0; k < 48; k++) h[k] = g[a.D[k]];  // the state a takes to g
                uint64_t r2;
                if (!rankG(h, r2)) continue;
                const int nd = d + a.mv.size();
                uint8_t old = __atomic_load_n(&dist[r2], __ATOMIC_RELAXED);
                while (old > nd && !__atomic_compare_exchange_n(&dist[r2], &old, (uint8_t)nd, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {}
              }
            }
          atD += n;
        });
      for (auto &x : th) x.join();
      th.clear();
      if (d >= maxLine) {
        bool more = false;
        for (uint64_t r = 0; r < total && !more; r++) if (dist[r] != 0xFF && dist[r] > d) more = true;
        if (!more) break;
      }
    }
    writeCache(cpath, {{dist.data(), total}, {src.data(), total * 4}});
    }
    long un = 0, hist[64] = {};
    for (uint64_t r = 0; r < total; r++) {
      if (dist[r] == 0xFF) { un++; continue; }
      W = std::max<int>(W, dist[r]);
      hist[std::min<int>(dist[r], 63)]++;
    }
    fprintf(stderr, "%s: %zu kept lines (%ld states), %zu stage moves, %zu actions, %zu symmetries; %s %lu states, worst %d (%.0fs):",
            name.c_str(), NL, kept, moves.size(), acts.size(), ss.size(), cached ? "read from the cache:" : "rebuilt", (unsigned long)total, W, now() - t0);
    for (int d = 0; d < 64; d++) if (hist[d]) fprintf(stderr, " %d:%ld", d, hist[d]);
    fprintf(stderr, "\n");
    if (un) { fprintf(stderr, "%s: %ld states not reached\n", name.c_str(), un); return false; }
    // self-check: rebuilt lines solve their states, at their lengths
    long badl = 0;
    std::mt19937_64 rr(12345);
    for (int i = 0; i < 2000; i++) {
      const uint64_t r = rr() % total;
      int8_t g[48];
      unrankG(r, g);
      const auto l = lineOf(r);
      applyG(l, g);
      uint64_t r2;
      if ((int)l.size() != dist[r] || !rankG(g, r2) || dist[r2] != 0) badl++;
    }
    if (badl) { fprintf(stderr, "%s: %ld of 2000 sampled lines do not solve their states\n", name.c_str(), badl); return false; }
    return true;
  }
};

// ---- --expand: every oblique stage in full, the form the checkers and the boundary tools read: one line
// per class (written once, at the class's least rank over the stage symmetries), its rebuilt line (never
// longer than the certificate's).  1ud and 2ud go to wNN.txt (NN target pieces off their home face),
// 3ax and 4op to lNN.txt (NN moves, two digits).
static bool expandStage(const Stage &A, const std::string &out, int threads) {
  const double t0 = now();
  if (system(("mkdir -p '" + out + "'").c_str())) return false;
  std::map<int, FILE *> fs;
  const uint64_t CH = 1 << 18, NC = (A.total + CH - 1) / CH;
  long nl = 0, hist[64] = {};
  for (uint64_t c0 = 0; c0 < NC; c0 += threads) {
    const uint64_t c1 = std::min<uint64_t>(NC, c0 + threads);
    std::vector<std::vector<std::pair<int, std::string>>> part(c1 - c0);
    std::vector<std::thread> th;
    for (uint64_t c = c0; c < c1; c++)
      th.emplace_back([&, c] {
        int8_t g[48], h[48];
        for (uint64_t r = c * CH; r < std::min(A.total, (c + 1) * CH); r++) {
          A.unrankG(r, g);
          bool least = true;
          for (int s : A.ss) {
            symG(gsyms[s], g, h);
            uint64_t r2;
            if (A.rankG(h, r2) && r2 < r) { least = false; break; }
          }
          if (!least) continue;
          int key = A.dist[r];
          if (A.ud()) {
            key = 0;
            for (int q = 0; q < 48; q++) if (A.cidx[g[q]] && faceOfK(q) != g[q] % 6) key++;
          }
          part[c - c0].push_back({key, text(A.lineOf(r))});
        }
      });
    for (auto &x : th) x.join();
    for (auto &pv : part)
      for (auto &[key, l] : pv) {
        FILE *&f = fs[key];
        if (!f) {
          char nm[16];
          snprintf(nm, sizeof nm, A.ud() ? "w%d.txt" : "l%02d.txt", key);
          if (!(f = fopen((out + "/" + nm).c_str(), "w"))) { fprintf(stderr, "%s: cannot write %s\n", A.name.c_str(), nm); return false; }
        }
        fprintf(f, "%s\n", l.c_str());
        nl++;
        hist[std::min<int>(std::count(l.begin(), l.end(), ' ') + !l.empty(), 63)]++;
      }
  }
  bool ok = true;
  for (auto &[k, f] : fs) ok &= fclose(f) == 0;
  fprintf(stderr, "%s: %ld classes written (%.0fs); lines by length:", A.name.c_str(), nl, now() - t0);
  for (int d = 0; d < 64; d++) if (hist[d]) fprintf(stderr, " %d:%ld", d, hist[d]);
  fprintf(stderr, "\n");
  return ok;
}

// ---- --makeconcise: a concise oblique stage.  Actions: the stage moves and every line of at most C moves
// (actions.txt, one line per class; the rebuild uses them under the stage symmetries and inverted).  Kept
// lines (lines.txt): level by level, a class whose length is v gets its line only if no action reaches it
// from a state already at most v - cost; so the rebuild gives every state at most its certificate length.
static bool makeConcise(Stage &A, int C, const std::string &out, int threads, bool relaxedW) {
  const double t0 = now();
  const uint64_t total = A.total;
  const std::vector<uint8_t> L = A.dist;
  int maxL = 0;
  for (auto x : L) maxL = std::max<int>(maxL, x);
  auto canon = [&](uint64_t r) {
    int8_t g[48], h[48];
    A.unrankG(r, g);
    uint64_t best = r;
    for (int s : A.ss) { symG(gsyms[s], g, h); uint64_t r2; if (A.rankG(h, r2)) best = std::min(best, r2); }
    return best;
  };
  // action lines: one per class of length 1..C
  std::vector<std::vector<cc::Mv>> actLines;
  for (uint64_t r = 0; r < total; r++)
    if (L[r] >= 1 && L[r] <= C && canon(r) == r) actLines.push_back(A.lineOf(r));
  std::vector<Stage::SAct> acts;
  {
    Stage B;  // just for addAct's dedup
    std::unordered_map<std::string, size_t> seen;
    for (auto &m : A.moves) B.addAct({m}, seen);
    for (auto &mv : actLines)
      for (int s2 : A.ss) {
        const auto m2 = symMoves(gsyms[s2], mv);
        B.addAct(m2, seen);
        std::vector<cc::Mv> inv;
        for (auto it = m2.rbegin(); it != m2.rend(); ++it) inv.push_back({it->face, it->depth, 4 - it->amt});
        B.addAct(inv, seen);
      }
    acts = B.acts;
  }
  fprintf(stderr, "%s: %zu action lines (length <= %d), %zu actions with the stage moves (%.0fs)\n", A.name.c_str(), actLines.size(), C, acts.size(), now() - t0);
  std::vector<uint8_t> R(total, 255), keepS(total, 0), dirty(total, 0);
  auto expand = [&](int v) {
    std::atomic<uint64_t> nr{0};
    std::vector<std::thread> th;
    for (int t = 0; t < threads; t++)
      th.emplace_back([&] {
        int8_t g[48], h[48];
        for (uint64_t lo; (lo = nr.fetch_add(1 << 16)) < total;)
          for (uint64_t r = lo; r < std::min(total, lo + (1 << 16)); r++) {
            if (R[r] != v || !dirty[r]) continue;
            dirty[r] = 0;
            A.unrankG(r, g);
            for (auto &a : acts) {
              for (int k = 0; k < 48; k++) h[k] = g[a.D[k]];
              uint64_t r2;
              if (!A.rankG(h, r2)) continue;
              const int nd = v + a.mv.size();
              uint8_t old = __atomic_load_n(&R[r2], __ATOMIC_RELAXED);
              while (old > nd) {
                if (__atomic_compare_exchange_n(&R[r2], &old, (uint8_t)nd, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) { __atomic_store_n(&dirty[r2], (uint8_t)1, __ATOMIC_RELAXED); break; }
              }
            }
          }
      });
    for (auto &x : th) x.join();
  };
  if (!relaxedW) {
    // strict: at each level v, a state of length v that nothing reaches within v keeps its line
    for (int v = 0; v <= maxL; v++) {
      for (uint64_t r = 0; r < total; r++) if (L[r] == v && R[r] > v) R[r] = v, keepS[r] = 1, dirty[r] = 1;
      expand(v);
    }
  } else {
    // relaxed: every state within W = the stage's worst; from the goal and the action lines, then in
    // rounds the classes still over W with the shortest lines
    for (uint64_t r = 0; r < total; r++) if (L[r] == 0) R[r] = 0, keepS[r] = 1, dirty[r] = 1;
    for (int v = 0; v <= maxL; v++) expand(v);
    for (int round = 1;; round++) {
      int lmin = 255;
      uint64_t over = 0;
      for (uint64_t r = 0; r < total; r++) if (R[r] > maxL) { over++; lmin = std::min<int>(lmin, L[r]); }
      fprintf(stderr, "  round %d: %llu states over %d (%.0fs)\n", round, (unsigned long long)over, maxL, now() - t0);
      if (!over) break;
      for (uint64_t r = 0; r < total; r++) if (R[r] > maxL && L[r] == lmin) R[r] = lmin, keepS[r] = 1, dirty[r] = 1;
      for (int v = lmin; v <= maxL; v++) expand(v);
    }
  }
  if (system(("mkdir -p '" + out + "'").c_str())) return false;
  FILE *fa = fopen((out + "/actions.txt").c_str(), "w"), *fl = fopen((out + "/lines.txt").c_str(), "w");
  if (!fa || !fl) return false;
  long nk = 0, bytes = 0;
  std::map<int, long> byLen;
  for (auto &mv : actLines) { const auto t = text(mv); fprintf(fa, "%s\n", t.c_str()); bytes += t.size() + 1; }
  fprintf(fl, "\n");  // the goal
  for (uint64_t r = 0; r < total; r++)
    if (keepS[r] && L[r] > 0 && canon(r) == r) {
      const auto t = text(A.lineOf(r));
      fprintf(fl, "%s\n", t.c_str());
      nk++, bytes += t.size() + 1, byLen[L[r]]++;
    }
  fclose(fa), fclose(fl);
  fprintf(stderr, "%s: kept %ld lines + %zu action lines, %ld bytes; kept by length:", A.name.c_str(), nk, actLines.size(), bytes);
  for (auto &[l, n] : byLen) fprintf(stderr, " %d:%ld", l, n);
  fprintf(stderr, " (%.0fs)\n", now() - t0);
  // reload the concise stage and compare
  Stage Z;
  Z.init(A.kind);
  const std::string keepCache = gCache;
  gCache.clear();
  const bool ok = Z.load(out, threads);
  gCache = keepCache;
  if (!ok) return false;
  uint64_t worse = 0;
  for (uint64_t r = 0; r < total; r++) worse += Z.dist[r] > (relaxedW ? maxL : L[r]);
  fprintf(stderr, "%s: reloaded concise stage: %llu states longer than %s (%.0fs)\n", A.name.c_str(), (unsigned long long)worse, relaxedW ? "the stage worst" : "their certificate lengths", now() - t0);
  return worse == 0;
}

// ---- actions for the boundary choice: outer turns and pool algorithm images (48 symmetries, inverses)
struct Act { int8_t D[48]; std::vector<cc::Mv> mv; };
static bool poolMoves(const char *buf, std::vector<cc::Mv> &mv) {
  size_t n = strlen(buf);
  if (!n || buf[n - 1] != '\n' || buf[0] == '#') return false;
  std::string l(buf);
  while (!l.empty() && (l.back() == '\n' || l.back() == ' ' || l.back() == '\r')) l.pop_back();
  if (l.size() >= 3 && l[l.size() - 3] == ' ' && l[l.size() - 2] == 'D') l.resize(l.size() - 3);
  size_t a = 0;
  while (a < l.size() && l[a] == ' ') a++;
  size_t e = a;
  while (e < l.size() && l[e] >= '0' && l[e] <= '9') e++;
  if (e > a && (e == l.size() || l[e] == ' ')) a = e;
  if (l.find('\t') != std::string::npos) a = l.find('\t') + 1;
  if (!cc::parseMoves(l.substr(a), mv) || mv.empty()) return false;
  for (auto &m : mv) if (m.depth < 1 || m.depth > 3) return false;
  return true;
}
static bool buildActs(const std::vector<std::string> &base, int C, bool keepUD, std::vector<Act> &acts) {
  std::unordered_map<std::string, size_t> idx;
  auto add = [&](std::vector<cc::Mv> mv) {
    simplify(mv);
    if (mv.empty() || (int)mv.size() > C) return;
    Act a;
    a.mv = mv;
    mapOf(mv, a.D);
    bool id = true;
    for (int k = 0; k < 48; k++) if (a.D[k] != k) id = false;
    if (id) return;
    if (keepUD) for (int k = 0; k < 48; k++) if (!side(faceOfK(k)) && groupOf(a.D[k]) != groupOf(k)) return;
    std::string key((const char *)a.D, 48);
    auto it = idx.find(key);
    if (it != idx.end()) { if (acts[it->second].mv.size() > mv.size()) acts[it->second].mv = mv; return; }
    idx[key] = acts.size();
    acts.push_back(a);
  };
  for (int f = 0; f < 6; f++) for (int a = 1; a <= 3; a++) add({{f, 1, a}});
  for (auto &bf : base) {
    FILE *in = fopen(bf.c_str(), "r");
    if (!in) { fprintf(stderr, "cannot read a pool\n"); return false; }
    char buf[4096];
    std::vector<cc::Mv> mv;
    while (fgets(buf, sizeof buf, in)) {
      if (!poolMoves(buf, mv) || (int)mv.size() > C) continue;
      std::vector<cc::Mv> inv;
      for (auto it = mv.rbegin(); it != mv.rend(); ++it) inv.push_back({it->face, it->depth, 4 - it->amt});
      for (auto &y : gsyms) { add(symMoves(y, mv)); add(symMoves(y, inv)); }
    }
    fclose(in);
  }
  std::stable_sort(acts.begin(), acts.end(), [](const Act &a, const Act &b) { return a.mv.size() < b.mv.size(); });
  return true;
}

// ---- the boundary choice: the first stage's solution P of the pair's state with the least
// |P| + d(second stage); returns that total (and P)
struct Choice { int c0 = 0, total = 0, len = 0, kind = 0; bool searched = false; std::vector<cc::Mv> P; long tried = 0, rejected = 0; };
static Choice choose(const Stage &A, const Stage &B, const std::vector<Act> &acts, const int8_t *G, int X, int SL, int DEEP, int DP) {
  Choice ch;
  uint64_t rA;
  if (!A.rankG(G, rA)) { fprintf(stderr, "%s: the pair is not at the stage start\n", A.name.c_str()); exit(1); }
  const auto L0 = A.lineOf(rA);
  ch.c0 = L0.size();
  ch.total = 1 << 20;
  auto eval = [&](std::vector<cc::Mv> P, int kind) {
    simplify(P);
    if ((int)P.size() >= ch.total) return;
    int8_t g[48];
    memcpy(g, G, 48);
    applyG(P, g);
    uint64_t r1, r2;
    ch.tried++;
    if (!A.rankG(g, r1) || A.dist[r1] != 0 || !B.rankG(g, r2)) return;  // must solve A (and so start B)
    const int t = (int)P.size() + B.dist[r2];
    if (t >= ch.total) return;
    if (!keepOthers(A.ud(), P)) { ch.rejected++; return; }
    ch.total = t, ch.len = P.size(), ch.P = P, ch.kind = kind;
  };
  eval(L0, 0);
  if (ch.total <= X) return ch;
  ch.searched = true;  // the table lines exceed the budget: look for an alternative
  // stage-symmetric copies of L0 (those that solve this state)
  for (int s : A.ss) eval(symMoves(gsyms[s], L0), 1);
  // an action, then the line of where it leads
  int8_t y[48], y1[48];
  for (auto &a : acts) {
    for (int k = 0; k < 48; k++) y[a.D[k]] = G[k];
    uint64_t ry;
    if (!A.rankG(y, ry)) continue;
    const int c = (int)a.mv.size() + A.dist[ry];
    if (c > ch.c0 + SL) continue;
    auto P = a.mv;
    const auto l = A.lineOf(ry);
    P.insert(P.end(), l.begin(), l.end());
    eval(P, 2);
  }
  if (ch.total <= X || DEEP < 0) return ch;
  // two actions (the second of at most DP moves), then the line
  for (auto &a1 : acts) {
    for (int k = 0; k < 48; k++) y1[a1.D[k]] = G[k];
    uint64_t r1;
    if (!A.rankG(y1, r1)) continue;
    if ((int)a1.mv.size() + A.dist[r1] > ch.c0 + SL + DEEP) continue;
    for (auto &a2 : acts) {
      if ((int)a2.mv.size() > DP || (int)(a1.mv.size() + a2.mv.size()) > ch.c0 + SL) break;
      for (int k = 0; k < 48; k++) y[a2.D[k]] = y1[k];
      uint64_t ry;
      if (!A.rankG(y, ry)) continue;
      if ((int)(a1.mv.size() + a2.mv.size()) + A.dist[ry] > ch.c0 + SL) continue;
      auto P = a1.mv;
      P.insert(P.end(), a2.mv.begin(), a2.mv.end());
      const auto l = A.lineOf(ry);
      P.insert(P.end(), l.begin(), l.end());
      eval(P, 3);
    }
  }
  return ch;
}

// ---- the cube side: certificate pair facelet k <-> xcube orbit block w (0: (a,b), 1: (b,a)) index j
static int phiW[48], phiJ[48];
static void readOrbitPhys(const xcube &c, int x, int y, uint8_t out[24]) {
  const xorbit &o = c.rows[y - 1][x - 1];
  for (int f = 0; f < 6; f++)
    for (int oz = 0; oz < 4; oz++) out[4 * f + oz] = o.a[4 * f + ((oz + c.facerots[f]) & 3)];
}
static bool buildLayout() {
  // brobdicube's quarter-turn maps on xorbits (1,2) / (2,1) of an 8x8x8 (Mx = My = 3)
  static int B[6][4][48];
  for (int f = 0; f < 6; f++)
    for (int d = 1; d <= 3; d++) {
      xcube c(3, 3, 'U');
      domove({d, (short)f, (short)1}, c, true);
      uint8_t X[2][24];
      readOrbitPhys(c, 1, 2, X[0]);
      readOrbitPhys(c, 2, 1, X[1]);
      for (int w = 0; w < 2; w++) for (int k = 0; k < 24; k++) B[f][d][w * 24 + X[w][k]] = w * 24 + k;
    }
  for (int o = 0; o < 2; o++) {
    int found = 0;
    for (int cand = 0; cand < 48; cand++) {
      if ((cand % 24) / 4 != 0) continue;
      int m[48];
      for (auto &x : m) x = -1;
      const int kb = 4 * o;  // U face, orbit o, i = 0
      m[kb] = cand;
      std::vector<int> q = {kb};
      bool bad = false;
      while (!q.empty() && !bad) {
        const int p = q.back();
        q.pop_back();
        for (int f = 0; f < 6 && !bad; f++)
          for (int d = 1; d <= 3 && !bad; d++) {
            const int p2 = mp[f][d][1][p], b2 = B[f][d][m[p]];
            if (m[p2] < 0) { m[p2] = b2; q.push_back(p2); }
            else if (m[p2] != b2) bad = true;
          }
      }
      for (int k = 0; k < 48 && !bad; k++) {
        if (orbitOfK(k) != o) continue;
        if (m[k] < 0 || (m[k] % 24) / 4 != faceOfK(k) || m[k] / 24 != cand / 24) bad = true;
      }
      if (bad) continue;
      found++;
      for (int k = 0; k < 48; k++) if (orbitOfK(k) == o) phiW[k] = m[k] / 24, phiJ[k] = m[k] % 24;
    }
    if (found != 1) { fprintf(stderr, "layout: %d consistent maps for orbit %d\n", found, o); return false; }
  }
  if (phiW[0] == phiW[4]) { fprintf(stderr, "layout: both orbits on one xorbit\n"); return false; }
  return true;
}
// the pair (a,b)'s groups (orbit * 6 + colour) in certificate order
static void readPair(const xcube &c, int a, int b, int8_t *G) {
  uint8_t X[2][24];
  readOrbitPhys(c, a, b, X[0]);
  readOrbitPhys(c, b, a, X[1]);
  for (int k = 0; k < 48; k++) G[k] = orbitOfK(k) * 6 + X[phiW[k]][phiJ[k]];
}
// every move played on the cube being solved (gRecCube) is recorded, for -o and --verify
static const xcube *gRecCube = nullptr;
static std::vector<mv> gRec;
static inline void dmove(const mv &m, xcube &c) {
  if (&c == gRecCube) gRec.push_back(m);
  domove(m, c, true);
}
static void play(xcube &c, int a, int b, const std::vector<cc::Mv> &mv) {
  for (auto &m : mv) dmove({m.depth == 1 ? 1 : m.depth == 2 ? a + 1 : b + 1, (short)m.face, (short)m.amt}, c);
}

#include "certrest.h"

// MAIN
// the certificate directory: certificate/ beside the directory holding this program, as named on the
// command line (the repo's solver/certsolve finds the repo's certificate/), else ./certificate
static std::string defaultCertDir(const char *argv0) {
  const std::string a0 = argv0;
  const size_t k = a0.find_last_of('/');
  const std::string d = (k == std::string::npos ? std::string() : a0.substr(0, k + 1)) + "../certificate";
  if (FILE *f = fopen((d + "/oblique/MANIFEST").c_str(), "r")) { fclose(f); return d; }
  return "certificate";
}

// conventional move names, as gullicube's -o: no layer deeper than the middle (layer d of a face is layer
// N + 1 - d of the opposite face turned the other way; the middle slice of an odd cube is kept)
static void conventionalMoves(std::vector<mv> &ms, int N) {
  for (auto &m : ms)
    if (2 * m.dep > N + 1) m.dep = N + 1 - m.dep, m.face = oppface[m.face], m.twist = 4 - m.twist;
}

// --timing: wall time and the process's instruction and cycle counts (macOS: proc_pid_rusage; elsewhere 0)
struct Ctr { double t = 0; uint64_t ins = 0, cyc = 0; };
static Ctr ctrNow() {
  Ctr c;
  c.t = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
#if defined(__APPLE__)
  rusage_info_v4 ri;
  if (proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri) == 0) c.ins = ri.ri_instructions, c.cyc = ri.ri_cycles;
#endif
  return c;
}
struct CtrSum {
  double t = 0, ins = 0, cyc = 0;
  void add(const Ctr &a, const Ctr &b) { t += b.t - a.t, ins += (double)(b.ins - a.ins), cyc += (double)(b.cyc - a.cyc); }
};
static void printCtr(const char *what, const CtrSum &c) {
  printf("# timing %-34s %10.3f s %10.3f G instr %10.3f G cycles  IPC %5.2f\n", what, c.t, c.ins * 1e-9, c.cyc * 1e-9, c.cyc > 0 ? c.ins / c.cyc : 0.0);
}

static void usage() {
  fputs("usage: certsolve N [options]      solve N x N x N cubes (N >= 6) from the certificates alone\n"
        "  scramble (default: a uniformly random position):\n"
        "    -r n            scramble with n random moves\n"
        "    -R              scramble with 50*N random moves\n"
        "    -i              read the scramble moves from stdin\n"
        "    -f              read the cube state from stdin as a Kociemba facelet string: 6*N*N\n"
        "                    letters, faces U R F D L B, each row by row as seen in the usual net\n"
        "                    (U with B at the top, D with F at the top, the others with U at the\n"
        "                    top); each letter names the face whose colour the sticker has when\n"
        "                    solved; whitespace is ignored\n"
        "    -F              print the start state as a Kociemba facelet string\n"
        "    -M n            solve n cubes (not with -i or -f) and report aggregate stats\n"
        "    --seed s        random seed (default: from entropy; the seed used is printed)\n"
        "  solve:\n"
        "    --bound         at the boundaries 1ud/2ud and 3ax/4op choose each pair's first-stage line\n"
        "                    so that the two stages together stay within --ud and --ax (P = 72);\n"
        "                    default: the certificate's lines (the plain sum, P = 80)\n"
        "    --ud x          --bound: 1ud + 2ud budget (default 33)\n"
        "    --ax x          --bound: 3ax + 4op budget (default 39)\n"
        "    --prop c        --bound: longest algorithm tried as an alternative's first action (default 10)\n"
        "    --slack l       --bound: how much longer than the table line an alternative may be (default 6)\n"
        "    --deep s        --bound: slack allowed when two actions are needed (default 6)\n"
        "    --deepprop c    --bound: longest algorithm for the second action (default 8)\n"
        "    --base f        --bound: algorithm pool, repeatable (default: pair3, p8s10, p9, eg8 and c12x.cls\n"
        "                    in pools/ beside the directory holding certsolve, beside certsolve, else here)\n"
        "    --obliques      solve the oblique orbits only\n"
        "    --solvedobl     testing: start with the oblique orbits solved\n"
        "    -t n            threads (default: all cores)\n"
        "  certificates:\n"
        "    -D dir          the certificate directory (default: certificate/ beside the directory\n"
        "                    holding certsolve, else ./certificate): oblique/, diagonal/, mid/, wing/\n"
        "    --cache dir     keep the rebuilt tables here and read them back on later runs\n"
        "                    (default ~/.cache/certsolve)\n"
        "    --nopersist     neither read nor write the cache\n"
        "  output and checks:\n"
        "    -o              write the solution (lines start with a space)\n"
        "    -d              show every orbit's stage lengths\n"
        "    --timing        wall time, instructions and cycles per part, and per oblique pair for each\n"
        "                    half (1ud + 2ud, 3ax + 4op), searched or not\n"
        "    --verify        replay scramble + solution on an independent sticker simulator\n"
        "  certificate building:\n"
        "    --expand dir    write the four oblique stages in full (one line per class) into dir/stages,\n"
        "                    the form certcheck, axcheck and the boundary tools read\n"
        "    --makeconcise k dir  write a concise form of oblique stage k (0..3) into dir\n"
        "    --act c         --makeconcise: kept lines of at most c moves also serve as actions (default 4)\n"
        "    --relaxed       --makeconcise: every state need only stay within the stage worst\n"
        "    --colact l      mid and diagonal tables: kept lines of at most l moves serve as actions\n"
        "                    (default 6; must match the certificate)\n",
        stderr);
}

int main(int argc, char **argv) {
  if (argc < 2) { usage(); return 1; }
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  fflush(stdout);
  int N = 0, CUBES = 1, T = std::max(1u, std::thread::hardware_concurrency()), C = 10, SL = 6, DEEP = 6, DP = 8, XUD = 33, XAX = 39;
  long scr = 0;
  uint64_t seed = 0;
  bool bound = false, oblOnly = false, solvedObl = false, persist = true, autoLen = false;
  bool timing = false;
  bool readScramble = false, readFacelets = false, printFacelets = false, writeMoves = false, detail = false, verify = false;
  int mkK = -1, mkC = 4, colAct = 6;
  bool mkRelaxed = false;
  std::string mkOut, expandOut;
  const std::string home = getenv("HOME") ? getenv("HOME") : ".";
  std::string cacheDir = home + "/.cache/certsolve", root = defaultCertDir(argv[0]);
  std::vector<std::string> base;
  for (int i = 1; i < argc; i++) {
    const std::string a = argv[i];
    auto need = [&](int k) { if (i + k >= argc) { fprintf(stderr, "option %s needs %d argument%s\n", a.c_str(), k, k > 1 ? "s" : ""); exit(2); } };
    if (a == "-i") readScramble = true;                       // read scramble moves from stdin
    else if (a == "-f") readFacelets = true;                  // read a Kociemba facelet string from stdin
    else if (a == "-F") printFacelets = true;                 // print the start state as a facelet string
    else if (a == "-r") need(1), scr = atol(argv[++i]);       // scramble with n random moves
    else if (a == "-R") autoLen = true;                       // scramble with 50*N random moves
    else if (a == "-M") need(1), CUBES = atoi(argv[++i]);     // this many cubes
    else if (a == "-o") writeMoves = true;                    // write the solution (lines start with a space)
    else if (a == "-d") detail = true;                        // every orbit's stage lengths
    else if (a == "-t") need(1), T = atoi(argv[++i]);
    else if (a == "-D") need(1), root = argv[++i];            // the certificate directory
    else if (a == "--seed") need(1), seed = strtoull(argv[++i], nullptr, 10);
    else if (a == "--verify") verify = true;                  // replay on the independent sticker simulator
    else if (a == "--timing") timing = true;
    else if (a == "--bound") bound = true;
    else if (a == "--ud") need(1), XUD = atoi(argv[++i]);
    else if (a == "--ax") need(1), XAX = atoi(argv[++i]);
    else if (a == "--prop") need(1), C = atoi(argv[++i]);
    else if (a == "--slack") need(1), SL = atoi(argv[++i]);
    else if (a == "--deep") need(1), DEEP = atoi(argv[++i]);
    else if (a == "--deepprop") need(1), DP = atoi(argv[++i]);
    else if (a == "--base") need(1), base.push_back(argv[++i]);
    else if (a == "--obliques") oblOnly = true;
    else if (a == "--solvedobl") solvedObl = true;
    else if (a == "--cache") need(1), cacheDir = argv[++i];
    else if (a == "--nopersist") persist = false;
    else if (a == "--expand") need(1), expandOut = argv[++i];
    else if (a == "--makeconcise") need(2), mkK = atoi(argv[++i]), mkOut = argv[++i];
    else if (a == "--act") need(1), mkC = atoi(argv[++i]);
    else if (a == "--relaxed") mkRelaxed = true;
    else if (a == "--colact") need(1), colAct = atoi(argv[++i]);
    else if (a[0] == '-') { fprintf(stderr, "unknown option %s\n", a.c_str()); return 2; }
    else if (isdigit((unsigned char)a[0]) && !N) N = atoi(a.c_str());
    else { fprintf(stderr, "unknown argument %s\n", a.c_str()); return 2; }
  }
  if (!N && (!expandOut.empty() || mkK >= 0)) N = 6;  // certificate building needs no cube size
  if (N < 6) { usage(); return 1; }
  if (autoLen) scr = 50L * N;
  if (readScramble + readFacelets + (scr > 0) > 1) { fprintf(stderr, "use at most one of -i, -f, -r and -R\n"); return 2; }
  if ((readScramble || readFacelets) && CUBES != 1) { fprintf(stderr, "-M is for random cubes, not with -i or -f\n"); return 2; }
  if (CUBES < 1 || T < 1) { fprintf(stderr, "-M and -t need a positive number\n"); return 2; }
  if ((writeMoves || verify) && (oblOnly || solvedObl)) { fprintf(stderr, "-o and --verify need a whole-cube solve (not --obliques or --solvedobl)\n"); return 2; }
  const std::string certDir = root + "/oblique", certD = root + "/diagonal", certM = root + "/mid", certW = root + "/wing";
  if (bound && base.empty()) {  // the algorithm pools: pools/ beside the directory holding certsolve, beside certsolve, else here
    const std::string a0 = argv[0];
    const size_t k = a0.find_last_of('/');
    const std::string exeDir = k == std::string::npos ? std::string() : a0.substr(0, k + 1);
    for (auto f : {"pair3.cls", "p8s10.cls", "p9.cls", "eg8.cls", "c12x.cls"}) {
      std::string got;
      for (const std::string &c : {exeDir + "../pools/" + f, exeDir + f, std::string(f)})
        if (FILE *fp = fopen(c.c_str(), "r")) { fclose(fp); got = c; break; }
      if (got.empty()) { fprintf(stderr, "algorithm pool %s not found in ../pools or beside certsolve, or in the current directory\n", f); return 1; }
      base.push_back(got);
    }
  }
  if (!seed) {
    std::random_device rd;
    seed = ((uint64_t)rd() << 32 ^ rd()) % 1000000000000ULL;
    if (!seed) seed = 1;
  }
  printf("# seed %llu\n", (unsigned long long)seed);
  fflush(stdout);
  const Ctr cStart = ctrNow();
  CtrSum tLoadObl, tActs, tLoadRest, tPh[2], tRest;
  std::vector<double> pairT[2], pairI[2], pairC[2];  // per pair and half: wall, instructions, cycles
  std::vector<char> pairS[2];                        // searched?
  // -i / -f: the scramble moves or the facelets, read once
  std::string inText;
  if (readScramble || readFacelets) {
    std::string line;
    while (std::getline(std::cin, line)) inText += line + " ";
  }
  rng.seed(seed);
  if (persist) gCache = cacheDir;
  const double t0 = now();
  const bool odd = N & 1;
  const int My = (N - 2) / 2, Mx = N - 2 - My;
  std::vector<std::pair<int, int>> pairs;
  for (int a = 1; a <= My; a++) for (int b = a + 1; b <= My; b++) pairs.push_back({a, b});
  const bool doObl = !solvedObl && !pairs.empty();

  // pair geometry (8x8x8 certificate frame)
  cc::Cube C8(8);
  const auto pf = cc::pairFacelets(C8);
  std::vector<int> fidx(C8.pos.size(), -1);
  for (int k = 0; k < 48; k++) fidx[pf[k]] = k;
  for (int f = 0; f < 6; f++)
    for (int d = 1; d <= 3; d++)
      for (int a = 1; a <= 3; a++) {
        for (int k = 0; k < 48; k++) {
          int i = pf[k];
          for (int q = 0; q < a; q++) i = C8.perms[f][d - 1][i];
          mp[f][d][a][k] = fidx[i];
        }
        for (int i = 0; i < 384; i++) {
          int x = i;
          for (int q = 0; q < a; q++) x = C8.perms[f][d - 1][x];
          c8perm[f][d][a][i] = x;
        }
      }
  for (int i = 0; i < 384; i++) {
    c8faceOf[i] = C8.faceOf[i];
    const int r = (i / 8) % 8, c = i % 8, dr = std::min(r, 7 - r) + 1, dc = std::min(c, 7 - c) + 1;
    if ((dr == 2 && dc == 4) || (dr == 4 && dc == 2) || (dr == 3 && dc == 4) || (dr == 4 && dc == 3)) othFac.push_back(i);
  }
  if (othFac.size() != 96) { fprintf(stderr, "expected 96 other oblique facelets, got %zu\n", othFac.size()); return 1; }
  for (int a = 0; a <= 48; a++) { binom[a][0] = 1; for (int b = 1; b <= a; b++) binom[a][b] = binom[a - 1][b - 1] + (b < a ? binom[a - 1][b] : 0); }
  for (int a = 0; a <= 48; a++) for (int b = a + 1; b <= 48; b++) binom[a][b] = 0;
  {
    int n = 0, n16 = 0, n8 = 0;
    for (int k = 0; k < 48; k++) { slotOf[k] = -1; if (side(faceOfK(k))) slotOf[k] = n, slots[n++] = k; }
    for (int v = 0; v < 65536; v++) rk16[v] = -1;
    for (int v = 0; v < 256; v++) rk8[v] = -1;
    for (int v = 0; v < 65536; v++) if (__builtin_popcount(v) == 8) ur16[n16] = v, rk16[v] = n16++;
    for (int v = 0; v < 256; v++) if (__builtin_popcount(v) == 4) ur8[n8] = v, rk8[v] = n8++;
  }
  {
    const int pm[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    const int par[6] = {1, -1, -1, 1, 1, -1};
    for (int pi = 0; pi < 6; pi++)
      for (int sg = 0; sg < 8; sg++) {
        GSym y;
        auto map = [&](const std::array<int, 3> &p) {
          std::array<int, 3> q;
          for (int r = 0; r < 3; r++) q[r] = p[pm[pi][r]] * ((sg >> r & 1) ? -1 : 1);
          return q;
        };
        for (int k = 0; k < 48; k++) {
          const auto q = map(C8.pos[pf[k]]);
          y.fac[k] = -1;
          for (int x = 0; x < 48; x++) if (C8.pos[pf[x]] == q) y.fac[k] = x;
          if (y.fac[k] < 0) { fprintf(stderr, "a symmetry leaves the pair\n"); return 1; }
        }
        for (int g = 0; g < 12; g++) for (int k = 0; k < 48; k++) if (groupOf(k) == g) y.gmap[g] = groupOf(y.fac[k]);
        for (int f = 0; f < 6; f++) {
          const auto q = map(cc::Cube::normalOf(f));
          for (int f2 = 0; f2 < 6; f2++) if (cc::Cube::normalOf(f2) == q) y.face[f] = f2;
        }
        y.flip = par[pi] * ((sg & 1) ? -1 : 1) * ((sg & 2) ? -1 : 1) * ((sg & 4) ? -1 : 1) < 0;
        gsyms.push_back(y);
      }
  }
  if (!buildLayout()) return 1;

  if (!expandOut.empty()) {  // --expand OUTDIR
    const char *sub[4] = {"none..aUbU", "aUbU..aDbD", "3ax", "4op"};
    for (int k = 0; k < 4; k++) {
      Stage A;
      A.init(k);
      if (!A.load(certDir + "/stages/" + sub[k], T) || !expandStage(A, expandOut + "/stages/" + sub[k], T)) return 1;
    }
    printf("expanded: %s/stages\n", expandOut.c_str());
    return 0;
  }
  if (mkK >= 0) {  // --makeconcise K OUTDIR [--act C]
    const char *sub[4] = {"none..aUbU", "aUbU..aDbD", "3ax", "4op"};
    Stage A;
    A.init(mkK);
    if (!A.load(certDir + "/stages/" + sub[mkK], T)) return 1;
    return makeConcise(A, mkC, mkOut + "/stages/" + sub[mkK], T, mkRelaxed) ? 0 : 1;
  }
  // the oblique stages, rebuilt from the small certificate
  Stage st[4];
  std::vector<Act> actsUD, actsAX;
  if (doObl) {
    const char *sub[4] = {"none..aUbU", "aUbU..aDbD", "3ax", "4op"};
    for (int k = 0; k < 4; k++) {
      st[k].init(k);
      if (!st[k].load(certDir + "/stages/" + sub[k], T)) return 1;
    }
    fprintf(stderr, "oblique tables rebuilt (%.0fs)\n", now() - t0);
    const Ctr c1 = ctrNow();
    tLoadObl.add(cStart, c1);
    if (bound) {
      const double ta = now();
      if (!buildActs(base, C, false, actsUD) || !buildActs(base, C, true, actsAX)) return 1;
      fprintf(stderr, "actions of at most %d moves: 1ud %zu, 3ax %zu (%.0fs)\n", C, actsUD.size(), actsAX.size(), now() - ta);
    }
    tActs.add(c1, ctrNow());
  }

  // the rest: mid, diagonal and wing certificates; corner tables
  Orb24 diagO, midO;
  ColStage diag[3], mid[2];
  WingStage wing[4];
  int phiD[24], phiM[24], phiW[24], phiWinv[24];
  const int nW = My, nD = My, nM = odd ? My : 0;  // orbits: wing depths 2..My+1, diagonal depths 2..My+1, mid depths 2..My+1
  const Ctr cRestLoad = ctrNow();
  if (!oblOnly) {
    const double tr = now();
    cc::Cube C6(6), C7(7);
    if (!diagO.init(6, cc::diagFacelets(C6, 2), 2) || !midO.init(7, cc::midFacelets(C7, 2), 3)) { fprintf(stderr, "colour orbit geometry failed\n"); return 1; }
    const char *dsub[3] = {"none..D", "D..U", "UD..LFRB"}, *msub[2] = {"none..UD", "UD..LFRB"};
    const char *wsub[4] = {"none..abcd", "abcd..efgh", "abcdefgh..ijklm", "abcdefghijklm..nopqrstuvwx"};
    for (int s = 0; s < 3; s++) if (!diag[s].load(certD + "/stages/" + dsub[s], diagO, T) || !diag[s].build(colAct, T)) return 1;
    if (odd)
      for (int s = 0; s < 2; s++) if (!mid[s].load(certM + "/stages/" + msub[s], midO, T) || !mid[s].build(colAct, T)) return 1;
    fprintf(stderr, "diagonal tables rebuilt: worst %d + %d + %d", diag[0].worst, diag[1].worst, diag[2].worst);
    if (odd) fprintf(stderr, "; mid: worst %d + %d", mid[0].worst, mid[1].worst);
    fprintf(stderr, " (%.0fs)\n", now() - tr);
    {
      const auto wf6 = cc::wingFacelets(C6, 2);
      std::vector<int> in6(C6.pos.size(), -1);
      for (int k = 0; k < 24; k++) in6[wf6[k]] = k;
      for (int f = 0; f < 6; f++)
        for (int d = 1; d <= 2; d++)
          for (int a = 1; a <= 3; a++)
            for (int k = 0; k < 24; k++) {
              int i = wf6[k];
              for (int q = 0; q < a; q++) i = C6.perms[f][d - 1][i];
              if (in6[i] < 0) { fprintf(stderr, "a move takes a wing facelet out of the orbit\n"); return 1; }
              mw[f][d][a][k] = in6[i];
            }
    }
    const double tw = now();
    if (!loadWingActs(certW + "/actions.txt")) return 1;
    for (int s = 0; s < 4; s++) if (!wing[s].build(wsub[s], T)) return 1;
    fprintf(stderr, "wing tables rebuilt from %zu algorithms: worst %d + %d + %d + %d (%.0fs)\n", wingActs.size(), wing[0].longest, wing[1].longest, wing[2].longest, wing[3].longest, now() - tw);
    const int xdD[4] = {0, 1, 2, 0}, xdM[4] = {0, 1, 2, 4}, xdW[4] = {0, 1, 2, 0};
    if (!layoutMap(diagO.mp, 2, 2, 2, xdD, [](const xcube &c, uint8_t *X) { readOrbitPhys(c, 1, 1, X); }, true, phiD) ||
        !layoutMap(midO.mp, 3, 3, 2, xdM, [](const xcube &c, uint8_t *X) { readOrbitPhys(c, 3, 1, X); }, true, phiM) ||
        !layoutMap(mw, 2, 2, 2, xdW, [](const xcube &c, uint8_t *X) { for (int k = 0; k < 24; k++) X[k] = c.wingorbits[0].a[k]; }, false, phiW)) {
      fprintf(stderr, "layout: no consistent map for a diagonal, mid or wing orbit\n");
      return 1;
    }
    for (int k = 0; k < 24; k++) phiWinv[phiW[k]] = k;
    if (!t3::init()) return 1;
    const double tc = now();
    if (odd) t3::buildTwoPhase();
    else t3::buildCorners(T);
    fprintf(stderr, "%s (%.0fs)\n", odd ? "two-phase tables built" : "corner table built", now() - tc);
  }

  // layout self-tests on this N: random certificate-depth sequences on the cube against the simulations
  {
    long bad = 0;
    xcube c(Mx, My, 'R');
    for (size_t pi = 0; pi < pairs.size(); pi += std::max<size_t>(1, pairs.size() / 100)) {
      const auto [a, b] = pairs[pi];
      int8_t G[48], H[48];
      readPair(c, a, b, G);
      std::vector<cc::Mv> s;
      for (int i = 0; i < 200; i++) s.push_back({(int)(rng() % 6), 1 + (int)(rng() % 3), 1 + (int)(rng() % 3)});
      play(c, a, b, s);
      applyG(s, G);
      readPair(c, a, b, H);
      if (memcmp(G, H, 48)) bad++;
    }
    if (!oblOnly) {
      auto rnd = [&](int nd) { std::vector<cc::Mv> s; for (int i = 0; i < 200; i++) s.push_back({(int)(rng() % 6), 1 + (int)(rng() % nd), 1 + (int)(rng() % 3)}); return s; };
      for (int d = 2; d <= My + 1; d += std::max(1, My / 30)) {
        const int dep[4] = {0, 1, d, Mx + 1};
        uint8_t X[24];
        int8_t G[24], H[24];
        // diagonal orbit (d-1, d-1)
        readOrbitPhys(c, d - 1, d - 1, X);
        for (int k = 0; k < 24; k++) G[k] = X[phiD[k]];
        auto s = rnd(2);
        for (auto &m : s) domove({dep[m.depth], (short)m.face, (short)m.amt}, c, true);
        diagO.apply(s, G);
        readOrbitPhys(c, d - 1, d - 1, X);
        for (int k = 0; k < 24; k++) H[k] = X[phiD[k]];
        bad += memcmp(G, H, 24) != 0;
        if (odd) {
          readOrbitPhys(c, Mx, d - 1, X);
          for (int k = 0; k < 24; k++) G[k] = X[phiM[k]];
          s = rnd(3);
          for (auto &m : s) domove({dep[m.depth], (short)m.face, (short)m.amt}, c, true);
          midO.apply(s, G);
          readOrbitPhys(c, Mx, d - 1, X);
          for (int k = 0; k < 24; k++) H[k] = X[phiM[k]];
          bad += memcmp(G, H, 24) != 0;
        }
        // wing orbit at depth d: where each piece is
        int P0[24], P1[24];
        for (int k = 0; k < 24; k++) P0[phiWinv[c.wingorbits[d - 2].a[phiW[k]]]] = k;
        s = rnd(2);
        for (auto &m : s) domove({dep[m.depth], (short)m.face, (short)m.amt}, c, true);
        for (auto &m : s) for (int t = 0; t < 24; t++) P0[t] = mw[m.face][m.depth][m.amt][P0[t]];
        for (int k = 0; k < 24; k++) P1[phiWinv[c.wingorbits[d - 2].a[phiW[k]]]] = k;
        bad += memcmp(P0, P1, sizeof P0) != 0;
      }
    }
    if (bad) { fprintf(stderr, "layout self-test failed (%ld)\n", bad); return 1; }
  }
  tLoadRest.add(cRestLoad, ctrNow());
  const int Pb = bound ? XUD + XAX : 80;
  int Mb = 0, Db = 0, Wb = 0, Cb = odd ? 20 : t3::cornerMax;
  if (!oblOnly) {
    for (int s = 0; s < 3; s++) Db += diag[s].worst;
    if (odd) for (int s = 0; s < 2; s++) Mb += mid[s].worst;
    for (int s = 0; s < 4; s++) Wb += wing[s].longest;
  }
  const long polyBound = (long)pairs.size() * Pb + (oblOnly ? 0 : (long)nM * Mb + (long)nD * Db + Cb + (long)nW * Wb);
  printf("# N=%d: %zu oblique pairs%s; mode %s\n", N, pairs.size(), solvedObl ? " (started solved: --solvedobl)" : "", bound ? "boundary choice" : "table lines (plain sum)");
  if (bound) printf("# bounds: 1ud + 2ud <= %d, 3ax + 4op <= %d; actions of at most %d moves, slack %d, deep %d (second action at most %d)\n", XUD, XAX, C, SL, DEEP, DP);
  if (!oblOnly)
    printf("# per orbit: oblique pair %d, mid %d, diagonal %d, wing %d; corners%s %d; %d mid, %d diagonal, %d wing orbits; whole-cube bound %ld\n", Pb, Mb, Db, Wb, odd ? " and midges" : "", Cb, nM, nD, nW,
           polyBound);
  fflush(stdout);

  int worst = 0;
  long fails = 0, sumAll = 0, worstCube = 0, searched[2] = {0, 0};
  std::vector<long> hist(200, 0);
  for (int cu = 0; cu < CUBES; cu++) {
    std::vector<mv> scramble;
    if (readScramble) scramble = parse_moves(inText);
    else for (long i = 0; i < scr; i++) scramble.push_back({1 + (int)(rng() % ((N + 1) / 2)), (short)(rng() % 6), (short)(1 + rng() % 3)});
    const bool fromMoves = readScramble || scr;
    moveset = scramble;  // brobdicube's 'I' construction applies this global
    xcube cube(Mx, My, readFacelets ? 'C' : fromMoves ? 'I' : 'R');
    moveset.clear();
    if (readFacelets) {
      std::string err;
      if (!xcube_from_facelets(cube, inText, err)) { fprintf(stderr, "bad facelets: %s\n", err.c_str()); return 1; }
    }
    const std::string startFacelets = xcube_to_facelets(cube);
    if (printFacelets) printf("facelets %s\n", startFacelets.c_str());
    // odd N with the face centres turned away (middle-slice scramble moves, or facelets): name each colour
    // by the face its centre is on.  The cube is the same; solved then means every face one colour.
    if (odd)
      for (int f = 0; f < 6; f++)
        if (cube.ccs[f] != f) {
          std::string rl;
          for (char ch : startFacelets) if (ch > ' ') rl += ch;
          char ren[256];
          for (int c = 0; c < 6; c++) {  // centre facelet of Kociemba face kf is at row/column N/2
            const int k = c * N * N + (N / 2) * N + N / 2;
            ren[(unsigned char)rl[k]] = "URFDLB"[c];
          }
          for (auto &ch : rl) ch = ren[(unsigned char)ch];
          xcube rc(Mx, My, 'C');
          std::string err;
          if (!xcube_from_facelets(rc, rl, err)) { fprintf(stderr, "recolouring by the face centres failed: %s\n", err.c_str()); return 1; }
          cube = rc;
          break;
        }
    gRecCube = &cube;
    gRec.clear();
    size_t mark = 0;
    // -o: the moves of one set solved at once (a pair's phase, an orbit, the corners) on one line
    auto flushSeg = [&] {
      if (writeMoves) {
        std::vector<mv> seg(gRec.begin() + mark, gRec.end());
        conventionalMoves(seg, N);
        std::string line;
        for (const mv &m : seg) {
          line += ' ';
          if (m.dep > 1) line += std::to_string(m.dep);
          line += "ULFRBD"[m.face];
          if (m.twist == 2) line += '2';
          if (m.twist == 3) line += '\'';
        }
        if (!line.empty()) printf("%s\n", line.c_str());
      }
      mark = gRec.size();
    };
    if (solvedObl)
      for (int y = 1; y <= My; y++)
        for (int x = 1; x <= My; x++)
          if (x != y) oclear(cube.rows[y - 1][x - 1]);
    for (int f = 0; f < 6; f++) if (cube.facerots[f]) { cube.normalizefacerots(); break; }
    long cnt[5] = {0, 0, 0, 0, 0};  // obliques, mids, diagonals, corners, wings
    if (doObl) {
      std::vector<std::array<int, 4>> len(pairs.size());
      auto stageGoal = [&](int k, int a, int b) {
        int8_t G[48];
        readPair(cube, a, b, G);
        uint64_t r;
        if (!st[k].rankG(G, r) || st[k].dist[r] != 0) { fprintf(stderr, "pair (%d,%d): %s goal not reached\n", a, b, st[k].name.c_str()); exit(1); }
      };
      for (int ph = 0; ph < 2; ph++) {
        const Stage &A = st[2 * ph], &B = st[2 * ph + 1];
        for (size_t pi = 0; pi < pairs.size(); pi++) {
          const auto [a, b] = pairs[pi];
          const Ctr cp0 = ctrNow();
          bool wasSearched = false;
          int8_t G[48];
          readPair(cube, a, b, G);
          std::vector<cc::Mv> P;
          if (bound) {
            const Choice ch = choose(A, B, ph ? actsAX : actsUD, G, ph ? XAX : XUD, SL, DEEP, DP);
            searched[ph] += ch.searched;
            wasSearched = ch.searched;
            P = ch.P;
            if (ch.kind && detail) printf("cube %d pair (%d,%d) %s: line %d, alternative (%s) %d, total with %s %d\n", cu + 1, a, b, A.name.c_str(), ch.c0,
                                ch.kind == 1 ? "symmetric copy" : ch.kind == 2 ? "action + line" : "two actions + line", ch.len, B.name.c_str(), ch.total);
          } else {
            uint64_t r;
            if (!A.rankG(G, r)) { fprintf(stderr, "pair (%d,%d): not at the %s start\n", a, b, A.name.c_str()); return 1; }
            P = A.lineOf(r);
          }
          play(cube, a, b, P);
          stageGoal(2 * ph, a, b);
          readPair(cube, a, b, G);
          uint64_t r;
          if (!B.rankG(G, r)) { fprintf(stderr, "pair (%d,%d): not at the %s start\n", a, b, B.name.c_str()); return 1; }
          const auto Q = B.lineOf(r);
          play(cube, a, b, Q);
          stageGoal(2 * ph + 1, a, b);
          {
            const Ctr cp1 = ctrNow();
            pairT[ph].push_back(cp1.t - cp0.t), pairI[ph].push_back((double)(cp1.ins - cp0.ins)), pairC[ph].push_back((double)(cp1.cyc - cp0.cyc));
            pairS[ph].push_back(wasSearched);
            tPh[ph].add(cp0, cp1);
          }
          len[pi][2 * ph] = P.size(), len[pi][2 * ph + 1] = Q.size();
          if (detail) printf("cube %d pair (%d,%d) phase %d: %s %zu + %s %zu = %zu\n", cu + 1, a, b, ph + 1, ph ? "3ax" : "1ud", P.size(), ph ? "4op" : "2ud", Q.size(), P.size() + Q.size());
          flushSeg();
        }
        for (auto [a, b] : pairs) {
          uint8_t X[24];
          for (auto [x, y] : {std::pair<int, int>{a, b}, {b, a}}) {
            readOrbitPhys(cube, x, y, X);
            for (int j = 0; j < 24; j++) {
              const int f = j / 4;
              if (ph == 0 ? ((f == 0 || f == 5) && X[j] != f) : X[j] != f) { fprintf(stderr, "orbit (%d,%d) not %s after phase %d\n", x, y, ph ? "solved" : "U/D-solved", ph + 1); return 1; }
            }
          }
        }
      }
      for (size_t pi = 0; pi < pairs.size(); pi++) {
        const int t = len[pi][0] + len[pi][1] + len[pi][2] + len[pi][3];
        worst = std::max(worst, t);
        sumAll += t;
        hist[t]++;
        cnt[0] += t;
        if (t > Pb || (bound && (len[pi][0] + len[pi][1] > XUD || len[pi][2] + len[pi][3] > XAX))) fails++;
        if (detail) printf("cube %d pair (%d,%d): 1ud %d + 2ud %d + 3ax %d + 4op %d = %d\n", cu + 1, pairs[pi].first, pairs[pi].second, len[pi][0], len[pi][1], len[pi][2], len[pi][3], t);
      }
    }
    const Ctr cR0 = ctrNow();
    if (!oblOnly) {
      auto die = [&](const std::string &m) { fprintf(stderr, "cube %d: %s\n", cu + 1, m.c_str()); exit(1); };
      auto playDep = [&](const std::vector<cc::Mv> &mv, const int *dep) { for (auto &m : mv) dmove({dep[m.depth], (short)m.face, (short)m.amt}, cube); };
      // mids (odd N), then diagonals: colour stages
      auto colOrbit = [&](const char *what, int x, int y, int d, ColStage *stg, int ns, const Orb24 &O, const int *phi, long &sum, int lim) {
        const int dep[4] = {0, 1, d, Mx + 1};
        std::string parts;
        int tot = 0;
        for (int s = 0; s < ns; s++) {
          uint8_t X[24];
          int8_t col[24];
          readOrbitPhys(cube, x, y, X);
          for (int k = 0; k < 24; k++) col[k] = X[phi[k]];
          if (!stg[s].atStart(col)) die(std::string(what) + " depth " + std::to_string(d) + ": not at the " + stg[s].name + " start");
          std::vector<cc::Mv> P;
          if (!stg[s].solveC(col, P)) die(std::string(what) + " depth " + std::to_string(d) + ": no " + stg[s].name + " line");
          playDep(P, dep);
          readOrbitPhys(cube, x, y, X);
          for (int k = 0; k < 24; k++) col[k] = X[phi[k]];
          if (!stg[s].atGoal(col)) die(std::string(what) + " depth " + std::to_string(d) + ": " + stg[s].name + " goal not reached");
          parts += (s ? " + " : "") + stg[s].name.substr(stg[s].name.find("..") + 2) + " " + std::to_string(P.size());
          tot += P.size();
          (void)O;
        }
        sum += tot;
        if (tot > lim) fails++;
        if (detail) printf("cube %d %s %d: %s = %d\n", cu + 1, what, d, parts.c_str(), tot);
        flushSeg();
      };
      if (odd)
        for (int d = 2; d <= My + 1; d++) colOrbit("mid", Mx, d - 1, d, mid, 2, midO, phiM, cnt[1], Mb);
      for (int d = 2; d <= My + 1; d++) colOrbit("diagonal", d - 1, d - 1, d, diag, 3, diagO, phiD, cnt[2], Db);
      // every centre colour-solved (and the face centres home)
      for (int y = 1; y <= My; y++)
        for (int x = 1; x <= Mx; x++) {
          uint8_t X[24];
          readOrbitPhys(cube, x, y, X);
          for (int j = 0; j < 24; j++) if (X[j] != j / 4) die("centre orbit (" + std::to_string(x) + "," + std::to_string(y) + ") not solved after the centre stages");
        }
      for (int f = 0; f < 6; f++) if (cube.ccs[f] != f) die("face centres moved");
      // corners (and midges)
      {
        t3::Cubie s;
        if (!t3::fromTC(cube.tc, odd, s)) die("cannot read the corners" + std::string(odd ? " and midges" : ""));
        int tw = 0, fl = 0;
        for (int i = 0; i < 8; i++) tw += s.co[i];
        for (int i = 0; i < 12; i++) fl += s.eo[i];
        auto par = [](const int8_t *p, int n) { int c = 0; for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) c += p[j] < p[i]; return c & 1; };
        if (tw % 3 || (odd && ((fl & 1) || par(s.cp, 8) != par(s.ep, 12)))) die("corners/midges not a legal position");
        std::vector<cc::Mv> P;
        bool ok;
        if (odd) { t3::TwoPhase tp; ok = tp.solve(s, P); }
        else ok = t3::solveCorners(s, P);
        if (!ok) die("corner solve failed");
        const int dep[2] = {0, 1};
        playDep(P, dep);
        for (int k = 0; k < 24; k++) if (cube.tc.corner.a[k] != k / 4 || (odd && cube.tc.edge.a[k] != k / 4)) die("corners not solved");
        cnt[3] = P.size();
        if ((int)P.size() > Cb) fails++;
        if (detail) printf("cube %d %s: %zu\n", cu + 1, odd ? "corners and midges" : "corners", P.size());
        flushSeg();
      }
      // wings
      for (int d = 2; d <= My + 1; d++) {
        const int dep[3] = {0, 1, d};
        std::string parts;
        int tot = 0;
        for (int s = 0; s < 4; s++) {
          int pinv[24];
          auto rd = [&] { for (int k = 0; k < 24; k++) pinv[phiWinv[cube.wingorbits[d - 2].a[phiW[k]]]] = k; };
          rd();
          uint64_t r;
          if (!wing[s].rankOf(pinv, r)) die("wing depth " + std::to_string(d) + ": not at the " + wing[s].name + " start");
          const auto P = wing[s].line(r);
          playDep(P, dep);
          rd();
          if (!wing[s].atGoal(pinv)) die("wing depth " + std::to_string(d) + ": " + wing[s].name + " goal not reached");
          parts += std::string(s ? " + " : "") + std::to_string(wing[s].K) + " wings " + std::to_string(P.size());
          tot += P.size();
        }
        cnt[4] += tot;
        if (tot > Wb) fails++;
        if (detail) printf("cube %d wing %d: %s = %d\n", cu + 1, d, parts.c_str(), tot);
        flushSeg();
      }
      // the whole cube
      for (int y = 1; y <= My; y++)
        for (int x = 1; x <= Mx; x++) {
          uint8_t X[24];
          readOrbitPhys(cube, x, y, X);
          for (int j = 0; j < 24; j++) if (X[j] != j / 4) die("a centre orbit is not solved at the end");
        }
      for (int i = 0; i < My; i++) for (int j = 0; j < 24; j++) if (cube.wingorbits[i].a[j] != j) die("a wing orbit is not solved at the end");
      for (int k = 0; k < 24; k++) if (cube.tc.corner.a[k] != k / 4 || (odd && cube.tc.edge.a[k] != k / 4)) die("corners or midges not solved at the end");
      for (int f = 0; f < 6; f++) if (cube.ccs[f] != f) die("face centres not home at the end");
      const long tot = cnt[0] + cnt[1] + cnt[2] + cnt[3] + cnt[4];
      worstCube = std::max(worstCube, tot);
      if (tot > polyBound) fails++;
      printf("cube %d solved: obliques %ld + mids %ld + diagonals %ld + %s %ld + wings %ld = %ld moves (bound %ld)\n", cu + 1, cnt[0], cnt[1], cnt[2], odd ? "corners and midges" : "corners", cnt[3], cnt[4], tot, polyBound);
      if ((long)gRec.size() != tot) die("recorded " + std::to_string(gRec.size()) + " moves, counted " + std::to_string(tot));
      if (verify) {
        // the independent sticker simulator (sim/cube.h): the start state (the scramble replayed, else the
        // start facelets), then the solution; solved = every face one colour
        Cube sim(N);
        std::vector<uint8_t> col(sim.pos.size());
        for (size_t i = 0; i < col.size(); i++) col[i] = sim.face[i];
        auto playSim = [&](const std::vector<mv> &ms) {
          for (auto m : ms) {
            const auto &Pm = sim.movePerm[m.face][m.dep - 1];
            for (int k = 0; k < m.twist; k++) {
              std::vector<uint8_t> nc(col.size());
              for (size_t i = 0; i < col.size(); i++) nc[Pm[i]] = col[i];
              col.swap(nc);
            }
          }
        };
        if (fromMoves) playSim(scramble);
        else {
          std::string fs;
          for (char ch : startFacelets) if (ch > ' ') fs += ch;
          for (int kf = 0, k = 0; kf < 6; kf++)
            for (int r = 0; r < N; r++)
              for (int c = 0; c < N; c++, k++) col[sim.kociemba(kf, r, c)] = face_of_char(fs[k]);
        }
        playSim(gRec);
        int faceCol[6] = {-1, -1, -1, -1, -1, -1}, seen = 0;
        long wrong = 0;
        for (size_t i = 0; i < col.size(); i++) {
          int &fc = faceCol[sim.face[i]];
          if (fc < 0) fc = col[i], seen |= 1 << col[i];
          else if (fc != col[i]) wrong++;
        }
        const bool ok = !wrong && seen == 63;
        printf("cube %d independent sticker-simulator check: %s (%zu moves replayed", cu + 1, ok ? "SOLVED" : "NOT SOLVED", gRec.size());
        if (wrong) printf(", %ld stickers off their face's colour", wrong);
        printf(")\n");
        if (!ok) fails++;
      }
    }
    tRest.add(cR0, ctrNow());
    fflush(stdout);
  }
  if (timing) {
    printCtr("oblique tables (load/rebuild)", tLoadObl);
    if (bound) printCtr("boundary actions (build)", tActs);
    if (!oblOnly) printCtr("mid/diagonal/wing/corner tables", tLoadRest);
    for (int ph = 0; ph < 2 && doObl; ph++) {
      const char *nm = ph ? "3ax + 4op" : "1ud + 2ud";
      char buf[64];
      snprintf(buf, sizeof buf, "phase %d (%s), all pairs", ph + 1, nm);
      printCtr(buf, tPh[ph]);
      for (int grp = 0; grp < 2; grp++) {  // not searched, searched
        std::vector<double> t, in, cy;
        for (size_t i = 0; i < pairT[ph].size(); i++)
          if (pairS[ph][i] == grp) t.push_back(pairT[ph][i]), in.push_back(pairI[ph][i]), cy.push_back(pairC[ph][i]);
        if (t.empty()) { printf("# timing   %s, %s: 0 pairs\n", nm, grp ? "searched" : "not searched"); continue; }
        auto mean = [](const std::vector<double> &v) { double x = 0; for (double y : v) x += y; return x / v.size(); };
        std::vector<double> st = cy;
        std::sort(st.begin(), st.end());
        auto q = [&](double f) { return st[std::min(st.size() - 1, (size_t)(f * st.size()))]; };
        printf("# timing   %s, %s: %zu pairs; per pair mean %.3f ms, %.2f M instr, %.2f M cycles; cycles median %.2f M, 90%% %.2f M, 99%% %.2f M, max %.2f M\n",
               nm, grp ? "searched" : "not searched", t.size(), mean(t) * 1e3, mean(in) * 1e-6, mean(cy) * 1e-6, q(0.5) * 1e-6, q(0.9) * 1e-6, q(0.99) * 1e-6, st.back() * 1e-6);
      }
    }
    if (!oblOnly) printCtr("rest of the cube (solve)", tRest);
    CtrSum all;
    all.add(cStart, ctrNow());
    printCtr("total", all);
  }
  if (doObl) {
    printf("pairs solved %zu, worst %d, mean %.2f\n", pairs.size() * CUBES, worst, (double)sumAll / (pairs.size() * CUBES));
    printf("totals:");
    for (int t = 0; t < 200; t++) if (hist[t]) printf(" %d:%ld", t, hist[t]);
    printf("\n");
    if (bound)
      printf("pairs searched (table lines over the budget): 1ud + 2ud %ld, 3ax + 4op %ld, of %zu\n", searched[0], searched[1], pairs.size() * CUBES);
  }
  if (!oblOnly) printf("cubes solved %d, most moves %ld, bound %ld\n", CUBES, worstCube, polyBound);
  printf("over a bound: %ld (%.0fs)\n", fails, now() - t0);
  return fails ? 2 : 0;
}
