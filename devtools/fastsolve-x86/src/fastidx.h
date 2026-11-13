// Fast NEON index for CosetPDB abstractions whose orbits have <= 16 positions.
// Bit-for-bit identical to CosetPDB::index (verified in fasttest).
//   key:  gather orbit lanes (TBL), labels (TBL), label masks (CMEQ+AND+ADDV),
//         multiset rank via colex/pext tables, orientation digits via dot product
//   rest: fused conjugate+project: labels = TBL(labSi[s], TBL(x, Sidx[s]) & posmask)
#pragma once
#include "pdb.h"

struct FastTabs {
  static uint16_t colex[1 << 16];          // bitmask -> colex rank among same-popcount masks
  static uint8_t pext8[256][256];          // pext(x, m) for bytes
  static uint8_t pc8[256];
  static bool done;
  static void init() {
    if (done) return;
    for (uint32_t m = 0; m < (1u << 16); m++) {
      uint32_t r = 0; int c = 0;
      for (int t = 0; t < 16; t++) if ((m >> t) & 1) { c++; r += binom(t, c); }
      colex[m] = r;
    }
    for (int x = 0; x < 256; x++) for (int m = 0; m < 256; m++) {
      int r = 0, k = 0;
      for (int t = 0; t < 8; t++) if ((m >> t) & 1) { if ((x >> t) & 1) r |= 1 << k; k++; }
      pext8[x][m] = r;
    }
    for (int x = 0; x < 256; x++) pc8[x] = __builtin_popcount(x);
    done = true;
  }
  static inline uint32_t pext16(uint32_t x, uint32_t m) {
    uint32_t lo = pext8[x & 255][m & 255];
    return lo | (pext8[(x >> 8) & 255][(m >> 8) & 255] << pc8[m & 255]);
  }
};
uint16_t FastTabs::colex[1 << 16];
uint8_t FastTabs::pext8[256][256];
uint8_t FastTabs::pc8[256];
bool FastTabs::done = false;

static inline uint32_t movemask16(uint8x16_t eq) {
#if !defined(__aarch64__)
  return (uint32_t)_mm_movemask_epi8(M_(eq));
#endif
  static const uint8_t bw[16] = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
  uint8x16_t v = vandq_u8(eq, vld1q_u8(bw));
  return vaddv_u8(vget_low_u8(v)) | (vaddv_u8(vget_high_u8(v)) << 8);
}

struct FastOrb {
  int n, nl, omod, off; bool ori, oriOnly; uint32_t pmask;
  uint32_t full;                  // (1<<n)-1
  vector<int> cnt;                // per label
  uint64_t bmul[16];              // multiplier per label step: binom(remaining, cnt[l])
  uint64_t oriSize, size;
  alignas(16) uint16_t ow[16];    // orientation digit weights
};

// Optional compile-time shape (generated per puzzle by solve.cpp):
//   key orbit size/labels/ori(0,1; 2 = runtime), number of rest parts, and per
//   rest part: size, labels, mode (0 labels, 2 orientation of the conjugate).
//   NR = 0 means fully runtime.
template <int KN_, int KNL_, int KORI_, int NR_, int RN0, int RNL0, int RM0, int RN1, int RNL1, int RM1, int RN2, int RNL2, int RM2>
struct ShapeT {
  static constexpr int KN = KN_, KNL = KNL_, KORI = KORI_, NR = NR_;
  static constexpr int RN[3] = {RN0, RN1, RN2}, RNL[3] = {RNL0, RNL1, RNL2}, RM[3] = {RM0, RM1, RM2};
  static constexpr bool anyOri = (NR > 0 && RM0 == 2) || (NR > 1 && RM1 == 2) || (NR > 2 && RM2 == 2);
};
using GenericShape = ShapeT<0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0>;
#ifdef SPEC1
using Shape1 = ShapeT<SPEC1>;
#else
using Shape1 = GenericShape;
#endif
#ifdef SPEC2
using Shape2 = ShapeT<SPEC2>;
#else
using Shape2 = GenericShape;
#endif
template <int N> static inline uint32_t movemaskN(uint8x16_t eq) {
#if !defined(__aarch64__)
  return (uint32_t)_mm_movemask_epi8(M_(eq)) & (N > 0 && N <= 8 ? 0xFF : 0xFFFF);
#endif
  static const uint8_t bw[16] = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
  uint8x16_t v = vandq_u8(eq, vld1q_u8(bw));
  if (N > 0 && N <= 8) return vaddv_u8(vget_low_u8(v));
  return vaddv_u8(vget_low_u8(v)) | (vaddv_u8(vget_high_u8(v)) << 8);
}
template <int NCH, class SH = GenericShape> struct FastIndex {
  const CosetPDB<NCH> &P;
  const Layout &L;
  FastOrb key; vector<FastOrb> rest;
  alignas(16) uint8_t keyGather[16], keyLab[64];
  // per symmetry, per rest orbit: gather of S.p[j] for the orbit's lanes, and label table of Si
  struct RestSym { alignas(16) uint8_t sidx[16]; alignas(16) uint8_t lab[64]; alignas(16) uint8_t oxor[16]; alignas(16) uint8_t sio[64]; };
  vector<vector<RestSym>> rs;  // [rest orbit][sym]
  vector<RestSym> rsFlat;      // [sym * nrest + i]
  int nrest = 0; uint64_t restSz[8]; bool anyOriOnly = false;
  vector<array<uint8_t, 16>> oriGather;  // per rest orbit
  vector<array<uint8_t, 64>> plainLab;   // per rest orbit: position -> label of the piece whose home it is
  vector<uint32_t> keyRepPacked; // raw key -> class<<8 | sym

  static FastOrb mkOrb(const OrbitProj &o) {
    FastOrb f; f.n = o.n; f.nl = o.nl; f.omod = o.omod; f.ori = o.ori; f.full = (1u << o.n) - 1;
    f.off = o.off; f.oriOnly = o.mode == 2; f.pmask = o.pmask;
    f.cnt = o.cnt; int rem = o.n;
    for (int l = 0; l < o.nl - 1; l++) { f.bmul[l] = binom(rem, o.cnt[l]); rem -= o.cnt[l]; }
    f.oriSize = o.oriSize; f.size = o.size();
    memset(f.ow, 0, sizeof f.ow);
    if (o.ori) { uint32_t w = 1; for (int j = o.n - 2; j >= 0; j--) { f.ow[j] = w; w *= o.omod; } }
    return f;
  }
  FastIndex(const CosetPDB<NCH> &p) : P(p), L(p.L) {
    FastTabs::init();
    if (P.keyP.size() != 1) { fprintf(stderr, "fastidx: one key orbit only\n"); exit(1); }
    auto &ko = P.keyP[0];
    key = mkOrb(ko);
    for (int j = 0; j < 16; j++) keyGather[j] = j < ko.n ? ko.off + j : 0xFF;
    for (int q = 0; q < 64; q++) keyLab[q] = (q >= ko.off && q < ko.off + ko.n) ? ko.lab[q - ko.off] : 0xEE;
    for (auto &ro : P.restP) {
      if (ro.ori && ro.mode != 2) { fprintf(stderr, "fastidx: rest orientation only as split orbit\n"); exit(1); }
      if (ro.mode == 2 || ro.pmask) anyOriOnly = true;
      if (ro.pmask && ro.omod != 2) { fprintf(stderr, "fastidx: partial orientation needs omod 2\n"); exit(1); }
      rest.push_back(mkOrb(ro));
      vector<RestSym> v(P.ks.size());
      for (size_t s = 0; s < P.ks.size(); s++) {
        const KSym &k = P.ks[s];
        for (int j = 0; j < 16; j++) v[s].sidx[j] = j < ro.n ? k.s.idx[ro.off + j] : 0xFF;
        for (int j = 0; j < 16; j++) v[s].oxor[j] = j < ro.n ? (((k.s.oadd[ro.off + j] >> L.p) & 1) << L.p) : 0;
        // label of Si.p[q] for piece position q (Si as table: sinvTab low bits)
        for (int q = 0; q < 64; q++) {
          if (q >= ro.off && q < ro.off + ro.n) { int sq = k.sinvTab[q] & L.posmask; v[s].lab[q] = ro.lab[sq - ro.off]; }
          else v[s].lab[q] = 0xEE;
          // omod-2 partial orientation: Si orientation bit of the piece at position q
          v[s].sio[q] = (q >= ro.off && q < ro.off + ro.n) ? (((k.sinvTab[q] >> L.p) & 1) << L.p) : 0;
        }
      }
      rs.push_back(v);
    }
    nrest = rest.size();
    for (auto &ro : P.restP) { array<uint8_t, 64> t; for (int q = 0; q < 64; q++) t[q] = (q >= ro.off && q < ro.off + ro.n) ? ro.lab[q - ro.off] : 0xEE; plainLab.push_back(t); }
    for (auto &f : rest) { array<uint8_t, 16> g; for (int j = 0; j < 16; j++) g[j] = j < f.n ? f.off + j : 0xFF; oriGather.push_back(g); }
    for (size_t s = 0; s < P.ks.size(); s++) for (int i = 0; i < nrest; i++) rsFlat.push_back(rs[i][s]);
    for (int i = 0; i < nrest; i++) restSz[i] = rest[i].size;
    keyRepPacked.resize(P.keySize);
    for (uint64_t v = 0; v < P.keySize; v++) keyRepPacked[v] = P.repClass[v] < 0 ? 0xFFFFFFFFu : (P.repClass[v] << 8) | P.repSym[v];
  }
  static inline uint8x16_t tbl64(const uint8_t *t, uint8x16_t idx) {
    uint8x16x4_t T = {vld1q_u8(t), vld1q_u8(t + 16), vld1q_u8(t + 32), vld1q_u8(t + 48)};
    return vqtbl4q_u8(T, idx);
  }
  // compile-time shaped rank: N, NL known (0 = runtime)
  template <int N, int NL> static inline uint64_t labRankT(const FastOrb &f, uint8x16_t lab) {
    if constexpr (N == 0 || NL == 0) return labRank(f, lab);
    else {
      constexpr uint32_t full = (1u << N) - 1;
      uint32_t m = movemaskN<N>(vceqq_u8(lab, vdupq_n_u8(0))) & full;
      uint64_t r = FastTabs::colex[m];
      uint32_t avail = full & ~m;
#pragma clang loop unroll(full)
      for (int l = 1; l < NL - 1; l++) {
        m = movemaskN<N>(vceqq_u8(lab, vdupq_n_u8(l)));
        uint32_t sub = N <= 8 ? FastTabs::pext8[m & 255][avail & 255] : FastTabs::pext16(m, avail);
        r = r * f.bmul[l] + FastTabs::colex[sub];
        avail &= ~m;
      }
      return r;
    }
  }
  // labels + partial orientation (omod 2), from the conjugated state y
  template <int N, int NL> inline uint64_t partialRank(int i, const St<NCH> &y, uint8x16_t PM) const {
    const FastOrb &f = rest[i];
    uint8x16_t g = tblN<NCH>(y, vld1q_u8(oriGather[i].data()));
    uint8x16_t lab = tbl64(plainLab[i].data(), vandq_u8(g, PM));
    uint64_t r = labRankT<N, NL>(f, lab);
    uint32_t o = movemask16(vtstq_u8(g, vdupq_n_u8(1 << L.p))) & f.full;
    uint32_t mm = 0;
    for (uint32_t pm = f.pmask; pm; pm &= pm - 1) mm |= movemask16(vceqq_u8(lab, vdupq_n_u8(__builtin_ctz(pm))));
    return r * f.oriSize + FastTabs::pext16(o, mm & f.full);
  }
  template <int I> inline uint64_t restRankI(const St<NCH> &x, const St<NCH> &y, const RestSym *RS, uint8x16_t PM) const {
    if constexpr (SH::RM[I] == 3) {
      // fused conjugation for omod 2: pieces via S, labels via Si, ori = x.o ^ S.o ^ Si.o(piece)
      const RestSym &R = RS[I]; const FastOrb &f = rest[I];
      uint8x16_t gg = tblN<NCH>(x, vld1q_u8(R.sidx));
      uint8x16_t pos = vandq_u8(gg, PM);
      uint8x16_t ll = tbl64(R.lab, pos);
      uint8x16_t ob = veorq_u8(veorq_u8(gg, vld1q_u8(R.oxor)), tbl64(R.sio, pos));
      uint64_t r = labRankT<SH::RN[I], SH::RNL[I]>(f, ll);
      uint32_t o = movemaskN<SH::RN[I]>(vtstq_u8(ob, vdupq_n_u8(1 << L.p)));
      uint32_t mm = 0;
      for (uint32_t pm = f.pmask; pm; pm &= pm - 1) mm |= movemaskN<SH::RN[I]>(vceqq_u8(ll, vdupq_n_u8(__builtin_ctz(pm))));
      return r * f.oriSize + FastTabs::pext16(o & f.full, mm & f.full);
    }
    else if constexpr (SH::RM[I] == 2) {
      uint8x16_t g2 = tblN<NCH>(y, vld1q_u8(oriGather[I].data()));
      return oriDigits(rest[I], g2, L.p);
    } else {
      const RestSym &R = RS[I];
      uint8x16_t gg = vandq_u8(tblN<NCH>(x, vld1q_u8(R.sidx)), PM);
      uint8x16_t ll = tbl64(R.lab, gg);
      return labRankT<SH::RN[I], SH::RNL[I]>(rest[I], ll);
    }
  }
  // multiset rank from a label vector (lanes >= n hold 0xEE / 0)
  static inline uint64_t labRank(const FastOrb &f, uint8x16_t lab) {
    uint32_t m = movemask16(vceqq_u8(lab, vdupq_n_u8(0))) & f.full;
    uint64_t r = FastTabs::colex[m];
    uint32_t avail = f.full & ~m;
    for (int l = 1; l < f.nl - 1; l++) {
      m = movemask16(vceqq_u8(lab, vdupq_n_u8(l)));
      r = r * f.bmul[l] + FastTabs::colex[FastTabs::pext16(m, avail)];
      avail &= ~m;
    }
    return r;
  }
  static inline uint32_t oriDigits(const FastOrb &f, uint8x16_t g, int p) {
    uint8x16_t o = vshlq_u8(g, vdupq_n_s8(-p));
    uint16x8_t lo = vmulq_u16(vmovl_u8(vget_low_u8(o)), vld1q_u16(f.ow));
    uint16x8_t hi = vmulq_u16(vmovl_u8(vget_high_u8(o)), vld1q_u16(f.ow + 8));
    return vaddlvq_u16(lo) + vaddlvq_u16(hi);
  }
  inline uint64_t index(const St<NCH> &x) const {
    const uint8x16_t PM = vdupq_n_u8(L.posmask);
    uint8x16_t g = tblN<NCH>(x, vld1q_u8(keyGather));
    uint8x16_t lab = tbl64(keyLab, vandq_u8(g, PM));
    uint64_t kr = labRankT<SH::KN, SH::KNL>(key, lab);
    if (SH::KORI == 2 ? key.ori : SH::KORI == 1) kr = kr * key.oriSize + oriDigits(key, g, L.p);
    uint32_t e = keyRepPacked[kr];
    uint32_t s = e & 255;
    uint64_t r = 0;
    const RestSym *RS = &rsFlat[s * nrest];
    if constexpr (SH::NR > 0) {
      St<NCH> y;
      if constexpr (SH::anyOri) y = conj<NCH>(L, x, P.ks[s]);
      r = restRankI<0>(x, y, RS, PM);
      if constexpr (SH::NR > 1) r = r * restSz[1] + restRankI<1>(x, y, RS, PM);
      if constexpr (SH::NR > 2) r = r * restSz[2] + restRankI<2>(x, y, RS, PM);
      return (uint64_t)(e >> 8) * P.restSize + r;
    }
    St<NCH> y;
    if (anyOriOnly) y = conj<NCH>(L, x, P.ks[s]);
    for (int i = 0; i < nrest; i++) {
      if (rest[i].pmask) { r = r * restSz[i] + partialRank<0, 0>(i, y, PM); continue; }
      if (rest[i].oriOnly) {
        uint8x16_t g2 = tblN<NCH>(y, vld1q_u8(oriGather[i].data()));
        r = r * restSz[i] + oriDigits(rest[i], g2, L.p);
        continue;
      }
      const RestSym &R = RS[i];
      uint8x16_t gg = vandq_u8(tblN<NCH>(x, vld1q_u8(R.sidx)), PM);
      uint8x16_t ll = tbl64(R.lab, gg);
      r = r * restSz[i] + labRank(rest[i], ll);
    }
    return (uint64_t)(e >> 8) * P.restSize + r;
  }
};
