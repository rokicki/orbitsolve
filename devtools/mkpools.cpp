// Build the phase-2 single-orbit pools in brobdicube's native orbit layout, by
// simulating each candidate on a 'U' xcube (every sticker distinct) and keeping
// only algorithms that change nothing but their target orbit.
//
//   diag.pool : diagonal orbit (a,a); algorithm slices all at depth a+1
//               (the round-1 single-orbit set with 3 -> 2)
//   mid.pool  : mid-centre orbit (Mx,b) on odd cubes; slices '2' -> b+1 and
//               '3' -> middle layer, or the reverse, whichever validates
//   wing.pool : wing orbit at depth k; brobdicube's wing commutators under all
//               48 symmetries x inverse, slice depth 2 -> k
//
// Output lines: "COST s0 .. s47  moves" in the pair-solver pool format, with
// slots 24..47 the identity (after the algorithm, position k holds what was at
// s_k).  Moves use '2'/'3' placeholders for the substituted depths.
#include "layout.h"
#include "validate.h"
#include <fstream>
#include <map>
#include <sstream>
#include <unordered_map>

struct Out {
  std::map<std::vector<uint8_t>, std::pair<int, std::string>> best;
  long tried = 0, valid = 0;
  void add(const std::vector<uint8_t> &perm, const std::vector<mv> &a) {
    valid++;
    bool ident = true;
    for (int k = 0; k < 24; k++) if (perm[k] != k) ident = false;
    if (ident) return;
    auto it = best.find(perm);
    if (it == best.end() || (int)a.size() < it->second.first) best[perm] = {(int)a.size(), algText(a)};
  }
  void write(const char *fn) {
    FILE *f = fopen(fn, "w");
    std::vector<std::pair<int, std::string>> lines;
    for (auto &kv : best) {
      // pair-solver pool format: 48 slots, the single orbit padded with identity
      std::string s = std::to_string(kv.second.first);
      for (int k = 0; k < 24; k++) s += " " + std::to_string(kv.first[k]);
      for (int k = 24; k < 48; k++) s += " " + std::to_string(k);
      lines.push_back({kv.second.first, s + "  " + kv.second.second});
    }
    std::sort(lines.begin(), lines.end());
    for (auto &l : lines) fprintf(f, "%s\n", l.second.c_str());
    fclose(f);
    printf("%s: tried %ld, valid %ld, distinct perms %zu\n", fn, tried, valid, best.size());
  }
};


// mkpools diagfrom IN OUT: diagonal algorithms from a pair pool file (lines
// "cost s0..s47 moves [D0|D1]"): each algorithm with every slice collapsed to
// depth 2 (3 -> 2), kept if it changes nothing but the diagonal orbit (1,1).
static int diagFrom(const char *in, const char *out, bool mid) {
  std::ifstream f(in);
  Out o;
  std::string line;
  std::vector<uint8_t> p;
  while (std::getline(f, line)) {
    std::istringstream is(line);
    int x;
    for (int k = 0; k < 49; k++) is >> x;
    std::string rest;
    std::getline(is, rest);
    while (!rest.empty() && isspace((unsigned char)rest.back())) rest.pop_back();
    if (rest.size() >= 3 && (rest.compare(rest.size() - 3, 3, " D0") == 0 || rest.compare(rest.size() - 3, 3, " D1") == 0))
      rest.resize(rest.size() - 3);  // the pair pool's diagonal flag (not a D move)
    auto a = parse_moves(rest);
    if (!mid) {
      o.tried++;
      if (centreOnly(3, 3, a, [](int) { return 2; }, 1, 1, p)) {
        for (auto &m : a) if (m.dep > 1) m.dep = 2;
        o.add(p, a);
      }
    } else  // mid orbit (4,1) on N=9: one slice depth -> b+1 (2), the other -> middle layer (5)
      for (int v = 0; v < 2; v++) {
        o.tried++;
        auto dm = [v](int d) { return (d == 2) == (v == 0) ? 2 : 5; };
        if (centreOnly(4, 3, a, dm, 4, 1, p)) {
          auto b = a;
          for (auto &m : b)
            if (m.dep > 1) m.dep = dm(m.dep) == 5 ? 3 : 2;  // '3' = middle layer, '2' = depth b+1
          o.add(p, b);
        }
      }
    if (o.tried % 1000000 == 0) { fprintf(stderr, "%ld tried, %ld valid, %zu distinct\n", o.tried, o.valid, o.best.size()); }
  }
  o.write(out);
  return 0;
}

int main(int argc, char **argv) {
  std::string mode = argc > 1 ? argv[1] : "";
  if (!((mode == "build" && argc == 2) || ((mode == "diagfrom" || mode == "midfrom") && argc == 4))) {
    fprintf(stderr, "usage: mkpools build                  (re)build diag.pool, mid.pool, wing.pool\n"
                    "       mkpools diagfrom IN OUT        diagonal algorithms from pair pool IN (3 -> 2)\n"
                    "       mkpools midfrom IN OUT         mid-orbit algorithms from pair pool IN (one depth -> middle)\n");
    return 1;
  }
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  if (mode == "diagfrom") return diagFrom(argv[2], argv[3], false);
  if (mode == "midfrom") return diagFrom(argv[2], argv[3], true);
  // round-1 single-orbit algorithms (both orbit orientations): text after the perm
  std::vector<std::vector<mv>> single;
  {
    std::ifstream in("../dist/gullicube/src/pair/c12x.txt");
    std::string line;
    while (std::getline(in, line)) {
      std::istringstream is(line);
      int x;
      for (int k = 0; k < 49; k++) is >> x;
      std::string rest;
      std::getline(is, rest);
      single.push_back(parse_moves(rest));
    }
  }
  printf("%zu single-orbit algorithms\n", single.size());
  // diagonals: N=8 (Mx=My=3), orbit (1,1): all slices -> depth 2
  {
    Out o;
    std::vector<uint8_t> p;
    for (auto &a : single) {
      o.tried++;
      if (centreOnly(3, 3, a, [](int) { return 2; }, 1, 1, p)) o.add(p, a);
    }
    o.write("diag.pool");
  }
  // mids: N=9 (Mx=4, My=3), orbit (4,1): middle layer is depth 5
  {
    Out o;
    std::vector<uint8_t> p;
    int ok[2] = {0, 0};
    for (auto &a : single)
      for (int v = 0; v < 2; v++) {
        o.tried++;
        auto dm = [v](int d) { return (d == 2) == (v == 0) ? 2 : 5; };
        if (centreOnly(4, 3, a, dm, 4, 1, p)) {
          ok[v]++;
          auto b = a;
          for (auto &m : b)  // record which placeholder is the middle layer: 'M' -> use depth 9 marker
            if (m.dep > 1) m.dep = dm(m.dep) == 5 ? 3 : 2;  // '3' = middle layer, '2' = depth b+1
          o.add(p, b);
        }
      }
    printf("mid variants valid: 2->b+1/3->mid %d, 2->mid/3->b+1 %d\n", ok[0], ok[1]);
    o.write("mid.pool");
  }
  // wings: brobdicube's base commutators x 48 symmetries x inverse, N=8, depth 2
  {
    Out o;
    auto syms = symmetries();
    std::ifstream in("wingbase.txt");
    std::string line;
    int nb = 0;
    while (std::getline(in, line)) {
      if (line.empty()) continue;
      nb++;
      auto base = parse_moves(line);
      for (int inv = 0; inv < 2; inv++) {
        auto a0 = base;
        if (inv) {
          std::reverse(a0.begin(), a0.end());
          for (auto &m : a0) m.twist = 4 - m.twist;
        }
        for (auto &y : syms) {
          auto a = a0;
          for (auto &m : a) {
            m.face = y.face[m.face];
            if (y.refl) m.twist = 4 - m.twist;
          }
          o.tried++;
          xcube c(3, 3, 'U');
          Snap s0(c);
          for (auto m : a) domove(m, c, true);
          Snap s1(c);
          if (s0.centres != s1.centres || s0.rest != s1.rest) continue;
          bool others = false;
          for (size_t i = 24; i < s0.wings.size(); i++) others |= s0.wings[i] != s1.wings[i];
          if (others) continue;
          std::vector<uint8_t> perm(s1.wings.begin(), s1.wings.begin() + 24);
          o.add(perm, a);
        }
      }
    }
    printf("%d wing base algorithms\n", nb);
    o.write("wing.pool");
  }
}
