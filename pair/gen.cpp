// Read algorithms (one per line, twsearch notation; an optional leading integer
// cost field and parenthesised cycles, as in *.costalgs, are skipped), check
// each on an 8x8x8 that it preserves everything except the (2,3)/(3,2) oblique
// pair and the diagonal orbits, expand 96 ways (48 symmetries x inverse), and
// print one line per distinct pair permutation, keeping the shortest:
//
//   LEN  s0 s1 ... s47  moves...
//
// where after the algorithm position d holds what was at s_d.  Positions 0..23
// are orbit A, 24..47 orbit B; position p has target colour (p%24)/4 (ULFRBD).
#include "cube.h"
#include "seq.h"
#include <cstdio>
#include <iostream>
#include <unordered_map>
#include <cstring>

struct Key {
  std::array<uint8_t, 48> a;
  uint16_t head = 0, tail = 0;  // only set with -H
  bool operator==(const Key &o) const { return a == o.a && head == o.head && tail == o.tail; }
};
struct KH {
  size_t operator()(const Key &k) const {
    uint64_t h = 1469598103934665603ULL;
    for (auto c : k.a) h = (h ^ c) * 1099511628211ULL;
    h = (h ^ k.head) * 1099511628211ULL;
    h = (h ^ k.tail) * 1099511628211ULL;
    return h;
  }
};

int main(int argc, char **argv) {
  int maxlen = 99;
  int maxSupport = 99;
  bool allowCorners = false, noexpand = false, headTail = false, conj = false;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-m")) maxlen = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-c")) allowCorners = true;
    else if (!strcmp(argv[i], "-n")) noexpand = true;
    else if (!strcmp(argv[i], "-H")) headTail = true;   // keep one per (perm, first group, last group)
    else if (!strcmp(argv[i], "-C")) conj = true;
    else if (!strcmp(argv[i], "-S")) maxSupport = atoi(argv[++i]);  // keep only perms moving <= this many pair pieces       // also emit X A X' for every single move X
  }
  Cube cube(8);
  int S = cube.pos.size();
  // classify orbits
  std::vector<int> otype(cube.norbits, -1);  // 0 must-fix, 1 corner, 2 ignore(diag), 3 pair
  for (int i = 0; i < S; i++) {
    int dr = cube.drow(i), dc = cube.dcol(i), t;
    if (dr == 1 && dc == 1) t = 1;
    else if (dr == 1 || dc == 1) t = 0;
    else if (dr == dc) t = 2;
    else if ((dr == 2 && dc == 3) || (dr == 3 && dc == 2)) t = 3;
    else t = 0;
    otype[cube.orbit[i]] = t;
  }
  std::vector<int> pairOrbits;
  for (int o = 0; o < cube.norbits; o++)
    if (otype[o] == 3) pairOrbits.push_back(o);
  if (pairOrbits.size() != 2) { fprintf(stderr, "expected 2 pair orbits\n"); return 1; }
  // orbit A: the one containing the U sticker at row 1, col 2
  int orbA = -1;
  for (int i = 0; i < S; i++)
    if (cube.face[i] == 0 && cube.row[i] == 1 && cube.col[i] == 2) orbA = cube.orbit[i];
  int orbB = pairOrbits[0] == orbA ? pairOrbits[1] : pairOrbits[0];
  std::vector<int> pidx(S, -1), pst;
  for (int which = 0; which < 2; which++) {
    int o = which ? orbB : orbA;
    std::vector<int> st;
    for (int i = 0; i < S; i++)
      if (cube.orbit[i] == o) st.push_back(i);
    // sticker list is already in face-major order
    if (st.size() != 24) abort();
    for (int k = 0; k < 24; k++) {
      if (cube.face[st[k]] != k / 4) abort();
      pidx[st[k]] = which * 24 + k;
      pst.push_back(st[k]);
    }
  }
  // restricted move perms on the 48 pair positions: pm[f][d][p] = new position
  std::vector<std::vector<std::array<uint8_t, 48>>> pm(6, std::vector<std::array<uint8_t, 48>>(4));
  for (int f = 0; f < 6; f++)
    for (int d = 1; d <= 3; d++)
      for (int p = 0; p < 48; p++) pm[f][d][p] = pidx[cube.movePerm[f][d - 1][pst[p]]];
  // 48 symmetries: signed permutation matrices -> face map and handedness
  std::vector<std::array<int, 6>> symFace;
  std::vector<int> symDet;
  int perms[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
  int permSign[6] = {1, -1, -1, 1, 1, -1};
  for (int pi = 0; pi < 6; pi++)
    for (int s = 0; s < 8; s++) {
      std::array<int, 6> fm;
      for (int f = 0; f < 6; f++) {
        int v[3];
        for (int r = 0; r < 3; r++) v[r] = NORMAL[f][perms[pi][r]] * ((s >> r) & 1 ? -1 : 1);
        fm[f] = face_of_normal(v[0], v[1], v[2]);
      }
      int det = permSign[pi] * (__builtin_popcount(s) & 1 ? -1 : 1);
      symFace.push_back(fm);
      symDet.push_back(det);
    }
  std::unordered_map<Key, std::pair<int, std::string>, KH> best;
  std::string line;
  long nin = 0, nbad = 0, ncorner = 0, nlong = 0;
  while (std::getline(std::cin, line)) {
    // strip "COST (cycles)" prefix if present
    size_t p = line.find_last_of(')');
    if (p != std::string::npos) line = line.substr(p + 1);
    else {
      size_t q = line.find_first_not_of(" \t");
      if (q != std::string::npos && isdigit((unsigned char)line[q])) {
        size_t e = line.find_first_of(" \t", q);
        std::string tok = line.substr(q, e - q);
        bool alldig = tok.find_first_not_of("0123456789") == std::string::npos;
        if (alldig) line = line.substr(e);
      }
    }
    std::vector<Move> alg;
    try { alg = parse_alg(line); } catch (...) { continue; }
    if (alg.empty()) continue;
    nin++;
    if ((int)alg.size() > maxlen) { nlong++; continue; }
    bool deep = false;
    for (auto &m : alg) if (m.depth > 3) deep = true;
    if (deep) { nbad++; continue; }
    auto loc = cube.apply(alg);
    bool ok = true, cornerOk = true;
    for (int i = 0; i < S; i++)
      if (loc[i] != i) {
        int t = otype[cube.orbit[i]];
        if (t == 0) ok = false;
        if (t == 1) cornerOk = false;
      }
    if (!ok) { nbad++; continue; }
    if (!cornerOk) { ncorner++; if (!allowCorners) continue; }
    if (maxSupport < 48) {
      int sup = 0;
      for (int i = 0; i < S; i++)
        if (loc[i] != i && otype[cube.orbit[i]] == 3) sup++;
      if (sup > maxSupport) continue;
    }
    int nsym = noexpand ? 1 : 48;
    for (int inv = 0; inv < (noexpand ? 1 : 2); inv++) {
      std::vector<Move> a = alg;
      if (inv) {
        std::reverse(a.begin(), a.end());
        for (auto &m : a) m.amt = 4 - m.amt;
      }
      for (int s = 0; s < nsym; s++) {
        std::vector<Move> b = a;
        if (s) for (auto &m : b) {
          m.face = symFace[s][m.face];
          if (symDet[s] < 0) m.amt = 4 - m.amt;
        }
        for (int x = -1; x < (conj ? 54 : 0); x++) {
        std::vector<Move> c = b;
        if (x >= 0) {
          Move X{(uint8_t)(x / 9), (uint8_t)(x / 3 % 3 + 1), (uint8_t)(x % 3 + 1)};
          Move Xi = X;
          Xi.amt = 4 - X.amt;
          c.insert(c.begin(), X);
          c.push_back(Xi);
        }
        auto grp = to_groups(c);
        int len = groups_cost(grp);
        std::array<uint8_t, 48> l;
        for (int q = 0; q < 48; q++) l[q] = q;
        for (auto &m : c)
          for (int k = 0; k < m.amt; k++)
            for (int q = 0; q < 48; q++) l[q] = pm[m.face][m.depth][l[q]];
        Key key;
        bool ident = true;
        for (int q = 0; q < 48; q++) { key.a[l[q]] = q; if (l[q] != q) ident = false; }
        if (ident) continue;
        if (headTail) { key.head = grp.front(); key.tail = grp.back(); }
        auto it = best.find(key);
        if (it == best.end() || len < it->second.first) best[key] = {len, groups_str(grp)};
        }
      }
    }
  }
  fprintf(stderr, "read %ld, bad %ld, corner-disturbing %ld, toolong %ld, distinct perms %zu\n",
          nin, nbad, ncorner, nlong, best.size());
  std::vector<std::pair<int, std::pair<Key, std::string>>> out;
  for (auto &kv : best) out.push_back({kv.second.first, {kv.first, kv.second.second}});
  std::sort(out.begin(), out.end(), [](auto &x, auto &y) {
    if (x.first != y.first) return x.first < y.first;
    return x.second.second < y.second.second;
  });
  for (auto &o : out) {
    printf("%d", o.first);
    for (int q = 0; q < 48; q++) printf(" %d", o.second.first.a[q]);
    printf("  %s\n", o.second.second.c_str());
  }
}
