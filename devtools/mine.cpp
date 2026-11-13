// Mine diagonal-only or mid-only centre algorithms from twsearch output.
//
//   xz -dc ../6centers.log.xz | ./mine diag|mid OUT [-t threads] [--max L] [--limit n]
//
// Input: one move sequence per line (layers 1..3; e.g. twsearch's canonical
// sequences restoring everything but the (2,3)/(3,2) pair and the diagonals).
// Each sequence is collapsed onto one orbit:
//   diag: both slice depths -> '2' (depth a+1 for diagonal orbit (a,a));
//   mid : one slice depth -> '2' (depth b+1), the other -> '3' (the middle
//         layer), both assignments tried, for mid orbit (Mx,b).
// A collapsed sequence is kept only if, on EVERY cube of a validation set
// (even and odd N, outer and innermost orbits), it changes nothing but its
// target orbit (corners, middle edges, centre centres, all wings, all other
// centre orbits) and induces the same permutation there.  Kept sequences are
// expanded under the 48 cube symmetries x inverse (validity is symmetric), with
// same-axis moves merged, and the cheapest sequence per distinct permutation
// is kept.  Finally every output algorithm is validated again on the whole
// set.  Output: pool lines "COST s0..s47  moves" (slots 24..47 identity).
#include "validate.h"
#include "../dist/gullicube/src/pair/seq.h"
#include <atomic>
#include <iostream>
#include <sstream>
#include <mutex>
#include <thread>
#include <unordered_map>

struct Probe { int Mx, My, x, y; int d2, d3; };  // cube, orbit, actual depths of placeholders '2' / '3'
using P24 = std::array<uint8_t, 24>;
struct PH { size_t operator()(const P24 &p) const { uint64_t h = 1469598103934665603ULL; for (auto c : p) h = (h ^ c) * 1099511628211ULL; return h; } };
struct Rec { uint8_t cost; uint8_t img; uint8_t var; uint32_t canon; };

static bool validAll(const std::vector<mv> &a, const std::vector<Probe> &probes, P24 &perm) {
  std::vector<uint8_t> p;
  for (size_t i = 0; i < probes.size(); i++) {
    const Probe &q = probes[i];
    auto dm = [&q](int d) { return d == 2 ? q.d2 : q.d3; };
    if (!centreOnly(q.Mx, q.My, a, dm, q.x, q.y, p)) return false;
    if (i == 0) std::copy(p.begin(), p.end(), perm.begin());
    else if (!std::equal(p.begin(), p.end(), perm.begin())) return false;
  }
  return true;
}

int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr,
            "usage: xz -dc LOG.xz | mine diag|mid OUT [options]\n"
            "  collapse each input sequence onto the diagonal (or mid) orbit, validate it on\n"
            "  several cube sizes, expand 96 ways, keep the cheapest per permutation\n"
            "  -t n        threads (default: all cores)\n"
            "  --max L     skip input sequences longer than L moves (default 99)\n"
            "  --limit n   read at most n input lines (for sampling)\n");
    return 1;
  }
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  std::string mode = argv[1];
  const char *outFile = argv[2];
  // "mine check diag|mid POOLFILE": validate an existing pool on the whole set
  const bool check = mode == "check";
  if (check) {
    if (argc < 4) { fprintf(stderr, "usage: mine check diag|mid POOLFILE [-t n]\n"); return 1; }
    mode = argv[2];
    outFile = argv[3];
  }
  if (mode != "diag" && mode != "mid") { fprintf(stderr, "mode must be diag or mid\n"); return 1; }
  const bool mid = mode == "mid";
  int threads = std::thread::hardware_concurrency(), maxLen = 99;
  long limit = -1;
  for (int i = check ? 4 : 3; i < argc; i++) {
    std::string a = argv[i];
    if (a == "-t" && i + 1 < argc) threads = atoi(argv[++i]);
    else if (a == "--max" && i + 1 < argc) maxLen = atoi(argv[++i]);
    else if (a == "--limit" && i + 1 < argc) limit = atol(argv[++i]);
    else { fprintf(stderr, "unknown option %s\n", argv[i]); return 1; }
  }
  // validation set; the first probe is the reference layout for permutations.
  // xcube(Mx, My): N = Mx + My + 2, middle layer (odd N) at depth Mx + 1.
  std::vector<Probe> probes;
  if (!mid)
    probes = {{3, 3, 1, 1, 2, 2}, {4, 3, 1, 1, 2, 2}, {4, 3, 3, 3, 4, 4}, {5, 5, 5, 5, 6, 6}, {5, 5, 2, 2, 3, 3},
              {6, 5, 5, 5, 6, 6}};  // N = 8, 9, 9 innermost, 12 innermost, 12, 13 innermost
  else
    probes = {{4, 3, 4, 1, 2, 5}, {4, 3, 4, 3, 4, 5}, {6, 5, 6, 1, 2, 7}, {6, 5, 6, 5, 6, 7}, {6, 5, 6, 3, 4, 7},
              {5, 4, 5, 4, 5, 6}};  // N = 9, 9 inner, 13, 13 innermost, 13, 11 innermost
  // single-move permutations on the reference orbit, for the symmetry images
  P24 mperm[6][4][4];  // face, placeholder depth 1..3, twist
  {
    const Probe &q = probes[0];
    for (int f = 0; f < 6; f++)
      for (int d = 1; d <= 3; d++)
        for (int t = 1; t <= 3; t++) {
          xcube c(q.Mx, q.My, 'U');
          applyMapped(c, {{(short)d, (short)f, (short)t}}, [&q](int dd) { return dd == 2 ? q.d2 : q.d3; });
          uint8_t o[24];
          readOrbit(c, q.x, q.y, o);
          std::copy(o, o + 24, mperm[f][d][t].begin());
        }
  }
  auto syms = symmetries();
  // placeholder sequence (Move: face, depth, amt) -> normalised same-axis groups.
  // A middle-layer move ('3' in mid mode) from the R/B/D side is rewritten from
  // the U/L/F side, so both spellings of the one layer merge.
  auto groupsOf = [mid](std::vector<Move> a) {
    for (auto &m : a)
      if (mid && m.depth == 3 && SIDE[m.face] == 1) {
        static const int OPP[6] = {5, 3, 4, 1, 2, 0};
        m.face = OPP[m.face];
        m.amt = (4 - m.amt) & 3;
      }
    return to_groups(a);
  };
  auto image = [&](const std::vector<Move> &a, int img) {  // img = sym * 2 + inverse
    std::vector<Move> b = a;
    if (img & 1) {
      std::reverse(b.begin(), b.end());
      for (auto &m : b) m.amt = (4 - m.amt) & 3;
    }
    const Sym &s = syms[img >> 1];
    for (auto &m : b) {
      m.face = s.face[m.face];
      if (s.refl) m.amt = (4 - m.amt) & 3;
    }
    return b;
  };
  auto collapse = [mid](const std::vector<Move> &in, int var) {
    std::vector<Move> a = in;
    for (auto &m : a)
      if (m.depth > 1) m.depth = !mid ? 2 : ((m.depth == 2) == (var == 0) ? 2 : 3);
    return a;
  };
  auto toMv = [](const std::vector<Move> &a) {
    std::vector<mv> r;
    for (auto &m : a) r.push_back({(short)m.depth, (short)m.face, (short)m.amt});
    return r;
  };

  if (check) {  // pool lines: "COST s0..s47  moves"
    std::vector<std::string> pl;
    FILE *f = fopen(outFile, "r");
    if (!f) { fprintf(stderr, "cannot read %s\n", outFile); return 1; }
    char buf[4096];
    while (fgets(buf, sizeof buf, f)) pl.push_back(buf);
    fclose(f);
    std::atomic<long> nx{0}, bad{0}, badPerm{0};
    std::vector<std::thread> th;
    for (int t = 0; t < threads; t++)
      th.emplace_back([&] {
        for (long i; (i = nx.fetch_add(1)) < (long)pl.size();) {
          std::istringstream is(pl[i]);
          int c;
          is >> c;
          P24 want;
          for (int k = 0; k < 48; k++) { int x; is >> x; if (k < 24) want[k] = x; }
          std::string rest;
          std::getline(is, rest);
          while (!rest.empty() && isspace((unsigned char)rest.back())) rest.pop_back();
          if (rest.size() >= 3 && (rest.compare(rest.size() - 3, 3, " D0") == 0 || rest.compare(rest.size() - 3, 3, " D1") == 0))
            rest.resize(rest.size() - 3);  // pair-pool diagonal flag, not a move
          P24 p;
          if (!validAll(toMv(parse_alg(rest)), probes, p)) { if (bad++ < 5) fprintf(stderr, "invalid: %s", pl[i].c_str()); }
          else if (p != want) { if (badPerm++ < 5) fprintf(stderr, "perm mismatch: %s", pl[i].c_str()); }
        }
      });
    for (auto &t : th) t.join();
    printf("%s: %zu algorithms, %ld invalid on the validation set, %ld with a different permutation\n", outFile,
           pl.size(), bad.load(), badPerm.load());
    return bad || badPerm ? 2 : 0;
  }
  std::vector<std::string> lines;
  for (std::string l; (limit < 0 || (long)lines.size() < limit) && std::getline(std::cin, l);) lines.push_back(l);
  fprintf(stderr, "%zu input lines\n", lines.size());

  const int NSH = 1024;
  std::vector<std::unordered_map<P24, Rec, PH>> shard(NSH);
  std::vector<std::mutex> mu(NSH);
  std::atomic<long> nx{0}, nParsed{0}, nValid{0};
  std::vector<std::thread> th;
  for (int t = 0; t < threads; t++)
    th.emplace_back([&] {
      for (long i; (i = nx.fetch_add(1)) < (long)lines.size();) {
        if (i % 2000000 == 0) fprintf(stderr, "  line %ld, valid %ld\n", i, nValid.load());
        std::vector<Move> base;
        try { base = parse_alg(lines[i]); } catch (...) { continue; }
        if (base.empty() || (int)base.size() > maxLen) continue;
        bool deep = false;
        for (auto &m : base) if (m.depth > 3) deep = true;
        if (deep) continue;
        nParsed++;
        for (int var = 0; var < (mid ? 2 : 1); var++) {
          auto a = collapse(base, var);
          P24 ref;
          if (!validAll(toMv(a), probes, ref)) continue;
          nValid++;
          for (int img = 0; img < 96; img++) {
            auto b = image(a, img);
            auto g = groupsOf(b);
            int cost = groups_cost(g);
            if (cost == 0) continue;
            P24 p;
            for (int k = 0; k < 24; k++) p[k] = k;
            for (auto &m : b) {  // after m, slot k holds what was at mperm[k]
              const P24 &q = mperm[m.face][m.depth][m.amt];
              P24 r;
              for (int k = 0; k < 24; k++) r[k] = p[q[k]];
              p = r;
            }
            bool ident = true;
            for (int k = 0; k < 24; k++) if (p[k] != k) ident = false;
            if (ident) continue;
            size_t sh = PH()(p) % NSH;
            std::lock_guard<std::mutex> lk(mu[sh]);
            auto it = shard[sh].find(p);
            Rec r{(uint8_t)cost, (uint8_t)img, (uint8_t)var, (uint32_t)i};
            if (it == shard[sh].end()) shard[sh].emplace(p, r);
            else if (cost < it->second.cost) it->second = r;
          }
        }
      }
    });
  for (auto &t : th) t.join();
  size_t distinct = 0;
  for (auto &s : shard) distinct += s.size();
  fprintf(stderr, "parsed %ld, valid collapsed %ld, distinct permutations %zu\n", nParsed.load(), nValid.load(), distinct);

  // regenerate text and validate every output algorithm on the whole set
  std::vector<std::pair<P24, Rec>> all;
  all.reserve(distinct);
  for (auto &s : shard) for (auto &kv : s) all.push_back(kv);
  shard.clear();
  std::vector<std::string> text(all.size());
  std::vector<uint8_t> ok(all.size(), 0);
  std::atomic<long> ny{0}, nBad{0};
  th.clear();
  for (int t = 0; t < threads; t++)
    th.emplace_back([&] {
      for (long i; (i = ny.fetch_add(1)) < (long)all.size();) {
        const Rec &r = all[i].second;
        auto b = image(collapse(parse_alg(lines[r.canon]), r.var), r.img);
        auto g = groupsOf(b);
        text[i] = groups_str(g);
        P24 p;
        auto alg = parse_alg(text[i]);
        if (validAll(toMv(alg), probes, p) && p == all[i].first && groups_cost(to_groups(alg)) == r.cost) ok[i] = 1;
        else nBad++;
      }
    });
  for (auto &t : th) t.join();
  fprintf(stderr, "final validation: %ld of %zu failed (dropped)\n", nBad.load(), all.size());
  std::vector<size_t> order;
  for (size_t i = 0; i < all.size(); i++) if (ok[i]) order.push_back(i);
  std::sort(order.begin(), order.end(), [&](size_t x, size_t y) {
    if (all[x].second.cost != all[y].second.cost) return all[x].second.cost < all[y].second.cost;
    return text[x] < text[y];
  });
  FILE *f = fopen(outFile, "w");
  std::vector<long> hist(64, 0);
  for (size_t i : order) {
    fprintf(f, "%d", all[i].second.cost);
    for (int k = 0; k < 24; k++) fprintf(f, " %d", all[i].first[k]);
    for (int k = 24; k < 48; k++) fprintf(f, " %d", k);
    fprintf(f, "  %s\n", text[i].c_str());
    hist[std::min(63, (int)all[i].second.cost)]++;
  }
  fclose(f);
  printf("%s: %zu algorithms; by cost:", outFile, order.size());
  for (int c = 0; c < 64; c++) if (hist[c]) printf(" %d:%ld", c, hist[c]);
  printf("\n");
}
