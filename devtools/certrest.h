// certrest.h: the rest of the cube for certsolve (included after the oblique code): mid (T-centre)
// orbits on odd N, diagonal (X-centre) orbits, corners (and midges on odd N), wing orbits.  Each
// centre or wing stage plays a line of its certificate (mid, diagonal, wing); corners are
// solved optimally (even N, full table) or by a two-phase search held to 20 moves (odd N, corners and
// midges as a 3x3x3 with face turns only).
#include <functional>
#include "certcache.h"

static inline int groupMask(const std::string &s) {
  if (s == "none") return 0;
  int m = 0;
  for (char ch : s) {
    const char *p = strchr(cc::FACES, ch);
    if (!p || !*p || (m >> (p - cc::FACES) & 1)) return -1;
    m |= 1 << (p - cc::FACES);
  }
  return m;
}
static inline int wingMask(const std::string &s) {
  if (s == "none") return 0;
  int m = 0;
  for (char ch : s) {
    if (ch < 'a' || ch > 'x' || (m >> (ch - 'a') & 1)) return -1;
    m |= 1 << (ch - 'a');
  }
  return m;
}
// the segment files wNN.txt of a stage directory, in name order
static inline bool listSegs(const std::string &dir, std::vector<std::pair<std::string, int>> &files) {
  files.clear();
  DIR *d = opendir(dir.c_str());
  if (!d) return false;
  while (dirent *e = readdir(d)) {
    std::string n = e->d_name;
    if (n.size() > 5 && n[0] == 'w' && n.compare(n.size() - 4, 4, ".txt") == 0) files.push_back({dir + "/" + n, atoi(n.c_str() + 1)});
  }
  closedir(d);
  std::sort(files.begin(), files.end());
  return !files.empty();
}
static inline bool readAll(const std::string &fn, std::string &data) {
  FILE *f = fopen(fn.c_str(), "r");
  if (!f) return false;
  data.clear();
  char buf[1 << 16];
  size_t r;
  while ((r = fread(buf, 1, sizeof buf, f)) > 0) data.append(buf, r);
  fclose(f);
  return true;
}
static inline std::vector<size_t> lineStarts(const std::string &data) {
  std::vector<size_t> s;
  for (size_t i = 0; i < data.size();) {
    s.push_back(i);
    const size_t e = data.find('\n', i);
    i = e == std::string::npos ? data.size() : e + 1;
  }
  return s;
}
static inline std::string lineAtPos(const std::string &data, size_t s0) {
  size_t e = data.find('\n', s0);
  if (e == std::string::npos) e = data.size();
  return data.substr(s0, e - s0);
}
static inline std::string baseName(const std::string &p) { const size_t s = p.rfind('/'); return s == std::string::npos ? p : p.substr(s + 1); }

// ---- 24-facelet colour orbits (mid, diagonal) in the certificate frame of a reference cube
struct Sym24 { int fac[24], gmap[6], face[6]; bool flip; };
struct Orb24 {
  int mp[6][4][4][24];  // dst form: the piece at k goes to mp[f][d][a][k]; certificate depths 1..nd
  int nd = 2;
  std::vector<Sym24> syms;  // the 48 cube symmetries
  std::vector<int> inv;
  bool init(int refN, const std::vector<int> &mf, int nd_) {
    nd = nd_;
    cc::Cube R(refN);
    const int real[4] = {0, 1, 2, (refN + 1) / 2};  // certificate depth 3 = the middle layer
    std::vector<int> fidx(R.pos.size(), -1);
    for (int k = 0; k < 24; k++) fidx[mf[k]] = k;
    for (int f = 0; f < 6; f++)
      for (int d = 1; d <= nd; d++)
        for (int a = 1; a <= 3; a++)
          for (int k = 0; k < 24; k++) {
            int i = mf[k];
            for (int q = 0; q < a; q++) i = R.perms[f][real[d] - 1][i];
            if (fidx[i] < 0) return false;
            mp[f][d][a][k] = fidx[i];
          }
    const int pm[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    const int par[6] = {1, -1, -1, 1, 1, -1};
    for (int pi = 0; pi < 6; pi++)
      for (int sg = 0; sg < 8; sg++) {
        Sym24 y;
        auto map = [&](const std::array<int, 3> &p) {
          std::array<int, 3> q;
          for (int r = 0; r < 3; r++) q[r] = p[pm[pi][r]] * ((sg >> r & 1) ? -1 : 1);
          return q;
        };
        for (int k = 0; k < 24; k++) {
          const auto q = map(R.pos[mf[k]]);
          y.fac[k] = -1;
          for (int x = 0; x < 24; x++) if (R.pos[mf[x]] == q) y.fac[k] = x;
          if (y.fac[k] < 0) return false;
        }
        for (int g = 0; g < 6; g++) {
          y.gmap[g] = -1;
          for (int k = 0; k < 24; k++)
            if (k / 4 == g) {
              if (y.gmap[g] >= 0 && y.gmap[g] != y.fac[k] / 4) return false;
              y.gmap[g] = y.fac[k] / 4;
            }
        }
        for (int f = 0; f < 6; f++) {
          const auto q = map(cc::Cube::normalOf(f));
          for (int f2 = 0; f2 < 6; f2++) if (cc::Cube::normalOf(f2) == q) y.face[f] = f2;
        }
        y.flip = par[pi] * ((sg & 1) ? -1 : 1) * ((sg & 2) ? -1 : 1) * ((sg & 4) ? -1 : 1) < 0;
        syms.push_back(y);
      }
    inv.assign(syms.size(), -1);
    for (size_t s = 0; s < syms.size(); s++)
      for (size_t t = 0; t < syms.size() && inv[s] < 0; t++) {
        bool id = true;
        for (int k = 0; k < 24 && id; k++) id = syms[t].fac[syms[s].fac[k]] == k;
        if (id) inv[s] = t;
      }
    for (int x : inv) if (x < 0) return false;
    return true;
  }
  void apply(const std::vector<cc::Mv> &mv, int8_t *col) const {
    int8_t b[24];
    for (auto &m : mv) {
      for (int k = 0; k < 24; k++) b[mp[m.face][m.depth][m.amt][k]] = col[k];
      memcpy(col, b, 24);
    }
  }
};
static inline std::vector<cc::Mv> symMoves24(const Sym24 &y, const std::vector<cc::Mv> &mv) {
  std::vector<cc::Mv> r;
  for (auto &m : mv) r.push_back({y.face[m.face], m.depth, y.flip ? 4 - m.amt : m.amt});
  return r;
}

// dense rank of a stage state: per target group in order, the colex rank of its 4 positions among the
// free positions not taken by earlier target groups
struct Dense {
  int nf = 0, ng = 0, free[24], fidx[24], tg[6];
  uint64_t radix[6], total = 1;
  uint64_t binom[25][5];
  void init(const std::vector<int> &freeK, const std::vector<int> &tgs) {
    nf = freeK.size(), ng = tgs.size();
    for (int k = 0; k < 24; k++) fidx[k] = -1;
    for (int i = 0; i < nf; i++) free[i] = freeK[i], fidx[free[i]] = i;
    for (int i = 0; i < ng; i++) tg[i] = tgs[i];
    for (int a = 0; a <= 24; a++) for (int b = 0; b <= 4; b++) binom[a][b] = b == 0 ? 1 : a == 0 ? 0 : binom[a - 1][b - 1] + binom[a - 1][b];
    for (int i = 0; i < ng; i++) radix[i] = binom[nf - 4 * i][4], total *= radix[i];
  }
  // col: colour (0..5 or -1) per certificate position
  uint64_t rank(const int8_t *col) const {
    uint32_t taken = 0;
    uint64_t r = 0;
    for (int g = 0; g < ng; g++) {
      uint64_t rg = 0;
      int j = 0, idx = 0;
      for (int i = 0; i < nf; i++) {
        if (taken >> i & 1) continue;
        if (col[free[i]] == tg[g]) { j++; rg += binom[idx][j]; taken |= 1u << i; }
        idx++;
      }
      r = r * radix[g] + rg;
    }
    return r;
  }
  void unrank(uint64_t r, int8_t *col, int S) const {
    for (int k = 0; k < 24; k++) col[k] = (S >> (k / 4) & 1) ? k / 4 : -1;
    uint64_t rg[6];
    for (int g = ng - 1; g >= 0; g--) rg[g] = r % radix[g], r /= radix[g];
    uint32_t taken = 0;
    for (int g = 0; g < ng; g++) {
      int rem[24], n = 0;
      for (int i = 0; i < nf; i++) if (!(taken >> i & 1)) rem[n++] = i;
      uint64_t x = rg[g];
      for (int j = 4, p = n - 1; j > 0; p--)
        if (x >= binom[p][j]) { x -= binom[p][j]; taken |= 1u << rem[p]; col[free[rem[p]]] = tg[g]; j--; }
    }
  }
};


// a mid or diagonal stage <solved>..<target>: one line per class up to the cube symmetries that keep
// the solved and target groups; a group is the four pieces of one face colour
struct ColStage {
  std::string name;
  int S = 0, T = 0, nf = 0, base = 0, longest = 0;
  std::vector<int> tg, freeK, ss;
  const Orb24 *O = nullptr;
  std::unordered_map<uint64_t, uint32_t> have;
  std::vector<uint64_t> off;
  std::vector<uint8_t> bytes;
  uint64_t keyOf(const int8_t *col) const {
    uint64_t k = 0;
    for (int i = 0; i < nf; i++) {
      const int g = col[freeK[i]];
      int dg = 0;
      if (g >= 0)
        for (size_t t = 0; t < tg.size(); t++) if (tg[t] == g) dg = t + 1;
      k = k * base + dg;
    }
    return k;
  }
  // concise form: rebuilt distances per state (Dense rank), and the actions
  Dense D;
  std::vector<uint8_t> R;
  struct CAct { int dst[24]; std::vector<cc::Mv> mv; };
  std::vector<CAct> cacts;
  int worst = 0;
  void stageCol(const int8_t *col, int8_t *c) const {
    for (int k = 0; k < 24; k++) c[k] = (col[k] >= 0 && ((S | T) >> col[k] & 1)) ? col[k] : -1;
  }
  // the concise stage: the lines read are kept lines; actions are the 18 face turns and every kept line
  // of at most actLen moves under the 48 symmetries and inverted (those keeping the solved groups' sets);
  // a search from the goal and the kept lines' positions gives every state its length
  bool build(int actLen, int threads) {
    D.init(freeK, tg);
    const uint64_t total = D.total;
    {
      std::unordered_map<std::string, size_t> seen;
      auto add = [&](const std::vector<cc::Mv> &mv) {
        CAct a;
        a.mv = mv;
        for (int k = 0; k < 24; k++) a.dst[k] = k;
        for (auto &m : mv) for (int k = 0; k < 24; k++) a.dst[k] = O->mp[m.face][m.depth][m.amt][a.dst[k]];
        bool id = true;
        for (int k = 0; k < 24; k++) { if ((S >> (k / 4) & 1) && a.dst[k] / 4 != k / 4) return; id &= a.dst[k] == k; }
        if (id) return;
        const std::string key((const char *)a.dst, sizeof a.dst);
        auto it = seen.find(key);
        if (it == seen.end()) { seen[key] = cacts.size(); cacts.push_back(a); }
        else if (cacts[it->second].mv.size() > mv.size()) cacts[it->second] = a;
      };
      for (int f = 0; f < 6; f++) for (int a = 1; a <= 3; a++) add({{f, 1, a}});
      for (size_t li = 0; li + 1 < off.size(); li++) {
        const auto mv = lineAt(li);
        if (mv.empty() || (int)mv.size() > actLen) continue;
        for (auto &y : O->syms) {
          auto m2 = symMoves24(y, mv);
          add(m2);
          std::vector<cc::Mv> inv;
          for (auto it = m2.rbegin(); it != m2.rend(); ++it) inv.push_back({it->face, it->depth, 4 - it->amt});
          add(inv);
        }
      }
    }
    R.assign(total, 255);
    uint64_t fp = fnv(bytes.data(), bytes.size(), fnv(off.data(), off.size() * 8, fnv(name.data(), name.size())));
    fp = fnv(&actLen, sizeof actLen, fp);
    const std::string cpath = cachePath((O->nd == 3 ? "mid-" : "diag-") + name, fp);
    if (!readCache(cpath, {{R.data(), total}})) {
      int8_t g[24];
      for (int k = 0; k < 24; k++) g[k] = ((S | T) >> (k / 4) & 1) ? k / 4 : -1;
      R[D.rank(g)] = 0;
      int maxV = 0;
      for (size_t li = 0; li + 1 < off.size(); li++) {
        int8_t a[24], b[24], c[24];
        memcpy(a, g, 24);
        const auto mv = lineAt(li);
        for (auto it = mv.rbegin(); it != mv.rend(); ++it) {
          for (int k = 0; k < 24; k++) b[O->mp[it->face][it->depth][4 - it->amt][k]] = a[k];
          memcpy(a, b, 24);
        }
        for (int s : ss) {
          const auto &y = O->syms[s];
          for (int k = 0; k < 24; k++) c[y.fac[k]] = a[k] < 0 ? -1 : y.gmap[a[k]];
          const uint64_t r = D.rank(c);
          if (R[r] > mv.size()) R[r] = mv.size();
        }
        maxV = std::max<int>(maxV, mv.size());
      }
      for (int v = 0;; v++) {
        std::atomic<uint64_t> next{0};
        auto w = [&] {
          int8_t col[24], q[24];
          for (uint64_t lo; (lo = next.fetch_add(1 << 14)) < total;)
            for (uint64_t r = lo; r < std::min(total, lo + (1 << 14)); r++) {
              const uint8_t x = __atomic_load_n(&R[r], __ATOMIC_RELAXED);
              if (x != v) continue;
              D.unrank(r, col, S);
              for (auto &a : cacts) {
                const int nv = v + a.mv.size();
                if (nv > maxV) continue;  // every state is within the stage worst (the longest kept line)
                for (int k = 0; k < 24; k++) q[k] = col[a.dst[k]];  // the state a takes to col
                const uint64_t rq = D.rank(q);
                uint8_t old = __atomic_load_n(&R[rq], __ATOMIC_RELAXED);
                while (old > nv && !__atomic_compare_exchange_n(&R[rq], &old, (uint8_t)nv, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {}
              }
            }
        };
        std::vector<std::thread> th;
        for (int t = 0; t < threads; t++) th.emplace_back(w);
        for (auto &t : th) t.join();
        uint64_t at = 0;
        for (auto x : R) at += x == v;
        fprintf(stderr, "%s: level %d, %llu states\n", name.c_str(), v, (unsigned long long)at);
        if (v >= maxV) break;
      }
      writeCache(cpath, {{R.data(), total}});
    }
    uint64_t miss = 0;
    for (auto x : R) { if (x == 255) miss++; else worst = std::max<int>(worst, x); }
    if (miss) { fprintf(stderr, "%s: %llu states not reached\n", name.c_str(), (unsigned long long)miss); return false; }
    return true;
  }
  // concise stage: a line for col, by actions down the rebuilt lengths to a kept line
  bool solveC(const int8_t *col, std::vector<cc::Mv> &out) const {
    out.clear();
    int8_t c[24], n[24];
    stageCol(col, c);
    uint64_t s = D.rank(c);
    for (int guard = 0; guard < 200; guard++) {
      const int d = R[s];
      if (d == 0) return true;
      std::vector<cc::Mv> L;
      if (solve(c, L) && (int)L.size() == d) { out.insert(out.end(), L.begin(), L.end()); return true; }
      bool step = false;
      for (auto &a : cacts) {
        const int cost = a.mv.size();
        if (cost > d) continue;
        for (int k = 0; k < 24; k++) n[a.dst[k]] = c[k];
        const uint64_t t = D.rank(n);
        if (R[t] + cost == d) { out.insert(out.end(), a.mv.begin(), a.mv.end()); memcpy(c, n, 24); s = t; step = true; break; }
      }
      if (!step) return false;
    }
    return false;
  }
  bool atStart(const int8_t *col) const {
    for (int k = 0; k < 24; k++) if ((S >> (k / 4) & 1) && col[k] != k / 4) return false;
    return true;
  }
  bool atGoal(const int8_t *col) const {
    for (int k = 0; k < 24; k++) if (((S | T) >> (k / 4) & 1) && col[k] != k / 4) return false;
    return true;
  }
  std::vector<cc::Mv> lineAt(uint32_t li) const {
    std::vector<cc::Mv> r;
    for (uint64_t i = off[li]; i < off[li + 1]; i++) r.push_back(byteMv(bytes[i]));
    return r;
  }
  bool load(const std::string &dir, const Orb24 &o, int threads) {
    O = &o;
    name = baseName(dir);
    const size_t dd = name.find("..");
    if (dd == std::string::npos) { fprintf(stderr, "stage directory must be named <solved>..<target>: %s\n", name.c_str()); return false; }
    S = groupMask(name.substr(0, dd)), T = groupMask(name.substr(dd + 2));
    if (S < 0 || T <= 0 || (S & T)) { fprintf(stderr, "bad stage name %s\n", name.c_str()); return false; }
    for (int g = 0; g < 6; g++) if (T >> g & 1) tg.push_back(g);
    for (int k = 0; k < 24; k++) if (!(S >> (k / 4) & 1)) freeK.push_back(k);
    nf = freeK.size(), base = tg.size() + 1;
    for (size_t s = 0; s < o.syms.size(); s++) {
      int s2 = 0, t2 = 0;
      for (int g = 0; g < 6; g++) { if (S >> g & 1) s2 |= 1 << o.syms[s].gmap[g]; if (T >> g & 1) t2 |= 1 << o.syms[s].gmap[g]; }
      if (s2 == S && t2 == T) ss.push_back(s);
    }
    std::vector<std::pair<std::string, int>> files;
    if (!listSegs(dir, files)) { fprintf(stderr, "no segments in %s\n", dir.c_str()); return false; }
    int8_t solvedCol[24];
    for (int k = 0; k < 24; k++) solvedCol[k] = ((S | T) >> (k / 4) & 1) ? k / 4 : -1;
    off.push_back(0);
    long bad = 0, dup = 0;
    for (auto &[fn, w] : files) {
      std::string data;
      if (!readAll(fn, data)) { fprintf(stderr, "cannot read %s\n", fn.c_str()); return false; }
      const auto st = lineStarts(data);
      std::vector<uint64_t> key(st.size());
      std::vector<std::vector<uint8_t>> mvb(st.size());
      std::vector<char> ok(st.size(), 0);
      std::atomic<size_t> ni{0};
      auto work = [&] {
        std::vector<cc::Mv> mv;
        for (size_t li; (li = ni++) < st.size();) {
          if (!cc::parseMoves(lineAtPos(data, st[li]), mv)) continue;
          bool good = true;
          for (auto &m : mv) good &= m.depth >= 1 && m.depth <= o.nd;
          if (!good) continue;
          int8_t a[24], b[24];
          memcpy(a, solvedCol, 24);
          for (auto it = mv.rbegin(); it != mv.rend(); ++it) {
            for (int k = 0; k < 24; k++) b[o.mp[it->face][it->depth][4 - it->amt][k]] = a[k];
            memcpy(a, b, 24);
          }
          key[li] = keyOf(a);
          for (auto &m : mv) mvb[li].push_back(mvByte(m));
          ok[li] = 1;
        }
      };
      std::vector<std::thread> th;
      for (int t = 0; t < threads; t++) th.emplace_back(work);
      for (auto &t : th) t.join();
      for (size_t li = 0; li < st.size(); li++) {
        if (!ok[li]) { bad++; continue; }
        if (!have.emplace(key[li], (uint32_t)(off.size() - 1)).second) { dup++; continue; }
        bytes.insert(bytes.end(), mvb[li].begin(), mvb[li].end());
        off.push_back(bytes.size());
        longest = std::max(longest, (int)mvb[li].size());
      }
    }
    if (bad || dup) { fprintf(stderr, "%s: %ld bad lines, %ld repeated classes\n", name.c_str(), bad, dup); return false; }
    // self-test: symmetric images of random lines' positions are solved by the lookup
    long fail = 0;
    for (int i = 0; i < 2000; i++) {
      const uint32_t li = rng() % (off.size() - 1);
      int8_t a[24], b[24], c[24];
      for (int k = 0; k < 24; k++) a[k] = ((S | T) >> (k / 4) & 1) ? k / 4 : -1;
      auto L = lineAt(li);
      for (auto it = L.rbegin(); it != L.rend(); ++it) {
        for (int k = 0; k < 24; k++) b[o.mp[it->face][it->depth][4 - it->amt][k]] = a[k];
        memcpy(a, b, 24);
      }
      const auto &y = o.syms[ss[rng() % ss.size()]];
      for (int k = 0; k < 24; k++) c[y.fac[k]] = a[k] < 0 ? -1 : y.gmap[a[k]];
      std::vector<cc::Mv> P;
      if (!solve(c, P)) { fail++; continue; }
      o.apply(P, c);
      if (!atGoal(c) || (int)P.size() > longest) fail++;
    }
    if (fail) { fprintf(stderr, "%s: lookup self-test failed %ld of 2000\n", name.c_str(), fail); return false; }
    return true;
  }
  // a line (certificate depths) that takes col (colours 0..5 per certificate position) to the goal
  bool solve(const int8_t *col, std::vector<cc::Mv> &out) const {
    int8_t c[24], img[24];
    for (int k = 0; k < 24; k++) c[k] = (col[k] >= 0 && ((S | T) >> col[k] & 1)) ? col[k] : -1;
    for (int s : ss) {
      const auto &y = O->syms[s];
      for (int k = 0; k < 24; k++) img[y.fac[k]] = c[k] < 0 ? -1 : y.gmap[c[k]];
      auto it = have.find(keyOf(img));
      if (it != have.end()) { out = symMoves24(O->syms[O->inv[s]], lineAt(it->second)); return true; }
    }
    return false;
  }
};

// ---- wing orbits: 24 distinct pieces, named by certcube.h's wingFacelets order on 6x6x6 (depth 2)
static int mw[6][3][4][24];  // dst form, certificate depths 1..2
// The concise wing certificate is a list of algorithms (actions.txt).  Each stage uses the ones that
// keep its placed wings home; a Dijkstra search from the goal over the stage positions, each algorithm
// costing its move count, gives every position its distance and first algorithm.  A line is the
// algorithms along the path, concatenated.
struct WingAct { std::vector<cc::Mv> mv; int dst[24], src[24]; };  // the wing at position s goes to dst[s]
static std::vector<WingAct> wingActs;
static inline bool loadWingActs(const std::string &fn) {
  std::string data;
  if (!readAll(fn, data)) { fprintf(stderr, "cannot read %s\n", fn.c_str()); return false; }
  for (size_t s0 : lineStarts(data)) {
    WingAct a;
    if (!cc::parseMoves(lineAtPos(data, s0), a.mv) || a.mv.empty()) { fprintf(stderr, "bad wing algorithm line\n"); return false; }
    for (auto &m : a.mv) if (m.depth != 1 && m.depth != 2) { fprintf(stderr, "wing algorithm with a move deeper than 2\n"); return false; }
    for (int k = 0; k < 24; k++) a.dst[k] = k;
    for (auto &m : a.mv)
      for (int k = 0; k < 24; k++) a.dst[k] = mw[m.face][m.depth][m.amt][a.dst[k]];
    for (int k = 0; k < 24; k++) a.src[a.dst[k]] = k;
    wingActs.push_back(a);
  }
  return true;
}
struct WingStage {
  std::string name;
  int S = 0, T = 0, n = 0, K = 0, longest = 0;
  uint64_t total = 0;
  std::vector<int> tg, freeP, use;
  int freeIdx[24];
  std::vector<uint32_t> best;  // per position rank: distance << 16 | first algorithm
  // tpos[i] = where target wing tg[i] is
  bool rankT(const int *tpos, uint64_t &r) const {
    r = 0;
    uint32_t used = 0;
    for (int i = 0; i < K; i++) {
      const int x = freeIdx[tpos[i]];
      if (x < 0) return false;
      r = r * (n - i) + (x - __builtin_popcount(used & ((1u << x) - 1)));
      used |= 1u << x;
    }
    return true;
  }
  void unrankT(uint64_t r, int *tpos) const {
    int d[24];
    for (int i = K - 1; i >= 0; i--) d[i] = r % (n - i), r /= (n - i);
    uint32_t used = 0;
    for (int i = 0; i < K; i++) {
      int c = d[i], x = 0;
      for (;; x++) if (!(used >> x & 1) && c-- == 0) break;
      used |= 1u << x;
      tpos[i] = freeP[x];
    }
  }
  // pinv[t] = where wing t is
  bool rankOf(const int *pinv, uint64_t &r) const {
    for (int k = 0; k < 24; k++) if ((S >> k & 1) && pinv[k] != k) return false;
    int tpos[24];
    for (int i = 0; i < K; i++) tpos[i] = pinv[tg[i]];
    return rankT(tpos, r);
  }
  bool atGoal(const int *pinv) const {
    for (int k = 0; k < 24; k++) if (((S | T) >> k & 1) && pinv[k] != k) return false;
    return true;
  }
  std::vector<cc::Mv> line(uint64_t r) const {
    std::vector<cc::Mv> v;
    int tpos[24];
    unrankT(r, tpos);
    while (best[r] >> 16) {
      const auto &a = wingActs[best[r] & 0xFFFF];
      v.insert(v.end(), a.mv.begin(), a.mv.end());
      for (int i = 0; i < K; i++) tpos[i] = a.dst[tpos[i]];
      rankT(tpos, r);
    }
    return v;
  }
  bool build(const std::string &stageName, int threads) {
    name = stageName;
    const size_t dd = name.find("..");
    S = wingMask(name.substr(0, dd)), T = wingMask(name.substr(dd + 2));
    if (S < 0 || T <= 0 || (S & T)) { fprintf(stderr, "bad stage name %s\n", name.c_str()); return false; }
    for (int k = 0; k < 24; k++) { freeIdx[k] = -1; if (T >> k & 1) tg.push_back(k); if (!(S >> k & 1)) freeP.push_back(k); }
    n = freeP.size(), K = tg.size();
    for (int i = 0; i < n; i++) freeIdx[freeP[i]] = i;
    total = 1;
    for (int i = 0; i < K; i++) total *= n - i;
    for (size_t a = 0; a < wingActs.size(); a++) {
      bool keeps = true;
      for (int k = 0; k < 24; k++) if ((S >> k & 1) && wingActs[a].dst[k] != k) keeps = false;
      if (keeps) use.push_back(a);
    }
    best.assign(total, ~0u);
    uint64_t fp = fnv(name.data(), name.size());
    for (int a : use) { fp = fnv(wingActs[a].dst, sizeof wingActs[a].dst, fp); for (auto &m : wingActs[a].mv) { const uint8_t b = mvByte(m); fp = fnv(&b, 1, fp); } fp = fnv("|", 1, fp); }
    const std::string cpath = cachePath("wing-" + name, fp);
    if (readCache(cpath, {{best.data(), total * 4}})) {
      for (auto v : best) if (v != ~0u) longest = std::max(longest, (int)(v >> 16));
      return true;
    }
    int home[24];
    for (int i = 0; i < K; i++) home[i] = tg[i];
    uint64_t r0;
    rankT(home, r0);
    best[r0] = 0xFFFF;
    std::atomic<int> maxD{0};
    for (int D = 0; D <= maxD.load(); D++) {
      std::atomic<uint64_t> next{0};
      auto work = [&] {
        int tpos[24], q[24];
        for (uint64_t lo; (lo = next.fetch_add(1 << 14)) < total;)
          for (uint64_t r = lo; r < std::min(total, lo + (1 << 14)); r++) {
            if ((__atomic_load_n(&best[r], __ATOMIC_RELAXED) >> 16) != (uint32_t)D) continue;
            unrankT(r, tpos);
            for (int a : use) {
              const auto &A = wingActs[a];
              for (int i = 0; i < K; i++) q[i] = A.src[tpos[i]];
              uint64_t rq;
              rankT(q, rq);
              const uint32_t nd = D + A.mv.size(), nv = nd << 16 | a;
              uint32_t old = __atomic_load_n(&best[rq], __ATOMIC_RELAXED);
              while ((old >> 16) > nd && !__atomic_compare_exchange_n(&best[rq], &old, nv, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {}
              int m = maxD.load();
              while ((int)nd > m && !maxD.compare_exchange_weak(m, nd)) {}
            }
          }
      };
      std::vector<std::thread> th;
      for (int t = 0; t < threads; t++) th.emplace_back(work);
      for (auto &t : th) t.join();
    }
    uint64_t missing = 0;
    for (auto v : best) { if (v == ~0u) missing++; else longest = std::max(longest, (int)(v >> 16)); }
    if (missing) { fprintf(stderr, "%s: %llu positions not reached\n", name.c_str(), (unsigned long long)missing); return false; }
    writeCache(cpath, {{best.data(), total * 4}});
    return true;
  }
};

// ---- equivariant map from certificate positions to xcube slots, from the quarter-turn maps of both
// (the xcube side measured on a reference cube with distinct labels).  faceKeep: a position and its
// slot lie on one face (colour orbits).
template <class MP>
static inline bool layoutMap(const MP &mpc, int nd, int refMx, int refMy, const int *xdep, const std::function<void(const xcube &, uint8_t *)> &read, bool faceKeep, int *phi) {
  static int B[6][4][24];
  for (int f = 0; f < 6; f++)
    for (int d = 1; d <= nd; d++) {
      xcube c(refMx, refMy, 'U');
      domove({xdep[d], (short)f, (short)1}, c, true);
      uint8_t X[24];
      read(c, X);
      for (int k = 0; k < 24; k++) B[f][d][X[k]] = k;
    }
  for (int cand = 0; cand < 24; cand++) {
    if (faceKeep && cand / 4 != 0) continue;
    int m[24];
    for (auto &x : m) x = -1;
    m[0] = cand;
    std::vector<int> q = {0};
    bool bad = false;
    while (!q.empty() && !bad) {
      const int p = q.back();
      q.pop_back();
      for (int f = 0; f < 6 && !bad; f++)
        for (int d = 1; d <= nd && !bad; d++) {
          const int p2 = mpc[f][d][1][p], b2 = B[f][d][m[p]];
          if (m[p2] < 0) { m[p2] = b2; q.push_back(p2); }
          else if (m[p2] != b2) bad = true;
        }
    }
    int used = 0;
    for (int k = 0; k < 24 && !bad; k++) {
      if (m[k] < 0 || (used >> m[k] & 1) || (faceKeep && m[k] / 4 != k / 4)) bad = true;
      else used |= 1 << m[k];
    }
    if (bad) continue;
    memcpy(phi, m, sizeof m);
    return true;
  }
  return false;
}

// ---- corners (and midges): cubie model on brobdicube's threecube facelets
namespace t3 {
static int C[8][3], E[12][2];  // slot facelets (U/D facelet first; corner triples of one handedness)
static int ccol[8][3], ecol[12][2];
struct Cubie { int8_t cp[8], co[8], ep[12], eo[12]; };
static Cubie mvC[18];  // move f * 3 + amount - 1
static inline Cubie ident() { Cubie c; for (int i = 0; i < 8; i++) c.cp[i] = i, c.co[i] = 0; for (int i = 0; i < 12; i++) c.ep[i] = i, c.eo[i] = 0; return c; }
static inline void mul(const Cubie &a, const Cubie &b, Cubie &r) {  // a, then b
  for (int i = 0; i < 8; i++) r.cp[i] = a.cp[b.cp[i]], r.co[i] = (a.co[b.cp[i]] + b.co[i]) % 3;
  for (int i = 0; i < 12; i++) r.ep[i] = a.ep[b.ep[i]], r.eo[i] = (a.eo[b.ep[i]] + b.eo[i]) & 1;
}
static inline bool fromTC(const threecube &t, bool edges, Cubie &s) {
  s = ident();
  int seen = 0;
  for (int i = 0; i < 8; i++) {
    int x[3], m0 = -1;
    for (int m = 0; m < 3; m++) { x[m] = t.corner.a[C[i][m]]; if (x[m] == 0 || x[m] == 5) { if (m0 >= 0) return false; m0 = m; } }
    if (m0 < 0) return false;
    int j = -1;
    for (int c = 0; c < 8 && j < 0; c++) {
      bool ok = true;
      for (int m = 0; m < 3; m++) ok &= x[(m0 + m) % 3] == ccol[c][m];
      if (ok) j = c;
    }
    if (j < 0 || (seen >> j & 1)) return false;
    seen |= 1 << j;
    s.cp[i] = j, s.co[i] = m0;
  }
  if (!edges) return true;
  seen = 0;
  for (int i = 0; i < 12; i++) {
    const int x0 = t.edge.a[E[i][0]], x1 = t.edge.a[E[i][1]];
    int j = -1, o = 0;
    for (int c = 0; c < 12 && j < 0; c++) {
      if (x0 == ecol[c][0] && x1 == ecol[c][1]) j = c, o = 0;
      else if (x0 == ecol[c][1] && x1 == ecol[c][0]) j = c, o = 1;
    }
    if (j < 0 || (seen >> j & 1)) return false;
    seen |= 1 << j;
    s.ep[i] = j, s.eo[i] = o;
  }
  return true;
}
static inline bool same(const Cubie &a, const Cubie &b, bool edges) {
  if (memcmp(a.cp, b.cp, 8) || memcmp(a.co, b.co, 8)) return false;
  return !edges || (!memcmp(a.ep, b.ep, 12) && !memcmp(a.eo, b.eo, 12));
}
static inline bool init() {
  for (int i = 0; i < 8; i++) for (int m = 0; m < 3; m++) C[i][m] = threecube::cor[i][m];
  for (int i = 0; i < 12; i++) for (int m = 0; m < 2; m++) E[i][m] = threecube::edg[i][m];
  // relative handedness of the corner slot triples, from where single quarter turns put the stickers
  int h[8];
  for (auto &x : h) x = 0;
  h[0] = 1;
  for (int pass = 0; pass < 8; pass++)
    for (int f = 0; f < 6; f++) {
      threecube t('C', 1);
      t.domove({1, (short)f, 1});
      for (int i = 0; i < 8; i++) {
        int x[3];
        for (int m = 0; m < 3; m++) x[m] = t.corner.a[C[i][m]];
        for (int j = 0; j < 8; j++) {
          int pi[3], hit = 0;
          for (int m = 0; m < 3; m++)
            for (int q = 0; q < 3; q++) if (x[m] == C[j][q] / 4) pi[m] = q, hit |= 1 << q;
          if (hit != 7) continue;
          const int inv = (pi[0] > pi[1]) + (pi[0] > pi[2]) + (pi[1] > pi[2]);
          const int sg = inv & 1 ? -1 : 1;
          if (h[j] && !h[i]) h[i] = h[j] * sg;
          else if (h[i] && !h[j]) h[j] = h[i] * sg;
          else if (h[i] && h[j] && h[i] * h[j] != sg) { fprintf(stderr, "corner handedness inconsistent\n"); return false; }
        }
      }
    }
  for (int i = 0; i < 8; i++) {
    if (!h[i]) { fprintf(stderr, "corner handedness undetermined\n"); return false; }
    if (h[i] < 0) std::swap(C[i][1], C[i][2]);
    for (int m = 0; m < 3; m++) ccol[i][m] = C[i][m] / 4;
    if (ccol[i][0] != 0 && ccol[i][0] != 5) { fprintf(stderr, "corner slot without a U/D facelet first\n"); return false; }
  }
  for (int i = 0; i < 12; i++) for (int m = 0; m < 2; m++) ecol[i][m] = E[i][m] / 4;
  for (int f = 0; f < 6; f++) {
    threecube t('C', 1);
    for (int a = 1; a <= 3; a++) {
      t.domove({1, (short)f, 1});
      if (!fromTC(t, true, mvC[f * 3 + a - 1])) { fprintf(stderr, "cannot read a face turn as cubies\n"); return false; }
    }
  }
  // self-test: random face turns on the facelets and on the cubie model agree
  for (int rep = 0; rep < 50; rep++) {
    threecube t('C', 1);
    Cubie s = ident(), r;
    for (int i = 0; i < 60; i++) {
      const int f = rng() % 6, a = 1 + rng() % 3;
      for (int q = 0; q < a; q++) t.domove({1, (short)f, 1});
      mul(s, mvC[f * 3 + a - 1], r);
      s = r;
      Cubie u;
      if (!fromTC(t, true, u) || !same(u, s, true)) { fprintf(stderr, "cubie model disagrees with the facelets\n"); return false; }
    }
  }
  return true;
}
static inline int permRank(const int8_t *p, int n) {
  int r = 0;
  for (int i = 0; i < n; i++) {
    int c = 0;
    for (int j = i + 1; j < n; j++) c += p[j] < p[i];
    r = r * (n - i) + c;
  }
  return r;
}
static inline void permUnrank(int r, int n, int8_t *p) {
  int d[12];
  for (int i = n - 1; i >= 0; i--) d[i] = r % (n - i), r /= (n - i);
  bool used[12] = {};
  for (int i = 0; i < n; i++) {
    int c = d[i];
    for (int v = 0; v < n; v++) if (!used[v] && c-- == 0) { p[i] = v, used[v] = true; break; }
  }
}
static inline int twistOf(const Cubie &c) { int t = 0; for (int i = 6; i >= 0; i--) t = t * 3 + c.co[i]; return t; }
static inline void setTwist(Cubie &c, int t) { int s = 0; for (int i = 0; i < 7; i++) c.co[i] = t % 3, s += t % 3, t /= 3; c.co[7] = (3 - s % 3) % 3; }
static inline int flipOf(const Cubie &c) { int t = 0; for (int i = 10; i >= 0; i--) t = t * 2 + c.eo[i]; return t; }
static inline void setFlip(Cubie &c, int t) { int s = 0; for (int i = 0; i < 11; i++) c.eo[i] = t & 1, s += t & 1, t >>= 1; c.eo[11] = s & 1; }
static inline bool isSlice(int e) { return e >= 4 && e <= 7; }  // F-L, F-R, B-R, B-L in threecube::edg
static int binom12[13][13];
static inline int sliceOf(const Cubie &c) {  // colex rank of the positions holding slice edges
  int r = 0, j = 0;
  for (int p = 0; p < 12; p++) if (isSlice(c.ep[p])) r += binom12[p][++j];
  return r;
}
static inline void setSlice(Cubie &c, int r) {
  int pos[4], j = 4;
  for (int p = 11; p >= 0 && j > 0; p--) if (r >= binom12[p][j]) r -= binom12[p][j], pos[--j] = p;
  int s = 4, o = 0;
  const int others[8] = {0, 1, 2, 3, 8, 9, 10, 11};
  bool isPos[12] = {};
  for (int i = 0; i < 4; i++) isPos[pos[i]] = true;
  for (int p = 0; p < 12; p++) c.ep[p] = isPos[p] ? s++ : others[o++];
}
static const int UD8[8] = {0, 1, 2, 3, 8, 9, 10, 11};
static inline int ud8Of(const Cubie &c) {
  int8_t q[8];
  for (int i = 0; i < 8; i++) { const int e = c.ep[UD8[i]]; q[i] = e < 4 ? e : e - 4; }
  return permRank(q, 8);
}
static inline void setUd8(Cubie &c, int r) {
  int8_t q[8];
  permUnrank(r, 8, q);
  for (int i = 0; i < 8; i++) c.ep[UD8[i]] = UD8[q[i]];
  for (int p = 4; p < 8; p++) c.ep[p] = p;
}
static inline int sliceperm(const Cubie &c) { int8_t q[4]; for (int i = 0; i < 4; i++) q[i] = c.ep[4 + i] - 4; return permRank(q, 4); }
static inline void setSliceperm(Cubie &c, int r) { int8_t q[4]; permUnrank(r, 4, q); for (int i = 0; i < 4; i++) c.ep[4 + i] = 4 + q[i]; }
static const int P2[10] = {0, 1, 2, 15, 16, 17, 4, 7, 10, 13};  // U U2 U' D D2 D' L2 F2 R2 B2
static const int opp[6] = {5, 3, 4, 1, 2, 0};
static std::vector<uint16_t> twMv, flMv, slMv, cpMv, udMv, spMv;  // [coord * 18 + m] (ud, sp: [coord * 10 + j])
static std::vector<uint8_t> pTS, pFS, pCS, pUS;
template <class F> static void bfs(std::vector<uint8_t> &t, size_t n, int start, int nm, F next) {
  t.assign(n, 255);
  t[start] = 0;
  for (int d = 0;; d++) {
    long c = 0;
    for (size_t i = 0; i < n; i++)
      if (t[i] == d)
        for (int m = 0; m < nm; m++) { const size_t j = next(i, m); if (t[j] == 255) t[j] = d + 1, c++; }
    if (!c) break;
  }
}
static bool tablesBuilt = false;
static inline void buildTwoPhase() {
  if (tablesBuilt) return;
  tablesBuilt = true;
  for (int a = 0; a <= 12; a++) for (int b = 0; b <= 12; b++) binom12[a][b] = b == 0 ? 1 : a == 0 ? 0 : binom12[a - 1][b - 1] + (b <= a - 1 ? binom12[a - 1][b] : 0);
  twMv.resize(2187 * 18), flMv.resize(2048 * 18), slMv.resize(495 * 18), cpMv.resize(40320 * 18), udMv.resize(40320 * 10), spMv.resize(24 * 10);
  Cubie c, r;
  for (int x = 0; x < 2187; x++) { c = ident(); setTwist(c, x); for (int m = 0; m < 18; m++) { mul(c, mvC[m], r); twMv[x * 18 + m] = twistOf(r); } }
  for (int x = 0; x < 2048; x++) { c = ident(); setFlip(c, x); for (int m = 0; m < 18; m++) { mul(c, mvC[m], r); flMv[x * 18 + m] = flipOf(r); } }
  for (int x = 0; x < 495; x++) { c = ident(); setSlice(c, x); for (int m = 0; m < 18; m++) { mul(c, mvC[m], r); slMv[x * 18 + m] = sliceOf(r); } }
  for (int x = 0; x < 40320; x++) { c = ident(); permUnrank(x, 8, c.cp); for (int m = 0; m < 18; m++) { mul(c, mvC[m], r); cpMv[x * 18 + m] = permRank(r.cp, 8); } }
  for (int x = 0; x < 40320; x++) { c = ident(); setUd8(c, x); for (int j = 0; j < 10; j++) { mul(c, mvC[P2[j]], r); udMv[x * 10 + j] = ud8Of(r); } }
  for (int x = 0; x < 24; x++) { c = ident(); setSliceperm(c, x); for (int j = 0; j < 10; j++) { mul(c, mvC[P2[j]], r); spMv[x * 10 + j] = sliceperm(r); } }
  const int sl0 = sliceOf(ident());
  bfs(pTS, 2187 * 495, sl0, 18, [](size_t i, int m) { return (size_t)twMv[(i / 495) * 18 + m] * 495 + slMv[(i % 495) * 18 + m]; });
  bfs(pFS, 2048 * 495, sl0, 18, [](size_t i, int m) { return (size_t)flMv[(i / 495) * 18 + m] * 495 + slMv[(i % 495) * 18 + m]; });
  bfs(pCS, 40320 * 24, 0, 10, [](size_t i, int j) { return (size_t)cpMv[(i / 24) * 18 + P2[j]] * 24 + spMv[(i % 24) * 10 + j]; });
  bfs(pUS, 40320 * 24, 0, 10, [](size_t i, int j) { return (size_t)udMv[(i / 24) * 10 + j] * 24 + spMv[(i % 24) * 10 + j]; });
}
// two-phase search: corners and midges (face turns), at most maxLen moves
struct TwoPhase {
  Cubie start;
  int path[40], sl0 = 0, maxLen = 20;
  bool phase2(int cp, int ud, int sp, int depth, int togo, int prev) {
    if (togo == 0) return cp == 0 && ud == 0 && sp == 0;
    if (std::max(pCS[(size_t)cp * 24 + sp], pUS[(size_t)ud * 24 + sp]) > togo) return false;
    for (int j = 0; j < 10; j++) {
      const int m = P2[j], f = m / 3;
      if (prev >= 0 && (f == prev || (f == opp[prev] && f < prev))) continue;
      path[depth] = m;
      if (phase2(cpMv[cp * 18 + m], udMv[ud * 10 + j], spMv[sp * 10 + j], depth + 1, togo - 1, f)) return true;
    }
    return false;
  }
  bool phase1(int tw, int fl, int sl, int depth, int togo, int prev) {
    if (togo == 0) {
      if (tw || fl || sl != sl0) return false;
      if (depth > 0) { const int m = path[depth - 1], f = m / 3; if (f == 0 || f == 5 || m % 3 == 1) return false; }
      Cubie c = start, r;
      for (int i = 0; i < depth; i++) { mul(c, mvC[path[i]], r); c = r; }
      const int cp = permRank(c.cp, 8), ud = ud8Of(c), sp = sliceperm(c);
      for (int l2 = 0; depth + l2 <= maxLen; l2++)
        if (phase2(cp, ud, sp, depth, l2, depth ? path[depth - 1] / 3 : -1)) { total = depth + l2; return true; }
      return false;
    }
    if (std::max(pTS[(size_t)tw * 495 + sl], pFS[(size_t)fl * 495 + sl]) > togo) return false;
    for (int m = 0; m < 18; m++) {
      const int f = m / 3;
      if (prev >= 0 && (f == prev || (f == opp[prev] && f < prev))) continue;
      path[depth] = m;
      if (phase1(twMv[tw * 18 + m], flMv[fl * 18 + m], slMv[sl * 18 + m], depth + 1, togo - 1, f)) return true;
    }
    return false;
  }
  int total = -1;
  bool solve(const Cubie &s, std::vector<cc::Mv> &out) {
    start = s;
    sl0 = sliceOf(ident());
    const int tw = twistOf(s), fl = flipOf(s), sl = sliceOf(s);
    for (int l1 = 0; l1 <= maxLen; l1++)
      if (phase1(tw, fl, sl, 0, l1, -1)) {
        out.clear();
        for (int i = 0; i < total; i++) out.push_back({path[i] / 3, 1, path[i] % 3 + 1});
        return true;
      }
    return false;
  }
};
// corners alone (even N): distance to solved for every corner state, by breadth-first search
static std::vector<uint8_t> cdist;
static int cornerMax = 0;
static inline void buildCorners(int threads) {
  if (!cdist.empty()) return;
  if (cpMv.empty()) {
    twMv.resize(2187 * 18), cpMv.resize(40320 * 18);
    Cubie c, r;
    for (int x = 0; x < 2187; x++) { c = ident(); setTwist(c, x); for (int m = 0; m < 18; m++) { mul(c, mvC[m], r); twMv[x * 18 + m] = twistOf(r); } }
    for (int x = 0; x < 40320; x++) { c = ident(); permUnrank(x, 8, c.cp); for (int m = 0; m < 18; m++) { mul(c, mvC[m], r); cpMv[x * 18 + m] = permRank(r.cp, 8); } }
  }
  const size_t n = 40320ull * 2187;
  cdist.assign(n, 255);
  cdist[0] = 0;
  for (int d = 0;; d++) {
    std::atomic<long> cnt{0};
    std::vector<std::thread> th;
    for (int t = 0; t < threads; t++)
      th.emplace_back([&, t] {
        long c = 0;
        for (size_t i = n * t / threads; i < n * (t + 1) / threads; i++)
          if (cdist[i] == d)
            for (int m = 0; m < 18; m++) {
              const size_t j = (size_t)cpMv[(i / 2187) * 18 + m] * 2187 + twMv[(i % 2187) * 18 + m];
              if (cdist[j] == 255) { __atomic_store_n(&cdist[j], (uint8_t)(d + 1), __ATOMIC_RELAXED); c++; }
            }
        cnt += c;
      });
    for (auto &x : th) x.join();
    if (!cnt) { cornerMax = d; break; }
  }
}
static inline bool solveCorners(const Cubie &s, std::vector<cc::Mv> &out) {
  size_t i = (size_t)permRank(s.cp, 8) * 2187 + twistOf(s);
  if (cdist[i] == 255) return false;
  out.clear();
  while (cdist[i]) {
    bool step = false;
    for (int m = 0; m < 18 && !step; m++) {
      const size_t j = (size_t)cpMv[(i / 2187) * 18 + m] * 2187 + twMv[(i % 2187) * 18 + m];
      if (cdist[j] + 1 == cdist[i]) { out.push_back({m / 3, 1, m % 3 + 1}); i = j; step = true; }
    }
    if (!step) return false;
  }
  return true;
}
}  // namespace t3
