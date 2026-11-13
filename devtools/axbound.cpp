// axbound: upper bounds for stage 3ax of the oblique chiral pair (2026-10-28).  After the pair's U
// and D are solved (both orbits), 3ax puts every F/B piece on F or B and every L/R piece on L or R.
// A state is 32 bits: per orbit the 16 side slots (L F R B, 4 each; bit set = F/B piece), 8 of each
// set, so C(16,8)^2 = 165,636,900 states; 16 stage symmetries (those of the 48 that keep the U-D axis).
//
// Compact: the distance table covers every position (one byte each; a class's images all hold its
// value) and back-pointers are kept for the least image only.  Actions are stored once per
// conjugacy class under the stage symmetries (deduplicated by sorting); applying every stored
// action to every position reaches all conjugates.
//
// Actions: outer L F R B turns (cost 1), with --slices also 2U2 3U2 2D2 3D2 (cost 1; alternative A
// in methods.md: valid only when phase 3 runs on all obliques first), and every pool algorithm
// (or symmetry image) that keeps the pair's U and D; closed under inverse.  Then
//   seeds: dist(a(solved)) <= cost(a) (exact through the pools' complete depth);
//   propagation: from the shortest lengths up, the actions of cost <= --prop C from every position
//   lowered since it was last expanded (cancellation between actions is not counted, so every value
//   is an upper bound);
//   band: for classes in the top --band B lengths, dist(x) <= cost(a) + dist(a(x)) over every action,
//   then propagation again, until nothing changes (at most --rounds R rounds; --band 255 is a full
//   band over every class).  Actions are applied by byte tables (4 KB each) when all of them fit
//   in --tabmb M megabytes (default 2048), else by bit loops; propagation always uses tables.
// merge: then every class's line is rebuilt and its moves merged (turns of one layer cancel, also across turns that
// commute with them on the centres: same axis, or outer-with-outer / same-depth inner slices on other axes; 2026-11-02:
// also final outer face turns are dropped, as no-ops on the goal colouring); a line that gets shorter
// than its class's length is replayed and loaded, and propagation runs again, until no line shortens (off with
// --nomerge).
// Every worst class and --verify N random classes are rebuilt as move sequences and replayed on the
// 48 pair facelets.  Lengths are reported by position and by class; --worst FILE writes one solving
// line per worst class (with --from L, per class of length >= L).  The table persists: --save FILE
// writes one replayed solving line per class ("LEN<TAB>moves"), and --load FILE (repeatable) starts
// from such files or any 3ax solving lines (e.g. the ML agent's), so a run only lowers the best table.
// --base FILE loads a saved table as already closed (its positions are not expanded again; only what
// --load lowers afterwards is): use it only for a table saved by a run with the same pools and a
// --prop at least as high, otherwise --load.
//   axbound [--prop C] [--band B] [--rounds R] [--tabmb M] [--slices] [--verify N] [--worst FILE] [--from L] [--load FILE]...
//           [--save FILE] [--diagok] [--nomerge] [--onemove] [--threads T] POOL...
//
// Built with OP4 defined (opbound.cpp), the same engine bounds stage 4op instead: after 3ax, F vs B
// and L vs R in both orbits at once.  Per orbit 8 F/B slots (F face bits 0-3, B face 4-7; bit set =
// F piece) and 8 L/R slots (L face 8-11, R face 12-15; bit set = L piece), 4 of each set: 70^4 =
// 24,010,000 states, SOLVED = 0x0F0F0F0F; actions must also keep the axis split.
#include "certcube.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <map>
#include <random>
#include <thread>

static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static inline uint8_t mvByte(const cc::Mv &m) { return m.face << 4 | m.depth << 2 | m.amt; }
static inline cc::Mv byteMv(uint8_t b) { return {b >> 4, b >> 2 & 3, b & 3}; }
// relaxed atomics on plain vector elements (compiler builtins: armory's libc++ has no std::atomic_ref)
template <class T> static inline T aload(T &x) { return __atomic_load_n(&x, __ATOMIC_RELAXED); }
template <class T> static inline void astore(T &x, T v) { __atomic_store_n(&x, v, __ATOMIC_RELAXED); }
template <class T> static inline bool acas(T &x, T &cur, T v) { return __atomic_compare_exchange_n(&x, &cur, v, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED); }

#ifdef OP4
static constexpr bool OP = true;
static const char *const STAGE = "4op", *const TOOL = "opbound";
#else
static constexpr bool OP = false;
static const char *const STAGE = "3ax", *const TOOL = "axbound";
#endif

struct Act { std::array<uint8_t, 32> dst; uint64_t mvOff; uint8_t mvLen, cost; };

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  fflush(stdout);
  int prop = 8, band = 2, rounds = 6, tabmb = 2048, T = std::max(1u, std::thread::hardware_concurrency());
  long verifyN = 1000;
  bool diagok = false, slices = false, merge = true, onemove = false;
  int from = -1;
  std::string worstFile, saveFile;
  std::vector<std::string> pools, loads, bases;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--prop" && i + 1 < argc) prop = atoi(argv[++i]);
    else if (o == "--band" && i + 1 < argc) band = atoi(argv[++i]);
    else if (o == "--rounds" && i + 1 < argc) rounds = atoi(argv[++i]);
    else if (o == "--tabmb" && i + 1 < argc) tabmb = atoi(argv[++i]);
    else if (o == "--verify" && i + 1 < argc) verifyN = atol(argv[++i]);
    else if (o == "--worst" && i + 1 < argc) worstFile = argv[++i];
    else if (o == "--from" && i + 1 < argc) from = atoi(argv[++i]);
    else if (o == "--load" && i + 1 < argc) loads.push_back(argv[++i]);
    else if (o == "--base" && i + 1 < argc) bases.push_back(argv[++i]);
    else if (o == "--save" && i + 1 < argc) saveFile = argv[++i];
    else if (o == "--threads" && i + 1 < argc) T = atoi(argv[++i]);
    else if (o == "--slices") slices = true;
    else if (o == "--diagok") diagok = true;
    else if (o == "--nomerge") merge = false;
    else if (o == "--onemove") onemove = true;
    else if (o.size() > 1 && o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else pools.push_back(o);
  }
  if (pools.empty()) { fprintf(stderr, "usage: %s [--prop C] [--band B] [--rounds R] [--tabmb M] [--slices] [--verify N] [--worst FILE] [--from L] [--base FILE]... [--load FILE]... [--save FILE] [--diagok] [--nomerge] [--threads T] POOL...\n", TOOL); return 1; }
  const double t0 = now();

  cc::Cube C8(8);
  const auto pf = cc::pairFacelets(C8);
  std::vector<int> fidx(C8.pos.size(), -1);
  for (int k = 0; k < 48; k++) fidx[pf[k]] = k;
  static int mp[6][4][4][48];
  for (int f = 0; f < 6; f++) for (int d = 1; d <= 3; d++) for (int a = 1; a <= 3; a++)
    for (int k = 0; k < 48; k++) { int i = pf[k]; for (int q = 0; q < a; q++) i = C8.perms[f][d - 1][i]; mp[f][d][a][k] = fidx[i]; }
  // facelet k = face * 8 + orbit * 4 + i (faces U L F R B D); side faces 1..4 -> bits
  auto faceOf = [](int k) { return k / 8; };
  const int axisOf[6] = {0, 1, 2, 1, 2, 0};
  const uint32_t SOLVED = OP ? 0x0F0F0F0Fu : 0xF0F0F0F0u;
  int bitOf[48], facOfBit[32];
  for (int k = 0; k < 48; k++) {
    const int f = k / 8, o = k % 8 / 4, i = k % 4;
    if (OP) bitOf[k] = f == 2 ? o * 16 + i : f == 4 ? o * 16 + 4 + i : f == 1 ? o * 16 + 8 + i : f == 3 ? o * 16 + 12 + i : -1;
    else bitOf[k] = (f >= 1 && f <= 4) ? o * 16 + (f - 1) * 4 + i : -1;
    if (bitOf[k] >= 0) facOfBit[bitOf[k]] = k;
  }

  // the 48 cube symmetries (as opbound): facelet map, face map, mirror
  struct Sym { int fac[48]; int face[6]; bool flip; };
  std::vector<Sym> syms;
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
          auto q = map(C8.pos[pf[k]]);
          for (int x = 0; x < 48; x++) if (C8.pos[pf[x]] == q) y.fac[k] = x;
        }
        for (int f = 0; f < 6; f++) {
          auto q = map(cc::Cube::normalOf(f));
          for (int f2 = 0; f2 < 6; f2++) if (cc::Cube::normalOf(f2) == q) y.face[f] = f2;
        }
        const int det = par[pi] * ((sg & 1) ? -1 : 1) * ((sg & 2) ? -1 : 1) * ((sg & 4) ? -1 : 1);
        y.flip = det < 0;
        syms.push_back(y);
      }
  }
  // stage symmetries: bit permutation, axis swap (F/B pieces become L/R pieces), byte tables
  std::vector<int> ss;
  for (int i = 0; i < 48; i++) if (syms[i].face[0] == 0 || syms[i].face[0] == 5) ss.push_back(i);
  const int NS = ss.size();
  std::vector<std::array<int, 32>> bp(NS);
  std::vector<uint32_t> sx(NS), stab((size_t)NS * 1024);
  for (int s = 0; s < NS; s++) {
    const Sym &y = syms[ss[s]];
    for (int j = 0; j < 32; j++) bp[s][j] = bitOf[y.fac[facOfBit[j]]];
    sx[s] = 0;
    for (int b = 0; b < 4; b++)
      for (int v = 0; v < 256; v++) {
        uint32_t t = 0;
        for (int j = 0; j < 8; j++) if (v >> j & 1) t |= 1u << bp[s][b * 8 + j];
        stab[((size_t)s * 4 + b) * 256 + v] = t;
      }
  }
  auto img = [&](int s, uint32_t x) {
    const uint32_t *tb = &stab[(size_t)s * 1024];
    return (tb[x & 255] | tb[256 + (x >> 8 & 255)] | tb[512 + (x >> 16 & 255)] | tb[768 + (x >> 24)]) ^ sx[s];
  };
  // recolouring: a bit flips where the symmetry takes its colour to the other face of the axis (3ax:
  // the F/B and L/R axes swap); per axis all or nothing, so the solved state, which every stage
  // symmetry fixes, gives the mask
  for (int s = 0; s < NS; s++) sx[s] = img(s, SOLVED) ^ SOLVED;
  auto canon = [&](uint32_t x) {
    uint32_t b = x;
    for (int s = 0; s < NS; s++) b = std::min(b, img(s, x));
    return b;
  };

  // actions, one per conjugacy class under the stage symmetries
  std::vector<Act> acts;
  std::vector<uint8_t> mvs;
  long nLines = 0, nUD = 0;
  auto conjKey = [&](const std::array<uint8_t, 32> &d) {
    std::array<uint8_t, 32> best = d, c;
    for (int s = 0; s < NS; s++) {
      for (int j = 0; j < 32; j++) c[bp[s][j]] = bp[s][d[j]];
      best = std::min(best, c);
    }
    return best;
  };
  auto addMoves = [&](const std::vector<cc::Mv> &mv, int cost) {
    // the effect on the 48 pair facelets: the piece at k goes to p[k]
    int p[48];
    for (int k = 0; k < 48; k++) p[k] = k;
    for (auto &m : mv) for (int k = 0; k < 48; k++) p[k] = mp[m.face][m.depth][m.amt][p[k]];
    for (int k = 0; k < 48; k++) {
      const int f = faceOf(k);
      if ((f == 0 || f == 5) && faceOf(p[k]) != f) return false;
      if (OP && axisOf[faceOf(p[k])] != axisOf[f]) return false;
    }
    std::array<uint8_t, 32> d;
    for (int j = 0; j < 32; j++) d[j] = bitOf[p[facOfBit[j]]];
    Act a{conjKey(d), mvs.size(), (uint8_t)mv.size(), (uint8_t)cost};
    for (auto &m : mv) mvs.push_back(mvByte(m));
    acts.push_back(a);
    return true;
  };
  auto dedupe = [&] {
    std::sort(acts.begin(), acts.end(), [](const Act &a, const Act &b) { return a.dst != b.dst ? a.dst < b.dst : a.cost < b.cost; });
    acts.erase(std::unique(acts.begin(), acts.end(), [](const Act &a, const Act &b) { return a.dst == b.dst; }), acts.end());
  };
  for (int f = 1; f <= 4; f++) for (int a = 1; a <= 3; a++) addMoves({{f, 1, a}}, 1);
  if (slices) for (int f : {0, 5}) for (int d : {2, 3}) addMoves({{f, d, 2}}, 1);
  // a pool line keeps U and D after a symmetry image only if it keeps some axis's two faces; one
  // symmetry per target axis suffices (the rest are conjugates)
  std::vector<int> reps;
  for (int want : {0, 1, 2}) {
    for (int i = 0; i < 48; i++) {
      const int g = syms[i].face[want];  // where face U, L or F goes
      if (g == 0) { reps.push_back(i); break; }
    }
  }
  for (auto &fn : pools) {
    FILE *in = fopen(fn.c_str(), "r");
    if (!in) { fprintf(stderr, "cannot read %s\n", fn.c_str()); return 1; }
    char buf[4096];
    std::vector<cc::Mv> mv, m2;
    const long before = nUD;
    while (fgets(buf, sizeof buf, in)) {
      if (buf[0] == '#') continue;
      std::string l(buf);
      while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
      // "COST moves... Dn"
      const size_t sp = l.find(' '), ls = l.rfind(' ');
      if (sp == std::string::npos || ls == sp) continue;
      const std::string flag = l.substr(ls + 1);
      if (flag != "D0" && !(diagok && flag == "D1")) continue;
      if (!cc::parseMoves(l.substr(sp + 1, ls - sp - 1), mv)) { fprintf(stderr, "bad pool line in %s\n", fn.c_str()); return 1; }
      nLines++;
      for (int r : reps) {
        const Sym &y = syms[r];
        m2.clear();
        for (auto &m : mv) m2.push_back({y.face[m.face], m.depth, y.flip ? 4 - m.amt : m.amt});
        if (addMoves(m2, mv.size())) nUD++;
      }
      if (acts.size() > 40000000) {
        dedupe();
        std::vector<uint8_t> nm;  // compact the move store
        for (auto &a : acts) { const uint64_t o = nm.size(); nm.insert(nm.end(), mvs.begin() + a.mvOff, mvs.begin() + a.mvOff + a.mvLen); a.mvOff = o; }
        mvs.swap(nm);
      }
    }
    fclose(in);
    dedupe();
    fprintf(stderr, "%s: %ld lines so far, %ld keep U and D%s (+%ld), %zu actions (%.0fs)\n", fn.c_str(), nLines, nUD, OP ? " and the axes" : "", nUD - before, acts.size(), now() - t0);
  }
  // close under inverse
  {
    const size_t n = acts.size();
    for (size_t a = 0; a < n; a++) {
      std::vector<cc::Mv> mv;
      for (int i = acts[a].mvLen - 1; i >= 0; i--) { cc::Mv m = byteMv(mvs[acts[a].mvOff + i]); m.amt = 4 - m.amt; mv.push_back(m); }
      addMoves(mv, acts[a].cost);
    }
    dedupe();
    std::vector<uint8_t> nm;
    for (auto &a : acts) { const uint64_t o = nm.size(); nm.insert(nm.end(), mvs.begin() + a.mvOff, mvs.begin() + a.mvOff + a.mvLen); a.mvOff = o; }
    mvs.swap(nm);
  }
  std::sort(acts.begin(), acts.end(), [](const Act &a, const Act &b) { return a.cost < b.cost; });
  {
    std::map<int, long> byCost;
    for (auto &a : acts) byCost[a.cost]++;
    fprintf(stderr, "actions (one per conjugacy class, %d stage symmetries): %zu, by cost:", NS, acts.size());
    for (auto &kv : byCost) fprintf(stderr, " %d:%ld", kv.first, kv.second);
    fprintf(stderr, " (%.0fs)\n", now() - t0);
  }

  // ranking: per orbit 16 bits with 8 set -> 0..12869
  std::vector<int16_t> r16(65536, -1);
  std::vector<uint16_t> u16;
  for (int s = 0; s < 65536; s++)
    if (OP ? __builtin_popcount(s & 255) == 4 && __builtin_popcount(s >> 8) == 4 : __builtin_popcount(s) == 8) r16[s] = u16.size(), u16.push_back(s);
  const uint64_t NU = u16.size(), total = NU * NU;
  auto rankOf = [&](uint32_t s) { return (uint64_t)r16[s & 0xFFFF] * NU + r16[s >> 16]; };
  auto stateOf = [&](uint64_t r) { return (uint32_t)u16[r / NU] | (uint32_t)u16[r % NU] << 16; };
  auto apply = [&](const Act &a, uint32_t s) {
    uint32_t t = 0;
    for (uint32_t x = s; x; x &= x - 1) t |= 1u << a.dst[__builtin_ctz(x)];
    return t;
  };
  std::vector<uint8_t> dist(total, 0xFF);
  std::vector<uint32_t> via(total, 0xFFFFFFFF);  // least image only: action, bit 31 = its inverse
  const uint32_t INV = 0x80000000u, LOADED = 0x40000000u;  // via: action (bit 31: its inverse) or loaded line
  std::vector<uint8_t> dirty(total, 0);  // lowered since it was last expanded
  // lower a whole class: the least image decides; the rest follow
  auto lower = [&](uint32_t t, int d, uint32_t v) {
    const uint32_t c = canon(t);
    const uint64_t rc = rankOf(c);
    uint8_t cur = aload(dist[rc]);
    for (;;) {
      if (d >= cur) return false;
      if (acas(dist[rc], cur, (uint8_t)d)) break;
    }
    astore(via[rc], (uint32_t)v);
    astore(dirty[rc], (uint8_t)1);
    for (int s = 0; s < NS; s++) {
      const uint64_t ry = rankOf(img(s, c));
      uint8_t cy = aload(dist[ry]);
      while (d < cy && !acas(dist[ry], cy, (uint8_t)d)) {}
      astore(dirty[ry], (uint8_t)1);
    }
    return true;
  };
  dist[rankOf(SOLVED)] = 0;
  for (size_t a = 0; a < acts.size(); a++) lower(apply(acts[a], SOLVED), acts[a].cost, a | INV);
  // loaded lines: each solves the state it starts from (and so its whole class)
  std::vector<std::vector<cc::Mv>> loaded;
  std::vector<std::string> files = bases;
  files.insert(files.end(), loads.begin(), loads.end());
  for (size_t fi = 0; fi < files.size(); fi++) {
    const std::string &fn = files[fi];
    if (fi == bases.size() && !bases.empty()) {  // the bases are closed: nothing of theirs to expand
      std::fill(dirty.begin(), dirty.end(), 0);
      fprintf(stderr, "bases loaded as closed; expanding only what later loads lower\n");
    }
    FILE *in = fopen(fn.c_str(), "r");
    if (!in) { fprintf(stderr, "cannot read %s\n", fn.c_str()); return 1; }
    char buf[4096];
    long nl = 0, nbad = 0, nlow = 0;
    std::vector<cc::Mv> mv;
    while (fgets(buf, sizeof buf, in)) {
      if (buf[0] == '#') continue;
      std::string l(buf);
      while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
      const size_t tb = l.find('\t');
      if (tb != std::string::npos) l = l.substr(tb + 1);
      if (l.empty()) continue;
      if (!cc::parseMoves(l, mv)) { fprintf(stderr, "bad line in %s\n", fn.c_str()); return 1; }
      nl++;
      int p[48];
      for (int k = 0; k < 48; k++) p[k] = k;
      for (auto &m : mv) for (int k = 0; k < 48; k++) p[k] = mp[m.face][m.depth][m.amt][p[k]];
      // the start state: the piece at k is the colour of where it ends, faceOf(p[k])
      bool ok = true;
      uint32_t x = 0;
      for (int k = 0; k < 48 && ok; k++) {
        const int f = faceOf(k), g = faceOf(p[k]);
        if ((f == 0 || f == 5 || g == 0 || g == 5) && g != f) ok = false;
        else if (OP && axisOf[g] != axisOf[f]) ok = false;
        else if (bitOf[k] >= 0 && (OP ? (g == 2 || g == 1) : (g == 2 || g == 4))) x |= 1u << bitOf[k];
      }
      if (!ok) { nbad++; continue; }
      if (lower(x, mv.size(), LOADED | (uint32_t)loaded.size())) nlow++;
      loaded.push_back(mv);
    }
    fclose(in);
    fprintf(stderr, "loaded %s: %ld lines, %ld not %s lines (skipped), %ld classes set (%.0fs)\n", fn.c_str(), nl, nbad, STAGE, nlow, now() - t0);
  }
  if (!bases.empty() && loads.empty()) std::fill(dirty.begin(), dirty.end(), 0);

  // acts are sorted by cost: the cheap ones are a prefix; tables for all of them if they fit
  size_t nCheap = 0;
  while (nCheap < acts.size() && acts[nCheap].cost <= prop) nCheap++;
  const bool allTab = acts.size() * 4096.0 <= tabmb * 1e6;
  const size_t nTab = allTab ? acts.size() : nCheap;
  std::vector<uint32_t> tab(nTab * 1024);
  {
    std::vector<std::thread> th;
    for (int w = 0; w < T; w++) th.emplace_back([&, w] {
      for (size_t c = w; c < nTab; c += T)
        for (int b = 0; b < 4; b++)
          for (int v = 0; v < 256; v++) {
            uint32_t t = 0;
            for (int j = 0; j < 8; j++) if (v >> j & 1) t |= 1u << acts[c].dst[b * 8 + j];
            tab[(c * 4 + b) * 256 + v] = t;
          }
    });
    for (auto &x : th) x.join();
  }
  auto tapply = [&](size_t c, uint32_t s) {
    const uint32_t *tb = &tab[c * 1024];
    return tb[s & 255] | tb[256 + (s >> 8 & 255)] | tb[512 + (s >> 16 & 255)] | tb[768 + (s >> 24)];
  };
  fprintf(stderr, "propagating with %zu actions of cost <= %d; byte tables for %s (%.1f MB)\n", nCheap, prop, allTab ? "all actions" : "those", tab.size() * 4 / 1e6);
  auto propagate = [&] {
    for (int d = 0; d < 255; d++) {
      std::atomic<uint64_t> ni{0};
      std::atomic<long> ch{0}, n{0}, above{0};
      auto work = [&] {
        long c0 = 0, n0 = 0, a0 = 0;
        for (uint64_t w; (w = ni.fetch_add(1 << 14)) < total;)
          for (uint64_t r = w; r < std::min<uint64_t>(w + (1 << 14), total); r++) {
            const uint8_t dr = aload(dist[r]);
            if (dr != d || !aload(dirty[r])) { if (dr > d && dr != 0xFF) a0++; continue; }
            astore(dirty[r], (uint8_t)0);
            n0++;
            const uint32_t s = stateOf(r);
            for (size_t c = 0; c < nCheap; c++) {
              const uint32_t t = tapply(c, s);
              const int a = c, nd = d + acts[a].cost;
              if (nd < aload(dist[rankOf(t)]) && lower(t, nd, a | INV)) c0++;
            }
          }
        ch += c0, n += n0, above += a0;
      };
      std::vector<std::thread> th;
      for (int t = 0; t < T; t++) th.emplace_back(work);
      for (auto &x : th) x.join();
      if (n) fprintf(stderr, "  length %d: %ld positions expanded, %ld classes lowered (%.0fs)\n", d, n.load(), ch.load(), now() - t0);
      if (!n && !above) break;
    }
  };
  auto hist = [&](const char *what) {
    std::map<int, long> h;
    for (auto d : dist) h[d]++;
    fprintf(stderr, "%s:", what);
    for (auto &kv : h) fprintf(stderr, " %d:%ld", kv.first, kv.second);
    fprintf(stderr, " (%.0fs)\n", now() - t0);
    return h.rbegin()->first;
  };
  propagate();
  int W = hist("positions after propagation");
  if (W == 0xFF) { fprintf(stderr, "some positions unreached; add pools or raise --prop\n"); return 1; }
  for (int round = 0; round < rounds; round++) {
    // band: every action from every image of every top class (actions by cost: stop once an action
    // alone costs as much as the class's length)
    std::atomic<uint64_t> ni{0};
    std::atomic<long> ch{0};
    const uint64_t step = total / 20 + 1;
    auto work = [&] {
      long c0 = 0;
      for (uint64_t w; (w = ni.fetch_add(1 << 14)) < total;) {
        if (w / step != (w + (1 << 14)) / step) fprintf(stderr, "    band round %d: %.0f%% (%.0fs)\n", round, 100.0 * (w + (1 << 14)) / total, now() - t0);
        for (uint64_t r = w; r < std::min<uint64_t>(w + (1 << 14), total); r++) {
          const int d = aload(dist[r]);
          if (d <= W - band) continue;
          const uint32_t s = stateOf(r);
          if (canon(s) != s) continue;
          for (int k = 0; k < NS; k++) {
            const uint32_t x = img(k, s);
            for (size_t a = 0; a < acts.size(); a++) {
              if (acts[a].cost >= aload(dist[r])) break;
              const int v = acts[a].cost + aload(dist[rankOf(allTab ? tapply(a, x) : apply(acts[a], x))]);
              if (v < aload(dist[r]) && lower(s, v, a)) c0++;
            }
          }
        }
      }
      ch += c0;
    };
    std::vector<std::thread> th;
    for (int t = 0; t < T; t++) th.emplace_back(work);
    for (auto &x : th) x.join();
    fprintf(stderr, "band round %d (lengths > %d): %ld classes lowered (%.0fs)\n", round, W - band, ch.load(), now() - t0);
    if (!ch) break;
    propagate();
    W = hist("positions after band and propagation");
  }

  // rebuild: from any position, the stored action of its class, in whichever symmetry image
  // takes it to a position at least that much shorter
  auto permOf = [&](const std::vector<cc::Mv> &mv, int *p) {
    for (int k = 0; k < 48; k++) p[k] = k;
    for (auto &m : mv) for (int k = 0; k < 48; k++) p[k] = mp[m.face][m.depth][m.amt][p[k]];
  };
  auto moveOn = [&](uint32_t x, const int *p) {
    uint32_t t = 0;
    for (int j = 0; j < 32; j++) if (x >> j & 1) t |= 1u << bitOf[p[facOfBit[j]]];
    return t;
  };
  auto lineOf = [&](uint32_t x, std::vector<cc::Mv> &out) {
    out.clear();
    for (int guard = 0; guard < 200; guard++) {
      const int d = dist[rankOf(x)];
      if (d == 0) return x == SOLVED;
      const uint32_t v = via[rankOf(canon(x))];
      if (v == 0xFFFFFFFF) return false;
      std::vector<cc::Mv> mv;
      if (!(v & INV) && (v & LOADED)) mv = loaded[v & ~LOADED];
      else {
        const Act &a = acts[v & ~INV];
        if (v & INV) for (int i = a.mvLen - 1; i >= 0; i--) { cc::Mv m = byteMv(mvs[a.mvOff + i]); m.amt = 4 - m.amt; mv.push_back(m); }
        else for (int i = 0; i < a.mvLen; i++) mv.push_back(byteMv(mvs[a.mvOff + i]));
      }
      const int cost = mv.size();
      bool found = false;
      for (int s = 0; s < NS && !found; s++) {
        const Sym &y = syms[ss[s]];
        std::vector<cc::Mv> m2;
        for (auto &m : mv) m2.push_back({y.face[m.face], m.depth, y.flip ? 4 - m.amt : m.amt});
        int p[48];
        permOf(m2, p);
        bool ud = true;
        for (int k = 0; k < 48 && ud; k++) if (faceOf(k) == 0 || faceOf(k) == 5) ud = faceOf(p[k]) == faceOf(k);
        if (!ud) continue;
        const uint32_t y2 = moveOn(x, p);
        if (dist[rankOf(y2)] + cost <= d) { out.insert(out.end(), m2.begin(), m2.end()); x = y2; found = true; }
      }
      if (!found) return false;
    }
    return false;
  };
  // replay on colours.  3ax: U 0, D 5, F/B pieces 2, L/R pieces 1; solved = each side face its axis
  // colour.  4op: every piece its face (F/B slots: F or B; L/R slots: L or R); solved = every face.
  auto replayOk = [&](uint32_t x, const std::vector<cc::Mv> &mv) {
    int8_t a[48], b[48];
    for (int k = 0; k < 48; k++) {
      const int f = faceOf(k);
      const bool set = bitOf[k] >= 0 && (x >> bitOf[k] & 1);
      a[k] = f == 0 ? 0 : f == 5 ? 5 : OP ? ((f == 2 || f == 4) ? (set ? 2 : 4) : (set ? 1 : 3)) : set ? 2 : 1;
    }
    for (auto &m : mv) {
      for (int k = 0; k < 48; k++) b[mp[m.face][m.depth][m.amt][k]] = a[k];
      memcpy(a, b, 48);
    }
    for (int k = 0; k < 48; k++) {
      const int f = faceOf(k), want = OP ? f : f == 0 ? 0 : f == 5 ? 5 : (f == 2 || f == 4) ? 2 : 1;
      if (a[k] != want) return false;
    }
    return true;
  };
  // merge: cancel turns of one layer, also across turns that commute with it on the centres: any turn of the
  // same axis; on another axis, outer face turns with outer face turns, and inner slices of the same depth
  // with each other (2026-10-31: those meet only on edges, corners or diagonal centres, which may move)
  auto axisOfFace = [](int f) { return f == 0 || f == 5 ? 0 : f == 1 || f == 3 ? 1 : 2; };
  auto commutes = [&](const cc::Mv &a, const cc::Mv &b) {
    return axisOfFace(a.face) == axisOfFace(b.face) || a.depth == b.depth;
  };
  auto simplify = [&](std::vector<cc::Mv> &v) {
    for (bool ch = true; ch;) {
      ch = false;
      std::vector<cc::Mv> o;
      for (auto m : v) {
        bool merged = false;
        for (int i = (int)o.size() - 1; i >= 0 && commutes(o[i], m); i--)
          if (o[i].face == m.face && o[i].depth == m.depth) {
            const int a = (o[i].amt + m.amt) % 4;
            if (a) o[i].amt = a; else o.erase(o.begin() + i);
            merged = ch = true;
            break;
          }
        if (!merged) o.push_back(m);
      }
      v.swap(o);
    }
  };
  // trailing outer turns (2026-11-02): a final outer face turn changes no colour at the goal (every face is
  // one colour there, pair and other centres alike), so it is dropped; the line solves the same position
  // with the same side effects.  47,800 3ax lines ended in one.  (Cutting general loops between equal pair
  // colourings is not safe: the cut stretch may move other centres.)
  auto cutTail = [&](std::vector<cc::Mv> &v) {
    while (!v.empty() && v.back().depth == 1) v.pop_back();
  };
  for (int pass = 0; merge && pass < 50; pass++) {
    std::vector<std::vector<std::pair<uint32_t, std::vector<cc::Mv>>>> found(T);
    std::atomic<uint64_t> ni{0};
    std::atomic<long> nFail{0};
    std::vector<std::thread> th;
    for (int t = 0; t < T; t++) th.emplace_back([&, t] {
      std::vector<cc::Mv> ln;
      for (uint64_t w; (w = ni.fetch_add(1 << 14)) < total;)
        for (uint64_t r = w; r < std::min<uint64_t>(w + (1 << 14), total); r++) {
          const int d = dist[r];
          if (d < 2) continue;
          const uint32_t s = stateOf(r);
          if (canon(s) != s) continue;
          if (!lineOf(s, ln)) { nFail++; continue; }
          const size_t n0 = ln.size();
          simplify(ln);
          cutTail(ln);
          if (ln.size() < n0 && (int)ln.size() < d && replayOk(s, ln)) found[t].push_back({s, ln});
        }
    });
    for (auto &x : th) x.join();
    long nf = 0, saved = 0;
    for (auto &v : found)
      for (auto &[s, mv] : v) {
        saved += dist[rankOf(s)] - (int)mv.size();
        if (lower(s, mv.size(), LOADED | (uint32_t)loaded.size())) { loaded.push_back(mv); nf++; }
      }
    fprintf(stderr, "merge pass %d: %ld classes shortened by merging moves (%ld moves saved)%s (%.0fs)\n", pass, nf, saved, nFail ? " [some lines failed to rebuild]" : "", now() - t0);
    if (!nf) break;
    propagate();
    W = hist("positions after merging and propagation");
  }
  const uint64_t seed = std::random_device{}() | (uint64_t)std::random_device{}() << 32;
  std::mt19937_64 rng(seed);
  long checked = 0, bad = 0, longer = 0, nw = 0;
  std::vector<cc::Mv> line;
  auto check = [&](uint32_t x) {
    checked++;
    if (!lineOf(x, line) || !replayOk(x, line)) { bad++; return; }
    if ((int)line.size() > dist[rankOf(x)]) longer++;
  };
  // --onemove (2026-11-03): per length, the classes one stage move (a cost-1 action, any symmetry image)
  // away from a position one shorter; a solver need not store their lines (one move, then the shorter
  // class's line).  Prints the counts and exits.
  if (onemove) {
    std::vector<std::map<int, std::array<long, 2>>> oc(T);
    std::vector<std::thread> th;
    for (int t = 0; t < T; t++) th.emplace_back([&, t] {
      for (uint64_t r = t; r < total; r += T) {
        const int d = dist[r];
        const uint32_t s = stateOf(r);
        if (canon(s) != s) continue;
        bool one = false;
        for (int k = 0; k < NS && !one && d > 0; k++) {
          const uint32_t x = img(k, s);
          for (size_t a = 0; a < acts.size() && acts[a].cost == 1; a++)
            if (dist[rankOf(apply(acts[a], x))] + 1 <= d) { one = true; break; }
        }
        oc[t][d][0]++;
        if (one) oc[t][d][1]++;
      }
    });
    for (auto &x : th) x.join();
    std::map<int, std::array<long, 2>> o;
    for (auto &m : oc) for (auto &kv : m) { o[kv.first][0] += kv.second[0]; o[kv.first][1] += kv.second[1]; }
    long all = 0, ones = 0;
    printf("%s classes one stage move from a shorter position (length: classes, one-move):", STAGE);
    for (auto &kv : o) { printf(" %d:%ld/%ld", kv.first, kv.second[0], kv.second[1]); all += kv.second[0]; ones += kv.second[1]; }
    printf("\n%s total: %ld classes, %ld one move from shorter (%.1f%%), %ld need a stored line\n", STAGE, all, ones, 100.0 * ones / all, all - ones);
    // prefixes: for each class that needs a line, the shortest prefix (2+ moves) of its loaded line that
    // ends at a stage position (the pair a stage start; on N = 6..10 every centre outside the pair and the
    // diagonals on its face, as axcheck) whose class is at most the rest of the line; then only the prefix
    // need be stored, followed by a lookup
    struct NC { std::vector<char> kind; std::vector<int> face; std::vector<std::vector<std::pair<int, int>>> mv; int n; };
    std::vector<NC> ncs;
    for (int N = 6; N <= 10; N++) {
      cc::Cube CN(N);
      const int M = N - 1;
      NC c;
      std::vector<int> cid(CN.pos.size(), -1);
      for (int f = 0; f < 6; f++)
        for (int r = 1; r < M; r++)
          for (int col = 1; col < M; col++) {
            bool pr = false;
            for (int o = 0; o < 2 && !pr; o++) {
              int x = o ? 2 : 1, yy = o ? 1 : 2;
              for (int q = 0; q < 4; q++) { if (r == x && col == yy) pr = true; const int nx = yy, ny = M - x; x = nx, yy = ny; }
            }
            cid[CN.index(f, r, col)] = c.kind.size();
            c.kind.push_back(pr ? 1 : (r == col || r + col == M) ? 2 : 3);
            c.face.push_back(f);
          }
      c.n = c.kind.size();
      c.mv.resize(6 * 4 * 4);
      for (int f = 0; f < 6; f++)
        for (int d = 1; d <= 3; d++)
          for (int a = 1; a <= 3; a++)
            for (size_t i = 0; i < CN.pos.size(); i++) {
              if (cid[i] < 0) continue;
              int j = i;
              for (int q = 0; q < a; q++) j = CN.perms[f][d - 1][j];
              if (j != (int)i) c.mv[(f * 4 + d) * 4 + a].push_back({cid[i], cid[j]});
            }
      ncs.push_back(std::move(c));
    }
    std::vector<std::map<int, std::array<long, 4>>> pc(T);  // need line, prefix found, stored moves, not loaded
    std::vector<std::map<int, long>> pk(T);                 // prefix lengths
    std::vector<std::thread> th2;
    for (int t = 0; t < T; t++) th2.emplace_back([&, t] {
      std::vector<std::vector<int>> arr(ncs.size()), tmp(ncs.size());
      for (uint64_t r = t; r < total; r += T) {
        const int d = dist[r];
        if (d == 0) continue;
        const uint32_t s = stateOf(r);
        if (canon(s) != s) continue;
        bool one = false;
        for (int k = 0; k < NS && !one; k++) {
          const uint32_t x = img(k, s);
          for (size_t a = 0; a < acts.size() && acts[a].cost == 1; a++)
            if (dist[rankOf(apply(acts[a], x))] + 1 <= d) { one = true; break; }
        }
        if (one) continue;
        auto &c = pc[t][d];
        c[0]++;
        const uint32_t v = via[r];
        if ((v & INV) || !(v & LOADED)) { c[3]++; c[2] += d; continue; }
        const auto &mv = loaded[v & ~LOADED];
        // the line's start, as colours (replayOk's encoding): run the line backwards from the goal
        int8_t a8[48], b8[48];
        for (int k = 0; k < 48; k++) { const int f = faceOf(k); a8[k] = OP ? f : f == 0 ? 0 : f == 5 ? 5 : (f == 2 || f == 4) ? 2 : 1; }
        for (auto it = mv.rbegin(); it != mv.rend(); ++it)
          { for (int k = 0; k < 48; k++) b8[mp[it->face][it->depth][4 - it->amt][k]] = a8[k]; memcpy(a8, b8, 48); }
        for (size_t z = 0; z < ncs.size(); z++) { arr[z].resize(ncs[z].n); for (int i = 0; i < ncs[z].n; i++) arr[z][i] = i; }
        int best = 0;
        for (int k = 0; k + 1 < (int)mv.size() && !best; k++) {
          const auto &m = mv[k];
          for (int q = 0; q < 48; q++) b8[mp[m.face][m.depth][m.amt][q]] = a8[q];
          memcpy(a8, b8, 48);
          for (size_t z = 0; z < ncs.size(); z++) {
            tmp[z] = arr[z];
            for (auto &pr : ncs[z].mv[(m.face * 4 + m.depth) * 4 + m.amt]) arr[z][pr.second] = tmp[z][pr.first];
          }
          const int len = k + 1;
          if (len < 2) continue;
          bool ok = true;
          uint32_t y = 0;
          for (int q = 0; q < 48 && ok; q++) {
            const int f = faceOf(q), g = a8[q];
            if (f == 0 || f == 5) { ok = g == f; continue; }
            if (OP) { if (axisOf[g] != axisOf[f]) ok = false; else if (g == 2 || g == 1) y |= 1u << bitOf[q]; }
            else { if (g == 0 || g == 5) ok = false; else if (g == 2) y |= 1u << bitOf[q]; }
          }
          for (size_t z = 0; z < ncs.size() && ok; z++)
            for (int i = 0; i < ncs[z].n && ok; i++) if (ncs[z].kind[i] == 3 && ncs[z].face[arr[z][i]] != ncs[z].face[i]) ok = false;
          if (ok && dist[rankOf(y)] + len <= d) best = len;
        }
        if (best) { c[1]++; c[2] += best; pk[t][best]++; } else c[2] += d;
      }
    });
    for (auto &x : th2) x.join();
    std::map<int, std::array<long, 4>> P;
    std::map<int, long> K;
    for (auto &m : pc) for (auto &kv : m) for (int j = 0; j < 4; j++) P[kv.first][j] += kv.second[j];
    for (auto &m : pk) for (auto &kv : m) K[kv.first] += kv.second;
    long need = 0, found = 0, moves = 0, nl = 0, fullMoves = 0;
    printf("%s classes that need a line, by length: classes/prefix found:", STAGE);
    for (auto &kv : P) { printf(" %d:%ld/%ld", kv.first, kv.second[0], kv.second[1]); need += kv.second[0]; found += kv.second[1]; moves += kv.second[2]; nl += kv.second[3]; fullMoves += (long)kv.first * kv.second[0]; }
    printf("\n%s prefix lengths:", STAGE);
    for (auto &kv : K) printf(" %d:%ld", kv.first, kv.second);
    printf("\n%s prefixes: %ld of %ld classes (%.1f%%) need only a prefix then a lookup; stored moves %ld (full lines: %ld)%s\n", STAGE, found, need, 100.0 * found / need, moves, fullMoves,
           nl ? " [some classes' lines not loaded lines]" : "");
    return 0;
  }
  std::vector<uint32_t> worst;
  for (uint64_t r = 0; r < total; r++) if (dist[r] == W) { const uint32_t s = stateOf(r); if (canon(s) == s) worst.push_back(s); }
  for (auto s : worst) check(s), nw++;
  for (long i = 0; i < verifyN; i++) check(stateOf(rng() % total));
  fprintf(stderr, "replayed %ld lines (%ld worst classes, %ld random positions; seed %llu): %ld bad, %ld longer than their bound\n", checked, nw, verifyN, (unsigned long long)seed, bad, longer);

  std::vector<std::map<int, long>> hc(T);
  {
    std::vector<std::thread> th;
    for (int t = 0; t < T; t++) th.emplace_back([&, t] {
      for (uint64_t r = t; r < total; r += T) { const uint32_t s = stateOf(r); if (canon(s) == s) hc[t][dist[r]]++; }
    });
    for (auto &x : th) x.join();
  }
  std::map<int, long> h, hcl;
  for (auto d : dist) h[d]++;
  for (auto &m : hc) for (auto &kv : m) hcl[kv.first] += kv.second;
  printf("%s positions by length:", STAGE);
  for (auto &kv : h) printf(" %d:%ld", kv.first, kv.second);
  printf("\n%s classes by length (%d stage symmetries):", STAGE, NS);
  for (auto &kv : hcl) printf(" %d:%ld", kv.first, kv.second);
  // write one line per class with dist >= lo (every class for --save), in rank order; threaded
  auto writeClasses = [&](const std::string &fn, int lo) {
    const std::string tmp = fn + ".tmp";
    FILE *f = fopen(tmp.c_str(), "w");
    if (!f) { fprintf(stderr, "cannot write %s\n", tmp.c_str()); return false; }
    const uint64_t chunk = 1 << 20;
    std::atomic<long> nOut{0}, nBad{0};
    for (uint64_t base = 0; base < total; base += chunk * T) {
      std::vector<std::string> buf(T);
      std::vector<std::thread> th;
      for (int t = 0; t < T; t++) th.emplace_back([&, t] {
        std::vector<cc::Mv> ln;
        const uint64_t a = base + t * chunk, b = std::min(total, a + chunk);
        for (uint64_t r = a; r < b; r++) {
          if (dist[r] < lo) continue;
          const uint32_t s = stateOf(r);
          if (canon(s) != s) continue;
          if (!lineOf(s, ln) || !replayOk(s, ln)) { nBad++; continue; }
          buf[t] += std::to_string(ln.size()) + "\t";
          for (size_t i = 0; i < ln.size(); i++) buf[t] += (i ? " " : "") + cc::mvText(ln[i]);
          buf[t] += "\n", nOut++;
        }
      });
      for (auto &x : th) x.join();
      for (auto &s : buf) fwrite(s.data(), 1, s.size(), f);
    }
    fclose(f);
    if (nBad) { fprintf(stderr, "%s: %ld lines fail replay; left in %s\n", fn.c_str(), nBad.load(), tmp.c_str()); return false; }
    rename(tmp.c_str(), fn.c_str());
    fprintf(stderr, "wrote %ld class lines (all replayed) to %s (%.0fs)\n", nOut.load(), fn.c_str(), now() - t0);
    return true;
  };
  if (!worstFile.empty() && !writeClasses(worstFile, from >= 0 ? from : W)) return 1;
  if (!saveFile.empty() && !writeClasses(saveFile, 0)) return 1;
  printf("\nworst %d (%.0fs)%s\n", W, now() - t0, slices ? " with slice half twists" : "");
  return bad ? 1 : 0;
}
