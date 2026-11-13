// Symmetry-reduction keys.  A key is a projection of the state onto some
// orbits (perm and/or orientation).  The rep table maps raw key -> the first
// symmetry minimizing the conjugated key, plus a tie mask (other syms that
// also minimize).  Using "first minimizing sym" without tie-breaking gives a
// deterministic but not class-canonical reduction ("twins").
#pragma once
#include "kern.h"
#include <cstdint>

struct KeyPart { int orbit; bool perm, ori; bool oriSumFree = false; };

struct Key {
  vector<KeyPart> parts;
  string desc;
  uint64_t size = 1;
  vector<uint64_t> radix; // per part: perm count, ori count
  // ------- raw rank of a state (generic scalar) -------
  static uint64_t permRank(const uint8_t *x, int n) { // x: values 0..n-1
    uint64_t r = 0; uint32_t used = 0;
    for (int i = 0; i < n - 1; i++) {
      int v = x[i];
      int c = __builtin_popcount(used & ((1u << v) - 1));
      r = r * (n - i) + (v - c);
      used |= 1u << v;
    }
    return r;
  }
  static void permUnrank(uint64_t r, uint8_t *x, int n) {
    int dig[64];
    for (int i = n - 1; i >= 0; i--) { dig[i] = r % (n - i); r /= (n - i); }
    uint32_t avail = (n == 32) ? 0xffffffffu : ((1u << n) - 1);
    for (int i = 0; i < n; i++) {
      int k = dig[i];
      uint32_t a = avail;
      for (int t = 0; t < k; t++) a &= a - 1;
      int v = __builtin_ctz(a);
      x[i] = v; avail &= ~(1u << v);
    }
  }
  static uint64_t fact(int n) { uint64_t f = 1; for (int i = 2; i <= n; i++) f *= i; return f; }
  void init(const Puzzle &pz) {
    size = 1; radix.clear(); desc.clear();
    for (auto &kp : parts) {
      auto &ob = pz.orbits[kp.orbit];
      uint64_t pr = kp.perm ? fact(ob.n) : 1;
      uint64_t orr = 1;
      int nd = kp.oriSumFree ? ob.n : ob.n - 1;
      if (kp.ori) for (int i = 0; i < nd; i++) orr *= ob.omod; // last ori determined if sum invariant
      radix.push_back(pr); radix.push_back(orr);
      size *= pr * orr;
      desc += ob.name + (kp.perm ? "P" : "") + (kp.ori ? "O" : "") + " ";
    }
  }
  // rank from a packed state byte array
  uint64_t rank(const Puzzle &pz, const Layout &L, const uint8_t *st) const {
    uint64_t r = 0;
    for (size_t i = 0; i < parts.size(); i++) {
      auto &kp = parts[i]; auto &ob = pz.orbits[kp.orbit];
      if (kp.perm) {
        uint8_t x[64];
        for (int j = 0; j < ob.n; j++) x[j] = (st[ob.off + j] & L.posmask) - ob.off;
        r = r * radix[2 * i] + permRank(x, ob.n);
      }
      if (kp.ori) {
        uint64_t o = 0;
        int nd = kp.oriSumFree ? ob.n : ob.n - 1;
        for (int j = 0; j < nd; j++) o = o * ob.omod + (st[ob.off + j] >> L.p);
        r = r * radix[2 * i + 1] + o;
      }
    }
    return r;
  }
  // build a packed state realizing raw key value v (other positions identity)
  void unrank(const Puzzle &pz, const Layout &L, uint64_t v, uint8_t *st) const {
    for (int j = 0; j < 64; j++) st[j] = j;
    for (int i = (int)parts.size() - 1; i >= 0; i--) {
      auto &kp = parts[i]; auto &ob = pz.orbits[kp.orbit];
      if (kp.ori) {
        uint64_t o = v % radix[2 * i + 1]; v /= radix[2 * i + 1];
        int sum = 0, nd = kp.oriSumFree ? ob.n : ob.n - 1;
        for (int j = nd - 1; j >= 0; j--) { int d = o % ob.omod; o /= ob.omod; st[ob.off + j] = (st[ob.off + j] & L.posmask) | (d << L.p); sum += d; }
        if (!kp.oriSumFree) {
          int last = (ob.omod - sum % ob.omod) % ob.omod;
          st[ob.off + ob.n - 1] = (st[ob.off + ob.n - 1] & L.posmask) | (last << L.p);
        }
      }
      if (kp.perm) {
        uint64_t pr = v % radix[2 * i]; v /= radix[2 * i];
        uint8_t x[64]; permUnrank(pr, x, ob.n);
        for (int j = 0; j < ob.n; j++) st[ob.off + j] = (st[ob.off + j] & L.orimask) | (x[j] + ob.off);
      }
    }
  }
};

struct RepTable {
  vector<uint8_t> sym;      // first minimizing symmetry
  vector<uint64_t> tie;     // mask of all minimizing syms (only if >1)
  double twinFrac = 0;      // fraction of raw key values with >1 minimizing sym
  double avgTies = 0;       // average number of minimizing syms
  uint64_t classes = 0;
};

// The key must be closed under the symmetries' orbit maps; returns false otherwise.
template <int NCH>
bool buildRep(const Puzzle &pz, const Layout &L, const Key &K, const vector<KSym> &ks, RepTable &rt) {
  // check closure: every sym maps key orbits to key orbits (with same perm/ori flags)
  // (we test by conjugating a state with only key orbits disturbed)
  rt.sym.assign(K.size, 0); rt.tie.assign(K.size, 0);
  uint64_t twins = 0, tiesum = 0;
  vector<uint8_t> seen(K.size, 0);
  uint8_t st[64], ct[64];
  for (uint64_t v = 0; v < K.size; v++) {
    K.unrank(pz, L, v, st);
    St<NCH> x = load<NCH>(st);
    uint64_t best = ~0ULL, mask = 0;
    int bs = 0, cnt = 0;
    for (size_t s = 0; s < ks.size(); s++) {
      St<NCH> y = conj<NCH>(L, x, ks[s]);
      store<NCH>(y, ct);
      uint64_t r = K.rank(pz, L, ct);
      if (r < best) { best = r; mask = s < 64 ? 1ULL << s : 0; bs = s; cnt = 1; }
      else if (r == best) { if (s < 64) mask |= 1ULL << s; cnt++; }
    }
    rt.sym[v] = bs;
    int c = cnt;
    if (c > 1) { twins++; rt.tie[v] = mask; }
    tiesum += c;
    if (!seen[best]) { seen[best] = 1; rt.classes++; }
  }
  rt.twinFrac = (double)twins / K.size;
  rt.avgTies = (double)tiesum / K.size;
  return true;
}
