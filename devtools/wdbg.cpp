#include "wings.h"
#include <map>
int main() {
  WingGens G; G.build();
  static const int AX[6] = {0, 1, 2, 1, 2, 0};
  std::map<std::string, int> types;
  for (int sf = 0; sf < 6; sf++) for (int st = 1; st <= 3; st++) for (int yf = 0; yf < 6; yf++) {
    if (AX[yf] == AX[sf]) continue;
    for (int yt = 1; yt <= 3; yt++) {
      const Perm24 &S = G.slice[sf][st], &Si = G.slice[sf][4 - st], &Y = G.face[yf][yt], &Yi = G.face[yf][4 - yt];
      Perm24 m = WingGens::compose(WingGens::compose(WingGens::compose(S, Y), Si), Yi);
      bool seen[24] = {}; std::vector<int> cyc;
      for (int j = 0; j < 24; j++) if (!seen[j] && m[j] != j) { int l = 0, x = j; while (!seen[x]) { seen[x] = 1; x = m[x]; l++; } cyc.push_back(l); }
      std::sort(cyc.begin(), cyc.end());
      std::string k; for (int c : cyc) k += std::to_string(c) + " ";
      types[k]++;
    }
  }
  for (auto &t : types) printf("cycles [%s]: %d macros\n", t.first.c_str(), t.second);
}
