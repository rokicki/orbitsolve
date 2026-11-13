// Generic symmetry-reduced coset pattern database.
//   abstraction: per orbit, labels (piece -> class) and optional orientation
//   key orbits: their projection is reduced by symmetry via a rep table
//   index = classId(min key) * restSize + rank(rest projection of the conjugated state)
//   values: 4-bit exact abstract distances (15 = unknown), filled by BFS with
//   twin closure (stabilizer images of every entry are set too).
#pragma once
#include "coset.h"
#include "kern.h"
#include <atomic>
#include <thread>
#include "perf.h"
#include <random>

static uint64_t binomSlow(int n, int k) {
  if (k < 0 || k > n) return 0;
  uint64_t r = 1;
  for (int i = 1; i <= k; i++) r = r * (n - k + i) / i;
  return r;
}
struct BinomTab {
  uint64_t t[65][65];
  BinomTab() { for (int n = 0; n < 65; n++) for (int k = 0; k < 65; k++) t[n][k] = binomSlow(n, k); }
};
static const BinomTab gBinom;
static inline uint64_t binom(int n, int k) { return (k < 0 || n < 0 || k > n) ? 0 : gBinom.t[n][k]; }

struct OrbitProj {
  int k, off, n, omod, nl;
  vector<int> lab;          // local home position -> label
  vector<int> cnt;          // pieces per label
  vector<vector<int>> homes;// label -> local home positions
  bool ori;
  int mode = 0;             // 0 full, 1 permutation (labels) only, 2 orientation only
  uint32_t pmask = 0;       // partial orientation: labels whose pieces keep orientation (digits in position order, LSB first)
  int pk = 0;
  uint64_t permSize = 1, oriSize = 1;
  void init(const Puzzle &pz, const Abstraction &A, int k_, int mode_ = 0) {
    k = k_; mode = mode_; auto &ob = pz.orbits[k]; off = ob.off; n = ob.n; omod = ob.omod; ori = A.keepOri[k] && mode != 1;
    nl = mode == 2 ? 1 : A.nlabels[k]; lab.resize(n); cnt.assign(nl, 0); homes.assign(nl, {});
    for (int j = 0; j < n; j++) { lab[j] = mode == 2 ? 0 : A.label[off + j]; cnt[lab[j]]++; homes[lab[j]].push_back(j); }
    permSize = 1; int rem = n;
    for (int l = 0; l < nl - 1; l++) { permSize *= binom(rem, cnt[l]); rem -= cnt[l]; }
    oriSize = 1; if (ori) for (int j = 0; j < n - 1; j++) oriSize *= omod;
    if (!ori && mode == 0 && k < (int)A.oriLabMask.size() && A.oriLabMask[k] && omod > 1) {
      pmask = A.oriLabMask[k]; pk = 0;
      for (int l = 0; l < nl; l++) if ((pmask >> l) & 1) pk += cnt[l];
      for (int t = 0; t < pk; t++) oriSize *= omod;
    }
  }
  uint64_t size() const { return permSize * oriSize; }
  // rank of this orbit's projection in a packed state
  uint64_t rank(const Layout &L, const uint8_t *st) const {
    int lv[64];
    for (int j = 0; j < n; j++) lv[j] = lab[(st[off + j] & L.posmask) - off];
    uint64_t r = 0;
    int idxs[64], m = n;
    for (int j = 0; j < n; j++) idxs[j] = j;
    for (int l = 0; l < nl - 1; l++) {
      // colex rank of positions (among remaining) holding label l
      uint64_t cr = 0; int c = 0, w = 0;
      for (int t = 0; t < m; t++) {
        int j = idxs[t];
        if (lv[j] == l) { c++; cr += binom(t, c); } else idxs[w++] = j;
      }
      r = r * binom(m, cnt[l]) + cr;
      m = w;
    }
    if (ori) for (int j = 0; j < n - 1; j++) r = r * omod + (st[off + j] >> L.p);
    if (pmask) {
      uint64_t d = 0, w = 1;
      for (int j = 0; j < n; j++) if ((pmask >> lv[j]) & 1) { d += w * (st[off + j] >> L.p); w *= omod; }
      r = r * oriSize + d;
    }
    return r;
  }
  // precomputed representatives (n bytes each) for small projections
  vector<uint8_t> rows;
  void buildRows(const Layout &L) {
    if (size() > 2000000 || n > 16) return;
    vector<uint8_t> rr(size() * n);
    uint8_t st[KMAX];
    for (uint64_t r = 0; r < size(); r++) {
      for (int j = 0; j < n; j++) st[off + j] = off + j;
      unrankSlow(L, r, st);
      for (int j = 0; j < n; j++) rr[r * n + j] = mode == 2 ? (st[off + j] & ~L.posmask) : st[off + j];
    }
    rows.swap(rr);
  }
  inline void unrank(const Layout &L, uint64_t r, uint8_t *st) const {
    if (!rows.empty()) {
      const uint8_t *row = &rows[r * n];
      if (mode == 2) for (int j = 0; j < n; j++) st[off + j] = (st[off + j] & L.posmask) | row[j];
      else memcpy(st + off, row, n);
      return;
    }
    unrankSlow(L, r, st);
  }
  // write a representative of projection value r into st (positions of this orbit)
  void unrankSlow(const Layout &L, uint64_t r, uint8_t *st) const {
    int ov[64];
    if (ori) {
      int sum = 0;
      for (int j = n - 2; j >= 0; j--) { ov[j] = r % omod; r /= omod; sum += ov[j]; }
      ov[n - 1] = (omod - sum % omod) % omod;
    }
    if (mode == 2) {   // orientation only: keep the permutation already written
      for (int j = 0; j < n; j++) st[off + j] = (st[off + j] & L.posmask) | (ov[j] << L.p);
      return;
    }
    if (!ori) for (int j = 0; j < n; j++) ov[j] = 0;
    uint64_t pr = 0;
    if (pmask) { pr = r % oriSize; r /= oriSize; }
    uint64_t parts[64]; int rems[64];
    { int rem = n; for (int l = 0; l < nl - 1; l++) { rems[l] = rem; rem -= cnt[l]; } }
    for (int l = nl - 2; l >= 0; l--) { uint64_t b = binom(rems[l], cnt[l]); parts[l] = r % b; r /= b; }
    int lv[64], idxs[64], m = n;
    for (int j = 0; j < n; j++) { idxs[j] = j; lv[j] = nl - 1; }
    for (int l = 0; l < nl - 1; l++) {
      uint64_t cr = parts[l]; int c = cnt[l];
      char chosen[64]; for (int t = 0; t < m; t++) chosen[t] = 0;
      for (int t = m - 1; t >= 0 && c > 0; t--) {
        uint64_t b = binom(t, c);
        if (cr >= b) { cr -= b; chosen[t] = 1; c--; }
      }
      int w = 0;
      for (int t = 0; t < m; t++) { if (chosen[t]) lv[idxs[t]] = l; else idxs[w++] = idxs[t]; }
      m = w;
    }
    if (pmask) for (int j = 0; j < n; j++) if ((pmask >> lv[j]) & 1) { ov[j] = pr % omod; pr /= omod; }
    int used[64]; for (int l = 0; l < nl; l++) used[l] = 0;
    for (int j = 0; j < n; j++) {
      int l = lv[j];
      int piece = homes[l][used[l]++];
      st[off + j] = (piece + off) | (ov[j] << L.p);
    }
  }
};

template <int NCH> struct CosetPDB {
  const Puzzle &pz; const Layout &L;
  Abstraction A;
  vector<OrbitProj> keyP, restP;
  vector<KSym> ks;
  uint64_t keySize = 1, restSize = 1, nclass = 0, size = 0;
  vector<uint8_t> repSym;        // raw key -> sym
  vector<int32_t> repClass;      // raw key -> class id
  vector<uint64_t> classRep;     // class id -> raw key of rep
  vector<vector<uint8_t>> stab;  // class id -> nontrivial stabilizer syms
  vector<uint8_t> tab;           // nibbles
  uint64_t twinClasses = 0, keyReachable = 0;

  // keyOrbits: whole projection is the symmetry key.  splitOrbits: labels go to the
  // key, orientation to the rest (key orbits must be listed before rest for unrank order).
  CosetPDB(const Puzzle &p, const Layout &l, const Abstraction &a, const vector<Elem> &syms, const vector<int> &keyOrbits,
           const vector<int> &splitOrbits = {})
      : pz(p), L(l), A(a) {
    for (auto &s : syms) ks.push_back(prepSym(pz, L, s));
    auto has = [](const vector<int> &v, int k) { return find(v.begin(), v.end(), k) != v.end(); };
    for (size_t k = 0; k < pz.orbits.size(); k++) {
      if (A.ignore[k]) continue;
      if (has(splitOrbits, k)) { OrbitProj op; op.init(pz, A, k, 1); keyP.push_back(op); continue; }
      OrbitProj op; op.init(pz, A, k);
      if (has(keyOrbits, k)) keyP.push_back(op); else restP.push_back(op);
    }
    for (size_t k = 0; k < pz.orbits.size(); k++)
      if (!A.ignore[k] && has(splitOrbits, k) && A.keepOri[k]) { OrbitProj op; op.init(pz, A, k, 2); restP.push_back(op); }
    for (auto &o : keyP) o.buildRows(L);
    for (auto &o : restP) o.buildRows(L);
    for (auto &o : keyP) keySize *= o.size();
    for (auto &o : restP) restSize *= o.size();
  }
  uint64_t keyRank(const uint8_t *st) const { uint64_t r = 0; for (auto &o : keyP) r = r * o.size() + o.rank(L, st); return r; }
  uint64_t restRank(const uint8_t *st) const { uint64_t r = 0; for (auto &o : restP) r = r * o.size() + o.rank(L, st); return r; }
  void keyUnrank(uint64_t r, uint8_t *st) const { for (int i = keyP.size() - 1; i >= 0; i--) { keyP[i].unrank(L, r % keyP[i].size(), st); r /= keyP[i].size(); } }
  void restUnrank(uint64_t r, uint8_t *st) const { for (int i = restP.size() - 1; i >= 0; i--) { restP[i].unrank(L, r % restP[i].size(), st); r /= restP[i].size(); } }

  vector<uint8_t> keyReach;      // reachable raw key values (BFS on the key projection)
  void buildRep() {
    repSym.assign(keySize, 0); repClass.assign(keySize, -1);
    vector<uint64_t> minOf(keySize, ~0ULL);
    uint8_t st[KMAX], ct[KMAX], id[KMAX];
    L.pack(pz.ident(), id);
    {
      keyReach.assign(keySize, 0);
      vector<KElem> km; for (auto &m : pz.moves) km.push_back(L.prep(pz, m.e));
      vector<uint64_t> q{keyRank(id)}; keyReach[q[0]] = 1;
      for (size_t h = 0; h < q.size(); h++) {
        memcpy(st, id, KMAX); keyUnrank(q[h], st);
        St<NCH> x = load<NCH>(st);
        for (auto &m : km) { store<NCH>(mulc<NCH>(L, x, m), ct); uint64_t r = keyRank(ct); if (!keyReach[r]) { keyReach[r] = 1; q.push_back(r); } }
      }
      keyReachable = q.size();
    }
    for (uint64_t v = 0; v < keySize; v++) {
      if (!keyReach[v]) continue;
      memcpy(st, id, KMAX); keyUnrank(v, st);
      St<NCH> x = load<NCH>(st);
      uint64_t best = ~0ULL; int bs = 0;
      for (size_t s = 0; s < ks.size(); s++) {
        store<NCH>(conj<NCH>(L, x, ks[s]), ct);
        uint64_t r = keyRank(ct);
        if (r < best) { best = r; bs = s; }
      }
      repSym[v] = bs; minOf[v] = best;
    }
    for (uint64_t v = 0; v < keySize; v++) if (minOf[v] == v) { repClass[v] = nclass++; classRep.push_back(v); }
    for (uint64_t v = 0; v < keySize; v++) if (keyReach[v]) repClass[v] = repClass[minOf[v]];
    stab.assign(nclass, {});
    for (uint64_t c = 0; c < nclass; c++) {
      memcpy(st, id, KMAX); keyUnrank(classRep[c], st);
      St<NCH> x = load<NCH>(st);
      for (size_t s = 1; s < ks.size(); s++) {
        store<NCH>(conj<NCH>(L, x, ks[s]), ct);
        if (keyRank(ct) == classRep[c]) stab[c].push_back(s);
      }
      if (!stab[c].empty()) twinClasses++;
    }
    size = nclass * restSize;
  }
  // index of a (packed) state; also returns the conjugated state
  inline uint64_t index(const St<NCH> &x, St<NCH> *yout = nullptr, uint64_t *cls = nullptr) const {
    uint8_t b[KMAX];
    store<NCH>(x, b);
    uint64_t kr = keyRank(b);
    St<NCH> y = conj<NCH>(L, x, ks[repSym[kr]]);
    store<NCH>(y, b);
    uint64_t c = repClass[kr];
    if (yout) *yout = y;
    if (cls) *cls = c;
    return c * restSize + restRank(b);
  }
  inline int get(uint64_t i) const { return (tab[i >> 1] >> ((i & 1) * 4)) & 15; }
  // 2-bit encoding: 0 = value <= base2 (unknown below), c = base2 + c (3 saturates: lower bound)
  vector<uint8_t> tab2; int base2 = 0;
  void build2bit(int b) {
    base2 = b; tab2.assign((size + 3) / 4, 0);
    for (uint64_t i = 0; i < size; i++) { int v = get(i); int c = v <= b ? 0 : min(v - b, 3); tab2[i >> 2] |= c << ((i & 3) * 2); }
  }
  inline int get2(uint64_t i) const { return (tab2[i >> 2] >> ((i & 3) * 2)) & 3; }
  inline bool setIfUnknown(uint64_t i, int v) {
    uint32_t *w = (uint32_t *)&tab[(i >> 3) << 2];
    int sh = (i & 7) * 4;
    uint32_t old = __atomic_load_n(w, __ATOMIC_RELAXED);
    while (((old >> sh) & 15) == 15) {
      uint32_t nw = (old & ~(15u << sh)) | ((uint32_t)v << sh);
      if (__atomic_compare_exchange_n(w, &old, nw, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) return true;
    }
    return false;
  }
  // set the entry for conjugated state y (in class c) and all of its twins
  uint64_t setWithTwins(const St<NCH> &y, uint64_t c, uint64_t idx, int v) {
    uint64_t n = setIfUnknown(idx, v);
    for (uint8_t t : stab[c]) {
      uint8_t b[KMAX]; store<NCH>(conj<NCH>(L, y, ks[t]), b);
      n += setIfUnknown(c * restSize + restRank(b), v);
    }
    return n;
  }
  struct NoFast {};
  // call f(i) for every entry i in [lo, hi) whose value is v (lo multiple of 16), 16 nibbles per word
  template <class F> inline void forEachEq(uint64_t lo, uint64_t hi, int v, F &&f) const {
    const uint64_t pat = 0x1111111111111111ULL * v, M7 = 0x7777777777777777ULL;
    for (uint64_t b = lo; b < hi; b += 16) {
      uint64_t w; memcpy(&w, &tab[b >> 1], 8);
      uint64_t y = w ^ pat;
      uint64_t z = ~(((y & M7) + M7) | y | M7);   // high bit of each zero nibble
      while (z) {
        uint64_t i = b + (__builtin_ctzll(z) >> 2);
        z &= z - 1;
        if (i < hi) f(i);
      }
    }
  }
  int forceDir = 0; double bwdRatio = getenv("BWDRATIO") ? atof(getenv("BWDRATIO")) : 1.0;   // 0 auto, 1 forward, 2 backward
  template <class FI = NoFast> void fill(int nthreads, bool verbose = true, const FI *fi = nullptr) {
    tab.assign(((size + 15) / 16) * 8 + 8, 0xFF);   // padded for 64-bit scanning
    St<NCH> s0; { uint8_t b[KMAX]; L.pack(pz.ident(), b); s0 = load<NCH>(b); }
    St<NCH> y; uint64_t c;
    uint64_t i0 = index(s0, &y, &c);
    uint64_t total = setWithTwins(y, c, i0, 0);
    vector<uint64_t> lastNew{total};
    vector<KElem> km; for (auto &m : pz.moves) km.push_back(L.prep(pz, m.e));
    uint8_t idb[KMAX]; L.pack(pz.ident(), idb);
    for (int d = 0; d < 14; d++) {
      atomic<uint64_t> cnt{0}, nexp{0}, nslow{0}, uins{0};
      PerfSnap ps = perfNow();
      vector<thread> th;
      uint64_t chunk = ((size + nthreads - 1) / nthreads + 15) & ~15ULL;
      // forward (expand depth-d entries) or backward (scan unknown entries for a depth-d
      // neighbor, first hit wins).  Backward needs no twin handling: every twin entry is
      // itself an unknown entry and finds the same neighbors (conjugated).
      uint64_t frontier = 0; for (auto f : lastNew) frontier += f;
      bool backward = forceDir == 2;
      if (forceDir == 0) {
        // cost model in "child evaluations": forward = frontier * (1 + nmoves);
        // backward = unknown * (1 + E[checks until a depth-d neighbor]), E sampled.
        uint64_t unknown = size - total;
        mt19937_64 rng(d * 7919 + 1);
        uint64_t found = 0, checks = 0; uint8_t st[KMAX];
        for (int tries = 0; tries < 200000 && found < 2000; tries++) {
          uint64_t i = rng() % size;
          if (get(i) != 15) continue;
          found++;
          memcpy(st, idb, KMAX); keyUnrank(classRep[i / restSize], st); restUnrank(i % restSize, st);
          St<NCH> x = load<NCH>(st);
          for (auto &m : km) {
            checks++;
            St<NCH> ch = mulc<NCH>(L, x, m);
            uint64_t ci; if constexpr (!is_same_v<FI, NoFast>) ci = fi->index(ch); else ci = index(ch);
            if (get(ci) == d) break;
          }
        }
        double E = found ? (double)checks / found : km.size();
        double fwd = frontier * (1.0 + km.size()), bwd = unknown * (1.0 + E);
        backward = bwd < fwd;
        if (verbose) printf("  depth %2d: frontier %llu unknown %llu  E[checks] %.2f  -> %s\n", d + 1, (unsigned long long)frontier,
                            (unsigned long long)unknown, E, backward ? "backward" : "forward");
      }
      if (backward) {
        for (int t = 0; t < nthreads; t++) th.emplace_back([&, t] {
          uint64_t lo = t * chunk, hi = min(size, lo + chunk), local = 0, lexp = 0;
          uint8_t st[KMAX];
          uint64_t lastCl = ~0ULL;
          uint8_t kst[KMAX];
          forEachEq(lo, hi, 15, [&](uint64_t i) {
            lexp++;
            uint64_t cl = i / restSize, r = i % restSize;
            if (cl != lastCl) { memcpy(kst, idb, KMAX); keyUnrank(classRep[cl], kst); lastCl = cl; }
            memcpy(st, kst, KMAX);
            restUnrank(r, st);
            St<NCH> x = load<NCH>(st);
            for (auto &m : km) {
              St<NCH> ch = mulc<NCH>(L, x, m);
              uint64_t ci;
              if constexpr (!is_same_v<FI, NoFast>) ci = fi->index(ch); else ci = index(ch);
              if (get(ci) == d) { local += setIfUnknown(i, d + 1); break; }
            }
          });
          cnt += local; nexp += lexp;
        });
      } else
      for (int t = 0; t < nthreads; t++) th.emplace_back([&, t] {
        uint64_t lo = t * chunk, hi = min(size, lo + chunk), local = 0, lexp = 0, lslow = 0;
        uint8_t st[KMAX], kst[64]; uint64_t lastCl = ~0ULL;
        forEachEq(lo, hi, d, [&](uint64_t i) {
          lexp++;
          uint64_t cl = i / restSize, r = i % restSize;
          if (cl != lastCl) { memcpy(kst, idb, KMAX); keyUnrank(classRep[cl], kst); lastCl = cl; }
          memcpy(st, kst, KMAX);
          restUnrank(r, st);
          St<NCH> x = load<NCH>(st);
          for (auto &m : km) {
            St<NCH> ch = mulc<NCH>(L, x, m), yy; uint64_t cc;
            if constexpr (!is_same_v<FI, NoFast>) {
              uint64_t ci = fi->index(ch);
              if (get(ci) != 15) continue;
              if (stab[ci / restSize].empty()) { local += setIfUnknown(ci, d + 1); continue; }
            }
            lslow++;
            uint64_t ci = index(ch, &yy, &cc);
            if (get(ci) == 15) local += setWithTwins(yy, cc, ci, d + 1);
          }
        });
        cnt += local; nexp += lexp; nslow += lslow;
      });
      for (auto &x : th) x.join();
      total += cnt;
      lastNew = {cnt.load()};
      if (verbose) {
        PerfDelta pd = perfSince(ps);
        printf("  %s depth %2d: %12llu new (total %llu of %llu)  expanded %llu  slow %llu  Ginstr %.1f  instr/expanded %.0f  %.1fs\n",
               backward ? "bwd" : "fwd", d + 1, (unsigned long long)cnt.load(), (unsigned long long)total, (unsigned long long)size, (unsigned long long)nexp.load(),
               (unsigned long long)nslow.load(), pd.ins / 1e9, pd.ins / max<double>(1, nexp.load()), pd.sec);
        fflush(stdout);
      }
      if (cnt == 0) break;
    }
  }
};
