#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using State = uint64_t;
static constexpr uint32_t BAD_RANK = std::numeric_limits<uint32_t>::max();

// > 0 = max activations per state (enables max-act mode via -k).
static int max_act_limit = 0;

// Zero-size tag used with [[no_unique_address]] to eliminate unused fields.
struct NoExt {};

// 2-bit packed activation-count array for max_act_limit <= 4.
// 4 entries per byte; 2 bits encode:
//   0 = "skip": uncovered state, initial state (count 0), or
//               count==4 saturated state (only for max_act_limit==4).
//   1 = count 1,  2 = count 2,  3 = count 3.
// The initial state is stored separately as init_rank and handled before the
// inner loop so its val=0 entry is never visited during normal iteration.
// For max_act_limit < 4, saturation is at val == max_act_limit (1/2/3); for
// max_act_limit == 4, saturation maps back to val=0 (identical to the skip
// bucket), which the loop skips without a separate saturation check.
// Only valid for max_act_limit in {1, 2, 3, 4}.
struct TwoBitCount {
    std::vector<uint8_t> data_;

    void assign_zero(size_t total) {
        data_.assign((total + 3u) >> 2, 0u);
    }

    inline uint8_t get(uint32_t i) const {
        return (data_[i >> 2] >> ((i & 3u) << 1u)) & 3u;
    }

    inline void set(uint32_t i, uint8_t val) {
        uint8_t& b = data_[i >> 2];
        int sh = static_cast<int>((i & 3u) << 1u);
        b = uint8_t((b & ~(3u << sh)) | ((val & 3u) << sh));
    }
};

// Extra fields present only in max-activation mode (M=true).
struct MaxActMatExt {
    uint64_t sum_sq = 0;
    uint32_t init_rank = BAD_RANK;  // rank of the single count-0 initial state
    TwoBitCount best_count;         // val 0=skip, 1-3=count; see TwoBitCount
};

static bool set_bit(std::vector<uint64_t>& bs, uint32_t idx) {
    uint64_t bit = 1ULL << (idx & 63);
    uint64_t& word = bs[idx >> 6];
    bool was = (word & bit) != 0;
    word |= bit;
    return !was;
}

static bool test_bit(const std::vector<uint64_t>& bs, uint32_t idx) {
    return (bs[idx >> 6] >> (idx & 63)) & 1ULL;
}

// Encodes a multipermutation as a base-B integer.  The base is chosen from
// the initial pattern, so the same code handles binary and multi-symbol
// inputs.  The chunk metadata also defines how permutation LUTs are built.
struct Codec {
    int n = 0;
    int base = 2;
    int chunk_size = 6;
    int nchunks = 0;
    std::vector<State> pow_base;
    std::vector<uint32_t> chunk_mod;

    void init(int n_, int base_) {
        n = n_;
        base = base_;
        if (n < 1 || n > 24) throw std::runtime_error("n must be 1..24");
        if (base < 2) throw std::runtime_error("base must be >= 2");

        // Keep per-chunk tables modest.  For base 2, chunks of 12 positions
        // are fine.
        // For larger bases, use smaller chunks so base^chunk stays small.
        chunk_size = 1;
        State v = static_cast<State>(base);
        while (chunk_size < 12) {
            if (v > 4096ULL / static_cast<State>(base)) break;
            v *= static_cast<State>(base);
            ++chunk_size;
        }
        if (chunk_size < 1) chunk_size = 1;
        nchunks = (n + chunk_size - 1) / chunk_size;

        pow_base.assign(n + 1, 1);
        for (int i = 1; i <= n; ++i) {
            if (pow_base[i - 1] >
                std::numeric_limits<State>::max() /
                    static_cast<State>(base)) {
                throw std::runtime_error(
                    "encoded state would overflow uint64_t");
            }
            pow_base[i] = pow_base[i-1] * static_cast<State>(base);
        }

        chunk_mod.assign(nchunks, 1);
        for (int c = 0; c < nchunks; ++c) {
            int len = std::min(chunk_size, n - c * chunk_size);
            State m = 1;
            for (int i = 0; i < len; ++i) m *= static_cast<State>(base);
            if (m > std::numeric_limits<uint32_t>::max()) {
                throw std::runtime_error("internal chunk modulus too large");
            }
            chunk_mod[c] = static_cast<uint32_t>(m);
        }
    }

    inline int digit(State s, int pos) const {
        return static_cast<int>(
            (s / pow_base[pos]) % static_cast<State>(base));
    }

    State encode_digits(const std::vector<int>& d) const {
        if (static_cast<int>(d.size()) != n) {
            throw std::runtime_error("wrong digit count");
        }
        State s = 0;
        for (int i = 0; i < n; ++i) {
            if (d[i] < 0 || d[i] >= base) {
                throw std::runtime_error("digit out of range");
            }
            s += static_cast<State>(d[i]) * pow_base[i];
        }
        return s;
    }

    std::string to_string(State s) const {
        std::string out;
        out.reserve(n);
        for (int i = 0; i < n; ++i) {
            int d = digit(s, i);
            if (d < 10) out.push_back(static_cast<char>('0' + d));
            else out.push_back(static_cast<char>('A' + (d - 10)));
        }
        return out;
    }
};

// Lookup table for applying one permutation to an encoded state.  Each
// chunk table maps a small block of base-B digits to its output contribution.
struct ChunkLUT {
    // table[chunk][pattern] -> output contribution.
    std::vector<std::vector<State>> table;
};

// One input permutation.  Only the permutation and cost are stored here;
// the expensive forward/inverse LUTs are built lazily by per-thread caches.
struct Candidate {
    // Image of each position: digit i moves to digit p[i].
    std::vector<uint8_t> p;
    std::string text;
    uint64_t cost = 1;        // secondary optimization cost
};

// Describes how a brobdicube key (e.g. "x0x1x2") maps to the compact
// position space that beam25 works in.  Face order is ULFRBD; each face
// occupies 4 consecutive slots in the full 24-slot CENTERS orbit.
struct KeyInfo {
    int n = 0;                         // number of active (internal) positions
    int base = 0;                      // number of distinct color groups
    std::string key;                   // original key string, e.g. "x0x1x2"
    std::string initial;               // compact multiperm, e.g. "000011112222"
    std::vector<int> full_to_internal; // [0..23] → internal index or -1
    std::vector<int> internal_to_full; // [0..n-1] → full index
};

static KeyInfo parse_beam_key(const std::string& key) {
    if (key.size() != 6)
        throw std::runtime_error("key must be exactly 6 characters: " + key);
    // Collect distinct non-x chars in first-appearance order.
    std::string colors;
    for (char c : key)
        if (c != 'x' && colors.find(c) == std::string::npos)
            colors += c;
    KeyInfo ki;
    ki.key = key;
    ki.base = (int)colors.size();
    ki.full_to_internal.assign(24, -1);
    for (int f = 0; f < 6; f++) {
        if (key[f] == 'x') continue;
        int color = (int)colors.find(key[f]);
        for (int p = 0; p < 4; p++) {
            int fp = 4 * f + p;
            ki.full_to_internal[fp] = ki.n;
            ki.internal_to_full.push_back(fp);
            ki.initial += char('0' + color);
            ki.n++;
        }
    }
    if (ki.n == 0)
        throw std::runtime_error("key has no active faces: " + key);
    return ki;
}

// Converts a 24-slot full CENTERS permutation to the compact ki.n-slot form.
// Throws if any active position is mapped to an inactive destination.
static Candidate remap_candidate(Candidate full, const KeyInfo& ki) {
    Candidate result;
    result.cost = full.cost;
    result.text = std::move(full.text);
    result.p.resize(ki.n);
    for (int j = 0; j < ki.n; j++) {
        int src = ki.internal_to_full[j];
        int dst = (int)full.p[src];
        int idst = ki.full_to_internal[dst];
        if (idst < 0)
            throw std::runtime_error(
                "algorithm moves inactive position " + std::to_string(dst));
        result.p[j] = static_cast<uint8_t>(idst);
    }
    return result;
}

// Candidates with equal cost are grouped so random sampling can first choose
// a cost class and then choose within that class.  This intentionally gives
// small, low-cost classes more sampling mass than their raw cardinality would
// receive under uniform candidate sampling.
struct CostBucket {
    uint64_t cost = 0;
    std::vector<int> candidates;
};

// Compact beam entry.  parent_idx/child_cand let Phase 2 build the next
// generation's materialized states without replaying the full sequence.
// When M=false (beam22 mode) sum_sq costs zero bytes via [[no_unique_address]].
template<bool M>
struct BeamState {
    std::vector<int> seq;
    uint32_t cover_count = 0;
    [[no_unique_address]] std::conditional_t<M, uint64_t, NoExt> sum_sq{};
    uint64_t cost = 0;
    uint64_t tie = 0;
    uint32_t parent_idx = 0;
    int      child_cand = -1;
};

// Expanded form of a BeamState: bitset coverage only.  The states vector has
// been removed; we recover state values on demand via ranker.unrank().
// When M=false the MaxActMatExt fields (sum_sq, best_count) are absent.
template<bool M>
struct MaterializedState {
    std::vector<uint64_t> covered;
    [[no_unique_address]] std::conditional_t<M, MaxActMatExt, NoExt> ext{};
    uint32_t cover_count = 0;
    uint64_t tie = 0;
};

// Prints the command-line contract.  The options are intentionally simple
// because long runs are normally scripted.
static void usage(const char* prog) {
    std::cerr << "usage: " << prog
        << " -w WIDTH -b BRANCHES -i INIT [options] < input\n"
              << "\n"
              << "Options:\n"
              << "  -w N       beam width\n"
              << "  -b N       random candidate branches per beam state\n"
              << "  -m COST    maximum input cost; stop reading at first\n"
              << "            sorted cost greater than COST\n"
              << "  -i STRING  initial state. Two modes:\n"
              << "            permutation mode: multiperm string, e.g.\n"
              << "              11110000, 001122, 01230123\n"
              << "            key mode: brobdicube 6-char face key, e.g.\n"
              << "              0x1x2x, x0x1x2 (x = inactive face)\n"
              << "            Key mode reads algorithm input (costalgs format).\n"
              << "  -s SEED    rng seed, default time-based\n"
              << "  -d N       maximum stages/depth, default 200\n"
              << "  -x [N]     reserve N final suffix stages and\n"
              << "            search them\n"
              << "            using missing-state inverse tests\n"
              << "            If N is omitted, N=1. Suffix search samples\n"
              << "            20 * -b candidates per suffix node.\n"
              << "  -t N       number of worker threads, default hardware count\n"
              << "  -k N       max activations per state; beam sorts by\n"
              << "            coverage first then sum-of-squares of activation\n"
              << "            counts. Default 0 = disabled.\n"
              << "  -I         only accept candidates that strictly increase\n"
              << "            coverage; scan all candidates each step until\n"
              << "            one is found. Intended for use with -w 1 -k.\n"
              << "  -v         verbose candidate parsing\n"
              << "\n"
              << "Permutation mode input (stdin), 0-based positions:\n"
              << "  image form:   1 2 0 3 4 5 6 7\n"
              << "  cycle form:   (0 2 1)(4 5)\n"
              << "  optional cost prefix: COST permutation, where COST > n,\n"
              << "  e.g. 37 (0 2 1)\n"
              << "\n"
              << "Key mode input (stdin): costalgs format from algutil -c -d 1000 -pc\n"
              << "  COST (24-slot cycles) algorithm_text\n"
              << "  e.g. 2002004000 (0 4)(1 5)(2 6)(3 7) U 2R U' 3L U 2R' U' 3L'\n"
              << "  Cost prefix 2000000000 (two depth-3 moves) is stripped automatically.\n"
              << "\n"
              << "Blank lines and lines beginning with # are ignored.\n";
}

// Removes surrounding whitespace from parser input without changing the
// middle of the line.
static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b-1]))) --b;
    return s.substr(a, b-a);
}

// Extracts signed integers from either cycle notation or image notation.
// This keeps the parser permissive about commas and extra whitespace.
static std::vector<int> parse_ints(const std::string& line) {
    std::vector<int> v;
    std::string cur;
    for (char ch : line) {
        if (std::isdigit(static_cast<unsigned char>(ch))||
            ch == '-') cur.push_back(ch);
        else if (!cur.empty()) { v.push_back(std::stoi(cur)); cur.clear(); }
    }
    if (!cur.empty()) v.push_back(std::stoi(cur));
    return v;
}

// Converts symbols in -i to numeric digits.  Decimal and alphabetic symbols
// are accepted so bases above ten remain easy to specify.
static int symbol_value(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'Z') return 10 + ch - 'A';
    if (ch >= 'a' && ch <= 'z') return 10 + ch - 'a';
    throw std::runtime_error("-i contains a non-symbol character");
}

// Checks that a parsed image is a true permutation of 0..n-1.
static bool valid_perm(const std::vector<uint8_t>& p) {
    int n = static_cast<int>(p.size());
    std::vector<int> seen(n, 0);
    for (int x : p) {
        if (x < 0 || x >= n || seen[x]) return false;
        seen[x] = 1;
    }
    return true;
}


// Returns true when the first non-whitespace character at or after pos is '('.
// Used to distinguish a small cost prefix from image-form permutation values:
// image notation is all space-separated integers with no parentheses, so a
// leading integer followed by '(' must be a cost prefix in cycle notation.
static bool next_nonspace_is_paren(const std::string& s, size_t pos) {
    while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos])))
        ++pos;
    return pos < s.size() && s[pos] == '(';
}

// Reads just the optional leading cost.  Since the input is sorted by cost,
// this lets -m stop before parsing or caching unnecessary candidates.
static bool leading_cost_value(const std::string& raw,
    int n,
    uint64_t& cost_out) {
    std::string line = trim(raw);
    size_t first_pos = 0;
    while (first_pos < line.size()&&
        std::isspace(static_cast<unsigned char>(line[first_pos]))) {
        ++first_pos;
    }
    size_t scan = first_pos;
    bool neg = false;
    if (scan < line.size() && line[scan] == '-') { neg = true; ++scan; }
    if (scan < line.size()&&
        std::isdigit(static_cast<unsigned char>(line[scan]))) {
        size_t start_digits = scan;
        while (scan < line.size()&&
            std::isdigit(static_cast<unsigned char>(line[scan]))) ++scan;
        if (!neg) {
            uint64_t first = std::stoull(line.substr(start_digits,
                scan - start_digits));
            if (first > static_cast<uint64_t>(n) ||
                    next_nonspace_is_paren(line, scan)) {
                cost_out = first;
                return true;
            }
        }
    }
    return false;
}

// Parses one candidate permutation.  A leading integer greater than n is
// treated as the candidate cost; otherwise cost defaults to one.
static Candidate parse_perm_line(const std::string& raw, int n) {
    std::string line = trim(raw);
    Candidate perm;
    perm.p.resize(n);
    std::iota(perm.p.begin(), perm.p.end(), 0);
    perm.text = line;

    // Optional leading cost: if the first integer on the line is > n,
    // treat it as a uint64_t cost and parse the remainder as the permutation.
    // Otherwise the cost defaults to 1 and the full line is parsed as before.
    size_t first_pos = 0;
    while (first_pos < line.size()&&
        std::isspace(static_cast<unsigned char>(line[first_pos]))) {
        ++first_pos;
    }
    size_t scan = first_pos;
    bool neg = false;
    if (scan < line.size() && line[scan] == '-') { neg = true; ++scan; }
    if (scan < line.size()&&
        std::isdigit(static_cast<unsigned char>(line[scan]))) {
        size_t start_digits = scan;
        while (scan < line.size()&&
            std::isdigit(static_cast<unsigned char>(line[scan]))) ++scan;
        if (!neg) {
            uint64_t first = std::stoull(line.substr(start_digits,
                scan - start_digits));
            if (first > static_cast<uint64_t>(n) ||
                    next_nonspace_is_paren(line, scan)) {
                perm.cost = first;
                line = trim(line.substr(scan));
                perm.text = line;
                if (line.empty()) {
                    throw std::runtime_error(
                        "cost prefix without permutation");
                }
            }
        }
    }

    if (line.find('(') != std::string::npos) {
        size_t pos = 0;
        while (true) {
            size_t l = line.find('(', pos);
            if (l == std::string::npos) break;
            size_t r = line.find(')', l + 1);
            if (r == std::string::npos) {
                throw std::runtime_error(
                    "unmatched '(' in cycle line: " + line);
            }
            std::vector<int> cyc = parse_ints(line.substr(l + 1, r - l - 1));
            if (cyc.size() >= 2) {
                for (int x : cyc) {
                    if (x < 0 || x >= n) {
                        throw std::runtime_error(
                            "cycle entry out of range: " + line);
                    }
                }
                for (size_t i = 0; i < cyc.size(); ++i) {
                    perm.p[cyc[i]] = static_cast<uint8_t>(cyc[(i + 1) % cyc.size()]);
                }
            }
            pos = r + 1;
        }
        // Text after the last ')' is the algorithm source (e.g. "F 2D F' 3U").
        perm.text = trim(line.substr(pos));
    } else {
        std::vector<int> v = parse_ints(line);
        if (static_cast<int>(v.size()) != n) {
            throw std::runtime_error(
                "image-form permutation must have n entries: " + line);
        }
        perm.p.assign(v.begin(), v.end());
    }
    if (!valid_perm(perm.p)) {
        throw std::runtime_error("invalid permutation: " + line);
    }
    return perm;
}

// Builds one chunked permutation LUT.  This is deliberately isolated so the
// lazy cache can build forward and inverse tables on demand.
static ChunkLUT build_chunk_lut_for_image(const std::vector<uint8_t>& image,
    const Codec& codec) {
    ChunkLUT lut;
    lut.table.resize(codec.nchunks);
    for (int c = 0; c < codec.nchunks; ++c) {
        int start = c * codec.chunk_size;
        int len = std::min(codec.chunk_size, codec.n - start);
        uint32_t patterns = codec.chunk_mod[c];
        lut.table[c].assign(patterns, 0);
        for (uint32_t pat = 0; pat < patterns; ++pat) {
            uint32_t x = pat;
            State out = 0;
            for (int j = 0; j < len; ++j) {
                int d = static_cast<int>(
                    x % static_cast<uint32_t>(codec.base));
                x /= static_cast<uint32_t>(codec.base);
                int src = start + j;
                if (d)
                    out += static_cast<State>(d) * codec.pow_base[image[src]];
            }
            lut.table[c][pat] = out;
        }
    }
    return lut;
}

// Applies a chunked LUT by decomposing the encoded state into the same digit
// chunks used when building the table.
static inline State apply_lut(State s,
    const ChunkLUT& lut,
    const Codec& codec) {
    State out = 0;
    State x = s;
    for (int c = 0; c < codec.nchunks; ++c) {
        uint32_t mod = codec.chunk_mod[c];
        uint32_t pat = static_cast<uint32_t>(x % static_cast<State>(mod));
        x /= static_cast<State>(mod);
        out += lut.table[c][pat];
    }
    return out;
}

// One direct-mapped LUT cache slot.  A slot owns both forward and inverse
// tables because most code paths need both during suffix searches.
struct CachedLUTEntry {
    int cand_idx = -1;
    ChunkLUT lut;
    ChunkLUT inv_lut;
};

// Estimates cache memory for one candidate.  The value is conservative and
// is used only to split the one GiB cache budget among worker threads.
static size_t estimate_lut_entry_bytes(const Codec& codec) {
    uint64_t patterns = 0;
    for (uint32_t m : codec.chunk_mod) patterns += static_cast<uint64_t>(m);
    uint64_t bytes = 2ULL * patterns * sizeof(State);
    bytes += 2ULL * static_cast<uint64_t>(codec.nchunks) *
             sizeof(std::vector<State>);
    bytes += sizeof(CachedLUTEntry) + 64;
    if (bytes > static_cast<uint64_t>(std::numeric_limits<size_t>::max()))
        return std::numeric_limits<size_t>::max();
    return static_cast<size_t>(bytes);
}

// Per-thread direct-mapped LUT cache.  Thread-local ownership avoids locks
// and data races; duplicate LUT builds are cheaper than shared contention.
struct ThreadLocalLUTCache {
    const std::vector<Candidate>* perms = nullptr;
    const Codec* codec = nullptr;
    std::vector<CachedLUTEntry> slots;

    ThreadLocalLUTCache() = default;

    ThreadLocalLUTCache(const std::vector<Candidate>& perms_,
        const Codec& codec_,
        size_t byte_budget) {
        init(perms_, codec_, byte_budget);
    }

    void init(const std::vector<Candidate>& perms_,
        const Codec& codec_,
        size_t byte_budget) {
        perms = &perms_;
        codec = &codec_;
        size_t entry_bytes = std::max<size_t>(1,
            estimate_lut_entry_bytes(codec_));
        size_t nslots = byte_budget / entry_bytes;
        if (nslots == 0) nslots = 1;
        nslots = std::min(nslots, perms_.size());
        if (nslots == 0) nslots = 1;
        slots.clear();
        slots.resize(nslots);
    }

    const CachedLUTEntry& get(int cand_idx) {
        if (!perms || !codec || slots.empty()) {
            throw std::runtime_error("uninitialized LUT cache");
        }
        size_t slot_idx = static_cast<size_t>(cand_idx) % slots.size();
        CachedLUTEntry& e = slots[slot_idx];
        if (e.cand_idx != cand_idx) {
            const Candidate& c = (*perms)[static_cast<size_t>(cand_idx)];
            ChunkLUT lut = build_chunk_lut_for_image(c.p, *codec);
            std::vector<uint8_t> inv(c.p.size());
            for (int i = 0; i < static_cast<int>(c.p.size()); ++i) {
                inv[c.p[i]] = static_cast<uint8_t>(i);
            }
            ChunkLUT inv_lut = build_chunk_lut_for_image(inv, *codec);
            e.lut = std::move(lut);
            e.inv_lut = std::move(inv_lut);
            e.cand_idx = cand_idx;
        }
        return e;
    }
};

// Fast line reader for huge candidate files.  It uses fgets and delays LUT
// construction until the candidate is actually used by the search.
// When ki != nullptr the input lines use full 24-slot CENTERS cycle notation
// (from algutil) and are remapped to ki.n compact slots.  cost_offset is
// subtracted from every candidate cost (used to strip the fixed depth-3
// component: 2 * d^3 at d=1000 → 2,000,000,000).
static std::vector<Candidate> read_perms(FILE* in,
    int n,
    bool verbose,
    uint64_t max_perm_cost,
    const KeyInfo* ki = nullptr,
    uint64_t cost_offset = 0) {
    const int parse_n = ki ? 24 : n;  // cycle indices range over full or compact space
    std::vector<Candidate> perms;
    std::vector<char> buf(1 << 20);
    int lineno = 0;
    while (std::fgets(buf.data(), static_cast<int>(buf.size()), in)) {
        ++lineno;
        std::string line(buf.data());
        // If the line was longer than the buffer, read and append the remainder.
        while (!line.empty() && line.back() != '\n' && !std::feof(in)) {
            if (!std::fgets(buf.data(), static_cast<int>(buf.size()), in))
                break;
            line += buf.data();
        }
        std::string t = trim(line);
        if (t.empty() || t[0] == '#')
            continue;
        try {
            uint64_t line_cost = 0;
            if (leading_cost_value(t, parse_n, line_cost)) {
                uint64_t adj_cost = (line_cost > cost_offset)
                    ? line_cost - cost_offset : line_cost;
                if (adj_cost > max_perm_cost) {
                    if (verbose)
                        std::cerr << "stopping at line " << lineno
                            << ": cost " << adj_cost
                            << " exceeds maximum " << max_perm_cost << "\n";
                    break;
                }
            }
            Candidate c = parse_perm_line(t, parse_n);
            if (cost_offset && c.cost >= cost_offset)
                c.cost -= cost_offset;
            if (ki) c = remap_candidate(std::move(c), *ki);
            perms.push_back(std::move(c));
        } catch (const std::exception& e) {
            std::ostringstream oss;
            oss << "line " << lineno << ": " << e.what();
            throw std::runtime_error(oss.str());
        }
    }
    if (verbose) std::cerr << "read " << perms.size()
        << " candidate permutations\n";
    return perms;
}

// Maps legal multipermutation states to dense ranks.  The split high/low
// tables avoid an unordered_map in the inner loops and also provide unrank.
struct Ranker {
    int n = 0;
    int base = 2;
    uint32_t total = 0;
    std::vector<int> counts;
    const Codec* codec = nullptr;

    // Split the encoded state into low_len low-order digits and high_len
    // high-order digits.
    // A state s is decomposed as:
    //     low_code  = s % split_mod
    //     high_code = s / split_mod
    // The rank is then:
    // high_base[high_code] +
    // low_rank_tables[high_low_table[high_code]][low_code]
    int low_len = 0;
    int high_len = 0;
    State split_mod = 1;
    uint64_t high_mod = 1;
    uint64_t low_mod = 1;

    std::vector<State> state_of_rank;
    std::vector<uint32_t> high_base;       // BAD_RANK for invalid high codes
    // Index into low_rank_tables, or BAD_RANK.
    std::vector<uint32_t> high_low_table;
    std::vector<std::vector<uint32_t>> low_rank_tables;
    std::vector<uint32_t> low_table_counts;

    // Direct count-key table used only during initialization.  Counts are
    // encoded
    // in radix key_radix; key_to_low_table[key] gives the low table index.
    uint64_t key_radix = 0;
    std::vector<uint32_t> key_to_low_table;

    static uint64_t checked_pow_u64(uint64_t a, int e, const char* what) {
        uint64_t r = 1;
        for (int i = 0; i < e; ++i) {
            if (a != 0 && r > std::numeric_limits<uint64_t>::max() / a) {
                throw std::runtime_error(
                    std::string(what) + " would overflow uint64_t");
            }
            r *= a;
        }
        return r;
    }

    uint64_t count_key(const std::vector<int>& c) const {
        uint64_t key = 0;
        uint64_t mul = 1;
        for (int v = 0; v < base; ++v) {
            key += static_cast<uint64_t>(c[v]) * mul;
            mul *= key_radix;
        }
        return key;
    }

    std::vector<int> digit_counts(uint64_t code, int len) const {
        std::vector<int> c(static_cast<size_t>(base), 0);
        for (int i = 0; i < len; ++i) {
            int d = static_cast<int>(code % static_cast<uint64_t>(base));
            code /= static_cast<uint64_t>(base);
            ++c[d];
        }
        return c;
    }

    bool counts_fit_original(const std::vector<int>& c) const {
        for (int v = 0; v < base; ++v) {
            if (c[v] < 0 || c[v] > counts[v]) return false;
        }
        return true;
    }

    bool counts_equal_code(uint64_t code,
        int len,
        const std::vector<int>& target) const {
        std::vector<int> c(static_cast<size_t>(base), 0);
        for (int i = 0; i < len; ++i) {
            int d = static_cast<int>(code % static_cast<uint64_t>(base));
            code /= static_cast<uint64_t>(base);
            if (++c[d] > target[d]) return false;
        }
        for (int v = 0; v < base; ++v) if (c[v] != target[v]) return false;
        return true;
    }

    void build_low_table_for_remaining(const std::vector<int>& rem) {
        uint64_t key = count_key(rem);
        if (key >= key_to_low_table.size()) {
            throw std::runtime_error("internal count-key overflow");
        }
        if (key_to_low_table[static_cast<size_t>(key)] != BAD_RANK) return;

        std::vector<uint32_t> table(static_cast<size_t>(low_mod), BAD_RANK);
        uint32_t r = 0;
        for (uint64_t lo = 0; lo < low_mod; ++lo) {
            if (counts_equal_code(lo, low_len, rem)) {
                table[static_cast<size_t>(lo)] = r++;
            }
        }
        uint32_t idx = static_cast<uint32_t>(low_rank_tables.size());
        low_rank_tables.push_back(std::move(table));
        low_table_counts.push_back(r);
        key_to_low_table[static_cast<size_t>(key)] = idx;
    }

    void rec_remaining_counts(int sym,
        int slots_left,
        std::vector<int>& rem) {
        if (sym == base) {
            if (slots_left == 0) build_low_table_for_remaining(rem);
            return;
        }
        int max_here = std::min(counts[sym], slots_left);
        for (int x = 0; x <= max_here; ++x) {
            rem[sym] = x;
            rec_remaining_counts(sym + 1, slots_left - x, rem);
        }
        rem[sym] = 0;
    }

    void init(int n_,
        int base_,
        const std::vector<int>& counts_,
        const Codec& codec_) {
        n = n_;
        base = base_;
        counts = counts_;
        codec = &codec_;
        if (n < 1 || n > 24) throw std::runtime_error("n must be 1..24");
        if (static_cast<int>(counts.size()) != base) {
            throw std::runtime_error("bad counts size");
        }

        low_len = n / 2;
        high_len = n - low_len;
        split_mod = codec->pow_base[low_len];
        low_mod = split_mod;
        high_mod = checked_pow_u64(static_cast<uint64_t>(base),
            high_len,
            "high rank table");

        uint64_t max_size = static_cast<uint64_t>(
            std::numeric_limits<size_t>::max());
        if (low_mod > max_size || high_mod > max_size) {
            throw std::runtime_error(
                "rank direct table too large for this platform");
        }

        state_of_rank.clear();
        high_base.assign(static_cast<size_t>(high_mod), BAD_RANK);
        high_low_table.assign(static_cast<size_t>(high_mod), BAD_RANK);
        low_rank_tables.clear();
        low_table_counts.clear();

        // Direct table from remaining-count vector to low-table index.
        // rem[v] is between 0 and low_len, so radix low_len+1 is sufficient.
        key_radix = static_cast<uint64_t>(low_len + 1);
        uint64_t key_space = checked_pow_u64(key_radix,
            base,
            "count-key table");
        if (key_space > max_size) {
            throw std::runtime_error(
                "count-key table too large for this platform");
        }
        key_to_low_table.assign(static_cast<size_t>(key_space), BAD_RANK);

        std::vector<int> rem(static_cast<size_t>(base), 0);
        rec_remaining_counts(0, low_len, rem);

        uint64_t rough = 1;
        int remaining_positions = n;
        for (int c : counts) {
            for (int i = 1; i <= c; ++i) {
                rough =
                    (rough * static_cast<uint64_t>(remaining_positions--)) /
                    static_cast<uint64_t>(i);
            }
        }
        if (rough > 0 && rough < 100000000ULL) {
            state_of_rank.reserve(static_cast<size_t>(rough));
        }

        uint32_t base_rank = 0;
        for (uint64_t hi = 0; hi < high_mod; ++hi) {
            std::vector<int> hc = digit_counts(hi, high_len);
            if (!counts_fit_original(hc)) continue;

            std::vector<int> need(static_cast<size_t>(base), 0);
            int need_sum = 0;
            bool ok = true;
            for (int v = 0; v < base; ++v) {
                need[v] = counts[v] - hc[v];
                if (need[v] < 0) { ok = false; break; }
                need_sum += need[v];
            }
            if (!ok || need_sum != low_len) continue;

            uint64_t key = count_key(need);
            uint32_t table_idx = key_to_low_table[static_cast<size_t>(key)];
            if (table_idx == BAD_RANK) {
                throw std::runtime_error("internal missing low rank table");
            }

            high_base[static_cast<size_t>(hi)] = base_rank;
            high_low_table[static_cast<size_t>(hi)] = table_idx;
            const std::vector<uint32_t>& lowtab = low_rank_tables[table_idx];
            uint32_t cnt = low_table_counts[table_idx];
            for (uint64_t lo = 0; lo < low_mod; ++lo) {
                uint32_t lr = lowtab[static_cast<size_t>(lo)];
                if (lr != BAD_RANK) {
                    if (base_rank + lr >= state_of_rank.size()) {
                        state_of_rank.resize(
                            static_cast<size_t>(base_rank + lr + 1));
                    }
                    state_of_rank[static_cast<size_t>(base_rank + lr)] =
                        static_cast<State>(lo + hi * split_mod);
                }
            }
            base_rank += cnt;
        }

        total = base_rank;
        if (total == 0) throw std::runtime_error("empty rank universe");
        if (state_of_rank.size() != total) {
            throw std::runtime_error("internal rank universe size mismatch");
        }
    }

    inline uint32_t rank(State s) const {
        uint64_t lo = static_cast<uint64_t>(s % split_mod);
        uint64_t hi = static_cast<uint64_t>(s / split_mod);
        if (hi >= high_base.size() || lo >= low_mod) return BAD_RANK;
        uint32_t hb = high_base[static_cast<size_t>(hi)];
        if (hb == BAD_RANK) return BAD_RANK;
        uint32_t ti = high_low_table[static_cast<size_t>(hi)];
        if (ti == BAD_RANK) return BAD_RANK;
        uint32_t lr = low_rank_tables[ti][static_cast<size_t>(lo)];
        if (lr == BAD_RANK) return BAD_RANK;
        return hb + lr;
    }

    inline State unrank(uint32_t r) const {
        return state_of_rank[r];
    }
};


// Bijective 64-bit hash for incremental, commutative tie computation.
// Covered states are folded in via XOR so order does not matter.
static inline uint64_t hash64(uint64_t x) {
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

// Scans the covered bitset once and returns the rank of every covered state.
static std::vector<uint32_t> extract_covered_ranks(
    const std::vector<uint64_t>& covered, uint32_t total) {
    std::vector<uint32_t> ranks;
    ranks.reserve(total / 2);
    uint32_t nwords = (total + 63) / 64;
    for (uint32_t w = 0; w < nwords; ++w) {
        uint64_t bits = covered[w];
        while (bits) {
            uint32_t bit = static_cast<uint32_t>(__builtin_ctzll(bits));
            ranks.push_back(w * 64u + bit);
            bits &= bits - 1;
        }
    }
    return ranks;
}

// Scans the covered bitset once and returns the rank of every UNcovered state.
static std::vector<uint32_t> extract_uncovered_ranks(
    const std::vector<uint64_t>& covered, uint32_t total) {
    std::vector<uint32_t> ranks;
    ranks.reserve(total / 2);
    uint32_t nwords = (total + 63) / 64;
    for (uint32_t w = 0; w < nwords; ++w) {
        uint64_t bits = ~covered[w];
        if (w == nwords - 1 && (total & 63u) != 0)
            bits &= (1ULL << (total & 63u)) - 1;
        while (bits) {
            uint32_t bit = static_cast<uint32_t>(__builtin_ctzll(bits));
            ranks.push_back(w * 64u + bit);
            bits &= bits - 1;
        }
    }
    return ranks;
}

// Prefetch distance for state_of_rank[] lookups (L3 latency / loop-body cost).
static constexpr size_t PF_DIST = 16;

// Result of scoring one candidate against a beam state.
struct ScoreResult { uint32_t cover_count; uint64_t sum_sq; uint64_t tie; };

// Scores one candidate against a materialized state without copying it.
// When M=false: beam22 behaviour (auto inverse mode above 50% coverage).
// When M=true: forward only, tracks improvements, computes updated sum-of-squares.
template<bool M>
static ScoreResult score_candidate(
    const MaterializedState<M>& mat,
    const std::vector<uint32_t>& covered_ranks,
    const std::vector<uint32_t>& uncovered_ranks,
    int cand_idx,
    const Ranker& ranker, const Codec& codec,
    ThreadLocalLUTCache& lut_cache) {
    const CachedLUTEntry& ce = lut_cache.get(cand_idx);
    uint32_t new_count = 0;
    uint64_t dtie = 0;
    if constexpr (M) {
        // covered_ranks: pre-filtered by caller to val ∈ [1, max_act_limit),
        // i.e. valid non-initial sources.  The initial state (val=0, count=0)
        // is handled first by a separate single lookup.
        // uncovered_ranks non-empty → caller chose inverse (uncov < unsat).
        int64_t delta_sq = 0;

        // --- Initial state (count 0, always a valid source) ---
        {
            uint32_t r0 = mat.ext.init_rank;
            State s2 = apply_lut(ranker.unrank(r0), ce.lut, codec);
            uint32_t r2 = ranker.rank(s2);
            if (r2 != BAD_RANK) {
                // new_c = 1
                if (!test_bit(mat.covered, r2)) {
                    ++new_count;
                    dtie ^= hash64(s2);
                    delta_sq += 1;  // 1^2
                } else {
                    uint8_t val2 = mat.ext.best_count.get(r2);
                    // old_c: val=0+not-init→sat-at-4; val=0+init→0 (1>0, no impr).
                    uint8_t old_c = (val2 == 0u && r2 != mat.ext.init_rank)
                        ? uint8_t(max_act_limit) : val2;
                    if (1u < old_c)   // new_c=1 improves
                        delta_sq += 1 - (int64_t)old_c * old_c;
                }
            }
        }

        if (!uncovered_ranks.empty()) {
            // Inverse mode (high coverage): iterate uncovered states and test
            // whether their preimage under inv_lut is in the covered set.
            // Improvements to already-covered states are not tracked here.
            const size_t n = uncovered_ranks.size();
            for (size_t i = 0; i < n; ++i) {
                if (i + PF_DIST < n)
                    __builtin_prefetch(
                        &ranker.state_of_rank[uncovered_ranks[i + PF_DIST]], 0, 0);
                uint32_t r2 = uncovered_ranks[i];
                State s = ranker.unrank(r2);
                State pre = apply_lut(s, ce.inv_lut, codec);
                uint32_t r0 = ranker.rank(pre);
                if (r0 == BAD_RANK) continue;
                if (r0 == mat.ext.init_rank) continue;  // handled by init block
                if (!test_bit(mat.covered, r0)) continue;
                uint8_t c0 = mat.ext.best_count.get(r0);
                if (c0 == 0u || c0 >= (uint8_t)max_act_limit) continue;
                uint8_t new_c = c0 + 1;
                ++new_count;
                dtie ^= hash64(s);
                delta_sq += (int64_t)new_c * new_c;
            }
        } else {
            // Forward mode: covered_ranks filtered to val ∈ [1, k).
            const size_t n = covered_ranks.size();
            for (size_t i = 0; i < n; ++i) {
                if (i + PF_DIST < n)
                    __builtin_prefetch(
                        &ranker.state_of_rank[covered_ranks[i + PF_DIST]], 0, 0);
                uint32_t r0 = covered_ranks[i];
                uint8_t c0 = mat.ext.best_count.get(r0);
                if (c0 == 0u || c0 >= (uint8_t)max_act_limit) continue;
                State s2 = apply_lut(ranker.unrank(r0), ce.lut, codec);
                uint32_t r2 = ranker.rank(s2);
                if (r2 == BAD_RANK) continue;
                uint8_t new_c = c0 + 1;
                if (!test_bit(mat.covered, r2)) {
                    ++new_count;
                    dtie ^= hash64(s2);
                    delta_sq += (int64_t)new_c * new_c;
                } else {
                    uint8_t val2 = mat.ext.best_count.get(r2);
                    uint8_t old_c = (val2 == 0u && r2 != mat.ext.init_rank)
                        ? uint8_t(max_act_limit) : val2;
                    if (new_c < old_c)
                        delta_sq += (int64_t)new_c * new_c - (int64_t)old_c * old_c;
                }
            }
        }
        return {mat.cover_count + new_count,
                (uint64_t)((int64_t)mat.ext.sum_sq + delta_sq),
                mat.tie ^ dtie};
    } else {
        if (mat.cover_count * 2 > ranker.total) {
            const size_t n = uncovered_ranks.size();
            for (size_t i = 0; i < n; ++i) {
                if (i + PF_DIST < n)
                    __builtin_prefetch(
                        &ranker.state_of_rank[uncovered_ranks[i + PF_DIST]], 0, 0);
                uint32_t r2 = uncovered_ranks[i];
                State s = ranker.unrank(r2);
                State pre = apply_lut(s, ce.inv_lut, codec);
                uint32_t r0 = ranker.rank(pre);
                if (r0 != BAD_RANK && test_bit(mat.covered, r0)) {
                    ++new_count;
                    dtie ^= hash64(s);
                }
            }
        } else {
            const size_t n = covered_ranks.size();
            for (size_t i = 0; i < n; ++i) {
                if (i + PF_DIST < n)
                    __builtin_prefetch(
                        &ranker.state_of_rank[covered_ranks[i + PF_DIST]], 0, 0);
                uint32_t r0 = covered_ranks[i];
                State s2 = apply_lut(ranker.unrank(r0), ce.lut, codec);
                uint32_t r2 = ranker.rank(s2);
                if (r2 != BAD_RANK && !test_bit(mat.covered, r2)) {
                    ++new_count;
                    dtie ^= hash64(s2);
                }
            }
        }
        return {mat.cover_count + new_count, 0, mat.tie ^ dtie};
    }
}

// Creates the materialized state for the empty sequence.
template<bool M>
static MaterializedState<M> make_start_materialized(State init_state,
    const Ranker& ranker) {
    MaterializedState<M> m;
    m.covered.assign((ranker.total + 63) >> 6, 0);
    m.cover_count = 1;
    uint32_t r = ranker.rank(init_state);
    if (r == BAD_RANK) {
        throw std::runtime_error("initial state not in rank universe");
    }
    set_bit(m.covered, r);
    m.tie = hash64(init_state);
    if constexpr (M) {
        m.ext.sum_sq = 0;
        m.ext.init_rank = r;
        m.ext.best_count.assign_zero(ranker.total);
        // Initial state has count 0; val=0 is the default (assign_zero).
        // It is handled separately before inner loops via init_rank.
    }
    return m;
}

// Extends a materialized covered set by one candidate.  Caller supplies both
// parent_ranks and parent_uncovered_ranks (one extracted from the parent
// bitset before this call); the function picks the shorter list automatically.
template<bool M>
static void apply_candidate_in_place(MaterializedState<M>& out,
                                     int cand_idx,
                                     const std::vector<uint32_t>& parent_ranks,
                                     const std::vector<uint32_t>& parent_uncovered_ranks,
                                     const std::vector<Candidate>& perms,
                                     const Ranker& ranker, const Codec& codec,
                                     ThreadLocalLUTCache& lut_cache) {
    (void)perms;
    const CachedLUTEntry& ce = lut_cache.get(cand_idx);
    if constexpr (M) {
        // max_act mode: forward only, update best_count and sum_sq exactly.
        // Snapshot source (r0, c0) pairs BEFORE any updates so that a chain
        // r0→r2→r3 within one candidate step isn't counted twice.
        // parent_ranks was pre-filtered to val ∈ [1, max_act_limit); the initial
        // state (val=0) is NOT in parent_ranks and is always appended first.
        struct Src { uint32_t r0; uint8_t c0; };
        std::vector<Src> srcs;
        srcs.reserve(parent_ranks.size() + 1);

        // Initial state: count 0 is always a valid source.
        srcs.push_back({out.ext.init_rank, 0u});

        for (size_t i = 0; i < parent_ranks.size(); ++i) {
            uint32_t r0 = parent_ranks[i];
            uint8_t c0 = out.ext.best_count.get(r0);
            if (c0 == 0u) continue;                      // init or sat-at-4
            if (c0 >= (uint8_t)max_act_limit) continue;  // sat for k<4
            srcs.push_back({r0, c0});
        }

        const size_t m = srcs.size();
        for (size_t i = 0; i < m; ++i) {
            if (i + PF_DIST < m)
                __builtin_prefetch(
                    &ranker.state_of_rank[srcs[i + PF_DIST].r0], 0, 0);
            uint32_t r0 = srcs[i].r0;
            uint8_t c0 = srcs[i].c0;
            State s2 = apply_lut(ranker.unrank(r0), ce.lut, codec);
            uint32_t r2 = ranker.rank(s2);
            if (r2 == BAD_RANK)
                throw std::runtime_error(
                    "internal rank error after applying permutation");
            uint8_t new_c = c0 + 1;
            // new_c ≤ 4 (c0 ≤ 3); store as val=new_c, or 0 when new_c==4 (sat).
            uint8_t store_val = (new_c <= 3u) ? new_c : 0u;
            if (!test_bit(out.covered, r2)) {
                set_bit(out.covered, r2);
                ++out.cover_count;
                out.tie ^= hash64(s2);
                out.ext.best_count.set(r2, store_val);
                out.ext.sum_sq += (uint64_t)new_c * new_c;
            } else {
                // Possible improvement: determine old_c from 2-bit val.
                // val=0 + covered: init state (count=0, can't improve from c0+1≥1)
                //                  OR saturated at k=4 (count=4, formula gives 4).
                uint8_t val2 = out.ext.best_count.get(r2);
                uint8_t old_c = (val2 == 0u && r2 != out.ext.init_rank)
                    ? uint8_t(max_act_limit) : val2;
                if (new_c < old_c) {
                    out.ext.sum_sq = out.ext.sum_sq
                        - (uint64_t)old_c * old_c + (uint64_t)new_c * new_c;
                    out.ext.best_count.set(r2, store_val);
                }
            }
        }
        return;
    }
    if (out.cover_count * 2 > ranker.total) {
        // Inverse mode: for each uncovered state, check if its preimage
        // (under the candidate's inverse LUT) lies in the covered set.
        // Two-pass: collect newly-covered ranks without modifying out.covered,
        // then apply them.  This prevents a newly-covered state's rank from
        // being mistakenly used as a valid preimage within the same call.
        std::vector<uint32_t> newly_covered;
        const size_t n = parent_uncovered_ranks.size();
        for (size_t i = 0; i < n; ++i) {
            if (i + PF_DIST < n)
                __builtin_prefetch(
                    &ranker.state_of_rank[parent_uncovered_ranks[i + PF_DIST]],
                    0, 0);
            uint32_t r2 = parent_uncovered_ranks[i];
            State s = ranker.unrank(r2);
            State pre = apply_lut(s, ce.inv_lut, codec);
            uint32_t r0 = ranker.rank(pre);
            if (r0 != BAD_RANK && test_bit(out.covered, r0))
                newly_covered.push_back(r2);
        }
        for (uint32_t r2 : newly_covered) {
            if (set_bit(out.covered, r2)) {
                ++out.cover_count;
                out.tie ^= hash64(ranker.unrank(r2));
            }
        }
    } else {
        const size_t n = parent_ranks.size();
        for (size_t i = 0; i < n; ++i) {
            if (i + PF_DIST < n)
                __builtin_prefetch(
                    &ranker.state_of_rank[parent_ranks[i + PF_DIST]], 0, 0);
            uint32_t r0 = parent_ranks[i];
            State s2 = apply_lut(ranker.unrank(r0), ce.lut, codec);
            uint32_t r2 = ranker.rank(s2);
            if (r2 == BAD_RANK) {
                throw std::runtime_error(
                    "internal rank error after applying permutation");
            }
            if (set_bit(out.covered, r2)) {
                ++out.cover_count;
                out.tie ^= hash64(s2);
            }
        }
    }
}

// Reconstructs a MaterializedState from scratch by replaying the sequence.
// Used only in the completion step; the main beam loop uses stored beam_mats.
template<bool M>
static MaterializedState<M> materialize(const std::vector<int>& seq,
    State init_state,
                                     const std::vector<Candidate>& perms,
                                     const Ranker& ranker, const Codec& codec,
                                     ThreadLocalLUTCache& lut_cache) {
    MaterializedState<M> out = make_start_materialized<M>(init_state, ranker);
    for (int ci : seq) {
        std::vector<uint32_t> ranks, uncovered_ranks;
        if (out.cover_count * 2 > ranker.total)
            uncovered_ranks = extract_uncovered_ranks(out.covered, ranker.total);
        else
            ranks = extract_covered_ranks(out.covered, ranker.total);
        apply_candidate_in_place<M>(out, ci, ranks, uncovered_ranks,
                                 perms, ranker, codec, lut_cache);
    }
    return out;
}

// Converts the complement of a coverage bitset back to explicit states.
// Uses bitwise scanning so cost is O(n_uncovered) rather than O(n_total).
template<bool M>
static std::vector<State> make_missing_states(const MaterializedState<M>& st,
    const Ranker& ranker) {
    std::vector<State> missing;
    missing.reserve(ranker.total - st.cover_count);
    uint32_t nwords = (ranker.total + 63) / 64;
    for (uint32_t w = 0; w < nwords; ++w) {
        uint64_t bits = ~st.covered[w];
        if (w == nwords - 1 && (ranker.total & 63u) != 0)
            bits &= (1ULL << (ranker.total & 63u)) - 1;
        while (bits) {
            uint32_t bit = static_cast<uint32_t>(__builtin_ctzll(bits));
            missing.push_back(ranker.unrank(w * 64u + bit));
            bits &= bits - 1;
        }
    }
    return missing;
}

// One-step final completion test.  It applies the inverse candidate to each
// missing state and checks membership in the prefix coverage set.
template<bool M>
static bool candidate_completes(
    const MaterializedState<M>& st,
    std::vector<State>& missing,
    int cand_idx,
    const std::vector<Candidate>& perms,
    const Ranker& ranker,
    const Codec& codec,
    ThreadLocalLUTCache& lut_cache) {
    (void)perms;
    const CachedLUTEntry& ce = lut_cache.get(cand_idx);
    for (size_t i = 0; i < missing.size(); ++i) {
        State pre = apply_lut(missing[i], ce.inv_lut, codec);
        uint32_t r = ranker.rank(pre);
        if (r == BAD_RANK || !test_bit(st.covered, r)) {
            size_t j = i >> 1;
            if (j != i) std::swap(missing[i], missing[j]);
            return false;
        }
    }
    return true;
}

// Pretty-prints an image-form permutation as disjoint cycles.
static std::string perm_to_cycles(const Candidate& perm) {
    int n = static_cast<int>(perm.p.size());
    std::vector<int> seen(n, 0);
    std::ostringstream oss;
    bool any = false;
    for (int i = 0; i < n; ++i) {
        if (seen[i] || perm.p[i] == i) continue;
        any = true;
        oss << '(';
        int x = i;
        bool first = true;
        while (!seen[x]) {
            seen[x] = 1;
            if (!first) oss << ' ';
            first = false;
            oss << x;
            x = perm.p[x];
        }
        oss << ')';
    }
    if (!any) oss << "()";
    return oss.str();
}

// Builds cost buckets in input order.  The input is expected to be sorted by
// cost, so equal-cost runs become one bucket with no extra sorting.
static std::vector<CostBucket> build_cost_buckets(
    const std::vector<Candidate>& perms) {
    std::vector<CostBucket> buckets;
    for (size_t i = 0; i < perms.size(); ++i) {
        uint64_t c = perms[i].cost;
        if (buckets.empty() || buckets.back().cost != c) {
            buckets.push_back(CostBucket{});
            buckets.back().cost = c;
        }
        buckets.back().candidates.push_back(static_cast<int>(i));
    }
    return buckets;
}

// Samples distinct candidate indices by first choosing a cost bucket uniformly
// and then choosing a candidate uniformly from that bucket.  This makes rare
// low-cost buckets appear much more often than uniform-over-all-candidates.
static std::vector<int> choose_branch_candidates(
    int requested,
    int pool_size,
    const std::vector<CostBucket>& cost_buckets,
    std::mt19937_64& rng) {
    if (requested <= 0 || pool_size <= 0) return {};
    if (requested >= pool_size) {
        std::vector<int> all(static_cast<size_t>(pool_size));
        std::iota(all.begin(), all.end(), 0);
        return all;
    }

    std::vector<uint8_t> seen(static_cast<size_t>(pool_size), 0);
    std::vector<int> out;
    out.reserve(static_cast<size_t>(requested));

    int attempts = 0;
    const int max_attempts = requested * 20;
    if (!cost_buckets.empty()) {
        std::uniform_int_distribution<size_t> pick_bucket(
            0, cost_buckets.size() - 1);
        while (static_cast<int>(out.size()) < requested &&
            attempts < max_attempts) {
            ++attempts;
            const CostBucket& bucket = cost_buckets[pick_bucket(rng)];
            if (bucket.candidates.empty()) continue;
            std::uniform_int_distribution<size_t> pick_in_bucket(
                0, bucket.candidates.size() - 1);
            int ci = bucket.candidates[pick_in_bucket(rng)];
            if (seen[static_cast<size_t>(ci)]) continue;
            seen[static_cast<size_t>(ci)] = 1;
            out.push_back(ci);
        }
    }

    // If bucketed sampling hit too many duplicates, finish deterministically.
    for (int ci = 0; static_cast<int>(out.size()) < requested &&
        ci < pool_size; ++ci) {
        if (seen[static_cast<size_t>(ci)]) continue;
        seen[static_cast<size_t>(ci)] = 1;
        out.push_back(ci);
    }
    return out;
}


// Derives independent deterministic RNG seeds for each worker/depth.
static uint64_t mix_seed(uint64_t seed, int tid);

// Suffix-beam node for -x N.  Instead of materializing reachable states, it
// tracks which target states still cannot be reached through the suffix.
struct SuffixState {
    std::vector<int> seq;
    std::vector<State> missing;
    std::vector<uint64_t> missing_bits;
    uint64_t cost = 0;
    uint64_t tie = 0;
};

// Builds a membership bitset for a suffix state's current missing set.
static std::vector<uint64_t> make_state_bitset(
    const std::vector<State>& states,
    const Ranker& ranker) {
    std::vector<uint64_t> bits((ranker.total + 63) >> 6, 0);
    for (State s : states) {
        uint32_t r = ranker.rank(s);
        if (r == BAD_RANK) {
            throw std::runtime_error(
                "internal rank error while making suffix bitset");
        }
        set_bit(bits, r);
    }
    return bits;
}

// Hashes a missing-state list for suffix beam tie-breaking.  Large lists are
// sampled deterministically to keep trimming cheap.
static uint64_t hash_state_vector_limited(const std::vector<State>& states) {
    uint64_t h = 1469598103934665603ULL;
    // Hash all when small, otherwise sample deterministically to keep suffix
    // trimming cheap.
    size_t step = 1;
    if (states.size() > 4096) step = states.size() / 4096 + 1;
    for (size_t i = 0; i < states.size(); i += step) {
        State m = states[i];
        h ^= static_cast<uint64_t>(m) + 0x9e3779b97f4a7c15ULL + (h
            << 6) + (h >> 2);
        h *= 1099511628211ULL;
    }
    h ^= static_cast<uint64_t>(states.size()) * 0x9e3779b97f4a7c15ULL;
    return h;
}

// Orders suffix states by fewer remaining misses, then lower suffix cost.
static bool suffix_better(const SuffixState& a, const SuffixState& b) {
    if (a.missing.size() != b.missing.size())
        return a.missing.size() < b.missing.size();
    if (a.cost != b.cost) return a.cost < b.cost;
    if (a.seq.size() != b.seq.size()) return a.seq.size() < b.seq.size();
    return a.tie < b.tie;
}

static uint64_t suffix_dedup_key(const SuffixState& st) {
    return st.tie ^ (uint64_t(st.missing.size())
        << 32) ^ (st.cost * 0x9e3779b97f4a7c15ULL) ^ uint64_t(st.seq.size());
}

// Sorts, deduplicates, and caps a suffix beam.
static void sort_and_trim_suffix(std::vector<SuffixState>& v, size_t width) {
    if (v.empty()) return;
    std::sort(v.begin(), v.end(), suffix_better);
    std::vector<SuffixState> out;
    out.reserve(std::min(width, v.size()));
    std::unordered_set<uint64_t> seen;
    seen.reserve(width * 2 + 1);
    for (auto& st : v) {
        uint64_t key = suffix_dedup_key(st);
        if (seen.insert(key).second) {
            out.push_back(std::move(st));
            if (out.size() >= width) break;
        }
    }
    v.swap(out);
}

// Adds one suffix candidate by inverse-testing the current missing set.
static SuffixState extend_suffix_state(
    const SuffixState& st,
    int cand_idx,
    const std::vector<Candidate>& perms,
    const Ranker& ranker,
    const Codec& codec,
    ThreadLocalLUTCache& lut_cache) {
    const Candidate& c = perms[cand_idx];
    const CachedLUTEntry& ce = lut_cache.get(cand_idx);
    SuffixState out;
    out.seq = st.seq;
    out.seq.push_back(cand_idx);
    out.cost = st.cost + c.cost;
    out.missing.reserve(st.missing.size());

    for (State u : st.missing) {
        State pre = apply_lut(u, ce.inv_lut, codec);
        uint32_t r = ranker.rank(pre);
        bool pre_still_missing = (r != BAD_RANK && test_bit(st.missing_bits,
            r));
        if (pre_still_missing) out.missing.push_back(u);
    }
    out.missing_bits = make_state_bitset(out.missing, ranker);
    out.tie = hash_state_vector_limited(out.missing);
    return out;
}

// Searches the reserved final suffix stages for one prefix.  It scans the
// sampled suffix space and returns the cheapest completion found there.
template<bool M>
static bool search_suffix_for_prefix(
    const BeamState<M>& prefix,
    const MaterializedState<M>& mat,
    int suffix_depth,
    int branches,
    size_t suffix_width,
    uint64_t seed,
    int tid,
    int depth,
    const std::vector<Candidate>& perms,
    const std::vector<CostBucket>& cost_buckets,
    const Ranker& ranker,
    const Codec& codec,
    ThreadLocalLUTCache& lut_cache,
    BeamState<M>& found_parent,
    int& found_candidate,
    std::vector<int>& found_suffix,
    size_t& candidates_tested) {
    SuffixState start;
    start.missing = make_missing_states<M>(mat, ranker);
    start.missing_bits = make_state_bitset(start.missing, ranker);
    start.cost = 0;
    start.tie = hash_state_vector_limited(start.missing);

    bool have_solution = false;
    uint64_t best_suffix_cost = std::numeric_limits<uint64_t>::max();
    std::vector<int> best_suffix;

    auto consider_solution = [&](const std::vector<int>& suffix,
        uint64_t suffix_cost) {
        if (!have_solution || suffix_cost < best_suffix_cost ||
            (suffix_cost == best_suffix_cost&&
                suffix.size() < best_suffix.size())) {
            have_solution = true;
            best_suffix_cost = suffix_cost;
            best_suffix = suffix;
        }
    };

    if (start.missing.empty()) {
        consider_solution(start.seq, 0);
    }

    std::vector<SuffixState> beam_suf;
    if (!start.missing.empty()) beam_suf.push_back(std::move(start));
    uint64_t rng_seed =
        seed + 0x517cc1b727220a95ULL * uint64_t(depth + 1);
    std::mt19937_64 local_rng(mix_seed(rng_seed,
        tid));
    // Boost branch count only when the missing set is small (< 1% of universe).
    // With a large missing set the per-candidate work already dominates, so the
    // boost just inflates runtime without improving suffix quality.
    const int branch_mult = (start.missing.size() * 100 < ranker.total) ? 20 : 1;
    const int suffix_branches = std::min<int>(static_cast<int>(perms.size()),
        branches * branch_mult);

    for (int sd = 0; sd < suffix_depth && !beam_suf.empty(); ++sd) {
        std::vector<SuffixState> next;
        const size_t keep = std::max<size_t>(1, suffix_width);
        const size_t threshold = std::max<size_t>(keep + 1, 2 * keep);
        next.reserve(std::min<size_t>(threshold,
            beam_suf.size() * static_cast<size_t>(suffix_branches)));
        for (const SuffixState& ss : beam_suf) {
            std::vector<int> trial_candidates =
                choose_branch_candidates(suffix_branches,
                static_cast<int>(perms.size()),
                cost_buckets,
                local_rng);
            for (int ci : trial_candidates) {
                ++candidates_tested;
                SuffixState ext = extend_suffix_state(ss,
                    ci,
                    perms,
                    ranker,
                    codec,
                    lut_cache);
                if (ext.missing.empty()) {
                    consider_solution(ext.seq, ext.cost);
                    continue;
                }
                // If we already have a cheaper/equal suffix completion, this
                // partial suffix
                // cannot improve the reported cheapest completion because
                // costs are nonnegative.
                if (have_solution && ext.cost >= best_suffix_cost)
                    continue;
                next.push_back(std::move(ext));
                if (next.size() >= threshold) sort_and_trim_suffix(next,
                    keep);
            }
        }
        sort_and_trim_suffix(next, keep);
        beam_suf.swap(next);
    }

    if (have_solution) {
        found_parent = prefix;
        found_candidate = -1;
        found_suffix = std::move(best_suffix);
        return true;
    }
    return false;
}

// Per-worker result bucket.  Workers never append to shared child vectors;
// the main thread merges these buckets after joining.
template<bool M>
struct ThreadResult {
    std::vector<BeamState<M>> children;
    size_t parents_tested = 0;
    size_t candidates_tested = 0;
    uint32_t best_cover = 0;
    bool found = false;
    BeamState<M> found_parent;
    int found_candidate = -1;
    std::vector<int> found_suffix;
};

static uint64_t mix_seed(uint64_t seed, int tid) {
    uint64_t x = seed + 0x9e3779b97f4a7c15ULL * (uint64_t(tid) + 1);
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

// Emits a found solution.  When key is non-empty (algorithm-input mode) the
// "initial" line carries the brobdicube key and each step emits the algorithm
// text; otherwise the compact multiperm and cycle notation are used.
template<bool M>
static void append_solution(const BeamState<M>& st, int ci,
                            const std::vector<Candidate>& perms,
                            const Codec& codec, State init_state,
                            const std::string& key) {
    uint64_t total_cost = st.cost + (ci >= 0 ? perms[ci].cost : 0);
    std::cout << "FOUND depth " << (st.seq.size() + (ci >= 0 ? 1 : 0))
        << "\n";
    std::cout << "cost " << total_cost << "\n";
    std::cout << "initial " << (key.empty() ? codec.to_string(init_state) : key) << "\n";
    auto emit_step = [&](size_t j, int idx) {
        const Candidate& cand = perms[idx];
        std::cout << j << ": cost " << cand.cost << " ";
        if (!cand.text.empty()) std::cout << cand.text;
        else std::cout << perm_to_cycles(cand);
        std::cout << "\n";
    };
    for (size_t j = 0; j < st.seq.size(); ++j)
        emit_step(j, st.seq[j]);
    if (ci >= 0) emit_step(st.seq.size(), ci);
}

// Splits the current beam into contiguous worker ranges.
static std::vector<std::pair<size_t,
    size_t>> make_ranges(size_t nitems,
    int nthreads) {
    std::vector<std::pair<size_t,size_t>> ranges;
    ranges.reserve(static_cast<size_t>(nthreads));
    for (int t = 0; t < nthreads; ++t) {
        size_t a =
            (nitems * static_cast<size_t>(t)) /
            static_cast<size_t>(nthreads);
        size_t b =
            (nitems * static_cast<size_t>(t + 1)) /
            static_cast<size_t>(nthreads);
        if (a < b)
            ranges.push_back({a, b});
    }
    return ranges;
}

// Primary beam order: maximize coverage, then (when max_act active) minimize
// sum-of-squares of activation counts, then minimize cost, then stabilize by
// shorter sequence and deterministic tie hash.
template<bool M>
static bool beam_better(const BeamState<M>& a, const BeamState<M>& b) {
    if (a.cover_count != b.cover_count) return a.cover_count > b.cover_count;
    if constexpr (M) { if (a.sum_sq != b.sum_sq) return a.sum_sq < b.sum_sq; }
    if (a.cost != b.cost) return a.cost < b.cost;
    if (a.seq.size() != b.seq.size()) return a.seq.size() < b.seq.size();
    return a.tie < b.tie;
}

template<bool M>
static bool beam_worse_than(const BeamState<M>& a, const BeamState<M>& b) {
    return beam_better<M>(b, a);
}

// Hash key for beam trimming.  Cost and sum_sq are included so different paths
// to the same coverage hash can compete under the secondary objectives.
template<bool M>
static uint64_t beam_dedup_key(const BeamState<M>& st) {
    uint64_t key = st.tie
        ^ (uint64_t(st.cover_count) << 32)
        ^ uint64_t(st.seq.size())
        ^ (st.cost * 0x9e3779b97f4a7c15ULL);
    if constexpr (M) key ^= (st.sum_sq * 0x6c62272e07bb0142ULL);
    return key;
}

// Sorts, deduplicates, and trims a beam vector to a target width.
template<bool M>
static void sort_and_trim_to(std::vector<BeamState<M>>& v, size_t width) {
    if (v.empty()) return;
    std::sort(v.begin(), v.end(), beam_better<M>);
    std::vector<BeamState<M>> out;
    out.reserve(std::min(width, v.size()));
    std::unordered_set<uint64_t> seen;
    seen.reserve(width * 2 + 1);
    for (auto& st : v) {
        uint64_t key = beam_dedup_key<M>(st);
        if (seen.insert(key).second) {
            out.push_back(std::move(st));
            if (out.size() >= width) break;
        }
    }
    v.swap(out);
}

template<bool M>
static void sort_and_trim(std::vector<BeamState<M>>& v, size_t width) {
    sort_and_trim_to<M>(v, width);
}

// Streaming child insertion with local pruning.  Once the vector has been
// trimmed, children worse than the current worst kept item are skipped.
template<bool M>
static void maybe_add_child(std::vector<BeamState<M>>& children,
    BeamState<M>&& child,
                            size_t keep_limit, size_t trim_threshold,
                            bool& have_worst, BeamState<M>& worst) {
    if (keep_limit == 0) return;
    if (have_worst && children.size() >= keep_limit &&
        beam_worse_than<M>(child, worst))
        return;

    children.push_back(std::move(child));
    if (children.size() >= trim_threshold) {
        sort_and_trim_to<M>(children, keep_limit);
        if (!children.empty()) {
            worst = children.back();
            have_worst = (children.size() >= keep_limit);
        } else {
            have_worst = false;
        }
    }
}

struct RunParams {
    int width;
    int branches;
    int max_depth;
    int n;
    int base;
    uint64_t max_perm_cost;
    uint64_t seed;
    bool final_completion_mode;
    int suffix_depth;
    int num_threads;
    bool improve_only = false;  // -I: only accept coverage-improving candidates
    std::string key;   // brobdicube key if algorithm-input mode, else ""
    Codec codec;
    State init_state;
    Ranker ranker;
    std::vector<Candidate> perms;
    std::vector<CostBucket> cost_buckets;
    size_t lut_cache_budget_per_thread;
};

template<bool M>
static int run_beam(RunParams& p) {
    if constexpr (M) {
        if (max_act_limit > 4)
            throw std::runtime_error(
                "-k value > 4 is not supported; the 2-bit count encoding "
                "only handles max_act_limit in {1, 2, 3, 4}");
    }

    auto wall_now = []() {
        return std::chrono::duration<double>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    };

    std::cerr << "n=" << p.n << " symbols=" << p.base << " universe="
        << p.ranker.total
              << " candidates=" << p.perms.size()
              << " cost_buckets=" << p.cost_buckets.size()
              << " width=" << p.width
              << " branches=" << p.branches << " max_cost=" << p.max_perm_cost
                  << " seed=" << p.seed
              << " chunk_size=" << p.codec.chunk_size
              << " lut_entry_bytes=" << estimate_lut_entry_bytes(p.codec)
              << " lut_cache_per_thread=" << p.lut_cache_budget_per_thread
              << " memory_light=1 threads=" << p.num_threads
              << " final_completion=" << (p.final_completion_mode ? 1 : 0)
              << " suffix_depth=" << p.suffix_depth
              << " max_act=" << max_act_limit
              << " cheapest_suffix=1"
              << " stream_trim=1"
              << " improve_only=" << (p.improve_only ? 1 : 0) << "\n";

    MaterializedState<M> start_mat = make_start_materialized<M>(p.init_state, p.ranker);
    BeamState<M> start;
    start.cover_count = start_mat.cover_count;
    if constexpr (M) start.sum_sq = start_mat.ext.sum_sq;
    start.tie = start_mat.tie;

    std::vector<BeamState<M>> beam;
    beam.push_back(std::move(start));
    std::vector<MaterializedState<M>> beam_mats;
    beam_mats.push_back(make_start_materialized<M>(p.init_state, p.ranker));

    bool did_retry_suffix = false;
    (void)did_retry_suffix; // suppressed when final_completion_mode never triggers

    for (int depth = 0; depth < p.max_depth; ++depth) {
        const double step_start = wall_now();
        const bool completion_step = p.final_completion_mode &&
            (depth == p.max_depth - p.suffix_depth);

        if (completion_step) {
            std::vector<ThreadResult<M>> results(
                static_cast<size_t>(p.num_threads));
            std::vector<std::thread> workers;
            auto ranges = make_ranges(beam.size(), p.num_threads);
            workers.reserve(ranges.size());

            for (size_t ti = 0; ti < ranges.size(); ++ti) {
                size_t begin = ranges[ti].first;
                size_t end_range = ranges[ti].second;
                workers.emplace_back([&, ti, begin, end_range]() {
                    ThreadResult<M>& res = results[ti];
                    ThreadLocalLUTCache lut_cache(p.perms, p.codec,
                        p.lut_cache_budget_per_thread);
                    const size_t suffix_width = std::max<size_t>(1,
                        static_cast<size_t>(p.width));
                    for (size_t bi = begin; bi < end_range; ++bi) {
                        const BeamState<M>& st = beam[bi];
                        ++res.parents_tested;
                        const MaterializedState<M>& mat = beam_mats[bi];
                        if (mat.cover_count == p.ranker.total) {
                            res.found = true;
                            res.found_parent = st;
                            res.found_candidate = -1;
                            res.found_suffix.clear();
                            continue;
                        }
                        if (mat.cover_count > res.best_cover)
                            res.best_cover = mat.cover_count;

                        BeamState<M> fp;
                        int fc = -1;
                        std::vector<int> fs;
                        size_t tested_here = 0;
                        if (search_suffix_for_prefix<M>(st, mat,
                            p.suffix_depth, p.branches, suffix_width,
                            p.seed, static_cast<int>(ti), depth,
                            p.perms, p.cost_buckets, p.ranker, p.codec,
                            lut_cache, fp, fc, fs, tested_here)) {
                            res.candidates_tested += tested_here;
                            res.found = true;
                            res.found_parent = fp;
                            res.found_candidate = fc;
                            res.found_suffix = std::move(fs);
                            continue;
                        }
                        res.candidates_tested += tested_here;
                    }
                });
            }
            for (auto& th : workers) th.join();

            uint32_t best_cover = 0;
            size_t parents_tested = 0;
            size_t candidates_tested = 0;
            bool have_best_solution = false;
            BeamState<M> best_solution;
            int best_solution_candidate = -1;
            uint64_t best_solution_cost = std::numeric_limits<uint64_t>::max();
            for (const ThreadResult<M>& r : results) {
                parents_tested += r.parents_tested;
                candidates_tested += r.candidates_tested;
                best_cover = std::max(best_cover, r.best_cover);
                if (r.found) {
                    BeamState<M> sol = r.found_parent;
                    for (int ci : r.found_suffix) {
                        sol.seq.push_back(ci);
                        sol.cost += p.perms[ci].cost;
                    }
                    uint64_t sol_cost = sol.cost;
                    if (r.found_candidate >= 0)
                        sol_cost += p.perms[r.found_candidate].cost;
                    if (!have_best_solution ||
                        sol_cost < best_solution_cost ||
                        (sol_cost == best_solution_cost &&
                            sol.seq.size() < best_solution.seq.size())) {
                        have_best_solution = true;
                        best_solution = std::move(sol);
                        best_solution_candidate = r.found_candidate;
                        best_solution_cost = sol_cost;
                    }
                }
            }
            if (have_best_solution) {
                append_solution<M>(best_solution, best_solution_candidate,
                    p.perms, p.codec, p.init_state, p.key);
                return 0;
            }
            std::cerr << "completion depth " << (depth + p.suffix_depth)
                      << ": no suffix completion of length " << p.suffix_depth
                      << ", parents=" << parents_tested
                      << " candidates_tested=" << candidates_tested
                      << " best_prefix=" << best_cover << "/" << p.ranker.total
                      << " t=" << (wall_now() - step_start) << "s\n";
            if (p.suffix_depth != 1 || did_retry_suffix) break;
            // suffix_depth==1, first failure: do one forward step then
            // retry the suffix search one depth longer.
            did_retry_suffix = true;
            p.max_depth++;
        }

        // Phase 1: score all candidates read-only (no MaterializedState copy).
        std::vector<ThreadResult<M>> results(
            static_cast<size_t>(p.num_threads));
        {
            std::vector<std::thread> workers;
            auto ranges = make_ranges(beam.size(), p.num_threads);
            workers.reserve(ranges.size());
            for (size_t ti = 0; ti < ranges.size(); ++ti) {
                size_t begin = ranges[ti].first;
                size_t end_range = ranges[ti].second;
                workers.emplace_back([&, ti, begin, end_range]() {
                    ThreadResult<M>& res = results[ti];
                    const size_t assigned = end_range - begin;
                    const size_t local_limit = std::max<size_t>(1,
                        (2 * static_cast<size_t>(p.width) * assigned +
                         beam.size() - 1) / beam.size());
                    const size_t local_threshold = std::max<size_t>(
                        local_limit + 1, 2 * local_limit);
                    res.children.reserve(std::min(local_threshold,
                        assigned * static_cast<size_t>(p.branches)));
                    bool have_local_worst = false;
                    BeamState<M> local_worst;
                    ThreadLocalLUTCache lut_cache(p.perms, p.codec,
                        p.lut_cache_budget_per_thread);
                    uint64_t rng_seed =
                        p.seed + 0x9e3779b97f4a7c15ULL * uint64_t(depth + 1);
                    std::mt19937_64 local_rng(mix_seed(rng_seed,
                        static_cast<int>(ti)));
                    for (size_t bi = begin; bi < end_range; ++bi) {
                        const BeamState<M>& st = beam[bi];
                        const MaterializedState<M>& mat = beam_mats[bi];
                        std::vector<uint32_t> covered_ranks, uncovered_ranks;
                        if constexpr (M) {
                            // Use inverse mode only when uncovered states are
                            // a small fraction (<10%) of total — inverse mode
                            // iterates uncovered states, forward iterates
                            // filtered covered sources (val ∈ [1, k)).
                            // Switching too early (e.g. at 50%) makes things
                            // slower because filtered covered << uncovered.
                            if ((p.ranker.total - mat.cover_count) * 10 <
                                    p.ranker.total) {
                                uncovered_ranks = extract_uncovered_ranks(
                                    mat.covered, p.ranker.total);
                            } else {
                                const uint8_t klim = (uint8_t)max_act_limit;
                                covered_ranks.reserve(mat.cover_count);
                                uint32_t nw = (p.ranker.total + 63u) >> 6;
                                for (uint32_t w = 0; w < nw; ++w) {
                                    uint64_t bits = mat.covered[w];
                                    while (bits) {
                                        uint32_t b = static_cast<uint32_t>(
                                            __builtin_ctzll(bits));
                                        uint32_t r = w * 64u + b;
                                        uint8_t v = mat.ext.best_count.get(r);
                                        if (v != 0u && v < klim)
                                            covered_ranks.push_back(r);
                                        bits &= bits - 1;
                                    }
                                }
                            }
                        } else if (mat.cover_count * 2 > p.ranker.total)
                            uncovered_ranks = extract_uncovered_ranks(
                                mat.covered, p.ranker.total);
                        else
                            covered_ranks = extract_covered_ranks(
                                mat.covered, p.ranker.total);
                        auto score_and_maybe_add = [&](int ci) {
                            ScoreResult sr = score_candidate<M>(
                                mat, covered_ranks, uncovered_ranks, ci,
                                p.ranker, p.codec, lut_cache);
                            if (p.improve_only &&
                                    sr.cover_count <= mat.cover_count)
                                return;
                            BeamState<M> child;
                            child.seq = st.seq;
                            child.seq.push_back(ci);
                            child.cover_count = sr.cover_count;
                            if constexpr (M) child.sum_sq = sr.sum_sq;
                            child.tie = sr.tie;
                            child.cost = st.cost + p.perms[ci].cost;
                            child.parent_idx = static_cast<uint32_t>(bi);
                            child.child_cand = ci;
                            maybe_add_child<M>(res.children,
                                std::move(child), local_limit, local_threshold,
                                have_local_worst, local_worst);
                        };
                        if (!p.improve_only) {
                            std::vector<int> trial_candidates =
                                choose_branch_candidates(p.branches,
                                static_cast<int>(p.perms.size()),
                                p.cost_buckets, local_rng);
                            for (int ci : trial_candidates)
                                score_and_maybe_add(ci);
                        } else {
                            // -I mode: repeatedly draw random branch-sized
                            // batches until one yields an improving child.
                            const int pool =
                                static_cast<int>(p.perms.size());
                            while (res.children.empty()) {
                                const size_t before = res.children.size();
                                auto batch = choose_branch_candidates(
                                    p.branches, pool,
                                    p.cost_buckets, local_rng);
                                for (int ci : batch)
                                    score_and_maybe_add(ci);
                                if (res.children.size() > before)
                                    break;
                            }
                        }
                    }
                    sort_and_trim_to<M>(res.children, local_limit);
                });
            }
            for (auto& th : workers) th.join();
        }

        std::vector<BeamState<M>> next;
        const size_t global_limit = static_cast<size_t>(p.width);
        const size_t global_threshold = std::max<size_t>(global_limit + 1,
            2 * global_limit);
        next.reserve(global_threshold);
        bool have_global_worst = false;
        BeamState<M> global_worst;
        for (ThreadResult<M>& r : results) {
            for (auto& child : r.children) {
                if (have_global_worst &&
                    next.size() >= global_limit &&
                    beam_worse_than<M>(child, global_worst))
                    continue;
                next.push_back(std::move(child));
                if (next.size() >= global_threshold) {
                    sort_and_trim_to<M>(next, global_limit);
                    if (!next.empty()) {
                        global_worst = next.back();
                        have_global_worst = (next.size() >= global_limit);
                    } else {
                        have_global_worst = false;
                    }
                }
            }
            std::vector<BeamState<M>>().swap(r.children);
        }

        if (next.empty())
            throw std::runtime_error("beam died unexpectedly");
        sort_and_trim<M>(next, global_limit);
        const double phase1_end = wall_now();

        // Phase 2: build next_mats in parallel from parent beam_mats.
        // Each surviving child copies its parent's bitarray (~338 KB) and
        // applies one candidate — far cheaper than the old full-state copy.
        std::vector<MaterializedState<M>> next_mats(next.size());
        {
            std::vector<std::thread> p2_workers;
            auto p2_ranges = make_ranges(next.size(), p.num_threads);
            p2_workers.reserve(p2_ranges.size());
            for (size_t ti = 0; ti < p2_ranges.size(); ++ti) {
                size_t begin = p2_ranges[ti].first;
                size_t end_range = p2_ranges[ti].second;
                p2_workers.emplace_back([&, begin, end_range]() {
                    ThreadLocalLUTCache lut_cache(p.perms, p.codec,
                        p.lut_cache_budget_per_thread);
                    for (size_t i = begin; i < end_range; ++i) {
                        const BeamState<M>& child = next[i];
                        next_mats[i] = beam_mats[child.parent_idx];
                        std::vector<uint32_t> parent_ranks, parent_uncovered;
                        if constexpr (M) {
                            // Collect non-skip valid sources: val ∈ [1, k).
                            // The initial state is handled inside apply_candidate
                            // via init_rank and must NOT be in parent_ranks.
                            const auto& cov_bs = next_mats[i].covered;
                            const auto& bc = next_mats[i].ext.best_count;
                            parent_ranks.reserve(next_mats[i].cover_count);
                            const uint8_t klim = (uint8_t)max_act_limit;
                            uint32_t nw = (p.ranker.total + 63u) >> 6;
                            for (uint32_t w = 0; w < nw; ++w) {
                                uint64_t bits = cov_bs[w];
                                while (bits) {
                                    uint32_t b = static_cast<uint32_t>(
                                        __builtin_ctzll(bits));
                                    uint32_t r = w * 64u + b;
                                    uint8_t v = bc.get(r);
                                    if (v != 0u && v < klim)
                                        parent_ranks.push_back(r);
                                    bits &= bits - 1;
                                }
                            }
                        } else if (next_mats[i].cover_count * 2 > p.ranker.total)
                            parent_uncovered = extract_uncovered_ranks(
                                next_mats[i].covered, p.ranker.total);
                        else
                            parent_ranks = extract_covered_ranks(
                                next_mats[i].covered, p.ranker.total);
                        apply_candidate_in_place<M>(next_mats[i],
                            child.child_cand, parent_ranks, parent_uncovered,
                            p.perms, p.ranker, p.codec, lut_cache);
                    }
                });
            }
            for (auto& th : p2_workers) th.join();
        }

        beam.swap(next);
        beam_mats = std::move(next_mats);

        const BeamState<M>& best = beam.front();
        const double step_end = wall_now();
        std::cerr << "depth " << (depth + 1) << ": best "
            << best.cover_count << "/" << p.ranker.total;
        if constexpr (M) std::cerr << " sum_sq=" << best.sum_sq;
        std::cerr << " cost=" << best.cost
                  << " beam=" << beam.size()
                  << " p1=" << (phase1_end - step_start) << "s"
                  << " p2=" << (step_end - phase1_end) << "s"
                  << " t=" << (step_end - step_start) << "s\n";

        if (best.cover_count == p.ranker.total) {
            append_solution<M>(best, -1, p.perms, p.codec, p.init_state, p.key);
            return 0;
        }
    }

    std::cout << "NOT FOUND\n";
    return 1;
}

// Program driver: parse options, build rank tables, read candidates, then run
// either the forward beam or the reserved-suffix completion mode.
int main(int argc, char** argv) {
    try {
        int width = -1, branches = -1, max_depth = std::numeric_limits<int>::max();
        uint64_t max_perm_cost = std::numeric_limits<uint64_t>::max();
        uint64_t seed = static_cast<uint64_t>(
            std::chrono::high_resolution_clock::now()
                .time_since_epoch()
                .count());
        bool verbose = false;
        bool final_completion_mode = false;
        bool improve_only = false;
        int suffix_depth = 0;
        int num_threads = (int)std::thread::hardware_concurrency();
        std::string init;

        for (int i = 1; i < argc; ++i) {
            std::string a = argv[i];
            auto need_arg = [&](const char* opt) -> const char* {
                if (i + 1 >= argc) {
                    throw std::runtime_error(
                        std::string("missing argument for ") + opt);
                }
                return argv[++i];
            };
            if (a == "-w") width = std::stoi(need_arg("-w"));
            else if (a == "-b") branches = std::stoi(need_arg("-b"));
            else if (a == "-m") max_perm_cost = std::stoull(need_arg("-m"));
            else if (a == "-i") init = need_arg("-i");
            else if (a == "-s") seed = std::stoull(need_arg("-s"));
            else if (a == "-d") max_depth = std::stoi(need_arg("-d"));
            else if (a == "-x") {
                suffix_depth = 1;
                if (i + 1 < argc) {
                    std::string nxt = argv[i + 1];
                    if (!nxt.empty() && nxt[0] != '-') {
                        suffix_depth = std::stoi(argv[++i]);
                    }
                }
                if (suffix_depth < 0) {
                    throw std::runtime_error(
                        "-x suffix depth must be non-negative");
                }
                final_completion_mode = (suffix_depth > 0);
            }
            else if (a == "-t") num_threads = std::stoi(need_arg("-t"));
            else if (a == "-k") max_act_limit = std::stoi(need_arg("-k"));
            else if (a == "-I") improve_only = true;
            else if (a == "-v") verbose = true;
            else if (a == "-h" || a == "--help") { usage(argv[0]); return 0; }
            else throw std::runtime_error("unknown argument: " + a);
        }
        if (num_threads <= 0) {
            throw std::runtime_error("thread count must be positive");
        }
        if (width <= 0||
            branches <= 0||
            init.empty()) { usage(argv[0]); return 2; }

        // Detect key format ("x0x1x2", "010101") vs compact multiperm ("000011112222").
        // A brobdicube key is exactly 6 chars (one per face ULFRBD), each 'x'/'X' or a digit.
        auto is_key_char = [](char c){ return c == 'x' || c == 'X' || std::isdigit((unsigned char)c); };
        bool key_mode = (init.size() == 6 &&
                         std::all_of(init.begin(), init.end(), is_key_char));
        KeyInfo ki;
        std::string init_multiperm = init;
        std::string key_str;
        if (key_mode) {
            ki = parse_beam_key(init);
            init_multiperm = ki.initial;
            key_str = ki.key;
        }

        int n = static_cast<int>(init_multiperm.size());
        if (n < 2 || n > 24) {
            throw std::runtime_error("initial string length must be 2..24");
        }

        std::vector<int> init_digits(n);
        int maxsym = 0;
        for (int i = 0; i < n; ++i) {
            init_digits[i] = symbol_value(init_multiperm[i]);
            maxsym = std::max(maxsym, init_digits[i]);
        }
        int base = maxsym + 1;
        if (base < 2) base = 2;
        std::vector<int> counts(base, 0);
        for (int d : init_digits) ++counts[d];

        Codec codec;
        codec.init(n, base);
        State init_state = codec.encode_digits(init_digits);

        Ranker ranker;
        ranker.init(n, base, counts, codec);

        // In key mode: input uses 24-slot full CENTERS cycles from algutil.
        // cost_offset strips the two-depth-3-move component: 2 * 1000^3.
        const uint64_t cost_offset = key_mode ? 2'000'000'000ULL : 0ULL;
        std::vector<Candidate> perms = read_perms(stdin, n, verbose,
            max_perm_cost,
            key_mode ? &ki : nullptr,
            cost_offset);
        if (perms.empty()) {
            throw std::runtime_error("no candidate permutations read");
        }
        std::vector<CostBucket> cost_buckets = build_cost_buckets(perms);

        const size_t total_lut_cache_budget = size_t(1) << 30;
        const size_t lut_cache_budget_per_thread = std::max<size_t>(
            estimate_lut_entry_bytes(codec),
            total_lut_cache_budget /
                static_cast<size_t>(std::max(1, num_threads)));

        RunParams p;
        p.width = width;
        p.branches = branches;
        p.max_depth = max_depth;
        p.n = n;
        p.base = base;
        p.max_perm_cost = max_perm_cost;
        p.seed = seed;
        p.final_completion_mode = final_completion_mode;
        p.suffix_depth = suffix_depth;
        p.num_threads = num_threads;
        p.key = key_str;
        p.codec = std::move(codec);
        p.init_state = init_state;
        p.ranker = std::move(ranker);
        p.perms = std::move(perms);
        p.cost_buckets = std::move(cost_buckets);
        p.lut_cache_budget_per_thread = lut_cache_budget_per_thread;
        p.improve_only = improve_only;

        if (max_act_limit > 0) return run_beam<true>(p);
        else return run_beam<false>(p);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 2;
    }
}
