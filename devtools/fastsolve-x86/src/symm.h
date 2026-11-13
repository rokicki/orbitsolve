// Automatic symmetry discovery: find all S = (sigma, d, eps) such that
// S^-1 * m * S is a move for every base move m.  eps=-1 gives mirrors.
// Relation (from mul semantics):  m*S == S*m'  <=>
//   sigma[m'.p[j]] = m.p[sigma[j]]
//   d[m'.p[j]]     = eps*m.o[sigma[j]] + d[j] - m'.o[j]   (mod omod)
#pragma once
#include "puzzle.h"
#include <functional>
#include <set>

struct SymFinder {
  const Puzzle &pz;
  vector<Elem> found;
  int eps;
  vector<int> epsv; // per-orbit orientation sign
  vector<int> sig, d;       // -1 = unassigned
  vector<int> img;          // base move -> move index image (-1 unassigned)
  vector<vector<int>> cand; // candidate images per base move
  long long nodes = 0, nUnassigned = 0, nDup = 0, nVerify = 0;
  vector<int> wl;           // colour refinement of positions (invariant under symmetries)
  SymFinder(const Puzzle &p) : pz(p) {
    // 1-dim Weisfeiler-Leman over the set of move permutations: a position's colour is
    // refined by the multiset over all moves of (cycle length, colour of its image)
    wl.assign(pz.N, 0);
    for (int j = 0; j < pz.N; j++) wl[j] = pz.orbits[pz.orbitOf[j]].n * 64 + pz.omodAt(j);
    for (int it = 0; it < 20; it++) {
      vector<vector<long long>> sg(pz.N);
      for (int j = 0; j < pz.N; j++) {
        sg[j].push_back(wl[j]);
        vector<long long> v;
        for (auto &mv : pz.moves) {
          int len = 1, x = mv.e.p[j]; while (x != j) { x = mv.e.p[x]; len++; }
          v.push_back((long long)len * 1000000 + wl[mv.e.p[j]]);
        }
        sort(v.begin(), v.end()); sg[j].insert(sg[j].end(), v.begin(), v.end());
      }
      map<vector<long long>, int> ids; for (auto &x : sg) ids.emplace(x, 0);
      int c = 0; for (auto &kv : ids) kv.second = c++;
      vector<int> nw(pz.N); for (int j = 0; j < pz.N; j++) nw[j] = ids[sg[j]];
      int before = set<int>(wl.begin(), wl.end()).size();
      wl = nw;
      if ((int)ids.size() == before) break;
    }
  }

  // signature of an element: per-orbit sorted list of (cycle len, twist sum) pairs
  vector<long long> signature(const Elem &e) {
    vector<long long> r;
    vector<vector<long long>> blocks;
    vector<int> seen(pz.N, 0);
    for (size_t k = 0; k < pz.orbits.size(); k++) {
      auto &ob = pz.orbits[k];
      vector<long long> cyc;
      for (int j = ob.off; j < ob.off + ob.n; j++) {
        if (seen[j]) continue;
        int len = 0, tw = 0, x = j;
        while (!seen[x]) { seen[x] = 1; len++; tw += e.o[x]; x = e.p[x]; }
        tw %= ob.omod;
        // twist sum of a cycle can map to its negative under mirrors; use min
        int t2 = (ob.omod - tw) % ob.omod;
        cyc.push_back(len * 1000 + min(tw, t2));
      }
      sort(cyc.begin(), cyc.end());
      cyc.insert(cyc.begin(), ob.n * 100 + ob.omod);
      blocks.push_back(cyc);
    }
    sort(blocks.begin(), blocks.end());
    for (auto &b : blocks) r.insert(r.end(), b.begin(), b.end());
    return r;
  }

  vector<int> orbMap, orbUsed; // orbit -> target orbit
  set<vector<int>> keys;        // dedupe trivially-equivalent symmetries
  bool assign(int j, int s, int dv, vector<pair<int,int>> &trail) {
    int m = pz.omodAt(j);
    dv = ((dv % m) + m) % m;
    if (sig[j] >= 0) return sig[j] == s && d[j] == dv;
    if (wl[j] != wl[s]) return false;
    int ko = pz.orbitOf[j], kt = pz.orbitOf[s];
    int flag = 0;
    if (orbMap[ko] < 0) {
      if (orbUsed[kt] || pz.orbits[ko].n != pz.orbits[kt].n || pz.orbits[ko].omod != pz.orbits[kt].omod) return false;
      orbMap[ko] = kt; orbUsed[kt] = 1; flag = 1;
    } else if (orbMap[ko] != kt) return false;
    for (int k = pz.orbits[ko].off, e = k + pz.orbits[ko].n; k < e; k++)
      if (sig[k] == s) { if (flag) { orbUsed[kt] = 0; orbMap[ko] = -1; } return false; }
    sig[j] = s; d[j] = dv; trail.push_back({j, flag});
    return true;
  }
  bool propagate(vector<pair<int,int>> &trail) {
    bool changed = true;
    while (changed) {
      changed = false;
      for (size_t b = 0; b < pz.base.size(); b++) {
        if (img[b] < 0) continue;
        const Elem &m = pz.base[b].e, &mp = pz.moves[img[b]].e;
        for (int j = 0; j < pz.N; j++) {
          if (sig[j] < 0) continue;
          int t = mp.p[j];
          int ns = m.p[sig[j]];
          int nd = epsv[pz.orbitOf[j]] * m.o[sig[j]] + d[j] - mp.o[j];
          bool was = sig[t] >= 0;
          if (!assign(t, ns, nd, trail)) return false;
          if (!was) changed = true;
        }
      }
    }
    return true;
  }
  void undo(vector<pair<int,int>> &trail, size_t to) {
    while (trail.size() > to) {
      int j = trail.back().first;
      if (trail.back().second) { int ko = pz.orbitOf[j]; orbUsed[orbMap[ko]] = 0; orbMap[ko] = -1; }
      sig[j] = -1; d[j] = -1; trail.pop_back();
    }
  }
  void search(vector<pair<int,int>> &trail) {
    nodes++;
    bool allImg = true;
    for (auto v : img) if (v < 0) allImg = false;
    // 1. seed the first orbit (or any untouched orbit once all images are fixed)
    for (size_t k = 0; k < pz.orbits.size(); k++) {
      auto &ob = pz.orbits[k];
      if (orbMap[k] >= 0) continue;
      if (k > 0 && !allImg) continue;
      for (int s = 0; s < pz.N; s++)
        for (int dv = 0; dv < ob.omod; dv++) {
          size_t t0 = trail.size();
          if (assign(ob.off, s, dv, trail) && propagate(trail)) search(trail);
          undo(trail, t0);
        }
      return;
    }
    // 2. choose the unassigned base move with the most information
    int best = -1, bestc = -1;
    for (size_t b = 0; b < pz.base.size(); b++) {
      if (img[b] >= 0) continue;
      int c = 0;
      for (int j = 0; j < pz.N; j++) if (sig[j] >= 0 && pz.base[b].e.p[sig[j]] != sig[j]) c++;
      if (c > bestc) { bestc = c; best = b; }
    }
    if (best >= 0) {
      int b = best;
      for (int c : cand[b]) {
        bool used = false;
        for (auto v : img) if (v >= 0 && pz.moves[v].base == pz.moves[c].base) used = true;
        if (used) continue;
        img[b] = c;
        size_t t0 = trail.size();
        if (propagate(trail)) search(trail);
        undo(trail, t0);
        img[b] = -1;
      }
      return;
    }
    for (int j = 0; j < pz.N; j++) if (sig[j] < 0) { nUnassigned++; return; }
    if (keys.count(img)) { nDup++; return; }
    nVerify++;
    Elem S; S.p.resize(pz.N); S.o.resize(pz.N); S.eps = eps; S.epsv = epsv;
    for (int j = 0; j < pz.N; j++) { S.p[j] = sig[j]; S.o[j] = d[j]; }
    Elem Si = pz.inv(S);
    for (auto &bm : pz.base) {
      Elem c = pz.mul(pz.mul(Si, bm.e), S);
      bool ok = false;
      for (auto &mv : pz.moves) if (pz.eq(c, mv.e)) { ok = true; break; }
      if (!ok) return;
    }
    keys.insert(img);
    found.push_back(S);
  }
  vector<Elem> run(bool mirrors = true) {
    cand.assign(pz.base.size(), {});
    vector<vector<long long>> msig;
    for (auto &mv : pz.moves) msig.push_back(signature(mv.e));
    for (size_t b = 0; b < pz.base.size(); b++) {
      auto sb = signature(pz.base[b].e);
      for (size_t c = 0; c < pz.moves.size(); c++)
        if (msig[c] == sb) cand[b].push_back(c);
    }
    vector<int> oriented;
    for (size_t k = 0; k < pz.orbits.size(); k++) if (pz.orbits[k].omod > 2) oriented.push_back(k);
    int nsign = mirrors ? (1 << oriented.size()) : 1;
    for (int sm = 0; sm < nsign; sm++) {
      epsv.assign(pz.orbits.size(), 1);
      for (size_t i = 0; i < oriented.size(); i++) if ((sm >> i) & 1) epsv[oriented[i]] = -1;
      eps = sm ? -1 : 1;
      sig.assign(pz.N, -1); d.assign(pz.N, -1); img.assign(pz.base.size(), -1);
      orbMap.assign(pz.orbits.size(), -1); orbUsed.assign(pz.orbits.size(), 0);
      vector<pair<int,int>> trail;
      search(trail);
    }
    return found;
  }
};

// Does symmetry S preserve the solved state up to a consistent relabeling
// of identical pieces?  Returns the color map (per orbit label) or empty.
inline bool symPreservesSolved(const Puzzle &pz, const Elem &S, vector<int> *cmapOut = nullptr) {
  // conj solved labels: position j receives the piece from S.p[j];
  // requirement: label(S.p[j]) -> label(j) is a consistent bijection per orbit
  vector<int> cmap(pz.N * 0 + 256 * pz.orbits.size(), -1);
  for (int j = 0; j < pz.N; j++) {
    int k = pz.orbitOf[j];
    int a = pz.solvedLabel[S.p[j]], b = pz.solvedLabel[j];
    int &c = cmap[k * 256 + a];
    if (c < 0) c = b; else if (c != b) return false;
  }
  if (cmapOut) *cmapOut = cmap;
  return true;
}

// Whole-puzzle rotations given as moves (e.g. "y"): a base move whose conjugation
// maps every base move to a move and which fixes the solved coloring exactly.
// They cost nothing (twsearch semantics), so drop them from the move set; the
// symmetry finder then recovers them as symmetries.
inline vector<string> dropRotations(Puzzle &pz) {
  vector<string> dropped; vector<int> keep;
  for (size_t b = 0; b < pz.base.size(); b++) {
    const Elem &r = pz.base[b].e; Elem ri = pz.inv(r);
    bool rot = true;
    for (auto &bm : pz.base) {
      Elem c = pz.mul(pz.mul(ri, bm.e), r); bool ok = false;
      for (auto &mv : pz.moves) if (pz.eq(c, mv.e)) { ok = true; break; }
      if (!ok) { rot = false; break; }
    }
    for (int j = 0; j < pz.N && rot; j++) if (pz.solvedLabel[r.p[j]] != pz.solvedLabel[j] || r.o[j]) rot = false;
    if (rot) dropped.push_back(pz.base[b].name); else keep.push_back(b);
  }
  if (dropped.empty()) return dropped;
  vector<Move> nb, nm; vector<int> remap(pz.base.size(), -1);
  for (int b : keep) { remap[b] = nb.size(); nb.push_back(pz.base[b]); nb.back().base = remap[b]; }
  for (auto &m : pz.moves) if (remap[m.base] >= 0) { Move x = m; x.base = remap[m.base]; nm.push_back(x); }
  pz.base = nb; pz.moves = nm;
  return dropped;
}

// symmetry search can be slow on big puzzles with identical pieces: cache by puzzle content
inline vector<Elem> findSymsCached(const Puzzle &pz, const string &cacheFile) {
  {
    ifstream f(cacheFile);
    if (f) {
      size_t n; f >> n; vector<Elem> r(n);
      for (auto &e : r) {
        e.p.resize(pz.N); e.o.resize(pz.N); int ne;
        f >> e.eps >> ne; e.epsv.resize(ne); for (auto &v : e.epsv) f >> v;
        for (auto &v : e.p) { int x; f >> x; v = x; }
        for (auto &v : e.o) { int x; f >> x; v = x; }
      }
      if (f) return r;
    }
  }
  SymFinder sf(pz); auto r = sf.run(true);
  ofstream f(cacheFile);
  f << r.size() << "\n";
  for (auto &e : r) {
    f << e.eps << " " << e.epsv.size(); for (auto v : e.epsv) f << " " << v; f << "\n";
    for (auto v : e.p) f << (int)v << " "; f << "\n";
    for (auto v : e.o) f << (int)v << " "; f << "\n";
  }
  return r;
}
