#include <algorithm>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>
#include <sys/stat.h>

struct Solution {
    int depth = 0;
    long long max_activations = 0;
    long long sum_activations = 0;
    double avg_activations = 0.0;
    int coverage = 0;
    int total_states = 0;
    long long sum_v = 0;  // sum of depth-3 inner move counts across algorithms
    long long sum_h = 0;  // sum of depth-2 inner move counts across algorithms
    long long sum_o = 0;  // sum of outer (face) inner move counts across algorithms
    long long sum_c = 0;  // sum of conjugation wrapper move counts across algorithms
    long long total_cost = 0;
    std::string shortkey;             // 6-char brobdicube key from "initial" line
    std::vector<std::string> algorithms;  // algorithm texts from step lines
    std::string source;
};

// a dominates b if a <= b in all available metrics and strictly < in at least one.
// max_activations is only used as a dimension when both solutions have it (> 0);
// logs that predate the max_activations field leave it 0 (unknown).
static bool dominates(const Solution& a, const Solution& b) {
    bool both_have_max = (a.max_activations > 0 && b.max_activations > 0);
    if (a.depth > b.depth || a.sum_activations > b.sum_activations || a.total_cost > b.total_cost)
        return false;
    if (both_have_max && a.max_activations > b.max_activations)
        return false;
    return a.depth < b.depth || a.sum_activations < b.sum_activations || a.total_cost < b.total_cost
        || (both_have_max && a.max_activations < b.max_activations);
}

static void flush_solution(Solution& sol, bool& in_found, std::vector<Solution>& out) {
    if (in_found && !sol.algorithms.empty()) {
        bool complete = (sol.depth > 0 && (int)sol.algorithms.size() == sol.depth);
        if (complete) {
            if (sol.avg_activations == 0.0 && sol.sum_activations > 0)
                sol.avg_activations = (double)sol.sum_activations / (double)sol.total_states;
            sol.total_cost = sol.sum_v * 1000000000LL
                           + sol.sum_h * 1000000LL
                           + sol.sum_o * 1000LL
                           + sol.sum_c;
            out.push_back(sol);
        }
    }
    in_found = false;
    sol = Solution();
}

// Compute v/h/o/c move-type counts directly from an algorithm text string.
// Strips outer conjugate pairs (matching letter-move inverse pairs), then
// classifies remaining moves: '3'→v, '2'→h, letter→o.
static void compute_vhoc_from_text(const std::string &alg,
                                    int &v, int &h, int &o, int &c) {
    v = h = o = c = 0;
    std::vector<std::string> toks;
    std::istringstream iss(alg);
    std::string tok;
    while (iss >> tok) toks.push_back(tok);
    int lo = 0, hi = (int)toks.size() - 1;
    while (lo < hi) {
        const auto &front = toks[lo];
        const auto &back  = toks[hi];
        if (!isalpha((unsigned char)front[0]) || !isalpha((unsigned char)back[0])) break;
        std::string inv;
        if (front.back() == '\'') inv = front.substr(0, front.size() - 1);
        else if (front.back() == '2') inv = front;
        else inv = front + "'";
        if (inv != back) break;
        c += 2; lo++; hi--;
    }
    for (int i = lo; i <= hi; i++) {
        char d = toks[i][0];
        if      (isalpha((unsigned char)d)) o++;
        else if (d == '2')                   h++;
        else if (d == '3')                   v++;
    }
}

static void parse_stream(FILE* f, const std::string& source, std::vector<Solution>& out) {
    char buf[65536];
    Solution cur;
    bool in_found = false;

    while (fgets(buf, (int)sizeof(buf), f)) {
        char* nl = strchr(buf, '\n');
        if (nl) *nl = '\0';
        const char* line = buf;

        if (strncmp(line, "FOUND depth ", 12) == 0) {
            flush_solution(cur, in_found, out);
            in_found = true;
            cur = Solution();
            cur.source = source;
            cur.depth = atoi(line + 12);
            continue;
        }

        if (strncmp(line, "NOT FOUND", 9) == 0) {
            flush_solution(cur, in_found, out);
            continue;
        }

        // "depth N: best ..." progress line ends any open FOUND block
        if (strncmp(line, "depth ", 6) == 0) {
            flush_solution(cur, in_found, out);
            continue;
        }

        if (!in_found) continue;

        if (strncmp(line, "coverage ", 9) == 0) {
            char* q;
            cur.coverage = (int)strtol(line + 9, &q, 10);
            if (*q == '/') cur.total_states = (int)strtol(q + 1, nullptr, 10);
        } else if (strncmp(line, "max_activations ", 16) == 0) {
            cur.max_activations = strtoll(line + 16, nullptr, 10);
        } else if (strncmp(line, "sum_activations ", 16) == 0) {
            cur.sum_activations = strtoll(line + 16, nullptr, 10);
        } else if (strncmp(line, "average_activations ", 20) == 0) {
            cur.avg_activations = strtod(line + 20, nullptr);
        } else if (strncmp(line, "initial ", 8) == 0) {
            // Value is the brobdicube key directly, e.g. "x0x1x2"
            cur.shortkey = line + 8;
        } else {
            // Step line: "N: cost COST ALGORITHM_TEXT"
            const char* colon = strchr(line, ':');
            if (colon && strncmp(colon + 1, " cost ", 6) == 0) {
                char* p;
                strtoll(colon + 7, &p, 10);  // skip cost field; recompute from text
                while (*p == ' ') ++p;
                cur.algorithms.push_back(p);
                int cv = 0, ch = 0, co = 0, cc = 0;
                compute_vhoc_from_text(p, cv, ch, co, cc);
                cur.sum_v += cv;
                cur.sum_h += ch;
                cur.sum_o += co;
                cur.sum_c += cc;
            }
        }
    }
    flush_solution(cur, in_found, out);
}

static uint64_t fnv1a(const std::vector<std::string>& algs) {
    uint64_t h = 14695981039346656037ULL;
    for (const auto& s : algs) {
        for (unsigned char c : s) {
            h ^= c;
            h *= 1099511628211ULL;
        }
        h ^= 0u;
        h *= 1099511628211ULL;
    }
    return h;
}

// ---------------------------------------------------------------------------
// Directory-based Pareto analysis (-D ALGSDIR)
// ---------------------------------------------------------------------------

struct DirSol {
    std::string filename;   // basename of .alg file
    std::string key;        // 6-char shortkey
    std::string mode;       // "greedy" or "optimal"
    int    depth = 0;
    int    max_activations = 0;  // last line of prob3 file
    long long sum_v = 0, sum_h = 0, sum_o = 0, sum_c = 0;
    long long total_cost = 0;
    double sum_prob = 0.0;  // sum of stage probabilities from prob3 file
};

// Parse "{key}-{depth}-{v}-{h}-{o}-{c}-{hash}.alg" into a DirSol.
// Returns false if the filename doesn't match the 6-hyphen format.
static bool parse_alg_filename(const std::string& name, DirSol& sol) {
    if (name.size() < 10) return false;
    if (name.compare(name.size() - 4, 4, ".alg") != 0) return false;
    std::string stem = name.substr(0, name.size() - 4);
    std::vector<std::string> parts;
    std::string cur;
    for (char c : stem) {
        if (c == '-') { parts.push_back(cur); cur.clear(); }
        else cur += c;
    }
    parts.push_back(cur);
    if (parts.size() != 7) return false;  // need key+depth+v+h+o+c+hash
    sol.key = parts[0];
    if (sol.key.size() != 6) return false;
    sol.depth   = atoi(parts[1].c_str());
    sol.sum_v   = atoll(parts[2].c_str());
    sol.sum_h   = atoll(parts[3].c_str());
    sol.sum_o   = atoll(parts[4].c_str());
    sol.sum_c   = atoll(parts[5].c_str());
    sol.total_cost = sol.sum_v * 1000000000LL + sol.sum_h * 1000000LL
                   + sol.sum_o * 1000LL + sol.sum_c;
    sol.filename = name;
    return true;
}

// Read effective_depth, sum_prob, and max_activations from a .prob3-MODE sidecar.
// effective_depth = number of stages with non-zero probability (stages that are
// actually used by at least one orbit state's plan).  Returns false if the file
// does not exist.
static bool read_prob3_one(const std::string& dir, const std::string& base,
                           const char* mode, int& effective_depth,
                           double& sum_prob, int& max_activations) {
    std::string path = dir + "/" + base + ".prob3-" + mode;
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return false;
    double sum = 0.0, prob;
    long long cost = 0;
    int n_nonzero = 0;
    while (fscanf(f, "%lld %lf", &cost, &prob) == 2) {
        sum += prob;
        if (prob > 0.0) n_nonzero++;
    }
    fclose(f);
    effective_depth = n_nonzero;
    sum_prob = sum;
    max_activations = (int)cost;  // last line: single integer = max_activations_seen
    return true;
}

// a dir-dominates b if a <= b on all four metrics and strictly < on at least one.
// sum_prob is compared with a small epsilon to absorb floating-point accumulation
// differences that arise when the same rational stage probabilities appear in
// different stage orderings across prob3 files.  Stage probabilities are
// rational numbers whose denominators are at most ~60M, so the smallest
// meaningful difference between two distinct sum_prob values is ~1/60M ≈ 1.7e-8.
// EPS = 1e-9 sits safely below that threshold while collapsing pure FP noise.
static bool dir_dominates(const DirSol& a, const DirSol& b) {
    static constexpr double EPS = 1e-9;
    if (a.depth > b.depth || a.max_activations > b.max_activations ||
        a.total_cost > b.total_cost || a.sum_prob > b.sum_prob + EPS)
        return false;
    return a.depth < b.depth || a.max_activations < b.max_activations ||
           a.total_cost < b.total_cost || a.sum_prob + EPS < b.sum_prob;
}

static void link_file(const std::string& src, const std::string& dst) {
    if (link(src.c_str(), dst.c_str()) != 0 && errno != EEXIST)
        fprintf(stderr, "warning: link %s -> %s: %s\n",
                src.c_str(), dst.c_str(), strerror(errno));
}

static void link_alg_and_sidecars(const std::string& src_dir, const std::string& dst_dir,
                                   const std::string& filename) {
    std::string base = filename.substr(0, filename.size() - 4); // strip ".alg"
    link_file(src_dir + "/" + filename,          dst_dir + "/" + filename);
    for (const char* suf : { ".prob3-greedy", ".prob3-optimal" }) {
        std::string src = src_dir + "/" + base + suf;
        struct stat st;
        if (stat(src.c_str(), &st) == 0)
            link_file(src, dst_dir + "/" + base + suf);
    }
}

static int analyze_algs_dir(const char* dir, bool keep_only, bool unique_only,
                             const char* out_dir) {
    DIR* d = opendir(dir);
    if (!d) {
        fprintf(stderr, "error: cannot open directory %s\n", dir);
        return 1;
    }

    std::vector<DirSol> sols;
    std::vector<std::string> unrecognised;
    int n_alg_files = 0;

    static const char* modes[] = { "greedy", "optimal" };

    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
        std::string name = entry->d_name;
        if (name.size() < 4 || name.compare(name.size() - 4, 4, ".alg") != 0)
            continue;
        DirSol tmpl;
        if (!parse_alg_filename(name, tmpl)) {
            unrecognised.push_back(name);
            continue;
        }
        std::string base = name.substr(0, name.size() - 4);
        bool found_any = false;
        for (const char* mode : modes) {
            DirSol sol = tmpl;
            sol.mode = mode;
            if (read_prob3_one(dir, base, mode, sol.depth, sol.sum_prob, sol.max_activations)) {
                sols.push_back(std::move(sol));
                found_any = true;
            }
        }
        if (!found_any) {
            fprintf(stderr, "error: no .prob3-greedy or .prob3-optimal file for %s\n",
                    name.c_str());
            closedir(d);
            return 1;
        }
        n_alg_files++;
    }
    closedir(d);

    if (!unrecognised.empty()) {
        fprintf(stderr, "warning: %zu .alg file(s) with unrecognised filename format skipped:\n",
                unrecognised.size());
        for (const auto& n : unrecognised)
            fprintf(stderr, "  %s\n", n.c_str());
    }

    if (!keep_only)
        printf("Loaded %d .alg files from %s (%zu entries)\n", n_alg_files, dir, sols.size());

    // Group entries by key for dominance checking (cross-mode domination is allowed).
    std::map<std::string, std::vector<size_t>> by_key;
    for (size_t i = 0; i < sols.size(); i++)
        by_key[sols[i].key].push_back(i);

    // Compute per-entry domination and record the best dominator for reporting.
    std::vector<bool> entry_dominated(sols.size(), false);
    std::vector<const DirSol*> entry_dominator(sols.size(), nullptr);
    for (auto& [key, indices] : by_key) {
        for (size_t ii : indices) {
            const DirSol& a = sols[ii];
            const DirSol* best = nullptr;
            for (size_t jj : indices) {
                if (ii == jj) continue;
                if (dir_dominates(sols[jj], a)) {
                    // pick lexicographically first (filename, then mode) for a stable report
                    if (!best || sols[jj].filename < best->filename ||
                        (sols[jj].filename == best->filename && sols[jj].mode < best->mode))
                        best = &sols[jj];
                }
            }
            if (best) {
                entry_dominated[ii] = true;
                entry_dominator[ii] = best;
            }
        }
    }

    // Aggregate by filename: a file is dominated only when ALL its entries are dominated.
    std::map<std::string, std::vector<size_t>> by_filename;
    for (size_t i = 0; i < sols.size(); i++)
        by_filename[sols[i].filename].push_back(i);

    std::vector<const DirSol*> keep_entries;
    int n_dominated_files = 0;
    for (auto& [fname, findices] : by_filename) {
        bool all_dominated = true;
        for (size_t ii : findices)
            if (!entry_dominated[ii]) { all_dominated = false; break; }

        if (all_dominated) {
            if (!keep_only) {
                printf("DOMINATED %s\n", fname.c_str());
                for (size_t ii : findices) {
                    const DirSol& a = sols[ii];
                    const DirSol* dom = entry_dominator[ii];
                    printf("  [%s] depth=%d max_act=%d cost=%lld sum_prob=%.6f\n"
                           "    by %s [%s] depth=%d max_act=%d cost=%lld sum_prob=%.6f\n",
                           a.mode.c_str(),
                           a.depth, a.max_activations, a.total_cost, a.sum_prob,
                           dom->filename.c_str(), dom->mode.c_str(),
                           dom->depth, dom->max_activations, dom->total_cost, dom->sum_prob);
                }
            }
            n_dominated_files++;
        } else if (keep_only) {
            for (size_t ii : findices)
                if (!entry_dominated[ii])
                    keep_entries.push_back(&sols[ii]);
        }
    }

    if (keep_only) {
        if (out_dir) {
            if (mkdir(out_dir, 0755) != 0 && errno != EEXIST) {
                fprintf(stderr, "error: cannot create output directory %s: %s\n",
                        out_dir, strerror(errno));
                return 1;
            }
        }

        auto emit = [&](const DirSol* a) {
            printf("%s [%s] depth=%d max_act=%d cost=%lld sum_prob=%.6f\n",
                   a->filename.c_str(), a->mode.c_str(),
                   a->depth, a->max_activations, a->total_cost, a->sum_prob);
            if (out_dir)
                link_alg_and_sidecars(dir, out_dir, a->filename);
        };

        if (unique_only) {
            std::sort(keep_entries.begin(), keep_entries.end(),
                [](const DirSol* a, const DirSol* b) {
                    if (a->depth           != b->depth)           return a->depth           < b->depth;
                    if (a->max_activations != b->max_activations) return a->max_activations < b->max_activations;
                    if (a->total_cost      != b->total_cost)      return a->total_cost      < b->total_cost;
                    return a->sum_prob < b->sum_prob;
                });
            const DirSol* last = nullptr;
            for (const DirSol* a : keep_entries) {
                if (last &&
                    a->depth           == last->depth &&
                    a->max_activations == last->max_activations &&
                    a->total_cost      == last->total_cost &&
                    std::abs(a->sum_prob - last->sum_prob) < 1e-9)
                    continue;
                emit(a);
                last = a;
            }
        } else {
            for (const DirSol* a : keep_entries)
                emit(a);
        }
    }

    if (!keep_only) {
        int n_total = (int)by_filename.size();
        int n_undominated = n_total - n_dominated_files;
        printf("\n%d dominated file%s, %d undominated file%s (of %d total)\n",
               n_dominated_files, n_dominated_files == 1 ? "" : "s",
               n_undominated,     n_undominated     == 1 ? "" : "s",
               n_total);
    }
    return 0;
}

static void usage(const char* prog) {
    fprintf(stderr,
        "Usage: %s [-o OUTDIR] [-d MAXDEPTH] [-n] LOGFILE...\n"
        "       %s -D ALGSDIR\n"
        "\n"
        "Log-file mode (default):\n"
        "  -o OUTDIR     output directory for .alg files (default: algs3)\n"
        "  -d MAXDEPTH   ignore solutions with depth > MAXDEPTH\n"
        "  -n            dry run: report solution count without writing files\n"
        "  LOGFILE...    findpermnet log files (required; no stdin)\n"
        "  Parses FOUND blocks, deduplicates, computes Pareto front on\n"
        "  (depth, max_activations, sum_activations, total_cost), and writes\n"
        "  non-dominated solutions as .alg files.\n"
        "\n"
        "Directory mode (-D):\n"
        "  -D ALGSDIR    analyse existing .alg files in ALGSDIR\n"
        "  -k            with -D: print only undominated files (one per line with\n"
        "                stats); suppress DOMINATED lines and summary\n"
        "  -u            with -k -D: suppress duplicate entries that are equivalent\n"
        "                on all four metrics; emit only one representative per group\n"
        "  Reads each .alg file's filename (depth + v/h/o/c cost fields) and its\n"
        "  .prob3-greedy or .prob3-optimal file (error if missing) to get the sum\n"
        "  of stage probabilities.  Reports files dominated on all four metrics\n"
        "  (depth, max_activations, total_cost, sum_prob) by another file with the\n"
        "  same key.  No files are written or removed.\n",
        prog, prog);
}

int main(int argc, char* argv[]) {
    for (int i = 0; i < argc; i++) {
        if (i > 0) fputc(' ', stdout);
        fputs(argv[i], stdout);
    }
    fputc('\n', stdout);

    const char* out_dir = nullptr;
    const char* algs_dir_arg = nullptr;
    bool dry_run = false;
    bool keep_only = false;
    bool unique_only = false;
    int max_depth = INT_MAX;
    std::vector<const char*> log_files;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out_dir = argv[++i];
        } else if (strcmp(argv[i], "-D") == 0 && i + 1 < argc) {
            algs_dir_arg = argv[++i];
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            max_depth = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-k") == 0) {
            keep_only = true;
        } else if (strcmp(argv[i], "-u") == 0) {
            unique_only = true;
        } else if (strcmp(argv[i], "-n") == 0) {
            dry_run = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        } else {
            log_files.push_back(argv[i]);
        }
    }

    if (algs_dir_arg) {
        return analyze_algs_dir(algs_dir_arg, keep_only, unique_only, out_dir);
    }

    if (log_files.empty()) {
        usage(argv[0]);
        return 1;
    }

    std::vector<Solution> all_solutions;
    for (const char* path : log_files) {
        FILE* f = fopen(path, "r");
        if (!f) {
            fprintf(stderr, "error: cannot open %s\n", path);
            exit(10);
        }
        size_t before = all_solutions.size();
        parse_stream(f, path, all_solutions);
        fclose(f);
        fprintf(stderr, "  %zu solution(s) from %s\n",
                all_solutions.size() - before, path);
    }
    fprintf(stderr, "Total: %zu solutions parsed\n", all_solutions.size());

    // Drop solutions without a valid 6-char shortkey
    {
        size_t before = all_solutions.size();
        all_solutions.erase(
            std::remove_if(all_solutions.begin(), all_solutions.end(),
                           [](const Solution& s) { return s.shortkey.size() != 6; }),
            all_solutions.end());
        if (all_solutions.size() < before)
            fprintf(stderr, "Dropped %zu solution(s) with missing/invalid shortkey\n",
                    before - all_solutions.size());
    }

    // Apply depth filter
    if (max_depth != INT_MAX) {
        size_t before = all_solutions.size();
        all_solutions.erase(
            std::remove_if(all_solutions.begin(), all_solutions.end(),
                           [max_depth](const Solution& s) { return s.depth > max_depth; }),
            all_solutions.end());
        if (all_solutions.size() < before)
            fprintf(stderr, "After depth filter (<= %d): %zu solutions\n",
                    max_depth, all_solutions.size());
    }

    // Deduplicate by (shortkey, algorithm texts)
    {
        std::set<std::pair<std::string, std::vector<std::string>>> seen;
        std::vector<Solution> unique;
        for (auto& sol : all_solutions)
            if (seen.insert({sol.shortkey, sol.algorithms}).second)
                unique.push_back(std::move(sol));
        if (unique.size() < all_solutions.size())
            fprintf(stderr, "After dedup: %zu unique solutions\n", unique.size());
        all_solutions = std::move(unique);
    }

    // Pareto front per shortkey
    size_t n = all_solutions.size();
    std::vector<bool> dominated(n, false);
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            if (i == j) continue;
            if (all_solutions[i].shortkey != all_solutions[j].shortkey) continue;
            if (dominates(all_solutions[j], all_solutions[i])) {
                dominated[i] = true;
                break;
            }
        }
    }

    std::vector<Solution> pareto;
    for (size_t i = 0; i < n; i++)
        if (!dominated[i]) pareto.push_back(std::move(all_solutions[i]));

    // Sort by (shortkey, depth, sum_activations, max_activations)
    std::sort(pareto.begin(), pareto.end(), [](const Solution& a, const Solution& b) {
        if (a.shortkey != b.shortkey) return a.shortkey < b.shortkey;
        if (a.depth != b.depth) return a.depth < b.depth;
        if (a.sum_activations != b.sum_activations) return a.sum_activations < b.sum_activations;
        return a.max_activations < b.max_activations;
    });

    fprintf(stderr, "Pareto front: %zu solutions\n", pareto.size());

    const char* effective_out_dir = out_dir ? out_dir : "algs3";

    if (dry_run) {
        printf("%zu solution(s) would be written to %s/\n", pareto.size(), effective_out_dir);
        return 0;
    }

    mkdir(effective_out_dir, 0755);

    int new_count = 0, existing_count = 0;
    for (const auto& sol : pareto) {
        std::string content;
        for (const std::string& alg : sol.algorithms) {
            content += ' ';
            content += alg;
            content += '\n';
        }

        char hash_str[17];
        snprintf(hash_str, sizeof(hash_str), "%016llx",
                 (unsigned long long)fnv1a(sol.algorithms));

        char fname[1024];
        snprintf(fname, sizeof(fname), "%s/%s-%04d-%04lld-%04lld-%04lld-%04lld-%s.alg",
                 effective_out_dir, sol.shortkey.c_str(), sol.depth,
                 sol.sum_v, sol.sum_h, sol.sum_o, sol.sum_c, hash_str);

        struct stat st;
        bool already_exists = (stat(fname, &st) == 0);
        if (already_exists) {
            existing_count++;
        } else {
            FILE* out = fopen(fname, "w");
            if (out) {
                fputs(content.c_str(), out);
                fclose(out);
                new_count++;
            } else {
                fprintf(stderr, "error: cannot write %s\n", fname);
            }
        }

        printf("%s depth=%d max_activations=%lld sum_activations=%lld"
               " avg_activations=%.6g sum_cost=%lld/%lld/%lld/%lld"
               " shortkey=%s\n"
               "  source=%s\n  -> %s\n",
               already_exists ? "EXISTING" : "NEW",
               sol.depth, (long long)sol.max_activations,
               (long long)sol.sum_activations, sol.avg_activations,
               sol.sum_v, sol.sum_h, sol.sum_o, sol.sum_c,
               sol.shortkey.c_str(),
               sol.source.c_str(), fname);
    }

    fprintf(stderr, "Written: %d new, %d already existed\n", new_count, existing_count);
    return 0;
}
