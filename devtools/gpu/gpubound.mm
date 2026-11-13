// gpubound: boundcheck (devtools/boundcheck.cpp) with check 4, the per-block colouring loop, on the
// GPU (Metal).  Same arguments, same claim, same four checks:
//   1. both stage certificates cover their stages (dA, dB = line lengths over every symmetric image);
//   2. every block: legal moves, each line solves the first line's A state, side effects as
//      certcheck (--keep u|ud as boundcheck);
//   3. every A state with dA > X - max dB has a block for a symmetric image of it;
//   4. per block, with L its first line: every B state s with |L| + dB(s) > X is solved by some
//      line P of the block with |P| + dB(P carried) <= X.
// Checks 1-3 run on the CPU as boundcheck does (stage lines are streamed in chunks, so memory stays
// near the tables' size).  Check 4 on the GPU: B's target pieces are tracked by position only.  L
// solves A's state, so a target piece at state facelet p sits at J0 facelet D0^-1(p) and P carries
// it to M(p), M = DP o D0^-1; one GPU thread per (block, B state) moves the pieces through M of
// each line in turn, ranks them and reads dB.  Needs one target group per orbit in stage B (UD).
//
//   gpubound STAGEDIR_A STAGEDIR_B BOUNDARYFILE[.zst] X [--threads T] [--keep u|ud] [--blocks K]
//            [--drop-line B:I] [--drop-block B] [--cpu-sample N] [--reorder S]
// --blocks K: blocks per GPU dispatch (default 16); --drop-line/--drop-block: break the
// certificate on purpose (the run must FAIL); --cpu-sample N: also check every Nth block on the
// CPU the boundcheck way and compare its counts with the GPU's.  --reorder S: try each block's lines
// most-used first (any order is valid: a state needs some line with |P| + dB <= X).  The use counts come
// from every 2^S-th bad state, each counted for the first line (in file order) that solves it; lines
// are then sorted by count, ties in file order; --reorder-iters N repeats this N times, each counting in
// the previous order.  The CPU sample uses the same order.  --k1 K: a GPU thread per state tries the first
// K lines; states still unsolved are finished by a SIMD group each, 32 lines at a time (so one hard state
// does not hold up its SIMD group's other states).  Same counts as without.
#import <Metal/Metal.h>
#include "certcube.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <functional>
#include <unistd.h>
#include <mutex>
#include <thread>

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
static int mp[6][4][4][48];
static uint64_t BN[49][5];  // BN[n][k] = C(n, k), k <= 4
static uint64_t binom(int n, int k) {
  if (k < 0 || n < k) return 0;
  uint64_t r = 1;
  for (int i = 1; i <= k; i++) r = r * (n - k + i) / i;
  return r;
}
struct Sym { int fac[48]; int gmap[12]; };
static std::vector<Sym> allSyms;

// read a text file in chunks of lines (memory stays at one chunk); a .zst file is read through zstd -dc
// (2026-11-10, as gpuaxbcheck: UD 31 blocks would be tens of GB plain)
static bool forChunks(const std::string &f, size_t chunk, const std::function<void(std::vector<std::string> &)> &fn) {
  const bool zst = f.size() > 4 && f.compare(f.size() - 4, 4, ".zst") == 0;
  if (zst && access(f.c_str(), R_OK) != 0) return false;
  FILE *in = zst ? popen(("zstd -dc -q '" + f + "'").c_str(), "r") : fopen(f.c_str(), "r");
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
  if (zst) { if (pclose(in) != 0) { fprintf(stderr, "zstd -dc failed on %s\n", f.c_str()); exit(1); } }
  else fclose(in);
  return true;
}

struct Stage {
  std::string name;
  int S = 0, T = 0;
  std::vector<int> tg;
  std::vector<int> freeO[2];
  std::vector<int> syms;
  uint64_t total = 1;
  std::vector<uint8_t> dist;
  int W = 0;
  long lines = 0;

  uint64_t rank(const int8_t *col) const {
    uint64_t r = 0;
    for (int o = 0; o < 2; o++) {
      int left[24], rest[24], n = freeO[o].size();
      for (int i = 0; i < n; i++) left[i] = freeO[o][i];
      for (int g : tg) {
        if (g / 6 != o) continue;
        uint64_t sub = 0;
        int j = 0, m = 0;
        for (int i = 0; i < n; i++) {
          if (col[left[i]] == g) { j++; sub += BN[i][j]; }
          else rest[m++] = left[i];
        }
        r = r * BN[n][4] + sub;
        memcpy(left, rest, m * sizeof(int));
        n = m;
      }
    }
    return r;
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
    for (int k = 0; k < 48; k++) if (!(S >> groupOf(k) & 1)) freeO[(k % 8) / 4].push_back(k);
    for (int o = 0; o < 2; o++) {
      int n = freeO[o].size();
      for (int g : tg) if (g / 6 == o) { total *= binom(n, 4); n -= 4; }
    }
    for (size_t s = 0; s < allSyms.size(); s++) {
      int s2 = 0, t2 = 0;
      for (int g = 0; g < 12; g++) { if (S >> g & 1) s2 |= 1 << allSyms[s].gmap[g]; if (T >> g & 1) t2 |= 1 << allSyms[s].gmap[g]; }
      if (s2 == S && t2 == T) syms.push_back(s);
    }
    return true;
  }
  bool init(const std::string &dir, int threads) {
    *this = Stage();  // init parses the name itself
    if (!parseName(dir)) return false;
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
    dist.assign(total, 0xFF);
    std::atomic<long> bad{0};
    for (auto &f : fs) {
      const bool rd = forChunks(f, 1 << 19, [&](std::vector<std::string> &raw) {
        lines += raw.size();
        std::atomic<size_t> ni{0};
        std::vector<std::thread> th;
        for (int t = 0; t < threads; t++)
          th.emplace_back([&] {
            std::vector<cc::Mv> mv;
            int8_t a[48], b[48], img[48];
            for (size_t li; (li = ni++) < raw.size();) {
              bool ok = cc::parseMoves(raw[li], mv);
              for (auto &m : mv) ok &= m.depth >= 1 && m.depth <= 3;
              if (!ok) { bad++; continue; }
              for (int k = 0; k < 48; k++) a[k] = ((S | T) >> groupOf(k) & 1) ? groupOf(k) : -1;
              for (auto it = mv.rbegin(); it != mv.rend(); ++it) {
                for (int k = 0; k < 48; k++) b[mp[it->face][it->depth][4 - it->amt][k]] = a[k];
                memcpy(a, b, 48);
              }
              for (int s : syms) {
                for (int k = 0; k < 48; k++) img[allSyms[s].fac[k]] = a[k] < 0 ? -1 : allSyms[s].gmap[a[k]];
                const uint64_t r = rank(img);
                uint8_t cur = std::atomic_ref<uint8_t>(dist[r]).load();
                while (mv.size() < cur && !std::atomic_ref<uint8_t>(dist[r]).compare_exchange_weak(cur, mv.size())) {}
              }
            }
          });
        for (auto &x : th) x.join();
      });
      if (!rd) { fprintf(stderr, "cannot open a segment\n"); return false; }
    }
    long un = 0;
    for (uint64_t r = 0; r < total; r++) { if (dist[r] == 0xFF) un++; else W = std::max<int>(W, dist[r]); }
    printf("stage %s: %ld lines (%ld bad), %lu states, %ld not covered, longest line %d\n", name.c_str(), lines, bad.load(), (unsigned long)total, un, W);
    fflush(stdout);
    return !bad && !un;
  }
  void forEach(const std::function<void(const int8_t *)> &fn) const {
    int8_t col[48];
    for (int k = 0; k < 48; k++) col[k] = (S >> groupOf(k) & 1) ? groupOf(k) : -1;
    std::function<void(size_t)> rec = [&](size_t gi) {
      if (gi == tg.size()) { fn(col); return; }
      const auto &v = freeO[tg[gi] / 6];
      std::function<void(size_t, int)> pick = [&](size_t from, int depth) {
        if (depth == 4) { rec(gi + 1); return; }
        for (size_t i = from; i < v.size(); i++) {
          if (col[v[i]] != -1) continue;
          col[v[i]] = tg[gi];
          pick(i + 1, depth + 1);
          col[v[i]] = -1;
        }
      };
      pick(0, 0);
    };
    rec(0);
  }
};

// check 4 on the GPU.  A B state: per target group (tg order), 4 free-facelet indices (into its
// orbit's free list).  A line: per free-index slot (orbit 0 then 1), the free index M sends it to.
static const char *kSrc = R"(
#include <metal_stdlib>
using namespace metal;
struct Params { uint blk0, nblk, X, nF0, stride, rad1, shift, k1, cap; };
// does line i of block B take state (s0, s1) to within X?  (lines of X or more moves never do)
static inline bool solves(device const uchar *alts, device const uchar *alen, device const uchar *dist,
                          constant uint *bn, constant Params &P, uint i, uchar4 s0, uchar4 s1) {
  const uint len = alen[i];
  device const uchar *m = alts + i * P.stride;
  uint m0 = (1u << m[s0.x]) | (1u << m[s0.y]) | (1u << m[s0.z]) | (1u << m[s0.w]);
  uint m1 = (1u << m[P.nF0 + s1.x]) | (1u << m[P.nF0 + s1.y]) | (1u << m[P.nF0 + s1.z]) | (1u << m[P.nF0 + s1.w]);
  uint r0 = 0, r1 = 0;
  for (uint j = 1; j <= 4; j++) { r0 += bn[ctz(m0) * 5 + j]; m0 &= m0 - 1; r1 += bn[ctz(m1) * 5 + j]; m1 &= m1 - 1; }
  return len + dist[r0 * P.rad1 + r1] <= P.X;  // rank: r0, then r1 in radix C(nF1, 4)
}
static inline void addCounts(device atomic_uint *out, uint b, uint str, uint sbad) {
  // tries: 64 bits as low word + carries
  if (str) { const uint old = atomic_fetch_add_explicit(&out[3 * b], str, memory_order_relaxed); if (old > 0xFFFFFFFFu - str) atomic_fetch_add_explicit(&out[3 * b + 1], 1u, memory_order_relaxed); }
  if (sbad) atomic_fetch_add_explicit(&out[3 * b + 2], sbad, memory_order_relaxed);
}
struct Blk { uint alt0, nalt, nstates, pad; };
// phase 1: one thread per (block, state).  With P.k1 > 0 a thread tries only the first k1 lines; a state
// still unsolved then goes to the list (block, state, tries so far) for check2, unless the list is full
// (P.cap), when the thread goes on through the lines itself.  The counts are those of trying the lines
// in order, one thread per state, either way.
kernel void check(device const uchar4 *st [[buffer(0)]], device const uchar *dist [[buffer(1)]],
                  device const uchar *alts [[buffer(2)]], device const uchar *alen [[buffer(3)]],
                  device const Blk *blk [[buffer(4)]], device atomic_uint *out [[buffer(5)]],
                  constant Params &P [[buffer(6)]], constant uint *bn [[buffer(7)]],
                  device uint4 *list [[buffer(8)]], device atomic_uint *nlist [[buffer(9)]],
                  uint2 gid [[thread_position_in_grid]]) {
  const uint b = P.blk0 + gid.y;
  const Blk B = blk[b];
  const bool live = gid.x < B.nstates;
  uint tr = 0, bad = 0;
  if (live) {
    const uchar4 s0 = st[2 * gid.x], s1 = st[2 * gid.x + 1];
    bool good = false;
    const uint lim = (P.k1 && B.nalt > P.k1) ? P.k1 : B.nalt;
    uint i = 0;
    for (; i < lim && !good; i++) {
      if (alen[B.alt0 + i] >= P.X) continue;
      tr++;
      good = solves(alts, alen, dist, bn, P, B.alt0 + i, s0, s1);
    }
    if (!good && i < B.nalt) {
      const uint li = atomic_fetch_add_explicit(nlist, 1u, memory_order_relaxed);
      if (li < P.cap) { list[li] = uint4(b, gid.x, tr, 0); tr = 0; good = true; }  // counted by check2
      else
        for (; i < B.nalt && !good; i++) {
          if (alen[B.alt0 + i] >= P.X) continue;
          tr++;
          good = solves(alts, alen, dist, bn, P, B.alt0 + i, s0, s1);
        }
    }
    bad = good ? 0 : 1;
  }
  const uint str = simd_sum(tr), sbad = simd_sum(bad);
  if (simd_is_first()) addCounts(out, b, str, sbad);
}
// phase 2: one SIMD group per listed state; its lanes try lines k1 + lane, k1 + 32 + lane, ... and stop at
// the first round with a solving line.  Tries are counted as one thread in order would: the lines (under
// X moves) up to the first solving one.
kernel void check2(device const uchar4 *st [[buffer(0)]], device const uchar *dist [[buffer(1)]],
                   device const uchar *alts [[buffer(2)]], device const uchar *alen [[buffer(3)]],
                   device const Blk *blk [[buffer(4)]], device atomic_uint *out [[buffer(5)]],
                   constant Params &P [[buffer(6)]], constant uint *bn [[buffer(7)]],
                   device const uint4 *list [[buffer(8)]], constant uint &n [[buffer(9)]],
                   uint tid [[thread_position_in_grid]], uint lane [[thread_index_in_simdgroup]],
                   uint sw [[threads_per_simdgroup]]) {
  const uint e = tid / sw;
  if (e >= n) return;  // a whole SIMD group (the grid is a multiple of sw)
  const uint4 L = list[e];
  const Blk B = blk[L.x];
  const uchar4 s0 = st[2 * L.y], s1 = st[2 * L.y + 1];
  uint tr = L.z;
  bool good = false;
  for (uint base = P.k1; base < B.nalt; base += sw) {
    const uint i = base + lane;
    const bool valid = i < B.nalt && alen[B.alt0 + i] < P.X;
    const bool g = valid && solves(alts, alen, dist, bn, P, B.alt0 + i, s0, s1);
    const uint first = simd_min(g ? i : 0xFFFFFFFFu);
    if (first != 0xFFFFFFFFu) { tr += simd_sum((valid && i <= first) ? 1u : 0u); good = true; break; }
    tr += simd_sum(valid ? 1u : 0u);
  }
  if (lane == 0) addCounts(out, L.x, tr, good ? 0 : 1);
}
// --reorder: every 2^shift-th bad state; the first line that solves it gets a count
kernel void firsthit(device const uchar4 *st [[buffer(0)]], device const uchar *dist [[buffer(1)]],
                     device const uchar *alts [[buffer(2)]], device const uchar *alen [[buffer(3)]],
                     device const Blk *blk [[buffer(4)]], device atomic_uint *hits [[buffer(5)]],
                     constant Params &P [[buffer(6)]], constant uint *bn [[buffer(7)]],
                     uint2 gid [[thread_position_in_grid]]) {
  const Blk B = blk[P.blk0 + gid.y];
  const uint si = gid.x << P.shift;
  if (si >= B.nstates) return;
  const uchar4 s0 = st[2 * si], s1 = st[2 * si + 1];
  for (uint i = 0; i < B.nalt; i++)
    if (alen[B.alt0 + i] < P.X && solves(alts, alen, dist, bn, P, B.alt0 + i, s0, s1)) {
      atomic_fetch_add_explicit(&hits[B.alt0 + i], 1u, memory_order_relaxed);
      return;
    }
}
)";

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  int threads = 2, keep = 0, perDispatch = 16, cpuSample = 0, reorder = -1, reoIters = 1, k1 = 0;
  long dropBlock = -1, dropLineB = -1, dropLineI = -1;
  std::vector<std::string> pos;
  std::string leftFile;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--threads" && i + 1 < argc) threads = atoi(argv[++i]);
    else if (o == "--blocks" && i + 1 < argc) perDispatch = atoi(argv[++i]);
    else if (o == "--cpu-sample" && i + 1 < argc) cpuSample = atoi(argv[++i]);
    else if (o == "--reorder" && i + 1 < argc) reorder = atoi(argv[++i]);
    else if (o == "--reorder-iters" && i + 1 < argc) reoIters = atoi(argv[++i]);
    else if (o == "--k1" && i + 1 < argc) k1 = atoi(argv[++i]);
    else if (o == "--left" && i + 1 < argc) leftFile = argv[++i];
    else if (o == "--drop-block" && i + 1 < argc) dropBlock = atol(argv[++i]);
    else if (o == "--drop-line" && i + 1 < argc) {
      if (sscanf(argv[++i], "%ld:%ld", &dropLineB, &dropLineI) != 2) { fprintf(stderr, "--drop-line takes B:I\n"); return 1; }
    }
    else if (o == "--keep" && i + 1 < argc) {
      const std::string k = argv[++i];
      if (k == "u") keep = 1;
      else if (k == "ud") keep = 2;
      else { fprintf(stderr, "--keep takes u or ud\n"); return 1; }
    }
    else if (o.size() > 1 && o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else pos.push_back(o);
  }
  if (pos.size() != 4) {
    fprintf(stderr, "usage: gpubound STAGEDIR_A STAGEDIR_B BOUNDARYFILE[.zst] X [--threads T] [--keep u|ud] [--blocks K] [--drop-line B:I] [--drop-block B] [--cpu-sample N] [--left FILE] [--reorder S] [--reorder-iters N] [--k1 K]\n");
    return 1;
  }
  const int X = atoi(pos[3].c_str());
  // --left FILE: the first lines of the blocks with colourings not solved (input for a deeper --only rerun)
  FILE *leftOut = nullptr;
  if (!leftFile.empty() && !(leftOut = fopen(leftFile.c_str(), "w"))) { fprintf(stderr, "cannot write %s\n", leftFile.c_str()); return 1; }
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
    for (int pi = 0; pi < 6; pi++)
      for (int sg = 0; sg < 8; sg++) {
        Sym y;
        for (int k = 0; k < 48; k++) {
          const auto &p = C8.pos[pf[k]];
          std::array<int, 3> q;
          for (int r = 0; r < 3; r++) q[r] = p[pm[pi][r]] * ((sg >> r & 1) ? -1 : 1);
          int j = -1;
          for (int x = 0; x < 48; x++) if (C8.pos[pf[x]] == q) j = x;
          if (j < 0) { fprintf(stderr, "a symmetry leaves the pair\n"); return 1; }
          y.fac[k] = j;
        }
        for (int g = 0; g < 12; g++) {
          int img = -1;
          for (int k = 0; k < 48; k++) if (groupOf(k) == g) { const int h = groupOf(y.fac[k]); if (img >= 0 && img != h) { fprintf(stderr, "a symmetry splits a group\n"); return 1; } img = h; }
          y.gmap[g] = img;
        }
        allSyms.push_back(y);
      }
  }
  Stage A, B;
  // B first: its checks need only B's table; A's table is needed for check 3 at the end
  if (!B.parseName(pos[1]) || !A.parseName(pos[0])) return 1;
  if (B.S != (A.S | A.T)) { fprintf(stderr, "stage B must start where stage A ends\n"); return 1; }
  if (!std::includes(B.syms.begin(), B.syms.end(), A.syms.begin(), A.syms.end())) { fprintf(stderr, "stage A has a symmetry stage B lacks\n"); return 1; }
  int perOrbit[2] = {0, 0};
  for (int g : B.tg) perOrbit[g / 6]++;
  if (perOrbit[0] != 1 || perOrbit[1] != 1 || B.freeO[0].size() > 24 || B.freeO[1].size() > 24) { fprintf(stderr, "gpubound needs stage B to have one target group in each orbit\n"); return 1; }
  const double tB0 = now();
  if (!B.init(pos[1], threads)) { printf("FAIL: a stage certificate does not cover its stage\n"); return 1; }
  const double tB = now() - tB0;


  // pass 1 over the boundary file: block count and the longest first line (the B states needed)
  long nBlocks = 0;
  int maxL = 0;
  {
    bool inBlock = false;
    const bool rd = forChunks(pos[2], 1 << 16, [&](std::vector<std::string> &v) {
      for (auto &l : v) {
        if (l.empty()) { inBlock = false; continue; }
        if (!inBlock) {
          inBlock = true;
          nBlocks++;
          std::vector<cc::Mv> mv;
          if (cc::parseMoves(l, mv)) maxL = std::max<int>(maxL, mv.size());
        }
      }
    });
    if (!rd) { fprintf(stderr, "cannot read the boundary file\n"); return 1; }
  }
  printf("boundary: %ld blocks, X = %d, max dB = %d\n", nBlocks - (dropBlock >= 0 && dropBlock < nBlocks), X, B.W);
  fflush(stdout);

  // B states beyond the lowest threshold, longest first (so each block uses a prefix)
  const int lowT = std::max(0, X - maxL);
  int fIdx[48];
  const int nF0 = B.freeO[0].size();
  for (int k = 0; k < 48; k++) fIdx[k] = -1;
  for (int o = 0; o < 2; o++) for (size_t i = 0; i < B.freeO[o].size(); i++) fIdx[B.freeO[o][i]] = i;
  std::vector<std::vector<uint8_t>> byD(B.W + 1);
  B.forEach([&](const int8_t *col) {
    const int d = B.dist[B.rank(col)];
    if (d <= lowT) return;
    uint8_t s[8];
    for (int gi = 0; gi < 2; gi++) {
      const int g = B.tg[gi], o = g / 6;
      int n = 0;
      for (int k : B.freeO[o]) if (col[k] == g) s[gi * 4 + n++] = fIdx[k];
    }
    byD[d].insert(byD[d].end(), s, s + 8);
  });
  std::vector<uint8_t> states;
  std::vector<size_t> cntAt(B.W + 2, 0);  // cntAt[d] = states with dist >= d (they come first)
  for (int d = B.W; d >= 0; d--) {
    cntAt[d] = cntAt[d + 1] + byD[d].size() / 8;
    if (d <= lowT) continue;
    states.insert(states.end(), byD[d].begin(), byD[d].end());
    printf("  B states at %d: %zu\n", d, byD[d].size() / 8);
    std::vector<uint8_t>().swap(byD[d]);
  }
  fflush(stdout);

  // side effects on N = 6..10, as certcheck
  struct NC { int N; std::vector<char> kind; std::vector<std::vector<std::pair<int, int>>> mvMap; std::vector<int> faceOf; size_t F; };
  std::vector<NC> ncs;
  for (int N = 6; N <= 10; N++) {
    cc::Cube CN(N);
    NC n;
    n.N = N;
    n.F = CN.pos.size();
    n.faceOf = CN.faceOf;
    const int M = N - 1;
    n.kind.assign(n.F, 0);
    for (int f = 0; f < 6; f++)
      for (int r = 1; r < M; r++)
        for (int c = 1; c < M; c++) {
          bool pair = false;
          for (int o = 0; o < 2 && !pair; o++) {
            int x = o ? 2 : 1, y = o ? 1 : 2;
            for (int q = 0; q < 4; q++) { if (r == x && c == y) pair = true; const int nx = y, ny = M - x; x = nx, y = ny; }
          }
          n.kind[CN.index(f, r, c)] = pair ? 1 : (r == c || r + c == M) ? 2 : 3;
        }
    n.mvMap.assign(6 * 4 * 4, {});
    for (int f = 0; f < 6; f++)
      for (int d = 1; d <= 3; d++)
        for (int a = 1; a <= 3; a++)
          for (size_t i = 0; i < n.F; i++) {
            if (!n.kind[i]) continue;
            int j = i;
            for (int q = 0; q < a; q++) j = CN.perms[f][d - 1][j];
            if (j != (int)i) n.mvMap[(f * 4 + d) * 4 + a].push_back({(int)i, j});
          }
    ncs.push_back(n);
  }
  auto sideOk = [&](const std::vector<cc::Mv> &mv) {
    for (auto &n : ncs) {
      std::vector<int> a(n.F), b;
      for (size_t i = 0; i < n.F; i++) a[i] = i;
      for (auto &m : mv) {
        b = a;
        for (auto &[i, j] : n.mvMap[(m.face * 4 + m.depth) * 4 + m.amt]) b[j] = a[i];
        a.swap(b);
      }
      for (size_t i = 0; i < n.F; i++) {
        if (n.kind[i] != 3) continue;
        const int fa = n.faceOf[a[i]], fi = n.faceOf[i];
        if (keep == 0 ? fa != fi : (fa == 0) != (fi == 0) || (keep == 2 && (fa == 5) != (fi == 5))) return false;
      }
    }
    return true;
  };

  // the GPU, once
  id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
  if (!dev) { fprintf(stderr, "no Metal device\n"); return 1; }
  NSError *err = nil;
  id<MTLLibrary> lib = [dev newLibraryWithSource:[NSString stringWithUTF8String:kSrc] options:nil error:&err];
  if (!lib) { fprintf(stderr, "Metal compile: %s\n", [[err description] UTF8String]); return 1; }
  id<MTLComputePipelineState> ps = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"check"] error:&err];
  if (!ps) { fprintf(stderr, "pipeline: %s\n", [[err description] UTF8String]); return 1; }
  id<MTLComputePipelineState> ps2 = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"check2"] error:&err];
  if (!ps2) { fprintf(stderr, "pipeline: %s\n", [[err description] UTF8String]); return 1; }
  const uint32_t simdW = (uint32_t)ps2.threadExecutionWidth;
  // --k1: the list of states for check2 (16 bytes each)
  const uint32_t listCap = 1u << 24;
  id<MTLBuffer> bList = [dev newBufferWithLength:(k1 ? (size_t)listCap * 16 : 16) options:MTLResourceStorageModePrivate];
  id<MTLBuffer> bNList = [dev newBufferWithLength:16 options:MTLResourceStorageModeShared];
  uint64_t nListed = 0;
  id<MTLComputePipelineState> psHit = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"firsthit"] error:&err];
  if (!psHit) { fprintf(stderr, "pipeline: %s\n", [[err description] UTF8String]); return 1; }
  id<MTLCommandQueue> q = [dev newCommandQueue];
  // at least 16 bytes; an empty input gets a zeroed buffer (2026-11-09: newBufferWithBytes read 16 bytes from
  // a null pointer when every block of a file had failed check 2, so no line was left)
  auto buf = [&](const void *p, size_t n) {
    return n ? [dev newBufferWithBytes:p length:std::max<size_t>(n, 16) options:MTLResourceStorageModeShared]
             : [dev newBufferWithLength:16 options:MTLResourceStorageModeShared];
  };
  id<MTLBuffer> bSt = buf(states.data(), states.size()), bDist = buf(B.dist.data(), B.dist.size());
  std::vector<uint32_t> bn(25 * 5);
  for (int n = 0; n < 25; n++) for (int k = 0; k < 5; k++) bn[n * 5 + k] = BN[n][k];
  id<MTLBuffer> bBn = buf(bn.data(), bn.size() * 4);
  struct Params { uint32_t blk0, nblk, X, nF0, stride, rad1, shift, k1, cap; };
  const NSUInteger tgs = std::min<NSUInteger>(ps.maxTotalThreadsPerThreadgroup, 256);

  // the blocks in chunks: check 2 (CPU), check 4 (GPU), the CPU sample, and each block's A state
  // images into hasBlock for check 3
  const int stride = 48;
  struct BI { uint32_t alt0, nalt, nstates, pad; };
  std::vector<uint64_t> hasBlock((A.total + 63) / 64, 0);
  long badBlock = 0, badLine = 0, sideBad = 0, notSolved = 0, frameErr = 0, leftBlocks = 0, sampleMismatch = 0, nSampled = 0, nLines = 0;
  uint64_t pairs = 0, tried = 0, left = 0;
  double tGpu = 0, tC2 = 0, tSamp = 0, tReo = 0, lastRep = now();
  auto process = [&](std::vector<std::vector<std::string>> &blocks, long g0) {
    const double tp0 = now();
    std::vector<BI> bi(blocks.size());
    std::vector<uint8_t> blockOk(blocks.size(), 0);
    std::vector<std::array<int8_t, 48>> U(blocks.size());
    std::vector<uint8_t> alts, alen;
    std::vector<std::vector<std::array<int8_t, 48>>> Dall(blocks.size());
    std::vector<std::vector<uint32_t>> perm(blocks.size());  // --reorder: GPU line i is the block's line perm[b][i]
    {
      std::vector<int> ok(blocks.size(), 0), bl(blocks.size(), 0), ns(blocks.size(), 0), sb(blocks.size(), 0);
      std::atomic<size_t> ni{0};
      std::vector<std::thread> th;
      for (int t = 0; t < threads; t++)
        th.emplace_back([&] {
          int8_t u[48], b8[48];
          for (size_t b; (b = ni++) < blocks.size();) {
            const auto &blk = blocks[b];
            std::vector<std::vector<cc::Mv>> mvs(blk.size());
            bool good = true;
            for (size_t i = 0; i < blk.size(); i++) {
              bool lo = cc::parseMoves(blk[i], mvs[i]) && !mvs[i].empty();
              for (auto &m : mvs[i]) lo &= m.depth >= 1 && m.depth <= 3;
              if (!lo) { bl[b]++; good = false; }
            }
            if (!good) continue;
            for (int k = 0; k < 48; k++) u[k] = ((A.S | A.T) >> groupOf(k) & 1) ? groupOf(k) : -1;
            for (auto it = mvs[0].rbegin(); it != mvs[0].rend(); ++it) {
              for (int k = 0; k < 48; k++) b8[mp[it->face][it->depth][4 - it->amt][k]] = u[k];
              memcpy(u, b8, 48);
            }
            memcpy(U[b].data(), u, 48);
            auto &D = Dall[b];
            D.resize(blk.size());
            for (size_t i = 0; i < blk.size() && good; i++) {
              for (int k = 0; k < 48; k++) {
                int x = k;
                for (auto &m : mvs[i]) x = mp[m.face][m.depth][m.amt][x];
                D[i][k] = x;
              }
              for (int k = 0; k < 48; k++) if (u[k] >= 0 && u[k] != groupOf(D[i][k])) { good = false; break; }
              if (!good) { ns[b]++; break; }
              if (!sideOk(mvs[i])) { sb[b]++; good = false; }
            }
            ok[b] = good;
          }
        });
      for (auto &x : th) x.join();
      for (size_t b = 0; b < blocks.size(); b++) {
        badLine += bl[b], notSolved += ns[b], sideBad += sb[b];
        if (!ok[b]) { badBlock++; continue; }
        blockOk[b] = 1;
      }
    }
    // M = D_P o D_0^-1 on B's free facelets, in free-index space
    for (size_t b = 0; b < blocks.size(); b++) {
      bi[b] = {(uint32_t)alen.size(), 0, 0, 0};
      nLines += blocks[b].size();
      if (!blockOk[b]) continue;
      const auto &D = Dall[b];
      int inv0[48];
      for (int k = 0; k < 48; k++) inv0[(int)D[0][k]] = k;
      for (size_t i = 0; i < D.size(); i++) {
        std::vector<cc::Mv> mv;
        cc::parseMoves(blocks[b][i], mv);
        alen.push_back(std::min<size_t>(mv.size(), 255));
        uint8_t row[48] = {0};
        for (int o = 0; o < 2; o++)
          for (size_t j = 0; j < B.freeO[o].size(); j++) {
            const int to = D[i][inv0[B.freeO[o][j]]];
            if (fIdx[to] < 0 || (to % 8) / 4 != o) frameErr++;  // a target piece would land on a solved facelet
            row[o * nF0 + j] = fIdx[to] < 0 ? 0 : fIdx[to];
          }
        alts.insert(alts.end(), row, row + stride);
      }
      bi[b].nalt = D.size();
      // the B states with |L| + d > X
      const int dmin = std::max(X - (int)alen[bi[b].alt0] + 1, lowT + 1);
      bi[b].nstates = cntAt[std::min(dmin, B.W + 1)];
      pairs += bi[b].nstates;
      int8_t img[48];
      for (int s : A.syms) {
        for (int k = 0; k < 48; k++) img[allSyms[s].fac[k]] = U[b][k] < 0 ? -1 : allSyms[s].gmap[U[b][k]];
        const uint64_t r = A.rank(img);
        hasBlock[r >> 6] |= 1ULL << (r & 63);
      }
    }
    tC2 += now() - tp0;
    // --reorder: count, on every 2^reorder-th bad state, the first line that solves it; then put each
    // block's lines in order of count (stable, so ties stay in file order)
    for (size_t b = 0; b < blocks.size(); b++) {
      perm[b].resize(bi[b].nalt);
      for (uint32_t i = 0; i < bi[b].nalt; i++) perm[b][i] = i;
    }
    if (reorder >= 0 && !alen.empty()) {
     const double tr0 = now();
     std::vector<uint8_t> a2, l2;
     std::vector<uint32_t> hits(alen.size()), qv, p2;
     for (int it = 0; it < reoIters; it++) {
      @autoreleasepool {
        id<MTLBuffer> bAlt = buf(alts.data(), alts.size()), bLen = buf(alen.data(), alen.size()), bBi = buf(bi.data(), bi.size() * sizeof(BI));
        id<MTLBuffer> bHit = [dev newBufferWithLength:std::max<size_t>(hits.size() * 4, 16) options:MTLResourceStorageModeShared];
        memset([bHit contents], 0, hits.size() * 4);
        for (size_t b0 = 0; b0 < blocks.size(); b0 += perDispatch) {
          const size_t nb = std::min<size_t>(perDispatch, blocks.size() - b0);
          uint32_t mx = 0;
          for (size_t b = b0; b < b0 + nb; b++) if (bi[b].nalt > 1) mx = std::max(mx, (bi[b].nstates + (1u << reorder) - 1) >> reorder);
          if (!mx) continue;
          Params P = {(uint32_t)b0, (uint32_t)nb, (uint32_t)X, (uint32_t)nF0, (uint32_t)stride, (uint32_t)BN[B.freeO[1].size()][4], (uint32_t)reorder, 0, 0};
          id<MTLCommandBuffer> cb = [q commandBuffer];
          id<MTLComputeCommandEncoder> ce = [cb computeCommandEncoder];
          [ce setComputePipelineState:psHit];
          [ce setBuffer:bSt offset:0 atIndex:0];
          [ce setBuffer:bDist offset:0 atIndex:1];
          [ce setBuffer:bAlt offset:0 atIndex:2];
          [ce setBuffer:bLen offset:0 atIndex:3];
          [ce setBuffer:bBi offset:0 atIndex:4];
          [ce setBuffer:bHit offset:0 atIndex:5];
          [ce setBytes:&P length:sizeof P atIndex:6];
          [ce setBuffer:bBn offset:0 atIndex:7];
          [ce dispatchThreads:MTLSizeMake(mx, nb, 1) threadsPerThreadgroup:MTLSizeMake(tgs, 1, 1)];
          [ce endEncoding];
          [cb commit];
          [cb waitUntilCompleted];
          if (cb.status != MTLCommandBufferStatusCompleted) { fprintf(stderr, "GPU command failed at block %ld: %s\n", g0 + (long)b0, [[cb.error description] UTF8String]); exit(1); }
        }
        memcpy(hits.data(), [bHit contents], hits.size() * 4);
      }
      // hits are by the current position; q = positions by count, then the lines move and perm follows
      for (size_t b = 0; b < blocks.size(); b++) {
        const uint32_t a0 = bi[b].alt0, n = bi[b].nalt;
        if (n < 2) continue;
        qv.resize(n);
        for (uint32_t i = 0; i < n; i++) qv[i] = i;
        std::stable_sort(qv.begin(), qv.end(), [&](uint32_t x, uint32_t y) { return hits[a0 + x] > hits[a0 + y]; });
        a2.assign(alts.begin() + (size_t)a0 * stride, alts.begin() + (size_t)(a0 + n) * stride);
        l2.assign(alen.begin() + a0, alen.begin() + a0 + n);
        p2 = perm[b];
        for (uint32_t i = 0; i < n; i++) {
          memcpy(&alts[(size_t)(a0 + i) * stride], &a2[(size_t)qv[i] * stride], stride);
          alen[a0 + i] = l2[qv[i]];
          perm[b][i] = p2[qv[i]];
        }
      }
     }
     tReo += now() - tr0;
    }
    // check 4 on the GPU
    std::vector<uint32_t> res(3 * blocks.size(), 0);
    @autoreleasepool {
      id<MTLBuffer> bAlt = buf(alts.data(), alts.size()), bLen = buf(alen.data(), alen.size()), bBi = buf(bi.data(), bi.size() * sizeof(BI));
      id<MTLBuffer> bOut = [dev newBufferWithLength:std::max<size_t>(res.size() * 4, 16) options:MTLResourceStorageModeShared];
      memset([bOut contents], 0, res.size() * 4);
      const double tk0 = now();
      for (size_t b0 = 0; b0 < blocks.size(); b0 += perDispatch) {
        const size_t nb = std::min<size_t>(perDispatch, blocks.size() - b0);
        uint32_t mx = 0;
        for (size_t b = b0; b < b0 + nb; b++) mx = std::max(mx, bi[b].nstates);
        if (!mx) continue;
        Params P = {(uint32_t)b0, (uint32_t)nb, (uint32_t)X, (uint32_t)nF0, (uint32_t)stride, (uint32_t)BN[B.freeO[1].size()][4], 0, (uint32_t)k1, listCap};
        *(uint32_t *)[bNList contents] = 0;
        id<MTLCommandBuffer> cb = [q commandBuffer];
        id<MTLComputeCommandEncoder> ce = [cb computeCommandEncoder];
        [ce setComputePipelineState:ps];
        [ce setBuffer:bSt offset:0 atIndex:0];
        [ce setBuffer:bDist offset:0 atIndex:1];
        [ce setBuffer:bAlt offset:0 atIndex:2];
        [ce setBuffer:bLen offset:0 atIndex:3];
        [ce setBuffer:bBi offset:0 atIndex:4];
        [ce setBuffer:bOut offset:0 atIndex:5];
        [ce setBytes:&P length:sizeof P atIndex:6];
        [ce setBuffer:bBn offset:0 atIndex:7];
        [ce setBuffer:bList offset:0 atIndex:8];
        [ce setBuffer:bNList offset:0 atIndex:9];
        [ce dispatchThreads:MTLSizeMake(mx, nb, 1) threadsPerThreadgroup:MTLSizeMake(tgs, 1, 1)];
        [ce endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
        if (cb.status != MTLCommandBufferStatusCompleted) { fprintf(stderr, "GPU command failed at block %ld: %s\n", g0 + (long)b0, [[cb.error description] UTF8String]); exit(1); }
        // phase 2 (--k1): the listed states, one SIMD group each
        const uint32_t nl = std::min(*(uint32_t *)[bNList contents], listCap);
        if (nl) {
          nListed += nl;
          cb = [q commandBuffer];
          ce = [cb computeCommandEncoder];
          [ce setComputePipelineState:ps2];
          [ce setBuffer:bSt offset:0 atIndex:0];
          [ce setBuffer:bDist offset:0 atIndex:1];
          [ce setBuffer:bAlt offset:0 atIndex:2];
          [ce setBuffer:bLen offset:0 atIndex:3];
          [ce setBuffer:bBi offset:0 atIndex:4];
          [ce setBuffer:bOut offset:0 atIndex:5];
          [ce setBytes:&P length:sizeof P atIndex:6];
          [ce setBuffer:bBn offset:0 atIndex:7];
          [ce setBuffer:bList offset:0 atIndex:8];
          [ce setBytes:&nl length:4 atIndex:9];
          [ce dispatchThreads:MTLSizeMake((size_t)nl * simdW, 1, 1) threadsPerThreadgroup:MTLSizeMake(tgs, 1, 1)];
          [ce endEncoding];
          [cb commit];
          [cb waitUntilCompleted];
          if (cb.status != MTLCommandBufferStatusCompleted) { fprintf(stderr, "GPU command (check2) failed at block %ld: %s\n", g0 + (long)b0, [[cb.error description] UTF8String]); exit(1); }
        }
      }
      tGpu += now() - tk0;
      memcpy(res.data(), [bOut contents], res.size() * 4);
    }
    for (size_t b = 0; b < blocks.size(); b++) {
      tried += res[3 * b] + ((uint64_t)res[3 * b + 1] << 32);
      if (res[3 * b + 2]) {
        left += res[3 * b + 2];
        if (++leftBlocks <= 5) printf("  block %ld: %u colourings not solved within %d\n", g0 + (long)b, res[3 * b + 2], X);
        if (leftOut) fprintf(leftOut, "%s\n", blocks[b][0].c_str());
      }
    }
    // --cpu-sample: the boundcheck loop on every Nth block (global index), its counts against the GPU's
    if (cpuSample > 0) {
      const double ts0 = now();
      std::vector<size_t> pick;
      for (size_t b = 0; b < blocks.size(); b++) if ((g0 + (long)b) % cpuSample == 0 && blockOk[b]) pick.push_back(b);
      nSampled += pick.size();
      std::atomic<size_t> ni{0};
      std::atomic<long> mism{0}, fb{0};
      std::vector<std::thread> th;
      for (int t = 0; t < threads; t++)
        th.emplace_back([&] {
          int8_t J0[48], F[48], col[48], s[48];
          for (size_t pi; (pi = ni++) < pick.size();) {
            const size_t b = pick[pi];
            const auto &D = Dall[b];
            const int8_t *u = U[b].data();
            uint64_t tr = 0, lf = 0;
            for (size_t si = 0; si < bi[b].nstates; si++) {
              for (int k = 0; k < 48; k++) s[k] = (B.S >> groupOf(k) & 1) ? groupOf(k) : -1;
              for (int gi = 0; gi < 2; gi++) for (int j = 0; j < 4; j++) s[B.freeO[B.tg[gi] / 6][states[si * 8 + gi * 4 + j]]] = B.tg[gi];
              for (int k = 0; k < 48; k++) J0[k] = (u[k] >= 0 && (A.T >> u[k] & 1)) ? u[k] : s[(int)D[0][k]];
              bool good = false;
              for (size_t i = 0; i < D.size() && !good; i++) {
                if (alen[bi[b].alt0 + i] >= X) continue;
                const auto &Di = D[perm[b][i]];
                for (int k = 0; k < 48; k++) F[(int)Di[k]] = J0[k];
                for (int k = 0; k < 48; k++) col[k] = (B.S >> groupOf(k) & 1) ? groupOf(k) : ((F[k] >= 0 && (B.T >> F[k] & 1)) ? F[k] : -1);
                const int dB = B.dist[B.rank(col)];
                tr++;
                if (perm[b][i] == 0 && dB != (int)B.dist[B.rank(s)]) fb++;
                if (alen[bi[b].alt0 + i] + dB <= X) good = true;
              }
              if (!good) lf++;
            }
            const uint64_t gtr = res[3 * b] + ((uint64_t)res[3 * b + 1] << 32);
            if (tr != gtr || lf != res[3 * b + 2]) {
              mism++;
              printf("  sample block %ld: CPU tried %lu left %lu, GPU tried %lu left %u\n", g0 + (long)b, (unsigned long)tr, (unsigned long)lf, (unsigned long)gtr, res[3 * b + 2]);
            }
          }
        });
      for (auto &x : th) x.join();
      sampleMismatch += mism;
      tSamp += now() - ts0;
      frameErr += fb;
    }
    if (now() - lastRep > 60) { fprintf(stderr, "  %ld/%ld blocks, %.0fs\n", g0 + (long)blocks.size(), nBlocks, now() - t0); lastRep = now(); }
  };
  {
    const size_t CH = 32768;
    std::vector<std::vector<std::string>> blocks;
    std::vector<std::string> cur;
    long gi = 0, g0 = 0;  // gi: blocks read (file order), g0: global index of blocks[0] after drops
    auto endBlock = [&] {
      if (cur.empty()) return;
      if (gi == dropBlock) printf("dropped block %ld\n", gi);
      else {
        if (gi == dropLineB && dropLineI > 0 && dropLineI < (long)cur.size()) { cur.erase(cur.begin() + dropLineI); printf("dropped line %ld of block %ld\n", dropLineI, gi); }
        blocks.push_back(cur);
      }
      gi++;
      cur.clear();
      if (blocks.size() == CH) { process(blocks, g0); g0 += blocks.size(); blocks.clear(); }
    };
    forChunks(pos[2], 1 << 16, [&](std::vector<std::string> &v) {
      for (auto &l : v) {
        if (l.empty()) endBlock();
        else cur.push_back(l);
      }
    });
    endBlock();
    if (!blocks.empty()) process(blocks, g0);
  }
  printf("check 2: %ld bad blocks; %ld lines; %lu (block, B state) pairs\n", badBlock, nLines, (unsigned long)pairs);
  printf("check 4 (GPU): %lu colourings tried, %lu not solved in %ld blocks; kernels %.1fs (%.2f G pairs/s)\n", (unsigned long)tried, (unsigned long)left, leftBlocks, tGpu, pairs / std::max(tGpu, 1e-9) / 1e9);
  if (k1) printf("--k1 %d: %lu states finished by check2 (SIMD group per state)\n", k1, (unsigned long)nListed);
  if (cpuSample > 0) printf("CPU sample: %ld blocks, %ld differ from the GPU (%.1fs on %d threads)\n", nSampled, sampleMismatch, tSamp, threads);
  printf("times: stage B %.1fs, check 2 + maps %.1fs, GPU check 4 %.1fs, --reorder %.1fs\n", tB, tC2, tGpu, tReo);
  fflush(stdout);

  // check 3: A's table, then every A state that needs a block has one
  std::vector<uint8_t>().swap(states);
  const double tA0 = now();
  if (!A.init(pos[0], threads)) { printf("FAIL: a stage certificate does not cover its stage\n"); return 1; }
  printf("times: stage A %.1fs\n", now() - tA0);
  long missing = 0, needed = 0;
  for (uint64_t r = 0; r < A.total; r++)
    if (A.dist[r] > X - B.W) {
      needed++;
      if (!(hasBlock[r >> 6] >> (r & 63) & 1)) missing++;
    }
  printf("blocks: %ld bad (%ld bad lines, %ld lines not solving the block's state, %ld with side effects); %lu colourings tried, %lu not solved; frame errors %ld\n",
         badBlock + leftBlocks, badLine, notSolved, sideBad, (unsigned long)tried, (unsigned long)left, frameErr);
  printf("A states with dA > %d: %ld, %ld without a block\n", X - B.W, needed, missing);
  const bool ok = !badBlock && !leftBlocks && !missing && !frameErr && !sampleMismatch;
  printf("%s: every position is solved through %s and %s in at most %d moves (%.0fs)\n", ok ? "PASS" : "FAIL", A.name.c_str(), B.name.c_str(), X, now() - t0);
  if (leftOut) fclose(leftOut);
  return ok ? 0 : 1;
}
