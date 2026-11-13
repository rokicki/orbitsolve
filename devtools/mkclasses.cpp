// mkclasses diag|mid|wing|pair IN OUT [-t threads]
// Reduce an expanded single-orbit pool ("COST s0..s47  moves", closed under the
// 48 symmetries x inverse, cheapest per permutation) to one representative per
// symmetry class ("COST moves").  Checks that re-expanding the classes gives
// back exactly the input's set of (permutation, cost) pairs.
#include "classpool.h"
#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>

using P24 = std::array<uint8_t, 48>;  // key (first P positions used)
struct PH { size_t operator()(const P24 &p) const { uint64_t h = 1469598103934665603ULL; for (auto c : p) h = (h ^ c) * 1099511628211ULL; return h; } };
static uint64_t pairHash(const uint8_t *p, int cost) {
  uint64_t h = 0x9E3779B97F4A7C15ULL ^ (uint64_t)cost;
  for (int k = 0; k < 48; k++) { h ^= p[k]; h *= 0xBF58476D1CE4E5B9ULL; h ^= h >> 29; }
  return h;
}

// (count, order-independent checksum) of the (perm, cost) pairs of a pool file:
// expanded ("COST s0..s47 moves") or symmetry classes (.cls)
static std::pair<long, uint64_t> digest(const OrbitModel &m, const char *fn) {
  long n = 0;
  uint64_t sum = 0;
  if (endsWith(fn, ".cls")) {
    ClassPool cp;
    if (!cp.load(fn)) { fprintf(stderr, "cannot read %s\n", fn); exit(1); }
    cp.expand(m, [&](const uint8_t *p, int c, uint32_t) {
      uint8_t q[48] = {};
      std::copy(p, p + m.P, q);
      sum += pairHash(q, c);
      n++;
    });
    return {n, sum};
  }
  FILE *f = fopen(fn, "r");
  if (!f) { fprintf(stderr, "cannot read %s\n", fn); exit(1); }
  char buf[4096];
  while (fgets(buf, sizeof buf, f)) {
    if (buf[0] == '#') continue;
    char *e;
    int c = (int)strtol(buf, &e, 10);
    if (e == buf) continue;
    uint8_t p[48] = {};
    for (int k = 0; k < 48; k++) { int x = (int)strtol(e, &e, 10); if (k < m.P) p[k] = x; }
    sum += pairHash(p, c);
    n++;
  }
  fclose(f);
  return {n, sum};
}

int main(int argc, char **argv) {
  if (argc >= 5 && std::string(argv[1]) == "compare") {  // mkclasses compare KIND A B
    std::string kind = argv[2];
    OrbitModel m = kind == "diag" ? diagModel() : kind == "mid" ? midModel() : kind == "wing" ? wingModel()
                 : kind == "pair" ? pairModel() : OrbitModel();
    if (m.sigma.size() != 48) { fprintf(stderr, "unknown kind %s\n", kind.c_str()); return 1; }
    auto a = digest(m, argv[3]), b = digest(m, argv[4]);
    printf("%s: %ld algorithms; %s: %ld algorithms; %s\n", argv[3], a.first, argv[4], b.first,
           a == b ? "SAME (permutations and costs)" : "DIFFERENT");
    return a == b ? 0 : 2;
  }
  if (argc < 4) {
    fprintf(stderr, "usage: mkclasses diag|mid|wing|pair IN OUT [-t threads]\n"
                    "         reduce an expanded pool to one line per symmetry class (and check it)\n"
                    "       mkclasses compare diag|mid|wing|pair A B\n"
                    "         do two pools (expanded or .cls) hold the same permutations and costs?\n");
    return 1;
  }
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  std::string kind = argv[1];
  int threads = std::thread::hardware_concurrency();
  for (int i = 4; i < argc; i++) {
    std::string a = argv[i];
    if (a == "-t" && i + 1 < argc) threads = atoi(argv[++i]);
    else { fprintf(stderr, "unknown option %s\n", argv[i]); return 1; }
  }
  OrbitModel m = kind == "diag" ? diagModel() : kind == "mid" ? midModel() : kind == "wing" ? wingModel()
               : kind == "pair" ? pairModel() : OrbitModel();
  if (m.sigma.size() != 48) { fprintf(stderr, "orbit model for %s failed\n", kind.c_str()); return 1; }
  FILE *f = fopen(argv[2], "r");
  if (!f) { fprintf(stderr, "cannot read %s\n", argv[2]); return 1; }
  const int NSH = 1024;
  std::vector<std::unordered_map<P24, std::pair<int, std::string>, PH>> shard(NSH);
  std::vector<std::mutex> mu(NSH);
  std::atomic<uint64_t> sumIn{0};
  std::atomic<long> nIn{0}, nBadPerm{0};
  std::vector<std::string> batch;
  auto process = [&] {
    std::atomic<size_t> nx{0};
    std::vector<std::thread> th;
    for (int t = 0; t < threads; t++)
      th.emplace_back([&] {
        uint8_t p[48], key[48];
        const int P = m.P;
        for (size_t i; (i = nx.fetch_add(1)) < batch.size();) {
          const char *s = batch[i].c_str();
          char *e;
          int c = (int)strtol(s, &e, 10);
          for (int k = 0; k < 48; k++) { int x = (int)strtol(e, &e, 10); p[k] = x; }
          for (int k = P; k < 48; k++) p[k] = 0;
          while (*e == ' ') e++;
          std::string t(e);
          while (!t.empty() && (t.back() == '\n' || t.back() == ' ')) t.pop_back();
          std::string flag;  // pair pools: trailing " D0" / " D1" is kept with the class
          if (t.size() >= 3 && t.compare(t.size() - 3, 2, " D") == 0 && (t.back() == '0' || t.back() == '1')) {
            flag = t.substr(t.size() - 3);
            t.resize(t.size() - 3);
          }
          // the text must produce this perm under the model
          auto q = m.permOf(parse_alg(t));
          if (!std::equal(q.begin(), q.end(), p)) { nBadPerm++; continue; }
          t += flag;
          sumIn += pairHash(p, c);
          nIn++;
          m.classKey(p, key);
          P24 k{};
          std::copy(key, key + P, k.begin());
          size_t sh = PH()(k) % NSH;
          std::lock_guard<std::mutex> lk(mu[sh]);
          auto it = shard[sh].find(k);
          if (it == shard[sh].end()) shard[sh].emplace(k, std::make_pair(c, t));
          else if (c < it->second.first || (c == it->second.first && t < it->second.second)) it->second = {c, t};
        }
      });
    for (auto &t : th) t.join();
    batch.clear();
  };
  char buf[4096];
  while (fgets(buf, sizeof buf, f)) {
    if (buf[0] == '#') continue;
    batch.push_back(buf);
    if (batch.size() >= (1 << 20)) process();
  }
  process();
  fclose(f);
  std::vector<std::pair<int, std::string>> cls;
  for (auto &s : shard) for (auto &kv : s) cls.push_back(kv.second);
  std::sort(cls.begin(), cls.end());
  FILE *o = fopen(argv[3], "w");
  fprintf(o, "# symmetry classes (48 x inverse) of %s: %zu classes, %ld algorithms\n", argv[2], cls.size(), nIn.load());
  for (auto &c : cls) fprintf(o, "%d %s\n", c.first, c.second.c_str());
  fclose(o);
  // re-expand and compare
  ClassPool cp;
  cp.load(argv[3]);
  uint64_t sumOut = 0;
  long nOut = 0;
  cp.expand(m, [&](const uint8_t *p, int c, uint32_t) {
    uint8_t q[48] = {};
    std::copy(p, p + m.P, q);
    sumOut += pairHash(q, c);
    nOut++;
  });
  printf("%s: %ld algorithms (%ld text/perm mismatches) -> %zu classes (%.1fx); re-expanded %ld, checksum %s\n", argv[3],
         nIn.load(), nBadPerm.load(), cls.size(), (double)nIn / std::max<size_t>(1, cls.size()), nOut,
         nOut == nIn && sumOut == sumIn ? "MATCH" : "MISMATCH");
  return nOut == nIn && sumOut == sumIn && !nBadPerm ? 0 : 2;
}
