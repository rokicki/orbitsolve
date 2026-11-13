// deepmerge: join pair algorithms whose ends cancel deeply.  For lines F and B (B any of the 96
// images of a line: 48 cube symmetries x inverse), if the last K moves of F are the inverse of
// the first K moves of B, then F B = F' B' with F = F' S, B = S^-1 B'; the joined line is
// simplified (turns of one axis commute and combine) and printed when it is at most --maxlen
// moves.  Turns of one axis commute, so a line has one K-move prefix (suffix) for every subset of
// the same-axis run the K-th move falls in; all of them are keyed.  F runs over the lines and
// their inverses; certextend applies all 96 images of each printed line, so this covers every
// pair of images up to symmetry.
//   deepmerge [--src N] [--back NB] [--overlap K] [--minlen M] [--maxlen L] [--part P:p] [--count | --stats] LOG...
// --part P:p: only front lines with index % P == p (slices of a big join; the back index is whole).
// --back NB: only lines of at most NB moves go into the back index (default N); the front still
// runs over every line, so a big log's long lines join the short ones without indexing them.
// --stats: only count, by source length, the image prefix keys (entries and distinct keys) and the
// joins they imply (pairs of front and back entries with one key, by n1 + n2), then stop.
// LOG: twsearch log lines (moves, maybe with a leading cost and a trailing D0/D1); only lines of
// at most N moves (default 13) are used.  stdout: "# invocation", then one merged line per line
// (deduplicated), unless --count.  stderr: counts by merged length.
#include "certcube.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

static const int AXIS[6] = {0, 1, 2, 1, 2, 0}, SIDE[6] = {0, 0, 0, 1, 1, 1};  // ULFRBD
typedef std::vector<cc::Mv> Line;
// as certextend: within each run of same-axis turns, turns of the same layer combine, and the
// run is written in a fixed order (face, then depth), so equal traces give equal lines
static void simplify(Line &v) {
  for (bool again = true; again;) {
    again = false;
    Line r;
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
static inline int mcode(const cc::Mv &m) { return 1 + m.face * 9 + (m.depth - 1) * 3 + (m.amt - 1); }  // 1..54
static std::string text(const Line &mv) {
  std::string s;
  for (auto &m : mv) { s += ' '; s += cc::mvText(m); }
  return s;
}
static Line inverse(const Line &v) {
  Line r(v.rbegin(), v.rend());
  for (auto &m : r) m.amt = 4 - m.amt;
  simplify(r);  // restores the order within runs
  return r;
}
// keys of the K-move prefixes of a simplified line: the full runs, then each subset of the run the
// K-th move falls in, written in canonical order (6 bits per move); with the moves left over (the
// rest of that run first, then the remainder of the line), as an index mask over the line
struct Pre { uint64_t key; uint64_t used; };  // used: bit i set if move i is in the prefix (lines <= 64)
static void prefixes(const Line &v, int K, std::vector<Pre> &out) {
  out.clear();
  if ((int)v.size() <= K) return;
  uint64_t key = 0, used = 0;
  int n = 0;
  size_t i = 0;
  while (i < v.size()) {
    size_t j = i;
    while (j < v.size() && AXIS[v[j].face] == AXIS[v[i].face]) j++;
    const int r = j - i, need = K - n;
    if (r <= need) {
      for (size_t t = i; t < j; t++) key = key << 6 | mcode(v[t]), used |= 1ULL << t;
      n += r;
      if (n == K) { out.push_back({key, used}); return; }
      i = j;
      continue;
    }
    for (int s = 0; s < (1 << r); s++) {
      if (__builtin_popcount(s) != need) continue;
      uint64_t k2 = key, u2 = used;
      for (int t = 0; t < r; t++) if (s >> t & 1) k2 = k2 << 6 | mcode(v[i + t]), u2 |= 1ULL << (i + t);
      out.push_back({k2, u2});
    }
    return;
  }
}

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  fflush(stdout);
  int N = 13, K = 5, L = -1, NB = -1;
  bool count = false, stats = false;
  int Mn = 0, PP = 1, Pp = 0;
  std::vector<std::string> logs;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--src" && i + 1 < argc) N = atoi(argv[++i]);
    else if (o == "--overlap" && i + 1 < argc) K = atoi(argv[++i]);
    else if (o == "--back" && i + 1 < argc) NB = atoi(argv[++i]);
    else if (o == "--maxlen" && i + 1 < argc) L = atoi(argv[++i]);
    else if (o == "--minlen" && i + 1 < argc) Mn = atoi(argv[++i]);
    else if (o == "--part" && i + 1 < argc) { if (sscanf(argv[++i], "%d:%d", &PP, &Pp) != 2 || PP < 1 || Pp < 0 || Pp >= PP) { fprintf(stderr, "bad --part\n"); return 1; } }
    else if (o == "--count") count = true;
    else if (o == "--stats") stats = true;
    else if (o.size() > 1 && o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else logs.push_back(o);
  }
  if (logs.empty() || K < 1 || K > 10 || N < 1 || N > 30) {
    fprintf(stderr, "usage: deepmerge [--src N] [--back NB] [--overlap K] [--minlen M] [--maxlen L] [--part P:p] [--count | --stats] LOG...\n");
    return 1;
  }
  if (NB < 0 || NB > N) NB = N;
  if (L < 0) L = N + NB - 2 * K;
  // the 48 cube symmetries on faces (signed axis permutations), flip = orientation-reversing
  struct Sym { int face[6]; bool flip; };
  std::vector<Sym> syms;
  {
    const int pm[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    const int par[6] = {1, -1, -1, 1, 1, -1};
    for (int pi = 0; pi < 6; pi++)
      for (int sg = 0; sg < 8; sg++) {
        Sym y;
        for (int f = 0; f < 6; f++) {
          auto p = cc::Cube::normalOf(f);
          std::array<int, 3> q;
          for (int r = 0; r < 3; r++) q[r] = p[pm[pi][r]] * ((sg >> r & 1) ? -1 : 1);
          for (int f2 = 0; f2 < 6; f2++) if (cc::Cube::normalOf(f2) == q) y.face[f] = f2;
        }
        y.flip = par[pi] * ((sg & 1) ? -1 : 1) * ((sg & 2) ? -1 : 1) * ((sg & 4) ? -1 : 1) < 0;
        syms.push_back(y);
      }
  }
  auto image = [&](const Line &v, int s) {  // s < 48: symmetry s; s >= 48: symmetry s - 48 of the inverse
    Line r;
    const Sym &y = syms[s % 48];
    for (auto &m : v) r.push_back({y.face[m.face], m.depth, y.flip ? 4 - m.amt : m.amt});
    if (s >= 48) r = inverse(r);
    else simplify(r);
    return r;
  };
  // read
  std::vector<Line> lines;
  {
    std::unordered_set<std::string> seen;
    long bad = 0, longer = 0;
    char buf[4096];
    for (auto &f : logs) {
      FILE *fp = fopen(f.c_str(), "r");
      if (!fp) { fprintf(stderr, "cannot open %s\n", f.c_str()); return 1; }
      while (fgets(buf, sizeof buf, fp)) {
        std::string l(buf);
        while (!l.empty() && (l.back() == '\n' || l.back() == ' ' || l.back() == '\r')) l.pop_back();
        if (l.empty() || l[0] == '#') continue;
        if (l.size() >= 3 && l[l.size() - 3] == ' ' && l[l.size() - 2] == 'D') l.resize(l.size() - 3);
        size_t st = 0;
        while (st < l.size() && l[st] == ' ') st++;
        size_t e = st;
        while (e < l.size() && l[e] >= '0' && l[e] <= '9') e++;
        if (e > st && (e == l.size() || l[e] == ' ')) st = e;
        Line mv;
        if (!cc::parseMoves(l.substr(st), mv) || mv.empty()) { bad++; continue; }
        bool ok = true;
        for (auto &m : mv) if (m.depth < 1 || m.depth > 3) ok = false;
        if (!ok) { bad++; continue; }
        if ((int)mv.size() > N) { longer++; continue; }
        simplify(mv);
        if ((int)mv.size() <= K) continue;
        if (seen.insert(text(mv)).second) lines.push_back(mv);
      }
      fclose(fp);
    }
    fprintf(stderr, "%zu lines of %d..%d moves (%ld longer, %ld unparsable)\n", lines.size(), K + 1, N, longer, bad);
  }
  // the back lines (at most NB moves) first: their index goes into a back entry's 25-bit line field
  std::stable_partition(lines.begin(), lines.end(), [&](const Line &v) { return (int)v.size() <= NB; });
  size_t nBack = 0;
  while (nBack < lines.size() && (int)lines[nBack].size() <= NB) nBack++;
  if (nBack >= (1u << 25)) { fprintf(stderr, "too many back lines (at most 2^25)\n"); return 1; }
  fprintf(stderr, "%zu back lines of at most %d moves\n", nBack, NB);
  // back index: every distinct image of every line, by each K-move prefix key
  struct Ent { uint64_t key; uint32_t ref; uint32_t used; };  // ref = line << 7 | image; used: prefix mask (lines <= 32)
  std::vector<Ent> back;
  {
    std::vector<Pre> pre;
    for (size_t i = 0; i < nBack; i++) {
      std::vector<std::string> done;
      for (int s = 0; s < 96; s++) {
        Line im = image(lines[i], s);
        std::string t = text(im);
        if (std::find(done.begin(), done.end(), t) != done.end()) continue;
        done.push_back(t);
        prefixes(im, K, pre);
        for (auto &p : pre) back.push_back({p.key, (uint32_t)(i << 7 | s), (uint32_t)p.used});
      }
    }
    std::sort(back.begin(), back.end(), [](const Ent &a, const Ent &b) { return a.key < b.key; });
    fprintf(stderr, "%zu prefix keys of the images\n", back.size());
  }
  if (stats) {
    // per source length n: lines, entries, distinct keys among that length's entries
    std::vector<long> nl(64, 0), ne(64, 0), nk(64, 0), front(64, 0);
    for (auto &v : lines) nl[v.size()]++;
    long distinct = 0;
    std::vector<std::vector<long>> join(64, std::vector<long>(64, 0));  // join[n1][n2]
    for (size_t a = 0; a < back.size();) {
      size_t b = a;
      while (b < back.size() && back[b].key == back[a].key) b++;
      distinct++;
      long cnt[64] = {0}, fr[64] = {0};
      for (size_t t = a; t < b; t++) {
        const int n = lines[back[t].ref >> 7].size(), s = back[t].ref & 127;
        cnt[n]++;
        if (s == 0 || s == 48) fr[n]++;  // the front's suffix keys are the prefixes of the line's inverse and of the line
      }
      for (int n = 0; n < 64; n++) if (cnt[n]) { ne[n] += cnt[n]; nk[n]++; front[n] += fr[n]; }
      for (int n1 = 0; n1 < 64; n1++) if (fr[n1]) for (int n2 = 0; n2 < 64; n2++) if (cnt[n2]) join[n1][n2] += fr[n1] * cnt[n2];
      a = b;
    }
    fprintf(stderr, "overlap %d: %ld distinct keys in all\n", K, distinct);
    fprintf(stderr, " len    lines  front keys   image keys  distinct keys\n");
    for (int n = 0; n < 64; n++) if (nl[n]) fprintf(stderr, "%4d %8ld %11ld %12ld %14ld\n", n, nl[n], front[n], ne[n], nk[n]);
    std::vector<long> byLen(128, 0);
    long tot = 0;
    for (int n1 = 0; n1 < 64; n1++) for (int n2 = 0; n2 < 64; n2++) if (join[n1][n2]) byLen[n1 + n2 - 2 * K] += join[n1][n2], tot += join[n1][n2];
    fprintf(stderr, "joins (front entry x back entry with one key) by n1 + n2 - 2K: %ld in all\n", tot);
    for (int l = 0; l < 128; l++) if (byLen[l]) fprintf(stderr, " %d:%ld", l, byLen[l]);
    fprintf(stderr, "\n");
    return 0;
  }
  // front: each line and its inverse; the K-move suffixes of F are the prefixes of F^-1, and
  // S^-1 is that prefix of F^-1, so a back entry with the same key cancels it
  std::vector<long> hist(64, 0);
  long matches = 0, printed = 0;
  std::unordered_set<uint64_t> outSeen;
  std::vector<Pre> pre;
  for (size_t i = Pp; i < lines.size(); i += PP)
    for (int o = 0; o < 2; o++) {
      const Line F = o ? inverse(lines[i]) : lines[i];
      const Line Fi = inverse(F);
      prefixes(Fi, K, pre);
      for (auto &p : pre) {
        Line Fp;  // F' = F without S: F'^-1 is Fi without the prefix
        {
          Line rest;
          for (size_t t = 0; t < Fi.size(); t++) if (!(p.used >> t & 1)) rest.push_back(Fi[t]);
          Fp = inverse(rest);
        }
        auto it = std::lower_bound(back.begin(), back.end(), p.key, [](const Ent &a, uint64_t k) { return a.key < k; });
        for (; it != back.end() && it->key == p.key; ++it) {
          if ((int)(F.size() + lines[it->ref >> 7].size()) - 2 * K < Mn) continue;  // cannot reach --minlen
          matches++;
          const Line Bi = image(lines[it->ref >> 7], it->ref & 127);
          Line M = Fp;
          for (size_t t = 0; t < Bi.size(); t++) if (!(it->used >> t & 1)) M.push_back(Bi[t]);
          simplify(M);
          if ((int)M.size() < std::max(Mn, 1) || (int)M.size() > L) continue;
          const std::string t = text(M);
          if (!outSeen.insert(std::hash<std::string>()(t)).second) continue;
          hist[M.size()]++;
          printed++;
          if (!count) printf("%s\n", t.c_str());
        }
      }
    }
  fprintf(stderr, "%ld matches, %ld distinct merged lines of at most %d moves\n", matches, printed, L);
  for (int l = 0; l < 64; l++) if (hist[l]) fprintf(stderr, " %d:%ld", l, hist[l]);
  fprintf(stderr, "\n");
  return 0;
}
