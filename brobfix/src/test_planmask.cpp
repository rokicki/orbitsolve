#include "planmask.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

// Reference implementation: sorted vector + explicit mode tracking.
// Mirrors planmask's mode-selection logic so check_pair can verify both
// correctness and minimality of the chosen mode.
struct ref_planmask {
   bool is_none_ = false ;
   std::vector<int> elems ;
   int mode_ = 0 ;  // 0=bitmask, 1-5=compact, 8=legacy

   static ref_planmask none() { ref_planmask r ; r.is_none_ = true ; return r ; }
   bool is_none() const { return is_none_ ; }
   int  mode()    const { return mode_ ; }

   ref_planmask set(int i) const {
      if (is_none_) return none() ;

      // Idempotent: already present.
      std::vector<int> ne = elems ;
      auto it = std::lower_bound(ne.begin(), ne.end(), i) ;
      if (it != ne.end() && *it == i) return *this ;
      ne.insert(it, i) ;

      // Mirror planmask::set() mode selection exactly.
      int cur_m = mode_ ;
      int new_m ;
      if (cur_m == 0 && i < 60) {
         // Bitmask fast path: stays bitmask, never overflows (at most 60 unique values < 60).
         ref_planmask r ; r.elems = std::move(ne) ; r.mode_ = 0 ; return r ;
      } else if (i < 511) {
         new_m = (cur_m >= 1 && cur_m <= 5) ? cur_m : 8 ;
      } else {
         int base = (cur_m >= 1 && cur_m <= 5) ? cur_m : 0 ;
         new_m = std::max(base, planmask::req_compact(i)) ;
      }

      if (new_m < 0) return none() ;
      int cap = (new_m == 0) ? 60 : planmask::N_SLOTS[new_m] ;
      if ((int)ne.size() > cap) return none() ;

      ref_planmask r ; r.elems = std::move(ne) ; r.mode_ = new_m ; return r ;
   }

   bool test(int i) const {
      if (is_none_) return false ;
      return std::binary_search(elems.begin(), elems.end(), i) ;
   }

   int popcount() const {
      if (is_none_) return 0 ;
      return (int)elems.size() ;
   }
} ;

static long n_pass = 0, n_fail = 0 ;

static void check(const char *desc, bool cond) {
   if (cond) n_pass++ ;
   else { n_fail++ ; printf("FAIL: %s\n", desc) ; }
}

static void check_pair(const char *label, planmask pm, ref_planmask rm) {
   char buf[256] ;

   // is_none must agree — this catches overflow at the wrong point.
   snprintf(buf, sizeof(buf), "%s: is_none", label) ;
   check(buf, pm.is_none() == rm.is_none()) ;
   if (pm.is_none() != rm.is_none()) return ;
   if (pm.is_none()) return ;

   // Mode: bitmask=0 exact, legacy=8..15 (data nybble varies), compact=exact.
   int pm_m = pm.mode(), rm_m = rm.mode() ;
   bool mode_ok = (rm_m == 0) ? (pm_m == 0)           // bitmask: exact
                : (rm_m == 8) ? (pm_m >= 8)            // legacy: 8-15 all valid
                :               (pm_m == rm_m) ;       // compact: exact
   snprintf(buf, sizeof(buf), "%s: mode pm=%d expected=%d", label, pm_m, rm_m) ;
   check(buf, mode_ok) ;

   // popcount.
   snprintf(buf, sizeof(buf), "%s: popcount", label) ;
   check(buf, pm.popcount() == rm.popcount()) ;

   // test() for every element the reference says is present.
   for (int v : rm.elems) {
      snprintf(buf, sizeof(buf), "%s: test(%d) true", label, v) ;
      check(buf, pm.test(v)) ;
   }

   // test() false for boundary values not in the reference.
   for (int v : {0, 1, 59, 60, 510, 511, 1022, 1023, 32766, 32767,
                 (1<<20)-1, (1<<20), (1<<30)-2}) {
      if (!rm.test(v)) {
         snprintf(buf, sizeof(buf), "%s: test(%d) false", label, v) ;
         check(buf, !pm.test(v)) ;
      }
   }

   // decode() must return the same sorted elements.
   if (!pm.is_bitmask()) {
      int fields[7], n = pm.decode(fields) ;
      bool count_ok = (n == (int)rm.elems.size()) ;
      snprintf(buf, sizeof(buf), "%s: decode count=%d expected=%d",
               label, n, (int)rm.elems.size()) ;
      check(buf, count_ok) ;
      int lim = std::min(n, (int)rm.elems.size()) ;
      for (int k = 0; k < lim; k++) {
         snprintf(buf, sizeof(buf), "%s: decode[%d]=%d expected=%d",
                  label, k, fields[k], rm.elems[k]) ;
         check(buf, fields[k] == rm.elems[k]) ;
      }
   }
}

// test_sequence requires indices to be sorted and duplicate-free (matching set()'s
// contract for compact modes: increasing order, each value at most once).
// Idempotency is only tested for bitmask mode, where OR is inherently idempotent.
static void test_sequence(const char *label, const std::vector<int> &indices) {
   planmask pm ;
   ref_planmask rm ;
   char buf[512] ;
   for (int i = 0; i < (int)indices.size(); i++) {
      int idx = indices[i] ;
      planmask    pm2 = pm.set(idx) ;
      ref_planmask rm2 = rm.set(idx) ;
      snprintf(buf, sizeof(buf), "%s[%d] insert(%d)", label, i, idx) ;
      check_pair(buf, pm2, rm2) ;
      if (!pm2.is_none() && pm2.is_bitmask()) {
         snprintf(buf, sizeof(buf), "%s[%d] idempotent(%d)", label, i, idx) ;
         check_pair(buf, pm2.set(idx), rm2.set(idx)) ;
      }
      pm = pm2 ; rm = rm2 ;
      if (pm.is_none()) break ;
   }
}

// ---- Fixed sanity tests -------------------------------------------------

static void test_basics() {
   planmask pm ;
   check("empty is_bitmask",    pm.is_bitmask()) ;
   check("empty !is_none",      !pm.is_none()) ;
   check("empty popcount==0",   pm.popcount() == 0) ;
   check("empty test(0)==false", !pm.test(0)) ;

   planmask n = planmask::none() ;
   check("none is_none",            n.is_none()) ;
   check("none popcount==0",        n.popcount() == 0) ;
   check("none test(0)==false",     !n.test(0)) ;
   check("none.set(0) is_none",     n.set(0).is_none()) ;
   check("none.set(99999) is_none", n.set(99999).is_none()) ;
}

static void test_bitmask_overflow() {
   // Fill all 60 bitmask slots, then verify that adding a value outside [0,59]
   // (which forces a mode transition needing 61 slots in legacy's 7) returns none().
   planmask pm ;
   for (int i = 0; i < 60; i++) pm = pm.set(i) ;
   check("60 bitmask slots: not none",    !pm.is_none()) ;
   check("60 bitmask slots: is_bitmask",  pm.is_bitmask()) ;
   check("60 bitmask + set(60)  → none", pm.set(60).is_none()) ;
   check("60 bitmask + set(512) → none", pm.set(512).is_none()) ;
}

// ---- Bucket-based random tests ------------------------------------------

// One bucket per mode: pick a random bucket, get a random index in its full range.
// A single sequence drawing from multiple buckets naturally exercises mode transitions
// and overflow at the right points.
static const int BUCKET_MAX[] = {
   59,           // 0: bitmask      (0..59)
   510,          // 1: legacy       (0..510)
   1022,         // 2: compact 1    (0..1022)
   4094,         // 3: compact 2    (0..4094)
   32766,        // 4: compact 3    (0..32766)
   (1<<20)-2,    // 5: compact 4    (0..2^20-2)
   (1<<30)-2,    // 6: compact 5    (0..2^30-2)
} ;
static constexpr int N_BUCKETS = (int)(sizeof(BUCKET_MAX) / sizeof(BUCKET_MAX[0])) ;

static void run_random_trial(std::mt19937_64 &rng, long trial) {
   int n = (int)(rng() % 16) + 2 ;   // 2..17 insertions per trial
   std::vector<int> seq ;
   for (int k = 0; k < n; k++) {
      int bucket = (int)(rng() % N_BUCKETS) ;
      seq.push_back((int)(rng() % ((uint64_t)BUCKET_MAX[bucket] + 1))) ;
   }
   // set() requires values in increasing order with no duplicates (compact mode contract).
   std::sort(seq.begin(), seq.end()) ;
   seq.erase(std::unique(seq.begin(), seq.end()), seq.end()) ;
   char label[128] ;
   snprintf(label, sizeof(label), "rnd_t%ld", trial) ;
   test_sequence(label, seq) ;
}

static void test_random(uint64_t seed, int trials) {
   std::mt19937_64 rng(seed) ;
   for (int t = 0; t < trials; t++)
      run_random_trial(rng, t) ;
}

// ---- Main ---------------------------------------------------------------

int main(int argc, char *argv[]) {
   int secs = 0 ;
   if (argc > 1) secs = atoi(argv[1]) ;

   test_basics() ;
   test_bitmask_overflow() ;
   test_random(0xdeadbeef12345678ULL, 2000) ;
   test_random(0xfeedcafe87654321ULL, 2000) ;

   if (secs > 0) {
      printf("Running stress test for %d second%s...\n", secs, secs == 1 ? "" : "s") ;
      fflush(stdout) ;
      using clock = std::chrono::steady_clock ;
      auto t_start  = clock::now() ;
      auto t_report = t_start ;
      auto t_end    = t_start + std::chrono::seconds(secs) ;
      std::mt19937_64 rng(std::random_device{}()) ;
      long trials = 0 ;
      while (clock::now() < t_end) {
         for (int b = 0; b < 64; b++)
            run_random_trial(rng, trials++) ;
         auto now = clock::now() ;
         if (now - t_report >= std::chrono::seconds(5)) {
            double elapsed = std::chrono::duration<double>(now - t_start).count() ;
            printf("  %5.0fs  %ld trials  %ld passed  %ld failed\n",
                   elapsed, trials, n_pass, n_fail) ;
            fflush(stdout) ;
            t_report = now ;
         }
      }
      double total = std::chrono::duration<double>(clock::now() - t_start).count() ;
      printf("Stress done: %ld trials in %.1fs (%.0f trials/s)\n",
             trials, total, trials / total) ;
   }

   printf("%ld passed, %ld failed\n", n_pass, n_fail) ;
   return n_fail ? 1 : 0 ;
}
