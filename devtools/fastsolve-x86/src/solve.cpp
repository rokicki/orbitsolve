// IDA* with a symmetry-reduced coset PDB, inverse probes, and subgroup tricks.
#include "fastidx.h"
#include "lattice.h"
#include "fulltab.h"
#include "perf.h"
#include <random>
#include <fstream>
#include <unistd.h>
#include <sys/stat.h>
#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif
#if defined(__aarch64__)
#define FS_CXXFLAGS " -std=c++20 -O3 -mcpu=native"
#else
#define FS_CXXFLAGS " -std=c++20 -O3 -march=native -I" FS_SIMDE " -Wno-unknown-pragmas"
#endif
static char **gArgv;
static bool gTiming = false; static const char *gTws = "";   // -T: microbenchmarks (as twsearch -T)
static string cacheDir() {
  const char *h = getenv("HOME");
#if defined(__APPLE__)
  string d = string(h ? h : "/tmp") + "/Library/Caches/fastsolve";
#else
  string d = string(h ? h : "/tmp") + "/.cache/fastsolve";
#endif
  mkdir(d.c_str(), 0755);
  return d;
}
static uint64_t fnv(const string &s) { uint64_t h = 1469598103934665603ULL; for (unsigned char c : s) { h ^= c; h *= 1099511628211ULL; } return h; }
static string slurp(const char *fn) { ifstream f(fn, ios::binary); return string((istreambuf_iterator<char>(f)), istreambuf_iterator<char>()); }

template <int NCH> struct alignas(64) Solver {   // per-worker (ws vector): own cache lines
  const Puzzle &pz; const Layout &L; const CosetPDB<NCH> &P;
  vector<KElem> km;
  vector<int> inH, base, invMove;
  vector<vector<char>> commute; // base x base
  St<NCH> ident, identColor;
  alignas(64) uint8_t colorTab[KMAX];
  inline St<NCH> colors(const St<NCH> &x) const {
    St<NCH> r; const uint8x16_t PM = vdupq_n_u8(L.posmask), OM = vdupq_n_u8(L.orimask);
    for (int i = 0; i < NCH; i++) r.c[i] = vorrq_u8(tblK<(NCH > 8 ? 9 : NCH > 4 ? 8 : 4)>(colorTab, vandq_u8(x.c[i], PM)), vandq_u8(x.c[i], OM));
    return r;
  }
  inline bool solved(const St<NCH> &x) const { return eqSt<NCH>(colors(x), identColor); }
  const FastIndex<NCH, Shape1> *FI = nullptr;
  const CosetPDB<NCH> *P2 = nullptr; const FastIndex<NCH, Shape2> *FI2 = nullptr;
  long long probes2 = 0; int secDelta = getenv("SEC_DELTA") ? atoi(getenv("SEC_DELTA")) : 99;
  inline int h2(const St<NCH> &x, int v, int lim) {   // refine v with the secondary table when it doesn't prune
    if (!P2 || v > lim || v < lim - secDelta) return v;
    probes2++;
    return max(v, P2->get(FI2->index(x)));
  }
  bool twoBit = false;
  // main-table value with fallback lb (used when the 2-bit code says "<= base")
  // extra lookups of the main table on conjugated states (other slices' orientation)
  vector<KSym> axes; long long probesAx = 0; bool eagerAx = getenv("EAGERAX") != nullptr;
  inline int hAx(const St<NCH> &x, int v, int lim) {
    for (auto &ax : axes) {
      if (v > lim) return v;
      probesAx++;
      uint64_t i = FI->index(conj<NCH>(L, x, ax));
      int w;
      if (!twoBit) w = P.get(i); else { int c = P.get2(i); w = c ? P.base2 + c : 0; }
      v = max(v, w);
    }
    return v;
  }
  inline int mval(uint64_t idx, const St<NCH> &x, int lim, int lb) {
    if (!twoBit) return hAx(x, h2(x, P.get(idx), lim), lim);
    int c = P.get2(idx);
    if (c) return hAx(x, h2(x, P.base2 + c, lim), lim);
    if (!P2) return hAx(x, lb, lim);
    probes2++;
    return hAx(x, max(lb, P2->get(FI2->index(x))), lim);
  }
  inline void pf(uint64_t idx) const { if (twoBit) __builtin_prefetch(&P.tab2[idx >> 2]); else __builtin_prefetch(&P.tab[idx >> 1]); }
  long long nodes = 0, probes = 0, invProbes = 0;
  bool useInv = true, useHtrick = true;
  vector<int> path;
  Solver(const Puzzle &p, const Layout &l, const CosetPDB<NCH> &pdb, const vector<int> &H) : pz(p), L(l), P(pdb) {
    for (auto &m : pz.moves) km.push_back(L.prep(pz, m.e));
    inH.assign(pz.moves.size(), 0); for (int h : H) inH[h] = 1;
    for (auto &m : pz.moves) base.push_back(m.base);
    for (size_t i = 0; i < pz.moves.size(); i++) {
      Elem iv = pz.inv(pz.moves[i].e);
      for (size_t j = 0; j < pz.moves.size(); j++) if (pz.eq(iv, pz.moves[j].e)) invMove.push_back(j);
    }
    int nb = pz.base.size();
    commute.assign(nb, vector<char>(nb, 0));
    for (int a = 0; a < nb; a++) for (int b = 0; b < nb; b++)
      commute[a][b] = pz.eq(pz.mul(pz.base[a].e, pz.base[b].e), pz.mul(pz.base[b].e, pz.base[a].e));
    uint8_t b[KMAX]; L.pack(pz.ident(), b); ident = load<NCH>(b);
    for (int q = 0; q < KMAX; q++) colorTab[q] = q < pz.N ? (pz.orbitOf[q] * 64 + pz.solvedLabel[q]) & L.posmask : 0;
    // colors must stay distinct across orbits: use a dense code
    { map<pair<int,int>, int> code; for (int q = 0; q < pz.N; q++) { auto k = make_pair(pz.orbitOf[q], (int)pz.solvedLabel[q]); if (!code.count(k)) { int c = code.size(); code[k] = c; } colorTab[q] = code[k]; } }
    identColor = colors(ident);
    allowed.assign(nb + 1, {0, 0});
    for (int pv = -1; pv < nb; pv++) for (int sk = 0; sk < 2; sk++) {
      int c = 0;
      for (size_t mi = 0; mi < km.size(); mi++) {
        int bb = base[mi];
        if (pv >= 0 && (bb == pv || (commute[pv][bb] && bb < pv))) continue;
        if (sk && inH[mi]) continue;
        c++;
      }
      allowed[pv + 1][sk] = c;
    }
  }
  vector<array<int, 2>> allowed;
  inline int h(const St<NCH> &x) { probes++; return mval(FI ? FI->index(x) : P.index(x), x, 99, 0); }
  long long lastStart = 0;
  bool useNiss = true; int nissStart = 0, curDepth = 0;
  vector<int> pathR;   // moves taken on the inverse side (R's last moves, in order taken)
  // x * R = id.  hf = h(x), hi = h(x^-1).  Normal side picks R's first move,
  // inverse side picks R's last move.
  // move bitmasks: canonical-after[base+1], H moves
  vector<uint64_t> canon; uint64_t Hmask = 0;
  void buildLists() {
    int nb = pz.base.size();
    canon.assign(nb + 1, 0);
    for (int pv = -1; pv < nb; pv++)
      for (size_t mi = 0; mi < km.size(); mi++) {
        int bb = base[mi];
        if (pv >= 0 && (bb == pv || (commute[pv][bb] && bb < pv))) continue;
        canon[pv + 1] |= 1ULL << mi;
      }
    for (size_t mi = 0; mi < km.size(); mi++) if (inH[mi]) Hmask |= 1ULL << mi;
    if (!useHtrick || !useInv) Hmask = 0;
  }
  // A, B with x*R = id: side 0 => A = x, B = x^-1.  mA/mB: allowed moves on each side
  // (canonical after that side's last move, minus H when R's corresponding end is known
  // not to be in H).  Moves apply on the right of A and as inverse premoves on B.
  bool dfs(const St<NCH> &A0, const St<NCH> &B0, int hA0, int hB0, int togo, uint64_t mA0, uint64_t mB0, int side0) {
    if (stopFlag && stopFlag->load(memory_order_relaxed)) return false;
    if (togo == 1) {
      // last move: must be allowed from both sides; no probes needed
      for (uint64_t m = mA0 & mB0; m; m &= m - 1) {
        int mi = __builtin_ctzll(m);
        nodes++;
        if (solved(mulc<NCH>(L, A0, km[mi]))) { (side0 ? pathR : path).push_back(mi); return true; }
      }
      return false;
    }
    const St<NCH> *A = &A0, *B = &B0; int hA = hA0, hB = hB0, side = side0; uint64_t mA = mA0, mB = mB0;
    if (useNiss && useInv && curDepth - togo >= nissStart && __builtin_popcountll(mB) < __builtin_popcountll(mA)) {
      swap(A, B); swap(hA, hB); swap(mA, mB); side ^= 1;
    }
    if (hB > togo - 1) mA &= ~Hmask;   // inherited bound kills H children outright
    int n = 0, ml[64];
    St<NCH> Y[64]; uint64_t I[64], IX[64][2];
    int nax = eagerAx ? min<int>(axes.size(), 2) : 0;
    for (uint64_t m = mA; m; m &= m - 1) {
      int mi = __builtin_ctzll(m);
      ml[n] = mi;
      Y[n] = mulc<NCH>(L, *A, km[mi]);
      I[n] = FI->index(Y[n]);
      pf(I[n]);
      for (int a = 0; a < nax; a++) { IX[n][a] = FI->index(conj<NCH>(L, Y[n], axes[a])); pf(IX[n][a]); }
      n++;
    }
    probesAx += (long long)n * nax;
    nodes += n; probes += n;
    vector<int> &pth = side ? pathR : path;
    for (int k = 0; k < n; k++) {
      int nA;
      if (nax) {
        if (!twoBit) {
          nA = h2(Y[k], P.get(I[k]), togo - 1);
          for (int a = 0; a < nax; a++) nA = max(nA, P.get(IX[k][a]));
        } else {
          auto dv = [&](uint64_t i) { int c = P.get2(i); return c ? P.base2 + c : 0; };
          nA = max(dv(I[k]), hA - 1);
          for (int a = 0; a < nax; a++) nA = max(nA, dv(IX[k][a]));
          nA = h2(Y[k], nA, togo - 1);
        }
      } else nA = mval(I[k], Y[k], togo - 1, hA - 1);
      if (nA > togo - 1) continue;
      int mi = ml[k];
      St<NCH> YB = premulc<NCH>(L, km[invMove[mi]].packed, *B);
      int nB = hB;
      if (useInv && !inH[mi]) { probes++; nB = mval(FI->index(YB), YB, togo - 1, 0); invProbes++; if (nB > togo - 1) continue; }
      uint64_t nmA = canon[base[mi] + 1], nmB = mB;
      if (nB == togo - 1) nmA &= ~Hmask;
      if (nA == togo - 1) nmB &= ~Hmask;
      pth.push_back(mi);
      if (dfs(Y[k], YB, nA, nB, togo - 1, nmA, nmB, side)) return true;
      pth.pop_back();
    }
    return false;
  }
  // full solution: normal moves, then inverses of inverse-side moves in reverse
  vector<int> solution() const {
    vector<int> r = path;
    for (int i = (int)pathR.size() - 1; i >= 0; i--) r.push_back(invMove[pathR[i]]);
    return r;
  }
  struct Task { int mv[8]; int len; St<NCH> A, B; int hA, hB, maxpre; long long rank; };
  int taskDepth = 4;
  int solve(const St<NCH> &x0, const St<NCH> &xi0, int maxd = 30) {
    int h0 = h(x0), hi0 = h(xi0);
    // first-move-prefix tasks (normal side, canonical), sorted by last iteration's work
    vector<Task> tasks;
    {
      Task t; t.len = 0; t.A = x0; t.B = xi0; t.hA = h0; t.hB = hi0; t.maxpre = max(h0, hi0); t.rank = 0;
      function<void(Task &, int)> gen = [&](Task &t, int prevb) {
        if (t.len == taskDepth) { tasks.push_back(t); return; }
        for (uint64_t m = canon[prevb + 1]; m; m &= m - 1) {
          int mi = __builtin_ctzll(m);
          Task u = t; u.mv[u.len++] = mi;
          u.A = mulc<NCH>(L, t.A, km[mi]); u.B = premulc<NCH>(L, km[invMove[mi]].packed, t.B);
          u.hA = h(u.A); u.hB = h(u.B);
          u.maxpre = max(t.maxpre, u.len + max(u.hA, u.hB));
          gen(u, base[mi]);
        }
      };
      gen(t, -1);
    }
    for (int d = max(h0, hi0); d <= maxd; d++) {
      path.clear(); pathR.clear();
      curDepth = d; lastStart = nodes;
      if (d <= taskDepth + 1) {
        if (d == 0) { if (solved(x0)) return 0; continue; }
        uint64_t mA = canon[0], mB = canon[0];
        if (hi0 == d) mA &= ~Hmask;
        if (h0 == d) mB &= ~Hmask;
        if (dfs(x0, xi0, h0, hi0, d, mA, mB, 0)) return d;
        continue;
      }
      if (nthr > 1) {
        // parallel over tasks in rank order; first solution stops everyone
        atomic<int> next{0}; atomic<bool> stop{false}; mutex mu; bool found = false;
        vector<Solver> ws(nthr, *this);
        vector<thread> th;
        for (int w = 0; w < nthr; w++) th.emplace_back([&, w] {
          Solver &S = ws[w];
          S.nthr = 1; S.stopFlag = &stop; S.nodes = S.probes = S.invProbes = S.probes2 = S.probesAx = 0;
          while (!stop.load(memory_order_relaxed)) {
            int i = next++; if (i >= (int)tasks.size()) break;
            Task &tk = tasks[i];
            long long n0 = S.nodes; S.nodes++;
            if (tk.maxpre <= d) {
              int togo = d - tk.len;
              uint64_t mA = canon[base[tk.mv[tk.len - 1]] + 1], mB = canon[0];
              if (tk.hB == togo) mA &= ~Hmask;
              if (tk.hA == togo) mB &= ~Hmask;
              S.path.assign(tk.mv, tk.mv + tk.len); S.pathR.clear();
              if (S.dfs(tk.A, tk.B, tk.hA, tk.hB, togo, mA, mB, 0)) {
                lock_guard<mutex> g(mu);
                if (!found) { found = true; path = S.path; pathR = S.pathR; }
                stop = true;
              }
            }
            tk.rank = (S.nodes - n0) * 1000 / __builtin_popcountll(canon[base[tk.mv[tk.len - 1]] + 1]);
          }
        });
        for (auto &x : th) x.join();
        for (auto &S : ws) { nodes += S.nodes; probes += S.probes; invProbes += S.invProbes; probes2 += S.probes2; probesAx += S.probesAx; }
        if (found) return d;
      } else
      for (auto &t : tasks) {
        long long n0 = nodes;
        nodes++;
        if (t.maxpre <= d) {
          int togo = d - t.len;
          uint64_t mA = canon[base[t.mv[t.len - 1]] + 1], mB = canon[0];
          if (t.hB == togo) mA &= ~Hmask;
          if (t.hA == togo) mB &= ~Hmask;
          path.assign(t.mv, t.mv + t.len); pathR.clear();
          if (dfs(t.A, t.B, t.hA, t.hB, togo, mA, mB, 0)) return d;
        }
        long long w = nodes - n0;
        t.rank = w * 1000 / __builtin_popcountll(canon[base[t.mv[t.len - 1]] + 1]);  // normalize for canonical branching
      }
      if (sortTasks) stable_sort(tasks.begin(), tasks.end(), [](const Task &a, const Task &b) { return a.rank > b.rank; });
    }
    return -1;
  }
  bool sortTasks = true;
  int nthr = 1; atomic<bool> *stopFlag = nullptr;
};

// move token = base name (longest matching prefix) + optional amount + optional '
static int parseMove(const Puzzle &pz, const string &t) {
  int bestLen = -1, bestBase = -1;
  for (size_t b = 0; b < pz.base.size(); b++) {
    const string &nm = pz.base[b].name;
    if (t.compare(0, nm.size(), nm) == 0 && (int)nm.size() > bestLen) {
      string suf = t.substr(nm.size());
      bool ok = true; for (size_t i = 0; i < suf.size(); i++) if (!(isdigit(suf[i]) || (suf[i] == '\'' && i + 1 == suf.size()))) ok = false;
      if (ok) { bestLen = nm.size(); bestBase = b; }
    }
  }
  if (bestBase < 0) return -1;
  string suf = t.substr(bestLen);
  bool prime = !suf.empty() && suf.back() == '\''; if (prime) suf.pop_back();
  int tw = suf.empty() ? 1 : atoi(suf.c_str());
  for (size_t m = 0; m < pz.moves.size(); m++) {
    auto &mv = pz.moves[m];
    if (mv.base != bestBase) continue;
    int want = prime ? ((mv.order - tw) % mv.order + mv.order) % mv.order : tw % mv.order;
    if (mv.twist == want) return m;
  }
  return -1;
}
static vector<int> parseAlg(const Puzzle &pz, const string &line) {
  vector<int> r; istringstream is(line); string t;
  while (is >> t) { int m = parseMove(pz, t); if (m < 0) { fprintf(stderr, "bad move %s\n", t.c_str()); exit(1); } r.push_back(m); }
  return r;
}

template <int NCH, class FIX> static void loadOrBuild(CosetPDB<NCH> &P, const FIX &FI, const string &tag, int nthreads) {
  string tf = cacheDir() + "/pdb-" + to_string(fnv(slurp(gTws) + tag + P.A.desc + to_string(P.size)) & 0xffffffffffULL) + ".dat";
  FILE *f = getenv("NOCACHE") ? nullptr : fopen(tf.c_str(), "rb");
  if (f) {
    P.tab.resize(((P.size + 7) / 8) * 4);
    size_t got = fread(P.tab.data(), 1, P.tab.size(), f); fclose(f);
    if (got != P.tab.size()) { fprintf(stderr, "bad table cache %s\n", tf.c_str()); exit(1); }
    printf("table %s: %llu entries loaded\n", tag.c_str(), (unsigned long long)P.size);
    if (getenv("FILLVERBOSE")) printf("  checksum %016llx\n", (unsigned long long)fnv(string(P.tab.begin(), P.tab.end())));
  } else {
    PerfSnap s = perfNow();
    P.fill(nthreads, getenv("FILLVERBOSE") != nullptr, &FI);
    P.tab.resize(((P.size + 7) / 8) * 4);
    PerfDelta fd = perfSince(s);
    printf("table %s: %llu entries (%llu classes, %llu twin) built in %.1fs, %.0f instr/entry\n", tag.c_str(), (unsigned long long)P.size,
           (unsigned long long)P.nclass, (unsigned long long)P.twinClasses, fd.sec, fd.ins / P.size);
    if (getenv("FILLVERBOSE")) printf("  checksum %016llx\n", (unsigned long long)fnv(string(P.tab.begin(), P.tab.end())));
    if (!getenv("NOCACHE") && (f = fopen(tf.c_str(), "wb"))) { fwrite(P.tab.data(), 1, P.tab.size(), f); fclose(f); }
  }
}
template <int NCH> int go(Puzzle &pz0, const char *scrfile, int nscr, int nthreads, int flags) {
  auto syms0 = findSymsCached(pz0, cacheDir() + "/syms-" + to_string(fnv(slurp(gTws)) & 0xffffffffffULL) + ".txt");
  CosetFinder cf(pz0, syms0);
  vector<int> H; for (size_t i = 0; i < pz0.moves.size(); i++) if (2 * pz0.moves[i].twist == pz0.moves[i].order) H.push_back(i);
  bool latticeMode = H.empty() || (flags & 16384);
  Abstraction A; int latticeKey = -1;
  if (!latticeMode) A = cf.derive(H, false);
  else {
    H.clear();
    double budget = getenv("BUDGET") ? atof(getenv("BUDGET")) : 2e8;
    LatticeChoice lc = chooseAbstraction(pz0, syms0, budget, false);
    if (lc.keyOrbit < 0) { fprintf(stderr, "no abstraction fits\n"); return 1; }
    printf("lattice abstraction: %s raw %.4g, |S|=%zu, reduced %.4g\n", lc.A.desc.c_str(), exp(lc.lgSize), lc.stab.size(), lc.reduced);
    A = lc.A; latticeKey = lc.keyOrbit;
    vector<Elem> ss; for (int s : lc.stab) ss.push_back(syms0[s]);
    syms0 = ss;
  }
  Abstraction A2 = A;   // secondary: orbits whose orientation the main table drops, labels+ori only
  vector<int> split2;
  for (size_t k = 0; k < pz0.orbits.size(); k++) if (pz0.orbits[k].omod == 2 && A.keepOri[k]) { A.keepOri[k] = false; split2.push_back(k); } // h0
  bool use2 = !split2.empty() && !(flags & 4096);
  for (size_t k = 0; k < pz0.orbits.size(); k++) if (find(split2.begin(), split2.end(), (int)k) == split2.end()) A2.ignore[k] = true;
  // EOSLICE=m: main table also keeps orientation of the pieces of the label classes in
  // bitmask m of the split orbit (e.g. one edge slice); symmetry drops to the stabilizer.
  vector<Elem> syms0main = syms0;
  if (getenv("EOSLICE") && !split2.empty()) {
    uint32_t m = strtoul(getenv("EOSLICE"), nullptr, 0);
    A.oriLabMask.assign(pz0.orbits.size(), 0);
    A.oriLabMask[split2[0]] = m;
    vector<int> glab(pz0.N), flag(pz0.orbits.size(), 1);
    for (int j = 0; j < pz0.N; j++) {
      int k = pz0.orbitOf[j];
      glab[j] = 1000 * k + A.label[j] + ((k == split2[0] && ((m >> A.label[j]) & 1)) ? 500 : 0);
    }
    auto st = stabilizer(pz0, syms0, glab, flag);
    syms0main.clear();
    for (int s : st) {   // the marked classes must map to themselves, not merely to some class
      bool ok = true;
      for (int j = 0; j < pz0.N && ok; j++) ok = (glab[j] % 1000 >= 500) == (glab[syms0[s].p[j]] % 1000 >= 500);
      if (ok) syms0main.push_back(syms0[s]);
    }
    printf("EOSLICE mask %u: %zu symmetries\n", m, syms0main.size());
    A.desc += " eoslice" + to_string(m);
  }
  Puzzle pz = regaugePuzzle(pz0, A.gauge);
  vector<Elem> syms; for (auto &s : syms0main) syms.push_back(regauge(pz0, s, A.gauge));
  vector<Elem> symsAll; for (auto &s : syms0) symsAll.push_back(regauge(pz0, s, A.gauge));
  Layout L(pz);
  vector<int> keyOrb; for (size_t k = 0; k < pz.orbits.size(); k++) if (A.keepOri[k]) keyOrb.push_back(k);
  if (latticeMode) keyOrb = {latticeKey};
  // inverse probes are valid only if every orbit the tables look at has distinct pieces
  bool invOk = true;
  for (size_t k = 0; k < pz.orbits.size(); k++) {
    if (A.ignore[k] && (!use2 || A2.ignore[k])) continue;
    auto &ob = pz.orbits[k]; set<int> seen;
    for (int j = ob.off; j < ob.off + ob.n; j++) if (!seen.insert(pz.solvedLabel[j]).second) invOk = false;
  }
  if (!invOk) printf("identical pieces in abstraction: inverse probes disabled\n");
  CosetPDB<NCH> P(pz, L, A, syms, keyOrb);
  // shape-specialized binary: compile once into the cache dir, then re-exec
  {
    auto shapeOf = [](const CosetPDB<NCH> &Q) -> string {
      if (Q.keyP.size() != 1 || Q.restP.size() > 3 || Q.restP.empty() || Q.keyP[0].n > 16) return "";
      auto &k = Q.keyP[0];
      string d = to_string(k.n) + "," + to_string(k.nl) + "," + to_string((int)k.ori) + "," + to_string(Q.restP.size());
      for (size_t i = 0; i < 3; i++) {
        if (i < Q.restP.size()) {
          auto &r = Q.restP[i];
          if (r.n > 16 || (r.ori && r.mode != 2)) return "";
          d += "," + to_string(r.n) + "," + to_string(r.nl) + "," + to_string(r.pmask ? 3 : r.mode);
        } else d += ",0,0,0";
      }
      return d;
    };
    string defs, s1 = shapeOf(P);
    if (!s1.empty()) defs += " -DSPEC1=" + s1;
    if (use2) { CosetPDB<NCH> Q(pz, L, A2, symsAll, {}, split2); string s2 = shapeOf(Q); if (!s2.empty()) defs += " -DSPEC2=" + s2; }
#ifndef SPEC_BUILT
    if (!defs.empty() && !(flags & 8)) {
      string src = string(__FILE__); string dir = src.substr(0, src.rfind('/') + 1);
      if (src.find('/') == string::npos) { char cwd[4096]; getcwd(cwd, sizeof cwd); dir = string(cwd) + "/"; src = dir + src; }
      string bin = cacheDir() + "/solve-" + to_string(fnv(defs + slurp(src.c_str()) + slurp((dir + "fastidx.h").c_str()) + slurp((dir + "pdb.h").c_str()) + slurp((dir + "kern.h").c_str())) & 0xffffffffffULL);
      struct stat st;
      if (stat(bin.c_str(), &st) != 0) {
        string cmd = string(getenv("CXX") ? getenv("CXX") : "c++") + FS_CXXFLAGS + " -DSPEC_BUILT " + defs + " -I" + dir + " -o " + bin + " " + src;
        printf("specializing: %s\n", defs.c_str()); fflush(stdout);
        if (system(cmd.c_str()) != 0) { fprintf(stderr, "spec compile failed; continuing generic\n"); goto generic; }
      }
      fflush(stdout);
      execv(bin.c_str(), gArgv);
      perror("execv");
    }
  generic:;
#endif
  }
  P.buildRep();
  FastIndex<NCH, Shape1> FI(P);
  loadOrBuild(P, FI, "main", nthreads);
  vector<KSym> axes;
  if (getenv("EOSLICE") && getenv("AXES") && !split2.empty()) {
    int k = split2[0]; auto &ob = pz.orbits[k];
    uint32_t m = A.oriLabMask[k];
    uint8_t b[KMAX]; L.pack(pz.ident(), b); St<NCH> id = load<NCH>(b);
    for (int c = 0; c < A.nlabels[k]; c++) {
      if ((m >> c) & 1) continue;
      Elem e = pz.ident(); for (int j = ob.off; j < ob.off + ob.n; j++) if (A.label[j] == c) e.o[j] = 1;
      L.pack(e, b); St<NCH> x = load<NCH>(b);
      for (auto &S : symsAll) {
        KSym ks = prepSym(pz, L, S);
        uint8_t y[KMAX], y0[KMAX]; store<NCH>(conj<NCH>(L, x, ks), y); store<NCH>(conj<NCH>(L, id, ks), y0);
        bool ok = true;
        for (int j = ob.off; j < ob.off + ob.n; j++) {
          bool flipped = (y[j] >> L.p) != (y0[j] >> L.p);
          int lab = A.label[y[j] & L.posmask];
          if (flipped != (bool)((m >> lab) & 1)) ok = false;
        }
        if (ok) { axes.push_back(ks); break; }
      }
    }
    printf("axis lookups: %zu extra conjugations\n", axes.size());
  }
  CosetPDB<NCH> P2(pz, L, A2, symsAll, {}, split2);
  if (use2) P2.buildRep();
  FastIndex<NCH, Shape2> FI2(use2 ? P2 : P);
  if (use2) {
    loadOrBuild(P2, FI2, "secondary", nthreads);
    // verify
    vector<KElem> km; for (auto &m : pz.moves) km.push_back(L.prep(pz, m.e));
    uint8_t b[KMAX]; L.pack(pz.ident(), b); St<NCH> x = load<NCH>(b); mt19937 rng(2); long bad = 0;
    for (int i = 0; i < 100000; i++) { x = mulc<NCH>(L, x, km[rng() % km.size()]); if (FI2.index(x) != P2.index(x)) bad++; }
    printf("secondary fastidx check: %ld mismatches\n", bad);
    vector<St<NCH>> xs; for (int i = 0; i < 100000; i++) { x = mulc<NCH>(L, x, km[rng() % km.size()]); xs.push_back(x); }
    uint64_t acc = 0; PerfSnap t1 = perfNow(); for (auto &y : xs) acc += FI2.index(y); PerfDelta d1 = perfSince(t1);
    printf("secondary index instr %.0f (%llu)\n", d1.ins / xs.size(), (unsigned long long)(acc & 1));
    if (bad) return 1;
  }
  if (flags & 8192) {
    // auto base: smallest b with at most 10% of entries saturating (value > b+3)
    vector<uint64_t> hist(16, 0); for (uint64_t i = 0; i < P.size; i++) hist[P.get(i)]++;
    int b = 0; for (; b < 12; b++) { uint64_t above = 0; for (int v = b + 4; v < 15; v++) above += hist[v]; if (above <= P.size / 10) break; }
    if (getenv("BASE2")) b = atoi(getenv("BASE2"));
    P.build2bit(b); vector<uint8_t>().swap(P.tab);   // keep only the 2-bit table
    printf("2-bit main table: base %d, %.1f MB\n", b, P.tab2.size() / 1e6);
  }
  PerfSnap s;

  {
    // verify fast index on random walks; also time both
    vector<KElem> km; for (auto &m : pz.moves) km.push_back(L.prep(pz, m.e));
    uint8_t b[KMAX]; L.pack(pz.ident(), b); St<NCH> x = load<NCH>(b);
    mt19937 rng(1); long bad = 0; uint64_t acc = 0;
    vector<St<NCH>> xs;
    for (int i = 0; i < 200000; i++) { x = mulc<NCH>(L, x, km[rng() % km.size()]); xs.push_back(x); }
    for (auto &y : xs) if (FI.index(y) != P.index(y)) bad++;
    printf("fastidx check: %ld mismatches of %zu\n", bad, xs.size());
    PerfSnap t0 = perfNow(); for (auto &y : xs) acc += P.index(y); PerfDelta d0 = perfSince(t0);
    PerfSnap t1 = perfNow(); for (auto &y : xs) acc += FI.index(y); PerfDelta d1 = perfSince(t1);
    printf("index instr: generic %.0f  fast %.0f  (%llu)\n", d0.ins / xs.size(), d1.ins / xs.size(), (unsigned long long)(acc & 1));
    if (bad) return 1;
  }
  // read scrambles
  ifstream f(scrfile); string line; vector<vector<int>> scr;
  if (!strncmp(scrfile, "rand", 4)) {   // randN: random scrambles of N moves (no immediate same-face repeats)
    int len = atoi(scrfile + 4); mt19937 rng(12345);
    for (int i = 0; i < nscr; i++) {
      vector<int> s; int pb = -1;
      while ((int)s.size() < len) { int m = rng() % pz.moves.size(); if (pz.moves[m].base == pb) continue; pb = pz.moves[m].base; s.push_back(m); }
      scr.push_back(s);
    }
  } else
  while (getline(f, line) && (int)scr.size() < nscr) if (line.size() > 2) scr.push_back(parseAlg(pz, line));
  vector<int> lens(scr.size()); vector<long long> px(scr.size()), p2(scr.size()), fin(scr.size()), nn(scr.size()), pp(scr.size()), ii(scr.size());
  atomic<int> next{0};
  s = perfNow();
  vector<thread> th;
  int outer = getenv("OUTER") ? nthreads : 1, inner = getenv("OUTER") ? 1 : nthreads;
  for (int t = 0; t < outer; t++) th.emplace_back([&] {
    Solver<NCH> S(pz, L, P, H);
    S.FI = &FI; S.buildLists();
    if (use2) { S.P2 = &P2; S.FI2 = &FI2; }
    S.axes = axes;
    S.twoBit = flags & 8192; S.nthr = inner;
    S.useInv = !(flags & 1) && invOk; S.useHtrick = !(flags & 2); S.useNiss = !(flags & 4); S.nissStart = (flags >> 4) & 15; S.sortTasks = !(flags & 2048);
    while (true) {
      int i = next++; if (i >= (int)scr.size()) break;
      St<NCH> x = S.ident, xi = S.ident;
      for (int m : scr[i]) { x = mulc<NCH>(L, x, S.km[m]); xi = premulc<NCH>(L, S.km[S.invMove[m]].packed, xi); }
      S.nodes = S.probes = S.invProbes = S.probes2 = S.probesAx = 0;
      lens[i] = S.solve(x, xi);
      // verify solution
      St<NCH> y = x; for (int m : S.solution()) y = mulc<NCH>(L, y, S.km[m]);
      if (!S.solved(y)) printf("!! bad solution for %d\n", i);
      p2[i] = S.probes2; px[i] = S.probesAx; nn[i] = S.nodes; fin[i] = S.nodes - S.lastStart; pp[i] = S.probes; ii[i] = S.invProbes;
    }
  });
  for (auto &x : th) x.join();
  PerfDelta d = perfSince(s);
  double an = 0, ap = 0, ai = 0, al = 0, af = 0;
  double a2 = 0;
  for (size_t i = 0; i < scr.size(); i++) { af += fin[i]; a2 += p2[i]; }
  for (size_t i = 0; i < scr.size(); i++) { an += nn[i]; ap += pp[i]; ai += ii[i]; al += lens[i]; }
  int n = scr.size();
  if (getenv("SHOWLENS")) { printf("lengths:"); for (int i = 0; i < n; i++) printf(" %d", lens[i]); printf("\n"); }
  { double ax = 0, tn = 0; for (size_t i = 0; i < px.size(); i++) { ax += px[i]; tn += nn[i]; } printf("axis probes/node %.3f\n", ax / max(1.0, tn)); }
  printf("secondary probes/node %.3f\n", a2 / max(1.0, [&] { double t = 0; for (auto v : nn) t += v; return t; }()));
  printf("final-iteration nodes avg %.4gM (complete iterations %.4gM)\n", af / n / 1e6, (an - af) / n / 1e6);
  printf("%d positions: avg len %.2f  avg nodes %.4gM  probes/node %.3f  inv/node %.3f  instr/node %.0f  (%.1fs)\n",
         n, al / n, an / n / 1e6, ap / an, ai / an, d.ins / an, d.sec);
  return 0;
}
// ---------------------------------------------------------------------------
// Full-state table path: colour states, single-sided IDA* (no inverses needed, so
// it also serves puzzles with identical pieces, which are not groups).  Input lines
// are generating sequences; output follows twsearch -s/-si (Solving / solution /
// Found ... / No solution found in d).
template <int NCH> struct alignas(64) FullSolver {   // per-worker search state and counters: own cache lines
  const Puzzle &pz; const Layout &L; const ColorSpace<NCH> &C; const FullTab<NCH> &T;
  vector<KElem> km; vector<int> base; vector<uint64_t> canon;
  long long nodes = 0, probes = 0;
  vector<int> path; St<NCH> anc[64];
  FullSolver(const Puzzle &p, const Layout &l, const ColorSpace<NCH> &c, const FullTab<NCH> &t, const vector<uint64_t> &cm)
      : pz(p), L(l), C(c), T(t), canon(cm) {
    for (auto &m : pz.moves) { km.push_back(L.prep(pz, m.e)); base.push_back(m.base); }
  }
  bool dfs(const St<NCH> &A, int hA, int togo, uint64_t mA, int depth) {
    anc[depth] = A;
    if (togo == 1) {
      for (uint64_t m = mA; m; m &= m - 1) {
        int mi = __builtin_ctzll(m); nodes++;
        if (eqSt<NCH>(mulc<NCH>(L, A, km[mi]), C.solved)) { path.push_back(mi); return true; }
      }
      return false;
    }
    int n = 0, ml[64]; St<NCH> Y[64]; uint64_t I[64];
    for (uint64_t m = mA; m; m &= m - 1) {
      int mi = __builtin_ctzll(m);
      Y[n] = mulc<NCH>(L, A, km[mi]);
      // exact transposition cuts: a child equal to an ancestor cannot start a
      // solution of exactly the iteration length (shorter ones do not exist)
      bool rep = false;
      for (int a = depth; a >= 0 && a >= depth - 2 && !rep; a--) rep = eqSt<NCH>(Y[n], anc[a]);
      if (rep) continue;
      I[n] = T.K.template canon<false>(Y[n]); T.pf(T.slot(I[n])); ml[n] = mi; n++;
    }
    nodes += n; probes += n;
    for (int k = 0; k < n; k++) {
      int v = max(T.bound(I[k]), hA - 1);
      if (v > togo - 1) continue;
      path.push_back(ml[k]);
      if (dfs(Y[k], v, togo - 1, canon[base[ml[k]] + 1], depth + 1)) return true;
      path.pop_back();
    }
    return false;
  }
  int solve(const St<NCH> &x, int maxd) {
    path.clear();
    if (eqSt<NCH>(x, C.solved)) return 0;
    int h0 = max(1, T.bound(T.K.template canon<false>(x)));
    for (int d = h0; d <= maxd; d++) if (dfs(x, h0, d, canon[0], 0)) return d;
    return -1;
  }
};

template <int NCH> int goFull(Puzzle &pz, const char *scrfile, int nscr, int nthreads) {
  Puzzle pzOrig = pz;
  auto dropped = dropRotations(pz);
  if (!dropped.empty()) { printf("rotations (free, used as symmetries):"); for (auto &d : dropped) printf(" %s", d.c_str()); printf("\n"); }
  // symmetries of the full move set (rotation moves only map among themselves, so these
  // also permute the remaining moves; keep those that do)
  auto syms = findSymsCached(pzOrig, cacheDir() + "/syms-" + to_string(fnv(slurp(gTws)) & 0xffffffffffULL) + ".txt");
  vector<Elem> keep;
  for (auto &S : syms) {
    if (!symPreservesSolved(pz, S)) continue;
    Elem Si = pz.inv(S); bool ok = true;
    for (auto &bm : pz.base) { Elem c = pz.mul(pz.mul(Si, bm.e), S); bool f = false; for (auto &mv : pz.moves) if (pz.eq(c, mv.e)) { f = true; break; } if (!f) { ok = false; break; } }
    if (ok) keep.push_back(S);
  }
  printf("%zu symmetries preserve the solved colouring\n", keep.size());
  Layout L(pz);
  if (!L.ok) { fprintf(stderr, "layout not supported\n"); return 1; }
  ColorSpace<NCH> C(pz, L);
  Canon<NCH> K(C, L, pz, keep);
  // canonical-sequence masks (commuting bases in increasing order)
  int nb = pz.base.size();
  vector<uint64_t> canon(nb + 1, 0);
  for (int pv = -1; pv < nb; pv++) for (size_t mi = 0; mi < pz.moves.size(); mi++) {
    int bb = pz.moves[mi].base;
    if (pv >= 0 && (bb == pv || (bb < pv && pz.eq(pz.mul(pz.base[pv].e, pz.base[bb].e), pz.mul(pz.base[bb].e, pz.base[pv].e))))) continue;
    canon[pv + 1] |= 1ULL << mi;
  }
  if (pz.moves.size() > 64) { fprintf(stderr, "too many moves\n"); return 1; }
  FullTab<NCH> T(pz, L, C, K, canon);
  double ram = 0; { size_t sz = sizeof ram; uint64_t mem = 0; size_t ms = sizeof mem; 
#if defined(__APPLE__)
  sysctlbyname("hw.memsize", &mem, &ms, nullptr, 0);
#else
  mem = (uint64_t)sysconf(_SC_PHYS_PAGES) * (uint64_t)sysconf(_SC_PAGESIZE); (void)ms;
#endif
  ram = mem / 1e9; (void)sz; }
  double gb = getenv("FTGB") ? atof(getenv("FTGB")) : 0.5 * ram, lgb = getenv("LAYERGB") ? atof(getenv("LAYERGB")) : 6;
  PerfSnap s0 = perfNow();
  // fingerprint: tws + table-format/hash version (x86 hashing differs by build; bump FS_TABVER when it changes)
  T.fp = to_string(fnv(slurp(gTws)) & 0xffffffffffffULL) + "-" FS_TABVER;
  T.build(gb, lgb, nthreads, true);
  PerfDelta fd = perfSince(s0);
  printf("table built in %.1fs\n", fd.sec); fflush(stdout);
  if (gTiming) {
    // twsearch -T analogue: rate (M/s, one thread) and instructions per operation
    vector<KElem> km; for (auto &m : pz.moves) km.push_back(L.prep(pz, m.e));
    const int NR = 65536; vector<uint16_t> rp(NR); mt19937 rng(1); for (auto &v : rp) v = rng() % km.size();
    const long cnt = getenv("TCOUNT") ? atol(getenv("TCOUNT")) : 20000000;
    auto run = [&](const char *name, auto body) {
      St<NCH> x = C.solved; uint64_t sum = 0;
      PerfSnap a = perfNow();
      for (long i = 0; i < cnt; i++) { x = mulc<NCH>(L, x, km[rp[i & (NR - 1)]]); sum += body(x, i); }
      PerfDelta d = perfSince(a);
      printf("%-34s %8.2f M/s  %6.0f instr  (sum %llu)\n", name, cnt / d.sec / 1e6, d.ins / cnt, (unsigned long long)(sum & 0xffff));
      fflush(stdout);
    };
    run("moves", [&](const St<NCH> &x, long) { return (uint64_t)vgetq_lane_u8(x.c[0], 1); });
    run("moves plus hash", [&](const St<NCH> &x, long) { return aesHash<NCH>(x); });
    run("moves plus masks", [&](const St<NCH> &x, long) { uint32_t mk[64]; K.masks(x, mk); return (uint64_t)mk[0]; });
    run("moves plus feature candidates", [&](const St<NCH> &x, long) { int cd[1024]; return (uint64_t)K.features(x, cd); });
    run("moves plus one symmetry gather (6 chunks)", [&](const St<NCH> &x, long i) { return (uint64_t)vgetq_lane_u8(K.apply(x, 1 + (i & 15)).c[NCH - 1], 3); });
    run("moves plus one symHash", [&](const St<NCH> &x, long i) { return K.symHash(x, 1 + (i & 15)); });
    run("moves plus symmetry (canonical hash)", [&](const St<NCH> &x, long) { return K.template canon<false>(x); });
    run("moves plus lookup", [&](const St<NCH> &x, long) { return (uint64_t)T.code(T.slotOf(x)); });
    for (int look = 2; look <= 16; look *= 2) {
      uint64_t tgo[16] = {0}; int mask = look - 1;
      char nm[64]; snprintf(nm, sizeof nm, "moves plus lookup piped %d", look);
      run(nm, [&](const St<NCH> &x, long i) {
        uint64_t r = T.code(tgo[i & mask]);
        tgo[i & mask] = T.slotOf(x); T.pf(tgo[i & mask]);
        return r;
      });
    }
    double c0 = 0, n0 = 0; { St<NCH> x = C.solved; for (int i = 0; i < 100000; i++) { x = mulc<NCH>(L, x, km[rp[i & (NR - 1)]]); c0 += K.candCount(x); n0++; } }
    printf("canonicalization: %.2f full evaluations per state (random walk)\n", c0 / n0);
    return 0;
  }
  // positions: generating sequences on the original move set (rotations allowed in input)
  vector<KElem> kmo; for (auto &m : pzOrig.moves) kmo.push_back(L.prep(pzOrig, m.e));
  vector<pair<St<NCH>, int>> pos; vector<string> lines;
  {
    uint8_t b[KMAX]; L.pack(pz.ident(), b); St<NCH> id = load<NCH>(b);
    ifstream f(scrfile); string line;
    while (getline(f, line) && (int)pos.size() < nscr) {
      if (line.find_first_not_of(" \t\r") == string::npos) continue;
      auto mv = parseAlg(pzOrig, line);
      St<NCH> x = id; for (int m : mv) x = mulc<NCH>(L, x, kmo[m]);
      pos.push_back({C.of(x), (int)mv.size()}); lines.push_back(line);
    }
  }
  int slack = getenv("SLACK") ? atoi(getenv("SLACK")) : -1;   // search up to len+slack (twsearch -si on a tail: len-1)
  int n = pos.size();
  vector<string> out(n); vector<char> ready(n, 0); vector<long long> nn(n), pp(n); vector<int> lens(n);
  atomic<int> next{0}; mutex mu; int printed = 0;
  PerfSnap s = perfNow(); double t0 = nowSec();
  vector<thread> th;
  for (int t = 0; t < nthreads; t++) th.emplace_back([&] {
    FullSolver<NCH> S(pz, L, C, T, canon);
    while (true) {
      int i = next++; if (i >= n) break;
      double ts = nowSec();
      S.nodes = S.probes = 0;
      int maxd = pos[i].second + slack; if (getenv("MAXD")) maxd = atoi(getenv("MAXD"));
      int d = S.solve(pos[i].first, maxd);
      char buf[256]; string o = "Solving\n";
      if (d >= 0) {
        St<NCH> y = pos[i].first; for (int m : S.path) y = mulc<NCH>(L, y, S.km[m]);
        if (!eqSt<NCH>(y, C.solved)) o += "!! bad solution\n";
        for (int m : S.path) o += " " + pz.moves[m].name;
        snprintf(buf, sizeof buf, "\nFound 1 solution max depth %d lookups %lld in %.4f\n", d, S.probes, nowSec() - ts); o += buf;
      } else { snprintf(buf, sizeof buf, "No solution found in %d\n", maxd); o += buf; }
      lock_guard<mutex> g(mu);
      out[i] = o; ready[i] = 1; nn[i] = S.nodes; pp[i] = S.probes; lens[i] = d;
      while (printed < n && ready[printed]) { fputs(out[printed].c_str(), stdout); printed++; }
      fflush(stdout);
    }
  });
  for (auto &x : th) x.join();
  PerfDelta d = perfSince(s);
  double an = 0, ap = 0; int found = 0; for (int i = 0; i < n; i++) { an += nn[i]; ap += pp[i]; found += lens[i] >= 0; }
  fprintf(stderr, "%d positions, %d solved within limit: avg nodes %.4gM  lookups %.4gM  instr/node %.0f  %.2fs (%.3fs/position wall)\n",
          n, found, an / n / 1e6, ap / n / 1e6, d.ins / max(1.0, an), nowSec() - t0, (nowSec() - t0) / max(1, n));
  return 0;
}

int main(int argc, char **argv) {
  gArgv = argv;
  {   // strip -T (kept in gArgv for re-exec)
    static vector<char *> av; for (int i = 0; i < argc; i++) { if (!strcmp(argv[i], "-T")) gTiming = true; else av.push_back(argv[i]); }
    av.push_back(nullptr); argc = av.size() - 1; argv = av.data(); gTws = argc > 1 ? argv[1] : "";
  }
  if (argc < 3) { fprintf(stderr, "usage: solve puzzle.tws scrambles [n] [threads] [flags: 1 noinv 2 noH 4 noNISS 8 nospec, nissStart<<4]\n"); return 1; }
  Puzzle pz = readTws(argv[1]);
  int nscr = argc > 3 ? atoi(argv[3]) : 10, nt = argc > 4 ? atoi(argv[4]) : 12, flags = argc > 5 ? atoi(argv[5]) : 0;
  Layout L(pz);
  if (L.N > 64 || getenv("FULLTAB")) {
    switch (L.nch) {
      case 1: return goFull<1>(pz, argv[2], nscr, nt);
      case 2: return goFull<2>(pz, argv[2], nscr, nt);
      case 3: return goFull<3>(pz, argv[2], nscr, nt);
      case 4: return goFull<4>(pz, argv[2], nscr, nt);
      case 5: return goFull<5>(pz, argv[2], nscr, nt);
      case 6: return goFull<6>(pz, argv[2], nscr, nt);
      case 7: return goFull<7>(pz, argv[2], nscr, nt);
      case 8: return goFull<8>(pz, argv[2], nscr, nt);
      case 9: return goFull<9>(pz, argv[2], nscr, nt);
    }
  }
  switch (L.nch) {
    case 1: return go<1>(pz, argv[2], nscr, nt, flags);
    case 2: return go<2>(pz, argv[2], nscr, nt, flags);
    case 3: return go<3>(pz, argv[2], nscr, nt, flags);
    case 4: return go<4>(pz, argv[2], nscr, nt, flags);
  }
  fprintf(stderr, "puzzle too large (N=%d)\n", L.N); return 1;
}
