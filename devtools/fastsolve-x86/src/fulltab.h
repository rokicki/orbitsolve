// Full-state near-goal distance table (generic; twsearch-style but symmetry reduced).
//
// States are colour states: each position holds the code of the solved label of the
// piece sitting there (dense over (orbit, label)), so identical pieces collapse and
// the search never needs inverses.  Moves act on colour states exactly as on
// permutation states (same TBL kernels).
//
// Symmetry: every symmetry S acts on colour states as  c'[j] = sigma_S(c[S.p[j]])
// (conjugation; sigma_S relabels colours).  Canonicalization is two-stage and
// derived automatically from the group's action on colour codes:
//   1. a feature: the position set (k of n <= 32 positions) of colour code w, for
//      every w in one G-orbit Omega of colour codes; precomputed per w and per
//      colex rank: the minimum feature value over the symmetries that carry w to
//      the target code, and the set of symmetries attaining it;
//   2. full gather + AES hash only for those few symmetries; minimum wins.
//
// Table: hashed slots of 2 bits, min-combining (collisions only lower values):
// exact BFS layers to depth E (packed canonical keys, sorted and deduplicated per
// hash bucket), then DFS one and two levels from layer E.  Stored value s = 3 - code,
// code 0 = <= E, 1 = E+1, 2 = E+2, 3 = deeper (s = 0: zero pages = unset).
// FTLEV=3 (2026-11-12): three DFS levels, D = E+3; code 0 = <= E+1 (the exact set
// separates <= E from E+1), 1 = E+2, 2 = E+3 (Z = 1 below; recorded in the .meta).
#pragma once
#include "kern.h"
// table hash/format version, part of the shared-memory fingerprint
#ifndef FS_TABVER
#if defined(__aarch64__)
#define FS_TABVER "arm1"
#else
#define FS_TABVER "x86v3"   // v3: 1-chunk raw partial key, uint16 symmetry ids (no 255 cap)
#endif
#endif
#include "symm.h"
#include "perf.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <array>
#include <sys/mman.h>
#include <fcntl.h>
#ifndef MAP_POPULATE
#define MAP_POPULATE 0
#endif
#include <unistd.h>
#include <chrono>
#include <cmath>
#include <memory>

template <int NCH> static inline uint64_t aesHash(const St<NCH> &a) {
  uint8x16_t h = veorq_u8(a.c[0], vdupq_n_u8(0x3c));
  for (int i = 1; i < NCH; i++) h = aesMix(h, a.c[i]);
  h = aesMix(h, vdupq_n_u8(0xa5));
  h = aesMix(h, vdupq_n_u8(0x96));
  uint64x2_t v = vreinterpretq_u64_u8(h);
  return vgetq_lane_u64(v, 0) ^ (vgetq_lane_u64(v, 1) * 0x9E3779B97F4A7C15ULL);
}
static inline double nowSec() { return chrono::duration<double>(chrono::steady_clock::now().time_since_epoch()).count(); }

template <int NCH> struct ColorSpace {
  const Puzzle &pz; const Layout &L;
  alignas(64) uint8_t colorTab[KMAX];
  int ncol = 0; vector<int> colOrbit, colLabel, colCount;
  St<NCH> solved;
  ColorSpace(const Puzzle &p, const Layout &l) : pz(p), L(l) {
    map<pair<int, int>, int> code;
    memset(colorTab, 0, sizeof colorTab);
    for (int q = 0; q < pz.N; q++) {
      auto k = make_pair(pz.orbitOf[q], (int)pz.solvedLabel[q]);
      if (!code.count(k)) { int c = code.size(); code[k] = c; colOrbit.push_back(k.first); colLabel.push_back(k.second); colCount.push_back(0); }
      colorTab[q] = code[k]; colCount[code[k]]++;
    }
    ncol = code.size();
    uint8_t b[KMAX]; L.pack(pz.ident(), b); solved = of(load<NCH>(b));
  }
  inline St<NCH> of(const St<NCH> &x) const {
    St<NCH> r; const uint8x16_t PM = vdupq_n_u8(L.posmask), OM = vdupq_n_u8(L.orimask);
    for (int i = 0; i < NCH; i++) r.c[i] = vorrq_u8(tblK<(NCH > 8 ? 9 : NCH > 4 ? 8 : 4)>(colorTab, vandq_u8(x.c[i], PM)), vandq_u8(x.c[i], OM));
    return r;
  }
};

template <int NCH> struct Canon {
  const ColorSpace<NCH> &C; const Layout &L; const Puzzle &pz;
  struct Sy { St<NCH> g; alignas(16) uint8_t sig[64]; bool sigId; GPlan gp; };
  vector<Sy> sy;
  int sigCh = 4;   // 16-byte chunks of sig in use (colour codes < C.ncol)
  inline uint8x16_t sigMap(const Sy &S, uint8x16_t y) const {
#if defined(__aarch64__)
    uint8x16x4_t T = {vld1q_u8(S.sig), vld1q_u8(S.sig + 16), vld1q_u8(S.sig + 32), vld1q_u8(S.sig + 48)};
    return vqtbl4q_u8(T, y);
#else
    return U_(tblX((const __m128i *)S.sig, sigCh, M_(y)));
#endif
  }
  // feature stage
  bool feat = false; int fn = 0, fk = 0;
  vector<int> omega;              // colour codes (Omega)
  vector<int> wOff, wChunk0, wNch; // per omega element: orbit offset, first chunk, chunks spanned
  vector<uint32_t> minR; vector<uint16_t> setOf;   // [w * nr + rank]
  vector<vector<uint16_t>> sets;   // symmetry ids (uint16: obl8op has 384 symmetries)
  vector<uint32_t> setStart; vector<uint16_t> setFlat;   // flattened sets: [setStart[i], setStart[i+1])
  uint32_t nr = 0; int nbytes = 0;
  int pc0 = NCH > 1 ? 1 : 0, pc1 = NCH > 2 ? 2 : 0;   // partial-key chunks
  int pcx = NCH > 1 ? 1 : 0;   // x86 partial key: this one chunk (env PCX overrides)
  mutable long long dbgFull = 0, dbgCalls = 0;
  // batched movemasks: (chunk, code) pairs, groups of 8 through a pairwise-add tree
  vector<int> mmChunk, mmCode; vector<array<int, 3>> wMM;   // per omega element: indices into the pair list (-1 none)
  uint8_t pc8[256];
  vector<uint32_t> cx;            // [t][cnt][byte]  colex contributions

  Canon(const ColorSpace<NCH> &c, const Layout &l, const Puzzle &p, const vector<Elem> &syms, bool verbose = true) : C(c), L(l), pz(p) {
    bool anyOri = false; for (auto &o : pz.orbits) if (o.omod > 1) anyOri = true;
    vector<Elem> use, kept;
    if (anyOri) { use.push_back(pz.ident()); if (verbose) printf("canon: orientation present, colour-state symmetry not supported yet: identity only\n"); }
    else use = syms;
    for (auto &S : use) {
      Elem Si = pz.inv(S);
      Sy s; uint8_t g[KMAX];
      for (int j = 0; j < KMAX; j++) g[j] = j < pz.N ? S.p[j] : j;
      s.g = load<NCH>(g); s.gp.make(g);
      // sigma: colour(Si.p[q]) = sigma(colour(q))
      vector<int> sg(64, -1); bool ok = true;
      for (int q = 0; q < pz.N; q++) {
        int a = C.colorTab[q], b = C.colorTab[Si.p[q]];
        if (sg[a] < 0) sg[a] = b; else if (sg[a] != b) ok = false;
      }
      if (!ok) continue;
      s.sigId = true;
      for (int a = 0; a < 64; a++) { s.sig[a] = sg[a] < 0 ? a : sg[a]; if (s.sig[a] != a) s.sigId = false; }
      sy.push_back(s); kept.push_back(S);
    }
    if (sy.empty()) { Sy s; uint8_t g[KMAX]; for (int j = 0; j < KMAX; j++) g[j] = j; s.g = load<NCH>(g); s.gp.make(g); s.sigId = true; for (int a = 0; a < 64; a++) s.sig[a] = a; sy.push_back(s); }
    // (2026-11-11) no cap: the old resize(255) for uint8 ids cut obl8op's 384-element group to a non-group,
    // so symmetric states could canonicalize differently
    if (sy.size() > 1024) { fprintf(stderr, "canon: %zu symmetries, more than 1024\n", sy.size()); exit(1); }
    sigCh = max(1, (C.ncol + 15) / 16);
    if (getenv("PCX")) pcx = min(NCH - 1, atoi(getenv("PCX")));
    // verify against kernel conjugation on random permutation states
    {
      vector<KElem> km; for (auto &m : pz.moves) km.push_back(L.prep(pz, m.e));
      uint8_t b[KMAX]; L.pack(pz.ident(), b); St<NCH> x = load<NCH>(b);
      unsigned r = 12345; long bad = 0;
      size_t si = 0;
      for (auto &S : kept) {
        if (si >= sy.size()) break;
        KSym ks = prepSym(pz, L, S);
        for (int it = 0; it < 20; it++) { r = r * 1103515245 + 12345; x = mulc<NCH>(L, x, km[(r >> 8) % km.size()]); }
        if (!eqSt<NCH>(C.of(conj<NCH>(L, x, ks)), apply(C.of(x), si))) bad++;
        si++;
      }
      if (bad) { fprintf(stderr, "canon: colour-state symmetry action mismatch (%ld)\n", bad); exit(1); }
    }
    buildFeature(verbose);
    { // self-check (2026-11-11): canon must be invariant under every symmetry (catches a non-group symmetry list)
      vector<KElem> km; for (auto &m : pz.moves) km.push_back(L.prep(pz, m.e));
      St<NCH> x = C.solved; unsigned r = 4321; long bad = 0;
      for (int it = 0; it < 300; it++) {
        for (int k = 0; k < 7; k++) { r = r * 1103515245 + 12345; x = mulc<NCH>(L, x, km[(r >> 8) % km.size()]); }
        uint64_t h = canon<false>(x);
        for (size_t s = 0; s < sy.size(); s += 1 + it % 3) if (canon<false>(apply(x, s)) != h) bad++;
      }
      if (bad) { fprintf(stderr, "canon: not symmetry-invariant (%ld mismatches)\n", bad); exit(1); }
    }
  }
  inline St<NCH> apply(const St<NCH> &c, int s) const {
    const Sy &S = sy[s]; St<NCH> y;
    for (int i = 0; i < NCH; i++) y.c[i] = gatherP<NCH>(c, S.gp, (const uint8_t *)S.g.c, i);
    if (!S.sigId) for (int i = 0; i < NCH; i++) y.c[i] = sigMap(S, y.c[i]);
    return y;
  }
  inline uint32_t colex(uint32_t m) const {
    const uint32_t *T = cx.data(); const int st = (fk + 1) * 256;
    uint32_t b0 = m & 255, b1 = (m >> 8) & 255, b2 = (m >> 16) & 255, b3 = m >> 24;
    uint32_t r = T[b0], c = pc8[b0];
    if (nbytes > 1) { r += T[st + c * 256 + b1]; c += pc8[b1]; }
    if (nbytes > 2) { r += T[2 * st + c * 256 + b2]; c += pc8[b2]; }
    if (nbytes > 3) r += T[3 * st + c * 256 + b3];
    return r;
  }
  // all feature masks at once
  alignas(16) uint8_t mmCodeV[32][16];
  inline void masks(const St<NCH> &c, uint32_t *out) const {
#if !defined(__aarch64__)
    { // x86: one PMOVMSKB per (chunk, code) pair instead of the emulated pairwise-add tree
      alignas(16) uint16_t mk[32];
      const int np = mmChunk.size(); const int *ch = mmChunk.data();
      for (int g = 0; g < np; g++) mk[g] = (uint16_t)_mm_movemask_epi8(_mm_cmpeq_epi8(M_(c.c[ch[g]]), _mm_load_si128((const __m128i *)mmCodeV[g])));
      const size_t nw = omega.size();
      for (size_t w = 0; w < nw; w++) {
        const auto &ix = wMM[w];
        uint64_t acc = mk[ix[0]] | (uint64_t)mk[ix[1]] << 16 | (uint64_t)mk[ix[2]] << 32;
        out[w] = (uint32_t)((acc >> wShift[w]) & fullMask);
      }
      return;
    }
#endif
    static const uint8_t bw[16] = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
    const uint8x16_t W = vld1q_u8(bw);
    alignas(16) uint8_t buf[16 * NCH]; store<NCH>(c, buf);
    alignas(16) uint16_t mk[32];
    const int np = mmChunk.size();
    const int *ch = mmChunk.data();
    for (int g = 0; g < np; g += 8) {
      uint8x16_t v[8];
#pragma clang loop unroll(full)
      for (int i = 0; i < 8; i++) v[i] = vandq_u8(vceqq_u8(vld1q_u8(buf + 16 * ch[g + i]), vld1q_u8(mmCodeV[g + i])), W);
      uint8x16_t a = vpaddq_u8(vpaddq_u8(vpaddq_u8(v[0], v[1]), vpaddq_u8(v[2], v[3])), vpaddq_u8(vpaddq_u8(v[4], v[5]), vpaddq_u8(v[6], v[7])));
      vst1q_u8((uint8_t *)(mk + g), a);
    }
    const size_t nw = omega.size();
    for (size_t w = 0; w < nw; w++) {
      const auto &ix = wMM[w];
      uint64_t acc = mk[ix[0]] | (uint64_t)mk[ix[1]] << 16 | (uint64_t)mk[ix[2]] << 32;
      out[w] = (uint32_t)((acc >> wShift[w]) & fullMask);
    }
  }
  vector<int> wShift; uint64_t fullMask = 0;
  // hash of the conjugate by symmetry s, gather fused into the hash chain
  inline uint64_t symHash(const St<NCH> &c, int s) const {
    const Sy &S = sy[s];
    auto gat = [&](int i) {
      uint8x16_t y = gatherP<NCH>(c, S.gp, (const uint8_t *)S.g.c, i);
      if (!S.sigId) y = sigMap(S, y);
      return y;
    };
    uint8x16_t h = veorq_u8(gat(0), vdupq_n_u8(0x3c));
#pragma clang loop unroll(full)
    for (int i = 1; i < NCH; i++) h = aesMix(h, gat(i));
    h = aesMix(h, vdupq_n_u8(0xa5));
    h = aesMix(h, vdupq_n_u8(0x96));
    uint64x2_t v = vreinterpretq_u64_u8(h);
    return vgetq_lane_u64(v, 0) ^ (vgetq_lane_u64(v, 1) * 0x9E3779B97F4A7C15ULL);
  }
  inline uint32_t maskOf(const St<NCH> &c, int w) const {
    uint8x16_t v = vdupq_n_u8(omega[w]);
    uint64_t acc = 0; int c0 = wChunk0[w];
    for (int i = 0; i < wNch[w]; i++) acc |= (uint64_t)movemask16(vceqq_u8(c.c[c0 + i], v)) << (16 * i);
    return (uint32_t)((acc >> (wOff[w] - 16 * c0)) & ((1ULL << fn) - 1));
  }
  void buildFeature(bool verbose) {
    int G = sy.size();
    if (G <= 2) return;
    // action of each symmetry on colour codes: target code t receives source code src(s, t)
    // (sigma maps source colour to target colour); Omega = orbits of codes under the group
    int nc = C.ncol;
    vector<vector<int>> srcOf(G, vector<int>(nc, -1));
    for (int s = 0; s < G; s++) for (int a = 0; a < nc; a++) srcOf[s][sy[s].sig[a]] = a;
    vector<int> comp(nc, -1);
    for (int a = 0; a < nc; a++) if (comp[a] < 0) {
      vector<int> st{a}; comp[a] = a;
      while (!st.empty()) { int x = st.back(); st.pop_back(); for (int s = 0; s < G; s++) { int y = sy[s].sig[x]; if (comp[y] < 0) { comp[y] = a; st.push_back(y); } } }
    }
    double bestScore = 0; int bestRoot = -1;
    for (int a = 0; a < nc; a++) if (comp[a] == a) {
      int o = C.colOrbit[a], n = pz.orbits[o].n, k = C.colCount[a];
      if (n > 32 || k == 0 || k == n) continue;
      double sz = 1; for (int i = 0; i < k; i++) sz = sz * (n - i) / (i + 1);
      int nw = 0; for (int b = 0; b < nc; b++) if (comp[b] == a) nw++;
      if (sz * nw > (1 << 19) || nw > 64) continue;
      if (sz > bestScore) { bestScore = sz; bestRoot = a; }
    }
    if (bestRoot < 0) { if (verbose) printf("canon: no feature, brute force over %d symmetries\n", G); return; }
    for (int b = 0; b < nc; b++) if (comp[b] == bestRoot) omega.push_back(b);
    int c0 = omega[0];
    fn = pz.orbits[C.colOrbit[c0]].n; fk = C.colCount[c0];
    nr = (uint32_t)bestScore;
    // colex contributions
    cx.assign(4 * (fk + 1) * 256, 0);
    auto binom = [](int n, int k) -> uint64_t { if (k < 0 || k > n) return 0; uint64_t r = 1; for (int i = 0; i < k; i++) r = r * (n - i) / (i + 1); return r; };
    for (int t = 0; t < 4; t++) for (int cnt = 0; cnt <= fk; cnt++) for (int b = 0; b < 256; b++) {
      uint32_t r = 0; int c = cnt;
      for (int i = 0; i < 8; i++) if ((b >> i) & 1) { c++; r += binom(8 * t + i, c); }
      cx[(t * (fk + 1) + cnt) * 256 + b] = r;
    }
    for (int i = 0; i < 256; i++) pc8[i] = __builtin_popcount(i);
    nbytes = (fn + 7) / 8;
    for (int w : omega) {
      int o = C.colOrbit[w]; wOff.push_back(pz.orbits[o].off);
      wChunk0.push_back(pz.orbits[o].off / 16);
      wNch.push_back((pz.orbits[o].off + fn - 1) / 16 - pz.orbits[o].off / 16 + 1);
      array<int, 3> ix = {-1, -1, -1};
      for (int i = 0; i < wNch.back(); i++) { ix[i] = mmChunk.size(); mmChunk.push_back(wChunk0.back() + i); mmCode.push_back(w); }
      wMM.push_back(ix);
    }
    if (mmChunk.size() > 32) { omega.clear(); wMM.clear(); mmChunk.clear(); if (verbose) printf("canon: feature too wide, brute force\n"); return; }
    // pad the pair list to a multiple of 8 (extra pairs read chunk 0, unused); index -1 -> a zero slot
    int real = mmChunk.size();
    while (mmChunk.size() % 8 || (int)mmChunk.size() == real) { mmChunk.push_back(0); mmCode.push_back(255); }
    for (size_t i = 0; i < mmChunk.size(); i++) memset(mmCodeV[i], mmCode[i], 16);
    for (auto &ix : wMM) for (auto &v : ix) if (v < 0) v = real;   // mmCode 255 never matches: zero mask
    for (size_t w = 0; w < omega.size(); w++) wShift.push_back(wOff[w] - 16 * wChunk0[w]);
    fullMask = (1ULL << fn) - 1;
    // per symmetry: source code (carried to c0) and local position map target t <- source u
    int o0 = C.colOrbit[c0], off0 = pz.orbits[o0].off;
    vector<int> srcW(G, -1); vector<vector<int>> tOfU(G, vector<int>(fn));
    uint8_t g[KMAX];
    for (int s = 0; s < G; s++) {
      int sc = srcOf[s][c0];
      int w = find(omega.begin(), omega.end(), sc) - omega.begin();
      srcW[s] = w;
      store<NCH>(sy[s].g, g);
      int offS = pz.orbits[C.colOrbit[sc]].off;
      for (int t = 0; t < fn; t++) tOfU[s][g[off0 + t] - offS] = t;
    }
    int nw = omega.size();
    minR.assign((size_t)nw * nr, 0); setOf.assign((size_t)nw * nr, 0);
    map<vector<uint16_t>, int> setId;
    for (int w = 0; w < nw; w++) {
      vector<int> ss; for (int s = 0; s < G; s++) if (srcW[s] == w) ss.push_back(s);
      if (ss.empty()) continue;
      uint32_t m = (1u << fk) - 1;
      for (uint32_t r = 0; r < nr; r++) {
        uint32_t mn = ~0u; vector<uint16_t> arg;
        for (int s : ss) {
          uint32_t im = 0; for (uint32_t x = m; x; x &= x - 1) im |= 1u << tOfU[s][__builtin_ctz(x)];
          uint32_t ir = colex(im);
          if (ir < mn) { mn = ir; arg.clear(); }
          if (ir == mn) arg.push_back(s);
        }
        if (colex(m) != r) { fprintf(stderr, "colex order mismatch\n"); exit(1); }
        auto it = setId.find(arg);
        int id; if (it == setId.end()) { id = sets.size(); setId[arg] = id; sets.push_back(arg); } else id = it->second;
        minR[(size_t)w * nr + r] = mn; setOf[(size_t)w * nr + r] = id;
        // next k-subset (Gosper)
        uint32_t c = m & -m, rr = m + c; m = (((rr ^ m) >> 2) / c) | rr;
      }
    }
    setStart.push_back(0);
    for (auto &st : sets) { setFlat.insert(setFlat.end(), st.begin(), st.end()); setStart.push_back(setFlat.size()); }
    feat = true;
    if (verbose) printf("canon: %d symmetries; feature = %d-of-%d positions of %zu colour codes (%u ranks, %zu symmetry sets)\n",
                        G, fk, fn, omega.size(), nr, sets.size());
  }
  // average number of full evaluations per canonicalization on given states
  double candCount(const St<NCH> &c) const {
    if (!feat) return sy.size();
    uint32_t mn = ~0u; double n = 0;
    for (size_t w = 0; w < omega.size(); w++) {
      size_t e = (size_t)w * nr + colex(maskOf(c, w));
      if (minR[e] < mn) { mn = minR[e]; n = 0; }
      if (minR[e] == mn) n += sets[setOf[e]].size();
    }
    return n;
  }
  // canonicalization statistics for one state: o[0] += feature candidates, o[1] += left after the
  // partial stage, o[2] += distinct images among those (equal images = stabilizer)
  void stat(const St<NCH> &c, long long *o) const {
    int cand[1024], nc = 0;
    if (!feat) { for (size_t s = 0; s < sy.size(); s++) cand[nc++] = s; }
    else {
      uint32_t mn = ~0u; int ids[64], ni = 0; uint32_t mk[64]; masks(c, mk);
      for (size_t w = 0; w < omega.size(); w++) { size_t e = (size_t)w * nr + colex(mk[w]); uint32_t v = minR[e]; if (v < mn) { mn = v; ni = 0; } if (v == mn) ids[ni++] = setOf[e]; }
      for (int i = 0; i < ni; i++) for (uint32_t j = setStart[ids[i]]; j < setStart[ids[i] + 1]; j++) cand[nc++] = setFlat[j];
    }
    o[0] += nc;
    uint64_t ph[1024], pl[1024], pm = ~0ULL, ml = ~0ULL; int keep = 0;
    for (int i = 0; i < nc; i++) {   // the x86 partial key (one chunk, raw)
      const Sy &S = sy[cand[i]];
      uint8x16_t a = gatherP<NCH>(c, S.gp, (const uint8_t *)S.g.c, pcx);
      if (!S.sigId) a = sigMap(S, a);
      uint64_t w[2]; memcpy(w, &a, 16); ph[i] = w[1]; pl[i] = w[0];
      if (w[1] < pm || (w[1] == pm && w[0] < ml)) { pm = w[1]; ml = w[0]; }
    }
    for (int i = 0; i < nc; i++) if (ph[i] == pm && pl[i] == ml) cand[keep++] = cand[i];
    o[1] += keep;
    vector<St<NCH>> im;
    for (int i = 0; i < keep; i++) { St<NCH> y = apply(c, cand[i]); bool dup = false; for (auto &u : im) if (eqSt<NCH>(u, y)) dup = true; if (!dup) im.push_back(y); }
    o[2] += im.size();
  }
  // canonical hash (and optionally the canonical form: minimum (hash, bytes))
  // feature stage: candidate symmetries (or all)
  inline int features(const St<NCH> &c, int *cand) const {
    int nc = 0;
    if (!feat) { for (size_t s = 0; s < sy.size(); s++) cand[nc++] = s; }
    else {
      uint32_t mn = ~0u; int ids[64], ni = 0; uint32_t mk[64];
      masks(c, mk);
      for (size_t w = 0; w < omega.size(); w++) {
        size_t e = (size_t)w * nr + colex(mk[w]);
        uint32_t v = minR[e];
        if (v < mn) { mn = v; ni = 0; }
        if (v == mn) ids[ni++] = setOf[e];
      }
      for (int i = 0; i < ni; i++) for (uint32_t j = setStart[ids[i]]; j < setStart[ids[i] + 1]; j++) cand[nc++] = setFlat[j];
    }
    return nc;
  }
  template <bool FORM> inline uint64_t canon(const St<NCH> &c, St<NCH> *form = nullptr) const {
    int cand[1024], nc = features(c, cand);
    // partial stage: two gathered chunks, one AES round
    if (nc > 1) {
      uint64_t pm = ~0ULL; int keep = 0; uint64_t ph[1024];
#if defined(__aarch64__)
      for (int i = 0; i < nc; i++) {
        const Sy &S = sy[cand[i]];
        uint8x16_t a = gatherP<NCH>(c, S.gp, (const uint8_t *)S.g.c, pc0), b = gatherP<NCH>(c, S.gp, (const uint8_t *)S.g.c, pc1);
        if (!S.sigId) { a = sigMap(S, a); b = sigMap(S, b); }
        uint64x2_t v = vreinterpretq_u64_u8(aesMix(a, b));
        ph[i] = vgetq_lane_u64(v, 0) ^ vgetq_lane_u64(v, 1);
        if (ph[i] < pm) pm = ph[i];
      }
      for (int i = 0; i < nc; i++) if (ph[i] == pm) cand[keep++] = cand[i];
#else
      // x86 (2026-11-11): one gathered chunk as a raw 128-bit key, no AES (the canonical choice may differ
      // from the ARM build; tables are per binary)
      uint64_t pl[1024], ml = ~0ULL;
      for (int i = 0; i < nc; i++) {
        const Sy &S = sy[cand[i]];
        uint8x16_t a = gatherP<NCH>(c, S.gp, (const uint8_t *)S.g.c, pcx);
        if (!S.sigId) a = sigMap(S, a);
        __m128i x = M_(a); uint64_t hi = (uint64_t)_mm_extract_epi64(x, 1), lo = (uint64_t)_mm_cvtsi128_si64(x);
        ph[i] = hi; pl[i] = lo;
        if (hi < pm || (hi == pm && lo < ml)) { pm = hi; ml = lo; }
      }
      for (int i = 0; i < nc; i++) if (ph[i] == pm && pl[i] == ml) cand[keep++] = cand[i];
#endif
      nc = keep;
#ifdef CANON_DEBUG
      dbgFull += nc; dbgCalls++;
#endif
    }
    uint64_t best = ~0ULL; St<NCH> bf;
    for (int i = 0; i < nc; i++) {
      int s = cand[i];
      if constexpr (FORM) {
        St<NCH> y = apply(c, s); uint64_t h = aesHash<NCH>(y);
        if (h < best || (h == best && cmpSt<NCH>(y, bf) < 0)) { best = h; bf = y; }
      } else { uint64_t h = symHash(c, s); if (h < best) best = h; }
    }
    if (FORM) *form = bf;
    return best;
  }
};

template <int NCH> struct FullTab {
  const Puzzle &pz; const Layout &L; const ColorSpace<NCH> &C; const Canon<NCH> &K;
  vector<KElem> km; vector<int> base; vector<uint64_t> canonAfter;  // [base+1] move mask
  uint8_t *tab = nullptr; uint64_t M = 0, bytes = 0;
  int E = -1, D = -1;
  // packed keys: per position, local colour code in the orbit's bit width
  vector<int> bitPos, bitW, localOf; vector<vector<int>> codeOf; int totalBits = 0;
  FullTab(const Puzzle &p, const Layout &l, const ColorSpace<NCH> &c, const Canon<NCH> &k, const vector<uint64_t> &canonMask)
      : pz(p), L(l), C(c), K(k), canonAfter(canonMask) {
    for (auto &m : pz.moves) { km.push_back(L.prep(pz, m.e)); base.push_back(m.base); }
    localOf.assign(C.ncol, 0); codeOf.assign(pz.orbits.size(), {});
    for (int a = 0; a < C.ncol; a++) { localOf[a] = codeOf[C.colOrbit[a]].size(); codeOf[C.colOrbit[a]].push_back(a); }
    for (int j = 0; j < pz.N; j++) {
      int nl = codeOf[pz.orbitOf[j]].size(), w = 0; while ((1 << w) < nl) w++;
      bitPos.push_back(totalBits); bitW.push_back(w); totalBits += w;
    }
  }
  ~FullTab() { if (tab) munmap(tab, bytes); }
  inline uint64_t slot(uint64_t h) const { return (uint64_t)(((__uint128_t)h * M) >> 64); }
  inline int code(uint64_t i) const { return 3 - ((tab[i >> 2] >> ((i & 3) * 2)) & 3); }
  inline void pf(uint64_t i) const { __builtin_prefetch(tab + (i >> 2)); }
  inline bool setIfUnset(uint64_t i, int c) {
    uint8_t *b = tab + (i >> 2); int sh = (i & 3) * 2;
    if ((*b >> sh) & 3) return false;
    __atomic_fetch_or(b, (uint8_t)((3 - c) << sh), __ATOMIC_RELAXED);
    return true;
  }
  // Fill queue (twsearch-style): per thread, per memory shard buffers of slots; a full
  // buffer is flushed under the shard's lock with plain writes and prefetch-ahead.
  // Shards are small (~16 MB) so a flush stays within a TLB-sized region.
  int shShift = 0, nShard = 0; static constexpr int QN = 256;
  struct alignas(64) Q { vector<uint64_t> buf; vector<uint16_t> cnt; long long news = 0; };  // own cache lines (false sharing)
  vector<Q> qs; unique_ptr<atomic<bool>[]> locks;
  void qInit(int nth) {
    shShift = 26; while (((M - 1) >> shShift) >= 4096) shShift++;   // 64M slots = 16 MB per shard
    nShard = ((M - 1) >> shShift) + 1;
    qs.assign(nth, {}); for (auto &q : qs) { q.buf.assign((size_t)nShard * QN, 0); q.cnt.assign(nShard, 0); }
    locks.reset(new atomic<bool>[nShard]); for (int i = 0; i < nShard; i++) locks[i] = false;
  }
  void qFlush(int t, int b, int c) {
    Q &q = qs[t]; int n = q.cnt[b]; if (!n) return;
    uint64_t *p = &q.buf[(size_t)b * QN];
    while (locks[b].exchange(true, memory_order_acquire)) while (locks[b].load(memory_order_relaxed)) ;
    const int PD = 16;
    for (int i = 0; i < n && i < PD; i++) __builtin_prefetch(tab + (p[i] >> 2), 1);
    long long nw = 0;
    for (int i = 0; i < n; i++) {
      if (i + PD < n) __builtin_prefetch(tab + (p[i + PD] >> 2), 1);
      uint64_t s = p[i]; uint8_t *bt = tab + (s >> 2); int sh = (s & 3) * 2;
      if (!((*bt >> sh) & 3)) { *bt |= (uint8_t)((3 - c) << sh); nw++; }
    }
    locks[b].store(false, memory_order_release);
    q.news += nw; q.cnt[b] = 0;
  }
  bool noQ = getenv("NOQ") != nullptr;
  long long candTot = 0;
  inline void qPush(int t, uint64_t s, int c) {
    if (noQ) { qs[t].news += setIfUnset(s, c); return; }
    Q &q = qs[t]; int b = s >> shShift;
    q.buf[(size_t)b * QN + q.cnt[b]++] = s;
    if (q.cnt[b] == QN) qFlush(t, b, c);
  }
  long long qDrain(int nth, int c) {
    parFor(nth, nth, [&](int, size_t t) { for (int b = 0; b < nShard; b++) qFlush(t, b, c); });
    long long r = 0; for (auto &q : qs) { r += q.news; q.news = 0; } return r;
  }
  // lower bound from a code (code 0: only "<= E": caller supplies a fallback)
  inline int value(int c, int fallback) const { return c == 0 ? fallback : c == 3 ? D + 1 : E + Z + c; }
  // DFS levels below layer E (FTLEV, 2 or 3); Z = levels folded into code 0 (0, or 1 when 3 levels)
  int nLev = getenv("FTLEV") ? atoi(getenv("FTLEV")) : 2, Z = 0;
  // exact distances for canonical states at depth <= E: open addressing on the
  // canonical hash (high 60 bits key, low 4 bits distance); key 0 = empty
  vector<uint64_t> ex; uint64_t exMask = 0;
  inline void exInsert(uint64_t h, int d) {
    uint64_t k = (((h >> 4) | (1ULL << 59)) << 4) | (uint64_t)d;   // never 0
    for (uint64_t i = (h * 0x9E3779B97F4A7C15ULL) >> 20 & exMask;; i = (i + 1) & exMask) {
      if (ex[i] == 0) { ex[i] = k; return; }
      if ((ex[i] >> 4) == (k >> 4)) return;
    }
  }
  // same, safe from many threads at once (compare-and-swap on the empty slot)
  inline void exInsertPar(uint64_t h, int d) {
    uint64_t k = (((h >> 4) | (1ULL << 59)) << 4) | (uint64_t)d;
    for (uint64_t i = (h * 0x9E3779B97F4A7C15ULL) >> 20 & exMask;; i = (i + 1) & exMask) {
      uint64_t v = __atomic_load_n(&ex[i], __ATOMIC_RELAXED);
      if (v == 0) { if (__atomic_compare_exchange_n(&ex[i], &v, k, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) return; }
      if ((v >> 4) == (k >> 4)) return;
    }
  }
  // exact distance if the state is within E (by canonical hash), else -1
  inline int exact(uint64_t h) const {
    uint64_t key = (h >> 4) | (1ULL << 59);
    for (uint64_t i = (h * 0x9E3779B97F4A7C15ULL) >> 20 & exMask;; i = (i + 1) & exMask) {
      uint64_t v = ex[i];
      if (v == 0) return -1;
      if ((v >> 4) == key) return v & 15;
    }
  }
  // lower bound from the canonical hash (code 0 resolved exactly)
  inline int bound(uint64_t h) const {
    int c = code(slot(h));
    if (c) return c == 3 ? D + 1 : E + Z + c;
    int d = exact(h);
    return d >= 0 ? d : E + 1;   // not within E: at least E+1 (exactly E+1 unless a slot collision)
  }
  inline uint64_t slotOf(const St<NCH> &x) const { return slot(K.template canon<false>(x)); }

  template <int W> using Key = array<uint64_t, W>;
  template <int W> inline Key<W> pack(const St<NCH> &c) const {
    uint8_t b[KMAX]; store<NCH>(c, b); Key<W> k{};
    for (int j = 0; j < pz.N; j++) {
      uint64_t v = localOf[b[j]]; int p = bitPos[j];
      k[p >> 6] |= v << (p & 63);
      if ((p & 63) + bitW[j] > 64) k[(p >> 6) + 1] |= v >> (64 - (p & 63));
    }
    return k;
  }
  template <int W> inline St<NCH> unpack(const Key<W> &k) const {
    uint8_t b[KMAX];
    for (int j = 0; j < KMAX; j++) b[j] = 0;
    for (int j = 0; j < pz.N; j++) {
      int p = bitPos[j], w = bitW[j];
      uint64_t v = k[p >> 6] >> (p & 63);
      if ((p & 63) + w > 64) v |= k[(p >> 6) + 1] << (64 - (p & 63));
      b[j] = codeOf[pz.orbitOf[j]][v & ((1u << w) - 1)];
    }
    return load<NCH>(b);
  }

  void alloc(double gb) {
    bytes = (uint64_t)(gb * 1e9); bytes &= ~(uint64_t)16383; M = bytes * 4;
    string p = shmPath();
    if (!p.empty()) {   // FTSHM: build straight into the shared file (no second copy)
      unlink((p + ".meta").c_str());
      int fd = open(p.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
      if (fd < 0 || ftruncate(fd, bytes) != 0) { perror(p.c_str()); exit(1); }
      tab = (uint8_t *)mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); close(fd);
    } else tab = (uint8_t *)mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (tab == MAP_FAILED) { perror("mmap"); exit(1); }
  }
  template <class F> static void parFor(int nth, size_t n, F &&f) {
    atomic<size_t> next{0}; vector<thread> th;
    for (int t = 0; t < nth; t++) th.emplace_back([&, t] { while (true) { size_t i = next++; if (i >= n) break; f(t, i); } });
    for (auto &x : th) x.join();
  }
  // FTSHM=path (2026-11-11): keep the table in a file (on Linux /dev/shm: RAM, survives the process).
  // The build writes path (table, MAP_SHARED), path.ex (exact set) and last path.meta; a later run
  // with the same tws, size and layer budget maps the table and reads the exact set instead of building.
  string fp;              // tws fingerprint (set by the caller)
  string shmPath() const { const char *s = getenv("FTSHM"); return s ? s : ""; }
  string metaWant(double gb, double layerGB) const {
    // levels appear only when not the default 2, so existing 2-level tables still match (and an older
    // binary never maps a 3-level table: its fingerprint line differs)
    char lv[32] = ""; if (nLev != 2) snprintf(lv, sizeof lv, " levels %d", nLev);
    char b[256]; snprintf(b, sizeof b, "fp %s bytes %llu layergb %.3f%s\n", fp.c_str(), (unsigned long long)((uint64_t)(gb * 1e9) & ~(uint64_t)16383), layerGB, lv);
    return b;
  }
  bool shmLoad(double gb, double layerGB, bool verbose) {
    string p = shmPath(); if (p.empty()) return false;
    FILE *f = fopen((p + ".meta").c_str(), "r"); if (!f) return false;
    char line[256] = {0}, line2[256] = {0};
    bool ok = fgets(line, sizeof line, f) && fgets(line2, sizeof line2, f); fclose(f);
    if (!ok || metaWant(gb, layerGB) != line) { if (verbose) printf("fulltab: %s.meta does not match; rebuilding\n", p.c_str()); return false; }
    unsigned long long exn = 0;
    if (sscanf(line2, "E %d D %d exsize %llu Z %d", &E, &D, &exn, &Z) < 3) return false;
    double t = nowSec();
    bytes = (uint64_t)(gb * 1e9) & ~(uint64_t)16383; M = bytes * 4;
    int fd = open(p.c_str(), O_RDONLY); if (fd < 0) { perror(p.c_str()); exit(1); }
    tab = (uint8_t *)mmap(nullptr, bytes, PROT_READ, MAP_SHARED | MAP_POPULATE, fd, 0); close(fd);
    if (tab == MAP_FAILED) { perror("mmap"); exit(1); }
    ex.assign(exn, 0); exMask = exn - 1;
    FILE *g = fopen((p + ".ex").c_str(), "rb");
    if (!g || fread(ex.data(), 8, exn, g) != exn) { fprintf(stderr, "fulltab: %s.ex unreadable\n", p.c_str()); exit(1); }
    fclose(g);
    if (verbose) printf("fulltab: mapped %s (%.1f GB, exact to %d, D=%d) in %.1fs\n", p.c_str(), bytes / 1e9, E, D, nowSec() - t);
    return true;
  }
  void shmSave(double gb, double layerGB, bool verbose) {
    string p = shmPath(); if (p.empty()) return;
    double t = nowSec();
    msync(tab, bytes, MS_SYNC);
    FILE *g = fopen((p + ".ex").c_str(), "wb");
    if (!g || fwrite(ex.data(), 8, ex.size(), g) != ex.size() || fclose(g)) { perror((p + ".ex").c_str()); exit(1); }
    FILE *f = fopen((p + ".meta.tmp").c_str(), "w");
    fprintf(f, "%sE %d D %d exsize %llu Z %d\n", metaWant(gb, layerGB).c_str(), E, D, (unsigned long long)ex.size(), Z); fclose(f);
    rename((p + ".meta.tmp").c_str(), (p + ".meta").c_str());
    if (verbose) printf("fulltab: saved to %s (+ .ex, .meta) in %.1fs\n", p.c_str(), nowSec() - t);
  }
  void build(double gb, double layerGB, int nth, bool verbose) {
    if (nLev != 2 && nLev != 3) { fprintf(stderr, "fulltab: FTLEV must be 2 or 3\n"); exit(1); }
    Z = 0;
    if (shmLoad(gb, layerGB, verbose)) return;
    // FTNOBUILD=1 (2026-11-12): only map an existing table; never build (a build truncates the FTSHM file,
    // which would kill another process mapping it)
    if (getenv("FTNOBUILD")) { fprintf(stderr, "fulltab: no matching table at FTSHM and FTNOBUILD set\n"); exit(1); }
    Z = nLev - 2;
    if (totalBits <= 64) build<1>(gb, layerGB, nth, verbose);
    else if (totalBits <= 128) build<2>(gb, layerGB, nth, verbose);
    else if (totalBits <= 192) build<3>(gb, layerGB, nth, verbose);
    else if (totalBits <= 256) build<4>(gb, layerGB, nth, verbose);
    else if (totalBits <= 320) build<5>(gb, layerGB, nth, verbose);
    else if (totalBits <= 384) build<6>(gb, layerGB, nth, verbose);
    else if (totalBits <= 448) build<7>(gb, layerGB, nth, verbose);
    else { fprintf(stderr, "fulltab: state too large to pack (%d bits)\n", totalBits); exit(1); }
    shmSave(gb, layerGB, verbose);
  }
  template <int W> void build(double gb, double layerGB, int nth, bool verbose) {
    alloc(gb);
    { // prefault in parallel (first-touch faults otherwise serialize the fill)
      double t = nowSec(); const uint64_t CH = 1 << 26;
      parFor(nth, (bytes + CH - 1) / CH, [&](int, size_t i) { uint64_t a = i * CH, b = min(bytes, a + CH); memset(tab + a, 0, b - a); });
      if (verbose) printf("fulltab: %.1f GB prefaulted in %.1fs\n", bytes / 1e9, nowSec() - t);
    }
    if (verbose) {
      St<NCH> x = C.solved; unsigned r = 7; double tot = 0;
      for (int i = 0; i < 20000; i++) { r = r * 1103515245 + 12345; x = mulc<NCH>(L, x, km[(r >> 8) % km.size()]); if (i >= 100) tot += K.candCount(x); }
      printf("canon: %.2f full evaluations per state on a random walk\n", tot / 19900);
    }
    const int NB = 4096;
    using KV = vector<Key<W>>;
    vector<vector<KV>> lay;
    double t0 = nowSec();
    auto bucketOf = [&](uint64_t h) { return (int)(h >> 52); };
    { St<NCH> f; uint64_t h = K.template canon<true>(C.solved, &f); lay.push_back(vector<KV>(NB)); lay[0][bucketOf(h)].push_back(pack<W>(f)); }
    vector<size_t> lsz{1};
    while (true) {
      int d = lay.size() - 1;
      double candBytes = (double)lsz[d] * km.size() * sizeof(Key<W>);
      if (candBytes > layerGB * 1e9) break;
      vector<vector<KV>> loc(nth, vector<KV>(NB));
      parFor(nth, NB, [&](int t, size_t b) {
        for (auto &k : lay[d][b]) {
          St<NCH> c = unpack<W>(k);
          for (size_t m = 0; m < km.size(); m++) {
            St<NCH> y = mulc<NCH>(L, c, km[m]);
            if (eqSt<NCH>(y, c)) continue;
            St<NCH> f; uint64_t h = K.template canon<true>(y, &f);
            loc[t][bucketOf(h)].push_back(pack<W>(f));
          }
        }
      });
      vector<KV> nx(NB); atomic<size_t> tot{0};
      parFor(nth, NB, [&](int, size_t b) {
        KV v; size_t n = 0; for (int t = 0; t < nth; t++) n += loc[t][b].size();
        v.reserve(n);
        for (int t = 0; t < nth; t++) { v.insert(v.end(), loc[t][b].begin(), loc[t][b].end()); KV().swap(loc[t][b]); }
        sort(v.begin(), v.end()); v.erase(unique(v.begin(), v.end()), v.end());
        KV out; out.reserve(v.size());
        const KV &a = lay[d][b]; const KV *pb = d > 0 ? &lay[d - 1][b] : nullptr;
        size_t ia = 0, ib = 0;
        for (auto &k : v) {
          while (ia < a.size() && a[ia] < k) ia++;
          if (ia < a.size() && a[ia] == k) continue;
          if (pb) { while (ib < pb->size() && (*pb)[ib] < k) ib++; if (ib < pb->size() && (*pb)[ib] == k) continue; }
          out.push_back(k);
        }
        tot += out.size(); nx[b] = std::move(out);
      });
      lay.push_back(std::move(nx)); lsz.push_back(tot);
      if (verbose) printf("  fulltab layer %d: %zu canonical states (%.1fs)\n", d + 1, (size_t)tot, nowSec() - t0), fflush(stdout);
      if (tot == 0) break;
    }
    E = lay.size() - 1;
    while (E > 0 && lsz[E] == 0) E--;
    // exact layers -> code 0
    size_t setE = 0, nEx = 0;
    for (int d = 0; d <= E; d++) nEx += lsz[d];
    { uint64_t sz = 1; while (sz < nEx * 2) sz <<= 1; ex.assign(sz, 0); exMask = sz - 1; }
    qInit(nth);
    // in parallel (2026-11-11: serially this took ~5 min for 83M states on arena); one layer at a time,
    // so on a 60-bit key collision the smaller distance still wins, as in the serial order
    double tx = nowSec();
    for (int d = 0; d <= E; d++)
      parFor(nth, NB, [&](int t, size_t b) {
        for (auto &k : lay[d][b]) { St<NCH> c = unpack<W>(k); uint64_t h = aesHash<NCH>(c); qPush(t, slot(h), 0); exInsertPar(h, d); }
      });
    setE = qDrain(nth, 0);
    if (verbose) printf("fulltab: exact set %zu states, %.2f GB (%.1fs)\n", nEx, ex.size() * 8 / 1e9, nowSec() - tx), fflush(stdout);
    // DFS levels from layer E
    D = E;
    // per-worker fill statistics, one cache line each (never a packed vector<long long>(nth): false sharing)
    struct alignas(64) FillStat { long long leaves = 0, news = 0, cs[4] = {0, 0, 0, 0}; };
    vector<FillStat> fs(nth);
    for (int lev = 1; lev <= nLev; lev++) {
      double est = (double)lsz[E] * pow((double)km.size() * 0.8, lev);
      if (lev == nLev && est > (getenv("MAXEST") ? atof(getenv("MAXEST")) : 4e11)) break;   // MAXEST: leaf cap for the last level
      if (verbose) printf("  depth %d: about %.3g leaves expected\n", E + lev, est), fflush(stdout);
      for (auto &f : fs) f = FillStat{};
      PerfSnap ps = perfNow();
      // progress (2026-11-11): a status line every PROGSEC seconds (default 120) while the level fills
      atomic<size_t> doneB{0}; atomic<bool> fin{false};
      thread prog([&] {
        const double every = getenv("PROGSEC") ? atof(getenv("PROGSEC")) : 120, ts = nowSec();
        double next = ts + every;
        while (!fin) {
          this_thread::sleep_for(chrono::milliseconds(200));
          if (fin || nowSec() < next) continue;
          next += every;
          long long lf = 0; for (int t = 0; t < nth; t++) lf += fs[t].leaves;
          const double fr = (double)doneB / NB, el = nowSec() - ts;
          if (verbose) printf("    depth %d: %zu of %zu blocks (%.1f%%), %.3g leaves, %.0fs, about %.0fs left\n", E + lev, (size_t)doneB, (size_t)NB, 100 * fr, (double)lf, el, fr > 0 ? el * (1 - fr) / fr : 0.0), fflush(stdout);
        }
      });
      const int code = lev - Z;   // level 1 writes code 0 when Z = 1 (<= E+1 through the exact set)
      const uint64_t allM = km.size() >= 64 ? ~0ULL : (1ULL << km.size()) - 1;
      // children of x at DFS level dl (1..lev): leaves at dl == lev; interior nodes cut when within E
      // (exact) or symmetric to an earlier sibling
      auto rec = [&](auto &self, const St<NCH> &x, uint64_t mask, int dl, FillStat &w, int t) -> void {
        uint64_t sib[64]; int ns = 0;
        for (uint64_t mm = mask; mm; mm &= mm - 1) {
          const int m = __builtin_ctzll(mm);
          St<NCH> y = mulc<NCH>(L, x, km[m]);
          if (eqSt<NCH>(y, x)) continue;
          if (dl == lev) {
            w.leaves++; qPush(t, slotOf(y), code);
            if (lev >= 2 && verbose && (w.leaves & 4095) == 0) { w.news += (long long)(K.candCount(y) * 1000); K.stat(y, w.cs); w.cs[3]++; }
            continue;
          }
          uint64_t hy = K.template canon<false>(y);
          if (exact(hy) >= 0) continue;
          bool dup = false; for (int i = 0; i < ns && !dup; i++) dup = sib[i] == hy;
          if (dup) continue;
          sib[ns++] = hy;
          self(self, y, canonAfter[base[m] + 1], dl + 1, w, t);
        }
      };
      parFor(nth, NB, [&](int t, size_t b) {
        struct Done { atomic<size_t> &d; ~Done() { d++; } } done{doneB};
        FillStat &w = fs[t];   // this worker's own cache line (a packed vector cost ~40% of the depth-9 fill on arena)
        for (auto &k : lay[E][b]) rec(rec, unpack<W>(k), allM, 1, w, t);
      });
      fin = true; prog.join();
      long long L1 = 0, N1 = qDrain(nth, code), CC = 0; for (auto &f : fs) { L1 += f.leaves; CC += f.news; }
      if (verbose && lev == nLev && lev >= 2) {
        long long cs[4] = {0, 0, 0, 0}; for (auto &f : fs) for (int i = 0; i < 4; i++) cs[i] += f.cs[i];
        double n = max(1LL, cs[3]);
        printf("  leaf canonicalization: %.2f full evaluations; sampled: %.2f candidates, %.2f after partial, %.2f distinct images\n",
               CC / 1000.0 / max(1LL, L1 / 4096), cs[0] / n, cs[1] / n, cs[2] / n);
      }
      D = E + lev;
      PerfDelta pd = perfSince(ps);
      if (verbose) printf("  fulltab depth %d: %lld leaves, %lld new slots, %.0f instr/leaf, %.0f cyc/leaf (%.1fs)\n", D, L1, N1, pd.ins / max(1LL, L1), pd.cyc / max(1LL, L1), nowSec() - t0), fflush(stdout);
    }
    if (verbose) {
      uint64_t hist[4] = {0, 0, 0, 0};
      for (uint64_t i = 0; i < M; i += 997) hist[code(i)]++;
      double tot = hist[0] + hist[1] + hist[2] + hist[3];
      printf("fulltab: %.1f GB, %.3g slots, exact to %d (D=%d); slot codes <=%d %.4f%%, %d %.3f%%, %d %.3f%%, more %.3f%%  (%.1fs)\n",
             bytes / 1e9, (double)M, E, D, E + Z, 100 * hist[0] / tot, E + Z + 1, 100 * hist[1] / tot, E + Z + 2, 100 * hist[2] / tot, 100 * hist[3] / tot, nowSec() - t0);
    }
  }
};
