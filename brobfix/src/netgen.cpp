#include "netgen.h"
#include <cmath>
using namespace std ;

#if WRITEFRAMES
extern ll writemoveinc, writemovetarget ;
extern void writeframe(xcube &) ;
#endif

int oppositemoves(const mv &a, const mv &b) {
   return (a.dep == b.dep && a.face == b.face && a.twist + b.twist == 4) ;
}

vector<blockalgo> loadalgs(const vector<string> &algs, int m) {
   expandrotations() ;
   xcube cube(4, 5, 'U') ;
   vector<mv> mappedmoves ;
   vector<blockalgo> r ;
   for (int i=0; i<(int)algs.size(); i++) {
      auto mvs = parse_moves(algs[i]) ;
      uclear(cube.rows[0][1]) ;
      mappedmoves.clear() ;
      for (auto mv: mvs) {
         mv.face = rots[m][mv.face] ;
         mappedmoves.push_back(mv) ;
         domove(mv, cube, true) ;
      }
      for (ll j=0; j<cube.My; j++)
         for (ll k=0; k<cube.Mx; k++)
            if (j != 0 && k != 1) {
               for (int p=0; p<24; p++)
                  if (cube.rows[j][k].a[p] != p) {
                     cout << "Failed alg " << i << " at " << j << " " << k << " touches " << p << endl ;
                     error("! algorithm hurts other orbits?") ;
                  }
            }
      int changed = 0 ;
      for (int i=0; i<24; i++)
         if (cube.rows[0][1].a[i] != i)
            changed++ ;
      if (changed == 0)
         error("! algorithm doesn't change any cubies") ;
      r.push_back(blockalgo(mappedmoves, cube.rows[0][1])) ;
   }
   return r ;
}

vector<mv> curblock, prevblock ;

void sendblock() {
   while (curblock.size() > 0 && prevblock.size() > 0) {
      mv merged ;
      int r = trymerge(prevblock[prevblock.size()-1], curblock[0], merged) ;
      if (r == 0) {
         curblock.erase(curblock.begin()) ;
         prevblock.pop_back() ;
      } else if (r == 1) {
         curblock[0] = merged ;
         prevblock.pop_back() ;
      } else {
         break ;
      }
   }
   if (prevblock.size()) {
      for (auto mv: prevblock) {
         cout << " " ;
         showmove(mv) ;
      }
      cout << endl ;
   }
   swap(prevblock, curblock) ;
   curblock.clear() ;
}

ll doblock(xcube &cube, blockalgo &alg, vector<int> &ys, vector<int> &xs) {
#if WRITEFRAMES
   if (writemoveinc && runningmovecount >= writemovetarget)
      writeframe(cube) ;
#endif
   ll oldcancellations = cancellations ;
   blocks++ ;
   if (writemoves) {
      for (auto mv: alg.mvs) {
         if (mv.dep == 2) {
            for (auto v:ys) {
               mv.dep = v + 2 ;
               curblock.push_back(mv) ;
            }
         } else if (mv.dep == 3) {
            for (auto v:xs) {
               mv.dep = v + 2 ;
               curblock.push_back(mv) ;
            }
         } else {
            curblock.push_back(mv) ;
         }
      }
      sendblock() ;
   }
   if (!fast) {
      for (auto mv: alg.mvs) {
         if (mv.dep == 2) {
            for (auto v:ys) {
               mv.dep = v + 2 ;
               domove(mv, cube) ;
            }
         } else if (mv.dep == 3) {
            for (auto v:xs) {
               mv.dep = v + 2 ;
               domove(mv, cube) ;
            }
         } else {
            domove(mv, cube)  ;
         }
      }
   } else {
      for (int i=0; i<alg.c; i++)
         trycancellation(alg.mvs[i]) ;
      prevmvs.clear() ;
      for (int i=alg.c-1; i>=0; i--)
         trycancellation(alg.mvs[alg.mvs.size()-1-i]) ;
   }
   ll mvs = alg.o + alg.v * xs.size() + alg.h * ys.size() + alg.c -
                                         (cancellations - oldcancellations) ;
   runningmovecount += mvs ;
   return mvs ;
}

vector<Stats> byphase ;

// MAXW bounds the widths optw may choose from.  It cannot be raised much: the
// solver allocates and scans 2^w entries per strip per stage (see the
// prv.assign(1LL << w, ...) / bp < (1LL << w) loops in brobdicube.cpp), so w is
// exponential in both time and memory there, and w >= 63 in a long long shift
// is outright undefined behaviour.
static const int MAXW = 24 ;

wplan optw(const vector<double> &byw, cubesizetype My) {
   wplan r = {INFCOST, -1, -1LL, -1LL, -1, -1, byw} ;
   r.plan(My) ;
   return r ;
}

wplan avcost(ll cost, cubesizetype Mx, cubesizetype My, double prob) {
   vector<double> byw = {0} ;
   int v = cost / 1000000000000LL ;
   int h = cost / 100000000 % 10000 ;
   int o = cost / 10000 % 10000 ;
   int c = cost % 10000 ;
   double q = 1 - prob ;
   double ratio = prob / q ;
   double qpow = q ;
   for (int w=1; ; w++) {
      double t = 0 ;
      double p1 = qpow * ratio ;
      double choose = w ;
      for (int bs=1; bs<=w; bs++) {
         double any = -expm1(Mx * log1p(-p1));
         t += choose * (v * p1 * Mx + h * any * bs + o * any) ;
         choose = choose * (w - bs) / (bs + 1) ;
         p1 *= ratio ;
      }
      byw.push_back(t) ;
      // A strip wider than My is meaningless.
      if ((cubesizetype)w >= My)
         break ;
      // (Mx >> (w + 1)) == 0 means w has reached lg(Mx), where the pattern
      // space 2^w overtakes the Mx columns and reuse across columns saturates.
      // Widening past that still helps, by amortizing the per-strip costs over
      // more rows -- which matters when Mx is small, and is why we go past it
      // at all (at Mx = 9 the old cap left optw stuck at w = 3).  MAXW bounds
      // only that excursion.  It must never bound w below lg(Mx): w ~ lg(Mx) is
      // what makes the move count n^2/lg n rather than n^2, and there 2^w ~ Mx,
      // so the solver's 2^w pattern table is merely linear in the cube size.
      if (w >= MAXW && (Mx >> (w + 1)) == 0)
         break ;
      qpow *= q ;
   }
   auto r = optw(byw, My) ;
   // The conjugation wrapper cancels between successive applications of the
   // same stage (the stage loop is outside the inner w-striping), so it is
   // paid once per stage that actually fires, independent of w.  Add it after
   // optw so it cannot distort the width choice.  wccost is the prob=1 limit.
   r.cost += c * -expm1((double)Mx * (double)My * log1p(-prob)) ;
   return r ;
}

wplan wccost(ll cost, cubesizetype Mx, cubesizetype My) {
   vector<double> byw = {0} ;
   int v = cost / 1000000000000LL ;
   int h = cost / 100000000 % 10000 ;
   int o = cost / 10000 % 10000 ;
   int c = cost % 10000 ;
   for (int w=1; ; w++) {
      // 2^w as a double: it overflows a long long long before it overflows a
      // double, and the old (Mx >> (w + 1)) == 0 bound let w reach 63 for a
      // large enough Mx, where 1LL << w is undefined behaviour.  Bound w the
      // same way avcost does instead.
      double tw = ldexp(1.0, w) ;
      double t = (v * (double)Mx + h * w * tw * 0.5 + o * (tw - 1)) ;
      byw.push_back(t) ;
      if ((cubesizetype)w >= My || w >= MAXW)
         break ;
   }
   auto r = optw(byw, My) ;
   r.cost += c ;
   return r ;
}

static mutex pack_cache_mu ;
static map<array<int,4>, shared_ptr<const PackTables>> pack_cache ;

shared_ptr<const PackTables> get_pack_tables(int digcounts[4]) {
   array<int,4> key = {digcounts[0], digcounts[1], digcounts[2], digcounts[3]} ;
   lock_guard<mutex> lk(pack_cache_mu) ;
   auto it = pack_cache.find(key) ;
   if (it != pack_cache.end())
      return it->second ;
   auto pt = make_shared<PackTables>(digcounts) ;
   return pack_cache.emplace(key, std::move(pt)).first->second ;
}

string canon(string key, int mod) {
   string r, k2(6, 0) ;
   for (int m=0; m<24; m++) {
      int renum[6] ;
      int at = '0' ;
      for (int i=0; i<6; i++)
         renum[i] = 0 ;
      for (int i=0; i<6; i++) {
         int c = key[rots[m][i]] ;
         if (c == 'x') {
            k2[i] = c ;
         } else {
            if (renum[c-'0'] == 0) {
               renum[c-'0'] = at ;
               if (!mod)
                  at++ ;
            }
            k2[i] = renum[c-'0'] ;
         }
      }
      if (r.size() == 0 || k2 < r)
         r = k2 ;
   }
   return r ;
}

map<string, vector<netgen *>> algset ;

void addgen(string key, netgen *ng) {
   key = canon(key, 1) ;
   algset[key].push_back(ng) ;
}

void readalgs() {
   expandrotations() ;
   duration() ;
   const string &dirname = algsdir ;
   DIR *dir = opendir(dirname.c_str()) ;
   if (dir == 0)
      error("! no algs directory to read") ;
   int nalgsloaded = 0 ;
   while (struct dirent *entry = readdir(dir)) {
      int len = strlen(entry->d_name) ;
      if (len < 4 || strcmp(entry->d_name + len - 4, ".alg") != 0)
         continue ;
      int validalgfile = 1 ;
      for (int i=0; validalgfile && i<6; i++)
         if (strchr("x0123", entry->d_name[i]) == 0)
            validalgfile = 0 ;
      if (!validalgfile || entry->d_name[6] != '-') {
         fprintf(stderr, "! invalid .alg filename (bad key or missing '-'): %s\n",
                 entry->d_name) ;
         error("! invalid .alg filename") ;
      }
      int nhyphens = 0 ;
      for (int i=6; i<len-4; i++)
         if (entry->d_name[i] == '-') nhyphens++ ;
      if (nhyphens == 5 || nhyphens == 6) {
         long long depth = atoll(entry->d_name + 7) ;
         if (depth > planmask::max_stages()) {
            fprintf(stderr, "! .alg filename depth %lld exceeds max_stages (%d): %s\n",
                    depth, planmask::max_stages(), entry->d_name) ;
            error("! .alg filename depth exceeds max_stages") ;
         }
      } else if (nhyphens != 2) {
         fprintf(stderr, "! unrecognised .alg filename format (%d hyphens): %s\n",
                 nhyphens, entry->d_name) ;
         error("! unrecognised .alg filename format") ;
      }
      const char *key = strndup(entry->d_name, 6) ;
      string filepath = dirname + "/" + entry->d_name ;
      FILE *f = fopen(filepath.c_str(), "r") ;
      if (f == 0)
         error("! cannot read algorithm file") ;
      vector<string> algs ;
      char inpline[1024] ;
      while (fgets(inpline, 1000, f)) {
         char *p = inpline + strlen(inpline) - 1 ;
         while (p >= inpline && *p <= ' ')
            *p-- = 0 ;
         if (p > inpline)
            algs.push_back(inpline) ;
      }
      fclose(f) ;
      netgen *gen_greedy = new netgen(key, algs, filepath, 0, false) ;
      addgen(key, gen_greedy) ;
      if (!greedyonly) {
         netgen *gen_optimal = new netgen(key, algs, filepath, 0, true) ;
         addgen(key, gen_optimal) ;
      }
      nalgsloaded++ ;
   }
   closedir(dir) ;
   cout << "Loaded " << nalgsloaded << " alg" << (nalgsloaded == 1 ? "" : "s")
        << " in " << duration() << endl ;
}

map<string, double> bestcostcache ;
map<string, reductionwplan> bestcostplan ;

int splitby(const string &key, string &k2, string &k3) {
   int firstmod = -1 ;
   for (int i=0; firstmod < 0 && i<5; i++)
      if (key[i] != 'x')
         for (int j=i+1; firstmod < 0 && j<6; j++)
            if (key[j] == key[i])
               firstmod = i ;
   if (firstmod == -1)
      return 0 ;
   k2 = string(6, 0) ;
   k3 = key ;
   for (int i=0; i<6; i++)
      if (key[i] == key[firstmod]) {
         k2[i] = '0' ;
         k3[i] = 'x' ;
      } else
         k2[i] = 'x' ;
   remapper remap ;
   for (int i=0; i<6; i++)
      if (k3[i] != 'x')
         k3[i] = remap.get(k3[i]) ;
   return 1 ;
}

double bestcost(string key, cubesizetype Mx, cubesizetype My, char typ,
                map<string,double> &cache, map<string,reductionwplan> &planmap) {
   int cnt[6] ;
   for (int i=0; i<6; i++)
      cnt[i] = 0 ;
   for (int i=0; i<6; i++)
      if (key[i] != 'x')
         cnt[key[i]-'0']++ ;
   for (int i=0; i<6; i++)
      if (key[i] != 'x' && cnt[key[i]-'0'] == 1)
         key[i] = 'x' ;
   auto ckey = canon(key) ;
   if (cache.find(ckey) != cache.end())
      return cache[ckey] ;
   string k2, k3 ;
   if (!splitby(key, k2, k3))
      return 0 ;
   auto remcost = bestcost(k3, Mx, My, typ, cache, planmap) ;
   auto ck2 = canon(k2) ;
   if (algset.find(ck2) == algset.end())
      error("! can't find any good continuations") ;
   double r = INFCOST ;
   reductionwplan decomp ;
   for (auto netgp: algset[ck2]) {
      if (netgp->bad) continue ;
      auto t = netgp->cost(Mx, My, typ) ;
      auto rem2 = bestcost(netgp->key, Mx, My, typ, cache, planmap) + remcost ;
      if (t.cost + rem2 < r) {
         r = t.cost + rem2 ;
         decomp = t ;
      }
   }
   cache[ckey] = r ;
   planmap[ckey] = decomp ;
   return r ;
}

double bestcost(string key, cubesizetype Mx, cubesizetype My, char typ) {
   return bestcost(key, Mx, My, typ, bestcostcache, bestcostplan) ;
}

vector<reductionwplan> reductionplan ;

void explain(string key, const map<string,reductionwplan> &planmap,
             vector<reductionwplan> &rplan, ostream &out) {
   while (1) {
      string k2, k3 ;
      if (key == "012345") {
         out << "Finished with key " << key << "\n" ;
         return ;
      }
      if (!splitby(key, k2, k3)) {
         cerr << "Could not spit " << key << endl ;
         error("! could not split key") ;
      }
      auto ckey = canon(k2) ;
      if (planmap.find(ckey) == planmap.end())
         error("! internal error; no progression?") ;
      const auto *plan = &planmap.at(ckey) ;
      string newkey(6, 0) ;
      int found = 0 ;
      int m ;
      for (m=0; m<24; m++) {
         remapper remap ;
         for (int i=0; i<6; i++)
            newkey[rots[m][i]] = remap.get(plan->net->key[i]) ;
         int good = 1 ;
         int remapt[6] ;
         for (int i=0; i<6; i++)
            remapt[i] = -1 ;
         for (int i=0; i<6; i++)
            if (k2[i] != 'x') {
               if (newkey[i] == 'x')
                  good = 0 ;
               else {
                  if (remapt[k2[i]-'0'] < 0)
                     remapt[k2[i]-'0'] = k2[i] ;
                  else if (remapt[k2[i]-'0'] != k2[i])
                     good = 0 ;
               }
            } else {
               if (newkey[i] != 'x')
                  good = 0 ;
            }
         if (good) {
            found = 1 ;
            break ;
         }
      }
      if (!found) {
         cerr << "Trying to use " << plan->net->key << " for " << key << endl ;
         error("! could not find right rotation") ;
      }
      const char *mode = plan->net->optimal ? "optimal" : "greedy" ;
      if (plan->net->used_perms < 0)
         plan->net->plan() ;
      out << "Use " << newkey << " " << plan->net->filename << " [" << mode << "] cost " << plan->cost << " on " << key << " map " << m << " used " << plan->net->used_perms << "/" << plan->net->stringalgs.size() << "\n" ;
      netgen *f = plan->net ;
      if (m != 0)
         f = new netgen(f->key, f->stringalgs, f->filename, m, f->optimal) ;
      rplan.push_back({plan->cost, f, plan->plan}) ;
      string k4(6, 0) ;
      remapper remap ;
      for (int i=0; i<6; i++)
         if (newkey[i] == 'x')
            k4[i] = remap.get(key[i]) ;
         else
            k4[i] = remap.get(6+newkey[i]) ;
      swap(key, k4) ;
   }
}

void explain(string key) {
   explain(key, bestcostplan, reductionplan, cout) ;
}
