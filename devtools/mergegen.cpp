// mergegen: oblique-pair algorithms by merging pure 5^3 projection words (see mergegen.h).
// Reports per length the merged words and distinct pair permutations, and with --check FILE (a
// tcwalk .cls) whether every tcwalk class up to MAXLEN is produced.  Nothing is written.
//
//   mergegen MAXLEN [--slices S] [--faces F] [--radius R] [--wfrom POOL.cls]... [--wsym]
//                   [--threads T] [--check FILE.cls]
#include "mergegen.h"
#include <mutex>

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  if (argc < 2) { fprintf(stderr, "usage: mergegen MAXLEN [--slices S] [--faces F] [--radius R] [--wfrom POOL.cls]... [--wsym] [--atoms] [--threads T] [--check FILE.cls]\n"); return 1; }
  int threads = std::max(1u, std::thread::hardware_concurrency());
  std::string check, opts;
  std::vector<std::string> pos;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--threads" && i + 1 < argc) threads = atoi(argv[++i]);
    else if (o == "--check" && i + 1 < argc) check = argv[++i];
    else if ((o == "--slices" || o == "--faces" || o == "--radius" || o == "--wfrom") && i + 1 < argc) opts += " " + o + " " + argv[++i];
    else if (o == "--wsym" || o == "--atoms") opts += " " + o;
    else if (o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else pos.push_back(o);
  }
  if (pos.size() != 1) { fprintf(stderr, "usage: mergegen MAXLEN [options]\n"); return 1; }
  mg::Gen g;
  g.cfg.maxLen = atoi(pos[0].c_str());
  if (!mg::parseConfig(opts, g.cfg)) return 1;
  OrbitModel model = pairModel();
  const double t0 = mg::now();
  if (!g.build(model)) return 1;
  struct H { size_t operator()(const std::array<uint8_t, 48> &v) const { size_t h = 1469598103934665603ULL; for (auto x : v) h = (h ^ x) * 1099511628211ULL; return h; } };
  std::vector<std::unordered_map<std::array<uint8_t, 48>, int, H>> mine(threads);
  std::vector<long> words;
  long nodes;
  const double tg = mg::now();
  g.run(threads, [&](int tid, const uint8_t *p, int n, const int *) {
    std::array<uint8_t, 48> a;
    memcpy(a.data(), p, 48);
    auto it = mine[tid].find(a);
    if (it == mine[tid].end() || it->second > n) mine[tid][a] = n;
  }, words, nodes);
  const double el = mg::now() - tg;
  std::unordered_map<std::array<uint8_t, 48>, int, H> perms;
  for (auto &m : mine) for (auto &[p, n] : m) { auto it = perms.find(p); if (it == perms.end() || it->second > n) perms[p] = n; }
  long tw = 0;
  for (auto x : words) tw += x;
  printf("merged words (cores) by length:");
  for (size_t d = 0; d < words.size(); d++) if (words[d]) printf(" %zu:%ld", d, words[d]);
  printf("\n%ld words, %ld merge nodes, %.1fs merging (%.3g words/s), %.1fs in all\n", tw, nodes, el, tw / el, mg::now() - t0);
  std::map<int, long> pl;
  for (auto &[p, n] : perms) pl[n]++;
  printf("distinct pair permutations by shortest length:");
  for (auto &kv : pl) printf(" %d:%ld", kv.first, kv.second);
  printf(" (%zu)\n", perms.size());
  if (!check.empty()) {
    std::unordered_map<std::string, int> classes;
    for (auto &[p, n] : perms) {
      std::string k(48, 0);
      model.classKey(p.data(), (uint8_t *)k.data());
      auto it = classes.find(k);
      if (it == classes.end() || it->second > n) classes[k] = n;
    }
    FILE *f = fopen(check.c_str(), "r");
    if (!f) { fprintf(stderr, "cannot open the check file\n"); return 1; }
    char buf[8192];
    std::map<int, long> have, miss, longer;
    int shown = 0;
    while (fgets(buf, sizeof buf, f)) {
      char *q;
      const int len = strtol(buf, &q, 10);
      if (q == buf || len > g.cfg.maxLen) continue;
      std::string t(q);
      while (!t.empty() && (t.back() == '\n' || t.back() == ' ')) t.pop_back();
      if (t.size() >= 3 && t.compare(t.size() - 3, 2, " D") == 0 && (t.back() == '0' || t.back() == '1')) t.resize(t.size() - 3);
      auto p = model.permOf(parse_alg(t));
      std::string k(48, 0);
      model.classKey(p.data(), (uint8_t *)k.data());
      auto it = classes.find(k);
      if (it == classes.end()) { miss[len]++; if (shown++ < 10) printf("  not produced: %d %s\n", len, t.c_str()); }
      else if (it->second > len) { longer[len]++; if (shown++ < 10) printf("  only longer (%d): %d %s\n", it->second, len, t.c_str()); }
      else have[len]++;
    }
    fclose(f);
    printf("check: tcwalk classes produced:");
    for (auto &kv : have) printf(" %d:%ld", kv.first, kv.second);
    printf("; only longer:");
    for (auto &kv : longer) printf(" %d:%ld", kv.first, kv.second);
    printf("; missing:");
    for (auto &kv : miss) printf(" %d:%ld", kv.first, kv.second);
    printf("\n");
  }
}
