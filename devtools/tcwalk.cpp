// tcwalk: oblique-pair algorithms for a centres-first solve, by direct enumeration.
//
// A sequence w of face, depth-2 and depth-3 turns keeps every other oblique orbit colour-solved
// iff both of its projections return the 5x5x5 T-centres to their faces: proj1 (w without
// its depth-3 turns) and proj2 (w without its depth-2 turns, 3 renamed 2).  (Checked against an
// 8x8x8 on 26,000 sequences and 167,618 re-interleaved pool algorithms.)  Diagonals, edges and
// corners are ignored.  tcwalk enumerates every such w up to MAXLEN moves:
//   - depth-first over the 54 moves, consecutive same-axis moves in one fixed order;
//   - pruning: exact distances to solved for every 5^3 T-centre colouring within --radius R
//     (each projection must be within the moves left), and the number of slice layers with a
//     nonzero net twist (every pool algorithm has all twists 0; requiring it loses at most
//     parity-style algorithms);
//   - "cores" only: no face turn in the first or last same-axis run (face turns at the ends are
//     free in a centres-first solve: orbitbound --faceturns);
//   - one sequence per class under the 48 symmetries x inverse, then the cheapest per class of
//     its effect on the (2,3)/(3,2) pair, written as .cls lines flagged D1.
//
//   tcwalk MAXLEN [--radius R] [--threads T] > out.cls
#include "classpool.h"
#include <atomic>
#include <chrono>
#include <cstring>
#include <map>
#include <mutex>
#include <thread>
#include <unordered_map>

typedef unsigned __int128 u128;
static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

static int t5[36][24];  // 5^3: T-centre p goes to t5[m][p], m = (face*2 + depth-1)*3 + amt-1
struct St { uint8_t c[24]; };
static inline u128 key(const St &s) { u128 k = 0; for (int i = 0; i < 24; i++) k = k << 3 | s.c[i]; return k; }
struct KH { size_t operator()(u128 k) const { uint64_t a = (uint64_t)k, b = (uint64_t)(k >> 64); return (a ^ (b * 0x9E3779B97F4A7C15ULL)) * 0xBF58476D1CE4E5B9ULL; } };
static inline void ap(const St &s, int m, St &t) { for (int p = 0; p < 24; p++) t.c[t5[m][p]] = s.c[p]; }

// w moves: (face, depth 1..3, amt); index (face*3 + depth-1)*3 + amt-1
static int wFace[54], wDepth[54], wAmt[54], wLayer[54], wAxis[54], wOrd[54], wP1[54], wP2[54];
static int symFace[48][6], symDet[48];
static int wIndex(int f, int d, int a) { return (f * 3 + d - 1) * 3 + a - 1; }

static void canon(std::vector<int> &s) {
  for (size_t i = 0; i < s.size();) {
    size_t j = i + 1;
    while (j < s.size() && wAxis[s[j]] == wAxis[s[i]]) j++;
    std::sort(s.begin() + i, s.begin() + j, [](int a, int b) { return wOrd[a] < wOrd[b]; });
    i = j;
  }
}
static bool isRep(const std::vector<int> &s) {
  std::vector<int> t(s.size());
  for (int inv = 0; inv < 2; inv++)
    for (int y = 0; y < 48; y++) {
      if (!inv && y == 0) continue;
      for (size_t i = 0; i < s.size(); i++) {
        const int m = inv ? s[s.size() - 1 - i] : s[i];
        int a = wAmt[m];
        if (inv) a = 4 - a;
        if (symDet[y] < 0) a = 4 - a;
        t[i] = wIndex(symFace[y][wFace[m]], wDepth[m], a);
      }
      canon(t);
      if (t < s) return false;
    }
  return true;
}

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  int R = 5, threads = std::max(1u, std::thread::hardware_concurrency());
  std::vector<std::string> pos;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--radius" && i + 1 < argc) R = atoi(argv[++i]);
    else if (o == "--threads" && i + 1 < argc) threads = atoi(argv[++i]);
    else if (o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else pos.push_back(o);
  }
  if (pos.size() != 1) { fprintf(stderr, "usage: tcwalk MAXLEN [--radius R] [--threads T] > out.cls\n"); return 1; }
  const int maxLen = atoi(pos[0].c_str());
  const double t0 = now();
  // 5^3 T-centres
  Cube c(5);
  std::vector<int> tpos, tidx(c.pos.size(), -1);
  for (size_t i = 0; i < c.pos.size(); i++) {
    const int r = c.drow(i), k = c.dcol(i);
    if ((r == 2 && k == 3) || (r == 3 && k == 2)) { tidx[i] = tpos.size(); tpos.push_back(i); }
  }
  if (tpos.size() != 24) { fprintf(stderr, "expected 24 T-centres\n"); return 1; }
  for (int f = 0; f < 6; f++)
    for (int d = 1; d <= 2; d++)
      for (int a = 1; a <= 3; a++)
        for (int p = 0; p < 24; p++) {
          int s = tpos[p];
          for (int q = 0; q < a; q++) s = c.movePerm[f][d - 1][s];
          t5[(f * 2 + d - 1) * 3 + a - 1][p] = tidx[s];
        }
  St solved;
  for (int p = 0; p < 24; p++) solved.c[p] = c.face[tpos[p]];
  const u128 solvedKey = key(solved);
  // w moves; along an axis: face, 2, 3 from one side, then 3, 2, face from the other
  const int axisOf[6] = {0, 1, 2, 1, 2, 0}, firstSide[6] = {1, 1, 1, 0, 0, 0};
  for (int f = 0; f < 6; f++)
    for (int d = 1; d <= 3; d++)
      for (int a = 1; a <= 3; a++) {
        const int m = wIndex(f, d, a);
        wFace[m] = f, wDepth[m] = d, wAmt[m] = a, wLayer[m] = f * 3 + d - 1, wAxis[m] = axisOf[f];
        wOrd[m] = firstSide[f] ? d - 1 : 5 - (d - 1);
        wP1[m] = d == 3 ? -1 : (f * 2 + d - 1) * 3 + a - 1;
        wP2[m] = d == 2 ? -1 : (f * 2 + (d == 1 ? 0 : 1)) * 3 + a - 1;
      }
  {
    const int perms[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}}, sign[6] = {1, -1, -1, 1, 1, -1};
    int y = 0;
    for (int pi = 0; pi < 6; pi++)
      for (int s = 0; s < 8; s++, y++) {
        for (int f = 0; f < 6; f++) {
          int v[3];
          for (int r = 0; r < 3; r++) v[r] = NORMAL[f][perms[pi][r]] * ((s >> r) & 1 ? -1 : 1);
          symFace[y][f] = face_of_normal(v[0], v[1], v[2]);
        }
        symDet[y] = sign[pi] * (__builtin_popcount(s) & 1 ? -1 : 1);
      }
    if (symFace[0][1] != 1 || symFace[0][5] != 5 || symDet[0] != 1) { fprintf(stderr, "symmetry 0 is not the identity\n"); return 1; }
  }
  // colourings within R moves (36 moves of the 5^3)
  std::unordered_map<u128, uint8_t, KH> near;
  {
    std::vector<St> frontier{solved};
    near[solvedKey] = 0;
    for (int d = 1; d <= R; d++) {
      std::vector<St> next;
      for (auto &s : frontier)
        for (int m = 0; m < 36; m++) {
          St t;
          ap(s, m, t);
          if (near.emplace(key(t), d).second) next.push_back(t);
        }
      fprintf(stderr, "radius %d: %zu new colourings (%.1fs)\n", d, next.size(), now() - t0);
      frontier.swap(next);
    }
  }
  auto dist = [&](const St &s) { auto it = near.find(key(s)); return it == near.end() ? R + 1 : (int)it->second; };
  OrbitModel model = pairModel();
  // roots: the first two moves (a core starts with a slice; no face in its first run)
  std::vector<std::pair<int, int>> roots;
  for (int a = 0; a < 54; a++) {
    if (wDepth[a] == 1) continue;
    for (int b = 0; b < 54; b++) {
      if (wLayer[b] == wLayer[a] || (wAxis[b] == wAxis[a] && (wOrd[b] < wOrd[a] || wDepth[b] == 1))) continue;
      roots.push_back({a, b});
    }
  }
  struct H { size_t operator()(const std::array<uint8_t, 48> &v) const { size_t h = 1469598103934665603ULL; for (auto x : v) h = (h ^ x) * 1099511628211ULL; return h; } };
  std::unordered_map<std::array<uint8_t, 48>, std::pair<int, std::string>, H> best;
  std::vector<long> reps(maxLen + 1, 0);
  std::map<std::string, std::pair<int, std::string>> cyc;  // cycle structure -> shortest (length, sequence)
  std::mutex cmu;
  std::atomic<size_t> nextRoot{0};
  std::atomic<long> nodes{0};
  std::mutex mu;
  auto work = [&] {
    std::unordered_map<std::array<uint8_t, 48>, std::pair<int, std::string>, H> mine;
    std::vector<long> myReps(maxLen + 1, 0);
    long nd = 0;
    std::vector<int> seq;
    std::vector<St> s1(maxLen + 1), s2(maxLen + 1);
    int tw[18];
    std::function<void(int)> dfs = [&](int depth) {
      nd++;
      const int last = seq.back();
      int nz = 0;
      for (int l = 0; l < 18; l++) nz += (l % 3) && tw[l];
      bool lastRunPure = true;
      for (int i = depth - 1; i >= 0 && wAxis[seq[i]] == wAxis[last]; i--) lastRunPure &= wDepth[seq[i]] != 1;
      if (nz == 0 && lastRunPure && key(s1[depth]) == solvedKey && key(s2[depth]) == solvedKey && isRep(seq)) {
        myReps[depth]++;
        std::vector<Move> w;
        for (int m : seq) w.push_back(Move{(uint8_t)wFace[m], (uint8_t)wDepth[m], (uint8_t)wAmt[m]});
        auto p = model.permOf(w);
        bool id = true;
        for (int k = 0; k < 48; k++) id &= p[k] == k;
        if (!id) {
          // cycle structure per orbit (cycles never leave an orbit)
          std::string half[2];
          bool seenp[48] = {};
          for (int o = 0; o < 2; o++) {
            std::vector<int> lens;
            for (int k = 24 * o; k < 24 * o + 24; k++) {
              if (seenp[k] || p[k] == k) continue;
              int n = 0;
              for (int x = k; !seenp[x]; x = p[x]) seenp[x] = true, n++;
              lens.push_back(n);
            }
            std::sort(lens.rbegin(), lens.rend());
            if (lens.empty()) half[o] = " -";
            for (int n : lens) half[o] += " " + std::to_string(n);
          }
          // a reflection swaps the two orbits: list the larger half first
          if (half[0] < half[1]) std::swap(half[0], half[1]);
          const std::string cs = half[0].substr(1) + " |" + half[1];
          {
            std::lock_guard<std::mutex> g(cmu);
            auto it = cyc.find(cs);
            if (it == cyc.end() || it->second.first > depth) {
              std::string t;
              for (auto &m : w) t += (t.empty() ? "" : " ") + move_str(m);
              fprintf(stderr, "%s cycle structure [%s] at %d moves: %s\n", it == cyc.end() ? "new" : "shorter", cs.c_str(), depth, t.c_str());
              cyc[cs] = {depth, t};
            }
          }
          std::array<uint8_t, 48> ck;
          model.classKey(p.data(), ck.data());
          auto it = mine.find(ck);
          if (it == mine.end() || it->second.first > depth) {
            std::string t;
            for (auto &m : w) t += (t.empty() ? "" : " ") + move_str(m);
            mine[ck] = {depth, t};
          }
        }
      }
      if (depth == maxLen) return;
      bool firstRun = true;
      for (int i = 0; i < depth; i++) firstRun &= wAxis[seq[i]] == wAxis[seq[0]];
      const int rem = maxLen - depth - 1;
      for (int m = 0; m < 54; m++) {
        if (wLayer[m] == wLayer[last] || (wAxis[m] == wAxis[last] && wOrd[m] < wOrd[last])) continue;
        if (firstRun && wAxis[m] == wAxis[seq[0]] && wDepth[m] == 1) continue;
        const int l = wLayer[m];
        const bool sl = wDepth[m] != 1;
        const int before = sl ? tw[l] : 0, after = sl ? (tw[l] + wAmt[m]) & 3 : 0;
        if (nz - (before != 0) + (after != 0) > rem) continue;
        if (wP1[m] >= 0) ap(s1[depth], wP1[m], s1[depth + 1]); else s1[depth + 1] = s1[depth];
        if (dist(s1[depth + 1]) > rem) continue;
        if (wP2[m] >= 0) ap(s2[depth], wP2[m], s2[depth + 1]); else s2[depth + 1] = s2[depth];
        if (dist(s2[depth + 1]) > rem) continue;
        if (sl) tw[l] = after;
        seq.push_back(m);
        dfs(depth + 1);
        seq.pop_back();
        if (sl) tw[l] = before;
      }
    };
    for (size_t i; (i = nextRoot++) < roots.size();) {
      const auto [a, b] = roots[i];
      if (maxLen < 2) break;
      memset(tw, 0, sizeof tw);
      seq.assign({a});
      s1[0] = s2[0] = solved;
      if (wP1[a] >= 0) ap(s1[0], wP1[a], s1[1]); else s1[1] = s1[0];
      if (wP2[a] >= 0) ap(s2[0], wP2[a], s2[1]); else s2[1] = s2[0];
      tw[wLayer[a]] = wAmt[a];
      const int rem = maxLen - 2;
      if (wP1[b] >= 0) ap(s1[1], wP1[b], s1[2]); else s1[2] = s1[1];
      if (wP2[b] >= 0) ap(s2[1], wP2[b], s2[2]); else s2[2] = s2[1];
      if (dist(s1[2]) > rem || dist(s2[2]) > rem) continue;
      if (wDepth[b] != 1) tw[wLayer[b]] = (tw[wLayer[b]] + wAmt[b]) & 3;
      int nz = 0;
      for (int l = 0; l < 18; l++) nz += (l % 3) && tw[l];
      if (nz > rem) continue;
      seq.push_back(b);
      dfs(2);
    }
    std::lock_guard<std::mutex> g(mu);
    for (auto &kv : mine) {
      auto it = best.find(kv.first);
      if (it == best.end() || it->second.first > kv.second.first) best[kv.first] = kv.second;
    }
    for (int d = 0; d <= maxLen; d++) reps[d] += myReps[d];
    nodes += nd;
  };
  std::vector<std::thread> th;
  for (int t = 0; t < threads; t++) th.emplace_back(work);
  for (auto &t : th) t.join();
  std::map<int, long> byCost;
  for (auto &kv : best) { printf("%d %s D1\n", kv.second.first, kv.second.second.c_str()); byCost[kv.second.first]++; }
  fprintf(stderr, "%zu cycle structures (shortest):\n", cyc.size());
  std::vector<std::pair<int, std::string>> cl;
  for (auto &kv : cyc) cl.push_back({kv.second.first, kv.first});
  std::sort(cl.begin(), cl.end());
  for (auto &x : cl) fprintf(stderr, "  %2d  [%s]  %s\n", x.first, x.second.c_str(), cyc[x.second].second.c_str());
  fprintf(stderr, "sequence classes:");
  for (int d = 0; d <= maxLen; d++) if (reps[d]) fprintf(stderr, " %d:%ld", d, reps[d]);
  fprintf(stderr, "\n%zu permutation classes:", best.size());
  for (auto &kv : byCost) fprintf(stderr, " %d:%ld", kv.first, kv.second);
  fprintf(stderr, "; %ld nodes, %.1fs\n", nodes.load(), now() - t0);
}
