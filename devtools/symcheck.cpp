// Feasibility of a symmetry-reduced finish table (memory-reduction branch).
//  1. colour consistency: target(sigma_s[p]) == face_s[target(p)] for every
//     symmetry s of the pair model (so a symmetry maps a state to a state);
//  2. the rotations about the U-D axis among the 48 symmetries (no reflection,
//     U and D fixed) and their closure;
//  3. distinct states vs distinct canonical states (least key over the group)
//     for the pair finish classes (c12x, p9, eg8), with exact hash sets.
//   symcheck            (run from dist/gullicube)
#include "classpool.h"
#include <cstdio>
#include <random>
#include <unordered_set>

int main(int argc, char **argv) {
  const std::string kind = argc > 1 ? argv[1] : "pair";
  OrbitModel m = kind == "diag" ? diagModel() : kind == "mid" ? midModel() : pairModel();
  printf("model %s: %d positions\n", kind.c_str(), m.P);
  printf("%zu symmetries\n", m.syms.size());
  int bad = 0;
  for (size_t s = 0; s < m.syms.size(); s++)
    for (int p = 0; p < m.P; p++)
      if (target(m.sigma[s][p]) != m.syms[s].face[target(p)]) bad++;
  printf("colour consistency: %s (%d mismatches)\n", bad ? "FAILS" : "ok", bad);
  // U = face 0, D = face 5 (ULFRBD)
  std::vector<int> G;
  for (size_t s = 0; s < m.syms.size(); s++)
    if (!m.syms[s].refl && m.syms[s].face[0] == 0 && m.syms[s].face[5] == 5) G.push_back(s);
  printf("rotations about U-D:");
  for (int g : G) {
    printf(" %d(", g);
    for (int f = 0; f < 6; f++) printf("%c", "ULFRBD"[m.syms[g].face[f]]);
    printf(")");
  }
  printf("\n");
  // closure: sigma_a o sigma_b is again in G
  int closed = 1;
  for (int a : G)
    for (int b : G) {
      bool found = false;
      for (int c : G) {
        bool eq = true;
        for (int p = 0; p < m.P && eq; p++) eq = m.sigma[a][m.sigma[b][p]] == m.sigma[c][p];
        found |= eq;
      }
      closed &= found;
    }
  printf("closed under composition: %s\n", closed ? "yes" : "NO");
  // keys
  std::mt19937_64 rng(4242);
  uint64_t Z[48][6];
  for (auto &z : Z) for (auto &x : z) x = rng();
  std::unordered_set<uint64_t> all, canon;
  size_t images = 0;
  std::vector<const char *> files = {"data/c12x.cls", "data/p9.cls", "data/eg8.cls"};
  if (kind == "diag") files = {"data/diag_x10.cls"};
  if (kind == "mid") files = {"data/mid_x10.cls"};
  for (const char *fn : files) {
    ClassPool cp;
    if (!cp.load(fn)) { fprintf(stderr, "cannot read %s\n", fn); return 1; }
    size_t a0 = all.size(), c0 = canon.size();
    cp.expand(m, [&](const uint8_t *src, int, uint32_t) {
      images++;
      // the state the algorithm solves: position src[d] holds colour target(d)
      uint8_t col[48];
      for (int d = 0; d < 48; d++) col[d] = target(d);
      for (int d = 0; d < m.P; d++) col[src[d]] = target(d);
      uint64_t k0 = 0, best = ~0ULL;
      for (int g : G) {
        uint64_t k = 0;  // key of the rotated state: position sigma[p] holds face_g[col[p]]
        for (int p = 0; p < 48; p++) k ^= p < m.P ? Z[m.sigma[g][p]][m.syms[g].face[col[p]]] : Z[p][col[p]];
        if (g == 0) k0 = k;
        best = std::min(best, k);
      }
      all.insert(k0);
      canon.insert(best);
    });
    printf("%s: +%zu states, +%zu canonical\n", fn, all.size() - a0, canon.size() - c0);
  }
  printf("total: %zu images, %zu distinct states, %zu canonical (reduction %.2fx)\n", images, all.size(), canon.size(),
         (double)all.size() / canon.size());
}
