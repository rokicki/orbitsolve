// GPU beam, stage 1 (Metal; macOS only).  The same search as TableBeam::solve
// (seam-aware costs, keep-solved, finish table, direct-mapped table with the
// seeded tie-break) except that each level's scoring runs on the GPU:
//   pass 1: every (node, algorithm) pair: keep-solved test, bit-sliced correct
//           count, score = tab[c] - g4 - e4 + seam bonus, into a histogram;
//   host:   the cut-off score keeping at least candMult * width candidates;
//   pass 2: the same pairs again, emitting (node, alg, score) at or above it.
// The host then admits those candidates node by node in (node, algorithm)
// order, as the CPU beam does.  Differences from the CPU beam come only from
// candidates below the cut-off that the CPU would have admitted (counted in
// `shortLevels`: levels whose table did not fill).
// Not supported (off in the comparison): scoreTab2, faceExtra, heur,
// finishLBMis, finishSkip, the endgame solver, seeds, rootTop.
#pragma once
#import <Metal/Metal.h>
#include "solvers.h"
#include <algorithm>
#include <chrono>
#include <map>

static const char *kGpuBeamSrc = R"(
#include <metal_stdlib>
using namespace metal;
struct Rec { ulong t0, t1, t2, x; int e4; ushort head; ushort pad0; int pad1, pad2; };
struct NodeD { ulong c0, c1, c2, corr; int g4; uint row; int pad0, pad1; };
struct Params { uint algs, nodes, nodesPerThread, heads, cutoff, maxOut, offset, bins; };
constant ulong M48 = (1ul << 48) - 1;

inline int scoreOf(Rec R, NodeD N, device const int *tab, device const int *bonus, uint heads) {
  const int c = popcount(~((N.c0 ^ R.t0) | (N.c1 ^ R.t1) | (N.c2 ^ R.t2)) & M48);
  return tab[c] - N.g4 - R.e4 + bonus[N.row * heads + R.head];
}

kernel void hist(device const Rec *rec [[buffer(0)]], device const NodeD *nodes [[buffer(1)]],
                 device const int *tab [[buffer(2)]], device const int *bonus [[buffer(3)]],
                 device atomic_uint *hist [[buffer(4)]], constant Params &P [[buffer(5)]],
                 uint2 gid [[thread_position_in_grid]], uint2 lid2 [[thread_position_in_threadgroup]],
                 uint2 tgs2 [[threads_per_threadgroup]]) {
  const uint lid = lid2.x, tgs = tgs2.x;  // threadgroups are 1-D along the algorithms
  threadgroup atomic_uint local[4096];
  for (uint i = lid; i < P.bins; i += tgs) atomic_store_explicit(&local[i], 0, memory_order_relaxed);
  threadgroup_barrier(mem_flags::mem_threadgroup);
  const uint a = gid.x;
  if (a < P.algs) {
    const Rec R = rec[a];
    const uint n0 = gid.y * P.nodesPerThread, n1 = min(n0 + P.nodesPerThread, P.nodes);
    for (uint n = n0; n < n1; n++) {
      const NodeD N = nodes[n];
      if (R.x & N.corr) continue;
      const int b = clamp(scoreOf(R, N, tab, bonus, P.heads) + (int)P.offset, 0, (int)P.bins - 1);
      atomic_fetch_add_explicit(&local[b], 1, memory_order_relaxed);
    }
  }
  threadgroup_barrier(mem_flags::mem_threadgroup);
  for (uint i = lid; i < P.bins; i += tgs) {
    const uint v = atomic_load_explicit(&local[i], memory_order_relaxed);
    if (v) atomic_fetch_add_explicit(&hist[i], v, memory_order_relaxed);
  }
}

struct Cand { uint node, alg; int score, pad; };
kernel void emit(device const Rec *rec [[buffer(0)]], device const NodeD *nodes [[buffer(1)]],
                 device const int *tab [[buffer(2)]], device const int *bonus [[buffer(3)]],
                 device atomic_uint *count [[buffer(4)]], constant Params &P [[buffer(5)]],
                 device Cand *out [[buffer(6)]], uint2 gid [[thread_position_in_grid]]) {
  const uint a = gid.x;
  if (a >= P.algs) return;
  const Rec R = rec[a];
  const int cut = (int)P.cutoff - (int)P.offset;
  const uint n0 = gid.y * P.nodesPerThread, n1 = min(n0 + P.nodesPerThread, P.nodes);
  for (uint n = n0; n < n1; n++) {
    const NodeD N = nodes[n];
    if (R.x & N.corr) continue;
    const int sc = scoreOf(R, N, tab, bonus, P.heads);
    if (sc >= cut) {
      const uint i = atomic_fetch_add_explicit(count, 1, memory_order_relaxed);
      if (i < P.maxOut) out[i] = Cand{n, a, sc, 0};
    }
  }
}

// best grandchild: for each child, max over algorithms of its score (keep-solved);
// threadgroup max, one atomic per threadgroup and child
kernel void bestnext(device const Rec *rec [[buffer(0)]], device const NodeD *nodes [[buffer(1)]],
                     device const int *tab [[buffer(2)]], device const int *bonus [[buffer(3)]],
                     device atomic_int *best [[buffer(4)]], constant Params &P [[buffer(5)]],
                     uint2 gid [[thread_position_in_grid]], uint lane [[thread_index_in_simdgroup]],
                     uint sg [[simdgroup_index_in_threadgroup]], uint nsg [[simdgroups_per_threadgroup]]) {
  threadgroup int red[32];
  const uint a = gid.x;
  const bool valid = a < P.algs;
  const Rec R = rec[valid ? a : 0];
  const uint n0 = gid.y * P.nodesPerThread, n1 = min(n0 + P.nodesPerThread, P.nodes);
  for (uint n = n0; n < n1; n++) {
    const NodeD N = nodes[n];
    int v = INT_MIN;
    if (valid && !(R.x & N.corr)) v = scoreOf(R, N, tab, bonus, P.heads);
    v = simd_max(v);
    if (lane == 0) red[sg] = v;
    threadgroup_barrier(mem_flags::mem_threadgroup);
    if (sg == 0) {
      int w = lane < nsg ? red[lane] : INT_MIN;
      w = simd_max(w);
      if (lane == 0 && w > INT_MIN) atomic_fetch_max_explicit(&best[n], w, memory_order_relaxed);
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
  }
}
)";

struct GpuBeam {
  struct Rec { uint64_t t0, t1, t2, x; int32_t e4; uint16_t head, pad0; int32_t pad1, pad2; };
  struct NodeD { uint64_t c0, c1, c2, corr; int32_t g4; uint32_t row; int32_t pad0, pad1; };
  struct Params { uint32_t algs, nodes, nodesPerThread, heads, cutoff, maxOut, offset, bins; };
  struct Cand { uint32_t node, alg; int32_t score, pad; };
  static constexpr uint32_t BINS = 4096, OFFSET = 2048;

  const TableBeam *tb = nullptr;
  int candMult = 16;       // candidates kept per table slot (cut-off from the histogram)
  uint32_t nodesPerThread = 32;
  int seenScale = 0;       // seen-table size >= seenScale * width (0: 4096, as the CPU beam)
  id<MTLDevice> dev;
  id<MTLCommandQueue> q;
  id<MTLComputePipelineState> psHist, psEmit, psNext;
  int lookahead = 0;       // 1: rank candidates by their best grandchild's score
  id<MTLBuffer> bKids, bBest;
  size_t capKids = 0, capBest = 0;
  double gpuNextSec = 0;
  id<MTLBuffer> bRec, bTab;
  id<MTLBuffer> bNodes, bBonus, bHist, bCount, bOut;
  size_t capNodes = 0, capBonusRows = 0, capOut = 0;
  // stats
  long shortLevels = 0, levels = 0, candidates = 0;
  double gpuSec = 0, hostSec = 0;

  bool init(const TableBeam &beam) {
    tb = &beam;
    dev = MTLCreateSystemDefaultDevice();
    if (!dev) return false;
    NSError *err = nil;
    id<MTLLibrary> lib = [dev newLibraryWithSource:[NSString stringWithUTF8String:kGpuBeamSrc] options:nil error:&err];
    if (!lib) { fprintf(stderr, "metal: %s\n", err.localizedDescription.UTF8String); return false; }
    psHist = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"hist"] error:&err];
    psEmit = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"emit"] error:&err];
    psNext = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"bestnext"] error:&err];
    if (!psHist || !psEmit || !psNext) return false;
    q = [dev newCommandQueue];
    const Pool &pool = *beam.cs->pool;
    const size_t n = pool.size();
    bRec = [dev newBufferWithLength:sizeof(Rec) * n options:MTLResourceStorageModeShared];
    Rec *r = (Rec *)bRec.contents;
    for (size_t a = 0; a < n; a++)
      r[a] = {beam.rec[a].t0, beam.rec[a].t1, beam.rec[a].t2, beam.X[a], beam.rec[a].e4,
              (uint16_t)(beam.seamAware ? beam.headIdx[a] : 0), 0, 0, 0};
    bTab = [dev newBufferWithLength:64 * 4 options:MTLResourceStorageModeShared];
    bHist = [dev newBufferWithLength:BINS * 4 options:MTLResourceStorageModeShared];
    bCount = [dev newBufferWithLength:4 options:MTLResourceStorageModeShared];
    return true;
  }
  void ensure(__strong id<MTLBuffer> &b, size_t &cap, size_t need, size_t elem) {
    if (need <= cap) return;
    cap = std::max(need, cap * 2);
    b = [dev newBufferWithLength:cap * elem options:MTLResourceStorageModeShared];
  }
  double run(id<MTLComputePipelineState> ps, const Params &P, bool withOut, id<MTLBuffer> nodesBuf = nil, id<MTLBuffer> outBuf4 = nil) {
    id<MTLCommandBuffer> cb = [q commandBuffer];
    id<MTLComputeCommandEncoder> ce = [cb computeCommandEncoder];
    [ce setComputePipelineState:ps];
    [ce setBuffer:bRec offset:0 atIndex:0];
    [ce setBuffer:(nodesBuf ? nodesBuf : bNodes) offset:0 atIndex:1];
    [ce setBuffer:bTab offset:0 atIndex:2];
    [ce setBuffer:bBonus offset:0 atIndex:3];
    [ce setBuffer:(outBuf4 ? outBuf4 : withOut ? bCount : bHist) offset:0 atIndex:4];
    [ce setBytes:&P length:sizeof P atIndex:5];
    if (withOut) [ce setBuffer:bOut offset:0 atIndex:6];
    MTLSize grid = MTLSizeMake(P.algs, (P.nodes + P.nodesPerThread - 1) / P.nodesPerThread, 1);
    MTLSize tg = MTLSizeMake(std::min<NSUInteger>(ps.maxTotalThreadsPerThreadgroup, 1024), 1, 1);
    [ce dispatchThreads:grid threadsPerThreadgroup:tg];
    [ce endEncoding];
    [cb commit];
    [cb waitUntilCompleted];
    return cb.GPUEndTime - cb.GPUStartTime;
  }

  std::vector<AlgRef> solve(const State &start, bool &ok, int T) {
    using Node = TableBeam::Node;
    using Slot = TableBeam::Slot;
    const TableBeam &B = *tb;
    ok = false;
    const Pool &pool = *B.cs->pool;
    const int mul = B.mulOverride ? B.mulOverride : 4 * B.cs->lambda;
    int *tab = (int *)bTab.contents;
    for (int c = 0; c <= 48; c++) tab[c] = B.scoreTab.empty() ? mul * c : B.scoreTab[c];
    const int TS = std::max(1, T);
    const uint64_t seedMix = B.slotSeed * 0x9E3779B97F4A7C15ULL;
    auto tieKey = [seedMix](uint64_t h) {
      uint64_t x = (h + seedMix) * 0xD6E8FEB86659FD93ULL;
      return x ^ (x >> 32);
    };
    auto slotOf = [TS, seedMix](uint64_t h) {
      uint64_t x = h ^ seedMix;
      x ^= x >> 31;
      x *= 0xBF58476D1CE4E5B9ULL;
      x ^= x >> 29;
      return (size_t)(((unsigned __int128)x * (uint64_t)TS) >> 64);
    };
    std::vector<Node> nodes;
    nodes.push_back({start, B.zob(start), 0, -1, 0, 0xFFFF});
    std::vector<int> level = {0};
    int SEEN = 1 << 12;  // as the CPU beam; seenScale > 0: at least seenScale * width
    while (seenScale > 0 && SEEN < seenScale * TS) SEEN <<= 1;
    std::vector<uint64_t> seen(SEEN, 0);
    seen[nodes[0].h & (SEEN - 1)] = nodes[0].h;
    std::vector<Slot> table(TS);
    int best4 = 1 << 30, bestNode = -1;
    long bestAlg = -1;
    const FinishTable::Slot *bestFin = nullptr;
    const int *e4 = B.cs->eff4.data();
    const size_t H = B.seamAware ? B.heads.size() : 1;
    std::map<uint16_t, uint32_t> rowOf;  // seam bonus row per tail (row 0: no tail)
    std::vector<int> rows;
    std::vector<Cand> cands;
    auto rowFor = [&](uint16_t tail) -> uint32_t {
      if (!B.seamAware || tail == 0xFFFF) return 0;
      auto it = rowOf.find(tail);
      if (it != rowOf.end()) return it->second;
      const uint32_t r = rows.size() / H;
      uint16_t mg;
      for (size_t h = 0; h < H; h++) rows.push_back(4 * TableBeam::seamSave(tail, B.heads[h], mg));
      rowOf.emplace(tail, r);
      return r;
    };
    rows.assign(H, 0);
    for (int depth = 0; depth < 60 && !level.empty(); depth++) {
      levels++;
      const double tH0 = now();
      // node data (solved nodes are skipped: correct = all, so no pair survives keep-solved)
      ensure(bNodes, capNodes, level.size(), sizeof(NodeD));
      NodeD *nd = (NodeD *)bNodes.contents;
      for (size_t i = 0; i < level.size(); i++) {
        const Node &N = nodes[level[i]];
        const auto &M = N.s.M;
        uint64_t corr = 0;
        for (int p = 0; p < 48; p++) corr |= (uint64_t)(N.s.col[p] == target(p)) << p;
        nd[i] = {M[1] | M[3] | M[5], M[2] | M[3], M[4] | M[5], corr == ((1ULL << 48) - 1) ? ~0ULL : corr, N.g4, rowFor(N.tail), 0, 0};
      }
      size_t bonusRows = rows.size() / H;
      ensure(bBonus, capBonusRows, bonusRows * H, 4);
      memcpy(bBonus.contents, rows.data(), rows.size() * 4);
      Params P{(uint32_t)pool.size(), (uint32_t)level.size(), nodesPerThread, (uint32_t)H, 0, 0, OFFSET, BINS};
      memset(bHist.contents, 0, BINS * 4);
      hostSec += now() - tH0;
      gpuSec += run(psHist, P, false);
      const double tH1 = now();
      const uint32_t *hist = (const uint32_t *)bHist.contents;
      const size_t want = (size_t)candMult * TS;
      size_t acc = 0;
      int cutBin = BINS - 1;
      for (; cutBin > 0; cutBin--) { acc += hist[cutBin]; if (acc >= want) break; }
      if (cutBin == 0) acc += hist[0];
      P.cutoff = cutBin;
      P.maxOut = acc;
      ensure(bOut, capOut, acc + 1, sizeof(Cand));
      *(uint32_t *)bCount.contents = 0;
      hostSec += now() - tH1;
      gpuSec += run(psEmit, P, true);
      double tH2 = now();
      const uint32_t got = std::min<uint32_t>(*(uint32_t *)bCount.contents, acc);
      candidates += got;
      cands.assign((Cand *)bOut.contents, (Cand *)bOut.contents + got);
      std::sort(cands.begin(), cands.end(), [](const Cand &x, const Cand &y) { return x.node < y.node || (x.node == y.node && x.alg < y.alg); });
      if (lookahead) {
        // each candidate child's best grandchild score (its own g4 subtracted on the host)
        const size_t K = cands.size();
        ensure(bKids, capKids, K, sizeof(NodeD));
        ensure(bBest, capBest, K, 4);
        NodeD *kd = (NodeD *)bKids.contents;
        std::vector<int> kg4(K);
        std::vector<uint8_t> kSolved(K);
        for (size_t i = 0; i < K; i++) {
          const Node &N = nodes[level[cands[i].node]];
          const uint32_t a = cands[i].alg;
          const int *bon = rows.data() + (size_t)nd[cands[i].node].row * H;
          const int hi = B.seamAware ? B.headIdx[a] : 0;
          kg4[i] = N.g4 + e4[a] - bon[hi];
          uint16_t ntail = 0xFFFF;
          if (B.seamAware) {
            uint16_t mg;
            int sv = TableBeam::seamSave(N.tail, pool.head[a], mg);
            ntail = B.nGroups[a] == 1 ? (sv || (N.tail != 0xFFFF && gaxis(N.tail) == gaxis(pool.head[a])) ? mg : B.lastG[a]) : B.lastG[a];
          }
          State cs2 = N.s.apply(pool, a);
          const auto &M = cs2.M;
          uint64_t corr = 0;
          for (int p = 0; p < 48; p++) corr |= (uint64_t)(cs2.col[p] == target(p)) << p;
          kSolved[i] = corr == ((1ULL << 48) - 1);
          kd[i] = {M[1] | M[3] | M[5], M[2] | M[3], M[4] | M[5], kSolved[i] ? ~0ULL : corr, 0, rowFor(ntail), 0, 0};
        }
        ensure(bBonus, capBonusRows, rows.size(), 4);
        memcpy(bBonus.contents, rows.data(), rows.size() * 4);
        int *bst = (int *)bBest.contents;
        for (size_t i = 0; i < K; i++) bst[i] = INT32_MIN;
        Params PN{(uint32_t)pool.size(), (uint32_t)K, nodesPerThread, (uint32_t)H, 0, 0, OFFSET, BINS};
        hostSec += now() - tH2;
        const double g = run(psNext, PN, false, bKids, bBest);
        gpuSec += g; gpuNextSec += g;
        tH2 = now();
        for (size_t i = 0; i < K; i++) {
          // solved children keep their own score; otherwise the best grandchild's
          if (!kSolved[i] && bst[i] != INT32_MIN) cands[i].score = bst[i] - kg4[i];
        }
      }
      // admission, node by node, as TableBeam::solve
      for (auto &sl : table) sl.score = INT32_MIN;
      int filled = 0, weakest = INT32_MIN;
      size_t ci = 0;
      for (size_t li = 0; li < level.size(); li++) {
        const int ni = level[li];
        const Node &N = nodes[ni];
        const int m = 48 - N.s.correct();
        if (m == 0) {
          if (N.g4 < best4) { best4 = N.g4; bestNode = ni; bestAlg = -1; bestFin = nullptr; }
          while (ci < cands.size() && cands[ci].node == li) ci++;
          continue;
        }
        if (B.finish) {
          const FinishTable::Slot *fi = B.finish->find(N.h);
          if (fi && N.g4 + fi->e4 < best4) {
            best4 = N.g4 + fi->e4;
            bestNode = N.parent >= 0 ? N.parent : ni;
            bestAlg = N.parent >= 0 ? (long)N.alg : -2; bestFin = fi;
          }
        }
        const int *bon = rows.data() + (size_t)nd[li].row * H;
        for (; ci < cands.size() && cands[ci].node == li; ci++) {
          const uint32_t a = cands[ci].alg;
          int sc = cands[ci].score;
          if (sc < weakest) continue;
          const int hi = B.seamAware ? B.headIdx[a] : 0;
          int g4 = N.g4 + e4[a] - bon[hi];
          uint16_t ntail = 0xFFFF;
          if (B.seamAware) {
            uint16_t mg;
            int sv = TableBeam::seamSave(N.tail, pool.head[a], mg);
            ntail = B.nGroups[a] == 1 ? (sv || (N.tail != 0xFFFF && gaxis(N.tail) == gaxis(pool.head[a])) ? mg : B.lastG[a]) : B.lastG[a];
          }
          if (g4 >= best4) continue;
          uint64_t h = N.h;
          const auto &src = pool.src[a];
          for (auto d : B.supp[a]) h ^= B.Z[d][N.s.col[d]] ^ B.Z[d][N.s.col[src[d]]];
          if (seen[h & (SEEN - 1)] == h) continue;
          if (B.finish) {
            const FinishTable::Slot *fi = B.finish->find(h);
            if (fi && g4 + fi->e4 < best4) { best4 = g4 + fi->e4; bestNode = ni; bestAlg = a; bestFin = fi; }
          }
          Slot &sl = table[slotOf(h)];
          if (sl.score == INT32_MIN) filled++;
          else if (sc < sl.score) continue;
          else if (sc == sl.score && tieKey(h) >= tieKey(sl.h)) continue;
          sl = {sc, ni, a, g4, h, ntail};
          if (filled == TS) {
            weakest = INT32_MAX;
            for (auto &t : table) weakest = std::min(weakest, t.score);
          }
        }
      }
      if (filled < TS && got >= want) shortLevels++;  // the CPU might have filled these slots from below the cut-off
      level.clear();
      for (auto &sl : table) {
        if (sl.score == INT32_MIN) continue;
        State ns = nodes[sl.parent].s.apply(pool, sl.alg);
        seen[sl.h & (SEEN - 1)] = sl.h;
        nodes.push_back({ns, sl.h, sl.g4, sl.parent, sl.alg, sl.tail});
        level.push_back(nodes.size() - 1);
      }
      hostSec += now() - tH2;
    }
    std::vector<AlgRef> r;
    if (bestNode < 0) return r;
    for (int nn = bestNode; nodes[nn].parent >= 0; nn = nodes[nn].parent) r.push_back({0, nodes[nn].alg});
    std::reverse(r.begin(), r.end());
    if (bestFin) {
      if (bestAlg >= 0) r.push_back({0, (uint32_t)bestAlg});
      r.push_back({bestFin->pid, bestFin->alg});
    }
    ok = true;
    return r;
  }
  static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
};
