// cornerbfs: exact distances for the corners of a cube under the 18 outer face turns (quarter
// and half turns each count 1), orientation fixed (the centres): every one of the 8! 3^7 corner
// states.  Moves come from certcube.h (N = 3 facelets).  Prints the distance distribution.
//
//   cornerbfs
#include "certcube.h"
#include <algorithm>
#include <atomic>
#include <thread>

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  if (argc != 1) { fprintf(stderr, "usage: cornerbfs (no arguments)\n"); return 1; }
  cc::Cube C(3);
  // corner positions: group corner facelets by cubie (facelet centre minus half the normal)
  std::vector<std::array<int, 3>> cubie;
  std::vector<std::vector<int>> stick;  // per corner position: its 3 facelets, U/D facelet first, then in a fixed rotational order
  std::vector<int> cornerOf(C.pos.size(), -1), slotOf(C.pos.size(), -1);
  for (size_t i = 0; i < C.pos.size(); i++) {
    const int r = (i / 3) % 3, c = i % 3;
    if ((r != 0 && r != 2) || (c != 0 && c != 2)) continue;
    std::array<int, 3> cb = {C.pos[i][0] - C.nrm[i][0], C.pos[i][1] - C.nrm[i][1], C.pos[i][2] - C.nrm[i][2]};
    int k = std::find(cubie.begin(), cubie.end(), cb) - cubie.begin();
    if (k == (int)cubie.size()) { cubie.push_back(cb); stick.push_back({}); }
    stick[k].push_back(i);
  }
  if (cubie.size() != 8) { fprintf(stderr, "expected 8 corners\n"); return 1; }
  for (int k = 0; k < 8; k++) {
    auto &s = stick[k];
    std::sort(s.begin(), s.end(), [&](int a, int b) { return (C.nrm[a][1] != 0) > (C.nrm[b][1] != 0); });  // U/D first
    // order the other two so that det(n0, n1, n2) > 0 (rotations keep this)
    auto det = [&](int a, int b, int c) {
      const auto &x = C.nrm[a], &y = C.nrm[b], &z = C.nrm[c];
      return x[0] * (y[1] * z[2] - y[2] * z[1]) - x[1] * (y[0] * z[2] - y[2] * z[0]) + x[2] * (y[0] * z[1] - y[1] * z[0]);
    };
    if (det(s[0], s[1], s[2]) < 0) std::swap(s[1], s[2]);
    for (int j = 0; j < 3; j++) cornerOf[s[j]] = k, slotOf[s[j]] = j;
  }
  // per move: corner at position p goes to position mp[p] with its twist increased by dt[p]
  int mp[18][8], dt[18][8];
  for (int f = 0; f < 6; f++)
    for (int a = 1; a <= 3; a++) {
      const int m = f * 3 + a - 1;
      for (int p = 0; p < 8; p++) {
        int i = stick[p][0];
        for (int q = 0; q < a; q++) i = C.perms[f][0][i];
        mp[m][p] = cornerOf[i], dt[m][p] = slotOf[i];
        // the whole corner must move rigidly
        for (int j = 1; j < 3; j++) {
          int i2 = stick[p][j];
          for (int q = 0; q < a; q++) i2 = C.perms[f][0][i2];
          if (cornerOf[i2] != cornerOf[i] || slotOf[i2] != (slotOf[i] + j) % 3) { fprintf(stderr, "corner geometry mismatch\n"); return 1; }
        }
      }
    }
  // state: pos[piece], tw[piece] (twist: which slot holds the piece's reference facelet)
  static const int FACT[9] = {1, 1, 2, 6, 24, 120, 720, 5040, 40320};
  const uint64_t NP = 40320, NT = 2187, total = NP * NT;
  auto encode = [&](const int *pos, const int *tw) {
    uint64_t pr = 0;
    for (int i = 0; i < 8; i++) { int smaller = 0; for (int j = i + 1; j < 8; j++) smaller += pos[j] < pos[i]; pr += smaller * FACT[7 - i]; }
    uint64_t tr = 0;
    for (int i = 0; i < 7; i++) tr = tr * 3 + tw[i];
    return pr * NT + tr;
  };
  auto decode = [&](uint64_t x, int *pos, int *tw) {
    uint64_t pr = x / NT, tr = x % NT;
    int s = 0;
    for (int i = 6; i >= 0; i--) { tw[i] = tr % 3; tr /= 3; s += tw[i]; }
    tw[7] = (3 - s % 3) % 3;
    bool used[8] = {false};
    for (int i = 0; i < 8; i++) {
      int k = pr / FACT[7 - i];
      pr %= FACT[7 - i];
      for (int v = 0; v < 8; v++) if (!used[v]) { if (k-- == 0) { pos[i] = v; used[v] = true; break; } }
    }
  };
  std::vector<uint8_t> dist(total, 0xFF);
  int p0[8], t0[8] = {0};
  for (int i = 0; i < 8; i++) p0[i] = i;
  dist[encode(p0, t0)] = 0;
  const int threads = std::max(1u, std::thread::hardware_concurrency());
  uint64_t reached = 1;
  std::vector<uint64_t> hist = {1};
  for (int d = 0; reached < total; d++) {
    std::atomic<uint64_t> nx{0}, found{0};
    auto w = [&] {
      uint64_t f = 0;
      int pos[8], tw[8], np[8], nt[8];
      for (uint64_t c0; (c0 = nx.fetch_add(1 << 16)) < total;)
        for (uint64_t x = c0, xe = std::min(total, c0 + (1 << 16)); x < xe; x++) {
          if (dist[x] != d) continue;
          decode(x, pos, tw);
          for (int m = 0; m < 18; m++) {
            for (int i = 0; i < 8; i++) { np[i] = mp[m][pos[i]]; nt[i] = (tw[i] + dt[m][pos[i]]) % 3; }
            const uint64_t y = encode(np, nt);
            std::atomic_ref<uint8_t> ay(dist[y]);
            uint8_t cur = 0xFF;
            if (ay.load(std::memory_order_relaxed) == 0xFF && ay.compare_exchange_strong(cur, d + 1, std::memory_order_relaxed)) f++;
          }
        }
      found += f;
    };
    std::vector<std::thread> th;
    for (int t = 0; t < threads; t++) th.emplace_back(w);
    for (auto &t : th) t.join();
    if (!found) { printf("unreachable states remain: %llu\n", (unsigned long long)(total - reached)); return 1; }
    reached += found;
    hist.push_back(found);
  }
  printf("corner states by distance:");
  for (size_t d = 0; d < hist.size(); d++) printf(" %zu:%llu", d, (unsigned long long)hist[d]);
  printf("\nall %llu corner states within %zu face turns\n", (unsigned long long)total, hist.size() - 1);
  return 0;
}
