// certcube.h: a plain N x N x N facelet cube for the certificates (certcheck.cpp).
//
// Facelets: face f in U L F R B D order, row r and column c in 0..N-1, each face seen from
// outside in the usual net orientation (U: back edge at the top; D: front edge at the top;
// L F R B: U edge at the top).  A move is (face, depth, amount): the layer at depth 1..N/2
// counted from that face turns amount quarter turns clockwise as seen from that face.  Text:
// "U", "U2", "U'" for depth 1, "2U", "3U'", ... for deeper layers.
#pragma once
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace cc {

static const char FACES[] = "ULFRBD";

struct Mv { int face, depth, amt; };

inline std::string mvText(const Mv &m) {
  std::string s;
  if (m.depth > 1) s += std::to_string(m.depth);
  s += FACES[m.face];
  if (m.amt == 2) s += '2';
  else if (m.amt == 3) s += '\'';
  return s;
}

// parse "2U' F2 3R ..." (false on a bad token)
inline bool parseMoves(const std::string &t, std::vector<Mv> &out) {
  out.clear();
  size_t i = 0;
  while (i < t.size()) {
    while (i < t.size() && t[i] == ' ') i++;
    if (i >= t.size()) break;
    int depth = 1;
    if (t[i] >= '0' && t[i] <= '9') {
      depth = 0;
      while (i < t.size() && t[i] >= '0' && t[i] <= '9') depth = depth * 10 + (t[i++] - '0');
    }
    if (i >= t.size()) return false;
    const char *p = strchr(FACES, t[i]);
    if (!p || !*p) return false;
    i++;
    int amt = 1;
    if (i < t.size() && t[i] == '2') amt = 2, i++;
    else if (i < t.size() && t[i] == '\'') amt = 3, i++;
    if (i < t.size() && t[i] != ' ') return false;
    if (depth < 1) return false;
    out.push_back({(int)(p - FACES), depth, amt});
  }
  return true;
}

// facelet geometry: index = (f * N + r) * N + c
struct Cube {
  int N;
  std::vector<std::array<int, 3>> pos;  // doubled coordinates of the facelet centre, cube centred at 0
  std::vector<std::array<int, 3>> nrm;  // outward normal
  std::vector<int> faceOf;
  // perms[face][depth-1]: dst form, the facelet at i moves to perm[i] after one clockwise quarter turn
  std::vector<std::vector<std::vector<int>>> perms;

  static std::array<int, 3> normalOf(int f) {
    switch (f) {
      case 0: return {0, 1, 0};   // U
      case 1: return {-1, 0, 0};  // L
      case 2: return {0, 0, 1};   // F
      case 3: return {1, 0, 0};   // R
      case 4: return {0, 0, -1};  // B
      default: return {0, -1, 0}; // D
    }
  }
  // integer cube coordinates x (L->R), y (D->U), z (B->F) of facelet (f, r, c), in 0..N-1
  void cubeXYZ(int f, int r, int c, int &x, int &y, int &z) const {
    const int M = N - 1;
    switch (f) {
      case 0: x = c, y = M, z = r; break;          // U: row 0 at the back
      case 1: x = 0, y = M - r, z = c; break;      // L: column 0 at the back
      case 2: x = c, y = M - r, z = M; break;      // F
      case 3: x = M, y = M - r, z = M - c; break;  // R: column 0 at the front
      case 4: x = M - c, y = M - r, z = 0; break;  // B: column 0 at the right
      default: x = c, y = 0, z = M - r; break;     // D: row 0 at the front
    }
  }
  int index(int f, int r, int c) const { return (f * N + r) * N + c; }
  explicit Cube(int n) : N(n) {
    const int F = 6 * N * N;
    pos.resize(F), nrm.resize(F), faceOf.resize(F);
    for (int f = 0; f < 6; f++)
      for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++) {
          int x, y, z;
          cubeXYZ(f, r, c, x, y, z);
          const int i = index(f, r, c);
          const auto n = normalOf(f);
          // doubled, centred coordinates of the facelet centre: cubie centre plus half the normal
          pos[i] = {2 * x - (N - 1) + n[0], 2 * y - (N - 1) + n[1], 2 * z - (N - 1) + n[2]};
          nrm[i] = n;
          faceOf[i] = f;
        }
    // lookup from (position, normal) to facelet
    auto find = [&](const std::array<int, 3> &p) {
      for (int i = 0; i < F; i++) if (pos[i] == p) return i;
      fprintf(stderr, "certcube: no facelet at a rotated position\n");
      exit(1);
    };
    perms.assign(6, {});
    for (int f = 0; f < 6; f++) {
      const auto n = normalOf(f);
      for (int d = 1; d <= (N + 1) / 2; d++) {  // odd N: the middle slice too
        std::vector<int> pm(F);
        for (int i = 0; i < F; i++) {
          // cubie coordinate along n, counted from the face: depth 1 is the outer layer
          const int along = (pos[i][0] - nrm[i][0]) * n[0] + (pos[i][1] - nrm[i][1]) * n[1] + (pos[i][2] - nrm[i][2]) * n[2];  // doubled, centred
          const int dep = (N - 1 - along) / 2 + 1;
          if (dep != d) { pm[i] = i; continue; }
          // clockwise seen from outside = rotation by -90 degrees about n: v -> -(n x v) + n (n . v)
          const auto &v = pos[i];
          const int dot = n[0] * v[0] + n[1] * v[1] + n[2] * v[2];
          std::array<int, 3> cr = {n[1] * v[2] - n[2] * v[1], n[2] * v[0] - n[0] * v[2], n[0] * v[1] - n[1] * v[0]};
          std::array<int, 3> w = {-cr[0] + n[0] * dot, -cr[1] + n[1] * dot, -cr[2] + n[2] * dot};
          pm[i] = find(w);
        }
        perms[f].push_back(pm);
      }
    }
  }
  // apply a move to a facelet array (contents move with the facelets)
  template <class T> void apply(std::vector<T> &a, const Mv &m, std::vector<T> &tmp) const {
    const auto &pm = perms[m.face][m.depth - 1];
    tmp = a;
    for (int q = 0; q < m.amt; q++) {
      for (size_t i = 0; i < a.size(); i++) tmp[pm[i]] = a[i];
      a.swap(tmp);
    }
  }
};

// the 48 oblique facelets of the chiral pair (2,3)/(3,2) (1-based rows and columns) on N^3,
// N >= 6: per face U L F R B D, orbit a = (1,2) (0-based) and its quarter-turn images about
// the face centre, then orbit b = (2,1) and its images.  The certificate colourings use
// this order.
inline std::vector<int> pairFacelets(const Cube &C) {
  const int M = C.N - 1;
  std::vector<int> v;
  for (int f = 0; f < 6; f++)
    for (int o = 0; o < 2; o++) {
      int r = o == 0 ? 1 : 2, c = o == 0 ? 2 : 1;
      for (int k = 0; k < 4; k++) {
        v.push_back(C.index(f, r, c));
        const int nr = c, nc = M - r;  // a quarter turn clockwise about the face centre
        r = nr, c = nc;
      }
    }
  return v;
}


// the 24 facelets of the wing orbit at depth d (2 <= d <= N/2, not the middle layer of an odd
// cube), one per wing: per face U L F R B D, the facelet in row 0 at column d-1 and its three
// quarter-turn images about the face centre.  Each wing has exactly one facelet in this set (its
// other facelet sits at column N-d of the neighbouring face), and moves map the set to itself.
// Wing k is named by the letter 'a' + k.
inline std::vector<int> wingFacelets(const Cube &C, int d) {
  const int M = C.N - 1;
  std::vector<int> v;
  for (int f = 0; f < 6; f++) {
    int r = 0, c = d - 1;
    for (int k = 0; k < 4; k++) {
      v.push_back(C.index(f, r, c));
      const int nr = c, nc = M - r;
      r = nr, c = nc;
    }
  }
  return v;
}

// the 24 facelets of the diagonal (X-centre) orbit at depth d on an N^3 (2 <= d <= N/2): per face
// U L F R B D, the facelet in row d-1, column d-1 and its three quarter-turn images about the face
// centre.  Group (home face) of facelet k is k / 4.
inline std::vector<int> diagFacelets(const Cube &C, int d) {
  const int M = C.N - 1;
  std::vector<int> v;
  for (int f = 0; f < 6; f++) {
    int r = d - 1, c = d - 1;
    for (int k = 0; k < 4; k++) {
      v.push_back(C.index(f, r, c));
      const int nr = c, nc = M - r;
      r = nr, c = nc;
    }
  }
  return v;
}

// the 24 facelets of the mid (T-centre) orbit at depth d on an odd N^3 (2 <= d <= (N-1)/2): per
// face U L F R B D, the facelet in row d-1 at the middle column and its three quarter-turn images
// about the face centre.  Group (home face) of facelet k is k / 4.
inline std::vector<int> midFacelets(const Cube &C, int d) {
  const int M = C.N - 1, m = M / 2;
  std::vector<int> v;
  for (int f = 0; f < 6; f++) {
    int r = d - 1, c = m;
    for (int k = 0; k < 4; k++) {
      v.push_back(C.index(f, r, c));
      const int nr = c, nc = M - r;
      r = nr, c = nc;
    }
  }
  return v;
}

}  // namespace cc
