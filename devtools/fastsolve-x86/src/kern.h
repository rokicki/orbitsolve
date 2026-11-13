// Packed-state kernels.  One byte per position: low p bits = source position
// (global, < 2^p), high bits = orientation.  N <= 128 (TBL + TBX for more than 64 bytes); N <= 144 with
// 8-bit positions and no orientation (2026-11-11, for obl8ax/obl8op: six orbits of 24).
//   compose with constant b:  c[j] = a[b.p[j]] (+) b.o[j]
//     -> one TBL (1..4 regs), one ADD, then "mod" via UMIN(x, x - M[j])
//   premultiply by constant m: c = m * x : c[j] = m[x.p[j]] (+) x.o[j]
//   conjugate by symmetry s: s^-1 * a * s  (mirror lanes negate orientation)
#pragma once
#include "puzzle.h"
#if defined(__aarch64__)
#include <arm_neon.h>
static inline uint8x16_t aesMix(uint8x16_t h, uint8x16_t k) { return vaesmcq_u8(vaeseq_u8(h, k)); }
#else  // x86 port (2026-11-11): NEON via SIMDe; AESE+AESMC(h, k) == AESENC(h ^ k, 0) exactly
#define SIMDE_ENABLE_NATIVE_ALIASES
#include <simde/arm/neon.h>
#include <immintrin.h>
#include <cstring>
static inline uint8x16_t aesMix(uint8x16_t h, uint8x16_t k) {
  __m128i x, y; std::memcpy(&x, &h, 16); std::memcpy(&y, &k, 16);
  x = _mm_aesenc_si128(_mm_xor_si128(x, y), _mm_setzero_si128());
  uint8x16_t r; std::memcpy(&r, &x, 16); return r;
}
// native SSE views of SIMDe vectors (free: same 16 bytes)
static inline __m128i M_(uint8x16_t v) { __m128i x; std::memcpy(&x, &v, 16); return x; }
static inline uint8x16_t U_(__m128i x) { uint8x16_t v; std::memcpy(&v, &x, 16); return v; }
// TBL over n consecutive 16-byte tables, natively (SIMDe's multi-register vqtbl/vqtbx cost ~4x on arena):
// per table k, lane index i - 16k saturating-added to 0x70 keeps the low nibble for 0..15 and sets the
// PSHUFB zeroing bit for everything else (also for lanes that wrapped below 0; needs n <= 8)
static inline __m128i tblX(const __m128i *t, int n, __m128i i) {
  const __m128i s16 = _mm_set1_epi8(16), k70 = _mm_set1_epi8(0x70);
  __m128i r = _mm_setzero_si128();
  for (int k = 0; k < n; k++) { r = _mm_or_si128(r, _mm_shuffle_epi8(_mm_loadu_si128(t + k), _mm_adds_epu8(i, k70))); i = _mm_sub_epi8(i, s16); }
  return r;
}
#endif
#include <cstring>
constexpr int KMAX = 144;   // max positions (7-bit position field when N > 64, 8-bit and no orientation when N > 128)

// Gather plan for a constant index vector (x86): for each output chunk i, only the source chunks it
// reads, each with its PSHUFB control (lane index within that chunk, 0x80 = not from this chunk).
// Pairs are stored contiguously so a plan touches only a few cache lines.
struct GPlan {
  uint8_t start[KMAX / 16 + 1], src[KMAX];   // src: byte offset (chunk * 16)
  alignas(16) uint8_t ctl[KMAX][16];
  void make(const uint8_t *idx) {
    int n = 0;
    for (int i = 0; i < KMAX / 16; i++) {
      start[i] = n;
      for (int k = 0; k < KMAX / 16; k++) {
        bool any = false; uint8_t c[16];
        for (int l = 0; l < 16; l++) { int v = idx[16 * i + l]; bool in = v >= 16 * k && v < 16 * k + 16; c[l] = in ? v - 16 * k : 0x80; any |= in; }
        if (any) { src[n] = 16 * k; memcpy(ctl[n], c, 16); n++; }
      }
    }
    start[KMAX / 16] = n;
  }
};

struct alignas(64) KElem {        // a constant element prepared for kernels
  uint8_t idx[KMAX];   // b.p[j] (pos only)
  uint8_t oadd[KMAX];  // b.o[j] << p
  uint8_t packed[KMAX];// b as data: b.p[j] | b.o[j] << p   (used as a TBL table)
  uint8_t neg[KMAX];   // 0xFF on lanes whose orientation must be negated (mirror)
  bool anyneg;
  GPlan gp;            // gather plan for idx (x86)
};

struct Layout {
  int N, p, nch;
  uint8_t posmask, orimask;
  alignas(64) uint8_t modsub[KMAX]; // omod << p, 0 for natural wrap or omod 1
  bool ok = true;
  Layout() {}
  Layout(const Puzzle &pz) {
    N = pz.N;
    p = N <= 32 ? 5 : N <= 64 ? 6 : N <= 128 ? 7 : 8;
    nch = (N + 15) / 16;
    if (N > KMAX) ok = false;
    if (p == 8) for (int j = 0; j < N; j++) if (pz.omodAt(j) > 1) ok = false;   // no room for orientation
    posmask = (1 << p) - 1;
    orimask = 0xFF & ~posmask;
    memset(modsub, 0, KMAX);
    for (int j = 0; j < N; j++) {
      int o = pz.omodAt(j);
      if (o == 1) continue;
      if ((o << p) == 256) continue; // natural wrap
      if (((2 * o - 2) << p) + posmask >= 256) ok = false;
      modsub[j] = (uint8_t)(o << p);
    }
  }
  KElem prep(const Puzzle &pz, const Elem &e) const {
    KElem k;
    k.anyneg = false;
    for (int j = 0; j < KMAX; j++) {
      if (j < N) {
        k.idx[j] = e.p[j];
        k.oadd[j] = (uint8_t)(e.o[j] << p);
        k.packed[j] = e.p[j] | (e.o[j] << p);
        bool ng = pz.omodAt(j) > 2 ? pz.sgn(e, pz.orbitOf[j]) < 0 : false;
        k.neg[j] = ng ? 0xFF : 0;
        k.anyneg |= ng;
      } else {
        k.idx[j] = j; k.oadd[j] = 0; k.packed[j] = j; k.neg[j] = 0;
      }
    }
    k.gp.make(k.idx);
    return k;
  }
  void pack(const Elem &e, uint8_t *out) const {
    for (int j = 0; j < KMAX; j++) out[j] = j < N ? (e.p[j] | (e.o[j] << p)) : j;
  }
  Elem unpack(const Puzzle &pz, const uint8_t *in) const {
    Elem e = pz.ident();
    for (int j = 0; j < N; j++) { e.p[j] = in[j] & posmask; e.o[j] = in[j] >> p; }
    return e;
  }
};

template <int NCH> struct St { uint8x16_t c[NCH]; };

template <int NCH> static inline uint8x16_t tblN(const St<NCH> &a, uint8x16_t idx) {
#if !defined(__aarch64__)
  return U_(tblX((const __m128i *)a.c, NCH, M_(idx)));
#endif
  if constexpr (NCH == 1) return vqtbl1q_u8(a.c[0], idx);
  if constexpr (NCH == 2) { uint8x16x2_t t = {a.c[0], a.c[1]}; return vqtbl2q_u8(t, idx); }
  if constexpr (NCH == 3) { uint8x16x3_t t = {a.c[0], a.c[1], a.c[2]}; return vqtbl3q_u8(t, idx); }
  if constexpr (NCH >= 4) {
    uint8x16x4_t t = {a.c[0], a.c[1], a.c[2], a.c[3]};
    uint8x16_t r = vqtbl4q_u8(t, idx);
    if constexpr (NCH > 4) {
      uint8x16_t hi = vsubq_u8(idx, vdupq_n_u8(64));   // lanes < 64 wrap to >= 192: out of range, kept
      if constexpr (NCH == 5) r = vqtbx1q_u8(r, a.c[4], hi);
      if constexpr (NCH == 6) { uint8x16x2_t u = {a.c[4], a.c[5]}; r = vqtbx2q_u8(r, u, hi); }
      if constexpr (NCH == 7) { uint8x16x3_t u = {a.c[4], a.c[5], a.c[6]}; r = vqtbx3q_u8(r, u, hi); }
      if constexpr (NCH >= 8) { uint8x16x4_t u = {a.c[4], a.c[5], a.c[6], a.c[7]}; r = vqtbx4q_u8(r, u, hi); }
      if constexpr (NCH == 9) r = vqtbx1q_u8(r, a.c[8], vsubq_u8(idx, vdupq_n_u8(128)));
    }
    return r;
  }
}
// output chunk i of the gather a[idx] by a plan (x86; ARM keeps the TBL on idx)
template <int NCH> static inline uint8x16_t gatherP(const St<NCH> &a, const GPlan &g, const uint8_t *idx, int i) {
#if defined(__aarch64__)
  (void)g; return tblN<NCH>(a, vld1q_u8(idx + 16 * i));
#else
  (void)idx; __m128i r = _mm_setzero_si128();
  for (int t = g.start[i]; t < g.start[i + 1]; t++) r = _mm_or_si128(r, _mm_shuffle_epi8(_mm_load_si128((const __m128i *)((const uint8_t *)a.c + g.src[t])), _mm_load_si128((const __m128i *)g.ctl[t])));
  return U_(r);
#endif
}
template <int NCH> static inline uint8x16_t tblK(const uint8_t *tab, uint8x16_t idx) {
  St<NCH> t;
  for (int i = 0; i < NCH; i++) t.c[i] = vld1q_u8(tab + 16 * i);
  return tblN<NCH>(t, idx);
}
template <int NCH> static inline St<NCH> load(const uint8_t *p) {
  St<NCH> s; for (int i = 0; i < NCH; i++) s.c[i] = vld1q_u8(p + 16 * i); return s;
}
template <int NCH> static inline void store(const St<NCH> &s, uint8_t *p) {
  for (int i = 0; i < NCH; i++) vst1q_u8(p + 16 * i, s.c[i]);
}

// c = a * b   (b constant: move or symmetry without negation)
template <int NCH> static inline St<NCH> mulc(const Layout &L, const St<NCH> &a, const KElem &b) {
  St<NCH> r;
  for (int i = 0; i < NCH; i++) {
    uint8x16_t t = gatherP<NCH>(a, b.gp, b.idx, i);
    t = vaddq_u8(t, vld1q_u8(b.oadd + 16 * i));
    r.c[i] = vminq_u8(t, vsubq_u8(t, vld1q_u8(L.modsub + 16 * i)));
  }
  return r;
}
// c = a * b where b may negate orientation on some lanes (mirror symmetries)
template <int NCH> static inline St<NCH> mulcNeg(const Layout &L, const St<NCH> &a, const KElem &b) {
  St<NCH> r;
  uint8x16_t om = vdupq_n_u8(L.orimask);
  for (int i = 0; i < NCH; i++) {
    uint8x16_t t = gatherP<NCH>(a, b.gp, b.idx, i);
    uint8x16_t M = vld1q_u8(L.modsub + 16 * i);
    uint8x16_t o = vandq_u8(t, om);
    uint8x16_t on = vminq_u8(vsubq_u8(M, o), vsubq_u8(vdupq_n_u8(0), o)); // -o mod omod
    o = vbslq_u8(vld1q_u8(b.neg + 16 * i), on, o);
    t = vaddq_u8(veorq_u8(t, vandq_u8(t, om)), o);
    t = vaddq_u8(t, vld1q_u8(b.oadd + 16 * i));
    r.c[i] = vminq_u8(t, vsubq_u8(t, M));
  }
  return r;
}
// c = m * x  (m constant, given by its packed table; table may be pre-negated)
template <int NCH> static inline St<NCH> premulc(const Layout &L, const uint8_t *mpacked, const St<NCH> &x) {
  St<NCH> r;
  uint8x16_t pm = vdupq_n_u8(L.posmask);
  for (int i = 0; i < NCH; i++) {
    uint8x16_t pos = vandq_u8(x.c[i], pm);
    uint8x16_t t = tblK<NCH>(mpacked, pos);
    t = vaddq_u8(t, vsubq_u8(x.c[i], pos));
    r.c[i] = vminq_u8(t, vsubq_u8(t, vld1q_u8(L.modsub + 16 * i)));
  }
  return r;
}

// Symmetry prepared for conjugation: T = s^-1 * a * s
struct KSym {
  KElem s;                     // right factor
  alignas(64) uint8_t sinvTab[KMAX]; // left factor as table; ori negated where eps(Y) = -1
};
inline KSym prepSym(const Puzzle &pz, const Layout &L, const Elem &S) {
  KSym k;
  k.s = L.prep(pz, S);
  Elem Si = pz.inv(S);
  // (Si * Y).o[j] = eps_Y(j) * Si.o[Y.p[j]] + Y.o[j]; eps_Y = eps_S for a real state.
  // Pre-negate Si's orientation entries on orbits where eps_S = -1.
  for (int j = 0; j < KMAX; j++) {
    if (j >= L.N) { k.sinvTab[j] = j; continue; }
    int o = Si.o[j], m = pz.omodAt(j);
    if (m > 2 && pz.sgn(S, pz.orbitOf[j]) < 0) o = (m - o) % m;
    k.sinvTab[j] = Si.p[j] | (o << L.p);
  }
  return k;
}
template <int NCH> static inline St<NCH> conj(const Layout &L, const St<NCH> &a, const KSym &k) {
  St<NCH> y = k.s.anyneg ? mulcNeg<NCH>(L, a, k.s) : mulc<NCH>(L, a, k.s);
  return premulc<NCH>(L, k.sinvTab, y);
}

// Fast full-state hash (twsearch semantics: hash the whole state)
template <int NCH> static inline uint64_t hashSt(const St<NCH> &a) {
  uint64x2_t acc = vreinterpretq_u64_u8(a.c[0]);
  if constexpr (NCH > 1) acc = veorq_u64(vreinterpretq_u64_u8(vextq_u8(a.c[1], a.c[1], 3)), acc);
  if constexpr (NCH > 2) acc = veorq_u64(vreinterpretq_u64_u8(vextq_u8(a.c[2], a.c[2], 7)), acc);
  if constexpr (NCH > 3) acc = veorq_u64(vreinterpretq_u64_u8(vextq_u8(a.c[3], a.c[3], 11)), acc);
  if constexpr (NCH > 4) acc = veorq_u64(vreinterpretq_u64_u8(vextq_u8(a.c[4], a.c[4], 5)), acc);
  if constexpr (NCH > 5) acc = veorq_u64(vreinterpretq_u64_u8(vextq_u8(a.c[5], a.c[5], 9)), acc);
  if constexpr (NCH > 6) acc = veorq_u64(vreinterpretq_u64_u8(vextq_u8(a.c[6], a.c[6], 13)), acc);
  if constexpr (NCH > 7) acc = veorq_u64(vreinterpretq_u64_u8(vextq_u8(a.c[7], a.c[7], 1)), acc);
  if constexpr (NCH > 8) acc = veorq_u64(vreinterpretq_u64_u8(vextq_u8(a.c[8], a.c[8], 6)), acc);
  uint64_t lo = vgetq_lane_u64(acc, 0), hi = vgetq_lane_u64(acc, 1);
  uint64_t h = (lo ^ (hi * 0x9E3779B97F4A7C15ULL)) * 0xD6E8FEB86659FD93ULL;
  return h ^ (h >> 32);
}
template <int NCH> static inline bool eqSt(const St<NCH> &a, const St<NCH> &b) {
  uint8x16_t x = veorq_u8(a.c[0], b.c[0]);
  for (int i = 1; i < NCH; i++) x = vorrq_u8(x, veorq_u8(a.c[i], b.c[i]));
#if defined(__aarch64__)
  return vmaxvq_u8(x) == 0;
#else
  return _mm_testz_si128(M_(x), M_(x));   // SIMDe's vmaxvq is a scalar loop on x86
#endif
}
// lexicographic compare on bytes (for exact canonical forms)
template <int NCH> static inline int cmpSt(const St<NCH> &a, const St<NCH> &b) {
  uint8_t x[KMAX], y[KMAX];
  store<NCH>(a, x); store<NCH>(b, y);
  return memcmp(x, y, 16 * NCH);
}
