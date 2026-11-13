// stagedump: the colourings of a pair stage's classes, for structure studies.  For every certificate
// line of at least LMIN moves (and, with --every K, every K-th line as a baseline sample), prints
// "LEN C0..C47": the colour index of each pair facelet (certcube pairFacelets order: per face U L F
// R B D, 4 of orbit a then 4 of orbit b) in the position the line solves; 0 = a group the stage
// does not track, 1.. = the target groups in stage-name order.  Local analysis only.
//   stagedump STAGEDIR LMIN [--every K]
#include "certcube.h"
#include <algorithm>

int main(int argc, char **argv) {
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  long every = 0;
  std::vector<std::string> pos;
  for (int i = 1; i < argc; i++) {
    std::string o = argv[i];
    if (o == "--every" && i + 1 < argc) every = atol(argv[++i]);
    else if (o.size() > 1 && o[0] == '-') { fprintf(stderr, "unknown option %s\n", o.c_str()); return 1; }
    else pos.push_back(o);
  }
  if (pos.size() != 2) { fprintf(stderr, "usage: stagedump STAGEDIR LMIN [--every K]\n"); return 1; }
  std::string dir = pos[0];
  const size_t LMIN = atoi(pos[1].c_str());
  while (dir.size() > 1 && dir.back() == '/') dir.pop_back();
  const std::string sname = dir.substr(dir.rfind('/') == std::string::npos ? 0 : dir.rfind('/') + 1);
  const size_t dd = sname.find("..");
  if (dd == std::string::npos) { fprintf(stderr, "stage directory must be named <solved>..<target>\n"); return 1; }
  const std::string tgt = sname.substr(dd + 2);
  std::vector<int> tg;  // target groups in name order
  for (size_t i = 0; i + 1 < tgt.size(); i += 2) {
    const int o = tgt[i] == 'a' ? 0 : tgt[i] == 'b' ? 1 : -1;
    const char *p = strchr(cc::FACES, tgt[i + 1]);
    if (o < 0 || !p || !*p) { fprintf(stderr, "bad stage name\n"); return 1; }
    tg.push_back(o * 6 + (p - cc::FACES));
  }
  cc::Cube C8(8);
  const auto pf = cc::pairFacelets(C8);
  std::vector<int> fidx(C8.pos.size(), -1);
  for (int k = 0; k < 48; k++) fidx[pf[k]] = k;
  static int mp[6][4][4][48];
  for (int f = 0; f < 6; f++) for (int d = 1; d <= 3; d++) for (int a = 1; a <= 3; a++)
    for (int k = 0; k < 48; k++) { int i = pf[k]; for (int q = 0; q < a; q++) i = C8.perms[f][d - 1][i]; mp[f][d][a][k] = fidx[i]; }
  int8_t solvedCi[48];
  for (int k = 0; k < 48; k++) {
    const int g = (k % 8) / 4 * 6 + k / 8;
    solvedCi[k] = std::find(tg.begin(), tg.end(), g) - tg.begin() + 1;
    if (solvedCi[k] > (int)tg.size()) solvedCi[k] = 0;
  }
  char buf[4096];
  std::vector<cc::Mv> mv;
  long n = 0;
  for (int w = 0; w < 100; w++) {
    FILE *in = nullptr;
    for (const char *fmt : {"/w%d.txt", "/w%02d.txt"}) {
      char fn[32];
      snprintf(fn, sizeof fn, fmt, w);
      if ((in = fopen((dir + fn).c_str(), "r"))) break;
    }
    if (!in) continue;
    while (fgets(buf, sizeof buf, in)) {
      std::string l(buf);
      while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
      if (!cc::parseMoves(l, mv)) { fprintf(stderr, "bad line\n"); return 1; }
      n++;
      if (mv.size() < LMIN && !(every && n % every == 0)) continue;
      int8_t a[48], b[48];
      memcpy(a, solvedCi, 48);
      for (auto it = mv.rbegin(); it != mv.rend(); ++it) {
        for (int k = 0; k < 48; k++) b[mp[it->face][it->depth][4 - it->amt][k]] = a[k];
        memcpy(a, b, 48);
      }
      printf("%zu ", mv.size());
      for (int k = 0; k < 48; k++) putchar('0' + a[k]);
      putchar('\n');
    }
    fclose(in);
  }
}
