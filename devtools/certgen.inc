// certgen.inc (included by orbitbound.cpp): CERT=prefix with LOADDIST writes a certificate
// for each loaded stage (CERTSTAGE=i: only stage i of --plan) to "<prefix>.<i>.txt": for every
// canonical stage state (least rank among its images under the stage's cube symmetries) a
// complete move sequence that solves the stage.  certcheck verifies the files independently.
//
// Witnesses: in increasing table distance d, each state s gets one action a, the first (actions
// in increasing cost) with cost(a) - saved + L(a(s)) <= d, where L is the length found for
// a(s) and saved counts the moves that cancel where a's last same-axis run meets the first run of
// a(s)'s sequence (moves on one axis commute).  A state with no such action keeps the best one
// found (reported as over the table).  A sequence is then the chain of witness algorithms,
// simplified run by run (same-layer turns of a run combined).

namespace cert {

inline int axisOf(int f) { static const int ax[6] = {0, 1, 2, 1, 2, 0}; return ax[f]; }
inline int slotOf(int f, int d) { return f == 0 || f == 1 || f == 2 ? d - 1 : 6 - d; }  // U L F side 0..2
// a run descriptor: bit 15 set, bits 12..13 the axis, 2 bits of quarter turns per layer slot
inline uint16_t runNew(int axis) { return 0x8000 | axis << 12; }
inline uint16_t runAdd(uint16_t r, const Move &m) {
  const int sl = slotOf(m.face, m.depth), v = ((r >> (2 * sl) & 3) + m.amt) & 3;
  return (r & ~(3 << (2 * sl))) | v << (2 * sl);
}
inline int runNz(uint16_t r) { int n = 0; for (int sl = 0; sl < 6; sl++) n += (r >> (2 * sl) & 3) != 0; return n; }
inline uint16_t runSum(uint16_t a, uint16_t b) {
  uint16_t r = a & 0xF000;
  for (int sl = 0; sl < 6; sl++) r |= (((a >> (2 * sl)) + (b >> (2 * sl))) & 3) << (2 * sl);
  return r;
}
struct Info { uint16_t first, last; uint8_t lastCnt, runs, len; };
inline Info infoOf(const std::vector<Move> &mv) {
  Info in{0, 0, 0, 0, (uint8_t)mv.size()};
  int axis = -1;
  uint16_t cur = 0;
  int cnt = 0;
  for (auto &m : mv) {
    const int ax = axisOf(m.face);
    if (ax != axis) {
      if (axis >= 0) { if (in.runs == 0) in.first = cur; in.runs = std::min(2, in.runs + 1); }
      axis = ax, cur = runNew(ax), cnt = 0;
    }
    cur = runAdd(cur, m), cnt++;
  }
  if (axis >= 0) { if (in.runs == 0) in.first = cur; in.runs = std::min(2, in.runs + 1); in.last = cur, in.lastCnt = cnt; }
  return in;
}
// moves saved where a run ending in `last` (lastCnt moves) meets a sequence starting with run f
inline int saved(const Info &a, uint16_t f) {
  if (!(f & 0x8000) || (f & 0x3000) != (a.last & 0x3000)) return 0;
  return a.lastCnt + runNz(f) - runNz(runSum(a.last, f));
}
// first run of a's moves followed by a sequence starting with run f (0: unknown)
inline uint16_t firstAfter(const Info &a, uint16_t f) {
  if (a.runs >= 2) return a.first;
  if (!(f & 0x8000) || (f & 0x3000) != (a.first & 0x3000)) return a.first;
  const uint16_t s = runSum(a.first, f);
  return runNz(s) ? s : 0;
}
// full simplification: adjacent same-axis runs merged, same-layer turns combined
inline std::vector<Move> simplify(const std::vector<Move> &mv) {
  std::vector<std::pair<int, std::array<int, 6>>> runs;  // axis, quarter turns per slot
  for (auto &m : mv) {
    const int ax = axisOf(m.face);
    if (runs.empty() || runs.back().first != ax) runs.push_back({ax, {0, 0, 0, 0, 0, 0}});
    auto &r = runs.back().second;
    r[slotOf(m.face, m.depth)] = (r[slotOf(m.face, m.depth)] + m.amt) & 3;
    bool empty = true;
    for (int x : r) empty &= x == 0;
    if (empty) runs.pop_back();
  }
  // faces per axis: slot 0..2 the U/L/F side at depth slot+1, 3..5 the D/R/B side at depth 6-slot
  static const int lo[3] = {0, 1, 2}, hi[3] = {5, 3, 4};
  std::vector<Move> out;
  for (auto &[ax, r] : runs)
    for (int sl = 0; sl < 6; sl++)
      if (r[sl]) out.push_back(sl < 3 ? Move{(uint8_t)lo[ax], (uint8_t)(sl + 1), (uint8_t)r[sl]} : Move{(uint8_t)hi[ax], (uint8_t)(6 - sl), (uint8_t)r[sl]});
  return out;
}
inline std::vector<Move> algMoves(const Alg &A) {
  if (A.src < 0) { fprintf(stderr, "CERT: an algorithm without a source\n"); exit(1); }
  const Src &sc = srcs[A.src];
  if (sc.file < 0) return {Move{(uint8_t)(sc.ref / 3), 1, (uint8_t)(sc.ref % 3 + 1)}};
  auto mv = gModel->image(pools[sc.file].rep[sc.ref / 96], sc.ref % 96);
  return std::vector<Move>(mv.begin() + sc.i, mv.end() - sc.j);
}
// model position -> certificate facelet index (certcube.h pairFacelets order on 8^3), found by
// matching every move's action; must be unique and keep each group on its own face and orbit
inline std::vector<int> facePhi() {
  cc::Cube C(8);
  auto pf = cc::pairFacelets(C);
  std::vector<int> fidx(C.pos.size(), -1);
  for (int k = 0; k < 48; k++) fidx[pf[k]] = k;
  std::vector<std::vector<int>> M, K;
  for (int f = 0; f < 6; f++)
    for (int d = 1; d <= 3; d++)
      for (int t = 1; t <= 3; t++) {
        std::vector<int> a(48), b(48);
        const auto &s = gModel->mv(f, d, t);
        for (int j = 0; j < 48; j++) a[s[j]] = j;
        for (int k = 0; k < 48; k++) {
          int i = pf[k];
          for (int q = 0; q < t; q++) i = C.perms[f][d - 1][i];
          b[k] = fidx[i];
          if (b[k] < 0) { fprintf(stderr, "CERT: a move takes a pair facelet off the pair\n"); exit(1); }
        }
        M.push_back(a), K.push_back(b);
      }
  int nsol = 0;
  std::vector<int> phi(48, -1), sol;
  std::function<void()> solve = [&]() {
    int p0 = -1;
    for (int p = 0; p < 48; p++) if (phi[p] < 0) { p0 = p; break; }
    if (p0 < 0) { nsol++; sol = phi; return; }
    for (int x = 0; x < 48; x++) {
      if (std::find(phi.begin(), phi.end(), x) != phi.end()) continue;
      auto keep = phi;
      phi[p0] = x;
      std::vector<int> stack = {p0};
      bool ok = true;
      while (!stack.empty() && ok) {
        const int p = stack.back();
        stack.pop_back();
        for (size_t mi = 0; mi < M.size() && ok; mi++) {
          const int q = M[mi][p], y = K[mi][phi[p]];
          if (phi[q] < 0) {
            if (std::find(phi.begin(), phi.end(), y) != phi.end()) ok = false;
            else phi[q] = y, stack.push_back(q);
          } else if (phi[q] != y) ok = false;
        }
      }
      if (ok) solve();
      phi = keep;
    }
  };
  solve();
  if (nsol != 1) { fprintf(stderr, "CERT: %d position matchings (need exactly 1)\n", nsol); exit(1); }
  for (int p = 0; p < 48; p++)
    if (sol[p] / 8 != (p / 4) % 6 || (sol[p] % 8) / 4 != p / 24) { fprintf(stderr, "CERT: position %d is not on its group's face\n", p); exit(1); }
  return sol;
}

}  // namespace cert

static void certStage(const Stage &st, const std::vector<uint8_t> &dist, const std::vector<CAlg> &ca) {
  const double t0 = now();
  int maxd = 0;
  for (auto x : dist) maxd = std::max<int>(maxd, x);
  const int nc = st.tg.size();
  const auto phi = cert::facePhi();
  std::vector<int> phiInv(48);
  for (int p = 0; p < 48; p++) phiInv[phi[p]] = p;
  // actions in increasing cost, with their run information
  std::vector<uint32_t> order(ca.size());
  for (uint32_t i = 0; i < ca.size(); i++) order[i] = i;
  std::stable_sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) { return ca[a].cost < ca[b].cost; });
  std::vector<cert::Info> info(ca.size());
  long lenMismatch = 0;
  {
    std::atomic<size_t> ni{0};
    std::atomic<long> mm{0};
    auto w = [&] {
      for (size_t i; (i = ni++) < ca.size();) {
        if (ca[i].alg < 0) { fprintf(stderr, "CERT: an action without an algorithm\n"); exit(1); }
        const auto mv = cert::algMoves(algs[ca[i].alg]);
        info[i] = cert::infoOf(mv);
        if ((int)mv.size() != ca[i].cost) mm++;
      }
    };
    std::vector<std::thread> th;
    for (int t = 0; t < nthreads; t++) th.emplace_back(w);
    for (auto &t : th) t.join();
    lenMismatch = mm;
  }
  if (lenMismatch) { fprintf(stderr, "CERT: %ld actions whose moves differ from their cost\n", lenMismatch); exit(1); }
  uint64_t m0[8];
  for (int c = 0; c < nc; c++) { m0[c] = 0; for (int i = 0; i < st.n; i++) if (st.pos[i] / 4 == st.tg[c]) m0[c] |= 1ULL << i; }
  const uint64_t r0 = rankOf(st, m0);
  // states by table distance
  std::vector<uint64_t> start(maxd + 2, 0);
  for (uint64_t r = 0; r < st.total; r++) start[dist[r] + 1]++;
  for (int d = 1; d <= maxd + 1; d++) start[d] += start[d - 1];
  std::vector<uint32_t> byD(st.total);
  {
    auto pos = start;
    for (uint64_t r = 0; r < st.total; r++) byD[pos[dist[r]]++] = r;
  }
  // states one action from solved: the cheapest such action (scanning for these is slow)
  std::vector<uint32_t> direct(st.total, 0xFFFFFFFF);
  for (uint32_t oi : order) {
    const CAlg &a = ca[oi];
    uint64_t t[8];
    std::vector<uint8_t> inv(st.n);
    for (int i = 0; i < st.n; i++) inv[a.d[i]] = i;
    for (int c = 0; c < nc; c++) { uint64_t z = 0; for (uint64_t q = m0[c]; q; q &= q - 1) z |= 1ULL << inv[__builtin_ctzll(q)]; t[c] = z; }
    const uint64_t x = rankOf(st, t);
    if (direct[x] == 0xFFFFFFFF) direct[x] = oi;
  }
  std::vector<uint8_t> L(st.total, 0xFF);
  std::vector<uint32_t> W(st.total, 0xFFFFFFFF);
  // W2: a second witness b for a(s) when s needs a(s) to start on a's last layer (the table counts
  // a merge with any optimal first move of a(s), the chain keeps one)
  std::vector<uint32_t> W2(st.total, 0xFFFFFFFF);
  // cheap actions by the layer of their first move; one-action states by (state, first move)
  std::vector<std::vector<uint32_t>> cheapByLayer(18);
  struct UH { size_t operator()(uint64_t k) const { return k * 0x9E3779B97F4A7C15ULL >> 17; } };
  std::unordered_map<uint64_t, uint32_t, UH> directFirst;  // rank * 55 + first move -> cheapest action
  std::vector<uint16_t> Fr(st.total, 0);
  L[r0] = 0;
  // cheap actions (pulled per state): the costs whose cumulative count stays within CERTCHEAP
  // (default 1e6); expensive ones are pushed from the low levels (states x expensive actions
  // within CERTPUSH, default 1e11): cand[s] = (value << 32) | action, the best push seen
  const double cheapCap = getenv("CERTCHEAP") ? atof(getenv("CERTCHEAP")) : 1e6;
  const double pushCap = getenv("CERTPUSH") ? atof(getenv("CERTPUSH")) : 1e11;
  int c0 = 0;
  {
    std::map<int, long> byC;
    for (auto &a : ca) byC[a.cost]++;
    long cum = 0;
    for (auto &kv : byC) { if (cum + kv.second > cheapCap && c0 > 0) break; cum += kv.second; c0 = kv.first; }
  }
  std::vector<uint32_t> cheap, dear;
  for (uint32_t oi : order) (ca[oi].cost <= c0 ? cheap : dear).push_back(oi);
  for (uint32_t oi : cheap) if (ca[oi].first < 54 && info[oi].runs >= 2) cheapByLayer[ca[oi].first / 3].push_back(oi);
  directFirst.reserve(ca.size());
  for (uint32_t oi : order) {
    const CAlg &a = ca[oi];
    if (a.first >= 54) continue;
    uint64_t t[8];
    std::vector<uint8_t> inv(st.n);
    for (int i = 0; i < st.n; i++) inv[a.d[i]] = i;
    for (int c = 0; c < nc; c++) { uint64_t z = 0; for (uint64_t q = m0[c]; q; q &= q - 1) z |= 1ULL << inv[__builtin_ctzll(q)]; t[c] = z; }
    directFirst.emplace(rankOf(st, t) * 55 + a.first, oi);
  }
  int K = -1;
  {
    double cum = 0;
    for (int k = 0; k <= maxd; k++) { cum += (double)(start[k + 1] - start[k]) * dear.size(); if (cum > pushCap) break; K = k; }
  }
  printf("  CERT %s: %zu cheap actions (cost <= %d) pulled, %zu dearer ones pushed from levels <= %d\n", gname(st.T).c_str(), cheap.size(), c0, dear.size(), K);
  fflush(stdout);
  std::vector<uint64_t> cand(st.total, ~0ULL);
  auto pushLevel = [&](int k) {
    const double tp = now();
    // the level's sources: masks, value base and first run, computed once
    struct Src { uint64_t m[8]; uint8_t L; uint16_t F; };
    std::vector<Src> srcv;
    for (uint64_t q = start[k]; q < start[k + 1]; q++) {
      const uint64_t x = byD[q];
      if (L[x] == 0xFF) continue;
      Src sv;
      unrank(st, x, sv.m);
      sv.L = L[x], sv.F = Fr[x];
      srcv.push_back(sv);
    }
    std::atomic<size_t> ni{0};
    auto w = [&] {
      std::vector<uint8_t> inv(st.n);
      uint64_t t[8];
      for (size_t j; (j = ni.fetch_add(64)) < dear.size();)
        for (size_t je = std::min(dear.size(), j + 64); j < je; j++) {
          const uint32_t oi = dear[j];
          const CAlg &a = ca[oi];
          for (int i = 0; i < st.n; i++) inv[a.d[i]] = i;
          for (const Src &sv : srcv) {
            const int v = a.cost + sv.L - cert::saved(info[oi], sv.F);
            for (int c = 0; c < nc; c++) { uint64_t z = 0; for (uint64_t b = sv.m[c]; b; b &= b - 1) z |= 1ULL << inv[__builtin_ctzll(b)]; t[c] = z; }
            const uint64_t s = rankOf(st, t);
            const uint64_t key = (uint64_t)v << 32 | oi;
            std::atomic_ref<uint64_t> ar(cand[s]);
            uint64_t cur = ar.load(std::memory_order_relaxed);
            while (key < cur && !ar.compare_exchange_weak(cur, key, std::memory_order_relaxed)) {}
          }
        }
    };
    std::vector<std::thread> th;
    for (int t = 0; t < nthreads; t++) th.emplace_back(w);
    for (auto &t : th) t.join();
    fprintf(stderr, "  CERT push from level %d: %llu states x %zu actions (%.1fs)\n", k, (unsigned long long)(start[k + 1] - start[k]), dear.size(), now() - tp);
  };
  if (K >= 0) pushLevel(0);
  long fullScans = 0, twoStep = 0;
  const bool fullScan = getenv("CERTFULL") != nullptr;  // also scan the dear actions when pulls fail
  std::map<int, long> over;  // L - d for states over the table
  long scanned = 0;
  for (int d = 1; d <= maxd; d++) {
    const double tl = now();
    std::atomic<uint64_t> ni{start[d]};
    std::atomic<long> sc{0};
    std::mutex om;
    auto w = [&] {
      std::map<int, long> ov;
      long s = 0, fs = 0, a2 = 0;
      uint64_t m[8], t[8];
      for (uint64_t c0; (c0 = ni.fetch_add(256)) < start[d + 1];)
        for (uint64_t k = c0, ke = std::min<uint64_t>(start[d + 1], c0 + 256); k < ke; k++) {
          const uint64_t r = byD[k];
          unrank(st, r, m);
          int bestV = 255;
          uint32_t bestA = 0xFFFFFFFF;
          uint64_t bestX = 0;
          if (direct[r] != 0xFFFFFFFF && ca[direct[r]].cost <= d) bestV = ca[direct[r]].cost, bestA = direct[r], bestX = r0;
          if (bestV > d && cand[r] != ~0ULL && (int)(cand[r] >> 32) < bestV) {
            bestV = cand[r] >> 32, bestA = (uint32_t)cand[r];
            const CAlg &a = ca[bestA];
            uint64_t t2[8];
            for (int c = 0; c < nc; c++) { uint64_t z = 0; for (uint64_t q = m[c]; q; q &= q - 1) z |= 1ULL << a.d[__builtin_ctzll(q)]; t2[c] = z; }
            bestX = rankOf(st, t2);
          }
          for (int pass = 0; pass < (fullScan ? 2 : 1) && bestV > d; pass++) {
          if (pass == 1) fs++;
          for (uint32_t oi : pass == 0 ? cheap : dear) {
            const CAlg &a = ca[oi];
            if (a.cost > d + 8 && bestV < 255) break;  // no saving reaches that far
            s++;
            for (int c = 0; c < nc; c++) { uint64_t z = 0; for (uint64_t q = m[c]; q; q &= q - 1) z |= 1ULL << a.d[__builtin_ctzll(q)]; t[c] = z; }
            const uint64_t x = rankOf(st, t);
            if (dist[x] >= d || L[x] == 0xFF) continue;
            const int v = a.cost + L[x] - cert::saved(info[oi], Fr[x]);
            if (v < bestV) { bestV = v, bestA = oi, bestX = x; if (v <= d) break; }
          }
          }
          // two-step witnesses: a, then b chosen for a(s) to start on a's last layer
          uint32_t bestB = 0xFFFFFFFF;
          uint16_t bestF2 = 0;
          if (bestV > d) {
            a2++;
            for (uint32_t oi : cheap) {
              const CAlg &a = ca[oi];
              if (a.last >= 54) continue;
              for (int c = 0; c < nc; c++) { uint64_t z = 0; for (uint64_t q = m[c]; q; q &= q - 1) z |= 1ULL << a.d[__builtin_ctzll(q)]; t[c] = z; }
              const uint64_t x = rankOf(st, t);
              if (dist[x] >= d || a.cost + dist[x] - 2 > d) continue;
              const int layer = a.last / 3;
              // b solves x in one action
              for (int am = 0; am < 3; am++) {
                auto it = directFirst.find(x * 55 + layer * 3 + am);
                if (it == directFirst.end()) continue;
                const uint32_t bi = it->second;
                if (info[bi].runs < 2) continue;
                const int v = a.cost + ca[bi].cost - cert::saved(info[oi], info[bi].first);
                if (v < bestV) { bestV = v, bestA = oi, bestB = bi, bestX = r0, bestF2 = cert::firstAfter(info[oi], info[bi].first); }
              }
              if (bestV <= d) break;
              // b among the cheap actions starting on that layer
              uint64_t u[8];
              for (uint32_t bi : cheapByLayer[layer]) {
                const CAlg &b = ca[bi];
                if (a.cost + b.cost - 4 + 0 > d) break;
                for (int c = 0; c < nc; c++) { uint64_t z = 0; for (uint64_t q = t[c]; q; q &= q - 1) z |= 1ULL << b.d[__builtin_ctzll(q)]; u[c] = z; }
                const uint64_t y = rankOf(st, u);
                if (dist[y] >= dist[x] || L[y] == 0xFF) continue;
                const int v = a.cost + b.cost + L[y] - cert::saved(info[oi], info[bi].first) - cert::saved(info[bi], Fr[y]);
                if (v < bestV) { bestV = v, bestA = oi, bestB = bi, bestX = y, bestF2 = cert::firstAfter(info[oi], cert::firstAfter(info[bi], Fr[y])); if (v <= d) break; }
              }
              if (bestV <= d) break;
            }
          }
          if (bestA == 0xFFFFFFFF) { fprintf(stderr, "CERT: state %llu (distance %d) has no witness\n", (unsigned long long)r, d); exit(1); }
          if (bestB != 0xFFFFFFFF) L[r] = bestV, W[r] = bestA, W2[r] = bestB, Fr[r] = bestF2;
          else L[r] = bestV, W[r] = bestA, Fr[r] = cert::firstAfter(info[bestA], Fr[bestX]);
          if (bestV > d) ov[bestV - d]++;
        }
      sc += s;
      std::lock_guard<std::mutex> g(om);
      fullScans += fs;
      twoStep += a2;
      for (auto &kv : ov) over[kv.first] += kv.second;
    };
    std::vector<std::thread> th;
    for (int t = 0; t < nthreads; t++) th.emplace_back(w);
    for (auto &t : th) t.join();
    scanned += sc;
    if (d <= K) pushLevel(d);
    long ovn = 0;
    for (auto &kv : over) ovn += kv.second;
    fprintf(stderr, "  CERT level %d: %llu states, %.3g actions tried (%.1fs); over the table so far %ld\n", d,
            (unsigned long long)(start[d + 1] - start[d]), (double)sc.load(), now() - tl, ovn);
    fprintf(stderr, "    (full scans so far %ld, two-step searches %ld)\n", fullScans, twoStep);
  }
  printf("  CERT %s: witnesses (%.3g actions tried, %.0fs); over the table:", gname(st.T).c_str(), (double)scanned, now() - t0);
  for (auto &kv : over) printf(" +%d:%ld", kv.first, kv.second);
  printf("\n");
  fflush(stdout);
  // stage symmetries (as in the search): canonical = least rank among the images
  struct SY { std::vector<uint8_t> fmap; std::vector<int> cmap; };
  std::vector<SY> sy;
  {
    auto gapply = [&](const std::vector<int> &g, int mask) { int r = 0; for (int q = 0; q < G; q++) if (mask >> q & 1) r |= 1 << g[q]; return r; };
    for (auto &gs : gsyms) {
      if (gapply(gs.gm, st.S) != st.S || gapply(gs.gm, st.T) != st.T) continue;
      SY y;
      bool ident = true, ok = true;
      for (int i = 0; i < st.n; i++) {
        const int q = gs.sigma[st.pos[i]] % P;
        const int fi = st.cidx[q];
        ok &= fi >= 0;
        y.fmap.push_back(fi < 0 ? 0 : fi);
        ident &= fi == i;
      }
      for (int c = 0; c < nc; c++) {
        int ci = -1;
        for (int e = 0; e < nc; e++) if (st.tg[e] == gs.gm[st.tg[c]]) ci = e;
        ok &= ci >= 0;
        y.cmap.push_back(ci);
        ident &= ci == c;
      }
      if (ok && !ident) sy.push_back(y);
    }
  }
  // sequences for the canonical states, written in rank order
  const std::string fn = std::string(getenv("CERT")) + "." + std::to_string(curStage) + ".txt";
  FILE *f = fopen(fn.c_str(), "w");
  if (!f) { fprintf(stderr, "CERT: cannot write the certificate\n"); exit(1); }
  fprintf(f, "# chiral oblique pair (2,3)/(3,2), centres first: a move sequence for every stage state up to symmetry\n");
  fprintf(f, "# colouring: 48 letters, faces U L F R B D in turn, each face's orbit-a facelets (row,col) = (1,2) (2,6) (6,5) (5,1)\n");
  fprintf(f, "# then its orbit-b facelets (2,1) (1,5) (5,6) (6,2) (0-based on 8x8x8, faces seen from outside in net orientation);\n");
  fprintf(f, "# a letter is the home face of the piece there, '-' a piece this stage does not track\n");
  fprintf(f, "solved: %s\n", st.S ? gname(st.S).c_str() : "-");
  fprintf(f, "target: %s\n", gname(st.T).c_str());
  const uint64_t BLK = 1 << 16;
  const uint64_t nblk = (st.total + BLK - 1) / BLK;
  std::vector<std::string> out(nblk);
  std::atomic<uint64_t> nb{0};
  std::atomic<long> entries{0}, bad{0};
  std::vector<std::array<long, 64>> lenH(nthreads);
  std::vector<std::array<long, 64>> overH(nthreads);  // emitted length - table distance (+32)
  for (auto &h : lenH) h.fill(0);
  for (auto &h : overH) h.fill(0);
  std::atomic<int> tidN{0};
  auto w = [&] {
    const int tid = tidN++;
    uint64_t m[8], t[8];
    for (uint64_t b; (b = nb++) < nblk;) {
      std::string &o = out[b];
      for (uint64_t r = b * BLK, re = std::min(st.total, r + BLK); r < re; r++) {
        unrank(st, r, m);
        bool can = true;
        for (auto &y : sy) {
          for (int c = 0; c < nc; c++) { uint64_t x = 0; for (uint64_t z = m[c]; z; z &= z - 1) x |= 1ULL << y.fmap[__builtin_ctzll(z)]; t[y.cmap[c]] = x; }
          if (rankOf(st, t) < r) { can = false; break; }
        }
        if (!can) continue;
        // the chain of witnesses
        std::vector<Move> seq;
        uint64_t x = r, mm[8];
        memcpy(mm, m, sizeof mm);
        for (int steps = 0; x != r0; steps++) {
          if (steps > 64) { fprintf(stderr, "CERT: witness chain too long\n"); exit(1); }
          for (int h = 0; h < 2; h++) {
            const uint32_t wi = h == 0 ? W[x] : W2[x];
            if (wi == 0xFFFFFFFF) break;
            const CAlg &a = ca[wi];
            auto mv = cert::algMoves(algs[a.alg]);
            seq.insert(seq.end(), mv.begin(), mv.end());
            for (int c = 0; c < nc; c++) { uint64_t z = 0; for (uint64_t q = mm[c]; q; q &= q - 1) z |= 1ULL << a.d[__builtin_ctzll(q)]; t[c] = z; }
            memcpy(mm, t, sizeof mm);
          }
          x = rankOf(st, mm);
        }
        seq = cert::simplify(seq);
        // colouring over the model positions, then re-applied with the model's own move perms
        std::vector<int> col(P, -1);
        for (int p = 0; p < P; p++) if (st.S >> (p / 4) & 1) col[p] = p / 4;
        for (int c = 0; c < nc; c++) for (uint64_t z = m[c]; z; z &= z - 1) col[st.pos[__builtin_ctzll(z)]] = st.tg[c];
        const auto q = gModel->permOf(seq);
        bool ok = true;
        for (int p = 0; p < P; p++) {
          const int g = col[q[p]];
          if ((st.S | st.T) >> (p / 4) & 1) ok &= g == p / 4;
        }
        if (!ok) bad++;
        lenH[tid][std::min<size_t>(63, seq.size())]++;
        overH[tid][std::clamp<int>((int)seq.size() - dist[r] + 32, 0, 63)]++;
        entries++;
        for (int k = 0; k < 48; k++) { const int g = col[phiInv[k]]; o += g < 0 ? '-' : cc::FACES[g % 6]; }
        for (auto &mv : seq) { o += ' '; o += move_str(mv); }
        o += '\n';
      }
    }
  };
  std::vector<std::thread> th;
  for (int t = 0; t < nthreads; t++) th.emplace_back(w);
  for (auto &t : th) t.join();
  std::array<long, 64> lh{}, oh{};
  for (auto &h : lenH) for (int i = 0; i < 64; i++) lh[i] += h[i];
  for (auto &h : overH) for (int i = 0; i < 64; i++) oh[i] += h[i];
  int maxLen = 0;
  for (int i = 0; i < 64; i++) if (lh[i]) maxLen = i;
  fprintf(f, "maxlen: %d\n", maxLen);
  fprintf(f, "entries: %ld\n", entries.load());
  for (auto &s : out) fputs(s.c_str(), f);
  fclose(f);
  printf("  CERT %s: %ld canonical states written (%zu symmetries), %ld not solved on re-check, longest %d; lengths:", gname(st.T).c_str(), entries.load(), sy.size() + 1, bad.load(), maxLen);
  for (int i = 0; i < 64; i++) if (lh[i]) printf(" %d:%ld", i, lh[i]);
  printf("; length - table:");
  for (int i = 0; i < 64; i++) if (oh[i]) printf(" %+d:%ld", i - 32, oh[i]);
  printf(" (%.0fs)\n", now() - t0);
  fflush(stdout);
}

// HARDEST=1 (with LOADDIST): for every canonical state at the stage's worst table distance, a
// path of at most that cost found by depth-first search guided by the table (as EXAMPLE), its
// moves simplified and re-checked; printed with the certificate colouring.
static void hardestStage(const Stage &st, const std::vector<uint8_t> &dist, const std::vector<CAlg> &ca) {
  const double t0 = now();
  const int nc = st.tg.size();
  int maxd = 0;
  for (auto x : dist) maxd = std::max<int>(maxd, x);
  const auto phi = cert::facePhi();
  std::vector<int> phiInv(48);
  for (int p = 0; p < 48; p++) phiInv[phi[p]] = p;
  std::vector<cert::Info> info(ca.size());
  for (size_t i = 0; i < ca.size(); i++) info[i] = cert::infoOf(cert::algMoves(algs[ca[i].alg]));
  // canonical states at maxd under the stage symmetries
  struct SY { std::vector<uint8_t> fmap; std::vector<int> cmap; };
  std::vector<SY> sy;
  auto gapply = [&](const std::vector<int> &g, int mask) { int r = 0; for (int q = 0; q < G; q++) if (mask >> q & 1) r |= 1 << g[q]; return r; };
  for (auto &gs : gsyms) {
    if (gapply(gs.gm, st.S) != st.S || gapply(gs.gm, st.T) != st.T) continue;
    SY y;
    bool ident = true, ok = true;
    for (int i = 0; i < st.n; i++) { const int fi = st.cidx[gs.sigma[st.pos[i]] % P]; ok &= fi >= 0; y.fmap.push_back(fi < 0 ? 0 : fi); ident &= fi == i; }
    for (int c = 0; c < nc; c++) { int ci = -1; for (int e = 0; e < nc; e++) if (st.tg[e] == gs.gm[st.tg[c]]) ci = e; ok &= ci >= 0; y.cmap.push_back(ci); ident &= ci == c; }
    if (ok && !ident) sy.push_back(y);
  }
  std::vector<uint64_t> hard;
  long atMax = 0;
  for (uint64_t r = 0; r < st.total; r++) {
    if (dist[r] != maxd) continue;
    atMax++;
    uint64_t m[8], t[8];
    unrank(st, r, m);
    bool can = true;
    for (auto &y : sy) {
      for (int c = 0; c < nc; c++) { uint64_t x = 0; for (uint64_t z = m[c]; z; z &= z - 1) x |= 1ULL << y.fmap[__builtin_ctzll(z)]; t[y.cmap[c]] = x; }
      if (rankOf(st, t) < r) { can = false; break; }
    }
    if (can) hard.push_back(r);
  }
  printf("  HARDEST %s: %ld states at %d, %zu canonical (%zu symmetries)\n", gname(st.T).c_str(), atMax, maxd, hard.size(), sy.size() + 1);
  fflush(stdout);
  std::vector<std::string> out(hard.size());
  std::atomic<size_t> ni{0};
  std::atomic<long> found{0};
  auto w = [&] {
    for (size_t hi; (hi = ni++) < hard.size();) {
      uint64_t m0[8];
      unrank(st, hard[hi], m0);
      std::vector<uint32_t> path, best;
      long nodes = 0;
      // left: moves still allowed; f: first run of nothing yet (actions are appended, so the
      // saving is between the previous action's last run and this action's first run)
      std::function<bool(const uint64_t *, int, int)> dfs = [&](const uint64_t *m, int prev, int left) -> bool {
        const int d = dist[rankOf(st, m)];
        if (d == 0) { best = path; return true; }
        if (++nodes > 20000000) return false;
        std::vector<std::pair<int, uint32_t>> kids;
        for (uint32_t i = 0; i < ca.size(); i++) {
          const CAlg &a = ca[i];
          int c = a.cost;
          if (prev >= 0) {  // moves saved against the previous action's last run
            const cert::Info &pi = info[prev], &ai = info[i];
            c -= cert::saved(pi, ai.first);
          }
          if (c < 1 || c > left) continue;  // an action that only cancels the last one is no step
          uint64_t t[8];
          for (int k = 0; k < nc; k++) { uint64_t z = 0; for (uint64_t q = m[k]; q; q &= q - 1) z |= 1ULL << a.d[__builtin_ctzll(q)]; t[k] = z; }
          const int d2 = dist[rankOf(st, t)];
          if (c + d2 - (d2 ? 2 : 0) > left) continue;
          kids.push_back({(c + d2) * 64 + c, i});
        }
        std::sort(kids.begin(), kids.end());
        for (auto &kd : kids) {
          const CAlg &a = ca[kd.second];
          uint64_t t[8];
          for (int k = 0; k < nc; k++) { uint64_t z = 0; for (uint64_t q = m[k]; q; q &= q - 1) z |= 1ULL << a.d[__builtin_ctzll(q)]; t[k] = z; }
          path.push_back(kd.second);
          if (dfs(t, kd.second, left - kd.first % 64)) return true;
          path.pop_back();
        }
        return false;
      };
      std::vector<int> col(P, -1);
      for (int p = 0; p < P; p++) if (st.S >> (p / 4) & 1) col[p] = p / 4;
      for (int c = 0; c < nc; c++) for (uint64_t z = m0[c]; z; z &= z - 1) col[st.pos[__builtin_ctzll(z)]] = st.tg[c];
      std::string colour;
      for (int k = 0; k < 48; k++) { const int g = col[phiInv[k]]; colour += g < 0 ? '-' : cc::FACES[g % 6]; }
      if (!dfs(m0, -1, maxd)) { out[hi] = colour + " (no path found)\n"; continue; }
      std::vector<Move> seq;
      for (uint32_t i : best) { auto mv = cert::algMoves(algs[ca[i].alg]); seq.insert(seq.end(), mv.begin(), mv.end()); }
      seq = cert::simplify(seq);
      const auto q = gModel->permOf(seq);
      bool ok = true;
      for (int p = 0; p < P; p++) if ((st.S | st.T) >> (p / 4) & 1) ok &= col[q[p]] == p / 4;
      std::string s = colour + " " + std::to_string(seq.size()) + (ok ? "" : " NOT SOLVED") + ":";
      for (auto &mv : seq) { s += ' '; s += move_str(mv); }
      out[hi] = s + "\n";
      found++;
    }
  };
  std::vector<std::thread> th;
  for (int t = 0; t < nthreads; t++) th.emplace_back(w);
  for (auto &t : th) t.join();
  for (auto &s : out) printf("  %s", s.c_str());
  printf("  HARDEST %s: %ld of %zu canonical states solved within %d (%.0fs)\n", gname(st.T).c_str(), found.load(), hard.size(), maxd, now() - t0);
  fflush(stdout);
}

// CERT=DIR in a search run (the stage is searched, not loaded): a certificate from the search's
// own bookkeeping.  Per state, dist and tails (the last moves of its cheapest scramble paths;
// a scramble action's first move merges with a tail of its source).  For a canonical state s the
// scramble path is rebuilt backwards: an action a with a.last among the allowed tails of the
// current state x, from r = a^-1(x) with dist[r] + cost(a) - save(a.first, tails[r]) <= dist[x];
// the next allowed tails are those of r giving that saving.  The solving sequence is the path
// inverted.  Actions: the search's own (budgeted) set by last move, then the dearer ones pushed
// from the levels that used them, then (rarely) a scan of every action.  Output, sequence only:
// DIR/stages/<solved>..<target>/wNN.txt by the number of target pieces off their home
// (wrongly placed), lines in canonical rank order; DIR/MANIFEST gets a line per file.
static void certSearch(const Stage &st, const std::vector<uint8_t> &dist, const std::vector<uint64_t> &tails,
                       const std::vector<CAlg> &ca, const std::vector<CAlg> &caFull) {
  const double t0 = now();
  const int nc = st.tg.size();
  int maxd = 0;
  for (auto x : dist) maxd = std::max<int>(maxd, x);
  // every action (full set), sorted by cost; which ones the budgeted search used
  struct AK { size_t operator()(const std::vector<uint8_t> &v) const { size_t h = 1469598103934665603ULL; for (auto c : v) h = (h ^ c) * 1099511628211ULL; return h; } };
  std::unordered_map<std::vector<uint8_t>, char, AK> inBudget;
  for (auto &a : ca) { auto k = a.d; k.push_back(a.first), k.push_back(a.last); inBudget[k] = 1; }
  std::vector<uint32_t> cheapIdx, dearIdx;
  for (uint32_t i = 0; i < caFull.size(); i++) {
    auto k = caFull[i].d;
    k.push_back(caFull[i].first), k.push_back(caFull[i].last);
    (inBudget.count(k) ? cheapIdx : dearIdx).push_back(i);
  }
  inBudget.clear();
  auto byCost = [&](uint32_t a, uint32_t b) { return caFull[a].cost < caFull[b].cost; };
  std::stable_sort(cheapIdx.begin(), cheapIdx.end(), byCost);
  std::stable_sort(dearIdx.begin(), dearIdx.end(), byCost);
  // cheap actions by last move (55 codes)
  std::vector<std::vector<uint32_t>> cheapByLast(55);
  for (uint32_t i : cheapIdx) cheapByLast[caFull[i].last].push_back(i);
  // inverse action perms (free index j -> i with a.d[i] == j)
  std::vector<uint8_t> invD(caFull.size() * st.n);
  for (size_t i = 0; i < caFull.size(); i++) for (int k = 0; k < st.n; k++) invD[i * st.n + caFull[i].d[k]] = k;
  auto saveOf = [&](int first, uint64_t tl) { return (tl & mergeSame0[first]) ? 2 : (tl & mergeSame1[first]) ? 1 : 0; };
  auto needAfter = [&](int first, uint64_t tl, int save) { return save == 2 ? (tl & mergeSame0[first]) : save == 1 ? (tl & mergeSame1[first]) : tl; };
  // levels whose sources used every action in the search: push the dear actions from them, keep
  // the tight ones: (target, last move) -> action
  std::vector<uint64_t> lvl(maxd + 1, 0);
  for (auto x : dist) lvl[x]++;
  struct UH { size_t operator()(uint64_t k) const { return k * 0x9E3779B97F4A7C15ULL >> 17; } };
  std::unordered_map<uint64_t, uint32_t, UH> dearMap;
  {
    std::vector<uint64_t> srcs;
    double work = 0;
    int kmax = -1;
    for (int k = 0; k <= maxd; k++) {
      if (!lvl[k]) continue;
      if ((double)lvl[k] * caFull.size() > fullWork) break;
      work += (double)lvl[k] * dearIdx.size();
      kmax = k;
    }
    for (uint64_t r = 0; r < st.total; r++) if (dist[r] <= kmax) srcs.push_back(r);
    std::vector<uint64_t> srcM(srcs.size() * 8);
    for (size_t si = 0; si < srcs.size(); si++) unrank(st, srcs[si], &srcM[si * 8]);
    std::vector<std::vector<std::pair<uint64_t, uint32_t>>> outs(nthreads);
    std::atomic<size_t> ni{0};
    std::atomic<int> tidN{0};
    const double tp = now();
    auto w = [&] {
      const int tid = tidN++;
      uint64_t m[8], t[8];
      for (size_t j; (j = ni.fetch_add(16)) < dearIdx.size();)
        for (size_t je = std::min(dearIdx.size(), j + 16); j < je; j++) {
          const uint32_t ai = dearIdx[j];
          const CAlg &a = caFull[ai];
          for (size_t si = 0; si < srcs.size(); si++) {
            const uint64_t r = srcs[si];
            memcpy(m, &srcM[si * 8], sizeof m);
            for (int c = 0; c < nc; c++) { uint64_t z = 0; for (uint64_t q = m[c]; q; q &= q - 1) z |= 1ULL << a.d[__builtin_ctzll(q)]; t[c] = z; }
            const uint64_t s = rankOf(st, t);
            if (dist[r] + a.cost - saveOf(a.first, tails[r]) <= dist[s] && (tails[s] >> a.last & 1)) outs[tid].push_back({s * 55 + a.last, ai});
          }
        }
    };
    std::vector<std::thread> th;
    for (int t = 0; t < nthreads; t++) th.emplace_back(w);
    for (auto &t : th) t.join();
    for (auto &o : outs) for (auto &kv : o) { auto it = dearMap.find(kv.first); if (it == dearMap.end() || caFull[kv.second].cost < caFull[it->second].cost) dearMap[kv.first] = kv.second; }
    printf("  CERT %s: %zu budgeted actions, %zu dearer; dearer pushed from %zu states (levels <= %d, %.3g pairs): %zu tight (%.0fs)\n", gname(st.T).c_str(),
           cheapIdx.size(), dearIdx.size(), srcs.size(), kmax, work, dearMap.size(), now() - tp);
    fflush(stdout);
  }
  // stage symmetries for canonical states
  struct SY { std::vector<uint8_t> fmap; std::vector<int> cmap; };
  std::vector<SY> sy;
  {
    auto gapply = [&](const std::vector<int> &g, int mask) { int r = 0; for (int q = 0; q < G; q++) if (mask >> q & 1) r |= 1 << g[q]; return r; };
    for (auto &gs : gsyms) {
      if (gapply(gs.gm, st.S) != st.S || gapply(gs.gm, st.T) != st.T) continue;
      SY y;
      bool ident = true, ok = true;
      for (int i = 0; i < st.n; i++) { const int fi = st.cidx[gs.sigma[st.pos[i]] % P]; ok &= fi >= 0; y.fmap.push_back(fi < 0 ? 0 : fi); ident &= fi == i; }
      for (int c = 0; c < nc; c++) { int ci = -1; for (int e = 0; e < nc; e++) if (st.tg[e] == gs.gm[st.tg[c]]) ci = e; ok &= ci >= 0; y.cmap.push_back(ci); ident &= ci == c; }
      if (ok && !ident) sy.push_back(y);
    }
  }
  uint64_t m0[8];
  for (int c = 0; c < nc; c++) { m0[c] = 0; for (int i = 0; i < st.n; i++) if (st.pos[i] / 4 == st.tg[c]) m0[c] |= 1ULL << i; }
  const uint64_t r0 = rankOf(st, m0);
  int maxW = 0;
  for (int c = 0; c < nc; c++) maxW += 4;
  // output files
  const std::string root = getenv("CERT");
  const std::string sname = (st.S ? gname(st.S) : std::string("none")) + ".." + gname(st.T);
  const std::string dir = root + "/stages/" + sname;
  if (system(("mkdir -p '" + dir + "'").c_str()) != 0) { fprintf(stderr, "CERT: cannot make the stage directory\n"); exit(1); }
  const int wDigits = maxW >= 10 ? 2 : 1;
  const uint64_t BLK = 1 << 14;
  const uint64_t nblk = (st.total + BLK - 1) / BLK;
  std::vector<std::vector<std::string>> out(nblk, std::vector<std::string>(maxW + 1));
  std::atomic<uint64_t> nb{0};
  std::atomic<long> entries{0}, bad{0}, over{0}, dearHits{0}, fullScans{0};
  std::vector<std::array<long, 64>> lenH(nthreads);
  std::vector<std::vector<int>> segMax(nthreads, std::vector<int>(maxW + 1, 0));
  std::vector<std::vector<long>> segN(nthreads, std::vector<long>(maxW + 1, 0));
  for (auto &h : lenH) h.fill(0);
  std::atomic<int> tidN{0};
  auto w = [&] {
    const int tid = tidN++;
    uint64_t m[8], t[8], x[8], rr[8];
    for (uint64_t b; (b = nb++) < nblk;) {
      for (uint64_t r = b * BLK, re = std::min(st.total, r + BLK); r < re; r++) {
        unrank(st, r, m);
        bool can = true;
        for (auto &y : sy) {
          for (int c = 0; c < nc; c++) { uint64_t z = 0; for (uint64_t q = m[c]; q; q &= q - 1) z |= 1ULL << y.fmap[__builtin_ctzll(q)]; t[y.cmap[c]] = z; }
          if (rankOf(st, t) < r) { can = false; break; }
        }
        if (!can) continue;
        // backwards: the scramble path's actions, last first
        std::vector<uint32_t> path;
        uint64_t cur = r, need = tails[r];
        memcpy(x, m, sizeof x);
        int budget = dist[r];
        bool ok = true;
        while (cur != r0) {
          int bestV = 255;
          uint32_t bestA = 0;
          uint64_t bestR = 0;
          auto tryA = [&](uint32_t ai) {
            const CAlg &a = caFull[ai];
            const uint8_t *iv = &invD[(size_t)ai * st.n];
            for (int c = 0; c < nc; c++) { uint64_t z = 0; for (uint64_t q = x[c]; q; q &= q - 1) z |= 1ULL << iv[__builtin_ctzll(q)]; rr[c] = z; }
            const uint64_t q = rankOf(st, rr);
            const int v = dist[q] + a.cost - saveOf(a.first, tails[q]);
            if (v < bestV) bestV = v, bestA = ai, bestR = q;
            return v <= dist[cur];
          };
          bool found = false;
          for (uint64_t nd = need ? need : (1ULL << 54); nd && !found; nd &= nd - 1) {
            const int code = __builtin_ctzll(nd);
            for (uint32_t ai : cheapByLast[code]) if (tryA(ai)) { found = true; break; }
          }
          for (uint64_t nd = need; nd && !found; nd &= nd - 1) {
            auto it = dearMap.find(cur * 55 + __builtin_ctzll(nd));
            if (it != dearMap.end() && tryA(it->second)) { found = true; dearHits++; }
          }
          if (!found) {
            fullScans++;
            for (uint32_t ai : dearIdx) if ((need >> caFull[ai].last & 1) && tryA(ai)) { found = true; break; }
          }
          if (!found) {  // the best seen, from any tail (a race in the search may have left one)
            if (bestV == 255) { ok = false; break; }
            over++;
          }
          const CAlg &a = caFull[bestA];
          path.push_back(bestA);
          const int sv = saveOf(a.first, tails[bestR]);
          need = needAfter(a.first, tails[bestR], sv);
          budget -= a.cost - sv;
          cur = bestR;
          unrank(st, cur, x);
          if (path.size() > 64) { ok = false; break; }
        }
        if (!ok) { bad++; continue; }
        // the solving sequence: the scramble path inverted (its actions last first, each inverted)
        std::vector<Move> seq;
        for (uint32_t ai : path) {
          auto mv = cert::algMoves(algs[caFull[ai].alg]);
          for (auto it = mv.rbegin(); it != mv.rend(); ++it) seq.push_back(Move{it->face, it->depth, (uint8_t)(4 - it->amt)});
        }
        seq = cert::simplify(seq);
        // re-check on the model
        std::vector<int> col(P, -1);
        for (int p = 0; p < P; p++) if (st.S >> (p / 4) & 1) col[p] = p / 4;
        for (int c = 0; c < nc; c++) for (uint64_t z = m[c]; z; z &= z - 1) col[st.pos[__builtin_ctzll(z)]] = st.tg[c];
        const auto q = gModel->permOf(seq);
        bool solved = true;
        for (int p = 0; p < P; p++) if ((st.S | st.T) >> (p / 4) & 1) solved &= col[q[p]] == p / 4;
        if (!solved) { bad++; continue; }
        int wr = 0;
        for (int c = 0; c < nc; c++) wr += __builtin_popcountll(m[c] & ~m0[c]);
        std::string &o = out[b][wr];
        for (size_t i = 0; i < seq.size(); i++) { if (i) o += ' '; o += move_str(seq[i]); }
        o += '\n';
        lenH[tid][std::min<size_t>(63, seq.size())]++;
        segMax[tid][wr] = std::max<int>(segMax[tid][wr], seq.size());
        segN[tid][wr]++;
        entries++;
      }
    }
  };
  std::vector<std::thread> th;
  for (int t = 0; t < nthreads; t++) th.emplace_back(w);
  for (auto &t : th) t.join();
  std::array<long, 64> lh{};
  for (auto &h : lenH) for (int i = 0; i < 64; i++) lh[i] += h[i];
  FILE *man = fopen((root + "/MANIFEST").c_str(), "a");
  for (int wr = 0; wr <= maxW; wr++) {
    long n = 0;
    int mx = 0;
    for (int t = 0; t < nthreads; t++) n += segN[t][wr], mx = std::max(mx, segMax[t][wr]);
    char fn[32];
    snprintf(fn, sizeof fn, "w%0*d.txt", wDigits, wr);
    FILE *f = fopen((dir + "/" + fn).c_str(), "w");
    if (!f) { fprintf(stderr, "CERT: cannot write a segment\n"); exit(1); }
    for (auto &blk : out) fputs(blk[wr].c_str(), f);
    fclose(f);
    if (man) fprintf(man, "stages/%s/%s %ld lines, longest %d\n", sname.c_str(), fn, n, mx);
  }
  if (man) fclose(man);
  int maxLen = 0;
  for (int i = 0; i < 64; i++) if (lh[i]) maxLen = i;
  printf("  CERT %s: %ld canonical states (%zu symmetries), %ld failed, %ld steps over the table, %ld dear lookups, %ld full scans; longest %d; lengths:",
         gname(st.T).c_str(), entries.load(), sy.size() + 1, bad.load(), over.load(), dearHits.load(), fullScans.load(), maxLen);
  for (int i = 0; i < 64; i++) if (lh[i]) printf(" %d:%ld", i, lh[i]);
  printf(" (%.0fs)\n", now() - t0);
  fflush(stdout);
}
