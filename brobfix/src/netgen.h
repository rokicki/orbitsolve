#pragma once
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <map>
#include <memory>
#include <mutex>
#include <ostream>
#include <utility>
#include "xcube.h"
#include "planmask.h"

extern std::mutex io_mutex ;
extern int fast, writemoves ;
extern ll blocks, runningmovecount ;
extern std::string algsdir ;
extern int greedyonly ;

double walltime() ;
double duration() ;

int oppositemoves(const mv &a, const mv &b) ;

struct blockalgo {
   std::vector<mv> mvs ;
   xorbit perm ;
   ll cost ; // vhoc each has three base-10 digits
   int v, h, o, c ;
   blockalgo(const std::vector<mv> &mvs_, const xorbit &perm_) :
                   mvs(mvs_), perm(perm_), cost(0), v(0), h(0), o(0), c(0) {
      int end = mvs.size()-1 ;
      int st = 0 ;
      while(st < end && mvs[st].dep == 1 && oppositemoves(mvs[st], mvs[end])) {
         end-- ;
         st++ ;
         c += 2 ;
      }
      for (int i=st; i<=end; i++) {
         switch (mvs[i].dep) {
case 1:     o++ ; break ;
case 2:     h++ ; break ;
case 3:     v++ ; break ;
default:    error("! unexpected depth in algorithm") ;
         }
      }
      cost = c + 10000 * (o + 10000 * (h + 10000LL * v)) ;
   }
   void show() const {
      for (auto m: mvs) {
         std::cout << " " ;
         showmove(m) ;
      }
   }
} ;

std::vector<blockalgo> loadalgs(const std::vector<std::string> &algs, int m=0) ;

extern std::vector<mv> curblock, prevblock ;
void sendblock() ;
ll doblock(xcube &cube, blockalgo &alg, std::vector<int> &ys, std::vector<int> &xs) ;

struct Stats {
   ll hits, moves ;
   double expectedcost ;
   blockalgo *ba ;
   int w1, w2 ;
   cubesizetype count1, count2 ;
   void showstats(int id, double denom) {
      if (hits) {
         std::cout << "Phase " << id << " p " << hits/denom << " mvs " << moves << " expected " << expectedcost ;
         if (count1 > 0 || count2 > 0) {
            std::cout << " widths:" ;
            if (count1 > 0) std::cout << " " << w1 << "x" << count1 ;
            if (count2 > 0) std::cout << " " << w2 << "x" << count2 ;
         }
         std::cout << std::endl ;
         std::cout << "   " ; ba->show() ; std::cout << std::endl ;
      }
   }
} ;
extern std::vector<Stats> byphase ;

const double INFCOST = 1e308 ;

/*
 *   Here we optimize for a discrete w.
 */
struct wplan {
   double cost ;
   cubesizetype My, count1, count2 ;
   int w1, w2 ;
   std::vector<double> byw ;
   void plan(cubesizetype My_) {
      if (My == My_)
         return ;
      My = My_ ;
      cost = INFCOST ;
      for (int i=1; i<(int)byw.size(); i++) {
         if (i > 1) {
            cubesizetype mless = (i - My % i) % i ;
            cubesizetype mi = (My - mless * (i - 1)) / i ;
            if (mi * i + mless * (i - 1) != My)
               error("! bad calculation") ;
            if (mi >= 0) {
               double t = mless * byw[i-1] + mi * byw[i] ;
               if (t < cost) {
                  cost = t ;
                  w1 = i-1 ;
                  w2 = i ;
                  count1 = mless ;
                  count2 = mi ;
               }
            }
         }
         cubesizetype mgreat = My % i ;
         cubesizetype c1 = My / i - mgreat ;
         if (c1 >= 0 && (mgreat == 0 || i + 1 < (int)byw.size())) {
            double t = c1 * byw[i] ;
            if (mgreat > 0)
               t += mgreat * byw[i+1] ;
            if (t < cost) {
               cost = t ;
               w1 = i ;
               w2 = i+1 ;
               count1 = c1 ;
               count2 = mgreat ;
            }
         }
      }
 if (count1 * w1 + count2 * w2 != My) std::cout << "count1 " << count1 << " w1 " << w1 << " count2 " << count2 << " w2 " << w2 << " My " << My << std::endl ;
      assert(count1 * w1 + count2 * w2 == My) ;
   }
} ;

struct netgen ;
struct reductionwplan {
   double cost ;
   netgen *net ;
   std::vector<wplan> plan ;
} ;

wplan optw(const std::vector<double> &byw, cubesizetype My) ;
wplan avcost(ll cost, cubesizetype Mx, cubesizetype My, double prob) ;
wplan wccost(ll cost, cubesizetype Mx, cubesizetype My) ;

/*
 *   Often we need to canonicalize or remap numbers so they are in order.
 */
struct remapper {
   remapper() {
      for (int i=0; i<12; i++)
         newval[i] = 0 ;
      at = '0' ;
   }
   char get(char c) {
      if (c == 'x')
         return 'x' ;
      if (newval[c-'0'] == 0)
         newval[c-'0'] = at++ ;
      return newval[c-'0'] ;
   }
   int at ;
   char newval[12] ;
   static void remap(std::string &k) {
      remapper remap ;
      for (int i=0; i<6; i++)
         k[i] = remap.get(k[i]) ;
   }
} ;

/*
 *   Pack/unpack tables shared across all netgens with the same digcounts.
 */
struct PackTables {
   std::vector<int> packhi, packlo ;
   std::vector<ll> unpack ;
   std::vector<ll> basepow ;
   int halfmod, goal, diggoal, base ;
   PackTables(int digcounts[4]) {
      base = (digcounts[3] ? 4 : digcounts[2] ? 3 : 2) ;
      int nonx = digcounts[0] + digcounts[1] + digcounts[2] + digcounts[3] ;
      basepow.resize(4 * nonx) ;
      basepow[0] = 1 ;
      for (int i=1; i<4*nonx; i++)
         basepow[i] = basepow[i-1] * base ;
      ll cases = 1, numer = 4 * nonx ;
      for (int i=0; i<4; i++)
         for (int j=0; j<4*digcounts[i]; j++) {
            cases = cases * numer / (j + 1) ;
            if (cases > 1000000000000000LL)
               error("! bad key; too many cases") ;
            numer-- ;
         }
      if (cases > 100000000)
         error("! bad key; too many cases") ;
      goal = cases ;
      halfmod = 1 ;
      for (int i=0; i<4*nonx; i += 2)
         halfmod *= base ;
      int digval[] = {1, 100, 10000, 1000000} ;
      std::vector<int> digsums(halfmod) ;
      digsums[0] = 4 * nonx / 2 ;
      for (int i=1; i<halfmod; i++)
         digsums[i] = digval[i % base] + digsums[i / base] - 1 ;
      diggoal = 4 * (digval[0] * digcounts[0] + digval[1] * digcounts[1] +
                     digval[2] * digcounts[2] + digval[3] * digcounts[3]) ;
      int at = 0 ;
      packhi.resize(halfmod, -1) ;
      packlo.resize(halfmod, -1) ;
      std::vector<std::pair<int,int>> sortme(halfmod) ;
      for (int th=0; th<halfmod; th++)
         sortme[th] = {digsums[th], th} ;
      std::sort(sortme.begin(), sortme.end()) ;
      // For base 3, encode each half-index in 2-bit-per-trit format so the hot
      // planning loop can extract groups with v & 255 / v >>= 8 (same as base 4).
      // ntrits = log3(halfmod) = 2*nonx trits per half-state.
      auto encode2bpt = [&](int val) -> ll {
         ll r = 0 ;
         for (int i = 0; i < 2*nonx; i++) { r |= (ll)(val % 3) << (2*i) ; val /= 3 ; }
         return r ;
      } ;
      int bw = halfmod - 1 ;
      for (int thi=0; thi<halfmod; thi++) {
         while (bw >= 0 && sortme[bw].first + sortme[thi].first >= diggoal)
            bw-- ;
         for (int bhi=bw+1; bhi<halfmod && sortme[bhi].first + sortme[thi].first == diggoal; bhi++) {
            if (packhi[sortme[thi].second] == -1)
               packhi[sortme[thi].second] = at ;
            if (packlo[sortme[bhi].second] == -1)
               packlo[sortme[bhi].second] = at - packhi[sortme[thi].second] ;
            ll u = (base == 3)
               ? (encode2bpt(sortme[thi].second) << (4*nonx)) | encode2bpt(sortme[bhi].second)
               : sortme[thi].second * (ll)halfmod + sortme[bhi].second ;
            unpack.push_back(u) ;
            at++ ;
         }
      }
      if (goal != at) {
         std::cout << "Digvals " << digval[0] << " " << digval[1] << " " << digval[2] << std::endl ;
         std::cout << "Digcounts " << digcounts[0] << " " << digcounts[1] << " " << digcounts[2] << std::endl ;
         std::cout << "Goal " << goal << " at " << at << " diggoal " << diggoal << std::endl ;
         error("! failed in goal computation") ;
      }
   }
} ;

std::shared_ptr<const PackTables> get_pack_tables(int digcounts[4]) ;

struct netgen {
   const static int MINWRITE = 50 ;
   int STAGE ;
   static const int n ;
   std::shared_ptr<const PackTables> pt ;
   std::vector<double> stageprob ;
   std::string key, filename ;
   std::vector<blockalgo> algs ;
   std::vector<ll> algcosts ;
   std::vector<std::string> stringalgs ;
   int halfmod, goal, diggoal ;
   int base ;
   int tolowdig[24] ;
   ll enccost ;
   int m ;
   bool optimal ;
   bool bad = false ;
   int used_perms = -1 ;
   int max_activations_seen = -1 ;
   // Guards the lazy-init in cost() below; see the comment there.
   std::mutex cost_mu ;
   netgen(std::string key_, const std::vector<std::string> &stringalgs_, const std::string &filename_, int m_=0, bool optimal_=false) {
      optimal = optimal_ ;
      m = m_ ;
      key.resize(6, 0) ;
      for (int i=0; i<6; i++)
         key[rots[m][i]] = key_[i] ;
      filename = filename_ ;
      stringalgs = stringalgs_ ;
      STAGE = stringalgs.size() ;
      int digcounts[4] = {} ;
      if (key.size() != 6)
         error("! invalid key 1") ;
      int at = 0 ;
      int f = 0 ;
      for (auto c: key) {
         if (c == 'x') {
            for (int i=0; i<4; i++)
               tolowdig[4*f+i] = -1 ;
         } else if ('0' <= c && c <= '3') {
            digcounts[c-'0']++ ;
            for (int i=0; i<4; i++)
               tolowdig[4*f+i] = at++ ;
         } else {
            std::cerr << key << std::endl ;
            error("! bad char in netgen key") ;
         }
         f++ ;
      }
      if (digcounts[0] == 0 || digcounts[1] == 0)
         error("! invalid key 2") ;
      pt = get_pack_tables(digcounts) ;
      halfmod = pt->halfmod ;
      goal    = pt->goal ;
      diggoal = pt->diggoal ;
      base    = pt->base ;
      enccost = 0 ;
   }
   void ensure_algs_loaded() {
      if (!algs.empty()) return ;
      algs = loadalgs(stringalgs, m) ;
      enccost = 0 ;
      for (auto &alg: algs) {
         enccost += alg.cost ;
         for (int f=0; f<6; f++)
            if (key[f] == 'x')
               for (int i=0; i<4; i++)
                  if (alg.perm.a[4*f+i] != 4*f+i)
                     error("! an algorithm in netgen modifies the wrong face") ;
      }
      if (algcosts.empty())
         for (auto &alg: algs)
            algcosts.push_back(alg.cost) ;
   }
   ll permdigs(ll v, xorbit &xo) {
      ll r = 0 ;
      if (base == 2 && halfmod == 4096) {
         for (int i=0; i<24; i++) {
            r += (v & 1) << xo.a[i] ;
            v >>= 1 ;
         }
      } else {
         for (int i=0; i<24; i += 4) {
            if (tolowdig[i] >= 0) {
               r += pt->basepow[tolowdig[xo.a[i]]] * (v % base) ;
               v /= base ;
               r += pt->basepow[tolowdig[xo.a[i+1]]] * (v % base) ;
               v /= base ;
               r += pt->basepow[tolowdig[xo.a[i+2]]] * (v % base) ;
               v /= base ;
               r += pt->basepow[tolowdig[xo.a[i+3]]] * (v % base) ;
               v /= base ;
            }
         }
      }
      return r ;
   }
   int build_fast_tab(const xorbit &xo, ll tab[][256]) const {
      // For base 3 we store 4 trits as 2 bits each (values 0-255, same as base 4).
      const int tab_size = (base == 3) ? 256 : base * base * base * base ;
      int ng = 0 ;
      for (int f = 0; f < 6; f++) {
         if (tolowdig[4*f] < 0) continue ;
         for (int val = 0; val < tab_size; val++) {
            ll contrib = 0 ; int tmp = val ;
            for (int di = 0; di < 4; di++) {
               int trit = (base == 3) ? (tmp & 3) : (tmp % base) ;
               contrib += pt->basepow[tolowdig[xo.a[4*f + di]]] * trit ;
               if (base == 3) tmp >>= 2 ; else tmp /= base ;
            }
            tab[ng][val] = contrib ;
         }
         ng++ ;
      }
      return ng ;
   }
   std::vector<planmask> plans ;
   int starti_for_plan() const {
      ll start = 0 ;
      for (int i=5; i>=0; i--)
         if (key[i] != 'x')
            for (int j=0; j<4; j++)
               start = start * base + key[i] - '0' ;
      return pt->packhi[start/halfmod] + pt->packlo[start%halfmod] ;
   }
   void finish_plan() {
      for (int i=0; i<goal; i++)
         if (plans[i].is_none()) {
            std::cerr << "Inconsistency in file: " << filename << std::endl ;
            std::cerr << "  key=" << key << " goal=" << goal << " STAGE=" << STAGE << " optimal=" << optimal << std::endl ;
            std::cerr << "  Could not get to state " << i << std::endl ;
            bad = true ;
            return ;
         }
      std::vector<int> cnts(STAGE) ;
      for (int i=0; i<goal; i++)
         for (int j=0; j<STAGE; j++)
            if (plans[i].test(j))
               cnts[j]++ ;
      used_perms = 0 ;
      for (int i=0; i<STAGE; i++)
         if (cnts[i] > 0)
            used_perms++ ;
      max_activations_seen = 0 ;
      for (int i=0; i<goal; i++) {
         int pc = plans[i].popcount() ;
         if (pc > max_activations_seen)
            max_activations_seen = pc ;
      }
      stageprob.clear() ;
      for (int i=0; i<STAGE; i++)
         stageprob.push_back(cnts[i]/(double)goal) ;
   }
   void plan_greedy() {
      ensure_algs_loaded() ;
      double t0 = walltime() ;
      if (pt->unpack.empty())
         error("! unpack should not be empty") ;
      std::vector<planmask> next(goal, planmask::none()) ;
      std::vector<ll> nextcost(goal, LLONG_MAX) ;
      std::vector<ll> curcost(goal, LLONG_MAX) ;
      plans.resize(goal, planmask::none()) ;
      int starti = starti_for_plan() ;
      plans[starti] = planmask{} ;
      curcost[starti] = 0 ;
      int covered = 1 ;
      const bool pow2base = !(base & (base - 1)) ;
      const int hmshift = pow2base ? __builtin_ctz(halfmod) : 0 ;
      const int hmmask  = halfmod - 1 ;
      ll min_overflow_nc = LLONG_MAX ;
      for (int i=0; i<STAGE && covered < goal; i++) {
         ll tab[6][256] ;
         int ng = build_fast_tab(algs[i].perm, tab) ;
         fill(next.begin(), next.end(), planmask::none()) ;
         fill(nextcost.begin(), nextcost.end(), LLONG_MAX) ;
         auto update = [&](int j, int d) {
            planmask newplan = plans[j].set(i) ;
            ll nc = curcost[j] + algs[i].cost ;
            if (newplan.is_none()) {
               if (nc < min_overflow_nc) min_overflow_nc = nc ;
               return ;
            }
            if (nc < nextcost[d]) { nextcost[d] = nc ; next[d] = newplan ; }
         } ;
         if (base == 4) {
            for (int j=0; j<goal; j++)
               if (!plans[j].is_none()) {
                  uint64_t v = pt->unpack[j] ; ll k = 0 ;
                  for (int g=0; g<ng; g++) { k += tab[g][v & 255] ; v >>= 8 ; }
                  update(j, pt->packhi[k >> hmshift] + pt->packlo[k & hmmask]) ;
               }
         } else if (base == 3) {
            for (int j=0; j<goal; j++)
               if (!plans[j].is_none()) {
                  uint64_t v = pt->unpack[j] ; ll k = 0 ;
                  for (int g=0; g<ng; g++) { k += tab[g][v & 255] ; v >>= 8 ; }
                  update(j, pt->packhi[k/halfmod] + pt->packlo[k%halfmod]) ;
               }
         } else { // base == 2
            for (int j=0; j<goal; j++)
               if (!plans[j].is_none()) {
                  uint64_t v = pt->unpack[j] ; ll k = 0 ;
                  for (int g=0; g<ng; g++) { k += tab[g][v & 15] ; v >>= 4 ; }
                  update(j, pt->packhi[k >> hmshift] + pt->packlo[k & hmmask]) ;
               }
         }
         for (int j=0; j<goal; j++)
            if (!next[j].is_none() && plans[j].is_none()) {
               plans[j] = next[j] ;
               curcost[j] = nextcost[j] ;
               covered++ ;
            }
      }
      if (min_overflow_nc != LLONG_MAX) {
         ll max_cost = 0 ;
         for (int j = 0; j < goal; j++)
            if (curcost[j] != LLONG_MAX && curcost[j] > max_cost)
               max_cost = curcost[j] ;
         if (max_cost > min_overflow_nc) {
            std::lock_guard<std::mutex> lk(io_mutex) ;
            std::cerr << "Warning: planmask overflow in " << key
                 << "; greedy plan may be suboptimal (min overflow cost "
                 << min_overflow_nc << ", max reached cost " << max_cost << ")\n" ;
         }
      }
      finish_plan() ;
      if (goal > 100000) {
         std::lock_guard<std::mutex> lk(io_mutex) ;
         std::cout << "Planned greedy " << key << " in " << (walltime() - t0) << std::endl ;
      }
   }
   void plan_optimal() {
      ensure_algs_loaded() ;
      double t0 = walltime() ;
      if (pt->unpack.empty())
         error("! unpack should not be empty") ;
      std::vector<planmask> next(goal, planmask::none()) ;
      std::vector<ll> nextcost(goal, LLONG_MAX) ;
      std::vector<ll> curcost(goal, LLONG_MAX) ;
      plans.resize(goal, planmask::none()) ;
      int starti = starti_for_plan() ;
      plans[starti] = planmask{} ;
      curcost[starti] = 0 ;
      const bool pow2base = !(base & (base - 1)) ;
      const int hmshift = pow2base ? __builtin_ctz(halfmod) : 0 ;
      const int hmmask  = halfmod - 1 ;
      ll min_overflow_nc = LLONG_MAX ;
      for (int i=0; i<STAGE; i++) {
         ll tab[6][256] ;
         int ng = build_fast_tab(algs[i].perm, tab) ;
         fill(next.begin(), next.end(), planmask::none()) ;
         fill(nextcost.begin(), nextcost.end(), LLONG_MAX) ;
         auto update = [&](int j, int d) {
            planmask newplan = plans[j].set(i) ;
            ll nc = curcost[j] + algs[i].cost ;
            if (newplan.is_none()) {
               if (nc < min_overflow_nc) min_overflow_nc = nc ;
               return ;
            }
            if (nc < nextcost[d]) { nextcost[d] = nc ; next[d] = newplan ; }
         } ;
         if (base == 4) {
            for (int j=0; j<goal; j++)
               if (!plans[j].is_none()) {
                  uint64_t v = pt->unpack[j] ; ll k = 0 ;
                  for (int g=0; g<ng; g++) { k += tab[g][v & 255] ; v >>= 8 ; }
                  update(j, pt->packhi[k >> hmshift] + pt->packlo[k & hmmask]) ;
               }
         } else if (base == 3) {
            for (int j=0; j<goal; j++)
               if (!plans[j].is_none()) {
                  uint64_t v = pt->unpack[j] ; ll k = 0 ;
                  for (int g=0; g<ng; g++) { k += tab[g][v & 255] ; v >>= 8 ; }
                  update(j, pt->packhi[k/halfmod] + pt->packlo[k%halfmod]) ;
               }
         } else { // base == 2
            for (int j=0; j<goal; j++)
               if (!plans[j].is_none()) {
                  uint64_t v = pt->unpack[j] ; ll k = 0 ;
                  for (int g=0; g<ng; g++) { k += tab[g][v & 15] ; v >>= 4 ; }
                  update(j, pt->packhi[k >> hmshift] + pt->packlo[k & hmmask]) ;
               }
         }
         for (int j=0; j<goal; j++)
            if (!next[j].is_none() && nextcost[j] < curcost[j]) {
               plans[j] = next[j] ;
               curcost[j] = nextcost[j] ;
            }
      }
      if (min_overflow_nc != LLONG_MAX) {
         ll max_cost = 0 ;
         for (int j = 0; j < goal; j++)
            if (curcost[j] != LLONG_MAX && curcost[j] > max_cost)
               max_cost = curcost[j] ;
         if (max_cost > min_overflow_nc) {
            std::lock_guard<std::mutex> lk(io_mutex) ;
            std::cerr << "Warning: planmask overflow in " << key
                 << "; optimal plan may be suboptimal (min overflow cost "
                 << min_overflow_nc << ", max reached cost " << max_cost << ")\n" ;
         }
      }
      finish_plan() ;
      if (goal > 100000) {
         std::lock_guard<std::mutex> lk(io_mutex) ;
         std::cout << "Planned optimal " << key << " in " << (walltime() - t0) << std::endl ;
      }
   }
   void plan() {
      if (plans.size())
         return ;
      if (optimal)
         plan_optimal() ;
      else
         plan_greedy() ;
   }
   planmask getplan(xorbit &orb) {
      if (plans.empty())
         plan() ;
      ll k = 0 ;
      for (int i=23; i>=0; i--) {
         if (tolowdig[i] >= 0) {
            k *= base ;
            if (key[orb.a[i]] == 'x')
               error("! oops; hit an x") ;
            k += key[orb.a[i]] - '0' ;
         }
      }
      return plans[pt->packhi[k/halfmod]+pt->packlo[k%halfmod]] ;
   }
   void doplan(planmask plan, xorbit &orb) {
      if (plan.is_bitmask()) {
         for (int pi=STAGE-1; pi>=0; pi--)
            if (plan.test(pi))
               permimul(orb, algs[pi].perm) ;
      } else {
         int fields[7], n = plan.decode(fields) ;
         for (int k = n-1; k >= 0; k--)
            permimul(orb, algs[fields[k]].perm) ;
      }
   }
   FILE *openprobcache(const char *how) {
      int len = filename.size() ;
      if (len < 4 || strcmp(filename.c_str()+len-4, ".alg") != 0) {
         std::cerr << "Looking at filename " << filename << std::endl ;
         error("! algorithm filename seems bogus") ;
      }
      std::string probname(filename) ;
      probname.resize(len-3) ;
      probname += optimal ? "prob3-optimal" : "prob3-greedy" ;
      FILE *r = fopen(probname.c_str(), how) ;
      return r ;
   }
   bool ensureprobs() {
      if (goal > MINWRITE) {
         FILE *f = openprobcache("r") ;
         if (f) {
            ll cost ; double v ;
            while (true) {
               int r = fscanf(f, "%lld", &cost) ;
               if (r != 1) break ;
               int r2 = fscanf(f, " %lg", &v) ;
               if (r2 == 1) {
                  algcosts.push_back(cost) ;
                  stageprob.push_back(v) ;
               } else {
                  max_activations_seen = (int)cost ;
                  break ;
               }
            }
            fclose(f) ;
            if (stageprob.size() != (size_t)STAGE) {
               std::cerr << "For " << filename << std::endl ;
               error("! probability cache file seems corrupt") ;
            }
            used_perms = 0 ;
            for (auto v : stageprob)
               if (v > 0) used_perms++ ;
         }
      }
      bool computed = (stageprob.size() == 0) ;
      if (computed) {
         plan() ;
         if (bad)
            return false ;
         if (goal > MINWRITE) {
            FILE *f = openprobcache("w") ;
            if (f == 0)
               error("! cannot write file probability cache") ;
            for (int i = 0; i < (int)stageprob.size(); i++)
               fprintf(f, "%lld %.15lg\n", algcosts[i], stageprob[i]) ;
            fprintf(f, "%d\n", max_activations_seen) ;
            fclose(f) ;
         }
      }
      plans.clear() ;
      plans.shrink_to_fit() ;
      return computed ;
   }
   reductionwplan cost(cubesizetype Mx, cubesizetype My, char typ) {
      // cost() is called on shared netgen objects from every -P worker
      // thread concurrently.  Normally preload_probs() has already warmed
      // stageprob/algcosts for every netgen before any of those threads
      // start, so the lazy-init below never actually mutates anything here.
      // But nothing enforces that invariant, so guard it for real:
      // double-checked locking makes ensureprobs()/ensure_algs_loaded()
      // safe even if two threads reach a not-yet-warmed netgen at once.
      if (typ == 'a' && stageprob.size() == 0) {
         std::lock_guard<std::mutex> lk(cost_mu) ;
         if (stageprob.size() == 0)
            ensureprobs() ;
      }
      if (algcosts.empty()) {
         std::lock_guard<std::mutex> lk(cost_mu) ;
         if (algcosts.empty())
            ensure_algs_loaded() ;
      }
      reductionwplan r ;
      r.cost = 0 ;
      r.net = this ;
      for (int i=0; i<(int)algcosts.size(); i++) {
         wplan rr ;
         if (typ == 'a')
            rr = ::avcost(algcosts[i], Mx, My, stageprob[i]) ;
         else if (typ == 'w')
            rr = ::wccost(algcosts[i], Mx, My) ;
         else
            error("! bad cost type") ;
         r.plan.push_back(rr) ;
         r.cost += rr.cost ;
      }
      return r ;
   }
} ;

std::string canon(std::string key, int mod=0) ;
extern std::map<std::string, std::vector<netgen *>> algset ;
void addgen(std::string key, netgen *ng) ;
void readalgs() ;

extern std::map<std::string, double> bestcostcache ;
extern std::map<std::string, reductionwplan> bestcostplan ;
int splitby(const std::string &key, std::string &k2, std::string &k3) ;
double bestcost(std::string key, cubesizetype Mx, cubesizetype My, char typ,
                std::map<std::string,double> &cache,
                std::map<std::string,reductionwplan> &planmap) ;
double bestcost(std::string key, cubesizetype Mx, cubesizetype My, char typ) ;

extern std::vector<reductionwplan> reductionplan ;
void explain(std::string key,
             const std::map<std::string,reductionwplan> &planmap,
             std::vector<reductionwplan> &rplan,
             std::ostream &out) ;
void explain(std::string key) ;
