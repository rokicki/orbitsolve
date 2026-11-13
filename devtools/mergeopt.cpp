// Merge orders: levels, then local improvement.  Reads a MERGEDUMP file and
// compares, on the same instances:
//   greedy (Merger::merge), pairwise (Merger::mergePairwise, full constraint
//   checks), levels (as late as possible; each level merged pairwise with no
//   constraints), and local improvement started from levels / greedy /
//   pairwise.
// Local improvement works on one sequence: move a run of 1..maxRun
// consecutive instances to just after a partner its first instance seams
// with, or to just before a partner its last instance seams with (partners
// from buckets of first / last groups).  A move is legal when every
// predecessor of the run stays before it and every successor after it,
// checked with order labels.  Its gain is the change in pairwise seam savings
// (exact per pair, cascades included); moves that gain are applied.  Passes
// repeat until no move gains or maxPass is reached.  Final costs are exact.
//   mergeopt DUMP.txt [maxRun 3] [maxPass 20] [candLim 64]
#include "merge.h"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

struct Inst {
  std::vector<PLayer> moves;
  std::vector<int> goff;  // group starts, plus moves.size()
};

int main(int argc, char **argv) {
  if (argc < 2) { fprintf(stderr, "usage: mergeopt DUMP.txt [maxRun] [maxPass] [candLim]\n"); return 1; }
  const bool bench = argc > 2 && std::string(argv[2]) == "--bench";
  int maxRun = 3, maxPass = 20, candLim = 64;
  if (!bench) {
    if (argc > 2) maxRun = atoi(argv[2]);
    if (argc > 3) maxPass = atoi(argv[3]);
    if (argc > 4) candLim = atoi(argv[4]);
  }
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  std::ifstream in(argv[1]);
  std::string line, a, b, w, c;
  int N;
  std::getline(in, line);
  { std::istringstream h(line); h >> a >> b >> w >> c >> N; }
  Merger mg(N);
  while (std::getline(in, line)) {
    std::istringstream ls(line);
    MergeInst I;
    size_t nd, nm;
    ls >> nd;
    for (size_t k = 0; k < nd; k++) { int cc; std::string hx; ls >> cc >> hx; I.deps.push_back({cc, std::stoull(hx, nullptr, 16)}); }
    ls >> nm;
    for (size_t k = 0; k < nm; k++) { int x, p, t; ls >> x >> p >> t; I.moves.push_back({(int8_t)x, (int16_t)p, (int8_t)t}); }
    mg.insts.push_back(std::move(I));
  }
  const int n = mg.insts.size();
  long raw = 0;
  for (auto &I : mg.insts) raw += I.moves.size();
  // constraints
  std::vector<std::vector<int>> pr(n), su(n);
  {
    std::vector<std::vector<std::pair<int, uint64_t>>> comp;
    for (int i = 0; i < n; i++)
      for (auto &d : mg.insts[i].deps) {
        if ((int)comp.size() <= d.first) comp.resize(d.first + 1);
        for (auto &p : comp[d.first]) if (p.second & d.second) { pr[i].push_back(p.first); su[p.first].push_back(i); }
        comp[d.first].push_back({i, d.second});
      }
  }
  std::vector<Inst> I(n);
  for (int i = 0; i < n; i++) {
    I[i].moves = mg.insts[i].moves;
    for (size_t k = 0; k < I[i].moves.size(); k++)
      if (k == 0 || I[i].moves[k].axis != I[i].moves[k - 1].axis) I[i].goff.push_back(k);
    I[i].goff.push_back(I[i].moves.size());
  }
  // exact pairwise saving of j directly after i (cascade through fully cancelling groups)
  auto save = [&](int i, int j) {
    if (i < 0 || j < 0) return 0;
    const auto &A = I[i], &B = I[j];
    int s = 0;
    for (int gi = (int)A.goff.size() - 2, gj = 0; gi >= 0 && gj + 1 < (int)B.goff.size(); gi--, gj++) {
      const int a0 = A.goff[gi], a1 = A.goff[gi + 1], b0 = B.goff[gj], b1 = B.goff[gj + 1];
      if (A.moves[a0].axis != B.moves[b0].axis) break;
      int cancelled = 0;
      for (int h = b0; h < b1; h++)
        for (int t = a0; t < a1; t++)
          if (A.moves[t].pos == B.moves[h].pos) {
            if (((A.moves[t].amt + B.moves[h].amt) & 3) == 0) { s += 2; cancelled++; }
            else s += 1;
            break;
          }
      if (cancelled != a1 - a0 || cancelled != b1 - b0) break;
    }
    return s;
  };
  auto exactCost = [&](const std::vector<int> &ord) {
    std::vector<Merger::Group> t;
    long cost = 0;
    for (int i : ord) for (auto &p : I[i].moves) Merger::push(t, cost, p);
    return cost;
  };
  auto valid = [&](const std::vector<int> &ord) {
    std::vector<int> at(n, -1);
    for (size_t k = 0; k < ord.size(); k++) at[ord[k]] = k;
    if ((int)ord.size() != n) return false;
    for (int i = 0; i < n; i++) for (int s : su[i]) if (at[i] > at[s]) return false;
    return true;
  };
  // buckets: instances by a layer of their first group, and of their last group
  auto bkey = [&](int axis, int pos, int amt) { return ((size_t)axis * (N + 2) + pos) * 4 + amt; };
  std::vector<std::vector<int>> headB(3 * (size_t)(N + 2) * 4), tailB(3 * (size_t)(N + 2) * 4);
  for (int i = 0; i < n; i++) {
    const auto &m = I[i].moves;
    if (m.empty()) continue;
    for (int k = I[i].goff[0]; k < I[i].goff[1]; k++) headB[bkey(m[k].axis, m[k].pos, m[k].amt)].push_back(i);
    int g = I[i].goff.size() - 2;
    for (int k = I[i].goff[g]; k < I[i].goff[g + 1]; k++) tailB[bkey(m[k].axis, m[k].pos, m[k].amt)].push_back(i);
  }

  // local improvement of an order
  long nRelabel = 0, nEval = 0;
  auto improve = [&](std::vector<int> ord, long &accepted, int &passes) {
    std::vector<int> nx(n, -1), pv(n, -1);
    std::vector<long double> lab(n);  // order labels (relabelled when gaps run out)
    for (int k = 0; k < n; k++) {
      lab[ord[k]] = k;
      if (k) { pv[ord[k]] = ord[k - 1]; nx[ord[k - 1]] = ord[k]; }
    }
    int first = ord[0];
    auto relabel = [&]() {
      nRelabel++;
      long double v = 0;
      for (int x = first; x >= 0; x = nx[x]) { lab[x] = v; v += 1; }
    };
    accepted = 0;
    for (passes = 0; passes < maxPass; passes++) {
      long acc0 = accepted;
      for (int x0 = 0; x0 < n; x0++) {
        int xe = x0;
        for (int len = 1; len <= maxRun; len++) {
          if (len > 1) { xe = nx[xe]; if (xe < 0) break; }
          // the run x0..xe (contiguous; labels increase along the list)
          const int P = pv[x0], Nn = nx[xe];
          const long double r0 = lab[x0], r1 = lab[xe];
          auto inRun = [&](int y) { return y >= 0 && lab[y] >= r0 && lab[y] <= r1; };
          // every predecessor outside the run must stay at or before u, every successor at or after v
          long double lo = -1e30L, hi = 1e30L;
          bool hasPred = false, hasSucc = false;
          for (int y = x0;; y = nx[y]) {
            for (int p : pr[y]) if (!inRun(p)) { lo = std::max(lo, lab[p]); hasPred = true; }
            for (int t : su[y]) if (!inRun(t)) { hi = std::min(hi, lab[t]); hasSucc = true; }
            if (y == xe) break;
          }
          const int removeGain = save(P, Nn) - save(P, x0) - save(xe, Nn);
          int bestGain = 0, bu = -2, bv = -2, evaluated = 0;
          auto consider = [&](int u, int v) {  // insert between u and v, adjacent once the run is out (-1: an end)
            if (inRun(u) || inRun(v) || (u == P && v == Nn)) return;
            if (u >= 0 ? lab[u] < lo : hasPred) return;
            if (v >= 0 ? lab[v] > hi : hasSucc) return;
            nEval++;
            const int g = removeGain + save(u, x0) + save(xe, v) - save(u, v);
            if (g > bestGain) { bestGain = g; bu = u; bv = v; }
          };
          {
            const auto &m = I[x0].moves;
            for (int k = I[x0].goff[0]; k < I[x0].goff[1]; k++)
              for (int amt = 1; amt <= 3; amt++)
                for (int u : tailB[bkey(m[k].axis, m[k].pos, amt)]) {  // after u
                  if (++evaluated > candLim) break;
                  consider(u, nx[u] == x0 ? Nn : nx[u]);
                }
            evaluated = 0;
            const auto &me = I[xe].moves;
            const int g = I[xe].goff.size() - 2;
            for (int k = I[xe].goff[g]; k < I[xe].goff[g + 1]; k++)
              for (int amt = 1; amt <= 3; amt++)
                for (int v : headB[bkey(me[k].axis, me[k].pos, amt)]) {  // before v
                  if (++evaluated > candLim) break;
                  consider(pv[v] == xe ? P : pv[v], v);
                }
          }
          if (bestGain <= 0) continue;
          // unlink the run, link it between bu and bv
          if (P >= 0) nx[P] = Nn; else first = Nn;
          if (Nn >= 0) pv[Nn] = P;
          if (bu >= 0) nx[bu] = x0; else first = x0;
          pv[x0] = bu;
          nx[xe] = bv;
          if (bv >= 0) pv[bv] = xe;
          const long double L = bu >= 0 ? lab[bu] : (bv >= 0 ? lab[bv] - (len + 1) : 0), R = bv >= 0 ? lab[bv] : L + len + 1;
          if (R - L < 1e-6L) relabel();
          else {
            long double step = (R - L) / (len + 1), v = L + step;
            for (int y = x0;; y = nx[y]) { lab[y] = v; v += step; if (y == xe) break; }
          }
          accepted++;
          break;  // x0 moved: next x0
        }
      }
      if (accepted == acc0) break;
    }
    std::vector<int> out;
    for (int x = first; x >= 0; x = nx[x]) out.push_back(x);
    return out;
  };

  auto report = [&](const char *name, const std::vector<int> &ord, double t) {
    printf("  %-28s %8ld moves (saves %5.1f%% of raw %ld)  %7.3fs%s\n", name, exactCost(ord), 100.0 * (raw - exactCost(ord)) / raw, raw, t,
           valid(ord) ? "" : "  ORDER INVALID");
  };
  auto levelsOrder = [&]() {
    std::vector<int> lev(n, 0), out;
    int L = 0;
    for (int i = n - 1; i >= 0; i--) { for (int s : su[i]) lev[i] = std::max(lev[i], lev[s] + 1); L = std::max(L, lev[i]); }
    for (int i = 0; i < n; i++) lev[i] = L - lev[i];
    for (int l = 0; l <= L; l++) {
      Merger m(N);
      std::vector<int> ids;
      for (int i = 0; i < n; i++) if (lev[i] == l) { ids.push_back(i); MergeInst J; J.moves = mg.insts[i].moves; m.insts.push_back(J); }
      long r, x;
      std::vector<int> ord;
      m.mergePairwise(r, x, &ord);
      for (int k : ord) out.push_back(ids[k]);
    }
    return out;
  };
  if (bench) {
    // mergeopt DUMP --bench REPS CFG...   CFG = greedy | beam:W | pair | levels, optionally +imp:RUN:PASS:CAND
    // prints "CFG moves seconds" (fastest of REPS runs, one thread)
    const int reps = atoi(argv[3]);
    for (int k = 4; k < argc; k++) {
      std::string cfg = argv[k], base = cfg, imp;
      size_t p = cfg.find("+imp");
      if (p != std::string::npos) { base = cfg.substr(0, p); imp = cfg.substr(p + 4); }
      double best = 1e30;
      long moves = 0;
      for (int r = 0; r < reps; r++) {
        double t0 = now();
        std::vector<int> ord;
        Merger m = mg;
        long rr, x;
        if (base == "greedy") m.merge(rr, x, &ord);
        else if (base.rfind("beam:", 0) == 0) m.mergeBeam(atoi(base.c_str() + 5), rr, x, &ord);
        else if (base == "pair") m.mergePairwise(rr, x, &ord);
        else if (base.rfind("restarts:", 0) == 0) {
          // restarts:K:NOISE  K seeded pairwise runs (seed 0 = deterministic), each improved; keep the best
          int K = 1, noise = 0;
          sscanf(base.c_str() + 9, "%d:%d", &K, &noise);
          long bestCost = -1;
          for (int r = 0; r < K; r++) {
            Merger mm = mg;
            mm.pairSeed = r;
            mm.pairNoise = noise;
            std::vector<int> o;
            long r2, x2;
            mm.mergePairwise(r2, x2, &o);
            long acc;
            int passes;
            maxRun = 3; maxPass = 20; candLim = 64;
            o = improve(o, acc, passes);
            long c = exactCost(o);
            if (bestCost < 0 || c < bestCost) { bestCost = c; ord = o; }
          }
        }
        else if (base == "levels") ord = levelsOrder();
        else { fprintf(stderr, "unknown config %s\n", cfg.c_str()); return 1; }
        if (!imp.empty()) {
          sscanf(imp.c_str(), ":%d:%d:%d", &maxRun, &maxPass, &candLim);
          long acc;
          int passes;
          ord = improve(ord, acc, passes);
        }
        best = std::min(best, now() - t0);
        moves = exactCost(ord);
        if (!valid(ord)) { printf("%s INVALID\n", cfg.c_str()); return 1; }
      }
      printf("%s %ld %.6f\n", cfg.c_str(), moves, best);
      fflush(stdout);
    }
    return 0;
  }
  printf("%s: %d instances\n", argv[1], n);
  std::vector<int> oGreedy, oPair, oLev;
  double t0 = now();
  { Merger m = mg; long r, x; m.merge(r, x, &oGreedy); }
  report("greedy", oGreedy, now() - t0);
  std::vector<int> oBeam;
  if (9e-8 * (double)n * n * 256 < 60) {
    t0 = now();
    Merger m = mg; long r, x; m.mergeBeam(256, r, x, &oBeam);
    report("merge beam 256", oBeam, now() - t0);
  }
  if (n <= 25000) {
    t0 = now();
    Merger m = mg; long r, x; m.mergePairwise(r, x, &oPair);
    report("pairwise (full constraints)", oPair, now() - t0);
  }
  t0 = now();
  {
    std::vector<int> lev(n, 0);
    int L = 0;
    for (int i = n - 1; i >= 0; i--) { for (int s : su[i]) lev[i] = std::max(lev[i], lev[s] + 1); L = std::max(L, lev[i]); }
    for (int i = 0; i < n; i++) lev[i] = L - lev[i];
    for (int l = 0; l <= L; l++) {
      Merger m(N);
      std::vector<int> ids;
      for (int i = 0; i < n; i++) if (lev[i] == l) { ids.push_back(i); MergeInst J; J.moves = mg.insts[i].moves; m.insts.push_back(J); }
      long r, x;
      std::vector<int> ord;
      m.mergePairwise(r, x, &ord);
      for (int k : ord) oLev.push_back(ids[k]);
    }
  }
  const double tLev = now() - t0;
  report("levels", oLev, tLev);
  for (auto [name, start, tStart] : {std::tuple<const char *, std::vector<int> *, double>{"levels + improve", &oLev, tLev},
                                     {"greedy + improve", &oGreedy, 0.0}, {"pairwise + improve", &oPair, 0.0},
                                     {"beam 256 + improve", &oBeam, 0.0}}) {
    if (start->empty()) continue;
    t0 = now();
    long acc;
    int passes;
    auto o = improve(*start, acc, passes);
    char nm[96];
    snprintf(nm, sizeof nm, "%s", name);
    report(nm, o, now() - t0 + tStart);
    printf("    (%ld moves accepted in %d passes; %ld relabels, %ld candidate positions scored)\n", acc, passes, nRelabel, nEval);
    nRelabel = nEval = 0;
  }
}
