// gpuboundx: boundx (devtools/boundx.cpp) with its two inner loops on the GPU (Metal); writes a
// boundary certificate for gpubound / boundcheck.  Stages A = <S>..<T>, B = <S+T>..<U> as dense
// tables.  For every A class whose line L0 is longer than X - worst(B):
//   1. GPU: every action a (outer turns, base pools' images of <= C moves) on the class's state u:
//      y = a(u), kept when |a| + dA(y) <= |L0| + 2 (pass 2: + --slack);
//   2. CPU: each kept a becomes the path a + line(y) (simplified), plus the stage-symmetric copies
//      of L0 that fix u; per path, where it carries each of B's free facelets from L0's B frame
//      (M = D_P o D_L0^-1); paths cheapest first, duplicates (same M) dropped;
//   3. GPU: every B state with |L0| + dB > X tries the paths in order until |P| + dB(M(s)) <= X;
//      the first such path is marked used; states no path solves are counted.
// Classes with states left get pass 2 (one action with more slack) on what is left, then with
// --deep S pass 3: two actions a1 a2 then the line (a1 with |a1| + dA(a1(u)) <= |L0| + slack + S,
// a2 with |a1| + |a2| + dA <= |L0| + slack; both among the actions of at most --deepprop C2 moves;
// GPU: the action kernel on every a1(u)).  The block of a class is L0 and its used paths in order.
// Stages need one target group per orbit (UD; the GPU ranks 4-subsets of each orbit).
//
//   gpuboundx STAGEDIR_A STAGEDIR_B X --out FILE[.zst] [--base POOL]... [--prop C (8)] [--slack L (5)]
//             [--nosteps] [--stage-acts L] [--deep S] [--deepprop C2 (C)] [--threads T (2)] [--batch K (256)] [--sample K] [--start N] [--first N]
//             [--only FILE] [--left FILE]
// --only FILE: only the classes whose line is in FILE; --left FILE: the lines of the classes not covered.
// --used-acts FILE: per action used by some block (as the first action, or the second of the deep pass): the
// number of used paths starting with it (and as a second action), its length and moves, most used first; the
// symmetric copies of L0 that were used are counted on a # line.
// --stage-acts L: the actions are every distinct effect of at most L stage moves (outer turns and the
// U/D-axis slices of depths 2 and 3) instead of the outer turns; pools only if --base is given.
// --acts FILE [--acts-top N]: the actions are the move sequences in FILE (a --used-acts file: the last tab field
// of each non-# line; or one sequence per line), only the first N if --acts-top N, instead of the outer turns
// (2026-11-10: to find a small action set that still covers every class).
#import <Metal/Metal.h>
#include "certcube.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <functional>
#include <mutex>
#include <random>
#include <thread>
#include <unordered_map>
#include <unordered_set>

static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static int groupMask(const std::string &s) {
  if (s == "none") return 0;
  int m = 0;
  if (s.size() % 2) return -1;
  for (size_t i = 0; i < s.size(); i += 2) {
    const int o = s[i] == 'a' ? 0 : s[i] == 'b' ? 1 : -1;
    const char *p = strchr(cc::FACES, s[i + 1]);
    if (o < 0 || !p || !*p) return -1;
    m |= 1 << (o * 6 + (p - cc::FACES));
  }
  return m;
}
static int groupOf(int k) { return (k % 8) / 4 * 6 + k / 8; }  // orbit * 6 + face
static int orbitOf(int k) { return (k % 8) / 4; }
static int mp[6][4][4][48];
static uint64_t BN[49][5];
static uint64_t binom(int n, int k) {
  if (k < 0 || n < k) return 0;
  uint64_t r = 1;
  for (int i = 1; i <= k; i++) r = r * (n - k + i) / i;
  return r;
}
struct Sym { int fac[48]; int gmap[12]; int face[6]; bool flip; };
static std::vector<Sym> allSyms;
static inline uint8_t mvByte(const cc::Mv &m) { return m.face << 4 | m.depth << 2 | m.amt; }
static inline cc::Mv byteMv(uint8_t b) { return {b >> 4, b >> 2 & 3, b & 3}; }
static std::vector<cc::Mv> symMoves(const Sym &y, const std::vector<cc::Mv> &mv) {
  std::vector<cc::Mv> r;
  for (auto &m : mv) r.push_back({y.face[m.face], m.depth, y.flip ? 4 - m.amt : m.amt});
  return r;
}
// a set of 64-bit hashes, open addressing; allocates only when it grows (2026-11-10: the per-class
// std::unordered_set allocated a node per path).  0 is the empty slot, so a hash of 0 is stored as 1 (a
// collision only drops a path, never adds a wrong one).
struct FlatSet {
  std::vector<uint64_t> t;
  size_t n = 0;
  void clear() { std::fill(t.begin(), t.end(), 0), n = 0; }
  void release() { std::vector<uint64_t>().swap(t), n = 0; }
  bool insert(uint64_t h) {
    if (!h) h = 1;
    if ((n + 1) * 2 > t.size()) grow();
    const size_t m = t.size() - 1;
    for (size_t i = h & m;; i = (i + 1) & m) {
      if (!t[i]) { t[i] = h, n++; return true; }
      if (t[i] == h) return false;
    }
  }
  void grow() {
    std::vector<uint64_t> o;
    o.swap(t);
    t.assign(o.empty() ? 1024 : o.size() * 2, 0), n = 0;
    for (uint64_t x : o) if (x) insert(x);
  }
};
static void simplify(std::vector<cc::Mv> &v) {
  std::vector<cc::Mv> r;
  for (auto &m : v) {
    if (!r.empty() && r.back().face == m.face && r.back().depth == m.depth) {
      const int a = (r.back().amt + m.amt) % 4;
      if (a) r.back().amt = a;
      else r.pop_back();
    } else r.push_back(m);
  }
  v.swap(r);
}
static void mapOf(const std::vector<cc::Mv> &mv, int8_t *D) {
  for (int k = 0; k < 48; k++) {
    int x = k;
    for (auto &m : mv) x = mp[m.face][m.depth][m.amt][x];
    D[k] = x;
  }
}
static bool poolMoves(const char *buf, std::vector<cc::Mv> &mv) {  // as boundx
  size_t n = strlen(buf);
  if (!n || buf[n - 1] != '\n' || buf[0] == '#') return false;
  std::string l(buf);
  while (!l.empty() && (l.back() == '\n' || l.back() == ' ' || l.back() == '\r')) l.pop_back();
  if (l.size() >= 3 && l[l.size() - 3] == ' ' && l[l.size() - 2] == 'D') l.resize(l.size() - 3);
  size_t st = 0;
  while (st < l.size() && l[st] == ' ') st++;
  size_t e = st;
  while (e < l.size() && l[e] >= '0' && l[e] <= '9') e++;
  if (e > st && (e == l.size() || l[e] == ' ')) st = e;
  if (!cc::parseMoves(l.substr(st), mv) || mv.empty()) return false;
  for (auto &m : mv) if (m.depth < 1 || m.depth > 3) return false;
  return true;
}
// a move's text appended to a char buffer (no allocation once the buffer has grown): as cc::mvText
static void appendMv(std::string &out, const cc::Mv &m) {
  if (m.depth > 1) out.push_back('0' + m.depth);
  out.push_back(cc::FACES[m.face]);
  if (m.amt == 2) out.push_back('2');
  else if (m.amt == 3) out.push_back('\'');
}
static std::string text(const std::vector<cc::Mv> &mv) {
  std::string s;
  for (auto &m : mv) { if (!s.empty()) s += ' '; s += cc::mvText(m); }
  return s;
}
static bool forChunks(const std::string &f, size_t chunk, const std::function<void(std::vector<std::string> &)> &fn) {
  FILE *in = fopen(f.c_str(), "r");
  if (!in) return false;
  char buf[4096];
  std::vector<std::string> v;
  while (fgets(buf, sizeof buf, in)) {
    std::string l(buf);
    while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
    v.push_back(std::move(l));
    if (v.size() == chunk) { fn(v); v.clear(); }
  }
  if (!v.empty()) fn(v);
  fclose(in);
  return true;
}

// a stage: rank as boundcheck (per target group, its 4 facelets among its orbit's free facelets,
// colex); one target group per orbit, so a state is 8 free indices (group 0's 4, group 1's 4)
struct Stage {
  std::string name;
  int S = 0, T = 0;
  std::vector<int> tg, freeO[2], syms;
  int fIdx[48];
  uint64_t total = 1, rad1 = 1;
  std::vector<uint8_t> dist, srcSym;
  std::vector<uint32_t> srcLine, lineOff;
  std::vector<uint8_t> lineBytes;
  int W = 0;
  bool keepLines = false;

  uint64_t rank8(const uint8_t *s) const {  // 8 free indices
    uint64_t r[2];
    for (int g = 0; g < 2; g++) {
      uint32_t m = 0;
      for (int j = 0; j < 4; j++) m |= 1u << s[g * 4 + j];
      uint64_t sub = 0;
      for (int j = 1; j <= 4; j++) { sub += BN[__builtin_ctz(m)][j]; m &= m - 1; }
      r[g] = sub;
    }
    return r[0] * rad1 + r[1];
  }
  void stateOf(const int8_t *col, uint8_t *s) const {  // colouring (group ids) -> 8 free indices
    for (int g = 0; g < 2; g++) {
      int n = 0;
      for (int k : freeO[orbitOf(tg[0]) == g ? orbitOf(tg[0]) : orbitOf(tg[1])]) if (col[k] == tg[g]) s[g * 4 + n++] = fIdx[k];
    }
  }
  bool parseName(const std::string &dir) {
    std::string d = dir;
    while (d.size() > 1 && d.back() == '/') d.pop_back();
    name = d.substr(d.rfind('/') == std::string::npos ? 0 : d.rfind('/') + 1);
    const size_t dd = name.find("..");
    if (dd == std::string::npos) { fprintf(stderr, "stage directory must be named <solved>..<target>\n"); return false; }
    S = groupMask(name.substr(0, dd)), T = groupMask(name.substr(dd + 2));
    if (S < 0 || T <= 0 || (S & T)) { fprintf(stderr, "bad stage name %s\n", name.c_str()); return false; }
    for (int g = 0; g < 12; g++) if (T >> g & 1) tg.push_back(g);
    if (tg.size() != 2 || tg[0] / 6 != 0 || tg[1] / 6 != 1) { fprintf(stderr, "%s: gpuboundx needs one target group per orbit\n", name.c_str()); return false; }
    for (int k = 0; k < 48; k++) fIdx[k] = -1;
    for (int k = 0; k < 48; k++) if (!(S >> groupOf(k) & 1)) { fIdx[k] = freeO[orbitOf(k)].size(); freeO[orbitOf(k)].push_back(k); }
    if (freeO[0].size() > 24 || freeO[1].size() > 24) return false;
    rad1 = binom(freeO[1].size(), 4);
    total = binom(freeO[0].size(), 4) * rad1;
    for (size_t s = 0; s < allSyms.size(); s++) {
      int s2 = 0, t2 = 0;
      for (int g = 0; g < 12; g++) { if (S >> g & 1) s2 |= 1 << allSyms[s].gmap[g]; if (T >> g & 1) t2 |= 1 << allSyms[s].gmap[g]; }
      if (s2 == S && t2 == T) syms.push_back(s);
    }
    return true;
  }
  // the state a line solves, as 8 free indices of its symmetric image s
  void lineState(const std::vector<cc::Mv> &mv, int s, uint8_t *out) const {
    int8_t a[48], b[48], img[48];
    for (int k = 0; k < 48; k++) a[k] = ((S | T) >> groupOf(k) & 1) ? groupOf(k) : -1;
    for (auto it = mv.rbegin(); it != mv.rend(); ++it) {
      for (int k = 0; k < 48; k++) b[mp[it->face][it->depth][4 - it->amt][k]] = a[k];
      memcpy(a, b, 48);
    }
    for (int k = 0; k < 48; k++) img[allSyms[s].fac[k]] = a[k] < 0 ? -1 : allSyms[s].gmap[a[k]];
    for (int g = 0; g < 2; g++) {
      int n = 0;
      for (int k : freeO[g]) if (img[k] == tg[g]) out[g * 4 + n++] = fIdx[k];
    }
  }
  // two passes: dist = shortest line over every symmetric image; then (keepLines) a line of
  // exactly that length per state, so the stored line always matches dist
  bool init(const std::string &dir, int threads, bool lines) {
    keepLines = lines;
    std::string d = dir;
    while (d.size() > 1 && d.back() == '/') d.pop_back();
    std::vector<std::string> fs;
    if (DIR *dp = opendir(d.c_str())) {
      while (dirent *e = readdir(dp)) {
        std::string n = e->d_name;
        if (n.size() > 5 && n[0] == 'w' && n.compare(n.size() - 4, 4, ".txt") == 0) fs.push_back(d + "/" + n);
      }
      closedir(dp);
    }
    if (fs.empty()) { fprintf(stderr, "no segments in %s\n", name.c_str()); return false; }
    std::sort(fs.begin(), fs.end());
    const double t0 = now();
    dist.assign(total, 0xFF);
    if (keepLines) srcSym.assign(total, 0), srcLine.assign(total, 0xFFFFFFFF);
    std::atomic<long> bad{0};
    long nl = 0;
    for (int pass = 0; pass < (keepLines ? 2 : 1); pass++) {
      nl = 0;
      for (auto &f : fs) {
        const bool rd = forChunks(f, 1 << 19, [&](std::vector<std::string> &raw) {
          const long base = nl;
          nl += raw.size();
          if (keepLines && pass == 0) {
            for (auto &l : raw) {
              std::vector<cc::Mv> mv;
              cc::parseMoves(l, mv);
              lineOff.push_back(lineBytes.size());
              for (auto &m : mv) lineBytes.push_back(mvByte(m));
            }
          }
          std::atomic<size_t> ni{0};
          std::vector<std::thread> th;
          for (int t = 0; t < threads; t++)
            th.emplace_back([&] {
              std::vector<cc::Mv> mv;
              uint8_t st[8];
              for (size_t li; (li = ni++) < raw.size();) {
                bool ok = cc::parseMoves(raw[li], mv);
                for (auto &m : mv) ok &= m.depth >= 1 && m.depth <= 3;
                if (!ok) { if (pass == 0) bad++; continue; }
                for (int s : syms) {
                  lineState(mv, s, st);
                  const uint64_t r = rank8(st);
                  if (pass == 0) {
                    uint8_t cur = std::atomic_ref<uint8_t>(dist[r]).load();
                    while (mv.size() < cur && !std::atomic_ref<uint8_t>(dist[r]).compare_exchange_weak(cur, mv.size())) {}
                  } else if (mv.size() == dist[r]) {
                    // any line of the shortest length will do; a race picks one of them
                    srcLine[r] = base + li, srcSym[r] = s;
                  }
                }
              }
            });
          for (auto &x : th) x.join();
        });
        if (!rd) { fprintf(stderr, "cannot open a segment\n"); return false; }
      }
    }
    if (keepLines) lineOff.push_back(lineBytes.size());
    long un = 0;
    for (uint64_t r = 0; r < total; r++) { if (dist[r] == 0xFF) un++; else W = std::max<int>(W, dist[r]); }
    fprintf(stderr, "%s: %ld lines (%ld bad), %lu states, %ld not covered, worst %d (%.0fs)\n", name.c_str(), nl, bad.load(), (unsigned long)total, un, W, now() - t0);
    return !bad && !un;
  }
  std::vector<cc::Mv> lineAt(size_t i) const {
    std::vector<cc::Mv> mv;
    for (uint32_t j = lineOff[i]; j < lineOff[i + 1]; j++) mv.push_back(byteMv(lineBytes[j]));
    return mv;
  }
  std::vector<cc::Mv> lineOf(uint64_t r) const { return symMoves(allSyms[srcSym[r]], lineAt(srcLine[r])); }
};

static const char *kSrc = R"(
#include <metal_stdlib>
using namespace metal;
struct AP { uint nact, ncls, nA0, rad1, cap; };
// pass 1/2: every (action, class): y = a(u), kept when |a| + dA(y) <= lim
kernel void acts(device const uchar *cls [[buffer(0)]], device const uchar *dA [[buffer(1)]],
                 device const uchar *act [[buffer(2)]], device const uchar *alen [[buffer(3)]],
                 device const uchar *lim [[buffer(4)]], device atomic_uint *cnt [[buffer(5)]],
                 device uint *out [[buffer(6)]], constant AP &P [[buffer(7)]], constant uint *bn [[buffer(8)]],
                 uint2 gid [[thread_position_in_grid]]) {
  const uint a = gid.x, k = gid.y;
  if (a >= P.nact) return;
  const uint al = alen[a];
  const uint L = lim[k];
  if (al >= L) return;
  device const uchar *m = act + a * 48;
  device const uchar *c = cls + k * 8;
  uint m0 = (1u << m[c[0]]) | (1u << m[c[1]]) | (1u << m[c[2]]) | (1u << m[c[3]]);
  uint m1 = (1u << m[P.nA0 + c[4]]) | (1u << m[P.nA0 + c[5]]) | (1u << m[P.nA0 + c[6]]) | (1u << m[P.nA0 + c[7]]);
  uint r0 = 0, r1 = 0;
  for (uint j = 1; j <= 4; j++) { r0 += bn[ctz(m0) * 5 + j]; m0 &= m0 - 1; r1 += bn[ctz(m1) * 5 + j]; m1 &= m1 - 1; }
  const uint ry = r0 * P.rad1 + r1;
  if (al + dA[ry] > L) return;
  const uint i = atomic_fetch_add_explicit(cnt, 1u, memory_order_relaxed);
  if (i < P.cap) { out[3 * i] = k; out[3 * i + 1] = a; out[3 * i + 2] = ry; }
}
struct CP { uint X, nF0, rad1, leftCap; };
struct Cls { uint path0, npath, nstates, used0; };
// does path P (moves m, length len) take the B state (s0, s1) to |P| + dB <= X
static inline bool solves(device const uchar *m, uint len, uchar4 s0, uchar4 s1, device const uchar *dB, constant CP &P, constant uint *bn) {
  uint m0 = (1u << m[s0.x]) | (1u << m[s0.y]) | (1u << m[s0.z]) | (1u << m[s0.w]);
  uint m1 = (1u << m[P.nF0 + s1.x]) | (1u << m[P.nF0 + s1.y]) | (1u << m[P.nF0 + s1.z]) | (1u << m[P.nF0 + s1.w]);
  uint r0 = 0, r1 = 0;
  for (uint j = 1; j <= 4; j++) { r0 += bn[ctz(m0) * 5 + j]; m0 &= m0 - 1; r1 += bn[ctz(m1) * 5 + j]; m1 &= m1 - 1; }
  return len + dB[r0 * P.rad1 + r1] <= P.X;
}
// check: every (bad B state, class): paths in order until |P| + dB(M(s)) <= X
kernel void check(device const uchar4 *st [[buffer(0)]], device const uchar *dB [[buffer(1)]],
                  device const uchar *paths [[buffer(2)]], device const uchar *plen [[buffer(3)]],
                  device const Cls *cl [[buffer(4)]], device atomic_uint *used [[buffer(5)]],
                  device atomic_uint *left [[buffer(6)]], device uint *leftOut [[buffer(7)]],
                  constant CP &P [[buffer(8)]], constant uint *bn [[buffer(9)]],
                  uint2 gid [[thread_position_in_grid]]) {
  const uint k = gid.y;
  const Cls C = cl[k];
  if (gid.x >= C.nstates) return;
  const uchar4 s0 = st[2 * gid.x], s1 = st[2 * gid.x + 1];
  for (uint i = 0; i < C.npath; i++)
    if (solves(paths + (C.path0 + i) * 48, plen[C.path0 + i], s0, s1, dB, P, bn)) {
      atomic_fetch_or_explicit(&used[C.used0 + i / 32], 1u << (i % 32), memory_order_relaxed);
      return;
    }
  const uint j = atomic_fetch_add_explicit(&left[0], 1u, memory_order_relaxed);
  if (j < P.leftCap) { leftOut[2 * j] = k; leftOut[2 * j + 1] = gid.x; }
}
// pass 2's check: a list of (class, state) items, each through its class's new paths in order; ok[item] = solved
kernel void check2(device const uchar4 *st [[buffer(0)]], device const uchar *dB [[buffer(1)]],
                   device const uchar *paths [[buffer(2)]], device const uchar *plen [[buffer(3)]],
                   device const Cls *cl [[buffer(4)]], device atomic_uint *used [[buffer(5)]],
                   device const uint2 *items [[buffer(6)]], device uchar *ok [[buffer(7)]],
                   constant CP &P [[buffer(8)]], constant uint *bn [[buffer(9)]], constant uint &n [[buffer(10)]],
                   uint gid [[thread_position_in_grid]]) {
  if (gid >= n) return;
  const uint2 it = items[gid];
  const Cls C = cl[it.x];
  const uchar4 s0 = st[2 * it.y], s1 = st[2 * it.y + 1];
  for (uint i = 0; i < C.npath; i++)
    if (solves(paths + (C.path0 + i) * 48, plen[C.path0 + i], s0, s1, dB, P, bn)) {
      atomic_fetch_or_explicit(&used[C.used0 + i / 32], 1u << (i % 32), memory_order_relaxed);
      ok[gid] = 1;
      return;
    }
  ok[gid] = 0;
}
)";

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  fflush(stdout);
  int C = 8, SL = 5, threads = 2, batch = 256, samp = 1, DEEP = -1, C2 = -1;
  bool steps = true;
  std::string usedActsFile, actsFile;
  long actsTop = -1;
  int stageActs = 0;
  int capB = -1;
  long first = -1, start = 0;
  std::vector<std::string> pos, base;
  std::string outFile, onlyFile, leftFile;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--prop" && i + 1 < argc) C = atoi(argv[++i]);
    else if (o == "--slack" && i + 1 < argc) SL = atoi(argv[++i]);
    else if (o == "--threads" && i + 1 < argc) threads = atoi(argv[++i]);
    else if (o == "--batch" && i + 1 < argc) batch = atoi(argv[++i]);
    else if (o == "--sample" && i + 1 < argc) samp = std::max(1, atoi(argv[++i]));
    else if (o == "--first" && i + 1 < argc) first = atol(argv[++i]);
    else if (o == "--start" && i + 1 < argc) start = atol(argv[++i]);
    else if (o == "--deep" && i + 1 < argc) DEEP = atoi(argv[++i]);
    else if (o == "--nosteps") steps = false;
    else if (o == "--stage-acts" && i + 1 < argc) stageActs = atoi(argv[++i]);
    else if (o == "--used-acts" && i + 1 < argc) usedActsFile = argv[++i];
    else if (o == "--acts" && i + 1 < argc) actsFile = argv[++i];
    else if (o == "--acts-top" && i + 1 < argc) actsTop = atol(argv[++i]);
    else if (o == "--cap-b" && i + 1 < argc) capB = atoi(argv[++i]);
    else if (o == "--deepprop" && i + 1 < argc) C2 = atoi(argv[++i]);
    else if (o == "--only" && i + 1 < argc) onlyFile = argv[++i];
    else if (o == "--left" && i + 1 < argc) leftFile = argv[++i];
    else if (o == "--out" && i + 1 < argc) outFile = argv[++i];
    else if (o == "--base" && i + 1 < argc) base.push_back(argv[++i]);
    else if (o.size() > 1 && o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else pos.push_back(o);
  }
  if (pos.size() != 3 || outFile.empty()) {
    fprintf(stderr, "usage: gpuboundx STAGEDIR_A STAGEDIR_B X --out FILE[.zst] [--base POOL]... [--prop C] [--slack L] [--nosteps] [--stage-acts L] [--acts FILE [--acts-top N]] [--used-acts FILE] [--deep S] [--deepprop C2] [--threads T] [--batch K] [--sample K] [--start N] [--first N] [--only FILE] [--left FILE]\n");
    return 1;
  }
  if (!actsFile.empty() && stageActs > 0) { fprintf(stderr, "--acts and --stage-acts exclude each other\n"); return 1; }
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
  for (int n = 0; n <= 48; n++) for (int k = 0; k <= 4; k++) BN[n][k] = binom(n, k);
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
          int j = -1;
          for (int x = 0; x < 48; x++) if (C8.pos[pf[x]] == q) j = x;
          if (j < 0) { fprintf(stderr, "a symmetry leaves the pair\n"); return 1; }
          y.fac[k] = j;
        }
        for (int g = 0; g < 12; g++) for (int k = 0; k < 48; k++) if (groupOf(k) == g) y.gmap[g] = groupOf(y.fac[k]);
        for (int f = 0; f < 6; f++) {
          auto q = map(cc::Cube::normalOf(f));
          for (int f2 = 0; f2 < 6; f2++) if (cc::Cube::normalOf(f2) == q) y.face[f] = f2;
        }
        y.flip = par[pi] * ((sg & 1) ? -1 : 1) * ((sg & 2) ? -1 : 1) * ((sg & 4) ? -1 : 1) < 0;
        allSyms.push_back(y);
      }
  }
  // self-test: a symmetric copy of a move sequence has the conjugated facelet map
  {
    std::vector<cc::Mv> mv;
    uint32_t x = 12345;
    for (int i = 0; i < 20; i++) { x = x * 1103515245 + 12345; mv.push_back({(int)(x >> 8) % 6, 1 + (int)(x >> 12) % 3, 1 + (int)(x >> 16) % 3}); }
    int8_t D[48], E[48];
    mapOf(mv, D);
    for (auto &y : allSyms) {
      mapOf(symMoves(y, mv), E);
      for (int k = 0; k < 48; k++) if (E[y.fac[k]] != y.fac[(int)D[k]]) { fprintf(stderr, "symmetry self-test failed\n"); return 1; }
    }
  }

  Stage A, B;
  if (!A.parseName(pos[0]) || !B.parseName(pos[1])) return 1;
  if (B.S != (A.S | A.T)) { fprintf(stderr, "stage B must start where A ends\n"); return 1; }
  if (!std::includes(B.syms.begin(), B.syms.end(), A.syms.begin(), A.syms.end())) { fprintf(stderr, "stage A has a symmetry stage B lacks\n"); return 1; }
  if (!B.init(pos[1], threads, false) || !A.init(pos[0], threads, true)) return 1;
  // --cap-b D (experiment only; the blocks are NOT valid): pretend every B state longer than D has length D,
  // to see how much shorter B lines would help the boundary
  if (capB >= 0) {
    long n = 0;
    for (auto &d : B.dist) if (d > capB) d = capB, n++;
    B.W = std::min(B.W, capB);
    fprintf(stderr, "--cap-b %d: %ld B states capped (EXPERIMENT: blocks not valid)\n", capB, n);
  }

  // actions: outer face turns and the base pools' images of cost <= C, by facelet map (as boundx)
  struct Act { int8_t D[48]; std::vector<cc::Mv> mv; };
  std::vector<Act> acts;
  {
    std::unordered_map<std::string, size_t> idx;
    auto add = [&](const std::vector<cc::Mv> &mv) {
      Act a;
      a.mv = mv;
      mapOf(mv, a.D);
      bool id = true;
      for (int k = 0; k < 48; k++) if (a.D[k] != k) id = false;
      if (id) return;
      for (int k = 0; k < 48; k++) if ((A.S >> groupOf(k) & 1) && groupOf(a.D[k]) != groupOf(k)) return;
      std::string key((const char *)a.D, 48);
      auto it = idx.find(key);
      if (it != idx.end()) { if (acts[it->second].mv.size() > mv.size()) acts[it->second].mv = mv; return; }
      idx[key] = acts.size();
      acts.push_back(a);
    };
    if (!actsFile.empty()) {
      FILE *in = fopen(actsFile.c_str(), "r");
      if (!in) { fprintf(stderr, "cannot read %s\n", actsFile.c_str()); return 1; }
      char buf[4096];
      long n = 0, bad = 0;
      std::vector<cc::Mv> mv;
      while ((actsTop < 0 || n < actsTop) && fgets(buf, sizeof buf, in)) {
        if (buf[0] == '#') continue;
        std::string l(buf);
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
        const size_t t = l.rfind('\t');
        if (t != std::string::npos) l.erase(0, t + 1);
        n++;
        const size_t before = acts.size();
        if (!cc::parseMoves(l, mv) || mv.empty()) { fprintf(stderr, "--acts: bad line: %s\n", l.c_str()); return 1; }
        add(mv);
        bad += acts.size() == before;
      }
      fclose(in);
      fprintf(stderr, "--acts %s: %ld sequences, %zu actions (%ld identity, not stage-legal or repeated)\n", actsFile.c_str(), n, acts.size(), bad);
    } else if (stageActs <= 0) for (int f = 0; f < 6; f++) for (int a = 1; a <= 3; a++) add({{f, 1, a}});
    else {
      // --stage-acts L (2026-11-09): every distinct effect of at most L stage moves (the 18 outer turns
      // and the 12 U/D-axis slices of depths 2 and 3: they keep the other obliques' U, ring and D sets),
      // breadth first, so each effect keeps its shortest sequence
      std::vector<cc::Mv> sm;
      for (int f = 0; f < 6; f++) for (int a = 1; a <= 3; a++) sm.push_back({f, 1, a});
      for (int f : {0, 5}) for (int d = 2; d <= 3; d++) for (int a = 1; a <= 3; a++) sm.push_back({f, d, a});
      size_t f0 = acts.size();
      for (auto &m : sm) add({m});
      for (int len = 2; len <= stageActs; len++) {
        const size_t f1 = acts.size();
        for (size_t i = f0; i < f1; i++)
          for (auto &m : sm) {
            if (acts[i].mv.back().face == m.face && acts[i].mv.back().depth == m.depth) continue;
            auto mv = acts[i].mv;
            mv.push_back(m);
            add(mv);
          }
        fprintf(stderr, "stage actions of %d moves: %zu new\n", len, acts.size() - f1);
        f0 = f1;
      }
    }
    for (auto &bf : base) {
      FILE *in = fopen(bf.c_str(), "r");
      if (!in) { fprintf(stderr, "cannot read %s\n", bf.c_str()); return 1; }
      char buf[4096];
      std::vector<cc::Mv> mv;
      while (fgets(buf, sizeof buf, in)) {
        if (!poolMoves(buf, mv) || (int)mv.size() > C) continue;
        std::vector<cc::Mv> inv;
        for (auto it = mv.rbegin(); it != mv.rend(); ++it) inv.push_back({it->face, it->depth, 4 - it->amt});
        for (auto &y : allSyms) { add(symMoves(y, mv)); add(symMoves(y, inv)); }
      }
      fclose(in);
    }
    std::stable_sort(acts.begin(), acts.end(), [](const Act &a, const Act &b) { return a.mv.size() < b.mv.size(); });
    fprintf(stderr, "%zu actions of cost <= %d\n", acts.size(), C);
  }
  // 2026-11-10: each A line's facelet map, once (A.lineOff.size() - 1 lines x 48 bytes), so that a path's map is a
  // composition (D[k] = line[a2[a[k]]], the line's map conjugated by its symmetry) instead of a replay of its
  // moves; and the inverse facelet map of each symmetry.  Self-tested against the replay on random states.
  const size_t nLinesA = A.lineOff.size() - 1;
  std::vector<int8_t> lineMap(nLinesA * 48);
  std::vector<std::array<int8_t, 48>> symInv(allSyms.size());
  {
    const double tl = now();
    for (size_t y = 0; y < allSyms.size(); y++) for (int k = 0; k < 48; k++) symInv[y][allSyms[y].fac[k]] = k;
    std::atomic<size_t> ni{0};
    std::vector<std::thread> th;
    for (int t = 0; t < threads; t++)
      th.emplace_back([&] {
        for (size_t i; (i = ni.fetch_add(4096)) < nLinesA;)
          for (size_t j = i; j < std::min(nLinesA, i + 4096); j++) mapOf(A.lineAt(j), &lineMap[j * 48]);
      });
    for (auto &x : th) x.join();
    std::mt19937_64 rr(20261110);
    for (int t = 0; t < 2000; t++) {
      const uint64_t r = rr() % A.total;
      if (A.srcLine[r] == 0xFFFFFFFFu) continue;
      int8_t D1[48];
      mapOf(A.lineOf(r), D1);
      const Sym &y = allSyms[A.srcSym[r]];
      const int8_t *L = &lineMap[(size_t)A.srcLine[r] * 48];
      for (int k = 0; k < 48; k++)
        if (D1[k] != y.fac[L[symInv[A.srcSym[r]][k]]]) { fprintf(stderr, "line map self-test failed\n"); return 1; }
    }
    fprintf(stderr, "A line maps: %zu lines, %.0f MB (%.1fs)\n", nLinesA, nLinesA * 48 / 1e6, now() - tl);
  }
  // per action, where it sends each of A's free facelets (A free index, orbit 0 then 1)
  const int nA0 = A.freeO[0].size(), nB0 = B.freeO[0].size();
  std::vector<uint8_t> actTab(acts.size() * 48, 0), actLen(acts.size());
  for (size_t a = 0; a < acts.size(); a++) {
    actLen[a] = std::min<size_t>(acts[a].mv.size(), 255);
    for (int o = 0; o < 2; o++)
      for (size_t j = 0; j < A.freeO[o].size(); j++) actTab[a * 48 + o * nA0 + j] = A.fIdx[(int)acts[a].D[A.freeO[o][j]]];
  }

  // bad B states, longest first: cntAt[d] = how many have dB >= d
  const int lowT = std::max(0, X - A.W);
  std::vector<uint8_t> states;
  std::vector<size_t> cntAt(B.W + 2, 0);
  {
    std::vector<std::vector<uint8_t>> byD(B.W + 1);
    uint8_t s[8];
    for (uint64_t r = 0; r < B.total; r++) {
      const int d = B.dist[r];
      if (d <= lowT) continue;
      // unrank r into 8 free indices
      uint64_t rr[2] = {r / B.rad1, r % B.rad1};
      for (int g = 0; g < 2; g++) {
        uint64_t rest = rr[g];
        int p = g == 0 ? nB0 : (int)B.freeO[1].size();
        for (int t = 4; t >= 1; t--) {
          p--;
          while (BN[p][t] > rest) p--;
          rest -= BN[p][t];
          s[g * 4 + t - 1] = p;
        }
      }
      if (B.rank8(s) != r) { fprintf(stderr, "unrank self-test failed\n"); return 1; }
      byD[d].insert(byD[d].end(), s, s + 8);
    }
    for (int d = B.W; d >= 0; d--) {
      cntAt[d] = cntAt[d + 1] + byD[d].size() / 8;
      if (d <= lowT) continue;
      states.insert(states.end(), byD[d].begin(), byD[d].end());
      fprintf(stderr, "B states at %d: %zu\n", d, byD[d].size() / 8);
    }
  }

  // the A classes: lines longer than X - worst(B)
  std::vector<uint32_t> todo;
  for (size_t i = 0; i + 1 < A.lineOff.size(); i++)
    if ((int)(A.lineOff[i + 1] - A.lineOff[i]) > X - B.W) todo.push_back(i);
  if (samp > 1) {
    size_t k = 0;
    for (size_t i = 0; i < todo.size(); i += samp) todo[k++] = todo[i];
    todo.resize(k);
  }
  // --start N: skip the first N classes (to resume a run whose blocks for them are kept, 2026-11-11)
  if (start > 0) todo.erase(todo.begin(), todo.begin() + std::min<size_t>(start, todo.size()));
  if (first >= 0 && (size_t)first < todo.size()) todo.resize(first);
  if (!onlyFile.empty()) {
    // compared as move bytes (as A.lineBytes stores them; a line of <= 22 moves is a short string, so the lookup of
    // every A line allocates nothing; 2026-11-10: formatting 14M lines as text dominated a sample run's start)
    std::unordered_set<std::string> want;
    bool parsed = true;
    if (!forChunks(onlyFile, 1 << 16, [&](std::vector<std::string> &v) {
          std::vector<cc::Mv> mv;
          for (auto &l : v) {
            if (l.find_first_not_of(' ') == std::string::npos) continue;
            if (!cc::parseMoves(l, mv)) { parsed = false; continue; }
            std::string key;
            for (auto &m : mv) key.push_back((char)mvByte(m));
            want.insert(key);
          }
        }) || !parsed) {
      fprintf(stderr, "cannot read the --only file\n");
      return 1;
    }
    size_t k = 0;
    std::string key;
    for (size_t i = 0; i < todo.size(); i++) {
      key.assign((const char *)A.lineBytes.data() + A.lineOff[todo[i]], A.lineOff[todo[i] + 1] - A.lineOff[todo[i]]);
      if (want.count(key)) todo[k++] = todo[i];
    }
    todo.resize(k);
    fprintf(stderr, "--only: %zu of %zu lines found\n", k, want.size());
  }
  fprintf(stderr, "A lines longer than %d: %zu to do%s\n", X - B.W, todo.size(), samp > 1 ? " (sampled)" : "");

  // --out FILE.zst: written through zstd (2026-11-10, as gpuaxbx: UD 32's blocks were 18 GB plain)
  const bool outZst = outFile.size() > 4 && outFile.compare(outFile.size() - 4, 4, ".zst") == 0;
  FILE *out = outZst ? popen(("zstd -q -T2 -10 -f -o '" + outFile + "'").c_str(), "w") : fopen(outFile.c_str(), "w"), *leftOut = nullptr;
  if (!out) { fprintf(stderr, "cannot write %s\n", outFile.c_str()); return 1; }
  if (!leftFile.empty() && !(leftOut = fopen(leftFile.c_str(), "w"))) { fprintf(stderr, "cannot write %s\n", leftFile.c_str()); return 1; }
  // the deep pass's actions: a prefix of acts (sorted by cost)
  if (C2 < 0) C2 = C;
  size_t nDeepAct = 0;
  while (nDeepAct < acts.size() && (int)acts[nDeepAct].mv.size() <= C2) nDeepAct++;
  if (DEEP >= 0) fprintf(stderr, "deep pass: %zu actions of cost <= %d\n", nDeepAct, C2);

  // GPU
  id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
  if (!dev) { fprintf(stderr, "no Metal device\n"); return 1; }
  NSError *err = nil;
  id<MTLLibrary> lib = [dev newLibraryWithSource:[NSString stringWithUTF8String:kSrc] options:nil error:&err];
  if (!lib) { fprintf(stderr, "Metal compile: %s\n", [[err description] UTF8String]); return 1; }
  id<MTLComputePipelineState> psActs = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"acts"] error:&err];
  id<MTLComputePipelineState> psCheck = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"check"] error:&err];
  id<MTLComputePipelineState> psCheck2 = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"check2"] error:&err];
  if (!psActs || !psCheck || !psCheck2) { fprintf(stderr, "pipeline: %s\n", [[err description] UTF8String]); return 1; }
  id<MTLCommandQueue> q = [dev newCommandQueue];
  // at least 16 bytes; an empty input gets a zeroed buffer (2026-11-09: gpubound read 16 bytes from a null
  // pointer for an empty vector)
  auto buf = [&](const void *p, size_t n) {
    return n ? [dev newBufferWithBytes:p length:std::max<size_t>(n, 16) options:MTLResourceStorageModeShared]
             : [dev newBufferWithLength:16 options:MTLResourceStorageModeShared];
  };
  auto zbuf = [&](size_t n) { return [dev newBufferWithLength:std::max<size_t>(n, 16) options:MTLResourceStorageModeShared]; };
  std::vector<uint32_t> bn(25 * 5);
  for (int n = 0; n < 25; n++) for (int k = 0; k < 5; k++) bn[n * 5 + k] = BN[n][k];
  id<MTLBuffer> bBn = buf(bn.data(), bn.size() * 4);
  id<MTLBuffer> bDA = buf(A.dist.data(), A.dist.size()), bDB = buf(B.dist.data(), B.dist.size());
  id<MTLBuffer> bAct = buf(actTab.data(), actTab.size()), bALen = buf(actLen.data(), actLen.size());
  id<MTLBuffer> bSt = buf(states.data(), states.size());
  std::vector<uint8_t>().swap(states);
  const uint32_t recCap = 1u << 24;
  uint32_t leftCap = 1u << 22;  // grown on overflow (2026-11-11)
  const size_t clsCap = std::max<size_t>(batch, 1 << 16);
  id<MTLBuffer> bCls = zbuf(clsCap * 8), bLim = zbuf(clsCap), bCnt = zbuf(16), bRec = zbuf((size_t)recCap * 12);
  id<MTLBuffer> bLeft = zbuf(16), bLeftOut = zbuf((size_t)leftCap * 8), bClsInfo = zbuf(batch * 16);
  id<MTLBuffer> bPaths = nil, bPLen = nil, bUsed = nil, bItems = nil, bOk = nil;
  size_t pathsCap = 0, usedCap = 0, itemsCap = 0;
  struct AP { uint32_t nact, ncls, nA0, rad1, cap; };
  struct CP { uint32_t X, nF0, rad1, leftCap; };
  struct ClsInfo { uint32_t path0, npath, nstates, used0; };
  std::atomic<double> tAct{0}, tCpu{0};
  double tChk = 0;

  // the action kernel's buffers: one set for the batch being processed, one for the next batch being prepared
  // (2026-11-10: preparing batch n+1 -- the action kernel and the CPU path building -- overlaps batch n's checks)
  struct ActBufs { id<MTLBuffer> cls, lim, cnt, rec; };
  ActBufs abM = {bCls, bLim, bCnt, bRec}, abP = {zbuf(clsCap * 8), zbuf(clsCap), zbuf(16), zbuf((size_t)recCap * 12)};
  // the action kernel over a set of A states with limits, using the first nact actions: records
  // (state index, action, y); grouped == true: in groups that cannot overflow the record buffer
  auto runActsB = [&](ActBufs &ab, std::atomic<double> &tacc, const std::vector<std::array<uint8_t, 8>> &cs, const std::vector<uint8_t> &lim, std::vector<std::array<uint32_t, 3>> &recs, size_t nact = 0, bool grouped = false) {
    const double ta = now();
    if (!nact) nact = acts.size();
    recs.clear();
    const size_t G = grouped ? std::max<size_t>(1, std::min<size_t>(clsCap, recCap / nact)) : clsCap;
    for (size_t g0 = 0; g0 < cs.size(); g0 += G) {
      const size_t n = std::min(G, cs.size() - g0);
      memcpy([ab.cls contents], cs.data() + g0, n * 8);
      memcpy([ab.lim contents], lim.data() + g0, n);
      *(uint32_t *)[ab.cnt contents] = 0;
      AP P = {(uint32_t)nact, (uint32_t)n, (uint32_t)nA0, (uint32_t)A.rad1, recCap};
      id<MTLCommandBuffer> cb = [q commandBuffer];
      id<MTLComputeCommandEncoder> ce = [cb computeCommandEncoder];
      [ce setComputePipelineState:psActs];
      [ce setBuffer:ab.cls offset:0 atIndex:0];
      [ce setBuffer:bDA offset:0 atIndex:1];
      [ce setBuffer:bAct offset:0 atIndex:2];
      [ce setBuffer:bALen offset:0 atIndex:3];
      [ce setBuffer:ab.lim offset:0 atIndex:4];
      [ce setBuffer:ab.cnt offset:0 atIndex:5];
      [ce setBuffer:ab.rec offset:0 atIndex:6];
      [ce setBytes:&P length:sizeof P atIndex:7];
      [ce setBuffer:bBn offset:0 atIndex:8];
      [ce dispatchThreads:MTLSizeMake(nact, n, 1) threadsPerThreadgroup:MTLSizeMake(std::min<NSUInteger>(psActs.maxTotalThreadsPerThreadgroup, 256), 1, 1)];
      [ce endEncoding];
      [cb commit];
      [cb waitUntilCompleted];
      if (cb.status != MTLCommandBufferStatusCompleted) { fprintf(stderr, "GPU acts failed: %s\n", [[cb.error description] UTF8String]); exit(1); }
      const uint32_t c = *(uint32_t *)[ab.cnt contents];
      if (c > recCap) { fprintf(stderr, "action records overflow (%u > %u): use a smaller --batch\n", c, recCap); exit(1); }
      const uint32_t *r = (const uint32_t *)[ab.rec contents];
      for (uint32_t i = 0; i < c; i++) recs.push_back({(uint32_t)(r[3 * i] + g0), r[3 * i + 1], r[3 * i + 2]});
    }
    tacc.fetch_add(now() - ta);
  };

  // a path: act >= 0: action act (then action a2 if a2 >= 0), then the line of A state y; act < 0: symmetry -1 - act of L0
  struct Path { uint8_t c; int32_t act, a2; uint32_t y; std::array<uint8_t, 48> M; };
  auto hashM = [](const std::array<uint8_t, 48> &M) {
    uint64_t h = 0x9E3779B97F4A7C15ULL, w[6];
    memcpy(w, M.data(), 48);
    for (int i = 0; i < 6; i++) { h ^= w[i] + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2); h *= 0xBF58476D1CE4E5B9ULL; h ^= h >> 31; }
    return h;
  };
  long nOk = 0, nFail = 0, nDup = 0, nPass2 = 0, nDeep = 0;
  // --used-acts: how many used paths start with each action (a2: second actions of the deep pass)
  std::vector<uint64_t> usedAct(acts.size(), 0), usedAct2(acts.size(), 0);
  uint64_t nUsedSym = 0;
  uint64_t nBad = 0, nLeft = 0, nPaths = 0;
  std::vector<long> usedHist(64, 0), leftHist(5, 0);
  double lastRep = now();
  // one batch's classes and paths
  struct Batch {
    size_t b0 = 0, K = 0;
    std::vector<std::vector<cc::Mv>> L0;
    std::vector<std::array<int8_t, 48>> U, D0;
    std::vector<std::array<uint8_t, 8>> cs;
    std::vector<uint8_t> lim, skip;
    std::vector<std::vector<Path>> paths;
    std::vector<FlatSet> seen;
  };
auto pathMovesG = [&](Batch &bt, size_t k, const Path &p) {
  auto &L0 = bt.L0;
    std::vector<cc::Mv> mv;
    if (p.act < 0) mv = symMoves(allSyms[-1 - p.act], L0[k]);
    else {
      mv = acts[p.act].mv;
      if (p.a2 >= 0) mv.insert(mv.end(), acts[p.a2].mv.begin(), acts[p.a2].mv.end());
      auto l = A.lineOf(p.y);
      mv.insert(mv.end(), l.begin(), l.end());
    }
    simplify(mv);
    return mv;
  };
  // the paths of class k from records rs (and its symmetric copies of L0), cheapest first, new
  // maps only (seen: hashes of M; a collision only drops a path, never adds a wrong one)
  // (a1of: deep records, r[0] indexes a1of, r[1] = a2)
auto buildPathsG = [&](Batch &bt, size_t k, const std::vector<std::array<uint32_t, 3>> &rs, bool withSyms, FlatSet &seen, const std::vector<uint32_t> *a1of = nullptr) {
  auto &paths = bt.paths;
  auto &U = bt.U;
  auto &D0 = bt.D0;
  auto pathMoves = [&](size_t kk, const Path &pp) { return pathMovesG(bt, kk, pp); };
    std::vector<Path> &P = paths[k];
    const size_t first = P.size();
    P.reserve(first + rs.size() + A.syms.size());
    int inv0[48];
    for (int x = 0; x < 48; x++) inv0[(int)D0[k][x]] = x;
    auto add = [&](int32_t act, int32_t a2, uint32_t y) {
      Path p;
      p.act = act, p.a2 = a2, p.y = y;
      int8_t D[48];
      if (act < 0) {
        const auto mv = pathMoves(k, p);
        if ((int)mv.size() >= X) return;
        p.c = mv.size();
        mapOf(mv, D);
      } else {
        // the simplified length: simplify's rule (adjacent moves of one face and depth merge or cancel) on a
        // stack array, the moves a, a2, then the line under its symmetry; the map by composition
        cc::Mv st[96];
        int n = 0;
        auto push = [&](cc::Mv m) {
          if (n && st[n - 1].face == m.face && st[n - 1].depth == m.depth) {
            const int am = (st[n - 1].amt + m.amt) % 4;
            if (am) st[n - 1].amt = am;
            else n--;
          } else st[n++] = m;
        };
        for (auto &m : acts[act].mv) push(m);
        if (a2 >= 0) for (auto &m : acts[a2].mv) push(m);
        const uint32_t li = A.srcLine[y];
        const Sym &sy = allSyms[A.srcSym[y]];
        for (uint32_t j = A.lineOff[li]; j < A.lineOff[li + 1]; j++) {
          const cc::Mv m = byteMv(A.lineBytes[j]);
          push({sy.face[m.face], m.depth, sy.flip ? 4 - m.amt : m.amt});
        }
        if (n >= X) return;
        p.c = n;
        const int8_t *L = &lineMap[(size_t)li * 48], *Si = symInv[A.srcSym[y]].data();
        const int8_t *Da = acts[act].D, *Db = a2 >= 0 ? acts[a2].D : nullptr;
        for (int x = 0; x < 48; x++) {
          int z = Da[x];
          if (Db) z = Db[z];
          D[x] = sy.fac[L[Si[z]]];
        }
      }
      for (int x = 0; x < 48; x++) if (U[k][x] >= 0 && U[k][x] != groupOf(D[x])) { fprintf(stderr, "a path does not solve its class's state\n"); exit(1); }
      p.M.fill(0);
      for (int o = 0; o < 2; o++)
        for (size_t j = 0; j < B.freeO[o].size(); j++) {
          const int to = D[inv0[B.freeO[o][j]]];
          if (B.fIdx[to] < 0 || orbitOf(to) != o) { fprintf(stderr, "a path does not keep B's free facelets\n"); exit(1); }
          p.M[o * nB0 + j] = B.fIdx[to];
        }
      P.push_back(p);
    };
    if (withSyms)
      for (int s : A.syms) {
        int8_t img[48];
        for (int x = 0; x < 48; x++) img[allSyms[s].fac[x]] = U[k][x] < 0 ? -1 : allSyms[s].gmap[U[k][x]];
        if (memcmp(img, U[k].data(), 48) == 0) add(-1 - s, -1, 0);
      }
    for (auto &r : rs) {
      if (a1of) add((*a1of)[r[0]], r[1], r[2]);
      else add(r[1], -1, r[2]);
    }
    // cheapest first, stable (a counting sort by length into a per-thread scratch, kept unless huge), then new
    // maps only
    static thread_local std::vector<Path> tmp;
    const size_t np = P.size() - first;
    if (tmp.size() < np) tmp.resize(np);
    size_t at[257] = {0};
    for (size_t i = first; i < P.size(); i++) at[P[i].c + 1]++;
    for (int c = 1; c < 257; c++) at[c] += at[c - 1];
    for (size_t i = first; i < P.size(); i++) tmp[at[P[i].c]++] = P[i];
    size_t w = first;
    for (size_t i = 0; i < np; i++)
      if (seen.insert(hashM(tmp[i].M))) P[w++] = tmp[i];
    P.resize(w);
    // 2026-11-12: resize keeps the capacity of every candidate (with the pools ~0.37 GB a class, for every class of the
    // batch: UD 31's pass j at --batch 256 was killed at a 94 GB footprint); dedup keeps a few thousand of millions
    if (P.capacity() > 2 * P.size() + 4096) P.shrink_to_fit();
    if (tmp.size() > (1u << 22)) std::vector<Path>().swap(tmp);  // a pass-2 class at slack 5 with the pools: ~10M paths
  };
  // prepare a batch: its classes, the action kernel (buffers ab), the paths (CPU threads)
  auto prep = [&](Batch &bt, size_t b0, ActBufs &ab) {
    bt.b0 = b0, bt.K = std::min<size_t>(batch, todo.size() - b0);
    const size_t K = bt.K;
    // the classes: u, L0, its map, and the GPU form of u
    auto &L0 = bt.L0;
    L0.assign(K, {});
    auto &U = bt.U, &D0 = bt.D0;
    U.assign(K, {}), D0.assign(K, {});
    auto &cs = bt.cs;
    cs.assign(K, {});
    auto &lim = bt.lim, &skip = bt.skip;
    lim.assign(K, 0), skip.assign(K, 0);
    for (size_t k = 0; k < K; k++) {
      L0[k] = A.lineAt(todo[b0 + k]);
      int8_t u[48], b8[48];
      for (int x = 0; x < 48; x++) u[x] = ((A.S | A.T) >> groupOf(x) & 1) ? groupOf(x) : -1;
      for (auto it = L0[k].rbegin(); it != L0[k].rend(); ++it) {
        for (int x = 0; x < 48; x++) b8[mp[it->face][it->depth][4 - it->amt][x]] = u[x];
        memcpy(u, b8, 48);
      }
      memcpy(U[k].data(), u, 48);
      mapOf(L0[k], D0[k].data());
      for (int g = 0; g < 2; g++) {
        int n = 0;
        for (int x : A.freeO[g]) if (u[x] == A.tg[g]) cs[k][g * 4 + n++] = A.fIdx[x];
      }
      if (A.dist[A.rank8(cs[k].data())] != L0[k].size()) skip[k] = 1;  // a duplicate class line
      lim[k] = skip[k] ? 0 : L0[k].size() + 2;
    }
    std::vector<std::array<uint32_t, 3>> recs;
    runActsB(ab, tAct, cs, lim, recs);
    // paths per class
    const double tc = now();
    auto &paths = bt.paths;
    paths.assign(K, {});
    auto buildPaths = [&](size_t k, const std::vector<std::array<uint32_t, 3>> &rs, bool ws, FlatSet &sn) { buildPathsG(bt, k, rs, ws, sn); };
    auto &seen = bt.seen;  // reused across batches (cleared, not freed)
    if (seen.size() < K) seen.resize(K);
    for (size_t k = 0; k < K; k++) seen[k].clear();
    {
      // records grouped by class
      std::vector<std::vector<std::array<uint32_t, 3>>> byK(K);
      for (auto &r : recs) byK[r[0]].push_back(r);
      std::vector<std::array<uint32_t, 3>>().swap(recs);
      for (auto &v : byK) std::sort(v.begin(), v.end(), [](const auto &a, const auto &b) { return a[1] != b[1] ? a[1] < b[1] : a[2] < b[2]; });
      std::atomic<size_t> ni{0};
      std::vector<std::thread> th;
      for (int t = 0; t < threads; t++)
        th.emplace_back([&] {
          for (size_t k; (k = ni++) < K;) {
            if (skip[k]) continue;
            std::array<uint8_t, 48> M0{};
            // L0 itself: M = identity (its bad states are by definition not solved by it)
            for (int o = 0; o < 2; o++) for (size_t j = 0; j < B.freeO[o].size(); j++) M0[o * nB0 + j] = j;
            seen[k].insert(hashM(M0));
            buildPaths(k, byK[k], true, seen[k]);
          }
        });
      for (auto &x : th) x.join();
    }
    tCpu.fetch_add(now() - tc);
  };
  // check, pass 2, the deep pass and the blocks of a prepared batch
  auto process = [&](Batch &bt) {
    const size_t b0 = bt.b0, K = bt.K;
    auto &L0 = bt.L0;
    auto &U = bt.U;
    auto &cs = bt.cs;
    auto &skip = bt.skip;
    auto &paths = bt.paths;
    auto &seen = bt.seen;
    (void)U;
    auto runActs = [&](const std::vector<std::array<uint8_t, 8>> &c, const std::vector<uint8_t> &l, std::vector<std::array<uint32_t, 3>> &r, size_t nact = 0, bool grouped = false) { runActsB(abM, tAct, c, l, r, nact, grouped); };
    auto pathMoves = [&](size_t k, const Path &p) { return pathMovesG(bt, k, p); };
    auto buildPaths = [&](size_t k, const std::vector<std::array<uint32_t, 3>> &rs, bool ws, FlatSet &sn, const std::vector<uint32_t> *a1 = nullptr) { buildPathsG(bt, k, rs, ws, sn, a1); };
    // check on the GPU
    std::vector<uint32_t> leftCount(K, 0);
    std::vector<std::vector<uint32_t>> leftIdx(K);
    std::vector<std::vector<char>> used(K);
    // one GPU run; false (nothing changed) when the left states overflow the buffer: then the buffer is grown to the
    // count (exact: the kernel counts every left state) when that is at most 2^27 states (1 GB), and grew is set
    auto runCheckOnce = [&](const std::vector<size_t> &ks, bool &grew) {
      const double tk = now();
      std::vector<ClsInfo> ci(K, {0, 0, 0, 0});
      size_t np = 0, nu = 0;
      for (size_t k : ks) { ci[k].path0 = np; ci[k].npath = paths[k].size(); np += paths[k].size(); ci[k].used0 = nu; nu += (paths[k].size() + 31) / 32; }
      if (np > pathsCap) { pathsCap = np * 2; bPaths = zbuf(pathsCap * 48); bPLen = zbuf(pathsCap); }
      if (nu > usedCap) { usedCap = nu * 2 + 16; bUsed = zbuf(usedCap * 4); }
      uint8_t *pp = (uint8_t *)[bPaths contents], *pl = (uint8_t *)[bPLen contents];
      for (size_t k : ks)
        for (size_t i = 0; i < paths[k].size(); i++) { memcpy(pp + (ci[k].path0 + i) * 48, paths[k][i].M.data(), 48); pl[ci[k].path0 + i] = paths[k][i].c; }
      memset([bUsed contents], 0, nu * 4);
      *(uint32_t *)[bLeft contents] = 0;
      uint32_t mx = 0;
      for (size_t k : ks) {
        const int dmin = std::max<int>(X - (int)L0[k].size() + 1, lowT + 1);
        ci[k].nstates = cntAt[std::min(dmin, B.W + 1)];
        mx = std::max(mx, ci[k].nstates);
      }
      memcpy([bClsInfo contents], ci.data(), K * 16);
      CP P = {(uint32_t)X, (uint32_t)nB0, (uint32_t)B.rad1, leftCap};
      id<MTLCommandBuffer> cb = [q commandBuffer];
      id<MTLComputeCommandEncoder> ce = [cb computeCommandEncoder];
      [ce setComputePipelineState:psCheck];
      [ce setBuffer:bSt offset:0 atIndex:0];
      [ce setBuffer:bDB offset:0 atIndex:1];
      [ce setBuffer:bPaths offset:0 atIndex:2];
      [ce setBuffer:bPLen offset:0 atIndex:3];
      [ce setBuffer:bClsInfo offset:0 atIndex:4];
      [ce setBuffer:bUsed offset:0 atIndex:5];
      [ce setBuffer:bLeft offset:0 atIndex:6];
      [ce setBuffer:bLeftOut offset:0 atIndex:7];
      [ce setBytes:&P length:sizeof P atIndex:8];
      [ce setBuffer:bBn offset:0 atIndex:9];
      if (mx) [ce dispatchThreads:MTLSizeMake(mx, K, 1) threadsPerThreadgroup:MTLSizeMake(std::min<NSUInteger>(psCheck.maxTotalThreadsPerThreadgroup, 256), 1, 1)];
      [ce endEncoding];
      [cb commit];
      [cb waitUntilCompleted];
      if (cb.status != MTLCommandBufferStatusCompleted) { fprintf(stderr, "GPU check failed: %s\n", [[cb.error description] UTF8String]); exit(1); }
      const uint32_t nl = *(uint32_t *)[bLeft contents];
      if (nl > leftCap) {
        tChk += now() - tk;
        if ((grew = nl <= (1u << 27))) {
          leftCap = nl + nl / 4;
          bLeftOut = zbuf((size_t)leftCap * 8);
          fprintf(stderr, "  left states overflow: buffer grown to %u\n", leftCap);
        }
        return false;
      }
      const uint32_t *lo = (const uint32_t *)[bLeftOut contents];
      for (uint32_t j = 0; j < nl; j++) leftIdx[lo[2 * j]].push_back(lo[2 * j + 1]);
      const uint32_t *uw = (const uint32_t *)[bUsed contents];
      for (size_t k : ks) {
        used[k].assign(paths[k].size(), 0);
        for (size_t i = 0; i < paths[k].size(); i++) used[k][i] = uw[ci[k].used0 + i / 32] >> (i % 32) & 1;
        nBad += ci[k].nstates;
      }
      tChk += now() - tk;
      return true;
    };
    // on overflow: rerun once with the grown buffer (2026-11-11: UD 31's 15s overflowed ~40% of the batches, and splitting
    // reran each one's whole check at least twice), else split the classes in halves until the left states fit
    std::function<void(const std::vector<size_t> &)> runCheck = [&](const std::vector<size_t> &ks) {
      bool grew = false;
      if (ks.empty() || runCheckOnce(ks, grew) || (grew && runCheckOnce(ks, grew))) return;
      if (ks.size() == 1) { fprintf(stderr, "left states overflow for one class: raise leftCap\n"); exit(1); }
      fprintf(stderr, "  left states overflow: splitting %zu classes\n", ks.size());
      const std::vector<size_t> a(ks.begin(), ks.begin() + ks.size() / 2), b(ks.begin() + ks.size() / 2, ks.end());
      runCheck(a), runCheck(b);
    };
    std::vector<size_t> ks;
    for (size_t k = 0; k < K; k++) if (!skip[k]) ks.push_back(k);
    runCheck(ks);
    // drop the paths no state used (2026-11-12: with the pools every class kept all its pass-1 paths, and UD 31's pass j
    // reached a 94 GB footprint); the deep pass's dedup still sees their maps, as hashes (8 bytes a path)
    std::vector<std::vector<uint64_t>> pass1Hash(DEEP >= 0 ? K : 0);
    std::vector<size_t> dropped(K, 0);
    {
      std::atomic<size_t> ni{0};
      std::vector<std::thread> th;
      for (int t = 0; t < threads; t++)
        th.emplace_back([&] {
          for (size_t i; (i = ni++) < ks.size();) {
            const size_t k = ks[i];
            if (DEEP >= 0) {
              pass1Hash[k].reserve(paths[k].size());
              for (auto &p : paths[k]) pass1Hash[k].push_back(hashM(p.M));
            }
            size_t w = 0;
            for (size_t pi = 0; pi < paths[k].size(); pi++)
              if (used[k][pi]) paths[k][w] = paths[k][pi], used[k][w] = 1, w++;
            dropped[k] = paths[k].size() - w;
            paths[k].resize(w), used[k].resize(w);
            paths[k].shrink_to_fit(), used[k].shrink_to_fit();
          }
        });
      for (auto &x : th) x.join();
    }
    // pass 2: one action with more slack, on the states left (GPU check, 2026-11-11; it was on the CPU, for few states:
    // UD 31 pass b, every class hard, ran one CPU thread for an hour on its first batch); slack raised in
    // steps 3, 4, .., SL, each step on the classes still with states left and only on the actions new at
    // that slack (2026-11-04: at slack 5 nearly every action qualifies, ~1M paths per class; UD 33
    // spent 93% of its time there).  --nosteps: slack SL at once.
    // Class groups small enough that every action of every class fits the record buffer, each through all its slack
    // steps before the next group (2026-11-12: step-major, every class of the batch kept its seen set -- tens of
    // millions of maps with the pools at slack 5 -- through all steps; UD 31's pass j reached 94 GB); a group's seen
    // sets are released after its steps (nothing later reads them; the deep pass builds its own).
    std::vector<size_t> pass2First(K, 0);
    const size_t G = std::max<size_t>(1, recCap / acts.size());
    std::vector<size_t> cand;
    for (size_t k : ks) if (!leftIdx[k].empty()) cand.push_back(k);
    if (SL > 2) nPass2 += cand.size();
    for (size_t gc = 0; gc < cand.size(); gc += G) {
     const std::vector<size_t> grp(cand.begin() + gc, cand.begin() + std::min(cand.size(), gc + G));
     for (int sl = steps ? 3 : SL; sl <= SL && SL > 2; sl++) {
      std::vector<size_t> again;
      for (size_t k : grp) if (!leftIdx[k].empty()) again.push_back(k);
      if (again.empty()) break;
      for (size_t g0 = 0; g0 < again.size(); g0 += G) {
        const size_t ng = std::min(G, again.size() - g0);
        std::vector<std::array<uint8_t, 8>> cs2;
        std::vector<uint8_t> lim2;
        for (size_t i = 0; i < ng; i++) cs2.push_back(cs[again[g0 + i]]), lim2.push_back(L0[again[g0 + i]].size() + sl);
        std::vector<std::array<uint32_t, 3>> recs2;
        runActs(cs2, lim2, recs2);
        std::vector<std::vector<std::array<uint32_t, 3>>> byK(ng);
        for (auto &r : recs2)
          if (!steps || sl == 3 || (int)(actLen[r[1]] + A.dist[r[2]]) > (int)L0[again[g0 + r[0]]].size() + sl - 1) byK[r[0]].push_back(r);
        std::vector<std::array<uint32_t, 3>>().swap(recs2);
        const double tc2 = now();
        auto parallel = [&](const std::function<void(size_t)> &fn) {
          std::atomic<size_t> ni{0};
          std::vector<std::thread> th;
          for (int t = 0; t < threads; t++) th.emplace_back([&] { for (size_t i; (i = ni++) < ng;) fn(i); });
          for (auto &x : th) x.join();
        };
        parallel([&](size_t i) {
          const size_t k = again[g0 + i];
          std::sort(byK[i].begin(), byK[i].end(), [](const auto &a, const auto &b) { return a[1] != b[1] ? a[1] < b[1] : a[2] < b[2]; });
          pass2First[k] = paths[k].size();
          buildPaths(k, byK[i], false, seen[k]);
          std::vector<std::array<uint32_t, 3>>().swap(byK[i]);
          used[k].resize(paths[k].size(), 0);
        });
        tCpu.fetch_add(now() - tc2);
        // the left states through the new paths in order, on the GPU: one thread per (class, left state)
        {
          const double tk = now();
          std::vector<ClsInfo> ci(ng);
          size_t np = 0, nu = 0, ns = 0;
          for (size_t i = 0; i < ng; i++) {
            const size_t k = again[g0 + i], n = paths[k].size() - pass2First[k];
            ci[i] = {(uint32_t)np, (uint32_t)n, 0, (uint32_t)nu};
            np += n, nu += (n + 31) / 32, ns += leftIdx[k].size();
          }
          if (np > pathsCap) { pathsCap = np * 2; bPaths = zbuf(pathsCap * 48); bPLen = zbuf(pathsCap); }
          if (nu > usedCap) { usedCap = nu * 2 + 16; bUsed = zbuf(usedCap * 4); }
          if (ns > itemsCap) { itemsCap = ns * 2; bItems = zbuf(itemsCap * 8); bOk = zbuf(itemsCap); }
          uint8_t *pp = (uint8_t *)[bPaths contents], *pl = (uint8_t *)[bPLen contents];
          uint32_t *it = (uint32_t *)[bItems contents];
          for (size_t i = 0, m = 0; i < ng; i++) {
            const size_t k = again[g0 + i];
            for (size_t j = 0; j < ci[i].npath; j++) {
              const Path &q = paths[k][pass2First[k] + j];
              memcpy(pp + (ci[i].path0 + j) * 48, q.M.data(), 48);
              pl[ci[i].path0 + j] = q.c;
            }
            for (uint32_t si : leftIdx[k]) it[2 * m] = i, it[2 * m + 1] = si, m++;
          }
          memset([bUsed contents], 0, nu * 4);
          memcpy([bClsInfo contents], ci.data(), ng * 16);
          CP P = {(uint32_t)X, (uint32_t)nB0, (uint32_t)B.rad1, leftCap};
          const uint32_t n32 = ns;
          id<MTLCommandBuffer> cb = [q commandBuffer];
          id<MTLComputeCommandEncoder> ce = [cb computeCommandEncoder];
          [ce setComputePipelineState:psCheck2];
          [ce setBuffer:bSt offset:0 atIndex:0];
          [ce setBuffer:bDB offset:0 atIndex:1];
          [ce setBuffer:bPaths offset:0 atIndex:2];
          [ce setBuffer:bPLen offset:0 atIndex:3];
          [ce setBuffer:bClsInfo offset:0 atIndex:4];
          [ce setBuffer:bUsed offset:0 atIndex:5];
          [ce setBuffer:bItems offset:0 atIndex:6];
          [ce setBuffer:bOk offset:0 atIndex:7];
          [ce setBytes:&P length:sizeof P atIndex:8];
          [ce setBuffer:bBn offset:0 atIndex:9];
          [ce setBytes:&n32 length:4 atIndex:10];
          if (ns) [ce dispatchThreads:MTLSizeMake(ns, 1, 1) threadsPerThreadgroup:MTLSizeMake(std::min<NSUInteger>(psCheck2.maxTotalThreadsPerThreadgroup, 256), 1, 1)];
          [ce endEncoding];
          [cb commit];
          [cb waitUntilCompleted];
          if (cb.status != MTLCommandBufferStatusCompleted) { fprintf(stderr, "GPU check (pass 2) failed: %s\n", [[cb.error description] UTF8String]); exit(1); }
          const uint8_t *ok = (const uint8_t *)[bOk contents];
          const uint32_t *uw = (const uint32_t *)[bUsed contents];
          for (size_t i = 0, m = 0; i < ng; i++) {
            const size_t k = again[g0 + i];
            for (size_t j = 0; j < ci[i].npath; j++) if (uw[ci[i].used0 + j / 32] >> (j % 32) & 1) used[k][pass2First[k] + j] = 1;
            std::vector<uint32_t> still;
            for (uint32_t si : leftIdx[k]) { if (!ok[m]) still.push_back(si); m++; }
            leftIdx[k].swap(still);
          }
          tChk += now() - tk;
        }
        // keep only the paths a block needs
        const double tc3 = now();
        parallel([&](size_t i) {
          const size_t k = again[g0 + i];
          std::vector<Path> keep;
          std::vector<char> ku;
          for (size_t pi = 0; pi < paths[k].size(); pi++) if (pi < pass2First[k] || used[k][pi]) keep.push_back(paths[k][pi]), ku.push_back(used[k][pi]);
          paths[k].swap(keep), used[k].swap(ku);
        });
        tCpu.fetch_add(now() - tc3);
      }
     }
     for (size_t k : grp) seen[k].release();
    }
    for (size_t k : ks) seen[k].release();  // the classes that had no pass 2
    // pass 3 (--deep): two actions then the line, for the classes with states left, one at a time
    if (DEEP >= 0) {
      for (size_t k : ks) {
        if (leftIdx[k].empty()) continue;
        nDeep++;
        const int c0 = L0[k].size();
        std::vector<std::array<uint32_t, 3>> r1;
        runActs({cs[k]}, {(uint8_t)(c0 + SL + DEEP)}, r1, nDeepAct, true);
        std::sort(r1.begin(), r1.end(), [](const auto &a, const auto &b) { return a[1] < b[1]; });
        std::vector<std::array<uint8_t, 8>> ys;
        std::vector<uint8_t> lim2;
        std::vector<uint32_t> a1of;
        for (auto &r : r1) {
          const int l = c0 + SL - (int)actLen[r[1]];
          if (l < 1) continue;
          std::array<uint8_t, 8> y;
          for (int j = 0; j < 8; j++) y[j] = actTab[(size_t)r[1] * 48 + (j < 4 ? 0 : nA0) + cs[k][j]];
          ys.push_back(y), lim2.push_back(l), a1of.push_back(r[1]);
        }
        // in chunks of a1 states, one GPU group each (2026-11-10: all (a1, a2) records and their paths at once took
        // 110 GB with stage actions); after each chunk the left states try its paths; stop when none are left.
        // Dedup within a chunk and against the earlier passes' paths (not across chunks, so a cheaper path of a
        // later chunk is never dropped for a dearer one with the same map)
        static FlatSet sk0, sk;
        sk0.clear();
        for (uint64_t h : pass1Hash[k]) sk0.insert(h);
        for (auto &p : paths[k]) sk0.insert(hashM(p.M));
        std::vector<uint64_t>().swap(pass1Hash[k]);
        const size_t CH = std::max<size_t>(1, std::min<size_t>(clsCap, (recCap / 4) / std::max<size_t>(1, nDeepAct)));
        const uint8_t *all = (const uint8_t *)[bSt contents];
        double tc3sum = 0;
        for (size_t c0i = 0; c0i < ys.size() && !leftIdx[k].empty(); c0i += CH) {
          const size_t c1i = std::min(ys.size(), c0i + CH);
          const std::vector<std::array<uint8_t, 8>> ysc(ys.begin() + c0i, ys.begin() + c1i);
          const std::vector<uint8_t> limc(lim2.begin() + c0i, lim2.begin() + c1i);
          const std::vector<uint32_t> a1c(a1of.begin() + c0i, a1of.begin() + c1i);
          std::vector<std::array<uint32_t, 3>> r2;
          runActs(ysc, limc, r2, nDeepAct, true);
          const double tc3 = now();
          std::sort(r2.begin(), r2.end(), [&](const auto &a, const auto &b) { return a1c[a[0]] != a1c[b[0]] ? a1c[a[0]] < a1c[b[0]] : a[1] != b[1] ? a[1] < b[1] : a[2] < b[2]; });
          sk = sk0;
          const size_t from = paths[k].size();
          buildPaths(k, r2, false, sk, &a1c);
          std::vector<std::array<uint32_t, 3>>().swap(r2);
          used[k].resize(paths[k].size(), 0);
          // the left states through the new paths in order (CPU, threads over states)
          std::vector<int64_t> hit(leftIdx[k].size(), -1);
          std::atomic<size_t> ni{0};
          std::vector<std::thread> th;
          for (int t = 0; t < threads; t++)
            th.emplace_back([&] {
              for (size_t i; (i = ni++) < leftIdx[k].size();) {
                const uint8_t *st = all + (size_t)leftIdx[k][i] * 8;
                for (size_t pi = from; pi < paths[k].size(); pi++) {
                  uint8_t t8[8];
                  for (int j = 0; j < 4; j++) t8[j] = paths[k][pi].M[st[j]], t8[4 + j] = paths[k][pi].M[nB0 + st[4 + j]];
                  if (paths[k][pi].c + B.dist[B.rank8(t8)] <= X) { hit[i] = pi; break; }
                }
              }
            });
          for (auto &x : th) x.join();
          std::vector<uint32_t> still;
          for (size_t i = 0; i < hit.size(); i++) { if (hit[i] < 0) still.push_back(leftIdx[k][i]); else used[k][hit[i]] = 1; }
          leftIdx[k].swap(still);
          // keep the earlier paths and this chunk's used ones
          size_t w = from;
          for (size_t pi = from; pi < paths[k].size(); pi++)
            if (used[k][pi]) paths[k][w] = paths[k][pi], used[k][w] = 1, w++;
          paths[k].resize(w), used[k].resize(w);
          tc3sum += now() - tc3;
        }
        tCpu.fetch_add(tc3sum);
      }
    }
    // blocks
    for (size_t k = 0; k < K; k++) {
      if (skip[k]) { nDup++; continue; }
      nPaths += paths[k].size() + dropped[k];
      int nu = 0;
      // the block's text in a reused buffer; a used path's moves as in add (simplify's rule on a stack array)
      static std::string blk;
      blk.clear();
      auto line = [&](const cc::Mv *mv, int n) {
        for (int i = 0; i < n; i++) { if (i) blk.push_back(' '); appendMv(blk, mv[i]); }
        blk.push_back('\n');
      };
      line(L0[k].data(), L0[k].size());
      for (size_t i = 0; i < paths[k].size(); i++) {
        if (!used[k][i]) continue;
        nu++;
        const Path &p = paths[k][i];
        if (p.act < 0) { const auto mv = pathMoves(k, p); line(mv.data(), mv.size()); nUsedSym++; continue; }
        usedAct[p.act]++;
        if (p.a2 >= 0) usedAct2[p.a2]++;
        cc::Mv st[96];
        int n = 0;
        auto push = [&](cc::Mv m) {
          if (n && st[n - 1].face == m.face && st[n - 1].depth == m.depth) {
            const int am = (st[n - 1].amt + m.amt) % 4;
            if (am) st[n - 1].amt = am;
            else n--;
          } else st[n++] = m;
        };
        for (auto &m : acts[p.act].mv) push(m);
        if (p.a2 >= 0) for (auto &m : acts[p.a2].mv) push(m);
        const uint32_t li = A.srcLine[p.y];
        const Sym &sy = allSyms[A.srcSym[p.y]];
        for (uint32_t j = A.lineOff[li]; j < A.lineOff[li + 1]; j++) {
          const cc::Mv m = byteMv(A.lineBytes[j]);
          push({sy.face[m.face], m.depth, sy.flip ? 4 - m.amt : m.amt});
        }
        line(st, n);
      }
      blk.push_back('\n');
      fwrite(blk.data(), 1, blk.size(), out);
      usedHist[std::min(nu, 63)]++;
      const size_t lf = leftIdx[k].size();
      nLeft += lf;
      leftHist[lf == 0 ? 0 : lf < 10 ? 1 : lf < 100 ? 2 : lf < 1000 ? 3 : 4]++;
      if (lf) { nFail++; if (leftOut) fprintf(leftOut, "%s\n", text(L0[k]).c_str()); }
      else nOk++;
    }
    if (now() - lastRep > 30 || b0 + K == todo.size()) {
      const double el = now() - t0;
      fprintf(stderr, "  %zu/%zu classes, %ld not covered, %ld to pass 2, %ld to pass 3; GPU acts %.0fs, CPU paths %.0fs, GPU check %.0fs; %.0fs\n", b0 + K, todo.size(), nFail, nPass2, nDeep, tAct.load(), tCpu.load(), tChk, el);
      lastRep = now();
    }
  };
  // batch n is processed while batch n + 1 is prepared on another thread
  Batch bat[2];
  if (!todo.empty()) {
    @autoreleasepool { prep(bat[0], 0, abP); }
  }
  for (size_t b0 = 0, cur = 0; b0 < todo.size(); b0 += batch, cur ^= 1) {
    std::thread next;
    if (b0 + batch < todo.size())
      next = std::thread([&, nb = b0 + batch, nx = cur ^ 1] {
        @autoreleasepool { prep(bat[nx], nb, abP); }
      });
    @autoreleasepool {  // drain the Metal command buffers and encoders every batch (2026-11-04: they piled up)
      process(bat[cur]);
    }
    if (next.joinable()) next.join();
  }
  if (outZst) { if (pclose(out) != 0) { fprintf(stderr, "zstd failed writing %s\n", outFile.c_str()); return 1; } }
  else fclose(out);
  if (!usedActsFile.empty()) {
    FILE *ua = fopen(usedActsFile.c_str(), "w");
    if (!ua) { fprintf(stderr, "cannot write %s\n", usedActsFile.c_str()); return 1; }
    std::vector<size_t> ix;
    for (size_t a = 0; a < acts.size(); a++) if (usedAct[a] || usedAct2[a]) ix.push_back(a);
    std::stable_sort(ix.begin(), ix.end(), [&](size_t x, size_t y) { return usedAct[x] + usedAct2[x] > usedAct[y] + usedAct2[y]; });
    fprintf(ua, "# used actions: %zu of %zu; used paths that are symmetric copies of L0: %llu\n# uses-as-first\tuses-as-second\tlength\tmoves\n", ix.size(), acts.size(), (unsigned long long)nUsedSym);
    for (size_t a : ix) fprintf(ua, "%llu\t%llu\t%zu\t%s\n", (unsigned long long)usedAct[a], (unsigned long long)usedAct2[a], acts[a].mv.size(), text(acts[a].mv).c_str());
    fclose(ua);
    fprintf(stderr, "used actions: %zu of %zu (%s)\n", ix.size(), acts.size(), usedActsFile.c_str());
  }
  if (leftOut) fclose(leftOut);
  fprintf(stderr, "classes: %ld covered, %ld not (%ld duplicate lines skipped); bad colourings %lu, left %lu; %.1f alternatives per class (%.0fs)\n",
          nOk, nFail, nDup, (unsigned long)nBad, (unsigned long)nLeft, todo.size() ? (double)nPaths / todo.size() : 0.0, now() - t0);
  fprintf(stderr, "classes by colourings left: 0: %ld, 1-9: %ld, 10-99: %ld, 100-999: %ld, 1000+: %ld\n", leftHist[0], leftHist[1], leftHist[2], leftHist[3], leftHist[4]);
  fprintf(stderr, "alternatives used per class:");
  for (int i = 0; i < 64; i++) if (usedHist[i]) fprintf(stderr, " %d:%ld", i, usedHist[i]);
  fprintf(stderr, "\n");
  printf("%s %s X=%d: %ld of %zu classes not covered\n", A.name.c_str(), B.name.c_str(), X, nFail, todo.size() - nDup);
  return 0;
}
