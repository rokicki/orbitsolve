// axbx: boundary search between stages 3ax and 4op of the oblique chiral pair (the axis split;
// 2026-11-01, the 3ax/4op counterpart of boundx).
//
// Stages (as axcheck): 3ax takes "the pair's U and D pieces home" to "also every side piece on its
// own axis" (F and B pieces one label, L and R another); 4op solves the rest.  A 3ax state is 32
// bits, one per side slot (bit = an F/B piece), C(16,8)^2 states; a 4op state one bit per side slot
// (bit = F on F/B slots, L on L/R slots), 70^4 states.  Both tables are dense over every state,
// filled from the certificate lines (each line's position and its 16 stage-symmetric images).
//
// For every 3ax class whose line L0 is longer than X - worst(4op), the 4op states s with
// |L0| + d4(s) > X are bad for L0: the whole position that L0 takes to s.  Alternative 3ax
// solutions P of the class's state are tried on each bad position, cheapest first: the stage-
// symmetric copies of L0 that fix the state, and an action a followed by the certificate line of
// the state a leads to (cost |a| + d3, at most |L0| + --slack).  The position is fine when some P
// has |P| + d4(where P takes it) <= X.  Actions: the 18 outer face turns and the images (16 stage
// symmetries times inverse... all 48 cube symmetries, then those keeping U and D home) of the pool
// algorithms of at most --prop C moves that keep the pair's U and D pieces home.
//
//   axbx STAGEDIR_3AX STAGEDIR_4OP X [--prop C] [--slack L] [--deep S] [--threads T] [--out FILE]
//        [--sample K] [--only FILE] [--left FILE] [--check N] [--base POOL]...
// --out FILE: the boundary blocks (axbcheck's input): per class checked, its line L0 and then every
//   alternative that covers something, one sequence per line, blocks separated by an empty line.
// --deep S: for a class with positions left, also two actions then the line, where the first action
//   alone comes within S moves of the cost allowed (slack as --slack).
// --check N: for about N classes, every covered position is replayed on the 48 pair facelets
//   (whole colouring, independent of the bit maps).
// --sample K: every K-th class only (timing).  --only FILE: only classes whose line is in FILE.
// --left FILE: the lines of the classes not covered, one per line (for a deeper rerun with --only).
// --status S: a progress line on stderr every S seconds (default 600; 0 = none).
#include "certcube.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <dirent.h>
#include <condition_variable>
#include <ctime>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static inline uint8_t mvByte(const cc::Mv &m) { return m.face << 4 | m.depth << 2 | m.amt; }
static inline cc::Mv byteMv(uint8_t b) { return {b >> 4, b >> 2 & 3, b & 3}; }

// geometry: 48 pair facelets k = face * 8 + orbit * 4 + i; moves in dst form (piece at k goes to mp[..][k])
static int mp[6][4][4][48];
static int faceOfK(int k) { return k / 8; }
static int orbitOfK(int k) { return (k % 8) / 4; }
static int groupOf(int k) { return orbitOfK(k) * 6 + faceOfK(k); }
static const int axisOf[6] = {0, 1, 2, 1, 2, 0};  // U L F R B D
static bool side(int f) { return axisOf[f] != 0; }
static int slots[32], slotOf[48];
// key bit of group g sitting in slot k, for stage st (0 = 3ax, 1 = 4op); -1: not a stage start
static int bitOf(int st, int g, int k) {
  if (g / 6 != orbitOfK(k)) return -1;
  const int f = g % 6, fk = faceOfK(k);
  if (!side(f)) return -1;
  if (st == 0) return axisOf[f] == 2;
  if (axisOf[f] != axisOf[fk]) return -1;
  return f == 2 || f == 1;
}
// key of a colouring (groups per facelet; U/D facelets must hold their own groups); 0xFFFFFFFF if not a start
static uint32_t keyOf(int st, const int8_t *col) {
  uint32_t x = 0;
  for (int k = 0; k < 48; k++) {
    if (!side(faceOfK(k))) { if (col[k] != groupOf(k)) return 0xFFFFFFFFu; continue; }
    const int b = bitOf(st, col[k], k);
    if (b < 0) return 0xFFFFFFFFu;
    x |= (uint32_t)b << slotOf[k];
  }
  return x;
}
// a representative colouring of a key (side slots: a group with that bit)
static void colOf(int st, uint32_t x, int8_t *col) {
  for (int k = 0; k < 48; k++) {
    if (!side(faceOfK(k))) { col[k] = groupOf(k); continue; }
    const int b = x >> slotOf[k] & 1, o = orbitOfK(k), ax = axisOf[faceOfK(k)];
    if (st == 0) col[k] = o * 6 + (b ? 2 : 1);
    else col[k] = o * 6 + (ax == 2 ? (b ? 2 : 4) : (b ? 1 : 3));
  }
}
// ranks: 3ax per orbit the 16 slot bits (colex rank of an 8-subset); 4op per orbit and axis 8 bits (4-subset)
static int16_t rk16[65536], rk8[256];
static uint16_t ur16[12870];
static uint8_t ur8[70];
static uint32_t orbBits(uint32_t x, int o) {  // 16 bits: face slots L F R B, 4 each
  uint32_t r = 0;
  for (int fi = 0; fi < 4; fi++) r |= (x >> (fi * 8 + o * 4) & 15) << (fi * 4);
  return r;
}
static uint64_t rankA(uint32_t x) {
  const int a = rk16[orbBits(x, 0)], b = rk16[orbBits(x, 1)];
  if (a < 0 || b < 0) return ~0ull;
  return (uint64_t)a * 12870 + b;
}
static const uint64_t NA = 12870ull * 12870, NB = 70ull * 70 * 70 * 70;
// 4op: per orbit, F/B slots (faces F, B = fi 1, 3) and L/R slots (L, R = fi 0, 2), 8 bits each
static uint32_t axBits(uint32_t x, int o, int ax) {
  const int f1 = ax == 2 ? 1 : 0, f2 = ax == 2 ? 3 : 2;
  return (x >> (f1 * 8 + o * 4) & 15) | (x >> (f2 * 8 + o * 4) & 15) << 4;
}
static uint64_t rankB(uint32_t x) {
  uint64_t r = 0;
  for (int o = 0; o < 2; o++)
    for (int ax : {2, 1}) {
      const int v = rk8[axBits(x, o, ax)];
      if (v < 0) return ~0ull;
      r = r * 70 + v;
    }
  return r;
}
static uint32_t unrankB(uint64_t r) {
  int v[4];
  for (int i = 3; i >= 0; i--) v[i] = r % 70, r /= 70;
  uint32_t x = 0;
  int i = 0;
  for (int o = 0; o < 2; o++)
    for (int ax : {2, 1}) {
      const uint32_t b = ur8[v[i++]];
      const int f1 = ax == 2 ? 1 : 0, f2 = ax == 2 ? 3 : 2;
      x |= (b & 15) << (f1 * 8 + o * 4) | (b >> 4 & 15) << (f2 * 8 + o * 4);
    }
  return x;
}

// the 16 cube symmetries that keep the stages (U-D axis kept): facelet map, group map, face map,
// mirror flag, and key maps for both stages
struct Sym { int fac[48], gmap[12], face[6]; bool flip; uint32_t tab[2][4][256]; };
static std::vector<Sym> syms;
static uint32_t imageKey(const Sym &y, int st, uint32_t x) { return y.tab[st][0][x & 255] | y.tab[st][1][x >> 8 & 255] | y.tab[st][2][x >> 16 & 255] | y.tab[st][3][x >> 24]; }
static std::vector<cc::Mv> symMoves(const Sym &y, const std::vector<cc::Mv> &mv) {
  std::vector<cc::Mv> r;
  for (auto &m : mv) r.push_back({y.face[m.face], m.depth, y.flip ? 4 - m.amt : m.amt});
  return r;
}
static void mapOf(const std::vector<cc::Mv> &mv, int8_t *D) {
  for (int k = 0; k < 48; k++) {
    int x = k;
    for (auto &m : mv) x = mp[m.face][m.depth][m.amt][x];
    D[k] = x;
  }
}
// the position a line solves: the stage goal run backwards
static void positionOf(const std::vector<cc::Mv> &mv, int8_t *col) {
  int8_t b[48];
  for (int k = 0; k < 48; k++) col[k] = groupOf(k);
  for (auto it = mv.rbegin(); it != mv.rend(); ++it) {
    for (int k = 0; k < 48; k++) b[mp[it->face][it->depth][4 - it->amt][k]] = col[k];
    memcpy(col, b, 48);
  }
}
// merges: turns of one layer, across turns that commute with them on the centres (same axis;
// outer with outer; inner slices of one depth)
static int axisOfFace(int f) { return axisOf[f]; }
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
      const bool com = axisOfFace(o[i].face) == axisOfFace(m.face) || o[i].depth == m.depth;
      if (!com) break;
    }
    if (!merged) o.push_back(m);
  }
  v.swap(o);
}
static std::string text(const std::vector<cc::Mv> &mv) {
  std::string s;
  for (auto &m : mv) { if (!s.empty()) s += ' '; s += cc::mvText(m); }
  return s;
}

struct Stage {
  int st;  // 0 = 3ax, 1 = 4op
  std::string name;
  uint64_t total;
  std::vector<uint8_t> dist;
  std::vector<uint32_t> src;  // the line (low 28 bits) and symmetry (high 4) of a line of exactly the state's length
  std::vector<uint64_t> lineOff;
  std::vector<uint8_t> lineBytes;
  int W = 0;
  bool keepSrc = false;
  uint64_t rank(uint32_t x) const { return st == 0 ? rankA(x) : rankB(x); }
  std::vector<cc::Mv> lineAt(size_t i) const {
    std::vector<cc::Mv> mv;
    for (uint64_t j = lineOff[i]; j < lineOff[i + 1]; j++) mv.push_back(byteMv(lineBytes[j]));
    return mv;
  }
  std::vector<cc::Mv> lineOf(uint64_t r) const { return symMoves(syms[src[r] >> 28], lineAt(src[r] & 0x0FFFFFFF)); }
  bool load(const std::string &dir, int threads) {
    const double t0 = now();
    total = st == 0 ? NA : NB;
    name = st == 0 ? "3ax" : "4op";
    std::vector<std::pair<int, std::string>> fs;
    if (DIR *d = opendir(dir.c_str())) {
      while (dirent *e = readdir(d)) {
        std::string n = e->d_name;
        if (n.size() > 5 && n[0] == 'l' && n.compare(n.size() - 4, 4, ".txt") == 0) fs.push_back({atoi(n.c_str() + 1), dir + "/" + n});
      }
      closedir(d);
    }
    if (fs.empty()) { fprintf(stderr, "no lNN.txt files in the %s directory\n", name.c_str()); return false; }
    std::sort(fs.begin(), fs.end());
    for (auto &[nn, f] : fs) {
      FILE *in = fopen(f.c_str(), "r");
      if (!in) { fprintf(stderr, "cannot open a %s length file\n", name.c_str()); return false; }
      char buf[4096];
      std::vector<cc::Mv> mv;
      while (fgets(buf, sizeof buf, in)) {
        std::string l(buf);
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
        if (!cc::parseMoves(l, mv)) { fprintf(stderr, "bad %s line\n", name.c_str()); return false; }
        for (auto &m : mv) if (m.depth < 1 || m.depth > 3) { fprintf(stderr, "bad %s move depth\n", name.c_str()); return false; }
        lineOff.push_back(lineBytes.size());
        for (auto &m : mv) lineBytes.push_back(mvByte(m));
      }
      fclose(in);
    }
    lineOff.push_back(lineBytes.size());
    const size_t NL = lineOff.size() - 1;
    dist.assign(total, 0xFF);
    if (NL >= (1u << 28)) { fprintf(stderr, "too many lines\n"); return false; }
    if (keepSrc) src.assign(total, 0xFFFFFFFFu);
    std::atomic<size_t> ni{0};
    std::atomic<long> notStart{0};
    std::vector<std::thread> th;
    for (int t = 0; t < threads; t++)
      th.emplace_back([&] {
        int8_t col[48];
        for (size_t i; (i = ni++) < NL;) {
          const auto mv = lineAt(i);
          positionOf(mv, col);
          const uint32_t x = keyOf(st, col);
          if (x == 0xFFFFFFFFu || rank(x) == ~0ull) { notStart++; continue; }
          for (size_t s = 0; s < syms.size(); s++) {
            const uint64_t r = rank(imageKey(syms[s], st, x));
            // races between threads only pick which of two equal-length lines is kept
            uint8_t cur = __atomic_load_n(&dist[r], __ATOMIC_RELAXED);
            while (mv.size() < cur) {
              if (__atomic_compare_exchange_n(&dist[r], &cur, (uint8_t)mv.size(), true, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) break;
            }
          }
        }
      });
    for (auto &x : th) x.join();
    if (keepSrc) {
      // the source of each state: any line of exactly its length, line and symmetry stored together
      std::atomic<size_t> nj{0};
      std::vector<std::thread> t2;
      for (int t = 0; t < threads; t++)
        t2.emplace_back([&] {
          int8_t col[48];
          for (size_t i; (i = nj++) < NL;) {
            const auto mv = lineAt(i);
            positionOf(mv, col);
            const uint32_t x = keyOf(st, col);
            if (x == 0xFFFFFFFFu) continue;
            for (size_t s = 0; s < syms.size(); s++) {
              const uint64_t r = rank(imageKey(syms[s], st, x));
              if (dist[r] == mv.size()) __atomic_store_n(&src[r], (uint32_t)(i | s << 28), __ATOMIC_RELAXED);
            }
          }
        });
      for (auto &x : t2) x.join();
    }
    long un = 0;
    for (uint64_t r = 0; r < total; r++) {
      if (dist[r] == 0xFF) { un++; continue; }
      W = std::max<int>(W, dist[r]);
    }
    fprintf(stderr, "%s: %zu lines (%ld not a stage start), %lu states, worst %d (%.0fs)\n", name.c_str(), NL, notStart.load(), (unsigned long)total, W, now() - t0);
    if (un || notStart) { fprintf(stderr, "%s: %ld states not covered\n", name.c_str(), un); return false; }
    return true;
  }
};

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

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  fflush(stdout);
  int C = 8, T = std::max(1u, std::thread::hardware_concurrency()), SL = 2, DEEP = -1, SAMP = 1, NCHK = 0, STATUS = 600;
  std::vector<std::string> pos, base;
  std::string outFile, onlyFile, leftFile;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--prop" && i + 1 < argc) C = atoi(argv[++i]);
    else if (o == "--slack" && i + 1 < argc) SL = atoi(argv[++i]);
    else if (o == "--deep" && i + 1 < argc) DEEP = atoi(argv[++i]);
    else if (o == "--threads" && i + 1 < argc) T = atoi(argv[++i]);
    else if (o == "--out" && i + 1 < argc) outFile = argv[++i];
    else if (o == "--sample" && i + 1 < argc) SAMP = std::max(1, atoi(argv[++i]));
    else if (o == "--only" && i + 1 < argc) onlyFile = argv[++i];
    else if (o == "--left" && i + 1 < argc) leftFile = argv[++i];
    else if (o == "--check" && i + 1 < argc) NCHK = atoi(argv[++i]);
    else if (o == "--status" && i + 1 < argc) STATUS = atoi(argv[++i]);
    else if (o == "--base" && i + 1 < argc) base.push_back(argv[++i]);
    else if (o.size() > 1 && o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else pos.push_back(o);
  }
  if (pos.size() != 3) {
    fprintf(stderr, "usage: axbx STAGEDIR_3AX STAGEDIR_4OP X [--prop C] [--slack L] [--deep S] [--threads T] [--out FILE] [--sample K] [--only FILE] [--left FILE] [--check N] [--status S] [--base POOL]...\n");
    return 1;
  }
  const int X = atoi(pos[2].c_str());
  const double t0 = now();

  cc::Cube C8(8);
  const auto pf = cc::pairFacelets(C8);
  std::vector<int> fidx(C8.pos.size(), -1);
  for (int k = 0; k < 48; k++) fidx[pf[k]] = k;
  for (int f = 0; f < 6; f++)
    for (int d = 1; d <= 3; d++)
      for (int a = 1; a <= 3; a++)
        for (int k = 0; k < 48; k++) {
          int i = pf[k];
          for (int q = 0; q < a; q++) i = C8.perms[f][d - 1][i];
          if (fidx[i] < 0) { fprintf(stderr, "a move takes a pair facelet off the pair\n"); return 1; }
          mp[f][d][a][k] = fidx[i];
        }
  {
    int n = 0;
    for (int k = 0; k < 48; k++) { slotOf[k] = -1; if (side(faceOfK(k))) slotOf[k] = n, slots[n++] = k; }
  }
  {
    int n16 = 0, n8 = 0;
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
        Sym y;
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
        const int det = par[pi] * ((sg & 1) ? -1 : 1) * ((sg & 2) ? -1 : 1) * ((sg & 4) ? -1 : 1);
        y.flip = det < 0;
        bool keeps = true;
        for (int g = 0; g < 12; g++) if (side(g % 6) != side(y.gmap[g] % 6)) keeps = false;
        if (!keeps) continue;
        for (int st = 0; st < 2; st++)
          for (int b = 0; b < 4; b++)
            for (int v = 0; v < 256; v++) {
              uint32_t out = 0;
              for (int t = 0; t < 8; t++) {
                const int k = slots[b * 8 + t], bit = v >> t & 1;
                int g = -1;
                for (int h = 0; h < 12 && g < 0; h++) if (bitOf(st, h, k) == bit) g = h;
                const int b2 = bitOf(st, y.gmap[g], y.fac[k]);
                if (b2 < 0) { fprintf(stderr, "a stage symmetry leaves the stage\n"); return 1; }
                out |= (uint32_t)b2 << slotOf[y.fac[k]];
              }
              y.tab[st][b][v] = out;
            }
        syms.push_back(y);
      }
    if (syms.size() != 16) { fprintf(stderr, "expected 16 stage symmetries, got %zu\n", syms.size()); return 1; }
  }

  Stage A, B;
  A.st = 0, B.st = 1, A.keepSrc = true;
  if (!A.load(pos[0], T) || !B.load(pos[1], T)) return 1;
  // self-check: the stored source lines solve their states
  {
    long bad = 0;
    for (uint64_t r = 0; r < A.total; r += 99991) {
      int8_t col[48];
      positionOf(A.lineOf(r), col);
      const uint32_t x = keyOf(0, col);
      if (x == 0xFFFFFFFFu || A.rank(x) != r || A.lineOf(r).size() != A.dist[r]) bad++;
    }
    if (bad) { fprintf(stderr, "3ax source lines do not solve their states (%ld)\n", bad); return 1; }
  }

  // actions: outer face turns and pool images of cost <= C that keep the pair's U and D home
  struct Act { int8_t D[48]; std::vector<cc::Mv> mv; };
  std::vector<Act> acts;
  {
    std::unordered_map<std::string, size_t> idx;
    // all 48 cube symmetries for the pool images
    std::vector<std::pair<std::array<int, 6>, bool>> all48;
    {
      const int pm[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
      const int par[6] = {1, -1, -1, 1, 1, -1};
      for (int pi = 0; pi < 6; pi++)
        for (int sg = 0; sg < 8; sg++) {
          std::array<int, 6> fm;
          for (int f = 0; f < 6; f++) {
            const auto n = cc::Cube::normalOf(f);
            std::array<int, 3> q;
            for (int r = 0; r < 3; r++) q[r] = n[pm[pi][r]] * ((sg >> r & 1) ? -1 : 1);
            for (int f2 = 0; f2 < 6; f2++) if (cc::Cube::normalOf(f2) == q) fm[f] = f2;
          }
          const int det = par[pi] * ((sg & 1) ? -1 : 1) * ((sg & 2) ? -1 : 1) * ((sg & 4) ? -1 : 1);
          all48.push_back({fm, det < 0});
        }
    }
    auto add = [&](std::vector<cc::Mv> mv) {
      simplify(mv);
      if (mv.empty() || (int)mv.size() > C) return;
      Act a;
      a.mv = mv;
      mapOf(mv, a.D);
      bool id = true;
      for (int k = 0; k < 48; k++) if (a.D[k] != k) id = false;
      if (id) return;
      for (int k = 0; k < 48; k++) if (!side(faceOfK(k)) && groupOf(a.D[k]) != groupOf(k)) return;
      std::string key((const char *)a.D, 48);
      auto it = idx.find(key);
      if (it != idx.end()) { if (acts[it->second].mv.size() > mv.size()) acts[it->second].mv = mv; return; }
      idx[key] = acts.size();
      acts.push_back(a);
    };
    for (int f = 0; f < 6; f++) for (int a = 1; a <= 3; a++) add({{f, 1, a}});
    for (auto &bf : base) {
      FILE *in = fopen(bf.c_str(), "r");
      if (!in) { fprintf(stderr, "cannot read a pool\n"); return 1; }
      char buf[4096];
      std::vector<cc::Mv> mv;
      long nl = 0;
      while (fgets(buf, sizeof buf, in)) {
        if (!poolMoves(buf, mv) || (int)mv.size() > C) continue;
        nl++;
        std::vector<cc::Mv> inv;
        for (auto it = mv.rbegin(); it != mv.rend(); ++it) inv.push_back({it->face, it->depth, 4 - it->amt});
        for (auto &[fm, fl] : all48) {
          std::vector<cc::Mv> a, b;
          for (auto &m : mv) a.push_back({fm[m.face], m.depth, fl ? 4 - m.amt : m.amt});
          for (auto &m : inv) b.push_back({fm[m.face], m.depth, fl ? 4 - m.amt : m.amt});
          add(a), add(b);
        }
      }
      fclose(in);
      fprintf(stderr, "pool %s: %ld algorithms of at most %d moves, %zu actions so far\n", bf.substr(bf.rfind('/') + 1).c_str(), nl, C, acts.size());
    }
    std::stable_sort(acts.begin(), acts.end(), [](const Act &a, const Act &b) { return a.mv.size() < b.mv.size(); });
    fprintf(stderr, "%zu actions of cost <= %d keep U and D\n", acts.size(), C);
  }

  // bad 4op states, longest first; badCnt[t] = how many have d4 > t
  const int lo = std::max(0, X - A.W);
  std::vector<uint32_t> bad;
  std::vector<size_t> badCnt(256, 0);
  {
    std::vector<std::vector<uint32_t>> byLen(B.W + 1);
    for (uint64_t r = 0; r < B.total; r++) if (B.dist[r] > lo) byLen[B.dist[r]].push_back(unrankB(r));
    for (int d = B.W; d > lo; d--) {
      for (auto x : byLen[d]) bad.push_back(x);
      badCnt[d - 1] = bad.size();
    }
    for (int d = 0; d < 256; d++) if (d >= B.W) badCnt[d] = 0;
    for (int d = lo; d >= 0; d--) badCnt[d] = bad.size();
    // sanity: unrankB inverts rankB
    for (size_t i = 0; i < bad.size(); i += 997) if (B.dist[B.rank(bad[i])] <= lo) { fprintf(stderr, "4op rank error\n"); return 1; }
  }
  fprintf(stderr, "4op states with d4 > %d: %zu\n", lo, bad.size());

  // the 3ax classes to check
  std::vector<size_t> todo;
  for (size_t i = 0; i + 1 < A.lineOff.size(); i++)
    if ((int)(A.lineOff[i + 1] - A.lineOff[i]) > X - B.W) todo.push_back(i);
  if (SAMP > 1) {
    size_t k = 0;
    for (size_t i = 0; i < todo.size(); i += SAMP) todo[k++] = todo[i];
    todo.resize(k);
    fprintf(stderr, "sampling every %d-th class\n", SAMP);
  }
  if (!onlyFile.empty()) {
    std::unordered_set<std::string> want;
    FILE *f = fopen(onlyFile.c_str(), "r");
    if (!f) { fprintf(stderr, "cannot read the --only file\n"); return 1; }
    char buf[4096];
    while (fgets(buf, sizeof buf, f)) {
      std::string l(buf);
      while (!l.empty() && (l.back() == '\n' || l.back() == '\r' || l.back() == ' ')) l.pop_back();
      if (!l.empty()) want.insert(l);
    }
    fclose(f);
    size_t k = 0;
    for (size_t i = 0; i < todo.size(); i++) if (want.count(text(A.lineAt(todo[i])))) todo[k++] = todo[i];
    todo.resize(k);
    fprintf(stderr, "--only: %zu of %zu lines found\n", k, want.size());
  }
  fprintf(stderr, "3ax lines longer than %d: %zu\n", X - B.W, todo.size());

  FILE *out = nullptr, *leftOut = nullptr;
  if (!outFile.empty() && !(out = fopen(outFile.c_str(), "w"))) { fprintf(stderr, "cannot write the --out file\n"); return 1; }
  if (!leftFile.empty() && !(leftOut = fopen(leftFile.c_str(), "w"))) { fprintf(stderr, "cannot write the --left file\n"); return 1; }
  std::atomic<size_t> ni{0};
  std::atomic<long> nDup{0}, nOk{0}, nFail{0}, nBad0{0}, nLeft{0}, nPaths{0}, nChk{0}, nChkBad{0};
  const size_t stride = NCHK > 0 ? std::max<size_t>(1, todo.size() / NCHK) : 0;
  std::vector<long> leftHist(5, 0), pathsUsed(64, 0);
  std::mutex mu;
  // replay: the whole position (side pieces' groups) that L0 takes to the 4op state x; play P; the 4op state reached
  auto replay = [&](const int8_t *D0, uint32_t x, const std::vector<cc::Mv> &P) -> int {
    int8_t s[48], J0[48], J[48], b[48];
    colOf(1, x, s);
    for (int k = 0; k < 48; k++) J0[k] = side(faceOfK(k)) ? s[(int)D0[k]] : groupOf(k);
    memcpy(J, J0, 48);
    for (auto &m : P) {
      for (int k = 0; k < 48; k++) b[mp[m.face][m.depth][m.amt][k]] = J[k];
      memcpy(J, b, 48);
    }
    const uint32_t y = keyOf(1, J);
    if (y == 0xFFFFFFFFu) return -1;
    return B.dist[B.rank(y)];
  };
  // progress reporter: classes finished, covered, not covered, positions left, elapsed, rough ETA
  std::mutex smu;
  std::condition_variable scv;
  bool sdone = false;
  std::thread reporter([&] {
    if (STATUS <= 0) return;
    const double ts = now();
    std::unique_lock<std::mutex> lk(smu);
    while (!scv.wait_for(lk, std::chrono::seconds(STATUS), [&] { return sdone; })) {
      const long d = nOk.load() + nFail.load() + nDup.load();
      const double el = now() - ts;
      char hm[16]; time_t tt = time(nullptr); strftime(hm, sizeof hm, "%H:%M", localtime(&tt));
      fprintf(stderr, "status %s: %ld of %zu classes (%.1f%%), %ld covered, %ld not, %ld positions left; %.0f min, ETA %.0f min\n",
              hm, d, todo.size(), todo.size() ? 100.0 * d / todo.size() : 0.0, nOk.load(), nFail.load(), nLeft.load(), el / 60,
              d > 0 ? el * (todo.size() - d) / d / 60 : 0.0);
      fflush(stderr);
    }
  });
  std::vector<std::thread> th;
  for (int t = 0; t < T; t++)
    th.emplace_back([&] {
      int8_t u[48], yv[48], D0[48], Pinv[48];
      for (size_t qi; (qi = ni++) < todo.size();) {
        const auto L0 = A.lineAt(todo[qi]);
        positionOf(L0, u);
        const uint32_t xu = keyOf(0, u);
        const uint64_t ru = A.rank(xu);
        if (A.dist[ru] != L0.size()) { nDup++; continue; }
        const int c0 = L0.size();
        const int thr = X - c0;
        if (thr >= B.W) { nOk++; continue; }
        mapOf(L0, D0);
        // u as labels: side pieces F/B or L/R (A colouring), U/D home
        int8_t ul[48];
        colOf(0, xu, ul);
        struct Path { int c; uint32_t tab[4][256]; std::vector<cc::Mv> mv; };
        std::vector<Path> paths;
        std::unordered_set<std::string> seen;
        seen.insert(std::string((const char *)D0, 48));
        auto addPath = [&](std::vector<cc::Mv> mv) {
          simplify(mv);
          if ((int)mv.size() >= X) return;
          int8_t D[48];
          mapOf(mv, D);
          if (!seen.insert(std::string((const char *)D, 48)).second) return;
          Path p;
          p.c = mv.size();
          p.mv = mv;
          // G = D0 o P^-1 on the side slots: the 4op bit at slot x after P is the bit at slot G[x] after L0
          for (int k = 0; k < 48; k++) Pinv[(int)D[k]] = k;
          int dstOf[32];
          for (int x = 0; x < 32; x++) {
            const int g = D0[(int)Pinv[slots[x]]];
            if (slotOf[g] < 0) { fprintf(stderr, "a path moves a side piece off the sides\n"); exit(1); }
            dstOf[slotOf[g]] = x;
          }
          for (int b = 0; b < 4; b++)
            for (int v = 0; v < 256; v++) {
              uint32_t o = 0;
              for (int tt = 0; tt < 8; tt++) if (v >> tt & 1) o |= 1u << dstOf[b * 8 + tt];
              p.tab[b][v] = o;
            }
          paths.push_back(std::move(p));
        };
        // stage-symmetric copies of L0 that fix the state
        for (size_t s = 0; s < syms.size(); s++) if (imageKey(syms[s], 0, xu) == xu) addPath(symMoves(syms[s], L0));
        // an action, then the line of where it leads
        for (auto &a : acts) {
          for (int k = 0; k < 48; k++) yv[(int)a.D[k]] = ul[k];
          const uint32_t xy = keyOf(0, yv);
          const uint64_t ry = A.rank(xy);
          if ((int)a.mv.size() + A.dist[ry] > c0 + SL) continue;
          auto mv = a.mv;
          const auto l = A.lineOf(ry);
          mv.insert(mv.end(), l.begin(), l.end());
          addPath(mv);
        }
        std::stable_sort(paths.begin(), paths.end(), [](const Path &a, const Path &b) { return a.c < b.c; });
        const size_t nb = badCnt[thr];
        nBad0 += nb;
        nPaths += paths.size();
        std::vector<char> hit(paths.size(), 0);
        std::vector<size_t> leftIdx;
        const bool chk = stride && qi % stride == 0;
        for (size_t i = 0; i < nb; i++) {
          const uint32_t x = bad[i];
          bool ok = false;
          for (size_t pi = 0; pi < paths.size() && !ok; pi++) {
            const auto &P = paths[pi];
            if (P.c >= X) break;
            const uint32_t y = P.tab[0][x & 255] | P.tab[1][x >> 8 & 255] | P.tab[2][x >> 16 & 255] | P.tab[3][x >> 24];
            const uint64_t ry = B.rank(y);
            if (ry == ~0ull) { fprintf(stderr, "a path leaves the 4op stage\n"); exit(1); }
            if (P.c + B.dist[ry] <= X) {
              ok = true, hit[pi] = 1;
              if (chk) {
                const int d = replay(D0, x, P.mv);
                nChk++;
                if (d < 0 || P.c + d > X) { nChkBad++; fprintf(stderr, "check failed: class %zu\n", todo[qi]); }
              }
            }
          }
          if (!ok) leftIdx.push_back(i);
        }
        long left = leftIdx.size();
        int used = 0;
        for (char h : hit) used += h;
        std::vector<size_t> deepUsed;
        if (left && DEEP >= 0) {
          const size_t first = paths.size();
          for (auto &a1 : acts) {
            int8_t y1[48];
            for (int k = 0; k < 48; k++) y1[(int)a1.D[k]] = ul[k];
            const int c1 = (int)a1.mv.size() + A.dist[A.rank(keyOf(0, y1))];
            if (c1 > c0 + SL + DEEP) continue;
            for (auto &a2 : acts) {
              if ((int)(a1.mv.size() + a2.mv.size()) > c0 + SL) break;
              for (int k = 0; k < 48; k++) yv[(int)a2.D[k]] = y1[k];
              const uint64_t ry = A.rank(keyOf(0, yv));
              if ((int)(a1.mv.size() + a2.mv.size()) + A.dist[ry] > c0 + SL) continue;
              auto mv = a1.mv;
              mv.insert(mv.end(), a2.mv.begin(), a2.mv.end());
              const auto l = A.lineOf(ry);
              mv.insert(mv.end(), l.begin(), l.end());
              addPath(mv);
            }
          }
          std::stable_sort(paths.begin() + first, paths.end(), [](const Path &a, const Path &b) { return a.c < b.c; });
          for (size_t pi = first; pi < paths.size() && !leftIdx.empty(); pi++) {
            const auto &P = paths[pi];
            std::vector<size_t> still;
            for (size_t i : leftIdx) {
              const uint32_t x = bad[i];
              const uint32_t y = P.tab[0][x & 255] | P.tab[1][x >> 8 & 255] | P.tab[2][x >> 16 & 255] | P.tab[3][x >> 24];
              if (P.c + B.dist[B.rank(y)] > X) { still.push_back(i); continue; }
              if (NCHK > 0) {
                const int d = replay(D0, x, P.mv);
                nChk++;
                if (d < 0 || P.c + d > X) { nChkBad++; fprintf(stderr, "deep check failed: class %zu\n", todo[qi]); }
              }
            }
            if (still.size() < leftIdx.size()) used++, deepUsed.push_back(pi);
            leftIdx.swap(still);
          }
          left = leftIdx.size();
        }
        nLeft += left;
        if (left) nFail++;
        else nOk++;
        std::string blk;
        if (out) {
          blk = text(L0) + "\n";
          for (size_t pi = 0; pi < hit.size(); pi++) if (hit[pi]) blk += text(paths[pi].mv) + "\n";
          for (size_t pi : deepUsed) blk += text(paths[pi].mv) + "\n";
          blk += "\n";
        }
        std::lock_guard<std::mutex> g(mu);
        if (out) fputs(blk.c_str(), out);
        if (leftOut && left) fprintf(leftOut, "%s\n", text(L0).c_str());
        leftHist[left == 0 ? 0 : left < 10 ? 1 : left < 100 ? 2 : left < 1000 ? 3 : 4]++;
        pathsUsed[std::min(used, 63)]++;
      }
    });
  for (auto &x : th) x.join();
  { std::lock_guard<std::mutex> g(smu); sdone = true; }
  scv.notify_all();
  reporter.join();
  if (out) fclose(out);
  if (leftOut) fclose(leftOut);
  fprintf(stderr, "classes: %ld covered, %ld not (%ld duplicate lines skipped); bad positions %ld, left %ld; %.1f alternatives per class (%.0fs)\n",
          nOk.load(), nFail.load(), nDup.load(), nBad0.load(), nLeft.load(), todo.size() ? (double)nPaths.load() / todo.size() : 0.0, now() - t0);
  fprintf(stderr, "classes by positions left: 0: %ld, 1-9: %ld, 10-99: %ld, 100-999: %ld, 1000+: %ld\n", leftHist[0], leftHist[1], leftHist[2], leftHist[3], leftHist[4]);
  fprintf(stderr, "alternatives used per class:");
  for (int i = 0; i < 64; i++) if (pathsUsed[i]) fprintf(stderr, " %d:%ld", i, pathsUsed[i]);
  fprintf(stderr, "\n");
  if (NCHK) fprintf(stderr, "check: %ld positions replayed, %ld failed\n", nChk.load(), nChkBad.load());
  printf("3ax 4op X=%d: %ld of %zu classes not covered\n", X, nFail.load(), todo.size() - nDup.load());
  return 0;
}
