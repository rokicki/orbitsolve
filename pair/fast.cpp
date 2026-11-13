// Fast path: greedy over a compact pool with a NEON scan, plus the depth-1
// endgame (enumeration + finish table) once few pieces are wrong.
//
//   fast [-n N] [-l LAMBDA] [-M MAXMIS] [-e] [-t THREADS] pool...
//
// Greedy step: choose the algorithm maximising  LAMBDA*correct_after - cost,
// never revisiting a state.  The pool is stored as six per-colour mask arrays;
// sixteen algorithms are scored per iteration: AND, byte popcount (vcnt),
// pairwise-add folding to one count byte per algorithm, then u8 score and a
// horizontal max to skip whole blocks.
#include "pool.h"
#include "eg2.h"
#include <arm_neon.h>
#include <atomic>
#include <thread>
#include <time.h>
#include <unordered_set>

// Effective (merge-aware) cost weights per move kind, in quarter moves.
struct Weights {
  int setupFace = 4, face = 4, slice2 = 4, slice3 = 4;
};

// literal face-turn conjugator depth: alg = X c X' with X face turns only
static int setupDepth(const std::vector<Move> &m) {
  int k = 0;
  while (2 * (k + 1) <= (int)m.size()) {
    const Move &a = m[k], &b = m[m.size() - 1 - k];
    if (a.depth != 1 || b.depth != 1 || a.face != b.face || (a.amt + b.amt) % 4) break;
    k++;
  }
  return k;
}

static int effCost4(const std::vector<Move> &m, const Weights &w) {
  int sd = setupDepth(m), c = 0;
  for (int i = 0; i < (int)m.size(); i++) {
    if (i < sd || i >= (int)m.size() - sd) c += w.setupFace;
    else if (m[i].depth == 1) c += w.face;
    else if (m[i].depth == 2) c += w.slice2;
    else c += w.slice3;
  }
  return c;
}

struct FastPool {
  size_t n = 0, npad = 0;
  std::vector<uint64_t> S[6];
  std::vector<uint8_t> bias;      // 12 - cost (0 for padding)
  std::vector<uint16_t> bias16;   // 4*16 - effective cost in quarter moves
  std::vector<int> eff4;          // effective cost in quarter moves
  void build(const Pool &p, const Weights &w = Weights()) {
    n = p.size();
    npad = (n + 15) & ~size_t(15);
    for (int c = 0; c < 6; c++) S[c].assign(npad, 0);
    bias.assign(npad, 0);
    bias16.assign(npad, 0);
    eff4.assign(npad, 0);
    for (size_t a = 0; a < n; a++) {
      for (int c = 0; c < 6; c++) S[c][a] = p.S[a][c];
      bias[a] = 12 - p.cost[a];
      std::string t = groups_str(std::vector<uint16_t>(p.gbegin(a), p.gend(a)));
      eff4[a] = effCost4(parse_alg(t), w);
      bias16[a] = 64 - eff4[a];
    }
  }
};

// best algorithm index by LAMBDA*count + bias (== LAMBDA*count - cost + 12)
static size_t scanBest(const FastPool &fp, const std::array<uint64_t, 6> &M, int lambda, int &bestScore) {
  uint64x2_t m[6];
  for (int c = 0; c < 6; c++) m[c] = vdupq_n_u64(M[c]);
  int best = -1;
  size_t bestA = 0;
  for (size_t a = 0; a < fp.npad; a += 16) {
    uint8x16_t cnt[8];
    for (int j = 0; j < 8; j++) {
      size_t b = a + 2 * j;
      uint8x16_t acc = vcntq_u8(vreinterpretq_u8_u64(vandq_u64(vld1q_u64(&fp.S[0][b]), m[0])));
      for (int c = 1; c < 6; c++)
        acc = vaddq_u8(acc, vcntq_u8(vreinterpretq_u8_u64(vandq_u64(vld1q_u64(&fp.S[c][b]), m[c]))));
      cnt[j] = acc;  // bytes 0..7: alg b, bytes 8..15: alg b+1
    }
    // fold: vpaddq sums adjacent bytes; after three levels byte j is the full
    // count for algorithm a+j (natural order -- checked at startup)
    uint8x16_t r = vpaddq_u8(vpaddq_u8(vpaddq_u8(cnt[0], cnt[1]), vpaddq_u8(cnt[2], cnt[3])),
                             vpaddq_u8(vpaddq_u8(cnt[4], cnt[5]), vpaddq_u8(cnt[6], cnt[7])));
    uint8x16_t sc = vld1q_u8(&fp.bias[a]);
    for (int k = 0; k < lambda; k++) sc = vaddq_u8(sc, r);
    if (vmaxvq_u8(sc) > best) {
      uint8_t v[16];
      vst1q_u8(v, sc);
      for (int j = 0; j < 16; j++)
        if (v[j] > best) { best = v[j]; bestA = a + j; }
    }
  }
  bestScore = best;
  return bestA;
}

// Top-k algorithms by LAMBDA*count + bias for one state (k small).
static void scanTopK(const FastPool &fp, const std::array<uint64_t, 6> &M, int lambda, int k,
                     std::vector<std::pair<int, uint32_t>> &out) {
  uint64x2_t m[6];
  for (int c = 0; c < 6; c++) m[c] = vdupq_n_u64(M[c]);
  out.clear();
  int thr = -1;  // current k-th best score
  const uint16_t mul = 4 * lambda;
  for (size_t a = 0; a < fp.npad; a += 16) {
    uint8x16_t cnt[8];
    for (int j = 0; j < 8; j++) {
      size_t b = a + 2 * j;
      uint8x16_t acc = vcntq_u8(vreinterpretq_u8_u64(vandq_u64(vld1q_u64(&fp.S[0][b]), m[0])));
      for (int c = 1; c < 6; c++)
        acc = vaddq_u8(acc, vcntq_u8(vreinterpretq_u8_u64(vandq_u64(vld1q_u64(&fp.S[c][b]), m[c]))));
      cnt[j] = acc;
    }
    uint8x16_t r = vpaddq_u8(vpaddq_u8(vpaddq_u8(cnt[0], cnt[1]), vpaddq_u8(cnt[2], cnt[3])),
                             vpaddq_u8(vpaddq_u8(cnt[4], cnt[5]), vpaddq_u8(cnt[6], cnt[7])));
    // score = 4*LAMBDA*count + 64 - effective cost (quarter moves), in u16
    uint16x8_t lo = vmlaq_n_u16(vld1q_u16(&fp.bias16[a]), vmovl_u8(vget_low_u8(r)), mul);
    uint16x8_t hi = vmlaq_n_u16(vld1q_u16(&fp.bias16[a + 8]), vmovl_u8(vget_high_u8(r)), mul);
    if (std::max(vmaxvq_u16(lo), vmaxvq_u16(hi)) > thr) {
      uint16_t v[16];
      vst1q_u16(v, lo);
      vst1q_u16(v + 8, hi);
      for (int j = 0; j < 16; j++)
        if (v[j] > thr && a + j < fp.n) {
          out.push_back({v[j], (uint32_t)(a + j)});
          if ((int)out.size() > k) {
            std::sort(out.begin(), out.end(), [](auto &x, auto &y) { return x.first > y.first; });
            out.resize(k);
          }
          if ((int)out.size() == k) thr = std::min_element(out.begin(), out.end())->first;
        }
    }
  }
  std::sort(out.begin(), out.end(), [](auto &x, auto &y) { return x.first > y.first; });
}

// Call f(alg, count) for every algorithm whose correct-after count exceeds c0.
template <class F>
static void scanGain(const FastPool &fp, const std::array<uint64_t, 6> &M, int c0, F &&f) {
  uint64x2_t m[6];
  for (int c = 0; c < 6; c++) m[c] = vdupq_n_u64(M[c]);
  uint8x16_t thr = vdupq_n_u8((uint8_t)c0);
  for (size_t a = 0; a < fp.npad; a += 16) {
    uint8x16_t cnt[8];
    for (int j = 0; j < 8; j++) {
      size_t b = a + 2 * j;
      uint8x16_t acc = vcntq_u8(vreinterpretq_u8_u64(vandq_u64(vld1q_u64(&fp.S[0][b]), m[0])));
      for (int c = 1; c < 6; c++)
        acc = vaddq_u8(acc, vcntq_u8(vreinterpretq_u8_u64(vandq_u64(vld1q_u64(&fp.S[c][b]), m[c]))));
      cnt[j] = acc;
    }
    uint8x16_t r = vpaddq_u8(vpaddq_u8(vpaddq_u8(cnt[0], cnt[1]), vpaddq_u8(cnt[2], cnt[3])),
                             vpaddq_u8(vpaddq_u8(cnt[4], cnt[5]), vpaddq_u8(cnt[6], cnt[7])));
    if (vmaxvq_u8(vcgtq_u8(r, thr))) {
      uint8_t v[16];
      vst1q_u8(v, r);
      for (int j = 0; j < 16; j++)
        if (v[j] > c0 && a + j < fp.n) f(a + j, v[j]);
    }
  }
}

// Endgame: one algorithm from the scan pool, then the finish table (or none).
static int scanFinish(const FastPool &fp, const Pool &p, const EG2 &eg, const State &s, int bound,
                      int &firstAlg, int &finAlg) {
  int best = bound;
  firstAlg = -1;
  finAlg = -1;
  uint64_t h = eg.zob(s.col);
  int c0 = s.correct();
  if (48 - c0 <= eg.finMaxMis) {
    int b = eg.find(h);
    if (b >= 0 && eg.fin.cost[b] < best) { best = eg.fin.cost[b]; finAlg = b; }
  }
  scanGain(fp, s.M, c0, [&](size_t a, int cnt) {
    int c = p.cost[a];
    if (c + (cnt < 48 ? 4 : 0) >= best) return;
    if (cnt == 48) { best = c; firstAlg = a; finAlg = -1; return; }
    if (48 - cnt > eg.finMaxMis) return;
    uint64_t h2 = h;
    const auto &sr = p.src[a];
    for (int d = 0; d < 48; d++)
      if (sr[d] != d) h2 ^= eg.Z[d][s.col[d]] ^ eg.Z[d][s.col[sr[d]]];
    int b = eg.find(h2);
    if (b >= 0 && c + eg.fin.cost[b] < best) { best = c + eg.fin.cost[b]; firstAlg = a; finAlg = b; }
  });
  return best;
}

// Level beam of width W (W=1 is the greedy above, modulo tie-breaks), with the
// depth-1 subset endgame tried at every node with at most maxMis wrong.
struct BNode { State s; int g4, g, parent; uint32_t alg; };

// effective quarter-move cost of an algorithm given as a pool entry
static int effOf(const Pool &p, uint32_t a, const Weights &w) {
  return effCost4(parse_alg(groups_str(std::vector<uint16_t>(p.gbegin(a), p.gend(a)))), w);
}

// Level beam of width W ranked by effective (merge-aware) cost; the depth-1
// subset endgame is tried at every node with at most maxMis wrong.  Returns
// the real move count of the solution with the lowest effective cost.
static int beamSolve(const FastPool &fp, const Pool &pool, const EG2 *eg, const State &start, int W, int lambda,
                     int maxMis, const Weights &wt, std::vector<uint32_t> &seq, EG2::Sol &fin) {
  std::vector<BNode> nodes = {{start, 0, 0, -1, 0}};
  std::vector<int> frontier = {0};
  std::unordered_set<uint64_t> seen = {start.hash()};
  int best4 = 1 << 20, bestReal = -1, bestNode = -1;
  EG2::Sol bestSol;
  std::vector<std::pair<int, uint32_t>> top;
  for (int depth = 0; depth < 40 && !frontier.empty(); depth++) {
    struct C { int key, g4, g, parent; uint32_t a; };
    std::vector<C> cs;
    for (int ni : frontier) {
      const BNode &nd = nodes[ni];
      int m = 48 - nd.s.correct();
      if (m == 0) {
        if (nd.g4 < best4) { best4 = nd.g4; bestReal = nd.g; bestNode = ni; bestSol = EG2::Sol(); }
        continue;
      }
      if (eg && m <= maxMis) {
        auto sol = eg->solve(nd.s, 1, 200);
        if (sol.cost < 200) {
          int e4 = 0;
          for (auto x : sol.path) e4 += effOf(eg->cand, x, wt);
          if (sol.finAlg >= 0) e4 += effOf(eg->fin, sol.finAlg, wt);
          if (nd.g4 + e4 < best4) { best4 = nd.g4 + e4; bestReal = nd.g + sol.cost; bestNode = ni; bestSol = sol; }
        }
      }
      scanTopK(fp, nd.s.M, lambda, 2 * W, top);
      for (auto &t : top) {
        int ng4 = nd.g4 + fp.eff4[t.second];
        if (ng4 >= best4) continue;
        cs.push_back({t.first - nd.g4, ng4, nd.g + pool.cost[t.second], ni, t.second});
      }
    }
    std::sort(cs.begin(), cs.end(), [](const C &x, const C &y) { return x.key > y.key; });
    frontier.clear();
    for (auto &c : cs) {
      State ns = nodes[c.parent].s.apply(pool, c.a);
      if (!seen.insert(ns.hash()).second) continue;
      nodes.push_back({ns, c.g4, c.g, c.parent, c.a});
      frontier.push_back(nodes.size() - 1);
      if ((int)frontier.size() >= W) break;
    }
  }
  seq.clear();
  for (int n = bestNode; n >= 0 && nodes[n].parent >= 0; n = nodes[n].parent) seq.push_back(nodes[n].alg);
  std::reverse(seq.begin(), seq.end());
  fin = bestSol;
  return bestNode < 0 ? -1 : bestReal;
}

int main(int argc, char **argv) {
  int N = 256, lambda = 3, maxMis = 8, threads = 16, W = 0;
  Weights wt;
  const char *outFile = nullptr;  // -o FILE: write each solution as algorithm texts
  bool useEG = true;
  const char *egFirst = nullptr;  // -f POOL: scan-based endgame with this first-alg pool
  std::vector<const char *> files;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "-n") N = atoi(argv[++i]);
    else if (a == "-l") lambda = atoi(argv[++i]);
    else if (a == "-M") maxMis = atoi(argv[++i]);
    else if (a == "-e") useEG = false;
    else if (a == "-f") egFirst = argv[++i];
    else if (a == "-w") W = atoi(argv[++i]);
    else if (a == "-o") outFile = argv[++i];
    else if (a == "-ws") wt.setupFace = atoi(argv[++i]);  // quarter moves per setup face turn
    else if (a == "-wf") wt.face = atoi(argv[++i]);
    else if (a == "-w2") wt.slice2 = atoi(argv[++i]);
    else if (a == "-t") threads = atoi(argv[++i]);
    else files.push_back(argv[i]);
  }
  Pool pool;
  for (auto f : files) pool.load(f, 99, false);
  pool.sortByCost();
  FastPool fp;
  fp.build(pool, wt);
  // self-check of the fold order against the scalar count
  {
    std::mt19937_64 r(5);
    State s = randomState(r);
    uint64x2_t m[6];
    for (int c = 0; c < 6; c++) m[c] = vdupq_n_u64(s.M[c]);
    uint8x16_t cnt[8];
    for (int j = 0; j < 8; j++) {
      uint8x16_t acc = vdupq_n_u8(0);
      for (int c = 0; c < 6; c++) acc = vaddq_u8(acc, vcntq_u8(vreinterpretq_u8_u64(vandq_u64(vld1q_u64(&fp.S[c][2 * j]), m[c]))));
      cnt[j] = acc;
    }
    uint8x16_t rr = vpaddq_u8(vpaddq_u8(vpaddq_u8(cnt[0], cnt[1]), vpaddq_u8(cnt[2], cnt[3])),
                              vpaddq_u8(vpaddq_u8(cnt[4], cnt[5]), vpaddq_u8(cnt[6], cnt[7])));
    uint8_t cs[16];
    vst1q_u8(cs, rr);
    for (int j = 0; j < 16; j++)
      if (cs[j] != correctAfter(pool.S[j], s.M)) { fprintf(stderr, "fold order mismatch at %d\n", j); return 1; }
  }
  EG2 eg;
  if (useEG) {
    eg.cand.load("eg8.d.txt", 99, false);
    eg.fin.load("eg8.d.txt", 99, false);
    eg.fin.load("p9.d.txt", 99, false);
    eg.fin.load("c12x.d.txt", 99, false);
    eg.build();
  }
  Pool firstPool;
  FastPool firstFp;
  if (egFirst) {
    firstPool.load(egFirst, 99, false);
    firstPool.sortByCost();
    firstFp.build(firstPool);
  }
  fprintf(stderr, "pool %zu\n", pool.size());
  std::mt19937_64 rng(1);
  std::vector<State> inst;
  for (int i = 0; i < N; i++) inst.push_back(randomState(rng));
  std::vector<int> moves(N), diagAlgs(N, 0);
  std::vector<std::string> sol(N);
  auto txt = [](const Pool &p, uint32_t a) {
    return groups_str(std::vector<uint16_t>(p.gbegin(a), p.gend(a)));
  };
  std::vector<double> cpu(N);
  std::atomic<int> nx{0};
  std::vector<std::thread> th;
  for (int t = 0; t < threads; t++)
    th.emplace_back([&] {
      for (int i; (i = nx++) < N;) {
        timespec t0, t1;
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t0);
        (void)t1;
        if (W > 0) {
          std::vector<uint32_t> sq;
          EG2::Sol fs;
          int c = beamSolve(fp, pool, useEG ? &eg : nullptr, inst[i], W, lambda, maxMis, wt, sq, fs);
          clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t1);
          cpu[i] = (t1.tv_sec - t0.tv_sec) + 1e-9 * (t1.tv_nsec - t0.tv_nsec);
          State v = inst[i];
          for (auto a : sq) v = v.apply(pool, a);
          for (auto a : fs.path) v = v.apply(eg.cand, a);
          if (fs.finAlg >= 0) v = v.apply(eg.fin, fs.finAlg);
          moves[i] = (c >= 0 && v.correct() == 48) ? c : -1;
          for (auto a : sq) diagAlgs[i] += pool.diag[a];
          for (auto a : fs.path) diagAlgs[i] += eg.cand.diag[a];
          if (fs.finAlg >= 0) diagAlgs[i] += eg.fin.diag[fs.finAlg];
          if (outFile) {
            std::string o;
            for (int p = 0; p < 48; p++) o += std::to_string(inst[i].col[p]) + (p < 47 ? " " : "");
            // a trailing '*' marks algorithms that move diagonal pieces
            auto D = [](const Pool &p, uint32_t a) { return std::string(p.diag[a] ? " *" : ""); };
            for (auto a : sq) o += " | " + txt(pool, a) + D(pool, a);
            for (auto a : fs.path) o += " | " + txt(eg.cand, a) + D(eg.cand, a);
            if (fs.finAlg >= 0) o += " | " + txt(eg.fin, fs.finAlg) + D(eg.fin, fs.finAlg);
            sol[i] = o;
          }
          continue;
        }
        State s = inst[i];
        int g = 0;
        std::unordered_set<uint64_t> seen = {s.hash()};
        std::vector<uint32_t> seq;
        EG2::Sol fin;
        bool done = false;
        int bestTotal = 1000, bestFirst = -1, bestFin = -1;
        std::vector<uint32_t> bestSeq;
        for (int step = 0; step < 60; step++) {
          int m = 48 - s.correct();
          if (m == 0) { done = true; break; }
          if (useEG && egFirst && m <= maxMis) {
            int fa, fb;
            int c = scanFinish(firstFp, firstPool, eg, s, bestTotal - g, fa, fb);
            if (g + c < bestTotal) {
              bestTotal = g + c;
              bestSeq = seq;
              bestFirst = fa;
              bestFin = fb;
            }
            if (g + 8 >= bestTotal) break;
          } else if (useEG && m <= maxMis) {
            auto sol = eg.solve(s, 1, 200);
            if (sol.cost < 200) { g += sol.cost; fin = sol; done = true; break; }
          }
          int bs;
          size_t a = scanBest(fp, s.M, lambda, bs);
          State ns = s.apply(pool, a);
          if (!seen.insert(ns.hash()).second) {
            // rare: fall back to the best unseen successor, scalar
            int bestv = -1000;
            size_t ba = 0;
            for (size_t b = 0; b < pool.size(); b++) {
              int v = lambda * correctAfter(pool.S[b], s.M) - pool.cost[b];
              if (v > bestv) {
                State t = s.apply(pool, b);
                if (!seen.count(t.hash())) { bestv = v; ba = b; }
              }
            }
            a = ba;
            ns = s.apply(pool, a);
            seen.insert(ns.hash());
          }
          s = ns;
          g += pool.cost[a];
          seq.push_back(a);
        }
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t1);
        cpu[i] = (t1.tv_sec - t0.tv_sec) + 1e-9 * (t1.tv_nsec - t0.tv_nsec);
        if (egFirst && (bestTotal < 1000) && (!done || bestTotal <= g)) {
          // the scan endgame's best beats (or replaces) the plain greedy finish
          seq = bestSeq;
          fin = EG2::Sol();
          g = bestTotal;
          done = true;
        }
        // verify
        State v = inst[i];
        for (auto a : seq) v = v.apply(pool, a);
        for (auto a : fin.path) v = v.apply(eg.cand, a);
        if (fin.finAlg >= 0) v = v.apply(eg.fin, fin.finAlg);
        if (egFirst && g == bestTotal) {
          if (bestFirst >= 0) v = v.apply(firstPool, bestFirst);
          if (bestFin >= 0) v = v.apply(eg.fin, bestFin);
        }
        moves[i] = (done && v.correct() == 48) ? g : -1;
      }
    });
  for (auto &t : th) t.join();
  if (outFile) {
    FILE *f = fopen(outFile, "w");
    for (int i = 0; i < N; i++) if (moves[i] >= 0) fprintf(f, "%s\n", sol[i].c_str());
    fclose(f);
  }
  double sd = 0;
  for (int i = 0; i < N; i++) sd += diagAlgs[i];
  printf("diagonal-moving algorithms per pair: %.2f\n", sd / N);
  double sm = 0, sc = 0;
  int ok = 0;
  for (int i = 0; i < N; i++) {
    sc += cpu[i];
    if (moves[i] >= 0) { sm += moves[i]; ok++; }
  }
  printf("W=%d pool=%zu lambda=%d M=%d eg=%d  solved %d/%d  moves %.2f  %.3f ms/pair (thread cpu)\n", W, pool.size(), lambda,
         maxMis, useEG, ok, N, sm / ok, 1000 * sc / N);
}
