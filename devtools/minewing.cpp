// Mine wing algorithms (centres free) from a stream of move sequences.
//
//   twsearch ... 6w.tws | ./minewing OUT [--maxsup s] [-t threads]
//
// Input: sequences of face turns and depth-2 slices (6x6x6 notation), e.g. from
// twsearch on 6w.tws (identity on corners and the depth-3 wings; depth-2 wings
// and centres identical).  For each sequence the permutation of the target wing
// orbit (depth 2) is computed from per-move permutations; sequences moving more
// than --maxsup wings (default 8) are dropped, and the cheapest (after merging
// same-axis moves) is kept per permutation.  Kept sequences are expanded under
// the 48 cube symmetries x inverse (the input is reduced by rotations), and every
// resulting algorithm is validated on several cubes -- 5^3 (middle edges), 6^3,
// 7^3 (both wing depths), 8^3 and 9^3 (innermost) -- to change nothing but its
// wing orbit among corners, middle edges and wings, with the same permutation.
// Output: pool lines "COST s0..s47  moves" (slots 24..47 identity; '2' = the
// wing orbit's depth).
#include "validate.h"
#include "../dist/gullicube/src/pair/seq.h"
#include <atomic>
#include <iostream>
#include <map>
#include <mutex>
#include <thread>
#include <unordered_map>

using P24 = std::array<uint8_t, 24>;
struct PH { size_t operator()(const P24 &p) const { uint64_t h = 1469598103934665603ULL; for (auto c : p) h = (h ^ c) * 1099511628211ULL; return h; } };
struct WProbe { int Mx, My, k; };  // cube, target wing orbit (depth k+2)

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: SEQUENCES | minewing OUT [--maxsup s] [-t threads]\n"
                    "  keep the cheapest sequence per depth-2 wing permutation moving <= s wings\n"
                    "  (default 8), expand 96 ways, validate on 5^3..9^3, write a pool to OUT\n");
    return 1;
  }
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  const char *outFile = argv[1];
  int maxSup = 8, threads = std::thread::hardware_concurrency();
  for (int i = 2; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--maxsup" && i + 1 < argc) maxSup = atoi(argv[++i]);
    else if (a == "-t" && i + 1 < argc) threads = atoi(argv[++i]);
    else { fprintf(stderr, "unknown option %s\n", argv[i]); return 1; }
  }
  // xcube(Mx, My): N = Mx + My + 2; wing orbit k at depth k + 2
  std::vector<WProbe> probes = {{2, 2, 0}, {2, 1, 0}, {3, 2, 0}, {3, 2, 1}, {3, 3, 2}, {4, 3, 2}};
  P24 mperm[6][3][4];  // face, depth 1..2, twist
  for (int f = 0; f < 6; f++)
    for (int d = 1; d <= 2; d++)
      for (int t = 1; t <= 3; t++) {
        xcube c(2, 2, 'U');
        domove({(short)d, (short)f, (short)t}, c, true);
        std::copy(c.wingorbits[0].a, c.wingorbits[0].a + 24, mperm[f][d][t].begin());
      }
  auto permOf = [&](const std::vector<Move> &a) {
    P24 p;
    for (int k = 0; k < 24; k++) p[k] = k;
    for (auto &m : a) {
      const P24 &q = mperm[m.face][m.depth][m.amt];
      P24 r;
      for (int k = 0; k < 24; k++) r[k] = p[q[k]];
      p = r;
    }
    return p;
  };
  auto supOf = [](const P24 &p) { int s = 0; for (int k = 0; k < 24; k++) s += p[k] != k; return s; };

  // stream: batches of lines, processed in parallel
  const int NSH = 256;
  std::vector<std::unordered_map<P24, std::pair<int, std::string>, PH>> shard(NSH);
  std::vector<std::mutex> mu(NSH);
  auto keep = [&](const P24 &p, int cost, const std::string &text) {
    size_t sh = PH()(p) % NSH;
    std::lock_guard<std::mutex> lk(mu[sh]);
    auto it = shard[sh].find(p);
    if (it == shard[sh].end()) shard[sh].emplace(p, std::make_pair(cost, text));
    else if (cost < it->second.first) it->second = {cost, text};
  };
  long nIn = 0, nBad = 0;
  std::atomic<long> nKept{0};
  std::vector<std::string> batch;
  auto process = [&]() {
    std::atomic<size_t> nx{0};
    std::vector<std::thread> th;
    for (int t = 0; t < threads; t++)
      th.emplace_back([&] {
        for (size_t i; (i = nx.fetch_add(1)) < batch.size();) {
          std::vector<Move> a;
          try { a = parse_alg(batch[i]); } catch (...) { continue; }
          bool ok = !a.empty();
          for (auto &m : a) if (m.depth > 2) ok = false;
          if (!ok) continue;
          P24 p = permOf(a);
          int s = supOf(p);
          if (s == 0 || s > maxSup) continue;
          auto g = to_groups(a);
          keep(p, groups_cost(g), groups_str(g));
          nKept++;
        }
      });
    for (auto &t : th) t.join();
    batch.clear();
  };
  for (std::string l; std::getline(std::cin, l);) {
    if (l.empty() || l[0] == '#' || l.compare(0, 6, "Trying") == 0 || l.compare(0, 2, "No") == 0) { nBad += !l.empty(); continue; }
    batch.push_back(l);
    nIn++;
    if (batch.size() >= (1 << 20)) { process(); fprintf(stderr, "  %ld sequences, %ld small-support\n", nIn, nKept.load()); }
  }
  process();
  size_t base = 0;
  for (auto &s : shard) base += s.size();
  fprintf(stderr, "read %ld sequences; %ld move <= %d wings; %zu distinct permutations\n", nIn, nKept.load(), maxSup, base);

  // expand under 48 symmetries x inverse
  std::vector<std::pair<P24, std::pair<int, std::string>>> seeds;
  for (auto &s : shard) for (auto &kv : s) seeds.push_back(kv);
  for (auto &s : shard) s.clear();
  auto syms = symmetries();
  {
    std::atomic<size_t> nx{0};
    std::vector<std::thread> th;
    for (int t = 0; t < threads; t++)
      th.emplace_back([&] {
        for (size_t i; (i = nx.fetch_add(1)) < seeds.size();) {
          auto base = parse_alg(seeds[i].second.second);
          for (int img = 0; img < 96; img++) {
            auto b = base;
            if (img & 1) {
              std::reverse(b.begin(), b.end());
              for (auto &m : b) m.amt = (4 - m.amt) & 3;
            }
            const Sym &sy = syms[img >> 1];
            for (auto &m : b) {
              m.face = sy.face[m.face];
              if (sy.refl) m.amt = (4 - m.amt) & 3;
            }
            auto g = to_groups(b);
            keep(permOf(b), groups_cost(g), groups_str(g));
          }
        }
      });
    for (auto &t : th) t.join();
  }
  std::vector<std::pair<P24, std::pair<int, std::string>>> all;
  for (auto &s : shard) for (auto &kv : s) all.push_back(kv);
  fprintf(stderr, "after symmetry expansion: %zu distinct permutations\n", all.size());

  // validate every algorithm on the probe cubes
  std::vector<uint8_t> ok(all.size(), 0);
  std::atomic<long> bad{0};
  {
    std::atomic<size_t> nx{0};
    std::vector<std::thread> th;
    for (int t = 0; t < threads; t++)
      th.emplace_back([&] {
        std::vector<uint8_t> p;
        for (size_t i; (i = nx.fetch_add(1)) < all.size();) {
          std::vector<mv> a;
          for (auto &m : parse_alg(all[i].second.second)) a.push_back({(short)m.depth, (short)m.face, (short)m.amt});
          bool good = true;
          for (auto &q : probes) {
            auto dm = [&q](int) { return q.k + 2; };
            if (!wingOnly(q.Mx, q.My, a, dm, q.k, p) || !std::equal(p.begin(), p.end(), all[i].first.begin())) { good = false; break; }
          }
          if (good) ok[i] = 1;
          else bad++;
        }
      });
    for (auto &t : th) t.join();
  }
  fprintf(stderr, "validation: %ld of %zu failed (dropped)\n", bad.load(), all.size());
  std::vector<size_t> order;
  for (size_t i = 0; i < all.size(); i++) if (ok[i]) order.push_back(i);
  std::sort(order.begin(), order.end(), [&](size_t x, size_t y) {
    if (all[x].second.first != all[y].second.first) return all[x].second.first < all[y].second.first;
    return all[x].second.second < all[y].second.second;
  });
  FILE *f = fopen(outFile, "w");
  std::map<std::pair<int, int>, long> hist;  // (cost, support)
  for (size_t i : order) {
    fprintf(f, "%d", all[i].second.first);
    for (int k = 0; k < 24; k++) fprintf(f, " %d", all[i].first[k]);
    for (int k = 24; k < 48; k++) fprintf(f, " %d", k);
    fprintf(f, "  %s\n", all[i].second.second.c_str());
    hist[{all[i].second.first, supOf(all[i].first)}]++;
  }
  fclose(f);
  printf("%s: %zu algorithms\n  cost support count\n", outFile, order.size());
  for (auto &h : hist) printf("  %4d %7d %8ld\n", h.first.first, h.first.second, h.second);
}
