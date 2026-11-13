// Brand-new puzzle model for the fast general solver prototype.
// Element semantics follow ksolve/twsearch: (a*b) = "a then b",
//   c.perm[j] = a.perm[b.perm[j]],  c.ori[j] = eps_b*a.ori[b.perm[j]] + b.ori[j]
// eps = -1 marks a mirror (orientation-reversing) element; only symmetries
// ever have eps = -1.  Positions are global (all orbits concatenated).
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <map>
#include <algorithm>
using namespace std;

struct Orbit {
  string name;
  int n, omod, off;
};

struct Elem {
  vector<uint8_t> p, o; // global perm, ori
  int eps = 1;
  vector<int> epsv; // optional per-orbit sign (empty = all eps)
};

struct Move {
  string name;
  Elem e;
  int base, twist, order;
};

struct Puzzle {
  string name;
  vector<Orbit> orbits;
  int N = 0;
  vector<int> orbitOf;   // position -> orbit
  vector<uint8_t> solvedLabel; // label of the piece that belongs at each position
  vector<Move> base, moves;
  bool uniqueLabels = true;

  int omodAt(int j) const { return orbits[orbitOf[j]].omod; }
  Elem ident() const {
    Elem e; e.p.resize(N); e.o.assign(N, 0);
    for (int i = 0; i < N; i++) e.p[i] = i;
    return e;
  }
  Elem mul(const Elem &a, const Elem &b) const {
    Elem c; c.p.resize(N); c.o.resize(N); c.eps = a.eps * b.eps;
    if (!a.epsv.empty() || !b.epsv.empty()) {
      c.epsv.resize(orbits.size());
      for (size_t k = 0; k < orbits.size(); k++) c.epsv[k] = sgn(a, k) * sgn(b, k);
    }
    for (int j = 0; j < N; j++) {
      int m = omodAt(j);
      c.p[j] = a.p[b.p[j]];
      int v = sgn(b, orbitOf[j]) * a.o[b.p[j]] + b.o[j];
      c.o[j] = ((v % m) + m) % m;
    }
    return c;
  }
  Elem inv(const Elem &a) const {
    // find b with a*b = id: b.p = a.p^-1 ; eps_b*a.o[b.p[j]] + b.o[j] = 0
    Elem b; b.p.resize(N); b.o.resize(N); b.eps = a.eps; b.epsv = a.epsv;
    for (int j = 0; j < N; j++) b.p[a.p[j]] = j;
    for (int j = 0; j < N; j++) {
      int m = omodAt(j);
      int v = -sgn(b, orbitOf[j]) * a.o[b.p[j]];
      b.o[j] = ((v % m) + m) % m;
    }
    return b;
  }
  int sgn(const Elem &e, int k) const { return e.epsv.empty() ? e.eps : e.epsv[k]; }
  bool eq(const Elem &a, const Elem &b) const {
    if (a.p != b.p || a.o != b.o) return false;
    for (size_t k = 0; k < orbits.size(); k++) if (orbits[k].omod > 2 && sgn(a, k) != sgn(b, k)) return false;
    return true;
  }
};

// ---------------------------------------------------------------------
// Minimal .tws reader (Name / Set / Solved / Move blocks; others skipped)
// ---------------------------------------------------------------------
static vector<string> toks(const string &s) {
  vector<string> r; string t; istringstream is(s);
  while (is >> t) r.push_back(t);
  return r;
}

inline Puzzle readTws(const string &fn) {
  Puzzle pz;
  ifstream f(fn);
  if (!f) { fprintf(stderr, "cannot open %s\n", fn.c_str()); exit(1); }
  vector<vector<string>> lines;
  string s;
  while (getline(f, s)) {
    size_t h = s.find('#'); if (h != string::npos) s = s.substr(0, h);
    auto t = toks(s); if (!t.empty()) lines.push_back(t);
  }
  map<string, int> setIdx;
  size_t li = 0;
  // read a block of set data until End; returns per-set perm/ori (1-based or 0-based raw)
  auto readBlock = [&](vector<vector<int>> &perm, vector<vector<int>> &ori) {
    perm.assign(pz.orbits.size(), {}); ori.assign(pz.orbits.size(), {});
    while (li < lines.size() && lines[li][0] != "End") {
      auto &L = lines[li++];
      auto it = setIdx.find(L[0]);
      if (it == setIdx.end()) { fprintf(stderr, "unknown set %s\n", L[0].c_str()); exit(1); }
      int k = it->second, n = pz.orbits[k].n;
      vector<int> pv, ov;
      for (auto &x : lines[li++]) pv.push_back(atoi(x.c_str()));
      if (li < lines.size() && lines[li].size() == (size_t)n && setIdx.find(lines[li][0]) == setIdx.end() && lines[li][0] != "End") {
        for (auto &x : lines[li++]) ov.push_back(atoi(x.c_str()));
      } else ov.assign(n, 0);
      perm[k] = pv; ori[k] = ov;
    }
    li++; // End
  };
  vector<vector<int>> sp, so;
  bool haveSolved = false;
  while (li < lines.size()) {
    auto &L = lines[li];
    if (L[0] == "Name") { pz.name = L.size() > 1 ? L[1] : ""; li++; }
    else if (L[0] == "Set") {
      Orbit o; o.name = L[1]; o.n = atoi(L[2].c_str()); o.omod = L.size() > 3 ? atoi(L[3].c_str()) : 1;
      o.off = pz.N; pz.N += o.n; setIdx[o.name] = pz.orbits.size(); pz.orbits.push_back(o); li++;
    } else if (L[0] == "Solved" || L[0] == "StartState") { li++; readBlock(sp, so); haveSolved = true; }
    else if (L[0] == "Move") {
      string nm = L[1]; li++;
      vector<vector<int>> mp, mo; readBlock(mp, mo);
      Move m; m.name = nm; m.e.p.resize(pz.N); m.e.o.resize(pz.N);
      for (size_t k = 0; k < pz.orbits.size(); k++) {
        auto &ob = pz.orbits[k];
        int lo = 1;
        if (!mp[k].empty()) { lo = *min_element(mp[k].begin(), mp[k].end()); }
        for (int j = 0; j < ob.n; j++) {
          m.e.p[ob.off + j] = mp[k].empty() ? ob.off + j : ob.off + mp[k][j] - lo;
          // ksolve-style moves give orientation by *source* position
          m.e.o[ob.off + j] = mp[k].empty() ? 0 : mo[k][mp[k][j] - lo] % ob.omod;
        }
      }
      m.base = pz.base.size(); pz.base.push_back(m);
    } else if (L[0] == "Scramble" || L[0] == "ScrambleState" || L[0] == "ScrambleAlg" || L[0] == "CPOS") {
      break;
    } else { // skip unknown block until End if it looks like a block
      li++;
      if (L[0] == "Illegal" || L[0] == "MoveSequence" || L[0] == "MoveAlias" || L[0] == "Ignore") {
        while (li < lines.size() && lines[li][0] != "End") li++;
        li++;
      }
    }
  }
  pz.orbitOf.resize(pz.N);
  for (size_t k = 0; k < pz.orbits.size(); k++)
    for (int j = 0; j < pz.orbits[k].n; j++) pz.orbitOf[pz.orbits[k].off + j] = k;
  pz.solvedLabel.resize(pz.N);
  for (size_t k = 0; k < pz.orbits.size(); k++) {
    auto &ob = pz.orbits[k];
    vector<int> v;
    if (haveSolved && !sp[k].empty()) v = sp[k]; else for (int j = 0; j < ob.n; j++) v.push_back(j);
    int lo = *min_element(v.begin(), v.end());
    vector<int> seen(256, 0);
    for (int j = 0; j < ob.n; j++) {
      pz.solvedLabel[ob.off + j] = v[j] - lo;
      if (seen[v[j] - lo]++) pz.uniqueLabels = false;
    }
  }
  // expand powers
  for (auto &bm : pz.base) {
    vector<Elem> pw; Elem cur = bm.e;
    Elem id = pz.ident();
    while (!pz.eq(cur, id)) { pw.push_back(cur); cur = pz.mul(cur, bm.e); }
    int ord = pw.size() + 1;
    bm.order = ord;
    for (int k = 1; k < ord; k++) {
      Move m; m.e = pw[k - 1]; m.base = bm.base; m.twist = k; m.order = ord;
      if (k == ord - 1) m.name = bm.name + "'";
      else if (2 * k <= ord) m.name = bm.name + (k == 1 ? "" : to_string(k));
      else m.name = bm.name + to_string(ord - k) + "'";
      pz.moves.push_back(m);
    }
  }
  return pz;
}
