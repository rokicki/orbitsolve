// boundx: explore a stage boundary of the chiral-pair certificate (feasibility only, no output
// certificate).  Stages A = <S>..<T> and B = <S+T>..<U> are loaded as dense tables.  For every
// A class whose line is longer than X - worst(B), the colourings r of the rest that its line L0
// carries onto B states beyond X - |L0| are listed; then alternative solving sequences of the A
// state (an action a, then the certificate line of where a leads, and the stage-symmetric copies
// of L0 that fix the state) are tried on each such r, cheapest first.  A + B <= X holds for that
// class when every r has some sequence P with |P| + dB(carried r) <= X.
//
//   boundx STAGEDIR_A STAGEDIR_B X [--prop C] [--threads T] [--check N] [--deep S] [--slack L] [--out FILE] [--sample K] [--only FILE] [--base POOL]...
// --sample K: check only every K-th of the A classes (for estimating time and coverage).
// --only FILE: check only the A classes whose certificate line is one of FILE's lines.
// --deep S: for a class some colourings of which are left, also try two actions, then the line
// (a1 a2 + line), where a1 alone comes within S moves of the cost allowed.
// --slack L: in that deeper pass, sequences up to L moves longer than the class's line (default 2).
// --out FILE: the boundary certificate: per A class checked, a block of its line L0 and the
// alternatives that cover something, one sequence per line, blocks separated by an empty line.
// --check N: for about N of the classes, up to 20 covered colourings each are re-solved by playing
// the moves on the full colouring (A's and B's pieces together), independently of the carries.
// Actions (those keeping A's solved faces): the 18 outer face turns and the images (48 symmetries, inverse) of the base pools'
// algorithms of at most C moves (default 8).
#include "certcube.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static int groupMask(const std::string &s) {
  if (s == "none") return 0;
  int m = 0;
  if (s.size() % 2) return -1;
  for (size_t i = 0; i < s.size(); i += 2) {
    const int o = s[i] == 'a' ? 0 : s[i] == 'b' ? 1 : -1;
    const char *p = strchr(cc::FACES, s[i + 1]);
    if (o < 0 || !p || !*p) return -1;
    m |= 1 << (o * 6 + (p - cc::FACES));
  }
  return m;
}
static inline uint8_t mvByte(const cc::Mv &m) { return m.face << 4 | m.depth << 2 | m.amt; }
static inline cc::Mv byteMv(uint8_t b) { return {b >> 4, b >> 2 & 3, b & 3}; }
static void simplify(std::vector<cc::Mv> &v) {
  std::vector<cc::Mv> r;
  for (auto &m : v) {
    if (!r.empty() && r.back().face == m.face && r.back().depth == m.depth) {
      const int a = (r.back().amt + m.amt) % 4;
      if (a) r.back().amt = a;
      else r.pop_back();
    } else r.push_back(m);
  }
  v.swap(r);
}
static bool poolMoves(const char *buf, std::vector<cc::Mv> &mv) {
  size_t n = strlen(buf);
  if (!n || buf[n - 1] != '\n' || buf[0] == '#') return false;
  std::string l(buf);
  while (!l.empty() && (l.back() == '\n' || l.back() == ' ' || l.back() == '\r')) l.pop_back();
  if (l.size() >= 3 && l[l.size() - 3] == ' ' && l[l.size() - 2] == 'D') l.resize(l.size() - 3);
  size_t st = 0;
  while (st < l.size() && l[st] == ' ') st++;
  size_t e = st;
  while (e < l.size() && l[e] >= '0' && l[e] <= '9') e++;
  if (e > st && (e == l.size() || l[e] == ' ')) st = e;
  if (!cc::parseMoves(l.substr(st), mv) || mv.empty()) return false;
  for (auto &m : mv) if (m.depth < 1 || m.depth > 3) return false;
  return true;
}

// geometry shared by the stages
static int mp[6][4][4][48];
static int groupOf(int k) { return (k % 8) / 4 * 6 + k / 8; }
struct GSym { int fac[48]; int gmap[12]; int face[6]; bool flip; };
static std::vector<GSym> gsyms;
static uint64_t binom[49][49];
static uint8_t CX[256][256];
static uint32_t BT[3][256][25];
static std::vector<cc::Mv> symMoves(const GSym &y, const std::vector<cc::Mv> &mv) {
  std::vector<cc::Mv> r;
  for (auto &m : mv) r.push_back({y.face[m.face], m.depth, y.flip ? 4 - m.amt : m.amt});
  return r;
}
// facelet map of a move sequence: the piece at k ends at D[k]
static void mapOf(const std::vector<cc::Mv> &mv, int8_t *D) {
  for (int k = 0; k < 48; k++) {
    int x = k;
    for (auto &m : mv) x = mp[m.face][m.depth][m.amt][x];
    D[k] = x;
  }
}

typedef uint32_t Masks[2][13];
struct Stage {
  std::string name;
  int S = 0, TG = 0, NC = 0;
  std::vector<int> tg, rel[2];
  int8_t cidx[12], solvedCi[48];
  int relIdx[48];
  struct OrbitRank { int n; int colours[13]; int nc; int cnt[13]; } orr[2];
  uint64_t Mo[2] = {1, 1}, total = 0;
  std::vector<std::array<int8_t, 13>> cmap;  // per symmetry
  std::vector<int> ss;
  std::vector<uint8_t> dist, srcSym;
  std::vector<uint32_t> srcLine;
  std::vector<uint64_t> lineOff;
  std::vector<uint8_t> lineBytes;
  int W = 0;

  bool init(const std::string &dir) {
    std::string d = dir;
    while (d.size() > 1 && d.back() == '/') d.pop_back();
    name = d.substr(d.rfind('/') == std::string::npos ? 0 : d.rfind('/') + 1);
    const size_t dd = name.find("..");
    if (dd == std::string::npos) { fprintf(stderr, "stage directory must be named <solved>..<target>\n"); return false; }
    S = groupMask(name.substr(0, dd)), TG = groupMask(name.substr(dd + 2));
    if (S < 0 || TG <= 0 || (S & TG)) { fprintf(stderr, "bad stage name %s\n", name.c_str()); return false; }
    for (int g = 0; g < 12; g++) if (TG >> g & 1) tg.push_back(g);
    for (int g = 0; g < 12; g++) cidx[g] = (TG >> g & 1) ? std::find(tg.begin(), tg.end(), g) - tg.begin() + 1 : 0;
    NC = tg.size() + 1;
    int cnt[2][13] = {};
    for (int k = 0; k < 48; k++) {
      const int o = (k % 8) / 4;
      bool on = false;
      for (int g : tg) if (g / 6 == o) on = true;
      if (!on || (S >> groupOf(k) & 1)) continue;
      rel[o].push_back(k);
    }
    for (int k = 0; k < 48; k++) relIdx[k] = -1;
    for (int o = 0; o < 2; o++) for (size_t i = 0; i < rel[o].size(); i++) relIdx[rel[o][i]] = i;
    for (int k = 0; k < 48; k++) solvedCi[k] = cidx[groupOf(k)];
    for (int o = 0; o < 2; o++) {
      for (int k : rel[o]) cnt[o][solvedCi[k]]++;
      uint64_t m = 1;
      int n = 0;
      for (int c = 0; c < NC; c++)
        for (int j = 1; j <= cnt[o][c]; j++) { n++; m = m * n / j; }
      Mo[o] = m;
    }
    total = Mo[0] * Mo[1];
    if (total > (1ULL << 32)) { fprintf(stderr, "stage too large\n"); return false; }
    for (int o = 0; o < 2; o++) {
      orr[o].n = rel[o].size();
      orr[o].nc = 0;
      for (int c = 1; c < NC; c++) if (cnt[o][c]) orr[o].colours[orr[o].nc] = c, orr[o].cnt[orr[o].nc++] = cnt[o][c];
    }
    for (size_t s = 0; s < gsyms.size(); s++) {
      const GSym &y = gsyms[s];
      int s2 = 0, t2 = 0;
      for (int g = 0; g < 12; g++) { if (S >> g & 1) s2 |= 1 << y.gmap[g]; if (TG >> g & 1) t2 |= 1 << y.gmap[g]; }
      std::array<int8_t, 13> cm{};
      for (size_t i = 0; i < tg.size(); i++) cm[i + 1] = cidx[y.gmap[tg[i]]];
      cmap.push_back(cm);
      if (s2 == S && t2 == TG) ss.push_back(s);
    }
    return load(d);
  }
  void masksOf(const int8_t *ci, Masks &mk) const {
    for (int o = 0; o < 2; o++) {
      const OrbitRank &R = orr[o];
      uint32_t m[13] = {};
      for (int i = 0; i < R.n; i++) m[ci[rel[o][i]]] |= 1u << i;
      for (int ii = 0; ii < R.nc; ii++) mk[o][ii] = m[R.colours[ii]];
    }
  }
  uint64_t maskRank(const Masks &mk) const {
    uint64_t r = 0;
    for (int o = 0; o < 2; o++) {
      const OrbitRank &R = orr[o];
      if (!R.n) continue;
      uint32_t fr = (1u << R.n) - 1;
      int nf = R.n;
      uint64_t ro = 0;
      const int nb = (R.n + 7) / 8;
      for (int ii = 0; ii < R.nc; ii++) {
        if (nf == R.cnt[ii]) break;
        const uint32_t x = mk[o][ii];
        uint32_t cx = 0;
        if (ii == 0) cx = x;
        else {
          int sh = 0;
          for (int bp = 0; bp < nb; bp++) {
            const int mb = fr >> (8 * bp) & 255;
            cx |= (uint32_t)CX[mb][x >> (8 * bp) & 255] << sh;
            sh += __builtin_popcount(mb);
          }
        }
        uint64_t rc = 0;
        int j = 0;
        for (int bp = 0; bp < nb; bp++) {
          const int v = cx >> (8 * bp) & 255;
          rc += BT[bp][v][j];
          j += __builtin_popcount(v);
        }
        ro = ro * binom[nf][R.cnt[ii]] + rc;
        fr &= ~x;
        nf -= R.cnt[ii];
      }
      r = r * Mo[o] + ro;
    }
    return r;
  }
  uint64_t rankOf(const int8_t *ci) const { Masks mk; masksOf(ci, mk); return maskRank(mk); }
  void unrank(uint64_t r, int8_t *ci) const {
    memcpy(ci, solvedCi, 48);
    for (int o = 1; o >= 0; o--) {
      const OrbitRank &R = orr[o];
      if (!R.n) continue;
      uint64_t ro = r % Mo[o];
      r /= Mo[o];
      uint64_t rc[13];
      {
        int nf = R.n;
        uint64_t rad[13];
        for (int ii = 0; ii < R.nc; ii++) rad[ii] = binom[nf][R.cnt[ii]], nf -= R.cnt[ii];
        for (int ii = R.nc - 1; ii >= 0; ii--) rc[ii] = ro % rad[ii], ro /= rad[ii];
      }
      for (int i = 0; i < R.n; i++) ci[rel[o][i]] = 0;
      std::vector<int> freeIdx;
      for (int i = 0; i < R.n; i++) freeIdx.push_back(i);
      for (int ii = 0; ii < R.nc; ii++) {
        uint64_t rest = rc[ii];
        std::vector<int> chosen;
        int p = freeIdx.size();
        for (int t = R.cnt[ii]; t >= 1; t--) {
          p--;
          while (binom[p][t] > rest) p--;
          rest -= binom[p][t];
          chosen.push_back(p);
        }
        std::vector<int> nfree;
        std::vector<char> take(freeIdx.size(), 0);
        for (int q : chosen) take[q] = 1;
        for (size_t q = 0; q < freeIdx.size(); q++) {
          if (take[q]) ci[rel[o][freeIdx[q]]] = R.colours[ii];
          else nfree.push_back(freeIdx[q]);
        }
        freeIdx.swap(nfree);
      }
    }
  }
  void symCi(int s, const int8_t *a, int8_t *b) const { for (int k = 0; k < 48; k++) b[gsyms[s].fac[k]] = cmap[s][a[k]]; }
  void positionOf(const std::vector<cc::Mv> &mv, int8_t *a) const {
    int8_t b[48];
    memcpy(a, solvedCi, 48);
    for (auto it = mv.rbegin(); it != mv.rend(); ++it) {
      for (int k = 0; k < 48; k++) b[mp[it->face][it->depth][4 - it->amt][k]] = a[k];
      memcpy(a, b, 48);
    }
  }
  std::vector<cc::Mv> lineAt(size_t i) const {
    std::vector<cc::Mv> mv;
    for (uint64_t j = lineOff[i]; j < lineOff[i + 1]; j++) mv.push_back(byteMv(lineBytes[j]));
    return mv;
  }
  std::vector<cc::Mv> lineOf(uint64_t r) const { return symMoves(gsyms[srcSym[r]], lineAt(srcLine[r])); }
  bool load(const std::string &dir) {
    const double t0 = now();
    dist.assign(total, 0xFF), srcSym.assign(total, 0), srcLine.assign(total, 0xFFFFFFFF);
    for (int w = 0; w <= 48; w++) {
      char fn[32];
      snprintf(fn, sizeof fn, "w%0*d.txt", tg.size() * 4 >= 10 ? 2 : 1, w);
      FILE *in = fopen((dir + "/" + fn).c_str(), "r");
      if (!in) continue;
      char buf[4096];
      std::vector<cc::Mv> mv;
      while (fgets(buf, sizeof buf, in)) {
        std::string l(buf);
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
        if (!cc::parseMoves(l, mv)) { fprintf(stderr, "bad certificate line in %s\n", fn); return false; }
        lineOff.push_back(lineBytes.size());
        for (auto &m : mv) lineBytes.push_back(mvByte(m));
      }
      fclose(in);
    }
    lineOff.push_back(lineBytes.size());
    const size_t NL = lineOff.size() - 1;
    std::atomic<size_t> ni{0};
    std::vector<std::thread> th;
    for (unsigned t = 0; t < std::max(1u, std::thread::hardware_concurrency()); t++)
      th.emplace_back([&] {
        int8_t a[48], b[48];
        for (size_t i; (i = ni++) < NL;) {
          auto mv = lineAt(i);
          positionOf(mv, a);
          for (int s : ss) {
            symCi(s, a, b);
            const uint64_t r = rankOf(b);
            // races between threads only pick which of two equal lines is kept
            if (mv.size() < dist[r]) dist[r] = mv.size(), srcLine[r] = i, srcSym[r] = s;
          }
        }
      });
    for (auto &x : th) x.join();
    long un = 0;
    for (uint64_t r = 0; r < total; r++) { if (dist[r] == 0xFF) un++; else W = std::max<int>(W, dist[r]); }
    fprintf(stderr, "%s: %zu lines, %lu positions, worst %d (%.0fs)\n", name.c_str(), NL, (unsigned long)total, W, now() - t0);
    if (un) { fprintf(stderr, "%s: %ld positions not covered\n", name.c_str(), un); return false; }
    return true;
  }
};

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  fflush(stdout);
  int C = 8, T = std::max(1u, std::thread::hardware_concurrency()), NCHK = 0, DEEP = -1, SL = 2, SAMP = 1;
  std::vector<std::string> pos, base;
  std::string outFile, onlyFile;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--prop" && i + 1 < argc) C = atoi(argv[++i]);
    else if (o == "--threads" && i + 1 < argc) T = atoi(argv[++i]);
    else if (o == "--check" && i + 1 < argc) NCHK = atoi(argv[++i]);
    else if (o == "--deep" && i + 1 < argc) DEEP = atoi(argv[++i]);
    else if (o == "--slack" && i + 1 < argc) SL = atoi(argv[++i]);
    else if (o == "--out" && i + 1 < argc) outFile = argv[++i];
    else if (o == "--only" && i + 1 < argc) onlyFile = argv[++i];
    else if (o == "--sample" && i + 1 < argc) SAMP = std::max(1, atoi(argv[++i]));
    else if (o == "--base" && i + 1 < argc) base.push_back(argv[++i]);
    else if (o.size() > 1 && o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else pos.push_back(o);
  }
  if (pos.size() != 3) {
    fprintf(stderr, "usage: boundx STAGEDIR_A STAGEDIR_B X [--prop C] [--threads T] [--check N] [--deep S] [--slack L] [--out FILE] [--sample K] [--only FILE] [--base POOL]...\n");
    return 1;
  }
  const int X = atoi(pos[2].c_str());
  const double t0 = now();

  cc::Cube C8(8);
  const auto pf = cc::pairFacelets(C8);
  std::vector<int> fidx(C8.pos.size(), -1);
  for (int k = 0; k < 48; k++) fidx[pf[k]] = k;
  for (int f = 0; f < 6; f++)
    for (int d = 1; d <= 3; d++)
      for (int a = 1; a <= 3; a++)
        for (int k = 0; k < 48; k++) {
          int i = pf[k];
          for (int q = 0; q < a; q++) i = C8.perms[f][d - 1][i];
          mp[f][d][a][k] = fidx[i];
        }
  for (int a = 0; a <= 48; a++) { binom[a][0] = 1; for (int b = 1; b <= a; b++) binom[a][b] = binom[a - 1][b - 1] + (b < a ? binom[a - 1][b] : 0); }
  for (int m = 0; m < 256; m++)
    for (int x = 0; x < 256; x++) {
      int r = 0, j = 0;
      for (int b = 0; b < 8; b++) if (m >> b & 1) { if (x >> b & 1) r |= 1 << j; j++; }
      CX[m][x] = r;
    }
  for (int bp = 0; bp < 3; bp++)
    for (int v = 0; v < 256; v++)
      for (int j = 0; j < 25; j++) {
        uint64_t s = 0;
        int t = j;
        for (int b = 0; b < 8; b++) if (v >> b & 1) { t++; s += binom[8 * bp + b][t]; }
        BT[bp][v][j] = s;
      }
  {
    const int pm[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    const int par[6] = {1, -1, -1, 1, 1, -1};
    for (int pi = 0; pi < 6; pi++)
      for (int sg = 0; sg < 8; sg++) {
        GSym y;
        auto map = [&](const std::array<int, 3> &p) {
          std::array<int, 3> q;
          for (int r = 0; r < 3; r++) q[r] = p[pm[pi][r]] * ((sg >> r & 1) ? -1 : 1);
          return q;
        };
        for (int k = 0; k < 48; k++) {
          auto q = map(C8.pos[pf[k]]);
          for (int x = 0; x < 48; x++) if (C8.pos[pf[x]] == q) y.fac[k] = x;
        }
        for (int g = 0; g < 12; g++) for (int k = 0; k < 48; k++) if (groupOf(k) == g) y.gmap[g] = groupOf(y.fac[k]);
        for (int f = 0; f < 6; f++) {
          auto q = map(cc::Cube::normalOf(f));
          for (int f2 = 0; f2 < 6; f2++) if (cc::Cube::normalOf(f2) == q) y.face[f] = f2;
        }
        const int det = par[pi] * ((sg & 1) ? -1 : 1) * ((sg & 2) ? -1 : 1) * ((sg & 4) ? -1 : 1);
        y.flip = det < 0;
        gsyms.push_back(y);
      }
  }

  Stage A, B;
  if (!A.init(pos[0]) || !B.init(pos[1])) return 1;
  if (B.S != (A.S | A.TG)) { fprintf(stderr, "stage B must start where A ends\n"); return 1; }
  // a block covers its A class through A's symmetries, so each must also keep B's distances: A's
  // symmetries a subset of B's (2026-10-27: was equality; 4ha has 4, 4hb 16)
  if (!std::includes(B.ss.begin(), B.ss.end(), A.ss.begin(), A.ss.end())) { fprintf(stderr, "stage A has a symmetry stage B lacks\n"); return 1; }

  // actions: outer face turns and the base pools' images of cost <= C, by their full facelet map
  struct Act { int8_t D[48]; std::vector<cc::Mv> mv; };
  std::vector<Act> acts;
  {
    std::unordered_map<std::string, size_t> idx;
    auto add = [&](const std::vector<cc::Mv> &mv) {
      Act a;
      a.mv = mv;
      mapOf(mv, a.D);
      bool id = true;
      for (int k = 0; k < 48; k++) if (a.D[k] != k) id = false;
      if (id) return;
      for (int k = 0; k < 48; k++) if ((A.S >> groupOf(k) & 1) && groupOf(a.D[k]) != groupOf(k)) return;  // must keep A's solved faces
      std::string key((const char *)a.D, 48);
      auto it = idx.find(key);
      if (it != idx.end()) { if (acts[it->second].mv.size() > mv.size()) acts[it->second].mv = mv; return; }
      idx[key] = acts.size();
      acts.push_back(a);
    };
    for (int f = 0; f < 6; f++) for (int a = 1; a <= 3; a++) add({{f, 1, a}});
    for (auto &bf : base) {
      FILE *in = fopen(bf.c_str(), "r");
      if (!in) { fprintf(stderr, "cannot read %s\n", bf.c_str()); return 1; }
      char buf[4096];
      std::vector<cc::Mv> mv;
      while (fgets(buf, sizeof buf, in)) {
        if (!poolMoves(buf, mv) || (int)mv.size() > C) continue;
        std::vector<cc::Mv> inv;
        for (auto it = mv.rbegin(); it != mv.rend(); ++it) inv.push_back({it->face, it->depth, 4 - it->amt});
        for (auto &y : gsyms) { add(symMoves(y, mv)); add(symMoves(y, inv)); }
      }
      fclose(in);
    }
    std::stable_sort(acts.begin(), acts.end(), [](const Act &a, const Act &b) { return a.mv.size() < b.mv.size(); });
    fprintf(stderr, "%zu actions of cost <= %d\n", acts.size(), C);
  }

  // B positions by distance, as masks, longest first: bad[t] = the positions with dB > t
  std::vector<std::vector<uint64_t>> byLen(256);
  for (uint64_t r = 0; r < B.total; r++) byLen[B.dist[r]].push_back(r);
  const int lo = std::max(0, X - A.W);  // the deepest B threshold any A line can need
  std::vector<uint64_t> badR;
  std::vector<size_t> badCnt(256, 0);  // badCnt[t]: how many of badR have dB > t
  for (int d = B.W; d > lo; d--) {
    for (auto r : byLen[d]) badR.push_back(r);
    badCnt[d - 1] = badR.size();
  }
  for (int t = B.W; t < 256; t++) badCnt[t] = 0;
  std::vector<std::array<uint32_t, 26>> badMk(badR.size());  // masks, flattened [o][ii]
  for (size_t i = 0; i < badR.size(); i++) {
    int8_t ci[48];
    B.unrank(badR[i], ci);
    Masks mk;
    B.masksOf(ci, mk);
    for (int o = 0; o < 2; o++) for (int ii = 0; ii < 13; ii++) badMk[i][o * 13 + ii] = mk[o][ii];
  }
  fprintf(stderr, "B positions with dB > %d: %zu\n", lo, badR.size());

  // the A classes to check: lines longer than X - worst(B)
  std::vector<size_t> todo;
  for (size_t i = 0; i + 1 < A.lineOff.size(); i++) {
    const int len = A.lineOff[i + 1] - A.lineOff[i];
    if (len > X - B.W) todo.push_back(i);
  }
  if (SAMP > 1) {
    size_t k = 0;
    for (size_t i = 0; i < todo.size(); i += SAMP) todo[k++] = todo[i];
    fprintf(stderr, "sampling every %d-th class\n", SAMP);
    todo.resize(k);
  }
  fprintf(stderr, "A lines longer than %d: %zu\n", X - B.W, todo.size());

  FILE *out = nullptr;
  if (!outFile.empty() && !(out = fopen(outFile.c_str(), "w"))) { fprintf(stderr, "cannot write %s\n", outFile.c_str()); return 1; }
  auto text = [](const std::vector<cc::Mv> &mv) {
    std::string s;
    for (auto &m : mv) { if (!s.empty()) s += ' '; s += cc::mvText(m); }
    return s;
  };
  if (!onlyFile.empty()) {
    std::unordered_set<std::string> want;
    FILE *f = fopen(onlyFile.c_str(), "r");
    if (!f) { fprintf(stderr, "cannot read %s\n", onlyFile.c_str()); return 1; }
    char buf[4096];
    while (fgets(buf, sizeof buf, f)) {
      std::string l(buf);
      while (!l.empty() && (l.back() == '\n' || l.back() == '\r' || l.back() == ' ')) l.pop_back();
      if (!l.empty()) want.insert(l);
    }
    fclose(f);
    size_t k = 0;
    for (size_t i = 0; i < todo.size(); i++) if (want.count(text(A.lineAt(todo[i])))) todo[k++] = todo[i];
    todo.resize(k);
    fprintf(stderr, "--only: %zu of %zu lines found\n", k, want.size());
  }
  std::atomic<size_t> ni{0};
  std::atomic<long> nDup{0}, nOk{0}, nFail{0}, nBad0{0}, nLeft{0}, nPaths{0}, nChk{0}, nChkBad{0};
  const size_t stride = NCHK > 0 ? std::max<size_t>(1, todo.size() / NCHK) : 0;
  // play mv on a full colouring (A colours as 100 + c, B colours as c)
  auto play = [&](const int *J0, const std::vector<cc::Mv> &mv, int *J) {
    int b[48];
    memcpy(J, J0, 48 * sizeof(int));
    for (auto &m : mv) {
      for (int k = 0; k < 48; k++) b[mp[m.face][m.depth][m.amt][k]] = J[k];
      memcpy(J, b, 48 * sizeof(int));
    }
  };
  // the cost of finishing a played colouring: -1 unless A is solved, else dB
  auto finish = [&](const int *J) {
    for (int k = 0; k < 48; k++) {
      const int ac = J[k] >= 100 ? J[k] - 100 : 0;
      if ((A.TG >> groupOf(k) & 1) && ac != A.solvedCi[k]) return -1;
      if (!(A.TG >> groupOf(k) & 1) && ac) return -1;
    }
    int8_t ci[48];
    for (int k = 0; k < 48; k++) ci[k] = B.solvedCi[k];
    for (int o = 0; o < 2; o++) for (int k : B.rel[o]) ci[k] = J[k] < 100 ? J[k] : 0;
    return (int)B.dist[B.rankOf(ci)];
  };
  std::vector<long> leftHist(8, 0), pathsUsed(64, 0);
  std::mutex mu;
  std::vector<std::thread> th;
  for (int t = 0; t < T; t++)
    th.emplace_back([&] {
      int8_t u[48], y[48], D0[48], G[48], Pinv[48];
      for (size_t qi; (qi = ni++) < todo.size();) {
        const auto L0 = A.lineAt(todo[qi]);
        A.positionOf(L0, u);
        const uint64_t ru = A.rankOf(u);
        if (A.dist[ru] != L0.size()) { nDup++; continue; }  // a duplicate class line
        const int c0 = L0.size();
        mapOf(L0, D0);
        // the alternatives: (cost, facelet map)
        struct Path { int c; std::array<int8_t, 48> D; std::vector<cc::Mv> mv; };
        std::vector<Path> paths;
        std::unordered_set<std::string> seenMap;
        {
          std::string k((const char *)D0, 48);
          seenMap.insert(k);
        }
        bool keepMv = out != nullptr;
        std::vector<size_t> deepUsed;
        auto addPath = [&](std::vector<cc::Mv> mv) {
          simplify(mv);
          if ((int)mv.size() >= X) return;
          Path p;
          p.c = mv.size();
          mapOf(mv, p.D.data());
          std::string k((const char *)p.D.data(), 48);
          if (!seenMap.insert(k).second) return;
          if (keepMv || (stride && qi % stride == 0)) p.mv = mv;
          paths.push_back(p);
        };
        // stage-symmetric copies of L0 that fix u
        for (int s : A.ss) {
          int8_t b[48];
          A.symCi(s, u, b);
          if (memcmp(b, u, 48) == 0) addPath(symMoves(gsyms[s], L0));
        }
        // an action, then the line of where it leads
        for (auto &a : acts) {
          for (int k = 0; k < 48; k++) y[a.D[k]] = u[k];
          const uint64_t ry = A.rankOf(y);
          const int c = (int)a.mv.size() + A.dist[ry];
          if (c > c0 + 2) continue;  // merges save at most 2
          auto mv = a.mv;
          auto l = A.lineOf(ry);
          mv.insert(mv.end(), l.begin(), l.end());
          addPath(mv);
        }
        std::sort(paths.begin(), paths.end(), [](const Path &a, const Path &b) { return a.c < b.c; });
        // the bad colourings for L0, in its B frame
        const int thr = X - c0;  // B states with dB > thr are bad
        if (thr >= B.W) { nOk++; continue; }
        const size_t nb = badCnt[thr];
        nBad0 += nb;
        nPaths += paths.size();
        // per path: G = D0 o P^-1 on B's ranked positions, as byte tables
        auto buildPT = [&](const Path &p, std::array<uint32_t, 2 * 3 * 256> &PT) {
          for (int k = 0; k < 48; k++) Pinv[p.D[k]] = k;
          for (int k = 0; k < 48; k++) G[k] = D0[(int)Pinv[k]];
          for (int o = 0; o < 2; o++) {
            uint32_t bit[24] = {};
            for (int i = 0; i < B.orr[o].n; i++) {
              const int src = B.relIdx[(int)G[B.rel[o][i]]];
              if (src < 0) { fprintf(stderr, "a path does not keep B's positions\n"); exit(1); }
              bit[src] = 1u << i;
            }
            for (int b = 0; b < 3; b++)
              for (int v = 0; v < 256; v++) {
                uint32_t x = 0;
                for (int tt = 0; tt < 8; tt++) if (v >> tt & 1 && 8 * b + tt < 24) x |= bit[8 * b + tt];
                PT[(o * 3 + b) * 256 + v] = x;
              }
          }
        };
        std::vector<std::array<uint32_t, 2 * 3 * 256>> PTs(paths.size());
        for (size_t pi = 0; pi < paths.size(); pi++) buildPT(paths[pi], PTs[pi]);
        long left = 0;
        int used = 0;
        std::vector<char> pathHit(paths.size(), 0);
        std::vector<size_t> leftIdx;
        for (size_t i = 0; i < nb; i++) {
          bool ok = false;
          for (size_t pi = 0; pi < paths.size() && !ok; pi++) {
            if (paths[pi].c >= X) break;
            Masks qm;
            for (int o = 0; o < 2; o++)
              for (int ii = 0; ii < B.orr[o].nc; ii++) {
                const uint32_t m = badMk[i][o * 13 + ii];
                const uint32_t *P = PTs[pi].data() + o * 3 * 256;
                qm[o][ii] = P[m & 255] | P[256 + (m >> 8 & 255)] | P[512 + (m >> 16 & 255)];
              }
            if (paths[pi].c + B.dist[B.maskRank(qm)] <= X) {
              ok = true, pathHit[pi] = 1;
              if (stride && qi % stride == 0 && i < 20) {
                // the full colouring: A's pieces from u, B's from the bad state carried back through L0
                int8_t s0[48];
                B.unrank(badR[i], s0);
                int J0[48], J[48];
                for (int k = 0; k < 48; k++) J0[k] = u[k] ? 100 + u[k] : s0[(int)D0[k]];
                play(J0, L0, J);
                const int d0 = finish(J);
                play(J0, paths[pi].mv, J);
                const int d1 = finish(J);
                nChk++;
                if (d0 != B.dist[badR[i]] || d1 < 0 || (int)paths[pi].mv.size() + d1 > X) {
                  nChkBad++;
                  fprintf(stderr, "check failed: class %zu, L0 gives %d (want %d), path gives %d + %d\n", todo[qi], d0, (int)B.dist[badR[i]], (int)paths[pi].mv.size(), d1);
                }
              }
            }
          }
          if (!ok) left++, leftIdx.push_back(i);
        }
        for (char h : pathHit) used += h;
        if (left && DEEP >= 0) {
          // one action with more slack, or two actions, then the line
          const size_t first = paths.size();
          keepMv = NCHK > 0 || out;
          for (auto &a : acts) {
            for (int k = 0; k < 48; k++) y[a.D[k]] = u[k];
            const uint64_t ry = A.rankOf(y);
            if ((int)a.mv.size() + A.dist[ry] > c0 + SL) continue;
            auto mv = a.mv;
            auto l = A.lineOf(ry);
            mv.insert(mv.end(), l.begin(), l.end());
            addPath(mv);
          }
          for (auto &a1 : acts) {
            int8_t y1[48];
            for (int k = 0; k < 48; k++) y1[a1.D[k]] = u[k];
            const int c1 = (int)a1.mv.size() + A.dist[A.rankOf(y1)];
            if (c1 > c0 + SL + DEEP) continue;
            for (auto &a2 : acts) {
              if ((int)(a1.mv.size() + a2.mv.size()) > c0 + SL) break;  // acts are sorted by cost
              for (int k = 0; k < 48; k++) y[a2.D[k]] = y1[k];
              const uint64_t ry = A.rankOf(y);
              if ((int)(a1.mv.size() + a2.mv.size()) + A.dist[ry] > c0 + SL) continue;
              auto mv = a1.mv;
              mv.insert(mv.end(), a2.mv.begin(), a2.mv.end());
              auto l = A.lineOf(ry);
              mv.insert(mv.end(), l.begin(), l.end());
              addPath(mv);
            }
          }
          std::sort(paths.begin() + first, paths.end(), [](const Path &a, const Path &b) { return a.c < b.c; });
          std::array<uint32_t, 2 * 3 * 256> PT;
          for (size_t pi = first; pi < paths.size() && !leftIdx.empty(); pi++) {
            buildPT(paths[pi], PT);
            std::vector<size_t> still;
            for (size_t i : leftIdx) {
              Masks qm;
              for (int o = 0; o < 2; o++)
                for (int ii = 0; ii < B.orr[o].nc; ii++) {
                  const uint32_t m = badMk[i][o * 13 + ii];
                  const uint32_t *P = PT.data() + o * 3 * 256;
                  qm[o][ii] = P[m & 255] | P[256 + (m >> 8 & 255)] | P[512 + (m >> 16 & 255)];
                }
              if (paths[pi].c + B.dist[B.maskRank(qm)] > X) { still.push_back(i); continue; }
              if (NCHK > 0) {  // every deep cover is replayed
                int8_t s0[48];
                B.unrank(badR[i], s0);
                int J0[48], J[48];
                for (int k = 0; k < 48; k++) J0[k] = u[k] ? 100 + u[k] : s0[(int)D0[k]];
                play(J0, paths[pi].mv, J);
                const int d1 = finish(J);
                nChk++;
                if (d1 < 0 || (int)paths[pi].mv.size() + d1 > X) {
                  nChkBad++;
                  fprintf(stderr, "deep check failed: class %zu, path gives %d + %d\n", todo[qi], (int)paths[pi].mv.size(), d1);
                }
              }
            }
            if (still.size() < leftIdx.size()) used++, deepUsed.push_back(pi);
            leftIdx.swap(still);
          }
          fprintf(stderr, "class %zu: %zu deep alternatives, %ld -> %zu colourings left\n", todo[qi], paths.size() - first, left, leftIdx.size());
          left = leftIdx.size();
        }
        nLeft += left;
        if (left) nFail++;
        else nOk++;
        if (out) {
          std::string blk = text(L0) + "\n";
          for (size_t pi = 0; pi < pathHit.size(); pi++) if (pathHit[pi]) blk += text(paths[pi].mv) + "\n";
          for (size_t pi : deepUsed) blk += text(paths[pi].mv) + "\n";
          blk += "\n";
          std::lock_guard<std::mutex> g(mu);
          fputs(blk.c_str(), out);
        }
        std::lock_guard<std::mutex> g(mu);
        leftHist[left == 0 ? 0 : left < 10 ? 1 : left < 100 ? 2 : left < 1000 ? 3 : 4]++;
        pathsUsed[std::min(used, 63)]++;
      }
    });
  for (auto &x : th) x.join();
  fprintf(stderr, "classes: %ld covered, %ld not (%ld duplicate lines skipped); bad colourings %ld, left %ld; %.1f alternatives per class (%.0fs)\n",
          nOk.load(), nFail.load(), nDup.load(), nBad0.load(), nLeft.load(), todo.size() ? (double)nPaths.load() / todo.size() : 0.0, now() - t0);
  fprintf(stderr, "classes by colourings left: 0: %ld, 1-9: %ld, 10-99: %ld, 100-999: %ld, 1000+: %ld\n", leftHist[0], leftHist[1], leftHist[2], leftHist[3], leftHist[4]);
  fprintf(stderr, "alternatives used per class:");
  for (int i = 0; i < 64; i++) if (pathsUsed[i]) fprintf(stderr, " %d:%ld", i, pathsUsed[i]);
  fprintf(stderr, "\n");
  if (out) fclose(out);
  if (NCHK) fprintf(stderr, "check: %ld colourings replayed, %ld failed\n", nChk.load(), nChkBad.load());
  printf("%s %s X=%d: %ld of %zu classes not covered\n", A.name.c_str(), B.name.c_str(), X, nFail.load(), todo.size() - nDup.load());
  return 0;
}
