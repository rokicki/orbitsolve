// tcenum: every move sequence on the 5x5x5 T-centres (faces and depth-2 slices, 36 moves)
// that returns every T-centre to its own face, keeping the other centre orbits of a
// centres-first oblique solve colour-solved (see upperbound.md).  Diagonals (X-centres),
// edges and corners are ignored.
//
// Output: "cores" only (first and last moves are slices; face turns at the ends are free in a
// centres-first solve), every slice layer with net twist 0 mod 4, consecutive same-axis moves
// in one fixed order, one sequence per class under the 48 cube symmetries x inverse (the
// least image).  Pruning: exact distances to solved for every colouring within --radius R
// moves, and the number of slice layers with a nonzero twist.
//
//   tcenum MAXLEN [--radius R] [--threads T]     (run from anywhere; writes text to stdout)
#include "cube.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

typedef unsigned __int128 u128;
static int NM = 36;
static int perm[36][24];        // T-centre position p goes to perm[m][p]
static int mFace[36], mDepth[36], mAmt[36], mLayer[36], mAxis[36], mOrd[36];
static int symFace[48][6], symDet[48];

static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

struct St { uint8_t c[24]; };
static inline u128 key(const St &s) { u128 k = 0; for (int i = 0; i < 24; i++) k = k << 3 | s.c[i]; return k; }
struct KH { size_t operator()(u128 k) const { uint64_t a = (uint64_t)k, b = (uint64_t)(k >> 64); return (a ^ (b * 0x9E3779B97F4A7C15ULL)) * 0xBF58476D1CE4E5B9ULL; } };
static inline void apply(const St &s, int m, St &t) { for (int p = 0; p < 24; p++) t.c[perm[m][p]] = s.c[p]; }

static std::unordered_map<u128, uint8_t, KH> near;  // colourings within the radius: distance
static int R = 5, maxLen;
static St solved;
static u128 solvedKey;

static int moveIndex(int f, int d, int a) { return (f * 2 + d - 1) * 3 + a - 1; }
static std::string text(const std::vector<int> &s) {
  std::string t;
  for (int m : s) {
    if (!t.empty()) t += ' ';
    if (mDepth[m] == 2) t += '2';
    t += "ULFRBD"[mFace[m]];
    t += mAmt[m] == 1 ? "" : mAmt[m] == 2 ? "2" : "'";
  }
  return t;
}
// same-axis runs sorted into the fixed order (moves of one axis commute)
static void canon(std::vector<int> &s) {
  for (size_t i = 0; i < s.size();) {
    size_t j = i + 1;
    while (j < s.size() && mAxis[s[j]] == mAxis[s[i]]) j++;
    std::sort(s.begin() + i, s.begin() + j, [](int a, int b) { return mOrd[a] < mOrd[b]; });
    i = j;
  }
}
// least image under the 48 symmetries x inverse; true if s itself is least
static bool isRep(const std::vector<int> &s) {
  std::vector<int> t(s.size());
  for (int inv = 0; inv < 2; inv++)
    for (int y = 0; y < 48; y++) {
      if (!inv && y == 0) continue;
      for (size_t i = 0; i < s.size(); i++) {
        const int m = inv ? s[s.size() - 1 - i] : s[i];
        int a = mAmt[m];
        if (inv) a = 4 - a;
        if (symDet[y] < 0) a = 4 - a;
        t[i] = moveIndex(symFace[y][mFace[m]], mDepth[m], a);
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
  int threads = std::max(1u, std::thread::hardware_concurrency());
  std::vector<std::string> pos;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--radius" && i + 1 < argc) R = atoi(argv[++i]);
    else if (o == "--threads" && i + 1 < argc) threads = atoi(argv[++i]);
    else if (o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else pos.push_back(o);
  }
  if (pos.size() != 1) { fprintf(stderr, "usage: tcenum MAXLEN [--radius R] [--threads T]\n"); return 1; }
  maxLen = atoi(pos[0].c_str());
  // T-centre positions on 5^3 and the 36 moves
  Cube c(5);
  std::vector<int> tpos, tidx(c.pos.size(), -1);
  for (size_t i = 0; i < c.pos.size(); i++) {
    const int r = c.drow(i), k = c.dcol(i);
    if ((r == 2 && k == 3) || (r == 3 && k == 2)) { tidx[i] = tpos.size(); tpos.push_back(i); }
  }
  if (tpos.size() != 24) { fprintf(stderr, "expected 24 T-centres\n"); return 1; }
  const int axisOf[6] = {0, 1, 2, 1, 2, 0}, ordOf[6] = {0, 0, 0, 3, 3, 3};  // U L F first, D R B last
  for (int f = 0; f < 6; f++)
    for (int d = 1; d <= 2; d++)
      for (int a = 1; a <= 3; a++) {
        const int m = moveIndex(f, d, a);
        mFace[m] = f, mDepth[m] = d, mAmt[m] = a, mLayer[m] = f * 2 + d - 1, mAxis[m] = axisOf[f];
        mOrd[m] = ordOf[f] == 0 ? d - 1 : 3 - (d - 1);  // along the axis: face, slice, slice, face
        for (int p = 0; p < 24; p++) {
          int s = tpos[p];
          for (int q = 0; q < a; q++) s = c.movePerm[f][d - 1][s];
          perm[m][p] = tidx[s];
        }
      }
  for (int p = 0; p < 24; p++) solved.c[p] = c.face[tpos[p]];
  solvedKey = key(solved);
  // symmetries: signed permutation matrices -> face map, handedness
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
  // breadth-first table of the colourings within R moves
  double t0 = now();
  {
    std::vector<St> frontier{solved};
    near[solvedKey] = 0;
    for (int d = 1; d <= R; d++) {
      std::vector<St> next;
      for (auto &s : frontier)
        for (int m = 0; m < NM; m++) {
          St t;
          apply(s, m, t);
          if (near.emplace(key(t), d).second) next.push_back(t);
        }
      fprintf(stderr, "radius %d: %zu new colourings (%.1fs)\n", d, next.size(), now() - t0);
      frontier.swap(next);
    }
  }
  // depth-first enumeration, split over threads by the first two moves
  std::vector<std::pair<int, int>> roots;
  for (int a = 0; a < NM; a++) {
    if (mDepth[a] != 2) continue;  // cores start with a slice
    for (int b = -1; b < NM; b++) {
      if (b >= 0 && (mLayer[b] == mLayer[a] || (mAxis[b] == mAxis[a] && (mOrd[b] < mOrd[a] || mDepth[b] == 1)))) continue;
      roots.push_back({a, b});
    }
  }
  std::atomic<size_t> next{0};
  std::vector<long> count(maxLen + 1, 0);
  std::atomic<long> nodes{0};
  std::mutex mu;
  auto work = [&] {
    std::vector<long> cnt(maxLen + 1, 0);
    long nd = 0;
    std::vector<int> seq;
    std::vector<St> st(maxLen + 1);
    int tw[12];
    std::string out;
    std::function<void(int)> dfs = [&](int depth) {
      nd++;
      const St &s = st[depth];
      const int last = seq.back();
      int nz = 0;
      for (int l = 0; l < 12; l++) nz += (l & 1) && tw[l];
      // a core: no face turn in the first or last same-axis run (it would commute to the end)
      bool lastRunPure = true;
      for (int i = depth - 1; i >= 0 && mAxis[seq[i]] == mAxis[last]; i--) lastRunPure &= mDepth[seq[i]] == 2;
      if (nz == 0 && lastRunPure && key(s) == solvedKey) {
        if (isRep(seq)) {
          cnt[depth]++;
          out += std::to_string(depth) + " " + text(seq) + "\n";
          if (out.size() > (1 << 20)) { std::lock_guard<std::mutex> g(mu); fputs(out.c_str(), stdout); out.clear(); }
        }
      }
      if (depth == maxLen) return;
      bool firstRun = true;
      for (int i = 0; i < depth; i++) firstRun &= mAxis[seq[i]] == mAxis[seq[0]];
      for (int m = 0; m < NM; m++) {
        if (mLayer[m] == mLayer[last] || (mAxis[m] == mAxis[last] && mOrd[m] < mOrd[last])) continue;
        if (firstRun && mAxis[m] == mAxis[seq[0]] && mDepth[m] == 1) continue;
        const int rem = maxLen - depth - 1;
        const int l = mLayer[m];
        const bool sl = l & 1;
        const int before = sl ? tw[l] : 0, after = sl ? (tw[l] + mAmt[m]) & 3 : 0;
        const int nz2 = nz - (before != 0) + (after != 0);
        if (nz2 > rem) continue;
        apply(s, m, st[depth + 1]);
        auto it = near.find(key(st[depth + 1]));
        const int dist = it == near.end() ? R + 1 : it->second;
        if (dist > rem) continue;
        if (sl) tw[l] = after;
        seq.push_back(m);
        dfs(depth + 1);
        seq.pop_back();
        if (sl) tw[l] = before;
      }
    };
    for (size_t i; (i = next++) < roots.size();) {
      const auto [a, b] = roots[i];
      memset(tw, 0, sizeof tw);
      seq.assign(1, a);
      apply(solved, a, st[1]);
      tw[mLayer[a]] = mAmt[a];
      if (b < 0 || maxLen < 2) continue;  // one slice turn never restores the T-centres
      apply(st[1], b, st[2]);
      const int rem = maxLen - 2;
      auto it = near.find(key(st[2]));
      if ((it == near.end() ? R + 1 : it->second) > rem) continue;
      if (mLayer[b] & 1) tw[mLayer[b]] = (tw[mLayer[b]] + mAmt[b]) & 3;
      seq.push_back(b);
      dfs(2);
    }
    std::lock_guard<std::mutex> g(mu);
    fputs(out.c_str(), stdout);
    for (int d = 0; d <= maxLen; d++) count[d] += cnt[d];
    nodes += nd;
  };
  std::vector<std::thread> th;
  for (int t = 0; t < threads; t++) th.emplace_back(work);
  for (auto &t : th) t.join();
  fflush(stdout);
  long tot = 0;
  for (int d = 0; d <= maxLen; d++) tot += count[d];
  fprintf(stderr, "%ld core classes up to %d moves:", tot, maxLen);
  for (int d = 0; d <= maxLen; d++) if (count[d]) fprintf(stderr, " %d:%ld", d, count[d]);
  fprintf(stderr, "; %ld nodes, %.1fs\n", nodes.load(), now() - t0);
}
