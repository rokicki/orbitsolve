// certextend: extend a pair-stage certificate incrementally with streamed algorithms.
// The certificate is loaded as a dense table (every position of the stage: its line length and
// the line).  New algorithms are streamed; each image (48 cube symmetries x inverse, and the
// algorithm with leading/trailing outer face turns stripped) that keeps the stage is skipped when
// its effect on the stage was already seen at the same or lower cost (--seen FILE keeps that set
// across runs).  A new image A is tried
//   seed: as the first block in front of every line of length <= K (--near K): the position
//         A^-1(q) gets A + line(q) when that is shorter than its line;
//   band: on every class at the top B lengths (--band B): A(p) is looked up in the whole table.
// Every position that gets shorter is then propagated, cheapest first, through the cheap actions
// (outer face turns, every new image of cost <= C, and the --base pools' images of cost <= C):
// x = b^-1(y) gets b + line(y).  Only shortened positions are ever expanded.  Every new line is
// replayed before it is printed; stdout is one line per shortened class, ready for certmerge.
// Pools: "COST MOVES [D0|D1]" or plain move lines; "-" is stdin.
// --propall: before the propagation, every class is tried through the fresh cheap actions (new
// images of cost <= C from the streamed pools; the table is already closed under the face turns and
// base pools); what that shortens is then propagated as usual through all the cheap actions.
// --proptop L: before the propagation, every class of length >= L is tried as the target of every
// cheap action (cost <= C, fresh or not): x is solved by b + line(y) where y = b(x), so only the top
// classes are looked at (|top| x |actions| lookups, for much larger C than --propall affords).
// --proptop2 C2: with --proptop, also two blocks: x by a (cost <= C) then b (cost <= C2) then
// line(z), z = b(a(x)) (|top| x |actions| x |actions of cost <= C2|: for a handful of classes).
// --slices (with --keep): the inner U and D layers (2U, 3U, 2D, 3D, every amount) never touch a U or
// D face piece, so under keep u / keep ud they are legal one-move actions; they are added to the
// propagation actions as fresh (so --propall runs every class through them).
// --keep u (stage 1u) / --keep ud (1ud, 2ud): the lines only keep the other orbits' U (and D) face
// sets, so only symmetry images that map U to U (or U to U or D) are used (a symmetry taking U
// elsewhere would turn "keep U" into "keep F").
//
//   certextend STAGEDIR [--near K] [--band B] [--worst] [--miss M] [--merge G] [--deepband C1] [--missacts] [--prop C] [--threads T] [--seen FILE] [--keep u|ud]
//              [--base POOL]... POOL...
// --worst: only the band matters; each image uses the seeds or the band, whichever is fewer
// positions to try (the seeds then still shorten whatever they reach).
// --miss M: a band lookup q = A(p) that misses by fewer than M moves (A + line(q) < line(p) + M)
// keeps q with the bound it would need (line(q) <= line(p) - 1 - |A|); after the stream the pools
// are streamed again with those q as the band, shortening each to its bound where an image can.
// A later run then finds p through A and q's new line.
// --deepband C1: every band position p, moved by every action b of at most C1 moves, becomes a
// near miss y = b(p) with the bound line(p) - 1 - |b| (see --miss); propagation brings a shortened
// y back to p.
// --missacts: the near-miss pass tries the cheap actions (cost <= C: face turns, base pools, new
// images) as the first block instead of streaming the pools again (|actions| x |near misses|
// lookups; with --deepband, two short blocks and then a certificate line).
// --merge G: also try seed and band candidates up to G moves longer than needed when the run of
// same-axis turns ending A and the run starting line(q) share a layer (turns of one axis commute,
// so they combine; the joined line is simplified and counted exactly).
#include "certcube.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

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
static std::string text(const std::vector<cc::Mv> &mv) {
  std::string s;
  for (auto &m : mv) { if (!s.empty()) s += ' '; s += cc::mvText(m); }
  return s;
}
static inline uint8_t mvByte(const cc::Mv &m) { return m.face << 4 | m.depth << 2 | m.amt; }
static inline cc::Mv byteMv(uint8_t b) { return {b >> 4, b >> 2 & 3, b & 3}; }
// turns of one axis commute: within each run of same-axis turns, turns of the same layer combine
// (repeated until a pass changes nothing, since an emptied run can join its neighbours)
static const int AXIS[6] = {0, 1, 2, 1, 2, 0}, SIDE[6] = {0, 0, 0, 1, 1, 1};  // ULFRBD
static void simplify(std::vector<cc::Mv> &v) {
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
// the run of same-axis turns at the front (or back) of a line: axis << 6 | a bit per layer
// (side * 3 + depth - 1), 255 if empty; two lines can combine at a seam when the keys share a bit
static uint8_t seamKey(const std::vector<cc::Mv> &v, bool front) {
  if (v.empty()) return 255;
  const int ax = AXIS[(front ? v.front() : v.back()).face];
  int m = 0;
  for (size_t k = 0; k < v.size(); k++) {
    const cc::Mv &x = front ? v[k] : v[v.size() - 1 - k];
    if (AXIS[x.face] != ax) break;
    m |= 1 << (SIDE[x.face] * 3 + x.depth - 1);
  }
  return (uint8_t)(ax << 6 | m);
}
static inline bool seamMeets(uint8_t a, uint8_t b) { return a != 255 && b != 255 && (a >> 6) == (b >> 6) && (a & b & 63); }
// one pool line: a plain move list, maybe after a COST token and before a D0/D1 flag
static bool poolMoves(const char *buf, std::vector<cc::Mv> &mv) {
  size_t n = strlen(buf);
  if (!n || buf[n - 1] != '\n' || buf[0] == '#') return false;  // cut-off last line, comments
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

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  fflush(stdout);
  int K = 10, B = 1, C = 8, T = std::max(1u, std::thread::hardware_concurrency()), seenBits = 28;
  std::string seenFile;
  int M = 0, MG = 0, DB = 0;
  bool worst = false, missActs = false, propAll = false;
  int propTop = 0, propTop2 = 0;
  bool slices = false;
  int keep = 0;  // 0 any symmetry image, 1 images keeping U, 2 images keeping the U/D axis
  std::vector<std::string> pos, base;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--near" && i + 1 < argc) K = atoi(argv[++i]);
    else if (o == "--band" && i + 1 < argc) B = atoi(argv[++i]);
    else if (o == "--worst") worst = true;
    else if (o == "--miss" && i + 1 < argc) M = atoi(argv[++i]);
    else if (o == "--merge" && i + 1 < argc) MG = atoi(argv[++i]);
    else if (o == "--deepband" && i + 1 < argc) DB = atoi(argv[++i]);
    else if (o == "--missacts") missActs = true;
    else if (o == "--propall") propAll = true;
    else if (o == "--slices") slices = true;
    else if (o == "--proptop" && i + 1 < argc) propTop = atoi(argv[++i]);
    else if (o == "--proptop2" && i + 1 < argc) propTop2 = atoi(argv[++i]);
    else if (o == "--keep" && i + 1 < argc) {
      std::string k = argv[++i];
      if (k == "u") keep = 1;
      else if (k == "ud") keep = 2;
      else { fprintf(stderr, "--keep takes u or ud\n"); return 1; }
    }
    else if (o == "--prop" && i + 1 < argc) C = atoi(argv[++i]);
    else if (o == "--threads" && i + 1 < argc) T = atoi(argv[++i]);
    else if (o == "--seen" && i + 1 < argc) seenFile = argv[++i];
    else if (o == "--seenbits" && i + 1 < argc) seenBits = atoi(argv[++i]);
    else if (o == "--base" && i + 1 < argc) base.push_back(argv[++i]);
    else if (o.size() > 1 && o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else pos.push_back(o);
  }
  if (pos.size() < 1) {
    fprintf(stderr, "usage: certextend STAGEDIR [--near K] [--band B] [--worst] [--miss M] [--merge G] [--deepband C1] [--missacts] [--prop C] [--threads T] [--seen FILE] [--seenbits N] [--keep u|ud] [--propall] [--proptop L] [--proptop2 C2] [--slices] [--base POOL]... POOL...\n");
    return 1;
  }
  const double t0 = now();
  std::string dir = pos[0];
  while (dir.size() > 1 && dir.back() == '/') dir.pop_back();
  const std::string sname = dir.substr(dir.rfind('/') == std::string::npos ? 0 : dir.rfind('/') + 1);
  const size_t dd = sname.find("..");
  if (dd == std::string::npos) { fprintf(stderr, "stage directory must be named <solved>..<target>\n"); return 1; }
  const int S = groupMask(sname.substr(0, dd)), TG = groupMask(sname.substr(dd + 2));
  if (S < 0 || TG <= 0 || (S & TG)) { fprintf(stderr, "bad stage name\n"); return 1; }

  // the 48 pair facelets on 8x8x8 and the move maps on them (as certmerge)
  cc::Cube C8(8);
  const auto pf = cc::pairFacelets(C8);
  std::vector<int> fidx(C8.pos.size(), -1);
  for (int k = 0; k < 48; k++) fidx[pf[k]] = k;
  static int mp[6][4][4][48];
  for (int f = 0; f < 6; f++)
    for (int d = 1; d <= 3; d++)
      for (int a = 1; a <= 3; a++)
        for (int k = 0; k < 48; k++) {
          int i = pf[k];
          for (int q = 0; q < a; q++) i = C8.perms[f][d - 1][i];
          mp[f][d][a][k] = fidx[i];
        }
  auto groupOf = [](int k) { return (k % 8) / 4 * 6 + k / 8; };
  std::vector<int> tg;
  for (int g = 0; g < 12; g++) if (TG >> g & 1) tg.push_back(g);
  int8_t cidx[12];  // colour index: 0 = don't care, 1.. = target group
  for (int g = 0; g < 12; g++) cidx[g] = (TG >> g & 1) ? std::find(tg.begin(), tg.end(), g) - tg.begin() + 1 : 0;
  const int NC = tg.size() + 1;
  // ranked positions: the free positions of each orbit that holds a target group
  std::vector<int> rel[2];
  int cnt[2][13] = {};
  for (int k = 0; k < 48; k++) {
    const int o = (k % 8) / 4;
    bool on = false;
    for (int g : tg) if (g / 6 == o) on = true;
    if (!on || (S >> groupOf(k) & 1)) continue;
    rel[o].push_back(k);
  }
  int relIdx[48];  // a ranked position's index in its orbit's rel (images keep each rel set)
  for (int k = 0; k < 48; k++) relIdx[k] = -1;
  for (int o = 0; o < 2; o++) for (size_t i = 0; i < rel[o].size(); i++) relIdx[rel[o][i]] = i;
  int8_t solvedCi[48];
  for (int k = 0; k < 48; k++) solvedCi[k] = cidx[groupOf(k)];
  uint64_t Mo[2] = {1, 1};
  for (int o = 0; o < 2; o++) {
    for (int k : rel[o]) cnt[o][solvedCi[k]]++;
    // multinomial: arrangements of the orbit's colours on its ranked positions
    uint64_t m = 1;
    int n = 0;
    for (int c = 0; c < NC; c++)
      for (int j = 1; j <= cnt[o][c]; j++) { n++; m = m * n / j; }
    Mo[o] = m;
  }
  const uint64_t total = Mo[0] * Mo[1];
  if (total > (1ULL << 32)) { fprintf(stderr, "stage too large\n"); return 1; }
  // rank of a colouring (colour indices at the ranked positions): per orbit, colour by colour,
  // the colex rank of that colour's positions among the positions the earlier colours left free
  static uint64_t binom[49][49];
  for (int a = 0; a <= 48; a++) { binom[a][0] = 1; for (int b = 1; b <= a; b++) binom[a][b] = binom[a - 1][b - 1] + (b < a ? binom[a - 1][b] : 0); }
  static uint8_t CX[256][256];  // CX[mask][x]: the bits of x under mask, packed
  for (int m = 0; m < 256; m++)
    for (int x = 0; x < 256; x++) {
      int r = 0, j = 0;
      for (int b = 0; b < 8; b++) if (m >> b & 1) { if (x >> b & 1) r |= 1 << j; j++; }
      CX[m][x] = r;
    }
  static uint32_t BT[3][256][25];  // colex part of one byte, given j set bits below it
  for (int bp = 0; bp < 3; bp++)
    for (int v = 0; v < 256; v++)
      for (int j = 0; j < 25; j++) {
        uint64_t s = 0;
        int t = j;
        for (int b = 0; b < 8; b++) if (v >> b & 1) { t++; s += binom[8 * bp + b][t]; }
        BT[bp][v][j] = s;
      }
  struct OrbitRank { int n; int colours[13]; int nc; int cnt[13]; };
  OrbitRank orr[2];
  for (int o = 0; o < 2; o++) {
    orr[o].n = rel[o].size();
    orr[o].nc = 0;
    for (int c = 1; c < NC; c++) if (cnt[o][c]) orr[o].colours[orr[o].nc] = c, orr[o].cnt[orr[o].nc++] = cnt[o][c];
    if (orr[o].n > 24) { fprintf(stderr, "orbit too large\n"); return 1; }
  }
  // a colouring as masks: mk[o][ii] holds the ranked positions (indices into rel[o]) of colour
  // orr[o].colours[ii]
  typedef uint32_t Masks[2][13];
  auto masksOf = [&](const int8_t *ci, Masks &mk) {
    for (int o = 0; o < 2; o++) {
      const OrbitRank &R = orr[o];
      uint32_t m[13] = {};
      for (int i = 0; i < R.n; i++) m[ci[rel[o][i]]] |= 1u << i;
      for (int ii = 0; ii < R.nc; ii++) mk[o][ii] = m[R.colours[ii]];
    }
  };
  auto maskRank = [&](const Masks &mk) {
    uint64_t r = 0;
    for (int o = 0; o < 2; o++) {
      const OrbitRank &R = orr[o];
      if (!R.n) continue;
      uint32_t fr = (1u << R.n) - 1;
      int nf = R.n;
      uint64_t ro = 0;
      const int nb = (R.n + 7) / 8;
      for (int ii = 0; ii < R.nc; ii++) {
        if (nf == R.cnt[ii]) break;  // the last colour takes every free position left: digit 0, radix 1
        const uint32_t x = mk[o][ii];
        uint32_t cx = 0;
        if (ii == 0) cx = x;  // nothing removed yet
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
  };
  auto rankOf = [&](const int8_t *ci) {
    Masks mk;
    masksOf(ci, mk);
    return maskRank(mk);
  };
  auto unrank = [&](uint64_t r, int8_t *ci) {
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
        // colex unrank: the largest p with C(p, t) <= rest, for t = cnt down to 1
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
  };
  // the 48 cube symmetries (as certimprove)
  struct Sym { int fac[48]; int gmap[12]; int8_t cmap[13]; int face[6]; bool flip; bool keepsStage; };
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
        const int det = par[pi] * ((sg & 1) ? -1 : 1) * ((sg & 2) ? -1 : 1) * ((sg & 4) ? -1 : 1);
        y.flip = det < 0;
        int s2 = 0, t2 = 0;
        for (int g = 0; g < 12; g++) { if (S >> g & 1) s2 |= 1 << y.gmap[g]; if (TG >> g & 1) t2 |= 1 << y.gmap[g]; }
        y.keepsStage = s2 == S && t2 == TG;
        y.cmap[0] = 0;
        for (size_t i = 0; i < tg.size(); i++) y.cmap[i + 1] = cidx[y.gmap[tg[i]]];
        syms.push_back(y);
      }
  }
  std::vector<int> ss;  // the stage symmetries
  for (int s = 0; s < 48; s++) if (syms[s].keepsStage) ss.push_back(s);
  // the image of an algorithm under h o y (h a stage symmetry) is the stage-symmetric copy of its
  // image under y, which finds the same shortenings (every improvement is stored for all of a
  // class's stage symmetries): one y per coset {h o y}
  std::vector<int> cosetReps;
  {
    std::vector<char> done(48, 0);
    for (int y = 0; y < 48; y++) {
      if (done[y]) continue;
      cosetReps.push_back(y);
      for (int h : ss)
        for (int z = 0; z < 48; z++) {
          bool eq = true;
          for (int k = 0; k < 48 && eq; k++) if (syms[z].fac[k] != syms[h].fac[syms[y].fac[k]]) eq = false;
          if (eq) done[z] = 1;
        }
    }
  }
  auto symMoves = [&](const Sym &y, const std::vector<cc::Mv> &mv) {
    std::vector<cc::Mv> r;
    for (auto &m : mv) r.push_back({y.face[m.face], m.depth, y.flip ? 4 - m.amt : m.amt});
    return r;
  };
  auto symCi = [&](const Sym &y, const int8_t *a, int8_t *b) { for (int k = 0; k < 48; k++) b[y.fac[k]] = y.cmap[a[k]]; };
  // the position a solving sequence solves (run backwards from solved), as colour indices
  auto positionOf = [&](const std::vector<cc::Mv> &mv, int8_t *a) {
    int8_t b[48];
    memcpy(a, solvedCi, 48);
    for (auto it = mv.rbegin(); it != mv.rend(); ++it) {
      for (int k = 0; k < 48; k++) b[mp[it->face][it->depth][4 - it->amt][k]] = a[k];
      memcpy(a, b, 48);
    }
  };
  auto solves = [&](const int8_t *p, const std::vector<cc::Mv> &mv) {
    int8_t a[48], b[48];
    memcpy(a, p, 48);
    for (auto &m : mv) {
      for (int k = 0; k < 48; k++) b[mp[m.face][m.depth][m.amt][k]] = a[k];
      memcpy(a, b, 48);
    }
    for (int o = 0; o < 2; o++) for (int k : rel[o]) if (a[k] != solvedCi[k]) return false;
    return true;
  };

  // the certificate as a dense table
  std::vector<uint8_t> dist(total, 0xFF);
  // src[r]: where position r's line is, (line << 6 | s): certificate line number, or with the top
  // bit an offset into the arena of shortened lines; s the cube symmetry that maps the stored line
  // onto r.  One word, so that a reader never sees half of an update.
  const uint64_t ARENA = 1ULL << 63;
  std::vector<uint64_t> src(total, ~0ULL);
  std::vector<uint64_t> lineOff;
  std::vector<uint8_t> lineBytes;
  {
    for (int w = 0; w <= 48; w++) {
      char fn[32];
      snprintf(fn, sizeof fn, "w%0*d.txt", tg.size() * 4 >= 10 ? 2 : 1, w);  // as certmerge
      FILE *in = fopen((dir + "/" + fn).c_str(), "r");
      if (!in) continue;
      char buf[4096];
      std::vector<cc::Mv> mv;
      while (fgets(buf, sizeof buf, in)) {
        std::string l(buf);
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
        if (!cc::parseMoves(l, mv)) { fprintf(stderr, "bad certificate line in %s\n", fn); return 1; }
        lineOff.push_back(lineBytes.size());
        for (auto &m : mv) lineBytes.push_back(mvByte(m));
      }
      fclose(in);
    }
    lineOff.push_back(lineBytes.size());
    const size_t NL = lineOff.size() - 1;
    std::atomic<size_t> ni{0};
    auto work = [&] {
      std::vector<cc::Mv> mv;
      int8_t a[48], b[48];
      for (size_t i; (i = ni++) < NL;) {
        mv.clear();
        for (uint64_t j = lineOff[i]; j < lineOff[i + 1]; j++) mv.push_back(byteMv(lineBytes[j]));
        positionOf(mv, a);
        for (int s : ss) {
          symCi(syms[s], a, b);
          const uint64_t r = rankOf(b);
          if (mv.size() < dist[r]) dist[r] = mv.size(), src[r] = (uint64_t)i << 6 | s;  // a class's images: one line
        }
      }
    };
    std::vector<std::thread> th;
    for (int t = 0; t < T; t++) th.emplace_back(work);
    for (auto &x : th) x.join();
    long un = 0;
    int W = 0;
    for (uint64_t r = 0; r < total; r++) { if (dist[r] == 0xFF) un++; else W = std::max<int>(W, dist[r]); }
    fprintf(stderr, "certificate: %zu lines, %lu positions (%zu stage symmetries), worst %d; %ld positions not covered (%.0fs)\n",
            NL, (unsigned long)total, ss.size(), W, un, now() - t0);
    if (un) { fprintf(stderr, "the certificate does not cover the stage\n"); return 1; }
  }
  int W = 0;
  for (auto d : dist) W = std::max<int>(W, d);
  std::mutex mu;  // guards the overrides and dist lowering
  // shortened lines, appended (under mu) to fixed blocks that never move; src points into them
  // (was a hash table per class, which doubled to 17 GB on 4h; before that a map per position)
  const uint64_t BLK = 1 << 26;
  std::vector<uint8_t *> arena(1 << 18, nullptr);
  uint64_t arenaEnd = 0;  // offset of the next free arena byte
  auto arenaPut = [&](const std::vector<uint8_t> &by) {  // caller holds mu
    if (arenaEnd % BLK + 1 + by.size() > BLK || !arena[arenaEnd / BLK]) {
      if (arena[arenaEnd / BLK]) arenaEnd = (arenaEnd / BLK + 1) * BLK;
      if (arenaEnd / BLK >= arena.size()) { fprintf(stderr, "arena full\n"); exit(1); }
      arena[arenaEnd / BLK] = new uint8_t[BLK];
    }
    uint8_t *q = arena[arenaEnd / BLK] + arenaEnd % BLK;
    q[0] = by.size();
    memcpy(q + 1, by.data(), by.size());
    const uint64_t off = arenaEnd;
    arenaEnd += 1 + by.size();
    return off;
  };
  // a bit per shortened position, for the output
  std::vector<uint64_t> overBit((dist.size() + 63) / 64, 0);
  auto lineOf = [&](uint64_t r) {
    std::vector<cc::Mv> mv;
    const uint64_t v = std::atomic_ref<uint64_t>(src[r]).load(std::memory_order_acquire);
    if (v & ARENA) {
      const uint64_t off = (v & ~ARENA) >> 6;
      const uint8_t *q = arena[off / BLK] + off % BLK;
      for (int i = 0; i < q[0]; i++) mv.push_back(byteMv(q[1 + i]));
    } else {
      const uint64_t i = v >> 6;
      for (uint64_t j = lineOff[i]; j < lineOff[i + 1]; j++) mv.push_back(byteMv(lineBytes[j]));
    }
    return symMoves(syms[v & 63], mv);
  };
  auto distAt = [&](uint64_t r) { return std::atomic_ref<uint8_t>(dist[r]).load(std::memory_order_relaxed); };
  // low[t]: a bit per position whose line has at most t moves (t <= W - 2), so that most band
  // lookups touch a small bitset instead of the table
  const uint64_t words = (total + 63) / 64;
  std::vector<std::vector<uint64_t>> low(std::max(0, W - 1));
  for (int t = 0; t + 1 < W; t++) {
    low[t].assign(words, 0);
    for (uint64_t r = 0; r < total; r++) if (dist[r] <= t) low[t][r >> 6] |= 1ULL << (r & 63);
  }
  auto lowAt = [&](int t, uint64_t r) { return std::atomic_ref<uint64_t>(low[t][r >> 6]).load(std::memory_order_relaxed) >> (r & 63) & 1; };
  std::vector<std::vector<uint64_t>> bucket(256);  // shortened classes by their new length, to propagate
  std::atomic<long> nImproved{0};
  // p (colour indices) is solved by mv: lower p's class wherever mv is shorter
  // a last move that leaves the goal as it is (an outer face turn; with --slices an inner U or D
  // layer) is wasted: the line without it solves the same position.  A seed line is an algorithm
  // image on its own, and an inverse image ends with whatever its algorithm started with.
  auto endFree = [&](const cc::Mv &m) {
    if (!(m.depth == 1 || (slices && (m.face == 0 || m.face == 5) && (m.depth == 2 || m.depth == 3)))) return false;
    int8_t b[48];
    for (int k = 0; k < 48; k++) b[mp[m.face][m.depth][m.amt][k]] = solvedCi[k];
    for (int o = 0; o < 2; o++) for (int k : rel[o]) if (b[k] != solvedCi[k]) return false;
    return true;
  };
  auto improve = [&](const int8_t *p, std::vector<cc::Mv> mv) {
    simplify(mv);
    while (!mv.empty() && endFree(mv.back())) mv.pop_back();
    const int L = mv.size();
    int8_t b[48];
    // every image of a class has the same line length: skip without the lock when not shorter
    symCi(syms[ss[0]], p, b);
    if (L >= distAt(rankOf(b))) return;
    uint64_t rs[48];
    for (size_t i = 0; i < ss.size(); i++) {
      symCi(syms[ss[i]], p, b);
      rs[i] = rankOf(b);
    }
    std::lock_guard<std::mutex> g(mu);
    if (L >= dist[rs[0]]) return;
    std::vector<uint8_t> by;
    for (auto &m : mv) by.push_back(mvByte(m));
    const uint64_t off = arenaPut(by);  // the line for p; image s of p takes symMoves(syms[s], line)
    for (size_t i = 0; i < ss.size(); i++) {
      const uint64_t r = rs[i];
      if (L >= dist[r]) continue;
      std::atomic_ref<uint64_t>(src[r]).store(ARENA | off << 6 | ss[i], std::memory_order_release);
      std::atomic_ref<uint8_t>(dist[r]).store(L, std::memory_order_relaxed);
      for (int t2 = L; t2 + 1 < W; t2++) std::atomic_ref<uint64_t>(low[t2][r >> 6]).fetch_or(1ULL << (r & 63), std::memory_order_relaxed);
      std::atomic_ref<uint64_t>(overBit[r >> 6]).fetch_or(1ULL << (r & 63), std::memory_order_relaxed);
    }
    bucket[L].push_back(rs[0]), nImproved++;
  };

  // seeds (lines of length <= K) and band targets (the top B lengths), one per class
  struct St { int8_t ci[48]; uint64_t r; Masks mk; uint8_t first; };  // first: seamKey of the line's front, for --merge
  std::vector<St> nearList, band;
  std::vector<size_t> nearCnt;  // seeds of length <= d
  {
    std::vector<cc::Mv> mv;
    for (size_t i = 0; i + 1 < lineOff.size(); i++) {
      const int len = lineOff[i + 1] - lineOff[i];
      if (len > K && len <= W - B) continue;
      mv.clear();
      for (uint64_t j = lineOff[i]; j < lineOff[i + 1]; j++) mv.push_back(byteMv(lineBytes[j]));
      St s;
      positionOf(mv, s.ci);
      s.r = rankOf(s.ci);
      if (dist[s.r] != len) continue;  // a duplicate class line (not the one kept)
      // every stage-symmetric copy (images are generated only up to the stage symmetries)
      std::vector<uint64_t> got;
      for (int h : ss) {
        St c;
        symCi(syms[h], s.ci, c.ci);
        c.first = 255;
        masksOf(c.ci, c.mk);
        c.r = maskRank(c.mk);
        if (std::find(got.begin(), got.end(), c.r) != got.end()) continue;
        got.push_back(c.r);
        if (len <= K) nearList.push_back(c);
        if (len > W - B) band.push_back(c);
      }
    }
    std::stable_sort(nearList.begin(), nearList.end(), [&](const St &a, const St &b) { return dist[a.r] < dist[b.r]; });
    nearCnt.assign(K + 1, 0);
    for (auto &s : nearList) nearCnt[dist[s.r]]++;
    for (int d = 1; d <= K; d++) nearCnt[d] += nearCnt[d - 1];
    fprintf(stderr, "seeds: %zu classes of length <= %d; band: %zu classes of length > %d\n", nearList.size(), K, band.size(), W - B);
  }

  // effects already seen: (fingerprint << 8 | cost), open addressing
  const uint64_t seenMask = (1ULL << seenBits) - 1;
  std::vector<std::atomic<uint64_t>> seen(1ULL << seenBits);
  std::atomic<long> seenCount{0};
  if (!seenFile.empty()) {
    if (FILE *f = fopen(seenFile.c_str(), "rb")) {
      uint64_t x;
      long n = 0;
      while (fread(&x, 8, 1, f) == 1) {
        for (uint64_t h = (x >> 8) & seenMask;; h = (h + 1) & seenMask)
          if (!seen[h].load()) { seen[h] = x; n++; break; }
      }
      fclose(f);
      seenCount = n;
      fprintf(stderr, "seen: %ld effects loaded\n", n);
    }
  }
  // true when (fp, cost) is new or cheaper than what was seen
  auto seenNew = [&](uint64_t fp, int cost) {
    fp = (fp >> 8) | 1;
    const uint64_t want = fp << 8 | cost;
    if ((uint64_t)seenCount.load(std::memory_order_relaxed) * 10 > seenMask * 7) return true;  // full: no skipping
    for (uint64_t h = fp & seenMask;; h = (h + 1) & seenMask) {
      uint64_t cur = seen[h].load(std::memory_order_relaxed);
      for (;;) {
        if (!cur) {
          if (seen[h].compare_exchange_weak(cur, want)) { seenCount++; return true; }
          continue;
        }
        if (cur >> 8 != fp) break;
        if ((int)(cur & 0xFF) <= cost) return false;
        if (seen[h].compare_exchange_weak(cur, want)) return true;
      }
    }
  };

  // images of an algorithm that keep the stage: (destination map, moves)
  struct Img { int8_t D[48]; std::vector<cc::Mv> mv; };
  auto images = [&](const std::vector<cc::Mv> &mv0, std::vector<Img> &out, bool strip) {
    out.clear();
    std::vector<std::vector<cc::Mv>> vars;
    {
      const int n = mv0.size();
      int L = 0, Tt = 0;
      while (L < n && mv0[L].depth == 1) L++;
      while (Tt < n && mv0[n - 1 - Tt].depth == 1) Tt++;
      if (L == n) return;
      for (int i = 0; i <= (strip ? L : 0); i++)
        for (int j = 0; j <= (strip ? Tt : 0); j++) vars.emplace_back(mv0.begin() + i, mv0.end() - j);
    }
    for (auto &v : vars) {
      std::vector<cc::Mv> inv;
      for (auto it = v.rbegin(); it != v.rend(); ++it) inv.push_back({it->face, it->depth, 4 - it->amt});
      int8_t D0[2][48];
      for (int k = 0; k < 48; k++) {
        int x = k;
        for (auto &m : v) x = mp[m.face][m.depth][m.amt][x];
        D0[0][k] = x;
        D0[1][x] = k;
      }
      for (int iv = 0; iv < 2; iv++)
        for (int s : cosetReps) {
          if (keep == 1 && syms[s].face[0] != 0) continue;
          if (keep == 2 && syms[s].face[0] != 0 && syms[s].face[0] != 5) continue;
          Img im;
          im.mv = symMoves(syms[s], iv ? inv : v);
          // the symmetry image's map is the conjugate: D[fac[k]] = fac[D0[k]]
          for (int k = 0; k < 48; k++) im.D[syms[s].fac[k]] = syms[s].fac[D0[iv][k]];
          bool keeps = true;
          for (int k = 0; k < 48 && keeps; k++) if ((S >> groupOf(k) & 1) && groupOf(im.D[k]) != groupOf(k)) keeps = false;
          if (!keeps) continue;
          bool dup = false, id = true;
          for (int o = 0; o < 2; o++) for (int k : rel[o]) if (im.D[k] != k) id = false;
          if (id) continue;
          for (auto &e : out) {
            if (e.mv.size() > im.mv.size()) continue;
            bool same = true;
            for (int o = 0; o < 2 && same; o++) for (int k : rel[o]) if (e.D[k] != im.D[k]) { same = false; break; }
            if (same) { dup = true; break; }
          }
          if (!dup) out.push_back(std::move(im));
        }
    }
  };
  auto fingerprint = [&](const int8_t *D) {
    uint64_t h = 1469598103934665603ULL;
    for (int o = 0; o < 2; o++) for (int k : rel[o]) h = (h ^ (uint8_t)D[k]) * 1099511628211ULL;
    return h * 0x9E3779B97F4A7C15ULL;
  };

  // cheap actions for the propagation: face turns, base pools, new images (cost <= C)
  struct Act { int8_t D[48]; std::vector<cc::Mv> mv; bool fresh = false; };  // fresh: from the streamed pools
  std::vector<Act> acts;
  std::unordered_map<uint64_t, size_t> actIdx;
  std::mutex actMu;
  auto addAct = [&](const Img &im, bool fresh = false) {
    const uint64_t fp = fingerprint(im.D);
    std::lock_guard<std::mutex> g(actMu);
    auto it = actIdx.find(fp);
    if (it != actIdx.end()) {
      if (acts[it->second].mv.size() > im.mv.size()) acts[it->second].mv = im.mv, memcpy(acts[it->second].D, im.D, 48), acts[it->second].fresh |= fresh;
      return;
    }
    actIdx[fp] = acts.size();
    Act a;
    memcpy(a.D, im.D, 48);
    a.mv = im.mv;
    a.fresh = fresh;
    acts.push_back(a);
  };
  for (int f = 0; f < 6; f++)
    for (int a = 1; a <= 3; a++) {
      Img im;
      im.mv = {{f, 1, a}};
      for (int k = 0; k < 48; k++) im.D[k] = mp[f][1][a][k];
      bool id = true;
      for (int o = 0; o < 2; o++) for (int k : rel[o]) if (im.D[k] != k) id = false;
      if (!id) addAct(im);
    }
  if (slices) {
    if (!keep) { fprintf(stderr, "--slices needs --keep\n"); return 1; }
    for (int f : {0, 5})
      for (int d = 2; d <= 3; d++)
        for (int a = 1; a <= 3; a++) {
          Img im;
          im.mv = {{f, d, a}};
          for (int k = 0; k < 48; k++) im.D[k] = mp[f][d][a][k];
          addAct(im, true);
        }
    fprintf(stderr, "slices: %zu actions in all\n", acts.size());
  }

  // stream pools in batches; fn(moves) per algorithm, threaded
  auto stream = [&](const std::string &name, auto fn) -> long {
    FILE *in = name == "-" ? stdin : fopen(name.c_str(), "r");
    if (!in) { fprintf(stderr, "cannot read %s\n", name.c_str()); exit(1); }
    const size_t BS = 1 << 16;
    char buf[4096];
    std::vector<std::vector<cc::Mv>> batch;
    long n = 0;
    bool eof = false;
    while (!eof) {
      batch.clear();
      std::vector<cc::Mv> mv;
      while (batch.size() < BS) {
        if (!fgets(buf, sizeof buf, in)) { eof = true; break; }
        if (poolMoves(buf, mv)) batch.push_back(mv);
      }
      n += batch.size();
      std::atomic<size_t> ni{0};
      std::vector<std::thread> th;
      for (int t = 0; t < T; t++)
        th.emplace_back([&] { for (size_t i; (i = ni++) < batch.size();) fn(batch[i]); });
      for (auto &x : th) x.join();
    }
    if (in != stdin) fclose(in);
    return n;
  };
  for (auto &bf : base) {
    const double tb = now();
    const size_t before = acts.size();
    const long n = stream(bf, [&](const std::vector<cc::Mv> &mv) {
      if ((int)mv.size() > C) return;
      std::vector<Img> ims;
      images(mv, ims, true);
      for (auto &im : ims) if ((int)im.mv.size() <= C) addAct(im);
    });
    fprintf(stderr, "base %s: %ld algorithms, %zu new actions of cost <= %d (%zu in all, %.0fs)\n", bf.c_str(), n, acts.size() - before, C, acts.size(), now() - tb);
  }

  // the image permutes each orbit's ranked positions: a colour's mask is moved by byte tables
  // (PT[o][b][v]: the new mask bits of byte b of an old mask), so a check costs a few lookups
  auto tables = [&](const int8_t *Mp, uint32_t (*PT)[3][256]) {  // new index i takes old index of Mp[rel[o][i]]
    for (int o = 0; o < 2; o++) {
      uint32_t bit[24] = {};
      for (int i = 0; i < orr[o].n; i++) bit[relIdx[Mp[rel[o][i]]]] = 1u << i;
      for (int b = 0; b < 3; b++)
        for (int v = 0; v < 256; v++) {
          uint32_t x = 0;
          for (int t = 0; t < 8; t++) if (v >> t & 1 && 8 * b + t < 24) x |= bit[8 * b + t];
          PT[o][b][v] = x;
        }
    }
  };
  auto permuted = [&](const Masks &a, uint32_t (*PT)[3][256], Masks &out) {
    for (int o = 0; o < 2; o++)
      for (int ii = 0; ii < orr[o].nc; ii++) {
        const uint32_t m = a[o][ii];
        out[o][ii] = PT[o][0][m & 255] | PT[o][1][m >> 8 & 255] | PT[o][2][m >> 16 & 255];
      }
  };
  // --miss: near-miss positions of the band and the bound each needs
  std::mutex missMu;
  std::unordered_map<uint64_t, uint8_t> missQ;

  // seamKey of each seed's line front, for --merge
  if (MG > 0) for (auto &s : nearList) s.first = seamKey(lineOf(s.r), true);
  // --deepband C1: for every band position p and action b of at most C1 moves, y = b(p) must be
  // solved in at most line(p) - 1 - |b| moves; those y join the near misses (the second pass)
  if (DB > 0) {
    long na = 0;
    for (auto &a : acts) {
      if ((int)a.mv.size() > DB) continue;
      na++;
      int8_t Dinv[48];
      for (int k = 0; k < 48; k++) Dinv[a.D[k]] = k;
      uint32_t PT[2][3][256];
      tables(Dinv, PT);
      for (auto &t : band) {
        Masks qm;
        permuted(t.mk, PT, qm);
        const uint64_t r = maskRank(qm);
        const int need = distAt(t.r) - 1 - (int)a.mv.size();
        if (need < 0 || distAt(r) <= need) continue;
        auto it = missQ.find(r);
        if (it == missQ.end() || it->second > need) missQ[r] = need;
      }
    }
    fprintf(stderr, "deepband: %ld actions of at most %d moves, %zu positions to shorten\n", na, DB, missQ.size());
  }
  std::atomic<long> nMerge{0};
  // the new algorithms: seed and band
  std::atomic<long> nImg{0}, nNew{0}, nSeed{0}, nBand{0};
  for (size_t pi = 1; pi < pos.size(); pi++) {
    const double tp = now();
    const long n = stream(pos[pi], [&](const std::vector<cc::Mv> &mv) {
      std::vector<Img> ims;
      images(mv, ims, true);
      int8_t q[48], Dinv[48];
      for (auto &im : ims) {
        nImg++;
        const int la = im.mv.size();
        if (!seenNew(fingerprint(im.D), la)) continue;
        const uint8_t lastL = seamKey(im.mv, false);
        nNew++;
        if (la <= C) addAct(im, true);
        for (int k = 0; k < 48; k++) Dinv[im.D[k]] = k;
        uint32_t PT[2][3][256];
        Masks qm;
        // seed: p = A^-1(q), p[k] = q[D[k]], solved by A + line(q)
        const int dmax = W - 1 - la;
        const bool useSeeds = !worst || (dmax <= K && (dmax < 0 || nearCnt[dmax] <= band.size()));
        if (useSeeds) tables(im.D, PT);  // q[k] = s.ci[D[k]]
        // both loops rank a chunk first and prefetch the table words, then test (the lookups are
        // random and mostly miss the cache)
        constexpr int CH = 32;
        size_t ci_[CH];
        uint64_t rk[CH];
        if (useSeeds) for (size_t i0 = 0; i0 < nearList.size();) {
          int nc = 0;
          bool stop = false;
          for (; i0 < nearList.size() && nc < CH; i0++) {
            const St &s = nearList[i0];
            if (la + distAt(s.r) - (MG > 0 ? MG : 0) >= W) { stop = true; break; }  // nothing is longer than W (seeds are sorted by length)
            if (la + distAt(s.r) >= W && !seamMeets(s.first, lastL)) continue;  // only a merge could help
            permuted(s.mk, PT, qm);
            rk[nc] = maskRank(qm);
            __builtin_prefetch(&dist[rk[nc]]);
            ci_[nc++] = i0;
          }
          for (int c = 0; c < nc; c++) {
            const St &s = nearList[ci_[c]];
            const int dq = distAt(s.r);
            const int dt = distAt(rk[c]);
            bool mg = false;
            if (la + dq >= dt) {
              if (MG <= 0 || !seamMeets(s.first, lastL) || la + dq - MG >= dt) continue;
              mg = true;
            }
            memcpy(q, solvedCi, 48);
            for (int o = 0; o < 2; o++) for (int k : rel[o]) q[k] = s.ci[im.D[k]];
            auto cand = im.mv;
            auto rest = lineOf(s.r);
            cand.insert(cand.end(), rest.begin(), rest.end());
            if (mg) { simplify(cand); if ((int)cand.size() >= dt) continue; nMerge++; }
            nSeed++;
            improve(q, cand);
          }
          if (stop) break;
        }
        // band: q = A(p), q[D[k]] = p[k], so q[j] = p[Dinv[j]]; p solved by A + line(q)
        // a band improvement A + line(q) with line(q) <= K moves is also a seed candidate
        if (dmax > K || !useSeeds) tables(Dinv, PT);  // q[j] = p[Dinv[j]]
        if (dmax > K || !useSeeds) for (size_t i0 = 0; i0 < band.size();) {
          int nc = 0;
          for (; i0 < band.size() && nc < CH; i0++) {
            const St &t = band[i0];
            const int dp = distAt(t.r);
            if (la + 1 >= dp) continue;
            permuted(t.mk, PT, qm);
            rk[nc] = maskRank(qm);
            const int need = dp - la - 1;
            if (need + 1 < W) __builtin_prefetch(&low[need][rk[nc] >> 6]);
            else __builtin_prefetch(&dist[rk[nc]]);
            ci_[nc++] = i0;
          }
          for (int c = 0; c < nc; c++) {
            const St &t = band[ci_[c]];
            const int dp = distAt(t.r);
            const uint64_t rq = rk[c];
            const int need = dp - la - 1;  // q's line must have at most this many moves
            if (need < 0) continue;
            const int slackT = std::max(M, MG);
            if (need + slackT + 1 < W && !lowAt(need + slackT, rq)) continue;
            if (la + distAt(rq) >= dp && MG > 0 && la + distAt(rq) - MG < dp) {
              auto rest = lineOf(rq);
              if (seamMeets(seamKey(rest, true), lastL)) {
                auto cand = im.mv;
                cand.insert(cand.end(), rest.begin(), rest.end());
                simplify(cand);
                if ((int)cand.size() < dp) { nMerge++; nBand++; improve(t.ci, cand); continue; }
              }
            }
            if (la + distAt(rq) >= dp) {
              if (M > 0 && la + distAt(rq) < dp + M) {
                std::lock_guard<std::mutex> g(missMu);
                auto it = missQ.find(rq);
                if (it == missQ.end() || it->second > need) missQ[rq] = need;
              }
              continue;
            }
            auto cand = im.mv;
            auto rest = lineOf(rq);
            cand.insert(cand.end(), rest.begin(), rest.end());
            nBand++;
            improve(t.ci, cand);
          }
        }
      }
    });
    fprintf(stderr, "%s: %ld algorithms, %ld stage images so far, %ld new effects, %ld seed and %ld band candidates (%ld through merges), %ld classes shortened (%.0fs)\n",
            pos[pi].c_str(), n, nImg.load(), nNew.load(), nSeed.load(), nBand.load(), nMerge.load(), nImproved.load(), now() - tp);
  }

  // --miss: the near-miss positions as the band, each with its bound; the pools streamed again
  if (M > 0 || DB > 0) {
    const double tm = now();
    std::vector<St> qb;
    std::vector<uint8_t> qneed;
    {
      std::unordered_map<uint64_t, uint8_t> all;  // every stage-symmetric copy, with the least bound
      for (auto &kv : missQ) {
        if (distAt(kv.first) <= kv.second) continue;  // already short enough
        int8_t ci[48], c[48];
        unrank(kv.first, ci);
        for (int h : ss) {
          symCi(syms[h], ci, c);
          const uint64_t r = rankOf(c);
          auto it = all.find(r);
          if (it == all.end() || it->second > kv.second) all[r] = kv.second;
        }
      }
      for (auto &kv : all) {
        St s;
        unrank(kv.first, s.ci);
        masksOf(s.ci, s.mk);
        s.r = kv.first;
        qb.push_back(s);
        qneed.push_back(kv.second);
      }
    }
    fprintf(stderr, "near misses: %zu positions (%zu with copies) to shorten to their bounds\n", missQ.size(), qb.size());
    missQ.clear();
    for (auto &x : seen) x.store(0, std::memory_order_relaxed);
    seenCount = 0;
    std::atomic<long> nMiss{0};
    const long before = nImproved.load();
    // one first block A (effect D, moves mv) against every near miss
    auto tryBlock = [&](const int8_t *D, const std::vector<cc::Mv> &amv) {
          int8_t Dinv[48];
          uint32_t PT[2][3][256];
          Masks qm;
          constexpr int CH = 32;
          size_t ci_[CH];
          uint64_t rk[CH];
          const int la = amv.size();
          for (int k = 0; k < 48; k++) Dinv[D[k]] = k;
          tables(Dinv, PT);
          for (size_t i0 = 0; i0 < qb.size();) {
            int nc = 0;
            for (; i0 < qb.size() && nc < CH; i0++) {
              if (la > qneed[i0]) continue;
              permuted(qb[i0].mk, PT, qm);
              rk[nc] = maskRank(qm);
              __builtin_prefetch(&low[std::min(qneed[i0] - la, W - 2)][rk[nc] >> 6]);
              ci_[nc++] = i0;
            }
            for (int c = 0; c < nc; c++) {
              const size_t i = ci_[c];
              const int need = qneed[i] - la;  // line(A(q)) must have at most this many moves
              const uint64_t rq = rk[c];
              if (need + 1 < W && !lowAt(need, rq)) continue;
              if (la + distAt(rq) > qneed[i] || la + distAt(rq) >= distAt(qb[i].r)) continue;
              auto cand = amv;
              auto rest = lineOf(rq);
              cand.insert(cand.end(), rest.begin(), rest.end());
              nMiss++;
              improve(qb[i].ci, cand);
            }
          }
    };
    if (missActs && !qb.empty()) {
      const size_t na = acts.size();
      fprintf(stderr, "near misses: %zu actions as the first block\n", na);
      std::atomic<size_t> ni{0};
      std::vector<std::thread> th;
      for (int t = 0; t < T; t++)
        th.emplace_back([&] {
          for (size_t i; (i = ni++) < na;) {
            int8_t D[48];
            std::vector<cc::Mv> amv;
            {
              std::lock_guard<std::mutex> g(actMu);
              memcpy(D, acts[i].D, 48);
              amv = acts[i].mv;
            }
            tryBlock(D, amv);
          }
        });
      for (auto &x : th) x.join();
    }
    for (size_t pi = 1; pi < pos.size() && !qb.empty() && !missActs; pi++) {
      stream(pos[pi], [&](const std::vector<cc::Mv> &mv) {
        std::vector<Img> ims;
        images(mv, ims, true);
        for (auto &im : ims) {
          if (!seenNew(fingerprint(im.D), im.mv.size())) continue;
          tryBlock(im.D, im.mv);
        }
      });
    }
    fprintf(stderr, "near misses: %ld candidates, %ld classes shortened (%.0fs)\n", nMiss.load(), nImproved.load() - before, now() - tm);
  }

  // propagate, cheapest first: x = b^-1(y), x[k] = y[D[k]], solved by b + line(y)
  {
    const double tq = now();
    fprintf(stderr, "propagating with %zu actions of cost <= %d\n", acts.size(), C);
    if (propAll) {  // every class (its smallest-rank image) through the fresh actions only: the old
                    // table is already closed under the face turns and base pools
      std::vector<const Act *> fr;
      for (auto &a : acts) if (a.fresh) fr.push_back(&a);
      std::atomic<long> nc{0};
      std::vector<std::thread> th;
      for (int t = 0; t < T; t++)
        th.emplace_back([&, t] {
          int8_t y[48], b[48], x[48];
          for (uint64_t r = t; r < total; r += T) {
            unrank(r, y);
            bool canon = true;
            for (int s : ss) { symCi(syms[s], y, b); if (rankOf(b) < r) { canon = false; break; } }
            if (!canon) continue;
            nc++;
            const int d = distAt(r);
            std::vector<cc::Mv> ly;
            bool have = false;
            for (auto *a : fr) {
              const int la = a->mv.size();
              memcpy(x, solvedCi, 48);
              for (int o = 0; o < 2; o++) for (int k : rel[o]) x[k] = y[a->D[k]];
              if (d + la - 2 >= distAt(rankOf(x))) continue;
              if (!have) ly = lineOf(r), have = true;
              auto cand = a->mv;
              cand.insert(cand.end(), ly.begin(), ly.end());
              improve(x, cand);
            }
          }
        });
      for (auto &x : th) x.join();
      fprintf(stderr, "propall: %ld classes through %zu fresh actions, %ld shortened in all (%.0fs)\n", nc.load(), fr.size(), nImproved.load(), now() - tq);
    }
    if (propTop > 0) {  // every top class x through every cheap action: y[D[k]] = x[k]
      std::vector<uint64_t> top;
      {
        int8_t y[48], b[48];
        for (uint64_t r = 0; r < total; r++) {
          if (distAt(r) < propTop) continue;
          unrank(r, y);
          bool canon = true;
          for (int s : ss) { symCi(syms[s], y, b); if (rankOf(b) < r) { canon = false; break; } }
          if (canon) top.push_back(r);
        }
      }
      std::vector<const Act *> a2;
      if (propTop2 > 0) for (auto &a : acts) if ((int)a.mv.size() <= propTop2) a2.push_back(&a);
      if (propTop2 > 0) fprintf(stderr, "proptop2: %zu classes x %zu actions x %zu actions of cost <= %d\n", top.size(), acts.size(), a2.size(), propTop2);
      const double tt = now();
      std::atomic<size_t> ni{0};
      std::vector<std::thread> th;
      for (int t = 0; t < T; t++)
        th.emplace_back([&] {
          int8_t x[48], y[48];
          // with --proptop2 a work item is one (class, action) pair, so a few classes still use every thread
          const size_t NA = propTop2 > 0 ? acts.size() : 1;
          for (size_t i; (i = ni++) < top.size() * NA;) {
            const uint64_t rx = top[i / NA];
            unrank(rx, x);
            for (size_t ai = propTop2 > 0 ? i % NA : 0; ai < (propTop2 > 0 ? i % NA + 1 : acts.size()); ai++) {
              const Act &a = acts[ai];
              const int dx = distAt(rx);  // lowered as we go
              const int la = a.mv.size();
              memcpy(y, solvedCi, 48);
              for (int o = 0; o < 2; o++) for (int k : rel[o]) y[a.D[k]] = x[k];
              const uint64_t ry = rankOf(y);
              if (propTop2 > 0) {
                int8_t z[48];
                for (auto *b : a2) {
                  const int lb = b->mv.size();
                  if (la + lb >= distAt(rx)) continue;
                  memcpy(z, solvedCi, 48);
                  for (int o = 0; o < 2; o++) for (int k : rel[o]) z[b->D[k]] = y[k];
                  const uint64_t rz = rankOf(z);
                  if (distAt(rz) + la + lb - 4 >= distAt(rx)) continue;
                  auto cand = a.mv;
                  cand.insert(cand.end(), b->mv.begin(), b->mv.end());
                  auto lz = lineOf(rz);
                  cand.insert(cand.end(), lz.begin(), lz.end());
                  improve(x, cand);
                }
              }
              if (distAt(ry) + la - 2 >= dx) continue;
              auto cand = a.mv;
              auto ly = lineOf(ry);
              cand.insert(cand.end(), ly.begin(), ly.end());
              improve(x, cand);
            }
          }
        });
      for (auto &x : th) x.join();
      fprintf(stderr, "proptop: %zu classes of length >= %d through %zu actions, %ld shortened in all (%.0fs)\n", top.size(), propTop, acts.size(), nImproved.load(), now() - tt);
    }
    long expanded = 0;
    const long seedImproved = nImproved.load();
    for (;;) {
      int d = 0;
      {
        std::lock_guard<std::mutex> g(mu);
        while (d < 256 && bucket[d].empty()) d++;
      }
      if (d == 256) break;
      std::vector<uint64_t> cur;
      {
        std::lock_guard<std::mutex> g(mu);
        cur.swap(bucket[d]);
      }
      std::sort(cur.begin(), cur.end());
      cur.erase(std::unique(cur.begin(), cur.end()), cur.end());
      expanded += cur.size();
      std::atomic<size_t> ni{0};
      std::vector<std::thread> th;
      for (int t = 0; t < T; t++)
        th.emplace_back([&] {
          int8_t y[48], x[48];
          for (size_t i; (i = ni++) < cur.size();) {
            const uint64_t ry = cur[i];
            if (distAt(ry) != d) continue;  // lowered again since
            unrank(ry, y);
            if (rankOf(y) != ry) { fprintf(stderr, "unrank mismatch\n"); exit(1); }
            std::vector<cc::Mv> ly;
            bool have = false;
            for (auto &a : acts) {
              const int la = a.mv.size();
              memcpy(x, solvedCi, 48);
              for (int o = 0; o < 2; o++) for (int k : rel[o]) x[k] = y[a.D[k]];
              if (d + la - 2 >= distAt(rankOf(x))) continue;  // a merge saves at most 2
              if (!have) ly = lineOf(ry), have = true;
              auto cand = a.mv;
              cand.insert(cand.end(), ly.begin(), ly.end());
              improve(x, cand);
            }
          }
        });
      for (auto &x : th) x.join();
      fprintf(stderr, "  length %d: %zu classes expanded, %ld shortened in all (%.0fs)\n", d, cur.size(), nImproved.load(), now() - tq);
    }
    fprintf(stderr, "propagation: %ld classes expanded, %ld more shortened (%.0fs)\n", expanded, nImproved.load() - seedImproved, now() - tq);
  }

  // output: one line per shortened class (at its least-rank image), replayed
  long out = 0, bad = 0;
  std::vector<long> hist(256);
  {
    std::atomic<uint64_t> nw{0};
    std::mutex om;
    auto work = [&] {
      int8_t p[48], b[48];
      std::string buf;
      long o = 0, bd = 0;
      for (uint64_t w; (w = nw.fetch_add(1024)) < overBit.size();)
        for (uint64_t x = w; x < std::min<uint64_t>(w + 1024, overBit.size()); x++)
          for (uint64_t bits = overBit[x]; bits; bits &= bits - 1) {
            const uint64_t r = x * 64 + __builtin_ctzll(bits);
            unrank(r, p);
            bool least = true;
            for (int s : ss) { symCi(syms[s], p, b); if (rankOf(b) < r) { least = false; break; } }
            if (!least) continue;
            auto mv = lineOf(r);
            if (!solves(p, mv)) { bd++; continue; }
            buf += text(mv);
            buf += '\n';
            o++;
            if (buf.size() > (1 << 20)) { std::lock_guard<std::mutex> g(om); fwrite(buf.data(), 1, buf.size(), stdout); buf.clear(); }
          }
      std::lock_guard<std::mutex> g(om);
      fwrite(buf.data(), 1, buf.size(), stdout);
      out += o, bad += bd;
    };
    std::vector<std::thread> th;
    for (int t = 0; t < T; t++) th.emplace_back(work);
    for (auto &x : th) x.join();
    for (auto d : dist) hist[d]++;
  }
  fprintf(stderr, "%ld classes shortened (%ld failed replays); positions by length now:", out, bad);
  for (int i = 0; i < 256; i++) if (hist[i] && i >= W - 4) fprintf(stderr, " %d:%ld", i, hist[i]);
  fprintf(stderr, " (%.0fs)\n", now() - t0);
  if (!seenFile.empty()) {
    FILE *f = fopen(seenFile.c_str(), "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", seenFile.c_str()); return 1; }
    for (auto &x : seen) { const uint64_t v = x.load(); if (v) fwrite(&v, 8, 1, f); }
    fclose(f);
    fprintf(stderr, "seen: %ld effects written\n", seenCount.load());
  }
  return bad ? 1 : 0;
}
