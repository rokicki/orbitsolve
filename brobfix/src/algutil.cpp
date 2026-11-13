#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using std::string;
using std::vector;
using std::map;
using std::set;

using uchar = unsigned char;
static constexpr int PERM_SIZE  = 24;
static constexpr int MAX_ORBITS =  8;   // enough for 6x6x6 (7 orbits)
using Perm = std::array<uchar, PERM_SIZE>;

// ---------------------------------------------------------------------------
// Puzzle definition
// ---------------------------------------------------------------------------

struct Orbit { string name; int size, orient_count; };
static vector<Orbit> orbits;
static map<string, int> orbit_idx;
static set<string> orbit_names;
static int num_orbits = 0;
static int centers_oi = -1;
static int corners_oi = -1;   // only orbit with orient_count > 1

struct Effect { Perm perm; Perm orient; };
static map<string, vector<Effect>> move_table;

// ---------------------------------------------------------------------------
// Deduplication set (keyed on CENTERS perm only)
// ---------------------------------------------------------------------------

struct PermHash {
    size_t operator()(const Perm &p) const noexcept {
        uint64_t w[3];
        memcpy(w, p.data(), 24);
        size_t h = w[0];
        h ^= w[1] * 0x9e3779b97f4a7c15ULL;
        h ^= w[2] * 0x6c62272e07bb0142ULL;
        return h;
    }
};
using SeenSet = std::unordered_set<Perm, PermHash>;

// ---------------------------------------------------------------------------
// Pre-built per-token effect for all orbits
// ---------------------------------------------------------------------------

struct TokenEntry {
    Perm perms[MAX_ORBITS];           // perms[oi] for each orbit
    Perm orient;                       // CORNERS orientation
    uint8_t nonid_count;               // number of orbits with non-identity perm
    uint8_t nonid_orbits[MAX_ORBITS];  // indices of those orbits
    bool has_corners_orient;           // true if orient is non-zero
};
static std::unordered_map<uint32_t, TokenEntry> token_table;
static std::unordered_map<uint32_t, string>     key_to_name;

static inline uint32_t pack_tok(const char *s, int len) {
    uint32_t k = 0;
    for (int i = 0; i < len; i++) k = (k << 8) | (uchar)s[i];
    return k;
}

// Per-line accumulator state (all orbits together)
struct LineState {
    Perm perms[MAX_ORBITS];
    Perm orient;   // CORNERS orientation only
};

static LineState identity_template;   // filled at startup; memcpy'd per line

static void init_identity_template() {
    for (int oi = 0; oi < num_orbits; oi++)
        for (int i = 0; i < PERM_SIZE; i++) identity_template.perms[oi][i] = (uchar)i;
    identity_template.orient.fill(0);
}

static const uchar add3[5] = {0, 1, 2, 0, 1};

static inline void apply_token(LineState &s, const TokenEntry &t) {
    for (int k = 0; k < t.nonid_count; k++) {
        int oi = t.nonid_orbits[k];
        Perm np;
        for (int i = 0; i < PERM_SIZE; i++) np[i] = s.perms[oi][t.perms[oi][i]];
        s.perms[oi] = np;
    }
    if (t.has_corners_orient) {
        Perm no;
        const Perm &tp = t.perms[corners_oi];
        for (int i = 0; i < PERM_SIZE; i++)
            no[i] = add3[s.orient[tp[i]] + t.orient[i]];
        s.orient = no;
    }
}

// Returns true iff all non-CENTERS orbits are identity.
static bool non_centers_is_id(const LineState &s) {
    for (int oi = 0; oi < num_orbits; oi++) {
        if (oi == centers_oi) continue;
        for (int i = 0; i < PERM_SIZE; i++)
            if (s.perms[oi][i] != (uchar)i) return false;
    }
    if (corners_oi >= 0)
        for (int i = 0; i < PERM_SIZE; i++)
            if (s.orient[i]) return false;
    return true;
}

// ---------------------------------------------------------------------------
// Perm utilities (used at startup only)
// ---------------------------------------------------------------------------

static Perm id_perm() {
    Perm r;
    for (int i = 0; i < PERM_SIZE; i++) r[i] = (uchar)i;
    return r;
}

static Perm compose_perm(const Perm &a, const Perm &b) {
    Perm r;
    for (int i = 0; i < PERM_SIZE; i++) r[i] = a[b[i]];
    return r;
}

static Perm inv_perm(const Perm &p) {
    Perm r;
    for (int i = 0; i < PERM_SIZE; i++) r[p[i]] = (uchar)i;
    return r;
}

static Perm compose_orient(const Perm &a_p, const Perm &a_o,
                            const Perm &b_p, const Perm &b_o, int oc) {
    Perm r;
    for (int i = 0; i < PERM_SIZE; i++)
        r[i] = (uchar)((a_o[b_p[i]] + b_o[i]) % oc);
    return r;
}

static Perm inv_orient(const Perm &p, const Perm &o, int oc) {
    Perm r; r.fill(0);
    for (int i = 0; i < PERM_SIZE; i++)
        r[p[i]] = (uchar)((oc - o[i]) % oc);
    return r;
}

// ---------------------------------------------------------------------------
// Rotation conjugation for -U (unique-up-to-rotation-and-inversion) mode
// ---------------------------------------------------------------------------

static Perm rotations[24];
static Perm inv_rotations[24];
static int  num_rotations = 0;

static void generate_rotations() {
    // Collect generators from the x, y, z whole-cube rotation moves.
    vector<Perm> gens;
    Perm id = id_perm();
    for (const char *name : {"x", "y", "z"}) {
        uint32_t k = pack_tok(name, (int)strlen(name));
        auto it = token_table.find(k);
        if (it == token_table.end()) continue;
        const Perm &p = it->second.perms[centers_oi];
        if (p == id) continue;
        gens.push_back(p);
        gens.push_back(inv_perm(p));
        gens.push_back(compose_perm(p, p));
    }
    if (gens.empty()) {
        fprintf(stderr, "No x/y/z rotation moves found; -U requires them.\n"); exit(1);
    }

    // BFS to enumerate the full rotation group on CENTERS.
    vector<Perm> queue = {id};
    std::set<Perm> seen_set = {id};
    for (int qi = 0; qi < (int)queue.size(); qi++) {
        for (const Perm &g : gens) {
            Perm next = compose_perm(queue[qi], g);
            if (seen_set.insert(next).second) queue.push_back(next);
        }
    }
    num_rotations = (int)queue.size();
    if (num_rotations != 24)
        fprintf(stderr, "Warning: found %d rotations (expected 24).\n", num_rotations);

    for (int i = 0; i < num_rotations; i++) {
        rotations[i]     = queue[i];
        inv_rotations[i] = inv_perm(queue[i]);
    }
}

// Return the size of the equivalence class of c under the 48-element group
// (24 rotations × {identity, inversion}), via the orbit-stabilizer theorem.
static int class_size(const Perm &c) {
    Perm ic = inv_perm(c);
    int stab = 0;
    for (int r = 0; r < num_rotations; r++) {
        const Perm &R  = rotations[r];
        const Perm &Ri = inv_rotations[r];
        bool c_fixed = true, ic_fixed = true;
        for (int i = 0; i < PERM_SIZE; i++) {
            if (c_fixed  && R[c [Ri[i]]] != c[i]) c_fixed  = false;
            if (ic_fixed && R[ic[Ri[i]]] != c[i]) ic_fixed = false;
            if (!c_fixed && !ic_fixed) break;
        }
        if (c_fixed)  stab++;
        if (ic_fixed) stab++;
    }
    return 48 / stab;
}

// Return the lex-min representative of P under rotation conjugacy and inversion.
static Perm canonicalize(const Perm &P) {
    Perm inv_P = inv_perm(P);
    Perm best  = P;
    for (int r = 0; r < num_rotations; r++) {
        const Perm &R  = rotations[r];
        const Perm &Ri = inv_rotations[r];
        Perm conj, inv_conj;
        for (int i = 0; i < PERM_SIZE; i++) {
            conj[i]     = R[    P[Ri[i]]];
            inv_conj[i] = R[inv_P[Ri[i]]];
        }
        if (conj     < best) best = conj;
        if (inv_conj < best) best = inv_conj;
    }
    return best;
}

// For -x: maps from input token key to output token name, for each of the 48 group elements.
// Elements 0-23 are pure rotations; elements 24-47 are inversion followed by rotation.
static std::unordered_map<uint32_t, string> expand_strs[48];

// Replace the face letter (U/L/F/R/B/D) in a token name using face_map_inv.
static string apply_face_map(const string &name, const int face_map_inv[6]) {
    static const char fl[] = "ULFRBD";
    string r = name;
    for (char &c : r)
        for (int f = 0; f < 6; f++)
            if (c == fl[f]) { c = fl[face_map_inv[f]]; return r; }
    return r;  // no face letter (e.g., y, z) — return unchanged
}

// Invert a token name: F→F', F'→F, F2→F2 (self-inverse), etc.
static string invert_name(const string &name) {
    if (!name.empty() && name.back() == '\'') return name.substr(0, name.size() - 1);
    if (!name.empty() && name.back() == '2')  return name;
    return name + "'";
}

// Build expand_strs after generate_rotations() has been called.
// Face order: U=0, L=1, F=2, R=3, B=4, D=5 (Speffz: face f → pieces f*4..f*4+3).
static void build_expand_maps() {
    for (int r = 0; r < num_rotations; r++) {
        // face_map[f] = which face f's pieces land on after rotation r
        int face_map[6], face_map_inv[6];
        for (int f = 0; f < 6; f++) {
            int p = f * 4;
            for (int pos = 0; pos < PERM_SIZE; pos++)
                if (rotations[r][pos] == (uchar)p) { face_map[f] = pos / 4; break; }
        }
        for (int f = 0; f < 6; f++) face_map_inv[face_map[f]] = f;

        for (auto &[key, name] : key_to_name) {
            // Pure rotation (group element r): conjugate T → R·T·R⁻¹
            expand_strs[r][key] = apply_face_map(name, face_map_inv);
            // Inversion + rotation (group element r+24): conjugate T → R·T⁻¹·R⁻¹
            expand_strs[r + 24][key] = apply_face_map(invert_name(name), face_map_inv);
        }
    }
}

// ---------------------------------------------------------------------------
// Embedded 6.tws puzzle definition (6x6x6 Rubik's Cube)
// ---------------------------------------------------------------------------
static const char g_builtin_tws[] = R"TWS(
# PuzzleGeometry 0.1 Copyright 2018 Tomas Rokicki.
Name PuzzleGeometryPuzzle
Set CORNERS 8 3
Set EDGES 24 1
Set EDGES2 24 1
Set CENTERS 24 1
Set CENTERS2 24 1
Set CENTERS3 24 1
Set CENTERS4 24 1

Solved
CORNERS
1 2 3 4 5 6 7 8
EDGES
1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
EDGES2
1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
CENTERS
1 1 1 1 2 2 2 2 3 3 3 3 4 4 4 4 5 5 5 5 6 6 6 6
CENTERS2
1 1 1 1 2 2 2 2 3 3 3 3 4 4 4 4 5 5 5 5 6 6 6 6
CENTERS3
1 1 1 1 2 2 2 2 3 3 3 3 4 4 4 4 5 5 5 5 6 6 6 6
CENTERS4
1 1 1 1 2 2 2 2 3 3 3 3 4 4 4 4 5 5 5 5 6 6 6 6
End

Move F
CORNERS
4 2 3 6 1 5 7 8
2 0 0 1 1 2 0 0
EDGES
1 2 6 4 5 21 7 8 12 9 10 11 13 14 15 3 17 18 19 20 16 22 23 24
EDGES2
1 2 6 4 5 21 7 8 12 9 10 11 13 14 15 3 17 18 19 20 16 22 23 24
CENTERS
1 2 3 4 5 6 7 8 12 9 10 11 13 14 15 16 17 18 19 20 21 22 23 24
CENTERS2
1 2 3 4 5 6 7 8 12 9 10 11 13 14 15 16 17 18 19 20 21 22 23 24
CENTERS3
1 2 3 4 5 6 7 8 12 9 10 11 13 14 15 16 17 18 19 20 21 22 23 24
CENTERS4
1 2 3 4 5 6 7 8 12 9 10 11 13 14 15 16 17 18 19 20 21 22 23 24
End

Move 2F
EDGES
1 2 3 7 5 6 22 8 9 10 11 12 4 14 15 16 17 18 19 20 21 13 23 24
CENTERS
1 2 6 4 5 21 7 8 9 10 11 12 13 14 15 3 17 18 19 20 16 22 23 24
CENTERS3
1 2 3 7 5 6 22 8 9 10 11 12 4 14 15 16 17 18 19 20 21 13 23 24
CENTERS4
1 2 6 7 5 21 22 8 9 10 11 12 4 14 15 3 17 18 19 20 16 13 23 24
End

Move 3F
EDGES2
1 2 3 7 5 6 22 8 9 10 11 12 4 14 15 16 17 18 19 20 21 13 23 24
CENTERS
1 2 3 7 5 6 22 8 9 10 11 12 4 14 15 16 17 18 19 20 21 13 23 24
CENTERS2
1 2 6 7 5 21 22 8 9 10 11 12 4 14 15 3 17 18 19 20 16 13 23 24
CENTERS3
1 2 6 4 5 21 7 8 9 10 11 12 13 14 15 3 17 18 19 20 16 22 23 24
End

Move 3B
EDGES2
1 15 3 4 2 6 7 8 9 10 11 12 13 14 24 16 17 18 19 20 21 22 23 5
CENTERS
1 15 3 4 2 6 7 8 9 10 11 12 13 14 24 16 17 18 19 20 21 22 23 5
CENTERS2
14 15 3 4 2 6 7 1 9 10 11 12 13 23 24 16 17 18 19 20 21 22 8 5
CENTERS3
14 2 3 4 5 6 7 1 9 10 11 12 13 23 15 16 17 18 19 20 21 22 8 24
End

Move 2B
EDGES
1 15 3 4 2 6 7 8 9 10 11 12 13 14 24 16 17 18 19 20 21 22 23 5
CENTERS
14 2 3 4 5 6 7 1 9 10 11 12 13 23 15 16 17 18 19 20 21 22 8 24
CENTERS3
1 15 3 4 2 6 7 8 9 10 11 12 13 14 24 16 17 18 19 20 21 22 23 5
CENTERS4
14 15 3 4 2 6 7 1 9 10 11 12 13 23 24 16 17 18 19 20 21 22 8 5
End

Move B
CORNERS
1 8 2 4 5 6 3 7
0 1 2 0 0 0 1 2
EDGES
14 2 3 4 5 6 7 1 9 10 11 12 13 23 15 16 20 17 18 19 21 22 8 24
EDGES2
14 2 3 4 5 6 7 1 9 10 11 12 13 23 15 16 20 17 18 19 21 22 8 24
CENTERS
1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 20 17 18 19 21 22 23 24
CENTERS2
1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 20 17 18 19 21 22 23 24
CENTERS3
1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 20 17 18 19 21 22 23 24
CENTERS4
1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 20 17 18 19 21 22 23 24
End

Move z
CORNERS
4 3 7 6 1 5 8 2
2 1 2 1 1 2 1 2
EDGES
8 5 6 7 24 21 22 23 12 9 10 11 4 1 2 3 18 19 20 17 16 13 14 15
EDGES2
8 5 6 7 24 21 22 23 12 9 10 11 4 1 2 3 18 19 20 17 16 13 14 15
CENTERS
8 5 6 7 24 21 22 23 12 9 10 11 4 1 2 3 18 19 20 17 16 13 14 15
CENTERS2
8 5 6 7 24 21 22 23 12 9 10 11 4 1 2 3 18 19 20 17 16 13 14 15
CENTERS3
8 5 6 7 24 21 22 23 12 9 10 11 4 1 2 3 18 19 20 17 16 13 14 15
CENTERS4
8 5 6 7 24 21 22 23 12 9 10 11 4 1 2 3 18 19 20 17 16 13 14 15
End

Move D
CORNERS
1 2 3 4 6 7 8 5
EDGES
1 2 3 4 5 6 19 8 9 10 7 12 13 14 11 16 17 18 15 20 24 21 22 23
EDGES2
1 2 3 4 5 6 19 8 9 10 7 12 13 14 11 16 17 18 15 20 24 21 22 23
CENTERS
1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 24 21 22 23
CENTERS2
1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 24 21 22 23
CENTERS3
1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 24 21 22 23
CENTERS4
1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 24 21 22 23
End

Move 2D
EDGES
1 2 3 4 5 6 7 20 9 10 11 8 13 14 15 12 17 18 19 16 21 22 23 24
CENTERS
1 2 3 4 5 6 19 8 9 10 7 12 13 14 11 16 17 18 15 20 21 22 23 24
CENTERS3
1 2 3 4 5 6 7 20 9 10 11 8 13 14 15 12 17 18 19 16 21 22 23 24
CENTERS4
1 2 3 4 5 6 19 20 9 10 7 8 13 14 11 12 17 18 15 16 21 22 23 24
End

Move 3D
EDGES2
1 2 3 4 5 6 7 20 9 10 11 8 13 14 15 12 17 18 19 16 21 22 23 24
CENTERS
1 2 3 4 5 6 7 20 9 10 11 8 13 14 15 12 17 18 19 16 21 22 23 24
CENTERS2
1 2 3 4 5 6 19 20 9 10 7 8 13 14 11 12 17 18 15 16 21 22 23 24
CENTERS3
1 2 3 4 5 6 19 8 9 10 7 12 13 14 11 16 17 18 15 20 21 22 23 24
End

Move 3U
EDGES2
1 2 3 4 5 10 7 8 9 14 11 12 13 18 15 16 17 6 19 20 21 22 23 24
CENTERS
1 2 3 4 5 10 7 8 9 14 11 12 13 18 15 16 17 6 19 20 21 22 23 24
CENTERS2
1 2 3 4 9 10 7 8 13 14 11 12 17 18 15 16 5 6 19 20 21 22 23 24
CENTERS3
1 2 3 4 9 6 7 8 13 10 11 12 17 14 15 16 5 18 19 20 21 22 23 24
End

Move 2U
EDGES
1 2 3 4 5 10 7 8 9 14 11 12 13 18 15 16 17 6 19 20 21 22 23 24
CENTERS
1 2 3 4 9 6 7 8 13 10 11 12 17 14 15 16 5 18 19 20 21 22 23 24
CENTERS3
1 2 3 4 5 10 7 8 9 14 11 12 13 18 15 16 17 6 19 20 21 22 23 24
CENTERS4
1 2 3 4 9 10 7 8 13 14 11 12 17 18 15 16 5 6 19 20 21 22 23 24
End

Move U
CORNERS
2 3 4 1 5 6 7 8
EDGES
4 1 2 3 9 6 7 8 13 10 11 12 17 14 15 16 5 18 19 20 21 22 23 24
EDGES2
4 1 2 3 9 6 7 8 13 10 11 12 17 14 15 16 5 18 19 20 21 22 23 24
CENTERS
4 1 2 3 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
CENTERS2
4 1 2 3 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
CENTERS3
4 1 2 3 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
CENTERS4
4 1 2 3 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
End

Move y
CORNERS
2 3 4 1 8 5 6 7
EDGES
4 1 2 3 9 10 11 12 13 14 15 16 17 18 19 20 5 6 7 8 22 23 24 21
EDGES2
4 1 2 3 9 10 11 12 13 14 15 16 17 18 19 20 5 6 7 8 22 23 24 21
CENTERS
4 1 2 3 9 10 11 12 13 14 15 16 17 18 19 20 5 6 7 8 22 23 24 21
CENTERS2
4 1 2 3 9 10 11 12 13 14 15 16 17 18 19 20 5 6 7 8 22 23 24 21
CENTERS3
4 1 2 3 9 10 11 12 13 14 15 16 17 18 19 20 5 6 7 8 22 23 24 21
CENTERS4
4 1 2 3 9 10 11 12 13 14 15 16 17 18 19 20 5 6 7 8 22 23 24 21
End

Move L
CORNERS
1 2 7 3 5 4 6 8
0 0 1 2 0 1 2 0
EDGES
1 2 3 18 8 5 6 7 9 10 11 4 13 14 15 16 17 24 19 20 21 22 23 12
EDGES2
1 2 3 18 8 5 6 7 9 10 11 4 13 14 15 16 17 24 19 20 21 22 23 12
CENTERS
1 2 3 4 8 5 6 7 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
CENTERS2
1 2 3 4 8 5 6 7 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
CENTERS3
1 2 3 4 8 5 6 7 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
CENTERS4
1 2 3 4 8 5 6 7 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
End

Move 2L
EDGES
19 2 3 4 5 6 7 8 1 10 11 12 13 14 15 16 17 18 21 20 9 22 23 24
CENTERS
1 2 3 18 5 6 7 8 9 10 11 4 13 14 15 16 17 24 19 20 21 22 23 12
CENTERS3
19 2 3 4 5 6 7 8 1 10 11 12 13 14 15 16 17 18 21 20 9 22 23 24
CENTERS4
19 2 3 18 5 6 7 8 1 10 11 4 13 14 15 16 17 24 21 20 9 22 23 12
End

Move 3L
EDGES2
19 2 3 4 5 6 7 8 1 10 11 12 13 14 15 16 17 18 21 20 9 22 23 24
CENTERS
19 2 3 4 5 6 7 8 1 10 11 12 13 14 15 16 17 18 21 20 9 22 23 24
CENTERS2
19 2 3 18 5 6 7 8 1 10 11 4 13 14 15 16 17 24 21 20 9 22 23 12
CENTERS3
1 2 3 18 5 6 7 8 9 10 11 4 13 14 15 16 17 24 19 20 21 22 23 12
End

Move 3R
EDGES2
1 2 11 4 5 6 7 8 9 10 23 12 13 14 15 16 3 18 19 20 21 22 17 24
CENTERS
1 2 11 4 5 6 7 8 9 10 23 12 13 14 15 16 3 18 19 20 21 22 17 24
CENTERS2
1 10 11 4 5 6 7 8 9 22 23 12 13 14 15 16 3 18 19 2 21 20 17 24
CENTERS3
1 10 3 4 5 6 7 8 9 22 11 12 13 14 15 16 17 18 19 2 21 20 23 24
End

Move 2R
EDGES
1 2 11 4 5 6 7 8 9 10 23 12 13 14 15 16 3 18 19 20 21 22 17 24
CENTERS
1 10 3 4 5 6 7 8 9 22 11 12 13 14 15 16 17 18 19 2 21 20 23 24
CENTERS3
1 2 11 4 5 6 7 8 9 10 23 12 13 14 15 16 3 18 19 20 21 22 17 24
CENTERS4
1 10 11 4 5 6 7 8 9 22 23 12 13 14 15 16 3 18 19 2 21 20 17 24
End

Move R
CORNERS
5 1 3 4 8 6 7 2
1 2 0 0 2 0 0 1
EDGES
1 10 3 4 5 6 7 8 9 22 11 12 16 13 14 15 17 18 19 2 21 20 23 24
EDGES2
1 10 3 4 5 6 7 8 9 22 11 12 16 13 14 15 17 18 19 2 21 20 23 24
CENTERS
1 2 3 4 5 6 7 8 9 10 11 12 16 13 14 15 17 18 19 20 21 22 23 24
CENTERS2
1 2 3 4 5 6 7 8 9 10 11 12 16 13 14 15 17 18 19 20 21 22 23 24
CENTERS3
1 2 3 4 5 6 7 8 9 10 11 12 16 13 14 15 17 18 19 20 21 22 23 24
CENTERS4
1 2 3 4 5 6 7 8 9 10 11 12 16 13 14 15 17 18 19 20 21 22 23 24
End

Move x
CORNERS
5 1 4 6 8 7 3 2
1 2 1 2 2 1 2 1
EDGES
9 10 11 12 6 7 8 5 21 22 23 24 16 13 14 15 3 4 1 2 19 20 17 18
EDGES2
9 10 11 12 6 7 8 5 21 22 23 24 16 13 14 15 3 4 1 2 19 20 17 18
CENTERS
9 10 11 12 6 7 8 5 21 22 23 24 16 13 14 15 3 4 1 2 19 20 17 18
CENTERS2
9 10 11 12 6 7 8 5 21 22 23 24 16 13 14 15 3 4 1 2 19 20 17 18
CENTERS3
9 10 11 12 6 7 8 5 21 22 23 24 16 13 14 15 3 4 1 2 19 20 17 18
CENTERS4
9 10 11 12 6 7 8 5 21 22 23 24 16 13 14 15 3 4 1 2 19 20 17 18
End
)TWS";

// .tws parser (startup only)
// ---------------------------------------------------------------------------

static void read_tws(std::istream &f) {
    const string path = "<builtin 6.tws>";

    string line, buf;
    bool has_buf = false;

    auto consume = [&]() -> bool {
        if (has_buf) { line = buf; has_buf = false; return true; }
        while (std::getline(f, line)) {
            while (!line.empty() && (uchar)line.back() <= ' ') line.pop_back();
            auto s = line.find_first_not_of(" \t");
            line = (s == string::npos) ? "" : line.substr(s);
            if (!line.empty() && line[0] != '#') return true;
        }
        return false;
    };

    auto peek_into = [&](string &out) -> bool {
        if (!has_buf) { has_buf = consume(); buf = line; }
        if (has_buf) { out = buf; return true; }
        return false;
    };

    auto parse_perm = [](const string &s, int n) -> Perm {
        Perm r = id_perm();
        std::istringstream ss(s);
        int v;
        for (int i = 0; i < n; i++) {
            if (!(ss >> v)) { std::cerr << "Too few values in permutation\n"; exit(1); }
            r[i] = (uchar)(v - 1);
        }
        if (ss >> v) { std::cerr << "Too many values in permutation\n"; exit(1); }
        return r;
    };

    while (consume()) {
        std::istringstream ss(line);
        string kw; ss >> kw;

        if (kw == "Set") {
            Orbit o; ss >> o.name >> o.size >> o.orient_count;
            if (o.size > PERM_SIZE) {
                std::cerr << "Orbit " << o.name << " has size " << o.size
                          << " > " << PERM_SIZE << "; recompile with larger PERM_SIZE\n";
                exit(1);
            }
            if ((int)orbits.size() >= MAX_ORBITS) {
                std::cerr << "Too many orbits (max " << MAX_ORBITS << ")\n"; exit(1);
            }
            if (o.orient_count > 1) {
                if (corners_oi >= 0) {
                    std::cerr << "Multiple orbits with orient_count > 1 not supported\n"; exit(1);
                }
                corners_oi = (int)orbits.size();
            }
            orbit_idx[o.name] = (int)orbits.size();
            orbit_names.insert(o.name);
            orbits.push_back(o);

        } else if (kw == "Solved") {
            while (consume() && line != "End") {}

        } else if (kw == "Move") {
            string mname; ss >> mname;
            vector<Effect> effs(orbits.size());
            for (auto &e : effs) { e.perm = id_perm(); e.orient.fill(0); }

            string peek;
            while (peek_into(peek) && peek != "End") {
                consume();
                if (!orbit_names.count(line)) {
                    std::cerr << "Unknown orbit '" << line << "' in move " << mname << "\n";
                    exit(1);
                }
                int oi = orbit_idx[line];
                const Orbit &orb = orbits[oi];
                if (!consume()) { std::cerr << "Unexpected EOF in " << path << "\n"; exit(1); }
                effs[oi].perm = parse_perm(line, orb.size);
                if (orb.orient_count > 1) {
                    string nxt;
                    if (peek_into(nxt) && !nxt.empty() && isdigit((uchar)nxt[0])) {
                        consume();
                        std::istringstream os(nxt);
                        for (int i = 0; i < orb.size; i++) {
                            int v; os >> v; effs[oi].orient[i] = (uchar)v;
                        }
                    }
                }
            }
            consume();
            move_table[mname] = std::move(effs);
        }
    }

    num_orbits = (int)orbits.size();

    if (!orbit_idx.count("CENTERS")) {
        std::cerr << "No CENTERS orbit in " << path << "\n"; exit(1);
    }
    centers_oi = orbit_idx["CENTERS"];
    if (orbits[centers_oi].size != PERM_SIZE) {
        std::cerr << "CENTERS orbit size is " << orbits[centers_oi].size
                  << ", expected " << PERM_SIZE << "\n";
        exit(1);
    }
}

// ---------------------------------------------------------------------------
// Token table: build once after .tws is parsed
// ---------------------------------------------------------------------------

static void build_token_table_for(const string &base, const vector<Effect> &effs, int power) {
    string tok = base + (power == 1 ? "" : power == -1 ? "'" : "2");
    uint32_t key = pack_tok(tok.c_str(), (int)tok.size());

    TokenEntry e;
    for (int oi = 0; oi < num_orbits; oi++) {
        const Perm &p = effs[oi].perm;
        if (power == 1)       e.perms[oi] = p;
        else if (power == 2)  e.perms[oi] = compose_perm(p, p);
        else                  e.perms[oi] = inv_perm(p);
    }
    if (corners_oi >= 0) {
        const Perm &cp = effs[corners_oi].perm;
        const Perm &co = effs[corners_oi].orient;
        if (power == 1) {
            e.orient = co;
        } else if (power == 2) {
            e.orient = compose_orient(cp, co, cp, co, 3);
        } else {
            e.orient = inv_orient(cp, co, 3);
        }
    } else {
        e.orient.fill(0);
    }
    // Identify which orbits have a non-identity perm.
    Perm id = id_perm();
    Perm zero; zero.fill(0);
    e.nonid_count = 0;
    for (int oi = 0; oi < num_orbits; oi++)
        if (e.perms[oi] != id) e.nonid_orbits[e.nonid_count++] = (uint8_t)oi;
    // Orient update needed when CORNERS perm is non-id (shuffles existing orient values)
    // OR orient is non-zero (adds twist). Skip only when both are trivial.
    e.has_corners_orient = (corners_oi >= 0) &&
        (e.perms[corners_oi] != id || e.orient != zero);

    token_table[key] = e;
    key_to_name[key] = tok;
}

static void build_token_table() {
    for (auto &[base, effs] : move_table) {
        build_token_table_for(base, effs,  1);
        build_token_table_for(base, effs,  2);
        build_token_table_for(base, effs, -1);
    }
}

// ---------------------------------------------------------------------------
// Mode flags (declared here so I/O helpers can reference them)
// ---------------------------------------------------------------------------

static int check_mode  = 0;   // -C: verify sequences only affect CENTERS
static int u_mode      = 0;   // -u: exact CENTERS-perm dedup
static int unique_mode = 0;   // -U: dedup up to rotation/inversion
static int S_mode      = 0;   // -S: prefix with equivalence class size
static int expand_mode = 0;   // -x: expand to all 48 conjugations
static int xu_mode     = 0;   // -xu: expand, dedup within each algorithm only
static int cost_mode   = 0;   // -c: prefix with move cost (v h o c)
static int perm_mode   = 0;   // -p: prefix with perm (1=array, 2=cycle)
static int filter_mode = 0;
static int filter_conj_mode = 0; // -F: drop sequences whose first/last moves are inverse face moves
static int g_mode      = 0;   // -g: greedy expansion via face-move conjugation
static long long d_multiplier = -1; // -1 = not set; 0 = unit cost (emit 1); >0 = collapse multiplier
static long long max_cost_filter = -1; // -m N: skip algorithms with collapsed cost > N

// Priority queue entry for -g mode.
struct GEntry {
    long long sort_cost;
    int v, h, o, c;
    Perm perm;
    vector<uint32_t> tokens;
    bool operator>(const GEntry &x) const { return sort_cost > x.sort_cost; }
};

// The 18 outer face moves used for conjugation in -g mode.
struct FaceMove18 { uint32_t key, inv_key; Perm perm, perm_inv; };
static FaceMove18 face_moves_18[18];
static int n_face_moves = 0;

static void build_face_moves_18() {
    static const char *names[] = {
        "U", "U'", "U2", "F", "F'", "F2", "R", "R'", "R2",
        "L", "L'", "L2", "B", "B'", "B2", "D", "D'", "D2"
    };
    for (const char *nm : names) {
        uint32_t k = pack_tok(nm, (int)strlen(nm));
        auto it = token_table.find(k);
        if (it == token_table.end()) {
            fprintf(stderr, "Warning: face move '%s' not in puzzle\n", nm);
            continue;
        }
        string inv_n = invert_name(nm);
        uint32_t inv_k = pack_tok(inv_n.c_str(), (int)inv_n.size());
        Perm mp = it->second.perms[centers_oi];
        face_moves_18[n_face_moves++] = { k, inv_k, mp, inv_perm(mp) };
    }
}

// ---------------------------------------------------------------------------
// Fast I/O buffers
// ---------------------------------------------------------------------------

static constexpr int IBUF = 1 << 23;   // 8 MB read buffer
static constexpr int OBUF = 1 << 23;   // 8 MB write buffer
static char ibuf[IBUF], obuf[OBUF];
static int  ipos = 0, ilen = 0, opos = 0;

static void flush_out() {
    if (opos) { fwrite(obuf, 1, opos, stdout); opos = 0; }
}

static long long collapse_cost(int v, int h, int o, int c, long long d) {
    return c + d * (o + d * (h + d * v));
}

// Write a prefix string (size, cost, and/or perm) into buf; return length.
static int build_prefix(char *buf, int bufsz, const Perm *perm,
                         int sz, int v, int h, int o, int c) {
    int n = 0;
    if (S_mode)    n += snprintf(buf + n, bufsz - n, "%d ", sz);
    if (cost_mode) {
        if (d_multiplier > 0)
            n += snprintf(buf + n, bufsz - n, "%lld ", collapse_cost(v, h, o, c, d_multiplier));
        else if (d_multiplier == 0)
            n += snprintf(buf + n, bufsz - n, "1 ");
        else
            n += snprintf(buf + n, bufsz - n, "%d %d %d %d ", v, h, o, c);
    }
    if (perm_mode && perm) {
        if (perm_mode == 1) {
            buf[n++] = '[';
            for (int i = 0; i < PERM_SIZE; i++) {
                if (i > 0) buf[n++] = ' ';
                n += snprintf(buf + n, bufsz - n, "%d", (int)(*perm)[i]);
            }
            buf[n++] = ']'; buf[n++] = ' ';
        } else {
            bool visited[PERM_SIZE] = {};
            bool any = false;
            for (int i = 0; i < PERM_SIZE; i++) {
                if (visited[i] || (*perm)[i] == (uchar)i) continue;
                buf[n++] = '(';
                int j = i; bool first = true;
                while (!visited[j]) {
                    visited[j] = true;
                    if (!first) buf[n++] = ' ';
                    first = false;
                    n += snprintf(buf + n, bufsz - n, "%d", j);
                    j = (*perm)[j];
                }
                buf[n++] = ')';
                any = true;
            }
            if (!any) { buf[n++] = '('; buf[n++] = ')'; }
            buf[n++] = ' ';
        }
    }
    return n;
}

// Emit a plain line with optional prefix.
static void emit_prefixed(const char *pfx, int plen, const char *s, int n) {
    if (opos + plen + n + 1 > OBUF) flush_out();
    if (plen) { memcpy(obuf + opos, pfx, plen); opos += plen; }
    memcpy(obuf + opos, s, n); opos += n;
    obuf[opos++] = '\n';
}

// Emit an algorithm from a token list (used in -g mode).
static void emit_g_alg(const vector<uint32_t> &toks, const char *pfx, int pfxlen) {
    if (pfxlen > 0) { if (opos + pfxlen > OBUF) flush_out(); memcpy(obuf + opos, pfx, pfxlen); opos += pfxlen; }
    bool first = true;
    for (uint32_t k : toks) {
        const string &s = key_to_name.at(k);
        if (!first) { if (opos + 1 > OBUF) flush_out(); obuf[opos++] = ' '; }
        first = false;
        int slen = (int)s.size();
        if (opos + slen > OBUF) flush_out();
        memcpy(obuf + opos, s.data(), slen); opos += slen;
    }
    if (opos + 1 > OBUF) flush_out();
    obuf[opos++] = '\n';
}

// Emit one expanded algorithm. g=0..23: pure rotation; g=24..47: inversion+rotation.
// For the inversion half the tokens are emitted in reverse order.
static void emit_expanded(const vector<uint32_t> &toks, int g,
                           const char *pfx = nullptr, int plen = 0) {
    bool inv = (g >= 24);
    if (plen > 0) { if (opos + plen > OBUF) flush_out(); memcpy(obuf + opos, pfx, plen); opos += plen; }
    bool first = true;
    int n = (int)toks.size();
    for (int idx = 0; idx < n; idx++) {
        uint32_t k = toks[inv ? n - 1 - idx : idx];
        const string &s = expand_strs[g].at(k);
        if (!first) { if (opos + 1 > OBUF) flush_out(); obuf[opos++] = ' '; }
        first = false;
        int slen = (int)s.size();
        if (opos + slen > OBUF) flush_out();
        memcpy(obuf + opos, s.data(), slen); opos += slen;
    }
    if (opos + 1 > OBUF) flush_out();
    obuf[opos++] = '\n';
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
static uint32_t filter_mask = 0;  // bitmask over positions 0-23; set bits = forbidden

// Compute the v/h/o/c cost of an algorithm from its token list.
// Strips face-move conjugations (matching inverse face-move pairs on the outside),
// then classifies inner moves: depth-3 → v, depth-2 → h, no-depth → o.
static void compute_cost(const vector<uint32_t> &toks,
                          int &v, int &h, int &o, int &c) {
    v = h = o = c = 0;
    int lo = 0, hi = (int)toks.size() - 1;
    while (lo < hi) {
        const string &front = key_to_name.at(toks[lo]);
        const string &back  = key_to_name.at(toks[hi]);
        if (!isalpha((uchar)front[0]) || !isalpha((uchar)back[0])) break;
        if (invert_name(front) != back) break;
        c += 2; lo++; hi--;
    }
    for (int i = lo; i <= hi; i++) {
        char d = key_to_name.at(toks[i])[0];
        if      (isalpha((uchar)d)) o++;
        else if (d == '2')           h++;
        else if (d == '3')           v++;
    }
}

// Face order: U(0-3) L(4-7) F(8-11) R(12-15) B(16-19) D(20-23) per Speffz
static void build_filter_mask(const char *key) {
    if (strlen(key) != 6) {
        fprintf(stderr, "-f key must be exactly 6 characters (ULFRBD order)\n"); exit(1);
    }
    filter_mask = 0;
    for (int f = 0; f < 6; f++)
        if (key[f] == 'x' || key[f] == 'X')
            for (int j = 0; j < 4; j++)
                filter_mask |= (1u << (f * 4 + j));
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        fprintf(stderr,
            "Usage: algutil [options] < input.algs > output.algs\n"
            "\n"
            "Reads move sequences from stdin, one per line.\n"
            "\n"
            "Filtering / deduplication:\n"
            "  -u          drop exact CENTERS-perm duplicates\n"
            "  -U          drop duplicates up to rotation and inversion\n"
            "  -f KEY      keep only sequences that fix the faces marked 'x'\n"
            "              KEY is 6 chars in ULFRBD order, '0'=active 'x'=fixed\n"
            "  -F          drop sequences whose outermost move pair is a conjugate\n"
            "  -m N        drop sequences whose collapsed cost exceeds N\n"
            "              (uses -d multiplier if set, otherwise 1000)\n"
            "\n"
            "Expansion:\n"
            "  -g          greedy conjugation expansion via 18 outer face moves\n"
            "  -x          expand each sequence to all 48 rotation/inversion conjugates\n"
            "  -xu         like -x but deduplicate within each input algorithm\n"
            "\n"
            "Cost annotation (prefixed to output lines):\n"
            "  -c          prefix with 'v h o c' move counts\n"
            "  -d N        with -c, collapse cost to single int (c + N*(o + N*(h + N*v)))\n"
            "  -pc         prefix with permutation in cycle notation\n"
            "  -p          prefix with permutation as array\n"
            "  -S          prefix with equivalence class size (requires -U or -x)\n"
            "\n"
            "Validation:\n"
            "  -C          verify every sequence affects only the CENTERS orbit\n"
        );
        return 1;
    }
    const char *filter_key = nullptr;
    for (int i = 1; i < argc; i++) {
        if      (string(argv[i]) == "-C") check_mode  = 1;
        else if (string(argv[i]) == "-c") cost_mode   = 1;
        else if (string(argv[i]) == "-u") u_mode      = 1;
        else if (string(argv[i]) == "-U") unique_mode = 1;
        else if (string(argv[i]) == "-S") S_mode      = 1;
        else if (string(argv[i]) == "-xu") { expand_mode = 1; xu_mode = 1; }
        else if (string(argv[i]) == "-x")  expand_mode = 1;
        else if (string(argv[i]) == "-pc") perm_mode = 2;
        else if (string(argv[i]) == "-p")  perm_mode = 1;
        else if (string(argv[i]) == "-g")  g_mode    = 1;
        else if (string(argv[i]) == "-d") {
            if (++i >= argc) { fprintf(stderr, "-d requires a numeric argument\n"); exit(1); }
            d_multiplier = atoll(argv[i]);
        }
        else if (string(argv[i]) == "-f") {
            if (++i >= argc) { fprintf(stderr, "-f requires a key argument\n"); exit(1); }
            filter_key = argv[i];
            filter_mode = 1;
        }
        else if (string(argv[i]) == "-F") filter_conj_mode = 1;
        else if (string(argv[i]) == "-m") {
            if (++i >= argc) { fprintf(stderr, "-m requires a numeric argument\n"); exit(1); }
            max_cost_filter = atoll(argv[i]);
        }
        else { fprintf(stderr, "Unknown argument: %s\n", argv[i]); exit(1); }
    }

    { std::istringstream ss(g_builtin_tws); read_tws(ss); }
    build_token_table();
    init_identity_template();
    if (unique_mode || S_mode || expand_mode) generate_rotations();
    if (filter_mode) build_filter_mask(filter_key);
    if (expand_mode) build_expand_maps();
    if (g_mode) build_face_moves_18();

    SeenSet seen;
    std::unordered_map<Perm, int, PermHash> seen_map;  // used in -U mode for class-size counts
    std::priority_queue<GEntry, vector<GEntry>, std::greater<GEntry>> pq;
    int lineno = 0, kept = 0, dupes = 0, errors = 0;

    static char line[4096];
    static vector<uint32_t> line_tokens;

    ilen = (int)fread(ibuf, 1, IBUF, stdin);

    for (;;) {
        // Read one line into line[].
        int llen = 0;
        for (;;) {
            if (ipos >= ilen) {
                ilen = (int)fread(ibuf, 1, IBUF, stdin);
                ipos = 0;
                if (!ilen) {
                    if (llen == 0) goto done;
                    break;   // partial last line with no trailing newline
                }
            }
            char c = ibuf[ipos++];
            if (c == '\n') break;
            if (c != '\r' && llen < (int)sizeof(line) - 1) line[llen++] = c;
        }
        line[llen] = '\0';
        ++lineno;

        // Skip leading whitespace.
        const char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) continue;
        const char *line_start = p;

        bool bad = false;
        Perm centers_state = identity_template.perms[centers_oi];
        if (expand_mode || cost_mode || g_mode || filter_conj_mode || max_cost_filter >= 0) line_tokens.clear();

        if (!check_mode) {
            // Fast path: compose only the CENTERS orbit.
            while (*p) {
                while (*p == ' ' || *p == '\t') p++;
                if (!*p) break;
                const char *tok = p;
                while (*p && *p != ' ' && *p != '\t') p++;
                int tlen = (int)(p - tok);
                uint32_t k = pack_tok(tok, tlen);
                auto it = token_table.find(k);
                if (it == token_table.end()) {
                    fprintf(stderr, "Error (line %d): unknown move '%.*s': %s\n",
                            lineno, tlen, tok, line_start);
                    bad = true; break;
                }
                if (expand_mode || cost_mode || g_mode || filter_conj_mode || max_cost_filter >= 0) line_tokens.push_back(k);
                centers_state = compose_perm(centers_state, it->second.perms[centers_oi]);
            }
        } else {
            // Check path: compose all orbits and verify non-CENTERS are identity.
            LineState state;
            memcpy(&state, &identity_template, sizeof(LineState));
            while (*p) {
                while (*p == ' ' || *p == '\t') p++;
                if (!*p) break;
                const char *tok = p;
                while (*p && *p != ' ' && *p != '\t') p++;
                int tlen = (int)(p - tok);
                uint32_t k2 = pack_tok(tok, tlen);
                auto it = token_table.find(k2);
                if (it == token_table.end()) {
                    fprintf(stderr, "Error (line %d): unknown move '%.*s': %s\n",
                            lineno, tlen, tok, line_start);
                    bad = true; break;
                }
                if (expand_mode || cost_mode || g_mode || filter_conj_mode || max_cost_filter >= 0) line_tokens.push_back(k2);
                apply_token(state, it->second);
            }
            if (!bad && !non_centers_is_id(state)) {
                fprintf(stderr, "Error (line %d): sequence affects non-CENTERS orbits: %s\n",
                        lineno, line_start);
                bad = true; ++errors;
            }
            centers_state = state.perms[centers_oi];
        }

        if (bad) { if (!check_mode) ++errors; continue; }

        if (filter_mode) {
            bool skip = false;
            uint32_t m = filter_mask;
            while (m) {
                int i = __builtin_ctz(m);
                if (centers_state[i] != (uchar)i) { skip = true; break; }
                m &= m - 1;
            }
            if (skip) continue;
        }

        if (filter_conj_mode && line_tokens.size() >= 2) {
            const string &front = key_to_name.at(line_tokens.front());
            const string &back  = key_to_name.at(line_tokens.back());
            if (isalpha((uchar)front[0]) && isalpha((uchar)back[0]) &&
                invert_name(front) == back)
                continue;
        }

        // In -g mode, push to priority queue; all processing happens after input is consumed.
        if (g_mode) {
            int cv = 0, ch = 0, co = 0, cc = 0;
            compute_cost(line_tokens, cv, ch, co, cc);
            long long g_d = d_multiplier > 0 ? d_multiplier : (d_multiplier == 0 ? 1LL : 100LL);
            GEntry e;
            e.sort_cost = collapse_cost(cv, ch, co, cc, g_d);
            e.v = cv; e.h = ch; e.o = co; e.c = cc;
            e.perm   = centers_state;
            e.tokens = line_tokens;
            pq.push(std::move(e));
            continue;
        }

        // Trim trailing whitespace for clean output.
        int out_len = (int)(p - line_start);
        while (out_len > 0 && (uchar)line_start[out_len-1] <= ' ') --out_len;

        // Build output prefix (shared by non-expand paths; expand builds per-expansion).
        int cv = 0, ch = 0, co = 0, cc = 0;
        if (cost_mode || max_cost_filter >= 0) compute_cost(line_tokens, cv, ch, co, cc);
        if (max_cost_filter >= 0) {
            long long d = d_multiplier > 0 ? d_multiplier : (d_multiplier == 0 ? 1LL : 1000LL);
            if (collapse_cost(cv, ch, co, cc, d) > max_cost_filter) continue;
        }
        int  sz = S_mode ? class_size(centers_state) : 0;
        char pfx[256];
        int  pfxlen = 0;

        if (expand_mode) {
            SeenSet local_seen;  // for -xu: resets per algorithm
            for (int g = 0; g < 48; g++) {
                // Compute CENTERS perm for this expansion: R · (inv?)base · R⁻¹
                bool inv = (g >= 24);
                int r = g % 24;
                Perm base = inv ? inv_perm(centers_state) : centers_state;
                Perm exp_c;
                for (int i = 0; i < PERM_SIZE; i++)
                    exp_c[i] = rotations[r][base[inv_rotations[r][i]]];

                // Apply -f filter on expanded perm
                if (filter_mode) {
                    bool skip = false;
                    uint32_t m = filter_mask;
                    while (m) {
                        int bi = __builtin_ctz(m);
                        if (exp_c[bi] != (uchar)bi) { skip = true; break; }
                        m &= m - 1;
                    }
                    if (skip) continue;
                }

                // Apply dedup if requested
                if (xu_mode) {
                    if (!local_seen.insert(exp_c).second) { ++dupes; continue; }
                } else if (unique_mode) {
                    Perm key = canonicalize(exp_c);
                    if (!seen.insert(key).second) { ++dupes; continue; }
                } else if (u_mode) {
                    if (!seen.insert(exp_c).second) { ++dupes; continue; }
                }

                int exp_sz = S_mode ? class_size(exp_c) : 0;
                pfxlen = build_prefix(pfx, sizeof(pfx), perm_mode ? &exp_c : nullptr,
                                      exp_sz, cv, ch, co, cc);
                emit_expanded(line_tokens, g, pfx, pfxlen);
                ++kept;
            }
            continue;
        }

        pfxlen = build_prefix(pfx, sizeof(pfx), perm_mode ? &centers_state : nullptr,
                              sz, cv, ch, co, cc);

        if (S_mode) {
            // Emit class size with every valid algorithm (optionally filtered by -U).
            if (unique_mode) {
                Perm key = canonicalize(centers_state);
                if (!seen.insert(key).second) { ++dupes; continue; }
            }
            emit_prefixed(pfx, pfxlen, line_start, out_len);
            ++kept;
        } else if (unique_mode) {
            Perm key = canonicalize(centers_state);
            auto [it, inserted] = seen_map.emplace(key, 0);
            it->second++;
            if (inserted) { emit_prefixed(pfx, pfxlen, line_start, out_len); ++kept; } else { ++dupes; }
        } else if (u_mode) {
            if (seen.insert(centers_state).second) { emit_prefixed(pfx, pfxlen, line_start, out_len); ++kept; } else { ++dupes; }
        } else {
            emit_prefixed(pfx, pfxlen, line_start, out_len);
            ++kept;
        }
    }
done:
    if (g_mode) {
        SeenSet g_seen;
        while (!pq.empty()) {
            GEntry e = std::move(const_cast<GEntry&>(pq.top())); pq.pop();

            Perm lookup = unique_mode ? canonicalize(e.perm) : e.perm;
            if (!g_seen.insert(lookup).second) { ++dupes; continue; }

            int sz = S_mode ? class_size(e.perm) : 0;
            char pfx[256];
            int pfxlen = build_prefix(pfx, sizeof(pfx),
                                      perm_mode ? &e.perm : nullptr,
                                      sz, e.v, e.h, e.o, e.c);
            emit_g_alg(e.tokens, pfx, pfxlen);
            ++kept;

            for (int fi = 0; fi < n_face_moves; fi++) {
                Perm cp;
                const Perm &M    = face_moves_18[fi].perm;
                const Perm &Minv = face_moves_18[fi].perm_inv;
                for (int i = 0; i < PERM_SIZE; i++) cp[i] = M[e.perm[Minv[i]]];

                Perm cp_lookup = unique_mode ? canonicalize(cp) : cp;
                if (g_seen.count(cp_lookup)) continue;

                GEntry ne;
                ne.sort_cost = e.sort_cost + 2;
                ne.v = e.v; ne.h = e.h; ne.o = e.o; ne.c = e.c + 2;
                ne.perm = cp;
                ne.tokens.reserve(e.tokens.size() + 2);
                ne.tokens.push_back(face_moves_18[fi].key);
                ne.tokens.insert(ne.tokens.end(), e.tokens.begin(), e.tokens.end());
                ne.tokens.push_back(face_moves_18[fi].inv_key);
                pq.push(std::move(ne));
            }
        }
    }
    flush_out();
    // fprintf(stderr, "kept=%d dupes=%d errors=%d\n", kept, dupes, errors);
    (void)dupes;
    if (unique_mode && !expand_mode) {
        std::map<int, int> dist;
        for (auto &[k, v] : seen_map) dist[v]++;
        long long check = 0;
        fprintf(stderr, "class_size  classes     sequences\n");
        for (auto &[size, count] : dist) {
            fprintf(stderr, "%10d %8d  %12d\n", size, count, size * count);
            check += (long long)size * count;
        }
        fprintf(stderr, "     total %8d  %12lld\n", kept, check);
    }
    return errors ? 1 : 0;
}
