// colconcise: a concise form of a mid or diagonal stage certificate.
//
// The concise stage keeps a subset of the stage's lines.  It is expanded by a search: the actions are the
// 18 outer face turns and every kept line of at most --act moves, under every stage symmetry and
// inverted, each costing its move count; a Dijkstra search from the goal and from every kept line's
// positions (at their line lengths) gives every stage state a solving sequence (actions, then a kept
// line).  The certificate claims every state then gets at most the stage's worst length W.  Every
// action keeps the other centres as the stage requires because every certificate line does (midcheck,
// diagcheck), and so do outer face turns.
//
// Kept lines are chosen greedily: the short lines that serve as actions, then, in order of length, a
// line of any class that is still over W, after the search from everything kept so far.
//
//   colconcise mid|diag STAGEDIR OUTDIR [--act L] [--threads T]
#include "certcube.h"
#include "xcube.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <dirent.h>
#include <map>
#include <queue>
#include <random>
#include <thread>
#include <unordered_map>
#include <unordered_set>

int alwaysinmem = 1;
ll cancellations = 0;
ll runningmovecount = 0;
int writemoves = 0;
std::vector<mv> curblock;
ll wingmoves = 0;
ll blocks = 0;
void sendblock() { curblock.clear(); }
double duration() { return 0; }
static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static inline uint8_t mvByte(const cc::Mv &m) { return m.face << 4 | m.depth << 2 | m.amt; }
static inline cc::Mv byteMv(uint8_t b) { return {b >> 4, b >> 2 & 3, b & 3}; }
#include "certrest.h"


int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", i == 0 ? argv[i] : baseName(argv[i]).c_str());
  printf("\n");
  int T = 16, actLen = 6;
  std::vector<std::string> pos;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--act" && i + 1 < argc) actLen = atoi(argv[++i]);
    else if (o == "--threads" && i + 1 < argc) T = atoi(argv[++i]);
    else if (o.size() > 1 && o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else pos.push_back(o);
  }
  if (pos.size() != 3 || (pos[0] != "mid" && pos[0] != "diag")) { fprintf(stderr, "usage: colconcise mid|diag STAGEDIR OUTDIR [--act L] [--threads T]\n"); return 1; }
  const bool isMid = pos[0] == "mid";
  const double t0 = now();
  rng.seed(1);
  Orb24 O;
  {
    cc::Cube R(isMid ? 7 : 6);
    if (!O.init(isMid ? 7 : 6, isMid ? cc::midFacelets(R, 2) : cc::diagFacelets(R, 2), isMid ? 3 : 2)) { fprintf(stderr, "geometry failed\n"); return 1; }
  }
  ColStage st;
  if (!st.load(pos[1], O, T)) return 1;
  const int W = st.longest;
  Dense D;
  D.init(st.freeK, st.tg);
  const uint64_t total = D.total;
  printf("stage %s: %zu lines, worst %d, %llu states, %zu stage symmetries (%.0fs)\n", st.name.c_str(), st.off.size() - 1, W, (unsigned long long)total, st.ss.size(), now() - t0);
  fflush(stdout);
  // the stage's line lengths per state, from every line under every stage symmetry
  std::vector<uint8_t> L(total, 255);
  std::vector<uint32_t> lineOfState(total, ~0u);
  {
    const size_t nl = st.off.size() - 1;
    std::atomic<size_t> ni{0};
    auto w = [&] {
      int8_t a[24], b[24], c[24];
      for (size_t li; (li = ni++) < nl;) {
        for (int k = 0; k < 24; k++) a[k] = ((st.S | st.T) >> (k / 4) & 1) ? k / 4 : -1;
        const auto mv = st.lineAt(li);
        for (auto it = mv.rbegin(); it != mv.rend(); ++it) {
          for (int k = 0; k < 24; k++) b[O.mp[it->face][it->depth][4 - it->amt][k]] = a[k];
          memcpy(a, b, 24);
        }
        for (int s : st.ss) {
          const auto &y = O.syms[s];
          for (int k = 0; k < 24; k++) c[y.fac[k]] = a[k] < 0 ? -1 : y.gmap[a[k]];
          const uint64_t r = D.rank(c);
          L[r] = mv.size();
          lineOfState[r] = li;
        }
      }
    };
    std::vector<std::thread> th;
    for (int t = 0; t < T; t++) th.emplace_back(w);
    for (auto &t : th) t.join();
    uint64_t miss = 0;
    for (auto x : L) miss += x == 255;
    printf("lengths per state: %llu states without a line (%.0fs)\n", (unsigned long long)miss, now() - t0);
    if (miss) return 1;
  }
  // actions: face turns, and short lines under the stage symmetries, inverted too; one per distinct effect
  struct Act { std::vector<cc::Mv> mv; int dst[24]; };
  std::vector<Act> acts;
  std::vector<uint32_t> actLines;  // the lines the actions come from (kept)
  {
    std::unordered_map<std::string, int> seen;
    auto add = [&](const std::vector<cc::Mv> &mv) {
      Act a;
      a.mv = mv;
      for (int k = 0; k < 24; k++) a.dst[k] = k;
      for (auto &m : mv) for (int k = 0; k < 24; k++) a.dst[k] = O.mp[m.face][m.depth][m.amt][a.dst[k]];
      // keep the solved groups' position sets
      for (int k = 0; k < 24; k++) if ((st.S >> (k / 4) & 1) && a.dst[k] / 4 != k / 4) return;
      std::string key((const char *)a.dst, sizeof a.dst);
      auto it = seen.find(key);
      if (it == seen.end()) { seen[key] = acts.size(); acts.push_back(a); }
      else if (acts[it->second].mv.size() > mv.size()) acts[it->second] = a;
    };
    for (int f = 0; f < 6; f++) for (int a = 1; a <= 3; a++) add({{f, 1, a}});
    for (size_t li = 0; li + 1 < st.off.size(); li++) {
      const auto mv = st.lineAt(li);
      if (mv.empty() || (int)mv.size() > actLen) continue;
      actLines.push_back(li);
      for (size_t s = 0; s < O.syms.size(); s++) {
        auto m2 = symMoves24(O.syms[s], mv);
        add(m2);
        std::vector<cc::Mv> inv;
        for (auto it = m2.rbegin(); it != m2.rend(); ++it) inv.push_back({it->face, it->depth, 4 - it->amt});
        add(inv);
      }
    }
    // the identity (a line that does nothing to the orbit) is no use
    acts.erase(std::remove_if(acts.begin(), acts.end(), [](const Act &a) { for (int k = 0; k < 24; k++) if (a.dst[k] != k) return false; return true; }), acts.end());
    printf("actions: %zu (face turns and %zu lines of at most %d moves, symmetric images and inverses)\n", acts.size(), actLines.size(), actLen);
    fflush(stdout);
  }
  std::vector<int> src(acts.size() * 24);
  for (size_t a = 0; a < acts.size(); a++) for (int k = 0; k < 24; k++) src[a * 24 + acts[a].dst[k]] = k;
  // R: the expanded length per state; search from the goal and from the action lines (states solved by
  // a kept line at its length), predecessors through the inverse actions
  std::vector<uint8_t> R(total, 255), dirty(total, 0);
  auto relaxFrom = [&](int maxV) {  // level-synchronous Dijkstra over values 0..maxV
    for (int v = 0; v <= maxV; v++) {
      std::atomic<uint64_t> next{0};
      auto w = [&] {
        int8_t col[24], q[24];
        for (uint64_t lo; (lo = next.fetch_add(1 << 14)) < total;)
          for (uint64_t r = lo; r < std::min(total, lo + (1 << 14)); r++) {
            if (!dirty[r] || __atomic_load_n(&R[r], __ATOMIC_RELAXED) != v) continue;
            dirty[r] = 0;
            D.unrank(r, col, st.S);
            for (size_t a = 0; a < acts.size(); a++) {
              const int nv = v + acts[a].mv.size();
              if (nv > maxV) continue;
              const int *sa = &src[a * 24];
              for (int k = 0; k < 24; k++) q[sa[k]] = col[k];
              const uint64_t rq = D.rank(q);
              uint8_t old = __atomic_load_n(&R[rq], __ATOMIC_RELAXED);
              while (old > nv) {
                if (__atomic_compare_exchange_n(&R[rq], &old, (uint8_t)nv, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) { __atomic_store_n(&dirty[rq], (uint8_t)1, __ATOMIC_RELAXED); break; }
              }
            }
          }
      };
      std::vector<std::thread> th;
      for (int t = 0; t < T; t++) th.emplace_back(w);
      for (auto &t : th) t.join();
    }
  };
  std::vector<char> keep(st.off.size() - 1, 0);
  auto seedLine = [&](uint32_t li) {
    keep[li] = 1;
    int8_t a[24], b[24], c[24];
    for (int k = 0; k < 24; k++) a[k] = ((st.S | st.T) >> (k / 4) & 1) ? k / 4 : -1;
    const auto mv = st.lineAt(li);
    for (auto it = mv.rbegin(); it != mv.rend(); ++it) {
      for (int k = 0; k < 24; k++) b[O.mp[it->face][it->depth][4 - it->amt][k]] = a[k];
      memcpy(a, b, 24);
    }
    for (int s : st.ss) {
      const auto &y = O.syms[s];
      for (int k = 0; k < 24; k++) c[y.fac[k]] = a[k] < 0 ? -1 : y.gmap[a[k]];
      const uint64_t r = D.rank(c);
      if (R[r] > mv.size()) R[r] = mv.size(), dirty[r] = 1;
    }
  };
  {
    int8_t g[24];
    for (int k = 0; k < 24; k++) g[k] = ((st.S | st.T) >> (k / 4) & 1) ? k / 4 : -1;
    R[D.rank(g)] = 0;
    dirty[D.rank(g)] = 1;
  }
  for (auto li : actLines) seedLine(li);
  relaxFrom(W);
  auto overCount = [&] { uint64_t c = 0; for (auto x : R) c += x > W; return c; };
  uint64_t over = overCount();
  printf("search from the goal and the %zu action lines: %llu states over %d (%.0fs)\n", actLines.size(), (unsigned long long)over, W, now() - t0);
  fflush(stdout);
  // greedy: in rounds, keep the line of each class still over W whose line is shortest, then search again
  for (int round = 1; over; round++) {
    // the shortest line length among states still over W; keep every such class at that length
    int lmin = 255;
    for (uint64_t r = 0; r < total; r++) if (R[r] > W && L[r] < lmin) lmin = L[r];
    long added = 0;
    for (uint64_t r = 0; r < total; r++)
      if (R[r] > W && L[r] == lmin && !keep[lineOfState[r]]) { seedLine(lineOfState[r]); added++; }
    relaxFrom(W);
    over = overCount();
    long kept = 0;
    for (auto k : keep) kept += k;
    printf("round %d: kept %ld lines of length %d (%ld kept in all); %llu states over %d (%.0fs)\n", round, added, lmin, kept, (unsigned long long)over, W, now() - t0);
    fflush(stdout);
  }
  // write the kept lines, in the stage's segment files
  std::vector<std::pair<std::string, int>> files;
  listSegs(pos[1], files);
  const std::string outStage = pos[2] + "/stages/" + st.name;
  if (system(("mkdir -p '" + outStage + "'").c_str())) return 1;
  long kept = 0, bytes = 0;
  std::map<int, long> byLen;
  {
    // map line index -> segment: lines were loaded file by file in order
    size_t li = 0;
    for (auto &[fn, w] : files) {
      std::string data;
      readAll(fn, data);
      const auto sts = lineStarts(data);
      FILE *f = fopen((outStage + "/" + baseName(fn)).c_str(), "w");
      for (size_t i = 0; i < sts.size(); i++, li++)
        if (keep[li]) {
          const std::string t = lineAtPos(data, sts[i]);
          fprintf(f, "%s\n", t.c_str());
          kept++, bytes += t.size() + 1;
          byLen[st.lineAt(li).size()]++;
        }
      fclose(f);
    }
  }
  int worstR = 0;
  for (auto x : R) worstR = std::max<int>(worstR, x);
  printf("kept %ld of %zu lines (%.2f%%), %ld bytes; by length:", kept, st.off.size() - 1, 100.0 * kept / (st.off.size() - 1), bytes);
  for (auto &[l, n] : byLen) printf(" %d:%ld", l, n);
  printf("\nexpanded: every state at most %d (stage worst %d) (%.0fs)\n", worstR, W, now() - t0);
  return 0;
}
