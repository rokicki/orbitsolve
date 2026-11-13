// Feasibility: how many collapsed pair-log sequences ('2' -> depth b+1, '3' ->
// middle layer) change nothing but the mid orbit (Mx,b) and the diagonal (b,b)?
// Checked on 9^3 (b = 1, 3), 7^3 (b = 1), 11^3 (b = 2), 13^3 (b = 5).
//   xz -dc ../6centers.log.xz | ./middiagcount [--max L] [-t threads]
#include "validate.h"
#include "../dist/gullicube/src/pair/seq.h"
#include <atomic>
#include <iostream>
#include <map>
#include <mutex>
#include <thread>
struct Pr { int Mx, My, b; };
static bool twoOrbits(const Pr &q, const std::vector<mv> &a, std::vector<uint8_t> &perm) {
  xcube c(q.Mx, q.My, 'U');
  Snap s0(c);
  applyMapped(c, a, [&](int d) { return d == 2 ? q.b + 1 : q.Mx + 1; });
  Snap s1(c);
  if (s0.wings != s1.wings || s0.rest != s1.rest) return false;
  int im = ((q.b - 1) * q.Mx + (q.Mx - 1)) * 24, id = ((q.b - 1) * q.Mx + (q.b - 1)) * 24;  // (x=Mx,y=b), (x=b,y=b)
  for (size_t i = 0; i < s0.centres.size(); i++) {
    bool mine = (i >= (size_t)im && i < (size_t)im + 24) || (i >= (size_t)id && i < (size_t)id + 24);
    if (!mine && s0.centres[i] != s1.centres[i]) return false;
  }
  perm.assign(s1.centres.begin() + im, s1.centres.begin() + im + 24);
  perm.insert(perm.end(), s1.centres.begin() + id, s1.centres.begin() + id + 24);
  return true;
}
int main(int argc, char **argv) {
  int maxLen = 9, threads = std::thread::hardware_concurrency();
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--max") maxLen = atoi(argv[++i]);
    else if (a == "-t") threads = atoi(argv[++i]);
    else { fprintf(stderr, "unknown option %s\n", argv[i]); return 1; }
  }
  std::vector<Pr> probes = {{4, 3, 1}, {4, 3, 3}, {3, 2, 1}, {5, 4, 2}, {6, 5, 5}};
  std::vector<std::string> lines;
  for (std::string l; std::getline(std::cin, l);) {
    int n = 0; for (char ch : l) n += ch == ' ';
    if (n <= maxLen) lines.push_back(l);  // leading space: n = moves
  }
  std::atomic<size_t> nx{0};
  std::mutex mu;
  std::map<int, long> both, midOnly, diagOnly;  // by cost: valid collapsed canonical sequences
  std::vector<std::thread> th;
  for (int t = 0; t < threads; t++)
    th.emplace_back([&] {
      std::vector<uint8_t> p, p0;
      for (size_t i; (i = nx.fetch_add(1)) < lines.size();) {
        std::vector<Move> base;
        try { base = parse_alg(lines[i]); } catch (...) { continue; }
        for (int var = 0; var < 2; var++) {
          std::vector<mv> a;
          for (auto &m : base) {
            int d = m.depth;
            if (d > 1) d = (d == 2) == (var == 0) ? 2 : 3;
            a.push_back({(short)d, (short)m.face, (short)m.amt});
          }
          bool ok = true;
          for (size_t k = 0; k < probes.size() && ok; k++) {
            if (!twoOrbits(probes[k], a, p)) ok = false;
            else if (k == 0) p0 = p;
            else if (p != p0) ok = false;
          }
          if (!ok) continue;
          bool mid = false, dg = false;
          for (int k = 0; k < 24; k++) { mid |= p0[k] != k; dg |= p0[24 + k] != k; }
          if (!mid && !dg) continue;
          std::vector<Move> b;
          for (auto &x : a) b.push_back(Move{(uint8_t)x.face, (uint8_t)x.dep, (uint8_t)x.twist});
          int c = groups_cost(to_groups(b));
          std::lock_guard<std::mutex> lk(mu);
          (mid && dg ? both : mid ? midOnly : diagOnly)[c]++;
        }
      }
    });
  for (auto &t : th) t.join();
  auto pr = [](const char *name, std::map<int, long> &m) {
    printf("%-22s", name);
    for (auto &kv : m) printf(" %d:%ld", kv.first, kv.second);
    printf("\n");
  };
  printf("canonical sequences (<= %d moves) valid after collapse, by cost (before symmetry expansion):\n", maxLen);
  pr("  mid + diagonal both", both);
  pr("  mid only", midOnly);
  pr("  diagonal only", diagOnly);
}
