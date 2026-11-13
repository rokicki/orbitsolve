// stagecompose: a certificate for a stage C = S..(TA+TB) made by composing two certified stages
// A = S..TA and B = (S+TA)..TB: for every symmetry class of C (least key among its images under
// the stage symmetries of C), the line of its A state (an image of an A certificate line), then
// the line of the B state that leaves (an image of a B line), joined and simplified (same-axis
// runs combined, as certextend).  Every joined line is replayed on the C position before it is
// written.  Output: OUTDIR/wNN.txt, NN = C target pieces off their home face, as certmerge
// names them; a starting certificate for certextend (e.g. 4h from 4ha + 4hb, 2026-10-27).
// Positions are enumerated orbit by orbit (each orbit's arrangements of its target colours on
// its target faces), so C may have up to 2^32 positions.
//   stagecompose STAGEDIR_A STAGEDIR_B OUTDIR [--threads T]
#include "certcube.h"
#include <algorithm>
#include <atomic>
#include <map>
#include <mutex>
#include <thread>
#include <unordered_map>

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
static const int AXIS[6] = {0, 1, 2, 1, 2, 0}, SIDE[6] = {0, 0, 0, 1, 1, 1};  // ULFRBD
static void simplify(std::vector<cc::Mv> &v) {  // as certextend
  for (bool again = true; again;) {
    again = false;
    std::vector<cc::Mv> r;
    for (size_t i = 0; i < v.size();) {
      const int ax = AXIS[v[i].face];
      int amt[2][4] = {};
      size_t j = i;
      for (; j < v.size() && AXIS[v[j].face] == ax; j++) {
        int &a = amt[SIDE[v[j].face]][v[j].depth];
        a = (a + v[j].amt) % 4;
      }
      const size_t before = r.size();
      for (int f = 0; f < 6; f++) if (AXIS[f] == ax) for (int d = 0; d < 4; d++)
        if (amt[SIDE[f]][d]) r.push_back({f, d, amt[SIDE[f]][d]});
      if (r.size() - before != j - i) again = true;
      i = j;
    }
    v.swap(r);
  }
}
static std::string stageName(const std::string &dir) {
  std::string d = dir;
  while (d.size() > 1 && d.back() == '/') d.pop_back();
  return d.substr(d.rfind('/') == std::string::npos ? 0 : d.rfind('/') + 1);
}

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  fflush(stdout);
  int T = std::max(1u, std::thread::hardware_concurrency());
  std::vector<std::string> pos;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--threads" && i + 1 < argc) T = atoi(argv[++i]);
    else if (o.size() > 1 && o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else pos.push_back(o);
  }
  if (pos.size() != 3) { fprintf(stderr, "usage: stagecompose STAGEDIR_A STAGEDIR_B OUTDIR [--threads T]\n"); return 1; }
  const std::string na = stageName(pos[0]), nb = stageName(pos[1]);
  const size_t da = na.find(".."), db = nb.find("..");
  if (da == std::string::npos || db == std::string::npos) { fprintf(stderr, "stage directories must be named <solved>..<target>\n"); return 1; }
  const int S = groupMask(na.substr(0, da)), TA = groupMask(na.substr(da + 2));
  const int SB = groupMask(nb.substr(0, db)), TB = groupMask(nb.substr(db + 2));
  if (S < 0 || TA <= 0 || SB < 0 || TB <= 0 || SB != (S | TA) || (TB & SB)) { fprintf(stderr, "B must start where A ends\n"); return 1; }
  const int TC = TA | TB;

  cc::Cube C8(8);
  const auto pf = cc::pairFacelets(C8);
  std::vector<int> fidx(C8.pos.size(), -1);
  for (int k = 0; k < 48; k++) fidx[pf[k]] = k;
  static int mp[6][4][4][48];
  for (int f = 0; f < 6; f++) for (int d = 1; d <= 3; d++) for (int a = 1; a <= 3; a++)
    for (int k = 0; k < 48; k++) { int i = pf[k]; for (int q = 0; q < a; q++) i = C8.perms[f][d - 1][i]; mp[f][d][a][k] = fidx[i]; }
  auto groupOf = [](int k) { return (k % 8) / 4 * 6 + k / 8; };
  // the 48 cube symmetries (as certextend): facelet map, group map, face map, reflection
  struct Sym { int fac[48]; int gmap[12]; int face[6]; bool flip; };
  std::vector<Sym> syms;
  {
    const int pm[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    const int par[6] = {1, -1, -1, 1, 1, -1};
    for (int pi = 0; pi < 6; pi++)
      for (int sg = 0; sg < 8; sg++) {
        Sym y;
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
        y.flip = par[pi] * ((sg & 1) ? -1 : 1) * ((sg & 2) ? -1 : 1) * ((sg & 4) ? -1 : 1) < 0;
        syms.push_back(y);
      }
  }
  auto keeps = [&](const Sym &y, int solved, int target) {
    int s2 = 0, t2 = 0;
    for (int g = 0; g < 12; g++) { if (solved >> g & 1) s2 |= 1 << y.gmap[g]; if (target >> g & 1) t2 |= 1 << y.gmap[g]; }
    return s2 == solved && t2 == target;
  };
  auto symMoves = [&](const Sym &y, const std::vector<cc::Mv> &mv) {
    std::vector<cc::Mv> r;
    for (auto &m : mv) r.push_back({y.face[m.face], m.depth, y.flip ? 4 - m.amt : m.amt});
    return r;
  };
  // a colouring: the group of the piece at each pair facelet, -1 = not tracked
  typedef std::array<int8_t, 48> Col;
  auto apply = [&](Col &c, const std::vector<cc::Mv> &mv) {
    Col b;
    for (auto &m : mv) { for (int k = 0; k < 48; k++) b[mp[m.face][m.depth][m.amt][k]] = c[k]; c = b; }
  };
  auto applyInv = [&](Col &c, const std::vector<cc::Mv> &mv) {  // the position mv solves
    Col b;
    for (auto it = mv.rbegin(); it != mv.rend(); ++it) { for (int k = 0; k < 48; k++) b[mp[it->face][it->depth][4 - it->amt][k]] = c[k]; c = b; }
  };
  auto symCol = [&](const Sym &y, const Col &a) { Col b; for (int k = 0; k < 48; k++) b[y.fac[k]] = a[k] < 0 ? -1 : y.gmap[a[k]]; return b; };
  // the C target positions (facelets whose home group is a C target), and a stage state's key
  // (exact): 2 bits per position, 1 + the group's index among the tracked groups of its orbit
  // (a position only ever holds its own orbit's pieces), 0 if the group isn't tracked
  std::vector<int> rel;
  for (int k = 0; k < 48; k++) if (TC >> groupOf(k) & 1) rel.push_back(k);
  if (rel.size() > 32) { fprintf(stderr, "too many target positions\n"); return 1; }
  auto keyOf = [&](const Col &c, int tracked) {
    uint64_t h = 0;
    for (int k : rel) {
      int d = 0;
      if (c[k] >= 0 && (tracked >> c[k] & 1)) d = 1 + __builtin_popcount(tracked & ((1 << c[k]) - 1) & (63 << (c[k] / 6 * 6)));
      if (d > 3) { fprintf(stderr, "more than 3 tracked groups in an orbit\n"); exit(1); }
      h = h << 2 | d;
    }
    return h;
  };
  // a stage's lines as a map: state key -> (line, symmetry) for every image under its stage symmetries
  struct Entry { uint32_t line; uint8_t sym; };
  auto load = [&](const std::string &dir, int solved, int target, std::vector<std::vector<cc::Mv>> &lines,
                  std::unordered_map<uint64_t, Entry> &m) {
    std::vector<int> ss;
    for (int s = 0; s < 48; s++) if (keeps(syms[s], solved, target)) ss.push_back(s);
    for (int w = 0; w <= 48; w++) {
      FILE *in = nullptr;
      for (const char *fmt : {"/w%02d.txt", "/w%d.txt"}) {
        char fn[32];
        snprintf(fn, sizeof fn, fmt, w);
        if ((in = fopen((dir + fn).c_str(), "r"))) break;
      }
      if (!in) continue;
      char buf[4096];
      std::vector<cc::Mv> mv;
      while (fgets(buf, sizeof buf, in)) {
        std::string l(buf);
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
        if (!cc::parseMoves(l, mv)) { fprintf(stderr, "bad line in %s\n", dir.c_str()); exit(1); }
        Col c;
        for (int k = 0; k < 48; k++) c[k] = ((solved | target) >> groupOf(k) & 1) ? groupOf(k) : -1;
        applyInv(c, mv);
        for (int s : ss) m.emplace(keyOf(symCol(syms[s], c), target), Entry{(uint32_t)lines.size(), (uint8_t)s});
        lines.push_back(mv);
      }
      fclose(in);
    }
    fprintf(stderr, "%s: %zu lines, %zu states, %zu stage symmetries\n", stageName(dir).c_str(), lines.size(), m.size(), ss.size());
  };
  std::vector<std::vector<cc::Mv>> linesA, linesB;
  std::unordered_map<uint64_t, Entry> mapA, mapB;
  load(pos[0], S, TA, linesA, mapA);
  load(pos[1], SB, TB, linesB, mapB);

  // C states orbit by orbit: each orbit's arrangements of its C target groups on its target positions
  std::vector<int> relO[2];
  for (int k : rel) relO[(k % 8) / 4].push_back(k);
  std::vector<std::vector<int8_t>> arr[2];  // arr[o][i][j]: the group at relO[o][j]
  std::map<std::vector<int8_t>, uint32_t> arrIdx[2];
  for (int o = 0; o < 2; o++) {
    std::vector<int8_t> v;
    for (int k : relO[o]) v.push_back(groupOf(k));
    std::sort(v.begin(), v.end());
    do { arrIdx[o][v] = arr[o].size(); arr[o].push_back(v); } while (std::next_permutation(v.begin(), v.end()));
  }
  const uint64_t n0 = arr[0].size(), n1 = arr[1].size(), total = n0 * n1;
  std::vector<int> ssC;
  for (int s = 0; s < 48; s++) if (keeps(syms[s], S, TC)) ssC.push_back(s);
  fprintf(stderr, "C: %zu target positions, %lu x %lu = %lu states, %zu stage symmetries\n", rel.size(), (unsigned long)n0, (unsigned long)n1, (unsigned long)total, ssC.size());
  // a symmetry's action on one orbit's arrangement: which orbit it lands in and its index there
  std::vector<std::vector<std::pair<int, uint32_t>>> img[2];
  for (int o = 0; o < 2; o++) {
    img[o].resize(ssC.size());
    for (size_t si = 0; si < ssC.size(); si++) {
      const Sym &y = syms[ssC[si]];
      const int o2 = (y.fac[relO[o][0]] % 8) / 4;
      for (auto &v : arr[o]) {
        std::vector<int8_t> w(relO[o2].size());
        for (size_t j = 0; j < relO[o].size(); j++) {
          const int k2 = y.fac[relO[o][j]];
          const size_t j2 = std::find(relO[o2].begin(), relO[o2].end(), k2) - relO[o2].begin();
          w[j2] = y.gmap[v[j]];
        }
        auto it = arrIdx[o2].find(w);
        if (it == arrIdx[o2].end()) { fprintf(stderr, "a symmetry leaves the stage\n"); return 1; }
        img[o][si].push_back({o2, it->second});
      }
    }
  }
  const int NW = __builtin_popcount(TC) * 4 >= 10 ? 2 : 1;
  std::atomic<uint64_t> next{0}, classes{0}, failed{0};
  std::atomic<int> maxLen{0};
  std::vector<std::atomic<long>> perLen(256);
  std::mutex outMu;
  std::vector<FILE *> out(49, nullptr);
  for (int w = 0; w <= (int)rel.size(); w++) {
    char fn[32];
    snprintf(fn, sizeof fn, "/w%0*d.txt", NW, w);
    out[w] = fopen((pos[2] + fn).c_str(), "w");
    if (!out[w]) { fprintf(stderr, "cannot write %s%s\n", pos[2].c_str(), fn); return 1; }
  }
  auto work = [&] {
    std::vector<std::string> buf(49);
    for (uint64_t i0; (i0 = next++) < n0;) {
      for (uint64_t i1 = 0; i1 < n1; i1++) {
        const uint64_t key = i0 * n1 + i1;
        bool canon = true;
        for (size_t si = 0; si < ssC.size() && canon; si++) {
          auto a = img[0][si][i0], b = img[1][si][i1];
          const uint64_t k2 = a.first == 0 ? (uint64_t)a.second * n1 + b.second : (uint64_t)b.second * n1 + a.second;
          if (k2 < key) canon = false;
        }
        if (!canon) continue;
        Col c;
        for (int k = 0; k < 48; k++) c[k] = (S >> groupOf(k) & 1) ? groupOf(k) : -1;
        int off = 0;
        for (size_t j = 0; j < relO[0].size(); j++) { c[relO[0][j]] = arr[0][i0][j]; off += arr[0][i0][j] != groupOf(relO[0][j]); }
        for (size_t j = 0; j < relO[1].size(); j++) { c[relO[1][j]] = arr[1][i1][j]; off += arr[1][i1][j] != groupOf(relO[1][j]); }
        auto ea = mapA.find(keyOf(c, TA));
        if (ea == mapA.end()) { failed++; continue; }
        std::vector<cc::Mv> mv = symMoves(syms[ea->second.sym], linesA[ea->second.line]);
        Col c2 = c;
        apply(c2, mv);
        auto eb = mapB.find(keyOf(c2, TB));
        if (eb == mapB.end()) { failed++; continue; }
        auto mb = symMoves(syms[eb->second.sym], linesB[eb->second.line]);
        mv.insert(mv.end(), mb.begin(), mb.end());
        simplify(mv);
        Col c3 = c;
        apply(c3, mv);
        bool ok = true;
        for (int k = 0; k < 48 && ok; k++) if (c3[k] >= 0 && c3[k] != groupOf(k)) ok = false;
        if (!ok) { failed++; continue; }
        classes++;
        perLen[mv.size()]++;
        int ml = maxLen.load();
        while ((int)mv.size() > ml && !maxLen.compare_exchange_weak(ml, mv.size())) {}
        std::string &s = buf[off];
        for (size_t q = 0; q < mv.size(); q++) { if (q) s += ' '; s += cc::mvText(mv[q]); }
        s += '\n';
        if (s.size() > (1 << 20)) { std::lock_guard<std::mutex> g(outMu); fwrite(s.data(), 1, s.size(), out[off]); s.clear(); }
      }
    }
    std::lock_guard<std::mutex> g(outMu);
    for (int w = 0; w <= 48; w++) if (!buf[w].empty()) fwrite(buf[w].data(), 1, buf[w].size(), out[w]);
  };
  std::vector<std::thread> th;
  for (int t = 0; t < T; t++) th.emplace_back(work);
  for (auto &x : th) x.join();
  for (auto f : out) if (f) fclose(f);
  printf("%lu classes written, %lu failed, longest %d; lengths:", (unsigned long)classes.load(), (unsigned long)failed.load(), maxLen.load());
  for (int L = 0; L < 256; L++) if (perLen[L]) printf(" %d:%ld", L, perLen[L].load());
  printf("\n");
  return failed ? 1 : 0;
}
