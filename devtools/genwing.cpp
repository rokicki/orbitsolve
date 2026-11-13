// Generate wing algorithms: commutators [A,B] = A B A' B' over face turns and
// depth-2 slices, validated on a 6x6x6 xcube to change nothing except the
// depth-2 wing orbit (all centres, corners, other wings untouched).  Orbits
// (2,3)/(3,2) of the 6x6x6 stand in for every (2,k), since slice k is never
// turned.  Output in the same 48-slot pool format as mkpools (slots 24..47
// identity), deduplicated by wing permutation keeping the shortest.
//
//   genwing > wing2.pool
#include "layout.h"
#include <map>
#include <unordered_map>

static std::string algText(const std::vector<mv> &a) {
  std::string s;
  for (auto m : a) {
    if (!s.empty()) s += ' ';
    if (m.dep > 1) s += std::to_string(m.dep);
    s += "ULFRBD"[m.face];
    if (m.twist == 2) s += '2';
    if (m.twist == 3) s += '\'';
  }
  return s;
}

// cancel adjacent same-layer moves (keeps the sequence honest about length)
static std::vector<mv> simplify(const std::vector<mv> &in) {
  std::vector<mv> r;
  for (auto m : in) {
    if (!r.empty() && r.back().face == m.face && r.back().dep == m.dep) {
      int t = (r.back().twist + m.twist) & 3;
      if (t) r.back().twist = t;
      else r.pop_back();
    } else r.push_back(m);
  }
  return r;
}

struct Key {
  std::array<uint8_t, 24> p;
  bool operator<(const Key &o) const { return p < o.p; }
};

int main(int argc, char **argv) {
  bool freeCentres = argc > 1 && std::string(argv[1]) == "--freecentres";  // centres may change (wings-first)
  std::vector<mv> gens;
  for (int d = 1; d <= 2; d++)
    for (short f = 0; f < 6; f++)
      for (short t = 1; t <= 3; t++) gens.push_back({d, f, t});
  auto inv = [](std::vector<mv> a) {
    std::reverse(a.begin(), a.end());
    for (auto &m : a) m.twist = 4 - m.twist;
    return a;
  };
  // all move sequences of length 1..3 without adjacent same-face moves
  std::vector<std::vector<mv>> seqs[4];
  for (auto &g : gens) seqs[1].push_back({g});
  for (int L = 2; L <= 3; L++)
    for (auto &s : seqs[L - 1])
      for (auto &g : gens)
        if (g.face != s.back().face) {
          auto t = s;
          t.push_back(g);
          seqs[L].push_back(t);
        }
  xcube base(2, 2, 'U');  // N = 6
  std::vector<uint8_t> baseCentres;
  for (int y = 1; y <= 2; y++)
    for (int x = 1; x <= 2; x++) {
      uint8_t o[24];
      readOrbit(base, x, y, o);
      baseCentres.insert(baseCentres.end(), o, o + 24);
    }
  std::map<Key, std::vector<mv>> best;
  long tried = 0, valid = 0;
  auto test = [&](const std::vector<mv> &A, const std::vector<mv> &B) {
    std::vector<mv> c = A;
    c.insert(c.end(), B.begin(), B.end());
    auto Ai = inv(A), Bi = inv(B);
    c.insert(c.end(), Ai.begin(), Ai.end());
    c.insert(c.end(), Bi.begin(), Bi.end());
    c = simplify(c);
    if (c.empty()) return;
    tried++;
    xcube x = base;
    for (auto m : c) domove(m, x, true);
    for (int k = 0; k < 24; k++)
      if (x.tc.corner.a[k] != base.tc.corner.a[k] || x.wingorbits[1].a[k] != base.wingorbits[1].a[k]) return;
    int i = 0;
    if (!freeCentres)
    for (int y = 1; y <= 2; y++)
      for (int xx = 1; xx <= 2; xx++) {
        uint8_t o[24];
        readOrbit(x, xx, y, o);
        for (int k = 0; k < 24; k++, i++)
          if (o[k] != baseCentres[i]) return;
      }
    Key key;
    bool ident = true;
    for (int k = 0; k < 24; k++) { key.p[k] = x.wingorbits[0].a[k]; ident &= key.p[k] == k; }
    if (ident) return;
    valid++;
    auto it = best.find(key);
    if (it == best.end() || c.size() < it->second.size()) best[key] = c;
  };
  for (auto &A : seqs[1])
    for (int L = 1; L <= 3; L++)
      for (auto &B : seqs[L]) test(A, B);
  for (auto &A : seqs[2])
    for (auto &B : seqs[2]) test(A, B);
  fprintf(stderr, "tried %ld commutators, %ld wing-only, %zu distinct wing permutations\n", tried, valid, best.size());
  std::map<int, int> byLen, bySup;
  for (auto &kv : best) {
    int sup = 0;
    for (int k = 0; k < 24; k++) sup += kv.first.p[k] != k;
    byLen[kv.second.size()]++;
    bySup[sup]++;
    printf("%zu", kv.second.size());
    for (int k = 0; k < 24; k++) printf(" %d", kv.first.p[k]);
    for (int k = 24; k < 48; k++) printf(" %d", k);
    printf("  %s\n", algText(kv.second).c_str());
  }
  for (auto &kv : byLen) fprintf(stderr, "  length %2d: %d\n", kv.first, kv.second);
  for (auto &kv : bySup) fprintf(stderr, "  support %2d: %d\n", kv.first, kv.second);
}
