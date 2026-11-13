// General abstraction chooser: per orbit pick a label partition (finest solved
// coloring, any partition induced by a candidate subgroup H, or ignore), keep
// orientation only where every label class is a single piece (then equivariance
// is automatic), require invariance under a symmetry subgroup, and pick the most
// informative combination whose symmetry-reduced size fits the budget.
#pragma once
#include "coset.h"
#include <cmath>
#include <unordered_set>

struct OrbOpt { vector<int> lab; int nl; bool ori; double lg; string name; };

struct LatticeChoice {
  Abstraction A;
  vector<int> stab;        // indices of symmetries preserving it
  int keyOrbit = -1;
  double lgSize = 0, reduced = 0;
};

static double lgMultinom(const vector<int> &lab, int nl) {
  vector<int> c(nl, 0); for (int l : lab) c[l]++;
  double r = lgamma(lab.size() + 1.0); for (int x : c) r -= lgamma(x + 1.0);
  return r;
}

// log of the number of reachable projections of orbit k (labels lab, optional ori);
// exact BFS when the raw count is small, else the raw count.
inline double reachLg(const Puzzle &pz, int k, const vector<int> &lab, bool ori, double rawLg) {
  if (rawLg > log(ori ? 2e6 : 1e5)) return rawLg;
  auto &ob = pz.orbits[k];
  int n = ob.n;
  // state: per position, label*omod + ori
  string s0(n, 0);
  for (int j = 0; j < n; j++) s0[j] = lab[j] * (ori ? ob.omod : 1);
  unordered_set<string> seen{s0}; vector<string> q{s0};
  for (size_t h = 0; h < q.size(); h++) {
    for (auto &mv : pz.moves) {
      const Elem &m = mv.e; string t(n, 0);
      for (int j = 0; j < n; j++) {
        int src = m.p[ob.off + j] - ob.off;
        int v = (unsigned char)q[h][src];
        if (ori) { int l = v / ob.omod, o = v % ob.omod; o = ((o + m.o[ob.off + j]) % ob.omod + ob.omod) % ob.omod; v = l * ob.omod + o; }
        t[j] = v;
      }
      if (seen.insert(t).second) q.push_back(t);
    }
  }
  return log((double)q.size());
}

inline vector<vector<OrbOpt>> orbitOptions(const Puzzle &pz, const vector<Elem> &syms) {
  int nb = pz.base.size(), no = pz.orbits.size();
  vector<vector<OrbOpt>> opts(no);
  auto add = [&](int k, vector<int> lab, const string &nm) {
    // canonical relabel (first occurrence order)
    map<int, int> ids; for (int &l : lab) { if (!ids.count(l)) { int id = ids.size(); ids[l] = id; } l = ids[l]; }
    int nl = ids.size();
    if (nl <= 1) return;
    for (auto &o : opts[k]) if (o.lab == lab) return;
    auto &ob = pz.orbits[k];
    double lg = lgMultinom(lab, nl);
    bool single = nl == ob.n;
    opts[k].push_back({lab, nl, false, reachLg(pz, k, lab, false, lg), nm});
    if (single && ob.omod > 1) {
      double lo = lg + (ob.n - 1) * log((double)ob.omod);
      opts[k].push_back({lab, nl, true, reachLg(pz, k, lab, true, lo), nm + "+ori"});
    }
  };
  // finest: the solved coloring
  for (int k = 0; k < no; k++) {
    auto &ob = pz.orbits[k];
    vector<int> lab(ob.n); for (int j = 0; j < ob.n; j++) lab[j] = pz.solvedLabel[ob.off + j];
    add(k, lab, "fine");
  }
  // H-induced partitions: per base absent / <b^k>
  vector<vector<int>> bopt(nb);
  for (int b = 0; b < nb; b++) {
    int ord = 0; for (auto &m : pz.moves) if (m.base == b) ord = m.order;
    bopt[b].push_back(0); for (int k = 1; k < ord; k++) if (ord % k == 0) bopt[b].push_back(k);
  }
  long long total = 1; for (auto &o : bopt) total *= o.size();
  if (total > 200000) total = 200000;
  CosetFinder cf(pz, syms);
  for (long long code = 1; code < total; code++) {
    long long c = code; vector<int> H; string nm = "H<";
    vector<int> ch(nb);
    for (int b = 0; b < nb; b++) { ch[b] = bopt[b][c % bopt[b].size()]; c /= bopt[b].size(); }
    for (size_t i = 0; i < pz.moves.size(); i++) { int k = ch[pz.moves[i].base]; if (k && pz.moves[i].twist % k == 0) H.push_back(i); }
    for (int b = 0; b < nb; b++) if (ch[b]) nm += pz.base[b].name + (ch[b] > 1 ? "^" + to_string(ch[b]) : "") + ",";
    nm += ">";
    // labels only (union-find in derive); skip the gauge work by passing no symmetries
    vector<Elem> none;
    CosetFinder cf0(pz, none);
    Abstraction A = cf0.derive(H, false);
    for (int k = 0; k < no; k++) {
      auto &ob = pz.orbits[k];
      vector<int> lab(ob.n); for (int j = 0; j < ob.n; j++) lab[j] = A.label[ob.off + j];
      add(k, lab, nm);
    }
  }
  return opts;
}

// symmetries preserving the combined labeling (with ignore and ori flags)
inline vector<int> stabilizer(const Puzzle &pz, const vector<Elem> &syms, const vector<int> &glab, const vector<int> &flag) {
  vector<int> st;
  int N = pz.N;
  for (size_t s = 0; s < syms.size(); s++) {
    const Elem &S = syms[s];
    bool ok = true;
    map<int, int> fwd, bwd;
    for (int j = 0; j < N && ok; j++) {
      int t = S.p[j];
      if (flag[pz.orbitOf[j]] != flag[pz.orbitOf[t]]) { ok = false; break; }
      int a = glab[j], b = glab[t];
      auto f = fwd.find(a); if (f == fwd.end()) fwd[a] = b; else if (f->second != b) ok = false;
      auto g = bwd.find(b); if (g == bwd.end()) bwd[b] = a; else if (g->second != a) ok = false;
    }
    if (ok) st.push_back(s);
  }
  return st;
}

inline LatticeChoice chooseAbstraction(const Puzzle &pz, const vector<Elem> &syms, double budget, bool verbose = true) {
  auto opts = orbitOptions(pz, syms);
  int no = pz.orbits.size();
  for (int k = 0; k < no; k++) opts[k].insert(opts[k].begin(), OrbOpt{{}, 0, false, 0, "ignore"});
  if (verbose) for (int k = 0; k < no; k++) {
    printf("  orbit %s: %zu options:", pz.orbits[k].name.c_str(), opts[k].size());
    for (auto &o : opts[k]) printf(" [%s %.3g]", o.name.c_str(), exp(o.lg));
    printf("\n");
  }
  LatticeChoice best; best.lgSize = -1;
  vector<int> idx(no, 0);
  while (true) {
    double lg = 0; int nori = 0; bool any = false;
    for (int k = 0; k < no; k++) { auto &o = opts[k][idx[k]]; lg += o.lg; nori += o.ori; any |= o.nl > 0; }
    if (any && nori <= 1 && lg > best.lgSize && lg - log(budget) < log((double)syms.size()) + 1e-9) {
      // labeling + flags
      vector<int> glab(pz.N, -1), flag(no, 0);
      for (int k = 0; k < no; k++) {
        auto &o = opts[k][idx[k]]; auto &ob = pz.orbits[k];
        flag[k] = o.nl == 0 ? 0 : 1 + o.ori;
        for (int j = 0; j < ob.n; j++) glab[ob.off + j] = o.nl == 0 ? -1 - k : 1000 * k + o.lab[j];
      }
      // ignored orbits: one shared label each is fine; orbit swaps need equal flags
      auto st = stabilizer(pz, syms, glab, flag);
      double red = exp(lg) / st.size();
      if (red <= budget) {
        // key orbit: the oriented one, else the smallest nontrivial orbit projection
        int key = -1; double kl = 1e300;
        for (int k = 0; k < no; k++) { auto &o = opts[k][idx[k]]; if (o.ori) { key = k; kl = -1; } }
        if (key < 0) for (int k = 0; k < no; k++) { auto &o = opts[k][idx[k]]; if (o.nl > 0 && o.lg > log(4.0 * st.size()) && o.lg < kl) { kl = o.lg; key = k; } }
        bool keyOk = key >= 0 && opts[key][idx[key]].lg < log(5e7) && pz.orbits[key].n <= 16;
        for (int k = 0; k < no; k++) if (pz.orbits[k].n > 16 && opts[k][idx[k]].nl) keyOk = false;
        if (keyOk) {
          LatticeChoice c;
          c.lgSize = lg; c.reduced = red; c.stab = st; c.keyOrbit = key;
          Abstraction &A = c.A;
          A.label.assign(pz.N, -1); A.nlabels.assign(no, 0); A.keepOri.assign(no, false); A.ignore.assign(no, false);
          A.gauge.assign(pz.N, 0);
          for (int k = 0; k < no; k++) {
            auto &o = opts[k][idx[k]]; auto &ob = pz.orbits[k];
            if (o.nl == 0) { A.ignore[k] = true; A.nlabels[k] = 1; for (int j = 0; j < ob.n; j++) A.label[ob.off + j] = 0; continue; }
            A.nlabels[k] = o.nl; A.keepOri[k] = o.ori;
            for (int j = 0; j < ob.n; j++) A.label[ob.off + j] = o.lab[j];
            A.desc += ob.name + ":" + o.name + "(" + to_string(o.nl) + ") ";
          }
          best = c;
        }
      }
    }
    int k = 0;
    while (k < no && ++idx[k] == (int)opts[k].size()) idx[k++] = 0;
    if (k == no) break;
  }
  if (verbose && best.lgSize >= 0)
    printf("  chosen: %s size %.4g, |S|=%zu, reduced %.4g, key %s\n", best.A.desc.c_str(), exp(best.lgSize), best.stab.size(), best.reduced,
           pz.orbits[best.keyOrbit].name.c_str());
  return best;
}
