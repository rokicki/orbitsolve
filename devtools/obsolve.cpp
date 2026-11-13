// obsolve: solve every oblique centre orbit of an N x N x N cube, pair by pair, from the stage
// certificates alone ("the proof is in the pudding").  It reads only the small certificate
// (smallcert.py: per stage the lines not one stage move from a shorter class) and rebuilds the full
// stage tables in memory at startup: every state of each stage gets its distance by a breadth-first
// pass over the stage moves from the kept lines' states (and their stage-symmetric images), and its
// line is the kept line or a stage move followed by the line of a state one closer.
//
// The cube is brobdicube's xcube (gullicube's src/brob, linked unmodified); centre orbit (x,y) is
// rows[y-1][x-1].  A certificate line acts on xorbits (1,2)/(2,1) with layer depths 1..3; for the
// chiral pair (a,b)/(b,a) its depths 2 and 3 become a+1 and b+1.  Order (methods.md): 1ud then 2ud
// for every pair (other obliques keep their U and D faces), then 3ax then 4op for every pair (other
// obliques keep their colours).
//
// Default: every stage plays its table line (the plain sum, P <= 80).  --bound: at each boundary
// (1ud/2ud and 3ax/4op) the first stage's solution is chosen as the boundary finders choose theirs
// (boundx, axbx): the table line, its stage-symmetric copies, an action (outer turn or a pool
// algorithm image) followed by the table line of where it leads, within --slack of the line, and if
// that is not enough two actions; the one with the least |P| + d(second stage) is played.  The bound
// it must meet is --ud X (default 33) and --ax X (default 40).
//
// Checks: the layout and depth substitution are tested on the given N at startup (random sequences
// on the cube against the pair-facelet simulation); after every stage the pair is re-read from the
// cube and must be at that stage's goal; after phase 1 every pair has U and D home, at the end every
// oblique orbit is solved; a chosen alternative must keep the other obliques as its stage requires.
//
//   obsolve N [--cert DIR] [--bound] [--ud X] [--ax X] [--cubes K] [--seed S] [-r LEN] [--threads T]
//           [--prop C] [--slack L] [--deep S] [--deepprop C] [--base POOL]... [--moves]
#include "certcube.h"
#include "xcube.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <dirent.h>
#include <random>
#include <thread>
#include <unordered_map>
#include <unordered_set>

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
      for (auto &m : moves) {
        applyG(m, g, h);
        uint64_t r2;
        if (rankG(h, r2) && dist[r2] == d - 1) { out.push_back(m); memcpy(g, h, 48); r = r2; found = true; break; }
      }
      if (!found) { fprintf(stderr, "%s: no stage move one closer from a state at %d\n", name.c_str(), d); exit(1); }
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
    for (auto &f : fs) {
      FILE *in = fopen(f.c_str(), "r");
      if (!in) { fprintf(stderr, "%s: cannot open a line file\n", name.c_str()); return false; }
      char buf[4096];
      std::vector<cc::Mv> mv;
      while (fgets(buf, sizeof buf, in)) {
        std::string l(buf);
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
        if (!cc::parseMoves(l, mv)) { fprintf(stderr, "%s: bad line\n", name.c_str()); return false; }
        for (auto &m : mv) if (m.depth > 3) { fprintf(stderr, "%s: bad move depth\n", name.c_str()); return false; }
        lineOff.push_back(lineBytes.size());
        for (auto &m : mv) lineBytes.push_back(mvByte(m));
      }
      fclose(in);
    }
    lineOff.push_back(lineBytes.size());
    const size_t NL = lineOff.size() - 1;
    if (NL >= (1u << 26)) { fprintf(stderr, "%s: too many lines\n", name.c_str()); return false; }
    dist.assign(total, 0xFF), src.assign(total, 0xFFFFFFFFu);
    std::atomic<long> bad{0}, clash{0};
    std::vector<std::thread> th;
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
    long kept = 0;
    for (uint64_t r = 0; r < total; r++) if (dist[r] != 0xFF) kept++;
    // breadth-first over the stage moves: a state one move from a state at d is at most d + 1
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
              for (auto &m : moves) {
                applyG(m, g, h);
                uint64_t r2;
                if (!rankG(h, r2)) continue;
                if (__atomic_load_n(&dist[r2], __ATOMIC_RELAXED) > d + 1) __atomic_store_n(&dist[r2], (uint8_t)(d + 1), __ATOMIC_RELAXED);
              }
            }
          atD += n;
        });
      for (auto &x : th) x.join();
      th.clear();
      if (!atD) {
        bool more = false;
        for (uint64_t r = 0; r < total && !more; r++) if (dist[r] != 0xFF && dist[r] > d) more = true;
        if (!more) break;
      }
    }
    long un = 0, hist[64] = {};
    for (uint64_t r = 0; r < total; r++) {
      if (dist[r] == 0xFF) { un++; continue; }
      W = std::max<int>(W, dist[r]);
      hist[std::min<int>(dist[r], 63)]++;
    }
    fprintf(stderr, "%s: %zu kept lines (%ld states), %zu stage moves, %zu symmetries; rebuilt %lu states, worst %d (%.0fs):",
            name.c_str(), NL, kept, moves.size(), ss.size(), (unsigned long)total, W, now() - t0);
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
struct Choice { int c0 = 0, total = 0, len = 0, kind = 0; std::vector<cc::Mv> P; long tried = 0, rejected = 0; };
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
static void play(xcube &c, int a, int b, const std::vector<cc::Mv> &mv) {
  for (auto &m : mv) domove({m.depth == 1 ? 1 : m.depth == 2 ? a + 1 : b + 1, (short)m.face, (short)m.amt}, c, true);
}

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  fflush(stdout);
  int N = 0, CUBES = 1, T = 8, C = 10, SL = 6, DEEP = 6, DP = 8, XUD = 33, XAX = 40;
  long scr = 0;
  uint64_t seed = 0;
  bool bound = false, showMoves = false;
  std::string certDir = std::string(getenv("HOME") ? getenv("HOME") : ".") + "/orbitsolve-certsafe/obl23pair-axis-2026-11-04-small";
  std::vector<std::string> base, pos;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--cert" && i + 1 < argc) certDir = argv[++i];
    else if (o == "--bound") bound = true;
    else if (o == "--ud" && i + 1 < argc) XUD = atoi(argv[++i]);
    else if (o == "--ax" && i + 1 < argc) XAX = atoi(argv[++i]);
    else if (o == "--cubes" && i + 1 < argc) CUBES = atoi(argv[++i]);
    else if (o == "--seed" && i + 1 < argc) seed = strtoull(argv[++i], nullptr, 10);
    else if (o == "-r" && i + 1 < argc) scr = atol(argv[++i]);
    else if (o == "--threads" && i + 1 < argc) T = atoi(argv[++i]);
    else if (o == "--prop" && i + 1 < argc) C = atoi(argv[++i]);
    else if (o == "--slack" && i + 1 < argc) SL = atoi(argv[++i]);
    else if (o == "--deep" && i + 1 < argc) DEEP = atoi(argv[++i]);
    else if (o == "--deepprop" && i + 1 < argc) DP = atoi(argv[++i]);
    else if (o == "--base" && i + 1 < argc) base.push_back(argv[++i]);
    else if (o == "--moves") showMoves = true;
    else if (o.size() > 1 && o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else pos.push_back(o);
  }
  if (pos.size() != 1 || (N = atoi(pos[0].c_str())) < 6) {
    fprintf(stderr, "usage: obsolve N [--cert DIR] [--bound] [--ud X] [--ax X] [--cubes K] [--seed S] [-r LEN] [--threads T] [--prop C] [--slack L] [--deep S] [--deepprop C] [--base POOL]... [--moves]\n"
                    "  N >= 6.  Default certificate: ~/orbitsolve-certsafe/obl23pair-axis-2026-11-04-small.\n");
    return 1;
  }
  if (bound && base.empty()) {
    const std::string d = std::string(getenv("HOME") ? getenv("HOME") : ".") + "/orbitsolve/dist/gullicube/data/";
    for (auto f : {"pair3.cls", "p8s10.cls", "p9.cls", "eg8.cls", "c12x.cls"}) base.push_back(d + f);
  }
  if (!seed) {
    std::random_device rd;
    seed = ((uint64_t)rd() << 32 ^ rd()) % 1000000000000ULL;
    if (!seed) seed = 1;
  }
  printf("# seed %llu\n", (unsigned long long)seed);
  fflush(stdout);
  const double t0 = now();

  // geometry
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

  // the stages, rebuilt from the small certificate
  Stage st[4];
  const char *sub[4] = {"none..aUbU", "aUbU..aDbD", "3ax", "4op"};
  for (int k = 0; k < 4; k++) {
    st[k].init(k);
    if (!st[k].load(certDir + "/stages/" + sub[k], T)) return 1;
  }
  fprintf(stderr, "tables rebuilt (%.0fs)\n", now() - t0);
  std::vector<Act> actsUD, actsAX;
  if (bound) {
    const double ta = now();
    if (!buildActs(base, C, false, actsUD) || !buildActs(base, C, true, actsAX)) return 1;
    fprintf(stderr, "actions of at most %d moves: 1ud %zu, 3ax %zu (%.0fs)\n", C, actsUD.size(), actsAX.size(), now() - ta);
  }

  rng.seed(seed);
  const int My = (N - 2) / 2, Mx = N - 2 - My;
  std::vector<std::pair<int, int>> pairs;
  for (int a = 1; a <= My; a++) for (int b = a + 1; b <= My; b++) pairs.push_back({a, b});
  // layout self-test on this N: random stage-depth sequences on the cube against the pair simulation
  // (one random cube, up to 100 pairs spread over all of them)
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
    if (bad) { fprintf(stderr, "layout self-test failed on %ld pairs\n", bad); return 1; }
  }
  printf("# N=%d: %zu oblique pairs; mode %s\n", N, pairs.size(), bound ? "boundary choice" : "table lines (plain sum)");
  if (bound) printf("# bounds: 1ud + 2ud <= %d, 3ax + 4op <= %d; actions of at most %d moves, slack %d, deep %d (second action at most %d)\n", XUD, XAX, C, SL, DEEP, DP);

  int worst = 0;
  long fails = 0, sumAll = 0;
  std::vector<long> hist(200, 0);
  for (int cu = 0; cu < CUBES; cu++) {
    std::vector<mv> scramble;
    for (long i = 0; i < scr; i++) scramble.push_back({1 + (int)(rng() % ((N + 1) / 2)), (short)(rng() % 6), (short)(1 + rng() % 3)});
    moveset = scramble;
    xcube cube(Mx, My, scr ? 'I' : 'R');
    moveset.clear();
    std::vector<std::array<int, 4>> len(pairs.size());
    std::vector<std::vector<cc::Mv>> sol(pairs.size());
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
        int8_t G[48];
        readPair(cube, a, b, G);
        std::vector<cc::Mv> P;
        if (bound) {
          const Choice ch = choose(A, B, ph ? actsAX : actsUD, G, ph ? XAX : XUD, SL, DEEP, DP);
          P = ch.P;
          if (ch.kind) printf("  pair (%d,%d) %s: line %d, alternative (%s) %d, total with %s %d\n", a, b, A.name.c_str(), ch.c0,
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
        len[pi][2 * ph] = P.size(), len[pi][2 * ph + 1] = Q.size();
        sol[pi].insert(sol[pi].end(), P.begin(), P.end());
        sol[pi].insert(sol[pi].end(), Q.begin(), Q.end());
      }
      // phase 1: every pair's U and D home; phase 2: every oblique orbit solved
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
      const int lim = bound ? XUD + XAX : 80;
      if (t > lim || (bound && (len[pi][0] + len[pi][1] > XUD || len[pi][2] + len[pi][3] > XAX))) fails++;
      printf("cube %d pair (%d,%d): 1ud %d + 2ud %d + 3ax %d + 4op %d = %d\n", cu + 1, pairs[pi].first, pairs[pi].second, len[pi][0], len[pi][1], len[pi][2], len[pi][3], t);
      if (showMoves) printf("  %s\n", text(sol[pi]).c_str());
    }
    fflush(stdout);
  }
  printf("pairs solved %zu, worst %d, mean %.2f; over the bound %ld (%.0fs)\n", pairs.size() * CUBES, worst, (double)sumAll / (pairs.size() * CUBES), fails, now() - t0);
  printf("totals:");
  for (int t = 0; t < 200; t++) if (hist[t]) printf(" %d:%ld", t, hist[t]);
  printf("\n");
  return fails ? 2 : 0;
}
