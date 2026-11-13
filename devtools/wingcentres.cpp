// wingcentres: keep the wing algorithms of a class pool that also keep every centre facelet
// colour-solved, for a wing orbit at any depth on any cube size (checked on N = MINN..MAXN,
// every wing depth 2..N/2), and check that each kept algorithm changes nothing outside its
// wing orbit and the centres (corners, middle edges, other wing orbits exactly in place).
// Placeholder depth 2 in the pool is the wing orbit's depth.  Writes the kept classes as a
// class file on stdout (the input header line is replaced); a summary goes to stderr.
//
//   wingcentres POOL.cls [--minn N] [--maxn N] > OUT.cls
#include "certcube.h"
#include <cstring>
#include <map>

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  int minN = 6, maxN = 12;
  std::vector<std::string> pos;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--minn" && i + 1 < argc) minN = atoi(argv[++i]);
    else if (o == "--maxn" && i + 1 < argc) maxN = atoi(argv[++i]);
    else if (o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else pos.push_back(o);
  }
  if (pos.size() != 1) { fprintf(stderr, "usage: wingcentres POOL.cls [--minn N] [--maxn N] > OUT.cls\n"); return 1; }
  if (minN < 4) { fprintf(stderr, "--minn must be at least 4\n"); return 1; }
  FILE *f = fopen(pos[0].c_str(), "r");
  if (!f) { fprintf(stderr, "cannot open the pool\n"); return 1; }
  std::vector<cc::Cube> cubes;
  for (int N = minN; N <= maxN; N++) cubes.emplace_back(N);
  char buf[4096];
  long total = 0, kept = 0, otherBad = 0;
  std::map<int, long> keptByCost, allByCost;
  while (fgets(buf, sizeof buf, f)) {
    if (buf[0] == '#') continue;
    char *q;
    const int cost = strtol(buf, &q, 10);
    if (q == buf) continue;
    std::string t(q);
    while (!t.empty() && (t.back() == '\n' || t.back() == ' ')) t.pop_back();
    // pair-pool D flags are not used in wing pools; strip a trailing " D0"/" D1" if present
    if (t.size() >= 3 && t.compare(t.size() - 3, 2, " D") == 0 && (t.back() == '0' || t.back() == '1')) t.resize(t.size() - 3);
    std::vector<cc::Mv> mv;
    if (!cc::parseMoves(t, mv)) { fprintf(stderr, "bad line: %s", buf); return 1; }
    for (auto &m : mv) if (m.depth > 2) { fprintf(stderr, "depth above 2 in a wing pool: %s", buf); return 1; }
    total++;
    allByCost[cost]++;
    bool centresOk = true, othersOk = true;
    for (auto &C : cubes) {
      const int N = C.N, M = N - 1;
      for (int d = 2; d <= N / 2 && centresOk && othersOk; d++) {
        std::vector<int> a(C.pos.size()), tmp;
        for (size_t i = 0; i < a.size(); i++) a[i] = i;
        for (auto &m : mv) C.apply(a, cc::Mv{m.face, m.depth == 2 ? d : 1, m.amt}, tmp);
        // a[j] = the facelet whose sticker is now at j
        for (int fc = 0; fc < 6; fc++)
          for (int r = 0; r < N; r++)
            for (int c = 0; c < N; c++) {
              const int j = C.index(fc, r, c);
              const bool centre = r > 0 && r < M && c > 0 && c < M;
              if (centre) { centresOk &= C.faceOf[a[j]] == fc; continue; }
              // edge facelets of the wing orbit at depth d: offset d-1 or N-d along the edge
              const bool edgeRow = r == 0 || r == M, edgeCol = c == 0 || c == M;
              const bool corner = edgeRow && edgeCol;
              const int along = edgeRow ? c : r;
              const bool inOrbit = !corner && (along == d - 1 || along == N - d);
              if (!inOrbit) othersOk &= a[j] == j;
            }
      }
    }
    if (!othersOk) { if (otherBad++ < 3) fprintf(stderr, "outside its orbit: %d %s\n", cost, t.c_str()); continue; }
    if (!centresOk) continue;
    kept++;
    keptByCost[cost]++;
    printf("%d %s\n", cost, t.c_str());
  }
  fclose(f);
  fprintf(stderr, "%ld classes, %ld change something outside their wing orbit and the centres, %ld keep every centre colour-solved on %d..%d\n", total, otherBad, kept, minN, maxN);
  fprintf(stderr, "by cost (kept/all):");
  for (auto &kv : allByCost) fprintf(stderr, " %d:%ld/%ld", kv.first, keptByCost[kv.first], kv.second);
  fprintf(stderr, "\n");
  return 0;
}
