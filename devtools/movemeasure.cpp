// movemeasure (2026-11-13): how much of a stage line's slice work is undone later in the same line?
// usage: movemeasure N A B STAGEDIR... [--max K] [--seed S]
//   For each line (sampled from the stage dirs' *.txt), play it on an N^3 xcube whose orbit records hold
//   slot labels (a[k] = k) with depth 2 = slice A, depth 3 = slice B (as certsolve's play()), then count:
//   s = slice turns; old work = slots written by domove; for a "generic" record (rows[A][j], rows[j][A] with
//   j not A or B; same for B) the slots whose raw content changed (d; facerots are lazy and global, so the
//   net face rotation costs nothing).  Checks that every generic record of A (of B) ends identical.
//   Wing orbits: face turns cycle every wing orbit; generic wing orbits' changed slots counted too.
#include "certcube.h"
#include "xcube.h"
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <random>
#include <string>
#include <vector>
int alwaysinmem = 1;
ll cancellations = 0;

int main(int argc, char **argv) {
  printf("# movemeasure");
  for (int i = 1; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  std::vector<std::string> dirs;
  long maxLines = 20000;
  unsigned seed = 1;
  std::vector<std::string> pos;
  for (int i = 1; i < argc; i++) {
    std::string s = argv[i];
    if (s == "--max" && i + 1 < argc) maxLines = atol(argv[++i]);
    else if (s == "--seed" && i + 1 < argc) seed = atoi(argv[++i]);
    else if (s.size() > 1 && s[0] == '-') { fprintf(stderr, "unknown option %s\n", s.c_str()); return 1; }
    else pos.push_back(s);
  }
  if (pos.size() < 4) { fprintf(stderr, "usage: movemeasure N A B STAGEDIR... [--max K] [--seed S]\n"); return 1; }
  const int N = atoi(pos[0].c_str()), A = atoi(pos[1].c_str()), B = atoi(pos[2].c_str());
  const int My = (N - 2) / 2, Mx = N - 2 - My;
  if (A < 1 || B < 1 || A > My || B > My || A == B) { fprintf(stderr, "need 1 <= A != B <= %d\n", My); return 1; }
  const int ra = A - 1, rb = B - 1;  // record indices (domove: dep = mv.dep - 2)
  printf("N %d (Mx %d, My %d), slices A %d B %d, seed %u\n", N, Mx, My, A, B, seed);
  for (size_t di = 3; di < pos.size(); di++) {
    std::vector<std::string> lines;
    DIR *d = opendir(pos[di].c_str());
    if (!d) { fprintf(stderr, "cannot open %s\n", pos[di].c_str()); return 1; }
    while (dirent *e = readdir(d)) {
      std::string f = e->d_name;
      if (f.size() < 5 || f.substr(f.size() - 4) != ".txt") continue;
      FILE *fp = fopen((pos[di] + "/" + f).c_str(), "r");
      char buf[4096];
      while (fgets(buf, sizeof buf, fp)) {
        std::string l = buf;
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
        if (!l.empty()) lines.push_back(l);
      }
      fclose(fp);
    }
    closedir(d);
    std::mt19937_64 rng(seed);
    std::shuffle(lines.begin(), lines.end(), rng);
    if ((long)lines.size() > maxLines) lines.resize(maxLines);
    xcube c(Mx, My, 'U');
    double sumS = 0, sumMoves = 0, oldSlots = 0, oldWing = 0, newA = 0, newB = 0, newW = 0, oldGenA = 0, oldGenB = 0;
    long hist[49] = {}, nz = 0, mismatch = 0, nl = 0;
    for (auto &l : lines) {
      std::vector<cc::Mv> mvs;
      if (!cc::parseMoves(l, mvs)) { fprintf(stderr, "bad line: %s\n", l.c_str()); return 1; }
      for (auto &row : c.rows)
        for (auto &xo : row)
          for (int k = 0; k < 24; k++) xo.a[k] = k;
      for (auto &w : c.wingorbits)
        for (int k = 0; k < 24; k++) w.a[k] = k;
      for (int f = 0; f < 6; f++) c.facerots[f] = 0;
      int s = 0, sa = 0, sb = 0;
      for (auto &m : mvs) {
        mv x{m.depth == 1 ? 1 : m.depth == 2 ? A + 1 : B + 1, (short)m.face, (short)m.amt};
        if (m.depth == 1) oldWing += 2.0 * 4 * m.amt * My;  // wings_cycle x2 per quarter turn, every wing orbit
        else {
          s++, (m.depth == 2 ? sa : sb)++;
          oldSlots += 4.0 * (Mx + My);  // rot4hor + rot4ver (or rot2: two swaps), 4 slots per record
        }
        domove(x, c, true);
      }
      // generic records of slice r: rows[r][j] and rows[j][r], j != ra, rb (and j < My for rows[j][r])
      auto changed = [&](int r, int j) {
        int n = 0;
        for (int k = 0; k < 24; k++) n += c.rows[r][j].a[k] != k;
        for (int k = 0; k < 24; k++) n += c.rows[j][r].a[k] != k;
        return n;
      };
      int j0 = 0;
      while (j0 == ra || j0 == rb) j0++;
      const int dA = changed(ra, j0), dB = changed(rb, j0);
      for (int j = j0 + 1; j < My; j++) {
        if (j == ra || j == rb) continue;
        for (int r : {ra, rb})
          if (memcmp(c.rows[r][j].a, c.rows[r][j0].a, 24) || memcmp(c.rows[j][r].a, c.rows[j0][r].a, 24)) mismatch++;
      }
      int dW = 0;
      for (int m = 0; m < My; m++)
        if (m != ra && m != rb) {
          int n = 0;
          for (int k = 0; k < 24; k++) n += c.wingorbits[m].a[k] != k;
          dW = n;
          break;
        }
      sumS += s, sumMoves += mvs.size();
      newA += dA, newB += dB, newW += dW;
      oldGenA += 8.0 * sa, oldGenB += 8.0 * sb;  // per generic j: 4 slots in rows[r][j] + 4 in rows[j][r] per slice turn
      hist[dA]++, hist[dB]++;
      nz += (dA == 0) + (dB == 0);
      nl++;
    }
    printf("\n%s: %ld lines, mean %.2f moves, %.2f slice turns\n", pos[di].c_str(), nl, sumMoves / nl, sumS / nl);
    printf("  per generic orbit pair (48 slots), per line: slice A old %.1f slot writes -> changed %.2f; B %.1f -> %.2f\n",
           oldGenA / nl, newA / nl, oldGenB / nl, newB / nl);
    printf("  ratio old/new (A+B): %.2f   lines with d = 0: %.1f%%   generic records not identical: %ld\n",
           (oldGenA + oldGenB) / (newA + newB), 50.0 * nz / nl, mismatch);
    printf("  generic wing orbit: changed %.2f of 24 per line (old: %.1f slot writes per wing orbit)\n", newW / nl,
           oldWing / nl / My);
    printf("  d histogram (A and B, slots of 48):");
    for (int k = 0; k <= 48; k++) if (hist[k]) printf(" %d:%ld", k, hist[k]);
    printf("\n");
  }
}
