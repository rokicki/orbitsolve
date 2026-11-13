// msclass: classify stage lines by the smallest move set they use.  Base (always): F,B,L,R (all powers),
// 2U2,3U2,2D2,3D2.  Optional groups: q = 2U,3U,2D,3D quarter turns; s2 = 2F2,2B2,2L2,2R2; s3 = 3F2,3B2,3L2,3R2.
// Anything else (U, D, slice quarter turns of F/B/L/R, ...) counts as "other".
// Usage: msclass STAGEDIR   (reads lNN.txt; prints counts by length x set, and move counts)
#include <cstdio>
#include <cstring>
#include <string>
#include <map>
#include <vector>
using namespace std;
int main(int argc, char **argv) {
  if (argc != 2) { fprintf(stderr, "usage: msclass STAGEDIR\n"); return 1; }
  map<string,long> mc;
  long cnt[40][128] = {};
  char buf[4096];
  for (int len = 0; len < 40; len++) {
    char fn[1024]; snprintf(fn, sizeof fn, "%s/l%02d.txt", argv[1], len);
    FILE *f = fopen(fn, "r"); if (!f) continue;
    while (fgets(buf, sizeof buf, f)) {
      int m = 0, n = 0;
      for (char *t = strtok(buf, " \n"); t; t = strtok(0, " \n")) {
        n++; mc[t]++;
        string s = t;
        int layer = 1; size_t i = 0;
        if (isdigit(s[0])) { layer = s[0]-'0'; i = 1; }
        char face = s[i]; string pw = s.substr(i+1);
        bool vert = (face=='U'||face=='D'), half = (pw=="2");
        if (layer == 1 && !vert) ;                       // F B L R any power
        else if (vert && layer > 1 && half) ;            // 2U2 3U2 2D2 3D2
        else if (vert && layer > 1) m |= 1;              // 2U 3U 2D 3D quarter
        else if (!vert && layer == 2 && half) m |= 2;    // 2F2 ...
        else if (!vert && layer == 3 && half) m |= 4;    // 3F2 ...
        else if (vert) m |= 8;                           // U D outer
        else if (layer == 2) m |= 16;                    // 2F 2F' ...
        else if (layer == 3) m |= 32;                    // 3F 3F' ...
        else m |= 64;
      }
      if (n != len) { fprintf(stderr, "length mismatch in %s\n", fn); return 1; }
      cnt[len][m]++;
    }
    fclose(f);
  }
  printf("len set count\n");
  for (int len = 0; len < 40; len++) {
    long t = 0; for (int m = 0; m < 128; m++) t += cnt[len][m];
    if (!t) continue;
    for (int m = 0; m < 128; m++) if (cnt[len][m]) printf("%d %d %ld\n", len, m, cnt[len][m]);
  }
  printf("moves");
  for (auto &p : mc) printf(" %s:%ld", p.first.c_str(), p.second);
  printf("\n");
}
