// certimprove: shorten the hardest lines of a pair-stage certificate with streamed algorithms.
// For every target line (a solving sequence in the certificate, e.g. cert/aring-22.seq.txt), its
// position p, and every image A (48 cube symmetries x inverse) of every algorithm read from the
// pools, A is tried as the first block: if A keeps the stage's solved groups and A(p) is a position
// the certificate itself solves in at most K moves (--near K), then A followed by that line (mapped
// through the symmetry) solves p.  Every improvement is replayed before it is printed; the output
// (stdout) is one solving sequence per line, the shortest found per target, ready for certmerge.
// Pools are streamed (any size; "-" is stdin): lines "COST MOVES [D0|D1]" or plain move lines.
//
//   certimprove STAGEDIR TARGETS.txt [--near K] [--threads T] POOL...
#include "certcube.h"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <thread>
#include <unordered_map>

typedef unsigned __int128 u128;
struct KH { size_t operator()(u128 k) const { uint64_t a = (uint64_t)k, b = (uint64_t)(k >> 64); return (a ^ (b * 0x9E3779B97F4A7C15ULL)) * 0xBF58476D1CE4E5B9ULL; } };

static int groupMask(const std::string &s) {
  if (s == "none") return 0;
  int m = 0;
  if (s.size() % 2) return -1;
  for (size_t i = 0; i < s.size(); i += 2) {
    const int o = s[i] == 'a' ? 0 : s[i] == 'b' ? 1 : -1;
    const char *p = strchr(cc::FACES, s[i + 1]);
    if (o < 0 || !p || !*p) return -1;
    m |= 1 << (o * 6 + (p - cc::FACES));
  }
  return m;
}
static std::vector<std::string> readLines(const std::string &f) {
  std::vector<std::string> v;
  FILE *in = fopen(f.c_str(), "r");
  if (!in) return v;
  char buf[4096];
  while (fgets(buf, sizeof buf, in)) {
    std::string l(buf);
    while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
    v.push_back(l);
  }
  fclose(in);
  return v;
}
static std::string text(const std::vector<cc::Mv> &mv) {
  std::string s;
  for (auto &m : mv) { if (!s.empty()) s += ' '; s += cc::mvText(m); }
  return s;
}

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  int K = 12, T = std::max(1u, std::thread::hardware_concurrency());
  std::vector<std::string> pos;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--near" && i + 1 < argc) K = atoi(argv[++i]);
    else if (o == "--threads" && i + 1 < argc) T = atoi(argv[++i]);
    else if (o.size() > 1 && o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else pos.push_back(o);
  }
  if (pos.size() < 3) { fprintf(stderr, "usage: certimprove STAGEDIR TARGETS.txt [--near K] [--threads T] POOL...\n"); return 1; }
  std::string dir = pos[0];
  while (dir.size() > 1 && dir.back() == '/') dir.pop_back();
  const std::string sname = dir.substr(dir.rfind('/') == std::string::npos ? 0 : dir.rfind('/') + 1);
  const size_t dd = sname.find("..");
  if (dd == std::string::npos) { fprintf(stderr, "stage directory must be named <solved>..<target>\n"); return 1; }
  const int S = groupMask(sname.substr(0, dd)), TG = groupMask(sname.substr(dd + 2));
  if (S < 0 || TG <= 0 || (S & TG)) { fprintf(stderr, "bad stage name\n"); return 1; }

  // the 48 pair facelets on 8x8x8 and the move maps on them (as certmerge)
  cc::Cube C8(8);
  const auto pf = cc::pairFacelets(C8);
  std::vector<int> fidx(C8.pos.size(), -1);
  for (int k = 0; k < 48; k++) fidx[pf[k]] = k;
  static int mp[6][4][4][48];
  for (int f = 0; f < 6; f++)
    for (int d = 1; d <= 3; d++)
      for (int a = 1; a <= 3; a++)
        for (int k = 0; k < 48; k++) {
          int i = pf[k];
          for (int q = 0; q < a; q++) i = C8.perms[f][d - 1][i];
          mp[f][d][a][k] = fidx[i];
        }
  auto groupOf = [](int k) { return (k % 8) / 4 * 6 + k / 8; };
  std::vector<int> tg;
  for (int g = 0; g < 12; g++) if (TG >> g & 1) tg.push_back(g);
  int8_t cidx[12];  // colour index used in keys: 0 = don't care, 1.. = target group
  for (int g = 0; g < 12; g++) cidx[g] = (TG >> g & 1) ? std::find(tg.begin(), tg.end(), g) - tg.begin() + 1 : 0;
  std::vector<int> freeK;
  for (int k = 0; k < 48; k++) if (!(S >> groupOf(k) & 1)) freeK.push_back(k);
  int bits = 1;
  while ((1 << bits) < (int)tg.size() + 1) bits++;
  if ((int)freeK.size() * bits > 128) { fprintf(stderr, "stage too large for the key\n"); return 1; }
  // key of a colouring (non-canonical): free positions' colour indices packed
  auto keyOf = [&](const int8_t *col) {
    u128 k = 0;
    for (int i : freeK) k = (k << bits) | (u128)(col[i] < 0 ? 0 : cidx[col[i]]);
    return k;
  };
  // the 48 cube symmetries: facelet map, group map, move map (face, amount flip)
  struct Sym { int fac[48]; int gmap[12]; int face[6]; bool flip; bool keepsStage; };
  std::vector<Sym> syms;
  {
    const int pm[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    const int par[6] = {1, -1, -1, 1, 1, -1};
    for (int pi = 0; pi < 6; pi++)
      for (int sg = 0; sg < 8; sg++) {
        Sym y;
        auto map = [&](const std::array<int, 3> &p) {
          std::array<int, 3> q;
          for (int r = 0; r < 3; r++) q[r] = p[pm[pi][r]] * ((sg >> r & 1) ? -1 : 1);
          return q;
        };
        for (int k = 0; k < 48; k++) {
          auto q = map(C8.pos[pf[k]]);
          for (int x = 0; x < 48; x++) if (C8.pos[pf[x]] == q) y.fac[k] = x;
        }
        for (int g = 0; g < 12; g++) for (int k = 0; k < 48; k++) if (groupOf(k) == g) y.gmap[g] = groupOf(y.fac[k]);
        for (int f = 0; f < 6; f++) {
          auto q = map(cc::Cube::normalOf(f));
          for (int f2 = 0; f2 < 6; f2++) if (cc::Cube::normalOf(f2) == q) y.face[f] = f2;
        }
        const int det = par[pi] * ((sg & 1) ? -1 : 1) * ((sg & 2) ? -1 : 1) * ((sg & 4) ? -1 : 1);
        y.flip = det < 0;
        int s2 = 0, t2 = 0;
        for (int g = 0; g < 12; g++) { if (S >> g & 1) s2 |= 1 << y.gmap[g]; if (TG >> g & 1) t2 |= 1 << y.gmap[g]; }
        y.keepsStage = s2 == S && t2 == TG;
        syms.push_back(y);
      }
  }
  auto symMoves = [&](const Sym &y, const std::vector<cc::Mv> &mv) {
    std::vector<cc::Mv> r;
    for (auto &m : mv) r.push_back({y.face[m.face], m.depth, y.flip ? 4 - m.amt : m.amt});
    return r;
  };
  int8_t solvedCol[48];
  for (int k = 0; k < 48; k++) solvedCol[k] = ((S | TG) >> groupOf(k) & 1) ? groupOf(k) : -1;
  // the position a solving sequence solves (run backwards from solved)
  auto positionOf = [&](const std::vector<cc::Mv> &mv, int8_t *a) {
    int8_t b[48];
    memcpy(a, solvedCol, 48);
    for (auto it = mv.rbegin(); it != mv.rend(); ++it) {
      for (int k = 0; k < 48; k++) b[mp[it->face][it->depth][4 - it->amt][k]] = a[k];
      memcpy(a, b, 48);
    }
  };
  auto solves = [&](const int8_t *p, const std::vector<cc::Mv> &mv) {
    int8_t a[48], b[48];
    memcpy(a, p, 48);
    for (auto &m : mv) {
      for (int k = 0; k < 48; k++) b[mp[m.face][m.depth][m.amt][k]] = a[k];
      memcpy(a, b, 48);
    }
    for (int k = 0; k < 48; k++) if (solvedCol[k] >= 0 && a[k] != groupOf(k)) return false;
    return true;
  };

  // near table: every stage-symmetric image of every certificate line of length <= K
  struct Near { uint32_t line; uint8_t sym, len; };
  std::unordered_map<u128, Near, KH> near;
  std::vector<std::string> nearLines;
  {
    long nl = 0;
    for (int w = 0; w <= 48; w++) {
      char fn[32];
      {
        snprintf(fn, sizeof fn, "w%0*d.txt", tg.size() * 4 >= 10 ? 2 : 1, w);  // as certmerge
        for (auto &l : readLines(dir + "/" + fn)) {
          std::vector<cc::Mv> mv;
          if (!cc::parseMoves(l, mv) || (int)mv.size() > K) continue;
          int8_t a[48], b[48];
          positionOf(mv, a);
          const uint32_t id = nearLines.size();
          nearLines.push_back(l);
          nl++;
          for (int s = 0; s < 48; s++) {
            if (!syms[s].keepsStage) continue;
            for (int k = 0; k < 48; k++) b[syms[s].fac[k]] = a[k] < 0 ? -1 : syms[s].gmap[a[k]];
            const u128 key = keyOf(b);
            auto it = near.find(key);
            if (it == near.end() || mv.size() < it->second.len) near[key] = {id, (uint8_t)s, (uint8_t)mv.size()};
          }
        }
      }
    }
    fprintf(stderr, "near table: %ld certificate lines of length <= %d, %zu positions\n", nl, K, near.size());
  }
  // targets
  struct Target { int8_t p[48]; int cur, best; std::string bestLine; };
  std::vector<Target> targets;
  for (auto &l : readLines(pos[1])) {
    std::vector<cc::Mv> mv;
    if (l.empty() || !cc::parseMoves(l, mv)) continue;
    Target t;
    positionOf(mv, t.p);
    t.cur = t.best = mv.size();
    targets.push_back(t);
  }
  fprintf(stderr, "targets: %zu\n", targets.size());
  std::mutex mu;
  std::atomic<long> nAlg{0}, nImg{0}, nKeep{0}, nHit{0}, nBad{0};
  auto tryAlg = [&](const std::vector<cc::Mv> &mv0) {
    nAlg++;
    std::vector<cc::Mv> inv;
    for (auto it = mv0.rbegin(); it != mv0.rend(); ++it) inv.push_back({it->face, it->depth, 4 - it->amt});
    std::vector<std::array<int8_t, 48>> seen;
    for (int iv = 0; iv < 2; iv++)
      for (int s = 0; s < 48; s++) {
        const auto A = symMoves(syms[s], iv ? inv : mv0);
        // destination of each facelet under A
        int D[48];
        for (int k = 0; k < 48; k++) {
          int x = k;
          for (auto &m : A) x = mp[m.face][m.depth][m.amt][x];
          D[k] = x;
        }
        nImg++;
        bool keeps = true;
        for (int k = 0; k < 48 && keeps; k++) if ((S >> groupOf(k) & 1) && groupOf(D[k]) != groupOf(k)) keeps = false;
        if (!keeps) continue;
        std::array<int8_t, 48> dk;
        for (int k = 0; k < 48; k++) dk[k] = D[k];
        if (std::find(seen.begin(), seen.end(), dk) != seen.end()) continue;
        seen.push_back(dk);
        nKeep++;
        const int la = A.size();
        for (auto &t : targets) {
          if (la + 1 > t.best - 1) continue;  // even a 1-move rest would not beat the best
          int8_t q[48];
          for (int k = 0; k < 48; k++) q[D[k]] = t.p[k];
          auto it = near.find(keyOf(q));
          if (it == near.end() || la + it->second.len >= t.best) continue;
          nHit++;
          // A, then the near line mapped through its symmetry; replayed before it is accepted
          std::vector<cc::Mv> rest;
          cc::parseMoves(nearLines[it->second.line], rest);
          std::vector<cc::Mv> cand = A;
          const auto r2 = symMoves(syms[it->second.sym], rest);
          cand.insert(cand.end(), r2.begin(), r2.end());
          if (!solves(t.p, cand)) { nBad++; continue; }
          std::lock_guard<std::mutex> g(mu);
          if ((int)cand.size() < t.best) t.best = cand.size(), t.bestLine = text(cand);
        }
      }
  };
  // stream the pools in batches
  const size_t B = 1 << 16;
  for (size_t pi = 2; pi < pos.size(); pi++) {
    FILE *in = pos[pi] == "-" ? stdin : fopen(pos[pi].c_str(), "r");
    if (!in) { fprintf(stderr, "cannot read %s\n", pos[pi].c_str()); return 1; }
    char buf[4096];
    std::vector<std::vector<cc::Mv>> batch;
    bool eof = false;
    while (!eof) {
      batch.clear();
      while (batch.size() < B) {
        if (!fgets(buf, sizeof buf, in)) { eof = true; break; }
        size_t n = strlen(buf);
        if (!n || buf[n - 1] != '\n' || buf[0] == '#') continue;  // cut-off last line, comments
        std::string l(buf);
        while (!l.empty() && (l.back() == '\n' || l.back() == ' ' || l.back() == '\r')) l.pop_back();
        if (l.size() >= 3 && l[l.size() - 3] == ' ' && l[l.size() - 2] == 'D') l.resize(l.size() - 3);
        size_t st = 0;
        while (st < l.size() && l[st] == ' ') st++;
        {  // "COST MOVES": skip a leading all-digit token (a move like "2F" is not one)
          size_t e = st;
          while (e < l.size() && l[e] >= '0' && l[e] <= '9') e++;
          if (e > st && (e == l.size() || l[e] == ' ')) st = e;
        }
        std::vector<cc::Mv> mv;
        if (!cc::parseMoves(l.substr(st), mv) || mv.empty()) continue;
        bool ok = true;
        for (auto &m : mv) if (m.depth < 1 || m.depth > 3) ok = false;
        if (ok) batch.push_back(mv);
      }
      std::vector<std::thread> th;
      for (int t = 0; t < T; t++)
        th.emplace_back([&, t] { for (size_t i = t; i < batch.size(); i += T) tryAlg(batch[i]); });
      for (auto &x : th) x.join();
    }
    if (in != stdin) fclose(in);
    long better = 0;
    for (auto &t : targets) if (t.best < t.cur) better++;
    fprintf(stderr, "%s: %ld algorithms so far, %ld images, %ld keep the stage, %ld hits, %ld failed replays; %ld of %zu targets shortened\n",
            pos[pi].c_str(), nAlg.load(), nImg.load(), nKeep.load(), nHit.load(), nBad.load(), better, targets.size());
  }
  std::vector<int> hist(64);
  for (auto &t : targets) {
    hist[std::min(t.best, 63)]++;
    if (t.best < t.cur) printf("%s\n", t.bestLine.c_str());
  }
  fprintf(stderr, "targets by best length:");
  for (int i = 0; i < 64; i++) if (hist[i]) fprintf(stderr, " %d:%d", i, hist[i]);
  fprintf(stderr, "\n");
  return 0;
}
