// wingcheck: verify wing stage certificates (written by wingbound CERT=DIR).
//
// A certificate segment is a file .../stages/<placed>..<target>/wNN.txt: one move sequence per
// line, nothing else.  Wings are distinct pieces, named 'a' + k in the order of certcube.h's
// wingFacelets (one facelet per wing of the orbit at the slice depth); <placed> and <target>
// list wing letters ("none" = no wing).  The stage keeps the placed wings at home and brings the
// target wings home; the other wings may go anywhere.  Each line's position is the one its
// sequence solves: the sequence run backwards from the solved stage.  NN is the number of target
// wings not at home.  Moves are outer faces and depth-2 slices; depth 2 stands for the wing
// orbit's slice depth.  This program uses nothing from the solver (certcube.h only).  It checks:
//   1. every line: legal moves, every placed wing stays at home, its position has NN target
//      wings away from home;
//   2. coverage: no two lines give the same position, and the lines give every stage position
//      (n! / (n-k)! for k target wings among n free positions; with every segment given);
//   3. side effects, on every N in 6..MAXN and every wing depth d in 2..N/2 (depth 2 of the
//      sequence turned into depth d): every centre facelet keeps its colour, every corner, middle
//      edge and other-depth wing facelet stays in place, and the wings of the depth-d orbit move
//      exactly as on the 6x6x6 reference.
//
//   wingcheck PATH... [--threads T] [--maxn N]
//     PATH: a segment file, or a stage directory (every w*.txt in it)
#include "certcube.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <map>
#include <mutex>
#include <thread>

static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

static int wingMask(const std::string &s) {
  if (s == "none") return 0;
  int m = 0;
  for (char ch : s) {
    if (ch < 'a' || ch > 'x' || (m >> (ch - 'a') & 1)) return -1;
    m |= 1 << (ch - 'a');
  }
  return m;
}

// one cube size and wing depth: per move the facelet cycles, and the facelet classes
struct Run {
  int N, d;
  cc::Cube C;
  std::vector<std::vector<int>> cyc;  // [(face * 3 + depthSlot) * 4 + amt]: flattened cycles, each "len, f1..flen"
  std::vector<char> kind;             // 0 centre, 1 depth-d wing facelet, 2 other (must stay)
  std::vector<int> wingAt;            // facelet -> wing index (wingFacelets of depth d), or -1
  std::vector<int> wf;
  Run(int n, int dd) : N(n), d(dd), C(n) {
    const int M = N - 1, F = C.pos.size();
    kind.assign(F, 2);
    wingAt.assign(F, -1);
    wf = cc::wingFacelets(C, d);
    for (int k = 0; k < 24; k++) wingAt[wf[k]] = k;
    for (int f = 0; f < 6; f++)
      for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++) {
          const int i = C.index(f, r, c);
          if (r > 0 && r < M && c > 0 && c < M) { kind[i] = 0; continue; }
          const bool er = r == 0 || r == M, ec = c == 0 || c == M;
          const int along = er ? c : r;
          if (!(er && ec) && (along == d - 1 || along == N - d)) kind[i] = 1;
        }
    cyc.assign(6 * 3 * 4, {});
    for (int f = 0; f < 6; f++)
      for (int slot = 1; slot <= 2; slot++)
        for (int a = 1; a <= 3; a++) {
          const auto &pm = C.perms[f][(slot == 1 ? 1 : d) - 1];
          std::vector<int> p(F);
          for (int i = 0; i < F; i++) { int j = i; for (int q = 0; q < a; q++) j = pm[j]; p[i] = j; }
          std::vector<char> seen(F, 0);
          auto &out = cyc[(f * 3 + slot) * 4 + a];
          for (int i = 0; i < F; i++) {
            if (seen[i] || p[i] == i) continue;
            std::vector<int> cy;
            for (int j = i; !seen[j]; j = p[j]) seen[j] = 1, cy.push_back(j);
            out.push_back(cy.size());
            out.insert(out.end(), cy.begin(), cy.end());
          }
        }
  }
  // content array a (a[j] = original facelet now at j); returns true if the side conditions hold
  // and the depth-d wings move as ref (dst form over wing indices)
  bool check(const std::vector<cc::Mv> &mv, const int *ref, std::vector<int> &a) const {
    const int F = C.pos.size();
    a.resize(F);
    for (int i = 0; i < F; i++) a[i] = i;
    for (auto &m : mv) {
      const auto &cy = cyc[(m.face * 3 + m.depth) * 4 + m.amt];
      for (size_t q = 0; q < cy.size();) {
        const int len = cy[q];
        const int *f = &cy[q + 1];
        // content at f[t] moves to f[t+1]
        int carry = a[f[len - 1]];
        for (int t = len - 1; t > 0; t--) a[f[t]] = a[f[t - 1]];
        a[f[0]] = carry;
        q += len + 1;
      }
    }
    for (int j = 0; j < F; j++) {
      const int src = a[j];
      if (kind[j] == 0) { if (C.faceOf[src] != C.faceOf[j]) return false; }
      else if (kind[j] == 2) { if (src != j) return false; }
    }
    for (int k = 0; k < 24; k++) {  // the wing whose facelet is now at wf[ref[k]] must be wing k
      if (wingAt[a[wf[ref[k]]]] != k) return false;
    }
    return true;
  }
};

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  int threads = std::max(1u, std::thread::hardware_concurrency()), maxN = 10;
  std::vector<std::string> paths;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--threads" && i + 1 < argc) threads = atoi(argv[++i]);
    else if (o == "--maxn" && i + 1 < argc) maxN = atoi(argv[++i]);
    else if (o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else paths.push_back(o);
  }
  if (paths.empty()) { fprintf(stderr, "usage: wingcheck PATH... [--threads T] [--maxn N]\n"); return 1; }
  if (maxN < 6) { fprintf(stderr, "--maxn must be at least 6\n"); return 1; }
  const double t0 = now();
  std::vector<std::pair<std::string, int>> files;
  std::string stageDir;
  for (auto p : paths) {
    while (p.size() > 1 && p.back() == '/') p.pop_back();
    std::vector<std::string> fs;
    if (DIR *dd = opendir(p.c_str())) {
      while (dirent *e = readdir(dd)) {
        std::string n = e->d_name;
        if (n.size() > 5 && n[0] == 'w' && n.compare(n.size() - 4, 4, ".txt") == 0) fs.push_back(p + "/" + n);
      }
      closedir(dd);
      std::sort(fs.begin(), fs.end());
    } else fs.push_back(p);
    for (auto &f : fs) {
      const size_t sl = f.rfind('/');
      const std::string dir = sl == std::string::npos ? "." : f.substr(0, sl), base = f.substr(sl == std::string::npos ? 0 : sl + 1);
      if (base.size() < 6 || base[0] != 'w' || base.compare(base.size() - 4, 4, ".txt")) { fprintf(stderr, "not a segment file (wNN.txt): %s\n", base.c_str()); return 1; }
      if (stageDir.empty()) stageDir = dir;
      else if (stageDir != dir) { fprintf(stderr, "segments from more than one stage\n"); return 1; }
      files.push_back({f, atoi(base.c_str() + 1)});
    }
  }
  const std::string sname = stageDir.substr(stageDir.rfind('/') == std::string::npos ? 0 : stageDir.rfind('/') + 1);
  const size_t dd = sname.find("..");
  if (dd == std::string::npos) { fprintf(stderr, "stage directory must be named <placed>..<target>\n"); return 1; }
  const int S = wingMask(sname.substr(0, dd)), T = wingMask(sname.substr(dd + 2));
  if (S < 0 || T <= 0 || (S & T)) { fprintf(stderr, "bad stage name %s\n", sname.c_str()); return 1; }
  std::vector<int> tg, freeP;
  for (int k = 0; k < 24; k++) { if (T >> k & 1) tg.push_back(k); if (!(S >> k & 1)) freeP.push_back(k); }
  const int n = freeP.size(), K = tg.size();
  int freeIdx[24];
  for (int k = 0; k < 24; k++) freeIdx[k] = -1;
  for (int i = 0; i < n; i++) freeIdx[freeP[i]] = i;
  uint64_t total = 1;
  for (int i = 0; i < K; i++) total *= n - i;
  printf("stage %s: %d wings kept, %d target wings among %d free positions (%llu positions); %zu segment files\n", sname.c_str(), __builtin_popcount(S), K, n,
         (unsigned long long)total, files.size());
  // reference: 6x6x6, depth 2; wing moves in dst form
  cc::Cube C6(6);
  const auto wf6 = cc::wingFacelets(C6, 2);
  std::vector<int> in6(C6.pos.size(), -1);
  for (int k = 0; k < 24; k++) in6[wf6[k]] = k;
  static int mw[6][3][4][24];
  for (int f = 0; f < 6; f++)
    for (int dp = 1; dp <= 2; dp++)
      for (int a = 1; a <= 3; a++)
        for (int k = 0; k < 24; k++) {
          int i = wf6[k];
          for (int q = 0; q < a; q++) i = C6.perms[f][dp - 1][i];
          if (in6[i] < 0) { fprintf(stderr, "a move takes a wing facelet out of the orbit\n"); return 1; }
          mw[f][dp][a][k] = in6[i];
        }
  std::vector<Run> runs;
  for (int N = 6; N <= maxN; N++)
    for (int d = 2; d <= N / 2; d++) runs.emplace_back(N, d);
  printf("side-effect checks on %zu (size, depth) cases, N = 6..%d\n", runs.size(), maxN);
  fflush(stdout);
  std::vector<std::atomic<uint64_t>> seen((total + 63) / 64);
  for (auto &x : seen) x.store(0);
  std::atomic<long> bad{0}, keptBad{0}, wrongW{0}, dup{0}, side{0}, lines{0};
  std::atomic<int> longest{0};
  std::map<int, long> segLines;
  std::map<int, int> segMax;
  std::mutex em;
  std::array<std::atomic<long>, 128> lenH;
  for (auto &h : lenH) h.store(0);
  for (auto &[fn, w] : files) {
    FILE *f = fopen(fn.c_str(), "r");
    if (!f) { fprintf(stderr, "cannot open a segment\n"); return 1; }
    std::string data;
    {
      char buf[1 << 16];
      size_t r;
      while ((r = fread(buf, 1, sizeof buf, f)) > 0) data.append(buf, r);
      fclose(f);
    }
    std::vector<size_t> starts;
    for (size_t i = 0; i < data.size();) {
      starts.push_back(i);
      const size_t e = data.find('\n', i);
      i = e == std::string::npos ? data.size() : e + 1;
    }
    segLines[w] = starts.size();
    std::atomic<size_t> ni{0};
    std::atomic<int> smax{0};
    auto work = [&] {
      std::vector<cc::Mv> mv;
      std::vector<int> a;
      for (size_t lo; (lo = ni.fetch_add(256)) < starts.size();) {
        const size_t hi = std::min(starts.size(), lo + 256);
        for (size_t li = lo; li < hi; li++) {
          const size_t s0 = starts[li];
          size_t e = data.find('\n', s0);
          if (e == std::string::npos) e = data.size();
          const std::string line = data.substr(s0, e - s0);
          lines++;
          bool ok = cc::parseMoves(line, mv);
          for (auto &m : mv) ok &= m.depth == 1 || m.depth == 2;
          if (!ok) { if (bad++ < 5) { std::lock_guard<std::mutex> g(em); printf("  bad line: %s\n", line.c_str()); } continue; }
          int P[24];  // dst form: the wing at position s goes to P[s]
          for (int k = 0; k < 24; k++) P[k] = k;
          for (auto &m : mv)
            for (int k = 0; k < 24; k++) P[k] = mw[m.face][m.depth][m.amt][P[k]];
          bool kept = true;
          for (int k = 0; k < 24; k++) if (S >> k & 1) kept &= P[k] == k;
          if (!kept) { if (keptBad++ < 5) { std::lock_guard<std::mutex> g(em); printf("  moves a kept wing: %s\n", line.c_str()); } continue; }
          int Pinv[24];
          for (int k = 0; k < 24; k++) Pinv[P[k]] = k;
          // position: target wing t sits at Pinv[t]; rank among the free positions
          uint64_t r = 0;
          uint32_t used = 0;
          int wr = 0;
          for (int i = 0; i < K; i++) {
            const int x = freeIdx[Pinv[tg[i]]];
            wr += Pinv[tg[i]] != tg[i];
            r = r * (n - i) + (x - __builtin_popcount(used & ((1u << x) - 1)));
            used |= 1u << x;
          }
          if (wr != w) { if (wrongW++ < 5) { std::lock_guard<std::mutex> g(em); printf("  wrong segment (%d away, file says %d): %s\n", wr, w, line.c_str()); } }
          const uint64_t bit = 1ULL << (r & 63);
          if (seen[r >> 6].fetch_or(bit) & bit) dup++;
          bool sideOk = true;
          for (auto &R : runs) if (!(sideOk = R.check(mv, P, a))) break;
          if (!sideOk) { if (side++ < 5) { std::lock_guard<std::mutex> g(em); printf("  side effect: %s\n", line.c_str()); } }
          const int len = mv.size();
          lenH[std::min(127, len)]++;
          int c = smax.load();
          while (len > c && !smax.compare_exchange_weak(c, len)) {}
        }
      }
    };
    std::vector<std::thread> th;
    for (int t = 0; t < threads; t++) th.emplace_back(work);
    for (auto &t : th) t.join();
    segMax[w] = smax.load();
    longest = std::max(longest.load(), smax.load());
    printf("  w%02d: %ld lines, longest %d (%.0fs)\n", w, segLines[w], segMax[w], now() - t0);
    fflush(stdout);
  }
  uint64_t covered = 0;
  for (auto &x : seen) covered += __builtin_popcountll(x.load());
  printf("lines: %ld, %ld bad, %ld move a kept wing, %ld in the wrong segment, %ld repeat a position, %ld with a side effect; lengths:", lines.load(), bad.load(), keptBad.load(),
         wrongW.load(), dup.load(), side.load());
  for (int i = 0; i < 128; i++) if (lenH[i]) printf(" %d:%ld", i, lenH[i].load());
  printf("\ncoverage: %llu of %llu stage positions\n", (unsigned long long)covered, (unsigned long long)total);
  const bool ok = !bad && !keptBad && !wrongW && !dup && !side && covered == total;
  printf("%s: every stage position is solved in at most %d moves, keeping centres colour-solved and everything outside the wing orbit in place (%.0fs)\n", ok ? "PASS" : "FAIL",
         longest.load(), now() - t0);
  return ok ? 0 : 1;
}
