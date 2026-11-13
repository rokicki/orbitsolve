// gpuaxbcheck: axbcheck (devtools/axbcheck.cpp) with check 4 on the GPU (Metal).  Same arguments,
// same claim, same four checks (see axbcheck.cpp):
//   1. both stage certificates cover their stages (d3, d4 = line lengths over the 16 symmetries);
//   2. every block: legal moves, each line solves the first line's 3ax state, side effects as axcheck;
//   3. every 3ax state with d3 > X - max d4 has a block for a symmetric image of it;
//   4. per block, with L its first line: every 4op state s with |L| + d4(s) > X is solved by some
//      line P of the block with |P| + d4(state P reaches) <= X.
// Checks 1-3 on the CPU as axbcheck (stage lines and blocks streamed).  Check 4 on the GPU: a 4op
// state is, per orbit and axis (F/B, L/R), which 4 of the axis's 8 facelets hold F (resp. L) pieces:
// four 8-bit masks.  L solves the 3ax state, so a piece at state facelet p sits at D_L^-1(p) and P
// carries it to M(p), M = D_P o D_L^-1, which keeps each (orbit, axis) set of 8 (checked here); one
// GPU thread per (block, bad 4op state) moves the masks through each line's M, ranks them (a table
// of 4-subsets of 8) and reads d4.
//
//   gpuaxbcheck STAGEDIR_3AX STAGEDIR_4OP BOUNDARYFILE X [--threads T (2)] [--blocks K]
//               [--cpu-sample N] [--drop-line B:I] [--drop-block B]
// --cpu-sample N: every Nth block also the axbcheck way on the CPU (counts compared);
// --drop-line / --drop-block: break the certificate on purpose (the run must FAIL).
#import <Metal/Metal.h>
#include "certcube.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <dirent.h>
#include <functional>
#include <mutex>
#include <thread>
#include <unistd.h>

static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static int mp[6][4][4][48];  // dst form: the piece at k goes to mp[f][d][a][k]
static inline int faceK(int k) { return k / 8; }
static inline int orbK(int k) { return (k % 8) / 4; }
static inline int grp(int k) { return orbK(k) * 6 + faceK(k); }
static const int AX[6] = {0, 1, 2, 1, 2, 0};  // U L F R B D
static uint64_t C_[17][17];
static std::vector<int> sideO[2], axO[2][3];
static int axIdx[48];  // index of k in axO[orb][axis]

// as axbcheck
static int64_t rank3(const int8_t *c) {
  int64_t r = 0;
  for (int k = 0; k < 48; k++) if (AX[faceK(k)] == 0 && c[k] != grp(k)) return -1;
  for (int o = 0; o < 2; o++) {
    int64_t ro = 0;
    int j = 0;
    for (int i = 0; i < 16; i++) {
      const int g = c[sideO[o][i]];
      if (g / 6 != o || AX[g % 6] == 0) return -1;
      if (AX[g % 6] == 2) ro += C_[i][++j];
    }
    if (j != 8) return -1;
    r = r * 12870 + ro;
  }
  return r;
}
static int64_t rank4(const int8_t *c) {
  int64_t r = 0;
  for (int k = 0; k < 48; k++) {
    if (AX[faceK(k)] == 0) { if (c[k] != grp(k)) return -1; continue; }
    if (c[k] / 6 != orbK(k) || AX[c[k] % 6] != AX[faceK(k)]) return -1;
  }
  for (int o = 0; o < 2; o++)
    for (int ax : {2, 1}) {
      int64_t ro = 0;
      int j = 0;
      for (int i = 0; i < 8; i++) if (c[axO[o][ax][i]] % 6 == ax) ro += C_[i][++j];
      if (j != 4) return -1;
      r = r * 70 + ro;
    }
  return r;
}
static void unrank4(int64_t r, int8_t *c) {
  int v[4];
  for (int i = 3; i >= 0; i--) v[i] = r % 70, r /= 70;
  for (int k = 0; k < 48; k++) c[k] = grp(k);
  int q = 0;
  for (int o = 0; o < 2; o++)
    for (int ax : {2, 1}) {
      int64_t rest = v[q++];
      bool take[8] = {};
      int p = 8;
      for (int t = 4; t >= 1; t--) {
        p--;
        while (C_[p][t] > (uint64_t)rest) p--;
        rest -= C_[p][t];
        take[p] = true;
      }
      for (int i = 0; i < 8; i++) c[axO[o][ax][i]] = o * 6 + (take[i] ? ax : ax + 2);
    }
}
static void play(const std::vector<cc::Mv> &mv, const int8_t *a, int8_t *out) {
  int8_t x[48], y[48];
  memcpy(x, a, 48);
  for (auto &m : mv) {
    for (int k = 0; k < 48; k++) y[mp[m.face][m.depth][m.amt][k]] = x[k];
    memcpy(x, y, 48);
  }
  memcpy(out, x, 48);
}
static void unplay(const std::vector<cc::Mv> &mv, const int8_t *a, int8_t *out) {
  int8_t x[48], y[48];
  memcpy(x, a, 48);
  for (auto it = mv.rbegin(); it != mv.rend(); ++it) {
    for (int k = 0; k < 48; k++) y[mp[it->face][it->depth][4 - it->amt][k]] = x[k];
    memcpy(x, y, 48);
  }
  memcpy(out, x, 48);
}
struct Sym { int fac[48], gmap[12]; };
static std::vector<Sym> syms;
static void image(const Sym &y, const int8_t *a, int8_t *b) { for (int k = 0; k < 48; k++) b[y.fac[k]] = y.gmap[a[k]]; }

// a .zst file is read through zstd -dc (2026-11-05: AX 38's merged blocks would be ~25 GB uncompressed)
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
// d over every state, lines streamed; returns false on a bad line; counts lines not a stage start
static bool fillDir(const std::string &dir, bool four, std::vector<uint8_t> &d, int threads, long &nLines, long &bad) {
  std::vector<std::pair<int, std::string>> fs;
  if (DIR *dp = opendir(dir.c_str())) {
    while (dirent *e = readdir(dp)) {
      std::string n = e->d_name;
      if (n.size() > 5 && n[0] == 'l' && n.compare(n.size() - 4, 4, ".txt") == 0) fs.push_back({atoi(n.c_str() + 1), dir + "/" + n});
    }
    closedir(dp);
  }
  if (fs.empty()) return false;
  std::sort(fs.begin(), fs.end());
  std::atomic<long> nb{0}, parseBad{0};
  for (auto &[nn, f] : fs) {
    const int maxLen = nn;
    const bool rd = forChunks(f, 1 << 19, [&](std::vector<std::string> &raw) {
      nLines += raw.size();
      std::atomic<size_t> ni{0};
      std::vector<std::thread> th;
      for (int t = 0; t < threads; t++)
        th.emplace_back([&] {
          int8_t g[48], c[48], b[48];
          for (int k = 0; k < 48; k++) g[k] = grp(k);
          std::vector<cc::Mv> mv;
          for (size_t i; (i = ni++) < raw.size();) {
            bool ok = cc::parseMoves(raw[i], mv);
            for (auto &m : mv) ok &= m.depth >= 1 && m.depth <= 3 && m.amt >= 1 && m.amt <= 3;
            if (!ok || (int)mv.size() > maxLen) { parseBad++; continue; }
            unplay(mv, g, c);
            if ((four ? rank4(c) : rank3(c)) < 0) { nb++; continue; }
            for (auto &y : syms) {
              image(y, c, b);
              const int64_t r = four ? rank4(b) : rank3(b);
              if (r < 0) { nb++; continue; }
              uint8_t cur = __atomic_load_n(&d[r], __ATOMIC_RELAXED);
              while (mv.size() < cur && !__atomic_compare_exchange_n(&d[r], &cur, (uint8_t)mv.size(), true, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {}
            }
          }
        });
      for (auto &x : th) x.join();
    });
    if (!rd) return false;
  }
  bad = nb;
  return parseBad == 0;
}

static const char *kSrc = R"(
#include <metal_stdlib>
using namespace metal;
struct Params { uint blk0, X; };
struct Blk { uint alt0, nalt, nstates, pad; };
// a state: 4 masks (orbit 0 F/B, orbit 0 L/R, orbit 1 F/B, orbit 1 L/R); a line: 4 x 8 targets
kernel void check(device const uchar4 *st [[buffer(0)]], device const uchar *d4 [[buffer(1)]],
                  device const uchar *alts [[buffer(2)]], device const uchar *alen [[buffer(3)]],
                  device const Blk *blk [[buffer(4)]], device atomic_uint *out [[buffer(5)]],
                  constant Params &P [[buffer(6)]], constant uchar *r70 [[buffer(7)]],
                  uint2 gid [[thread_position_in_grid]]) {
  const uint b = P.blk0 + gid.y;
  const Blk B = blk[b];
  const bool live = gid.x < B.nstates;
  uint tr = 0, bad = 0;
  if (live) {
    const uchar4 s = st[gid.x];
    bool good = false;
    for (uint i = 0; i < B.nalt && !good; i++) {
      const uint len = alen[B.alt0 + i];
      if (len >= P.X) continue;
      device const uchar *m = alts + (B.alt0 + i) * 32;
      uint r = 0;
      for (uint g = 0; g < 4; g++) {
        uint x = s[g], y = 0;
        while (x) { y |= 1u << m[g * 8 + ctz(x)]; x &= x - 1; }
        r = r * 70 + r70[y];
      }
      tr++;
      if (len + d4[r] <= P.X) good = true;
    }
    bad = good ? 0 : 1;
  }
  const uint str = simd_sum(tr), sbad = simd_sum(bad);
  if (simd_is_first()) {
    // tries: 64 bits as low word + carries
    if (str) { const uint old = atomic_fetch_add_explicit(&out[3 * b], str, memory_order_relaxed); if (old > 0xFFFFFFFFu - str) atomic_fetch_add_explicit(&out[3 * b + 1], 1u, memory_order_relaxed); }
    if (sbad) atomic_fetch_add_explicit(&out[3 * b + 2], sbad, memory_order_relaxed);
  }
}
)";

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  int threads = 2, perDispatch = 16, cpuSample = 0;
  long dropBlock = -1, dropLineB = -1, dropLineI = -1;
  std::vector<std::string> pos;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--threads" && i + 1 < argc) threads = atoi(argv[++i]);
    else if (o == "--blocks" && i + 1 < argc) perDispatch = atoi(argv[++i]);
    else if (o == "--cpu-sample" && i + 1 < argc) cpuSample = atoi(argv[++i]);
    else if (o == "--drop-block" && i + 1 < argc) dropBlock = atol(argv[++i]);
    else if (o == "--drop-line" && i + 1 < argc) {
      if (sscanf(argv[++i], "%ld:%ld", &dropLineB, &dropLineI) != 2) { fprintf(stderr, "--drop-line takes B:I\n"); return 1; }
    }
    else if (o.size() > 1 && o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else pos.push_back(o);
  }
  if (pos.size() != 4) { fprintf(stderr, "usage: gpuaxbcheck STAGEDIR_3AX STAGEDIR_4OP BOUNDARYFILE X [--threads T] [--blocks K] [--cpu-sample N] [--drop-line B:I] [--drop-block B]\n"); return 1; }
  const int X = atoi(pos[3].c_str());
  const double t0 = now();
  for (int n = 0; n <= 16; n++) { C_[n][0] = 1; for (int k = 1; k <= 16; k++) C_[n][k] = n == 0 ? 0 : C_[n - 1][k - 1] + C_[n - 1][k]; }
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
  for (int k = 0; k < 48; k++) axIdx[k] = -1;
  for (int k = 0; k < 48; k++) if (AX[faceK(k)]) { sideO[orbK(k)].push_back(k); axIdx[k] = axO[orbK(k)][AX[faceK(k)]].size(); axO[orbK(k)][AX[faceK(k)]].push_back(k); }
  {
    const int pm[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    for (int pi = 0; pi < 6; pi++)
      for (int sg = 0; sg < 8; sg++) {
        Sym y;
        for (int k = 0; k < 48; k++) {
          const auto &p = C8.pos[pf[k]];
          std::array<int, 3> q;
          for (int r = 0; r < 3; r++) q[r] = p[pm[pi][r]] * ((sg >> r & 1) ? -1 : 1);
          y.fac[k] = -1;
          for (int x = 0; x < 48; x++) if (C8.pos[pf[x]] == q) y.fac[k] = x;
          if (y.fac[k] < 0) { fprintf(stderr, "a symmetry leaves the pair\n"); return 1; }
        }
        bool keeps = true;
        for (int g = 0; g < 12; g++) {
          int img = -1;
          for (int k = 0; k < 48; k++) if (grp(k) == g) { const int h = grp(y.fac[k]); if (img >= 0 && img != h) { fprintf(stderr, "a symmetry splits a group\n"); return 1; } img = h; }
          y.gmap[g] = img;
          if ((AX[g % 6] == 0) != (AX[img % 6] == 0)) keeps = false;
        }
        if (keeps) syms.push_back(y);
      }
    if (syms.size() != 16) { fprintf(stderr, "expected 16 stage symmetries\n"); return 1; }
  }
  // the group order of a 4op state (rank4: orbit 0 axis 2, orbit 0 axis 1, orbit 1 axis 2, orbit 1 axis 1)
  const int GO[4] = {0, 0, 1, 1}, GA[4] = {2, 1, 2, 1};
  uint8_t r70[256] = {0};
  for (int m = 0; m < 256; m++) {
    if (__builtin_popcount(m) != 4) continue;
    int j = 0, ro = 0;
    for (int i = 0; i < 8; i++) if (m >> i & 1) ro += C_[i][++j];
    r70[m] = ro;
  }

  // 1. the 4op stage (the 3ax table is built at the end, for check 3)
  std::vector<uint8_t> d4(70ull * 70 * 70 * 70, 0xFF);
  long n4 = 0, b4 = 0;
  if (!fillDir(pos[1], true, d4, threads, n4, b4)) { printf("FAIL: a stage directory has a bad line or none\n"); return 1; }
  int W4 = 0;
  long u4 = 0;
  for (auto v : d4) { if (v == 0xFF) u4++; else W4 = std::max<int>(W4, v); }
  printf("4op: %ld lines (%ld not a stage start), %zu states, %ld not covered, longest %d (%.0fs)\n", n4, b4, d4.size(), u4, W4, now() - t0);
  fflush(stdout);
  if (b4 || u4) { printf("FAIL: a stage certificate does not cover its stage\n"); return 1; }

  // pass 1 over the boundary file: blocks and the longest first line
  long nBlocks = 0;
  int maxL = 0;
  {
    bool inBlock = false;
    if (!forChunks(pos[2], 1 << 16, [&](std::vector<std::string> &v) {
          for (auto &l : v) {
            if (l.empty()) { inBlock = false; continue; }
            if (!inBlock) {
              inBlock = true, nBlocks++;
              std::vector<cc::Mv> mv;
              if (cc::parseMoves(l, mv)) maxL = std::max<int>(maxL, mv.size());
            }
          }
        })) { fprintf(stderr, "cannot read the boundary file\n"); return 1; }
  }
  // 4op states by length beyond the lowest threshold, longest first: as masks, and their ranks (CPU sample)
  // (axbcheck takes maxL = max(W3, first lines); first lines suffice: a block uses d > X - |L|)
  const int lowT = std::max(0, X - maxL);
  std::vector<std::vector<int32_t>> byD(W4 + 1);
  for (int64_t r = 0; r < (int64_t)d4.size(); r++) if (d4[r] > lowT) byD[d4[r]].push_back(r);
  std::vector<uint8_t> states;
  std::vector<int32_t> stRank;
  std::vector<size_t> cntAt(W4 + 2, 0);
  for (int d = W4; d >= 0; d--) {
    cntAt[d] = cntAt[d + 1] + byD[d].size();
    for (int32_t r : byD[d]) {
      int8_t c[48];
      unrank4(r, c);
      for (int g = 0; g < 4; g++) {
        uint8_t m = 0;
        for (int i = 0; i < 8; i++) if (c[axO[GO[g]][GA[g]][i]] % 6 == GA[g]) m |= 1 << i;
        states.push_back(m);
      }
      stRank.push_back(r);
    }
    std::vector<int32_t>().swap(byD[d]);
  }
  printf("boundary: %ld blocks, X = %d, longest 4op line %d\n", nBlocks - (dropBlock >= 0 && dropBlock < nBlocks), X, W4);
  for (int d = W4; d > lowT; d--) if (cntAt[d] - cntAt[d + 1]) printf("  4op states at %d: %zu\n", d, cntAt[d] - cntAt[d + 1]);
  fflush(stdout);

  // side effects on N = 6..10, as axcheck
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
          bool pair = false;
          for (int o = 0; o < 2 && !pair; o++) {
            int x = o ? 2 : 1, yy = o ? 1 : 2;
            for (int q = 0; q < 4; q++) { if (r == x && col == yy) pair = true; const int nx = yy, ny = M - x; x = nx, yy = ny; }
          }
          cid[CN.index(f, r, col)] = c.kind.size();
          c.kind.push_back(pair ? 1 : (r == col || r + col == M) ? 2 : 3);
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
            if (cid[j] < 0) { fprintf(stderr, "a move takes a centre off the centres\n"); return 1; }
            if (j != (int)i) c.mv[(f * 4 + d) * 4 + a].push_back({cid[i], cid[j]});
          }
    ncs.push_back(std::move(c));
  }
  auto sideOk = [&](const std::vector<cc::Mv> &mv) {
    std::vector<int> a, tmp;
    for (auto &c : ncs) {
      a.resize(c.n);
      for (int i = 0; i < c.n; i++) a[i] = i;
      for (auto &m : mv) {
        const auto &pr = c.mv[(m.face * 4 + m.depth) * 4 + m.amt];
        tmp.resize(pr.size());
        for (size_t q = 0; q < pr.size(); q++) tmp[q] = a[pr[q].first];
        for (size_t q = 0; q < pr.size(); q++) a[pr[q].second] = tmp[q];
      }
      for (int i = 0; i < c.n; i++) if (c.kind[i] == 3 && c.face[a[i]] != c.face[i]) return false;
    }
    return true;
  };

  // the GPU
  id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
  if (!dev) { fprintf(stderr, "no Metal device\n"); return 1; }
  NSError *err = nil;
  id<MTLLibrary> lib = [dev newLibraryWithSource:[NSString stringWithUTF8String:kSrc] options:nil error:&err];
  if (!lib) { fprintf(stderr, "Metal compile: %s\n", [[err description] UTF8String]); return 1; }
  id<MTLComputePipelineState> ps = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"check"] error:&err];
  if (!ps) { fprintf(stderr, "pipeline: %s\n", [[err description] UTF8String]); return 1; }
  id<MTLCommandQueue> q = [dev newCommandQueue];
  // at least 16 bytes; an empty input gets a zeroed buffer (2026-11-09: gpubound read 16 bytes from a null
  // pointer for an empty vector)
  auto buf = [&](const void *p, size_t n) {
    return n ? [dev newBufferWithBytes:p length:std::max<size_t>(n, 16) options:MTLResourceStorageModeShared]
             : [dev newBufferWithLength:16 options:MTLResourceStorageModeShared];
  };
  id<MTLBuffer> bSt = buf(states.data(), states.size()), bD4 = buf(d4.data(), d4.size()), bR70 = buf(r70, 256);
  struct Params { uint32_t blk0, X; };
  struct BI { uint32_t alt0, nalt, nstates, pad; };
  const NSUInteger tgs = std::min<NSUInteger>(ps.maxTotalThreadsPerThreadgroup, 256);

  std::vector<uint64_t> has((12870ull * 12870 + 63) / 64, 0);
  long badBlock = 0, badLine = 0, notSolved = 0, sideBad = 0, frameErr = 0, leftBlocks = 0, mismatch = 0, nSampled = 0, nLines = 0;
  uint64_t pairs = 0, tried = 0, left = 0;
  double tGpu = 0, tC2 = 0, tSamp = 0, lastRep = now();
  auto process = [&](std::vector<std::vector<std::string>> &blocks, long g0) {
    const double tp0 = now();
    const size_t NB = blocks.size();
    std::vector<BI> bi(NB);
    std::vector<uint8_t> blockOk(NB, 0);
    std::vector<std::vector<std::array<int8_t, 48>>> Dm(NB);
    std::vector<std::vector<uint8_t>> lens(NB);
    {
      std::vector<int> ok(NB, 0), bl(NB, 0), ns(NB, 0), sb(NB, 0);
      std::atomic<size_t> ni{0};
      std::vector<std::thread> th;
      for (int t = 0; t < threads; t++)
        th.emplace_back([&] {
          int8_t g[48], u[48], img[48], J[48];
          for (int k = 0; k < 48; k++) g[k] = grp(k);
          for (size_t b; (b = ni++) < NB;) {
            const auto &blk = blocks[b];
            std::vector<std::vector<cc::Mv>> mvs(blk.size());
            bool good = true;
            for (size_t i = 0; i < blk.size() && good; i++) {
              good = cc::parseMoves(blk[i], mvs[i]) && !mvs[i].empty();
              for (auto &m : mvs[i]) good &= m.depth >= 1 && m.depth <= 3 && m.amt >= 1 && m.amt <= 3;
              if (!good) bl[b]++;
            }
            if (!good) continue;
            unplay(mvs[0], g, u);
            if (rank3(u) < 0) { ns[b]++; continue; }
            for (size_t i = 0; i < blk.size() && good; i++) {
              play(mvs[i], u, J);
              for (int k = 0; k < 48 && good; k++) {
                if (AX[faceK(k)] == 0) good = J[k] == grp(k);
                else good = J[k] / 6 == orbK(k) && AX[J[k] % 6] == AX[faceK(k)];
              }
              if (!good) { ns[b]++; break; }
              if (!sideOk(mvs[i])) { sb[b]++; good = false; }
            }
            if (!good) continue;
            for (auto &y : syms) {
              image(y, u, img);
              const int64_t r = rank3(img);
              __atomic_fetch_or(&has[r >> 6], 1ull << (r & 63), __ATOMIC_RELAXED);
            }
            Dm[b].resize(blk.size());
            for (size_t i = 0; i < blk.size(); i++) {
              int8_t idx[48], m[48];
              for (int k = 0; k < 48; k++) idx[k] = k;
              play(mvs[i], idx, m);
              for (int x = 0; x < 48; x++) Dm[b][i][(int)m[x]] = x;
              lens[b].push_back(std::min<size_t>(mvs[i].size(), 255));
            }
            ok[b] = 1;
          }
        });
      for (auto &x : th) x.join();
      for (size_t b = 0; b < NB; b++) {
        badLine += bl[b], notSolved += ns[b], sideBad += sb[b];
        if (!ok[b]) badBlock++;
        else blockOk[b] = 1;
      }
    }
    // M = D_P o D_L^-1 on each (orbit, axis) set of 8
    std::vector<uint8_t> alts, alen;
    for (size_t b = 0; b < NB; b++) {
      bi[b] = {(uint32_t)alen.size(), 0, 0, 0};
      nLines += blocks[b].size();
      if (!blockOk[b]) continue;
      int inv0[48];
      for (int k = 0; k < 48; k++) inv0[(int)Dm[b][0][k]] = k;
      for (size_t i = 0; i < Dm[b].size(); i++) {
        uint8_t row[32];
        for (int g = 0; g < 4; g++)
          for (int j = 0; j < 8; j++) {
            const int to = Dm[b][i][inv0[axO[GO[g]][GA[g]][j]]];
            if (orbK(to) != GO[g] || AX[faceK(to)] != GA[g]) frameErr++;  // a line leaves a side piece off its axis
            row[g * 8 + j] = axIdx[to] < 0 ? 0 : axIdx[to];
          }
        alts.insert(alts.end(), row, row + 32);
        alen.push_back(lens[b][i]);
      }
      bi[b].nalt = Dm[b].size();
      const int dmin = std::max(X - (int)lens[b][0] + 1, lowT + 1);
      bi[b].nstates = cntAt[std::min(dmin, W4 + 1)];
      pairs += bi[b].nstates;
    }
    tC2 += now() - tp0;
    std::vector<uint32_t> res(3 * NB, 0);
    @autoreleasepool {
      id<MTLBuffer> bAlt = buf(alts.data(), alts.size()), bLen = buf(alen.data(), alen.size()), bBi = buf(bi.data(), bi.size() * sizeof(BI));
      id<MTLBuffer> bOut = [dev newBufferWithLength:std::max<size_t>(res.size() * 4, 16) options:MTLResourceStorageModeShared];
      memset([bOut contents], 0, res.size() * 4);
      const double tk0 = now();
      for (size_t b0 = 0; b0 < NB; b0 += perDispatch) {
        const size_t nb = std::min<size_t>(perDispatch, NB - b0);
        uint32_t mx = 0;
        for (size_t b = b0; b < b0 + nb; b++) mx = std::max(mx, bi[b].nstates);
        if (!mx) continue;
        Params P = {(uint32_t)b0, (uint32_t)X};
        id<MTLCommandBuffer> cb = [q commandBuffer];
        id<MTLComputeCommandEncoder> ce = [cb computeCommandEncoder];
        [ce setComputePipelineState:ps];
        [ce setBuffer:bSt offset:0 atIndex:0];
        [ce setBuffer:bD4 offset:0 atIndex:1];
        [ce setBuffer:bAlt offset:0 atIndex:2];
        [ce setBuffer:bLen offset:0 atIndex:3];
        [ce setBuffer:bBi offset:0 atIndex:4];
        [ce setBuffer:bOut offset:0 atIndex:5];
        [ce setBytes:&P length:sizeof P atIndex:6];
        [ce setBuffer:bR70 offset:0 atIndex:7];
        [ce dispatchThreads:MTLSizeMake(mx, nb, 1) threadsPerThreadgroup:MTLSizeMake(tgs, 1, 1)];
        [ce endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
        if (cb.status != MTLCommandBufferStatusCompleted) { fprintf(stderr, "GPU command failed at block %ld: %s\n", g0 + (long)b0, [[cb.error description] UTF8String]); exit(1); }
      }
      tGpu += now() - tk0;
      memcpy(res.data(), [bOut contents], res.size() * 4);
    }
    for (size_t b = 0; b < NB; b++) {
      tried += res[3 * b] + ((uint64_t)res[3 * b + 1] << 32);
      if (res[3 * b + 2]) {
        left += res[3 * b + 2];
        if (++leftBlocks <= 5) printf("  block %ld: %u positions not solved within %d\n", g0 + (long)b, res[3 * b + 2], X);
      }
    }
    // --cpu-sample: axbcheck's loop (whole colourings, rank4) on every Nth block
    if (cpuSample > 0) {
      const double ts0 = now();
      std::vector<size_t> pick;
      for (size_t b = 0; b < NB; b++) if ((g0 + (long)b) % cpuSample == 0 && blockOk[b]) pick.push_back(b);
      nSampled += pick.size();
      std::atomic<size_t> ni{0};
      std::atomic<long> mism{0}, fb{0};
      std::vector<std::thread> th;
      for (int t = 0; t < threads; t++)
        th.emplace_back([&] {
          int8_t s[48], J0[48], J[48];
          for (size_t pi; (pi = ni++) < pick.size();) {
            const size_t b = pick[pi];
            uint64_t tr = 0, lf = 0;
            for (size_t si = 0; si < bi[b].nstates; si++) {
              const int64_t r = stRank[si];
              unrank4(r, s);
              for (int k = 0; k < 48; k++) J0[k] = s[(int)Dm[b][0][k]];
              bool good = false;
              for (size_t i = 0; i < Dm[b].size() && !good; i++) {
                if (lens[b][i] >= X) continue;
                for (int k = 0; k < 48; k++) J[(int)Dm[b][i][k]] = J0[k];
                const int64_t r4 = rank4(J);
                tr++;
                if (r4 < 0) { fb++; continue; }
                if (i == 0 && r4 != r) fb++;
                if (lens[b][i] + d4[r4] <= X) good = true;
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
      mismatch += mism;
      frameErr += fb;
      tSamp += now() - ts0;
    }
    if (now() - lastRep > 60) { fprintf(stderr, "  %ld/%ld blocks, %.0fs\n", g0 + (long)NB, nBlocks, now() - t0); lastRep = now(); }
  };
  {
    const size_t CH = 32768;
    std::vector<std::vector<std::string>> blocks;
    std::vector<std::string> cur;
    long gi = 0, g0 = 0;
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
  printf("check 2: %ld bad blocks; %ld lines; %lu (block, 4op state) pairs\n", badBlock, nLines, (unsigned long)pairs);
  printf("check 4 (GPU): %lu positions tried, %lu not solved in %ld blocks; kernels %.1fs (%.2f G pairs/s)\n", (unsigned long)tried, (unsigned long)left, leftBlocks, tGpu, pairs / std::max(tGpu, 1e-9) / 1e9);
  if (cpuSample > 0) printf("CPU sample: %ld blocks, %ld differ from the GPU (%.1fs on %d threads)\n", nSampled, mismatch, tSamp, threads);
  fflush(stdout);

  // check 3: the 3ax table
  std::vector<uint8_t>().swap(states);
  std::vector<int32_t>().swap(stRank);
  const double tA = now();
  std::vector<uint8_t> d3(12870ull * 12870, 0xFF);
  long n3 = 0, b3 = 0;
  if (!fillDir(pos[0], false, d3, threads, n3, b3)) { printf("FAIL: a stage directory has a bad line or none\n"); return 1; }
  int W3 = 0;
  long u3 = 0;
  for (auto v : d3) { if (v == 0xFF) u3++; else W3 = std::max<int>(W3, v); }
  printf("3ax: %ld lines (%ld not a stage start), %zu states, %ld not covered, longest %d (%.0fs)\n", n3, b3, d3.size(), u3, W3, now() - tA);
  if (b3 || u3) { printf("FAIL: a stage certificate does not cover its stage\n"); return 1; }
  long needed = 0, missing = 0;
  for (size_t r = 0; r < d3.size(); r++)
    if (d3[r] > X - W4) {
      needed++;
      if (!(has[r >> 6] >> (r & 63) & 1)) missing++;
    }
  printf("times: check 2 + maps %.0fs, GPU check 4 %.1fs, CPU sample %.0fs\n", tC2, tGpu, tSamp);
  printf("blocks: %ld bad (%ld bad lines, %ld lines not solving the block's state, %ld with side effects); %lu positions tried, %lu not solved; frame errors %ld\n",
         badBlock + leftBlocks, badLine, notSolved, sideBad, (unsigned long)tried, (unsigned long)left, frameErr);
  printf("3ax states with d3 > %d: %ld, %ld without a block\n", X - W4, needed, missing);
  const bool ok = !badBlock && !leftBlocks && !missing && !frameErr && !mismatch;
  printf("%s: every 3ax start is solved through 3ax and 4op in at most %d moves (%.0fs)\n", ok ? "PASS" : "FAIL", X, now() - t0);
  return ok ? 0 : 1;
}
