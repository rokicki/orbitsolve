// pairlogcheck: replay twsearch identity-solve output (one algorithm per line, moves
// F 2F 3F ... in slice notation) on n x n x n cubes and check that every centre
// orbit other than the oblique pair (2,3)/(3,2) and the diagonals stays
// colour-solved; with --ud also that the pair pieces on U and D stay on U and D (--uonly: on U).
// --keep u (stage 1u) relaxes the other orbits to keeping only their U face set (U pieces stay on
// U), --keep ud (1ud, 2ud) to keeping their U and D face sets.
// Streams: memory stays flat for any log size; a last line without a newline (a copy
// of a growing log) is skipped.  With --emit, stdout is a pool file ("LEN MOVES D0|D1",
// D1 if the diagonals move) of the lines that pass, in input order; reports go to stderr.
// With --new FILE, FILE holds the (sorted 64-bit) hashes of every line already processed: those
// lines are skipped before any checking, and FILE is rewritten with the new lines added, so a
// grown log is processed again in time proportional to what is new.
// usage: pairlogcheck [--keep u|ud] [--ud] [--uonly] [--bonly] [--emit] [--new FILE] [--threads T] [--show K] NMIN NMAX < LOG
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <cstdint>

static const char *FACES = "ULFRBD";
// outward normals of U L F R B D in (x right, y up, z front)
static const int NORM[6][3] = {{0, 1, 0}, {-1, 0, 0}, {0, 0, 1}, {1, 0, 0}, {0, 0, -1}, {0, -1, 0}};

struct Cube {
  int n, S;
  std::vector<int> px, py, pz, pf;  // doubled centred coordinates, face of each sticker
  std::vector<std::vector<int>> perm;  // [face*n*3 + (layer-1)*3 + amt-1]
  std::vector<int> kind;  // 0 ignore (edge/corner/wing), 1 must stay solved, 2 pair, 3 diagonal
  std::vector<int> orb;  // pair stickers: chiral orbit 0 or 1 (by connectivity under the moves)
  int idx(int f, int a, int b) const { return (f * n + a) * n + b; }
  Cube(int n_) : n(n_), S(6 * n_ * n_) {
    px.resize(S), py.resize(S), pz.resize(S), pf.resize(S), kind.resize(S);
    for (int f = 0; f < 6; f++)
      for (int a = 0; a < n; a++)
        for (int b = 0; b < n; b++) {
          int i = idx(f, a, b), c[3];
          for (int k = 0; k < 3; k++) c[k] = NORM[f][k] * n;  // on the surface
          // the two free axes
          int fr[2], m = 0;
          for (int k = 0; k < 3; k++) if (!NORM[f][k]) fr[m++] = k;
          c[fr[0]] = 2 * a - (n - 1), c[fr[1]] = 2 * b - (n - 1);
          px[i] = c[0], py[i] = c[1], pz[i] = c[2], pf[i] = f;
          int ia = a < n - 1 - a ? a : n - 1 - a, ib = b < n - 1 - b ? b : n - 1 - b;  // depth from edges
          if (ia == 0 || ib == 0) kind[i] = 0;
          else if (ia == ib) kind[i] = 3;
          else if ((ia == 1 && ib == 2) || (ia == 2 && ib == 1)) kind[i] = 2;
          else kind[i] = 1;
        }
    auto find = [&](int x, int y, int z) {
      for (int i = 0; i < S; i++) if (px[i] == x && py[i] == y && pz[i] == z) return i;
      return -1;
    };
    perm.resize(6 * n * 3);
    for (int f = 0; f < 6; f++)
      for (int L = 1; L <= n; L++) {
        // stickers whose coordinate along the normal is in layer L from face f
        int ax = NORM[f][0] ? 0 : NORM[f][1] ? 1 : 2, sg = NORM[f][ax];
        std::vector<int> q1(S);
        for (int i = 0; i < S; i++) {
          int c[3] = {px[i], py[i], pz[i]};
          int coord = c[ax] * sg;  // n on the face itself, else 2*j-(n-1) oriented
          int layer;
          if (coord == n) layer = 1;
          else if (coord == -n) layer = n;
          else layer = (n - 1 - coord) / 2 + 1;
          if (layer != L) { q1[i] = i; continue; }
          // rotate clockwise seen from face f: v' = -(u x v) + u(u.v) with u = normal
          int u[3] = {NORM[f][0], NORM[f][1], NORM[f][2]};
          int cr[3] = {u[1] * c[2] - u[2] * c[1], u[2] * c[0] - u[0] * c[2], u[0] * c[1] - u[1] * c[0]};
          int d = u[0] * c[0] + u[1] * c[1] + u[2] * c[2];
          int v[3];
          for (int k = 0; k < 3; k++) v[k] = -cr[k] + u[k] * d;
          int j = find(v[0], v[1], v[2]);
          if (j < 0) { fprintf(stderr, "geometry error\n"); exit(1); }
          q1[i] = j;
        }
        if (L > 3) continue;
        for (int amt = 1; amt <= 3; amt++) {
          std::vector<int> q(S);
          for (int i = 0; i < S; i++) { int j = i; for (int r = 0; r < amt; r++) j = q1[j]; q[i] = j; }
          perm[(f * 3 + L - 1) * 3 + amt - 1] = q;
        }
      }
    // split the pair stickers into their two chiral orbits
    std::vector<int> uf(S);
    for (int i = 0; i < S; i++) uf[i] = i;
    auto root = [&](int x) { while (uf[x] != x) x = uf[x] = uf[uf[x]]; return x; };
    for (auto &q : perm) if (!q.empty())
      for (int i = 0; i < S; i++) if (kind[i] == 2) uf[root(i)] = root(q[i]);
    orb.assign(S, -1);
    int r0 = -1;
    for (int i = 0; i < S; i++)
      if (kind[i] == 2) {
        if (r0 < 0) r0 = root(i);
        orb[i] = root(i) == r0 ? 0 : 1;
      }
  }
};

struct Mv { int f, L, amt; };
static bool parse(const char *s, std::vector<Mv> &out) {
  out.clear();
  while (*s) {
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    if (!*s) break;
    int L = 1;
    if (*s >= '0' && *s <= '9') L = *s++ - '0';
    const char *p = strchr(FACES, *s);
    if (!*s || !p) return false;
    s++;
    int amt = 1;
    if (*s == '2') amt = 2, s++;
    else if (*s == '\'') amt = 3, s++;
    if (*s && *s != ' ' && *s != '\n' && *s != '\r') return false;
    if (L < 1 || L > 3) return false;
    out.push_back({(int)(p - FACES), L, amt});
  }
  return true;
}

int main(int argc, char **argv) {
  bool ud = false, bonly = false, emit = false, uonly = false;
  FILE *out = stdout;
  int T = 16, show = 10, keep = 0;  // keep: 0 everything, 1 U face set, 2 U and D face sets
  std::string newFile;
  int ai = 1;
  for (; ai < argc && argv[ai][0] == '-'; ai++) {
    std::string o = argv[ai];
    if (o == "--ud") ud = true;
    else if (o == "--bonly") ud = bonly = true;
    else if (o == "--emit") emit = true;
    else if (o == "--uonly") ud = uonly = true;
    else if (o == "--threads" && ai + 1 < argc) T = atoi(argv[++ai]);
    else if (o == "--show" && ai + 1 < argc) show = atoi(argv[++ai]);
    else if (o == "--new" && ai + 1 < argc) newFile = argv[++ai];
    else if (o == "--keep" && ai + 1 < argc) {
      std::string k = argv[++ai];
      if (k == "u") keep = 1;
      else if (k == "ud") keep = 2;
      else { fprintf(stderr, "--keep takes u or ud\n"); return 1; }
    }
    else { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
  }
  if (argc - ai != 2) { fprintf(stderr, "usage: pairlogcheck [--keep u|ud] [--ud] [--uonly] [--bonly] [--emit] [--new FILE] [--threads T] [--show K] NMIN NMAX < LOG\n"); return 1; }
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  if (emit) fflush(stdout);
  int n0 = atoi(argv[ai]), n1 = atoi(argv[ai + 1]);
  std::vector<Cube> cubes;
  for (int n = n0; n <= n1; n++) cubes.emplace_back(n);
  std::atomic<long> keeps0{0}, keeps1{0}, keepsNone{0}, already{0};
  std::vector<uint64_t> oldH, newH;  // --new: hashes of the lines seen before, and of this run's new lines
  if (!newFile.empty())
    if (FILE *f = fopen(newFile.c_str(), "rb")) {
      uint64_t x;
      while (fread(&x, 8, 1, f) == 1) oldH.push_back(x);
      fclose(f);
      if (!std::is_sorted(oldH.begin(), oldH.end())) { fprintf(stderr, "%s: not sorted\n", newFile.c_str()); return 1; }
    }
  auto lineHash = [](const std::string &l) {
    size_t e = l.size();
    while (e && (l[e - 1] == '\n' || l[e - 1] == '\r' || l[e - 1] == ' ')) e--;
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < e; i++) h = (h ^ (uint8_t)l[i]) * 1099511628211ULL;
    return h ^ (h >> 29);
  };
  std::atomic<long> nlines{0}, nonalg{0}, badparse{0}, badoblique{0}, badud{0}, diagmoved{0}, pairident{0};
  long hist[64] = {0}, truncated = 0, emitted = 0;
  std::mutex mu;
  int shown = 0;
  auto report = [&](const char *why, int n, const std::string &l) {
    std::lock_guard<std::mutex> g(mu);
    if (shown++ < show) fprintf(emit ? stderr : stdout, "  %s (n=%d):%s", why, n, l.c_str());
  };
  // streamed in batches: read B lines, check them on T threads, emit in order, repeat
  const size_t B = 1 << 20;
  std::vector<std::string> lines;
  std::vector<int8_t> res;  // per line: -1 rejected/not an algorithm, else 0/1 = moves the diagonals
  std::vector<int> len;
  char buf[8192];
  bool eof = false;
  while (!eof) {
    lines.clear();
    while (lines.size() < B) {
      if (!fgets(buf, sizeof buf, stdin)) { eof = true; break; }
      size_t k = strlen(buf);
      if (k == 0 || buf[k - 1] != '\n') { truncated++; continue; }  // cut off mid-line (a copy of a growing log)
      lines.push_back(buf);
    }
    res.assign(lines.size(), -1);
    len.assign(lines.size(), 0);
    std::vector<uint64_t> lh(lines.size());
    std::vector<std::thread> th;
    for (int t = 0; t < T; t++)
      th.emplace_back([&, t] {
        std::vector<Mv> mv;
        std::vector<uint8_t> a, b;
        long h[64] = {0};
        for (size_t li = t; li < lines.size(); li += T) {
          const std::string &l = lines[li];
          if (!newFile.empty()) {
            lh[li] = lineHash(l);
            if (std::binary_search(oldH.begin(), oldH.end(), lh[li])) { already++; lh[li] = 0; continue; }
          }
          if (l.empty() || l[0] != ' ') { nonalg++; continue; }
          if (!parse(l.c_str(), mv)) { badparse++; report("unparsable", 0, l); continue; }
          if (mv.empty()) { nonalg++; continue; }  // the identity
          nlines++;
          h[mv.size() < 63 ? mv.size() : 63]++;
          bool dm = false, allIdent = true, ok = true;
          for (auto &c : cubes) {
            a.resize(c.S), b.resize(c.S);
            for (int i = 0; i < c.S; i++) a[i] = c.pf[i];
            for (auto &m : mv) {
              const auto &q = c.perm[(m.f * 3 + m.L - 1) * 3 + m.amt - 1];
              for (int i = 0; i < c.S; i++) b[q[i]] = a[i];
              a.swap(b);
            }
            bool okO = true, okU = true, pairId = true, k0 = true, k1 = true;
            for (int i = 0; i < c.S; i++) {
              if (c.kind[i] == 1) {
                if (keep == 0) { if (a[i] != c.pf[i]) okO = false; }
                else if ((c.pf[i] == 0) != (a[i] == 0) || (keep == 2 && (c.pf[i] == 5) != (a[i] == 5))) okO = false;
              }
              if (c.kind[i] == 3 && a[i] != c.pf[i]) dm = true;
              if (c.kind[i] == 2) {
                if (a[i] != c.pf[i]) { pairId = false; (c.orb[i] ? k1 : k0) = false; }
                bool udPos = c.pf[i] == 0 || c.pf[i] == 5, udCol = a[i] == 0 || a[i] == 5;
                if (uonly) { if ((c.pf[i] == 0) != (a[i] == 0)) okU = false; }
                else {
                  if (udPos && a[i] != c.pf[i]) okU = false;
                  if (!udPos && udCol) okU = false;
                }
              }
            }
            if (!pairId) allIdent = false;
            if (bonly && &c == &cubes[0]) (k0 ? keeps0 : k1 ? keeps1 : keepsNone)++;
            if (bonly && !k0 && !k1) { ok = false; report("moves both pair orbits", c.n, l); break; }
            if (!okO) { ok = false; badoblique++; report("moves another oblique", c.n, l); break; }
            if (ud && !okU) { ok = false; badud++; report("breaks U/D", c.n, l); break; }
          }
          if (dm) diagmoved++;
          if (allIdent) pairident++;
          if (ok) res[li] = dm, len[li] = mv.size();
        }
        std::lock_guard<std::mutex> g(mu);
        for (int k = 0; k < 64; k++) hist[k] += h[k];
      });
    for (auto &x : th) x.join();
    if (!newFile.empty()) for (auto h : lh) if (h) newH.push_back(h);
    if (emit)
      for (size_t li = 0; li < lines.size(); li++) {
        if (res[li] < 0) continue;
        std::string &l = lines[li];
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r' || l.back() == ' ')) l.pop_back();
        fprintf(out, "%d%s D%d\n", len[li], l.c_str(), res[li]);
        emitted++;
      }
  }
  FILE *rep = emit ? stderr : stdout;
  fprintf(rep, "algorithms %ld; other lines %ld; unparsable %ld; move another oblique %ld; break U/D %ld; move diagonals %ld; pair colours unchanged %ld\n",
         nlines.load(), nonalg.load(), badparse.load(), badoblique.load(), badud.load(), diagmoved.load(), pairident.load());
  if (bonly) fprintf(rep, "orbit 0 colour-solved %ld; orbit 1 colour-solved %ld; neither %ld\n", keeps0.load(), keeps1.load(), keepsNone.load());
  fprintf(rep, "lengths:");
  for (int k = 0; k < 64; k++) if (hist[k]) fprintf(rep, " %d:%ld", k, hist[k]);
  fprintf(rep, "\n");
  if (truncated) fprintf(rep, "skipped %ld line(s) without a newline (cut off)\n", truncated);
  if (emit) fprintf(rep, "emitted %ld pool lines\n", emitted);
  if (!newFile.empty()) {
    std::sort(newH.begin(), newH.end());
    std::vector<uint64_t> all(oldH.size() + newH.size());
    std::merge(oldH.begin(), oldH.end(), newH.begin(), newH.end(), all.begin());
    all.erase(std::unique(all.begin(), all.end()), all.end());
    const std::string tmp = newFile + ".tmp";
    FILE *f = fopen(tmp.c_str(), "wb");
    if (!f || fwrite(all.data(), 8, all.size(), f) != all.size() || fclose(f)) { fprintf(stderr, "cannot write %s\n", tmp.c_str()); return 1; }
    if (rename(tmp.c_str(), newFile.c_str())) { fprintf(stderr, "cannot rename %s\n", tmp.c_str()); return 1; }
    fprintf(rep, "--new: %ld lines seen before (skipped), %zu new; %zu lines recorded\n", already.load(), newH.size(), all.size());
  }
  return 0;
}
