// axbfs: exact distances for stage 3ax of the oblique chiral pair in the group <F, R, B, L, 2U, 3U,
// 2D, 3D>, preserving nothing else (2026-10-28).  None of these moves touches the pair's U or D
// pieces, so the state is axbound's: per orbit the 16 side slots (L F R B, 4 each; bit set = F/B
// piece), 8 set, C(16,8)^2 = 165,636,900 states.  Breadth-first search, every move costing 1 (htm).
// Without other orbits kept these are not 3ax distances for the solve; they are its relaxed values
// (a lower bound for 3ax within these moves, and a start for the variant that keeps nothing).
// Reports lengths by position and by class (16 stage symmetries); --save FILE writes the distance
// table (one byte per rank, rank = r16[orbit a] * 12870 + r16[orbit b]).
//   axbfs [--save FILE] [--threads T]
#include "certcube.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <map>
#include <thread>

static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  fflush(stdout);
  int T = std::max(1u, std::thread::hardware_concurrency());
  std::string saveFile;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--save" && i + 1 < argc) saveFile = argv[++i];
    else if (o == "--threads" && i + 1 < argc) T = atoi(argv[++i]);
    else { fprintf(stderr, "unknown option %s\nusage: axbfs [--save FILE] [--threads T]\n", o.c_str()); return 1; }
  }
  const double t0 = now();
  cc::Cube C8(8);
  const auto pf = cc::pairFacelets(C8);
  std::vector<int> fidx(C8.pos.size(), -1);
  for (int k = 0; k < 48; k++) fidx[pf[k]] = k;
  int bitOf[48], facOfBit[32];
  for (int k = 0; k < 48; k++) {
    const int f = k / 8, o = k % 8 / 4, i = k % 4;
    bitOf[k] = (f >= 1 && f <= 4) ? o * 16 + (f - 1) * 4 + i : -1;
    if (bitOf[k] >= 0) facOfBit[bitOf[k]] = k;
  }
  // the moves as byte tables on the 32 side bits
  std::vector<uint32_t> tab;
  int nm = 0;
  auto addMove = [&](int f, int d, int a) {
    int p[48];
    for (int k = 0; k < 48; k++) { int i = pf[k]; for (int q = 0; q < a; q++) i = C8.perms[f][d - 1][i]; p[k] = fidx[i]; }
    for (int k = 0; k < 48; k++)
      if ((bitOf[k] < 0) != (bitOf[p[k]] < 0) || (bitOf[k] < 0 && p[k] != k)) { fprintf(stderr, "move %d %d %d moves U or D pieces\n", f, d, a); exit(1); }
    for (int b = 0; b < 4; b++)
      for (int v = 0; v < 256; v++) {
        uint32_t t = 0;
        for (int j = 0; j < 8; j++) if (v >> j & 1) t |= 1u << bitOf[p[facOfBit[b * 8 + j]]];
        tab.push_back(t);
      }
    nm++;
  };
  for (int f = 1; f <= 4; f++) for (int a = 1; a <= 3; a++) addMove(f, 1, a);
  for (int f : {0, 5}) for (int d : {2, 3}) for (int a = 1; a <= 3; a++) addMove(f, d, a);

  std::vector<int16_t> r16(65536, -1);
  std::vector<uint16_t> u16;
  for (int s = 0; s < 65536; s++) if (__builtin_popcount(s) == 8) r16[s] = u16.size(), u16.push_back(s);
  const uint64_t NU = u16.size(), total = NU * NU;
  auto rankOf = [&](uint32_t s) { return (uint64_t)r16[s & 0xFFFF] * NU + r16[s >> 16]; };
  auto stateOf = [&](uint64_t r) { return (uint32_t)u16[r / NU] | (uint32_t)u16[r % NU] << 16; };
  const uint32_t SOLVED = 0xF0F0F0F0;
  std::vector<uint8_t> dist(total, 0xFF);
  dist[rankOf(SOLVED)] = 0;
  for (int d = 0;; d++) {
    std::atomic<uint64_t> ni{0};
    std::atomic<long> nNew{0};
    auto work = [&] {
      long c = 0;
      for (uint64_t w; (w = ni.fetch_add(1 << 16)) < total;)
        for (uint64_t r = w; r < std::min<uint64_t>(w + (1 << 16), total); r++) {
          if (dist[r] != d) continue;
          const uint32_t s = stateOf(r);
          for (int m = 0; m < nm; m++) {
            const uint32_t *tb = &tab[(size_t)m * 1024];
            const uint64_t q = rankOf(tb[s & 255] | tb[256 + (s >> 8 & 255)] | tb[512 + (s >> 16 & 255)] | tb[768 + (s >> 24)]);
            uint8_t cur = __atomic_load_n(&dist[q], __ATOMIC_RELAXED);
            if (cur == 0xFF && __atomic_compare_exchange_n(&dist[q], &cur, (uint8_t)(d + 1), false, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) c++;
          }
        }
      nNew += c;
    };
    std::vector<std::thread> th;
    for (int t = 0; t < T; t++) th.emplace_back(work);
    for (auto &x : th) x.join();
    fprintf(stderr, "  length %d: %ld new (%.0fs)\n", d + 1, nNew.load(), now() - t0);
    if (!nNew) break;
  }

  // classes under the 16 stage symmetries (those of the 48 that keep the U-D axis), as axbound
  std::vector<std::array<int, 32>> bp;
  std::vector<uint32_t> sx;
  {
    const int pm[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    for (int pi = 0; pi < 6; pi++)
      for (int sg = 0; sg < 8; sg++) {
        auto map = [&](const std::array<int, 3> &p) {
          std::array<int, 3> q;
          for (int r = 0; r < 3; r++) q[r] = p[pm[pi][r]] * ((sg >> r & 1) ? -1 : 1);
          return q;
        };
        int face[6], fac[48];
        for (int f = 0; f < 6; f++) {
          auto q = map(cc::Cube::normalOf(f));
          for (int f2 = 0; f2 < 6; f2++) if (cc::Cube::normalOf(f2) == q) face[f] = f2;
        }
        if (face[0] != 0 && face[0] != 5) continue;
        for (int k = 0; k < 48; k++) {
          auto q = map(C8.pos[pf[k]]);
          for (int x = 0; x < 48; x++) if (C8.pos[pf[x]] == q) fac[k] = x;
        }
        std::array<int, 32> b;
        for (int j = 0; j < 32; j++) b[j] = bitOf[fac[facOfBit[j]]];
        bp.push_back(b);
        sx.push_back((face[2] == 1 || face[2] == 3) ? 0xFFFFFFFFu : 0);
      }
  }
  auto canon = [&](uint32_t x) {
    uint32_t best = x;
    for (size_t s = 0; s < bp.size(); s++) {
      uint32_t t = 0;
      for (uint32_t y = x; y; y &= y - 1) t |= 1u << bp[s][__builtin_ctz(y)];
      best = std::min(best, t ^ sx[s]);
    }
    return best;
  };
  std::vector<std::map<int, long>> hc(T);
  {
    std::vector<std::thread> th;
    for (int t = 0; t < T; t++) th.emplace_back([&, t] {
      for (uint64_t r = t; r < total; r += T) { const uint32_t s = stateOf(r); if (canon(s) == s) hc[t][dist[r]]++; }
    });
    for (auto &x : th) x.join();
  }
  std::map<int, long> h, hcl;
  for (auto d : dist) h[d]++;
  for (auto &m : hc) for (auto &kv : m) hcl[kv.first] += kv.second;
  printf("3ax in <F,R,B,L,2U,3U,2D,3D> (%d moves), positions by length:", nm);
  for (auto &kv : h) printf(" %d:%ld", kv.first, kv.second);
  printf("\nclasses by length (%zu stage symmetries):", bp.size());
  for (auto &kv : hcl) printf(" %d:%ld", kv.first, kv.second);
  printf("\nworst %d (%.0fs)\n", h.rbegin()->first, now() - t0);
  if (!saveFile.empty()) {
    FILE *f = fopen(saveFile.c_str(), "wb");
    if (!f || fwrite(dist.data(), 1, total, f) != total) { fprintf(stderr, "cannot write %s\n", saveFile.c_str()); return 1; }
    fclose(f);
  }
  return 0;
}
