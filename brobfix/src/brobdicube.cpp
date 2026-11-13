#ifndef WRITEFRAMES
#define WRITEFRAMES (0)
#endif
#include <algorithm>
#include <array>
#include <assert.h>
#include <strings.h>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <random>
#include <map>
#include <queue>
#include <sstream>
#include <vector>
#include <dirent.h>
#include <sys/time.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>
#if WRITEFRAMES
#include "fpng.h"
#endif
using namespace std ;
using ll = long long ;
using lll = __int128_t ;
// In the following like, you can change ll to lll so you can run
// estimates up to about 10^34 or so (until something overflows).
// But this may hurt performance, so by default we leave it at ll.
using cubesizetype = ll ;

// The solver allocates and scans 2^w entries per strip per stage, so the strip
// width is exponential in both time and memory here.  MAXW in netgen.cpp does
// NOT protect us: it only bounds how far past lg(Mx) optw may reach, because
// clipping w below lg(Mx) would destroy the n^2/lg n move count.  So for a large
// enough cube optw will legitimately return a width no machine can allocate --
// fine while we are only estimating, fatal if we actually try to solve.  Call
// this at the allocation, not where w is chosen, so it cannot drift away from
// the thing it guards.  Only a new worst width warns, so it stays quiet after
// the first report.
static void warn_wide_strip(int w) {
   static std::atomic<int> worst{19} ;
   int prev = worst.load(std::memory_order_relaxed) ;
   while (w > prev &&
          !worst.compare_exchange_weak(prev, w, std::memory_order_relaxed))
      ;
   if (w > prev)
      std::cerr << "WARNING: strip width w=" << w << " needs " << (1LL << w)
                << " pattern slots per strip per stage" << std::endl ;
}

static cubesizetype atocsz(const char *s) {
   cubesizetype r = 0 ;
   int neg = (*s == '-') ; if (neg) s++ ;
   while (*s >= '0' && *s <= '9') r = r * 10 + (*s++ - '0') ;
   return neg ? -r : r ;
}
#include "planmask.h"
#include "xorbit.h"
#include "mv.h"
int showdetails = 0 ;
int alwaysinmem = 0 ;
ll writemoveinc ;
ll writemovetarget = 0 ;
ll runningmovecount = 0 ;
int fast = 1 ;
int readmoves, writemoves ;
ll randmoves = 0 ;
ll cancellations = 0 ;
ll blocks = 0 ;
ll wingmoves = 0 ; 
int extrapolate = 0 ;
int stats = 0 ;
double extrapolatemul = 1 ;
ll solvecount = 1 ;
cubesizetype N ;
cubesizetype phasewidth = -1 ;
int planonly = 0 ;
int useavg = 1 ;
int greedyonly = 0 ;
cubesizetype plan_range_start = 0, plan_range_end = 0 ;
string algsdir = "./algs3" ;
#include "util.h"
static double start ;
mutex io_mutex ;
static int nprob_threads = (int)thread::hardware_concurrency() ;
static int nprob_threads_set_by_user = 0 ;
static bool describealgorithms = false ;
double walltime() {
   struct timeval tv ;
   gettimeofday(&tv, 0) ;
   return tv.tv_sec + 0.000001 * tv.tv_usec ;
}
double duration() {
   double now = walltime() ;
   double r = now - start ;
   start = now ;
   return r ;
}
ostream& operator<<(ostream& os, __int128_t n) {
    if (n == 0) return os << "0";
    if (n < 0) {
        os << "-";
        n = -n;
    }
    string s;
    while (n > 0) {
        s += (char)('0' + (n % 10));
        n /= 10;
    }
    reverse(s.begin(), s.end());
    return os << s;
}
#include "threecube.h"
#include "xcube.h"
#include "cornersolver.h"
#include "wingsolver.h"
#include "netgen.h"
#if WRITEFRAMES
int frameseq = 0 ;
uchar *framebuf = 0 ;
ll framew, frameh ;
int facexoff[] = { 1, 0, 1, 2, 3, 1 } ;
int faceyoff[] = { 0, 1, 1, 1, 1, 2 } ;
/*               w    o     g    r    b    y  grey dark_orange*/
int colorr[] = { 255, 255,   0, 255,   0, 255,  128, 255} ;
int colorg[] = { 255, 165, 160,   0,   0, 255,  128, 128} ;
int colorb[] = { 255,   0,   0,   0, 255,   0,  128, 0} ;
const int S = 24 ;
void placepixel(int col, int x, int y) {
   int st = (S >= 5 ? 1 : 0) ;
   int r=colorr[col], g=colorg[col], b=colorb[col] ;
   for (int dy=st; dy<S; dy++) for (int dx=st; dx<S; dx++) {
      ll o = ((ll)(y*S+dy)*framew + (x*S+dx)) * 3 ;
      framebuf[o]=r; framebuf[o+1]=g; framebuf[o+2]=b;
   }
}
void writeframe(xcube &cube) {
   writemovetarget += writemoveinc ;
   if (cube.Mx + cube.My > 1000000000)
      error("! cube too large for frame output") ;
   int N = (int)(cube.Mx + cube.My) ;
   if (framebuf == 0) {
      framew = 4LL * (N+2) * S ;
      frameh = 3LL * (N+2) * S ;
      ll framemem = 3 * framew * frameh ;
      framebuf = (uchar *)calloc(1, framemem) ;
      if (framebuf == 0)
         error("! no memory for the frame buffer") ;
   }
   for (int i=0; i<cube.My; i++) {
      auto &row = cube.rows[i] ;
      for (int j=0; j<cube.Mx; j++) {
         auto &xo = row[j] ;
         for (int f=0; f<6; f++) {
            int fx = facexoff[f] * (N+2) +1 ;
            int fy = faceyoff[f] * (N+2) +1 ;
            placepixel(xo.a[4*f+((cube.facerots[f])&3)], fx+j, fy+i) ;
            placepixel(xo.a[4*f+((1+cube.facerots[f])&3)], fx+N-1-i, fy+j) ;
            placepixel(xo.a[4*f+((2+cube.facerots[f])&3)], fx+N-1-j, fy+N-1-i) ;
            placepixel(xo.a[4*f+((3+cube.facerots[f])&3)], fx+i, fy+N-1-j) ;
         }
      }
   }
   for (int f=0; f<6; f++) //###
      for (int i=1; i<=N; i++) { // skip corners
         if (i+i==N+1) // skip middle edges
            continue ;
         placepixel(cube.wing_color(0, f, i, N), facexoff[f]*(N+2)+i,        faceyoff[f]*(N+2)) ;
         placepixel(cube.wing_color(1, f, i, N), facexoff[f]*(N+2)+N+1,      faceyoff[f]*(N+2)+i) ;
         placepixel(cube.wing_color(2, f, i, N), facexoff[f]*(N+2)+(N+1-i),  faceyoff[f]*(N+2)+N+1) ;
         placepixel(cube.wing_color(3, f, i, N), facexoff[f]*(N+2),          faceyoff[f]*(N+2)+(N+1-i)) ;
      }
   auto &threecube = cube.tc ;
   for (int f=0; f<6; f++) {
      int fx = facexoff[f] * (N+2);
      int fy = faceyoff[f] * (N+2);
      placepixel(threecube.corner.a[4*f], fx, fy) ;
      placepixel(threecube.corner.a[4*f+1], fx+N+1, fy) ;
      placepixel(threecube.corner.a[4*f+2], fx+N+1, fy+N+1) ;
      placepixel(threecube.corner.a[4*f+3], fx, fy+N+1) ;
      if (N & 1) {
         int half = (N+1)/2 ;
         placepixel(threecube.edge.a[4*f], fx+half, fy) ;
         placepixel(threecube.edge.a[4*f+1], fx+N+1, fy+half) ;
         placepixel(threecube.edge.a[4*f+2], fx+half, fy+N+1) ;
         placepixel(threecube.edge.a[4*f+3], fx, fy+half) ;
         placepixel(threecube.centercenters[f], fx+half, fy+half) ;
      }
   }
   string framename = "frame-" ;
   cout << " " << frameseq << flush ;
   for (int dig=1000000; dig>=1; dig /= 10)
      framename.push_back((char)(frameseq / dig % 10 + '0')) ;
   framename += ".png" ;
   frameseq++ ;
   int t = fpng::fpng_encode_image_to_file(framename.c_str(), framebuf,
      framew, frameh, 3) ;
   if (t > 1) {
      cerr << t << endl ;
      error("! problem writing frame") ;
   }
}
#endif
// Strip-local rotation helpers for fill_strip_from_moveset.
// srows[0..pw-1][0..Mx-1]; fr[6] is current accumulated face rotation.
static void s_rot4hor(vector<vector<xorbit>> &srows, int f1, int o1, int f2, int o2,
                      int f3, int o3, int f4, int o4, int local_dep, const int fr[6]) {
   for (auto &orb : srows[local_dep]) {
      uchar &c1 = orb.a[4*f1+((o1+fr[f1])&3)] ;
      uchar &c2 = orb.a[4*f2+((o2+fr[f2])&3)] ;
      uchar &c3 = orb.a[4*f3+((o3+fr[f3])&3)] ;
      uchar &c4 = orb.a[4*f4+((o4+fr[f4])&3)] ;
      auto t = c1 ; c1 = c2 ; c2 = c3 ; c3 = c4 ; c4 = t ;
   }
}
static void s_rot2hor(vector<vector<xorbit>> &srows, int f1, int o1, int f2, int o2,
                      int f3, int o3, int f4, int o4, int local_dep, const int fr[6]) {
   for (auto &orb : srows[local_dep]) {
      uchar &c1 = orb.a[4*f1+((o1+fr[f1])&3)] ;
      uchar &c2 = orb.a[4*f2+((o2+fr[f2])&3)] ;
      uchar &c3 = orb.a[4*f3+((o3+fr[f3])&3)] ;
      uchar &c4 = orb.a[4*f4+((o4+fr[f4])&3)] ;
      swap(c1, c3) ;
      swap(c2, c4) ;
   }
}
static void s_rot4ver(vector<vector<xorbit>> &srows, int f1, int o1, int f2, int o2,
                      int f3, int o3, int f4, int o4, int dep, const int fr[6]) {
   for (auto &row : srows) {
      uchar &c1 = row[dep].a[4*f1+((o1+fr[f1])&3)] ;
      uchar &c2 = row[dep].a[4*f2+((o2+fr[f2])&3)] ;
      uchar &c3 = row[dep].a[4*f3+((o3+fr[f3])&3)] ;
      uchar &c4 = row[dep].a[4*f4+((o4+fr[f4])&3)] ;
      auto t = c1 ; c1 = c2 ; c2 = c3 ; c3 = c4 ; c4 = t ;
   }
}
static void s_rot2ver(vector<vector<xorbit>> &srows, int f1, int o1, int f2, int o2,
                      int f3, int o3, int f4, int o4, int dep, const int fr[6]) {
   for (auto &row : srows) {
      uchar &c1 = row[dep].a[4*f1+((o1+fr[f1])&3)] ;
      uchar &c2 = row[dep].a[4*f2+((o2+fr[f2])&3)] ;
      uchar &c3 = row[dep].a[4*f3+((o3+fr[f3])&3)] ;
      uchar &c4 = row[dep].a[4*f4+((o4+fr[f4])&3)] ;
      swap(c1, c3) ;
      swap(c2, c4) ;
   }
}

struct StripSolver {
   vector<mv> prevmvs ;
   ll cancellations = 0 ;
   ll block_count = 0 ;
   vector<vector<planmask>> movemasks ;
   vector<ll> bits ;
   vector<int> nxt, prv ;
   vector<int> xs, ys ;
   vector<reductionwplan> localplan ;
   mt19937_64 local_rng ;
   struct PhaseStat { ll hits = 0, moves = 0 ; double expectedcost = 0 ; } ;
   vector<PhaseStat> phase_stats ;

   StripSolver(ll phasewidth_, ll Mx_) : local_rng(random_device{}()) {
      movemasks.resize((size_t)phasewidth_) ;
      for (auto &mm : movemasks)
         mm.resize((size_t)Mx_) ;
      localplan = reductionplan ;
   }

   void fill_row(xcube &cube, ll row) {
      cube.rows[(size_t)row].resize((size_t)cube.Mx) ;
      for (auto &orb : cube.rows[(size_t)row]) {
         oclear(orb) ;
         shuffle(orb.a, orb.a+24, local_rng) ;
      }
   }

   // Apply one pre-normalized move to the strip-local srows and fr.
   // dep < My_: normal slice (hor if in strip, ver across all strip rows).
   // dep >= My_: middle slice (ver only, no ccs).
   // dep == -1 (face move): update fr only.
   static void apply_strip_move(const mv &smv, int fr[6],
                                vector<vector<xorbit>> &srows,
                                ll strip_start, ll pw, ll My_) {
      if (smv.dep == 1) {
         fr[smv.face] = (fr[smv.face] + 4 - smv.twist) & 3 ;
         return ;
      }
      int dep = smv.dep - 2 ;
      int local_dep = dep - (int)strip_start ;
#define SH(fn,a,b,c,d,e,f,g,h) if(local_dep>=0&&local_dep<(int)pw) s_##fn##hor(srows,a,b,c,d,e,f,g,h,local_dep,fr)
#define SV(fn,a,b,c,d,e,f,g,h) s_##fn##ver(srows,a,b,c,d,e,f,g,h,dep,fr)
#define SV2(fn,a,b,c,d,e,f,g,h) s_##fn##ver(srows,a,b,c,d,e,f,g,h,dep,fr);s_##fn##ver(srows,a,b^2,c,d^2,e,f^2,g,h^2,dep,fr)
      if (dep >= (int)My_) {
         switch (smv.face * 3 + smv.twist) {
case 1:  SV2(rot4,1,1,2,1,3,1,4,1); break;
case 2:  SV2(rot2,1,1,2,1,3,1,4,1); break;
case 3:  SV2(rot4,4,1,3,1,2,1,1,1); break;
case 4:  SV2(rot4,2,0,0,0,4,2,5,0); break;
case 5:  SV2(rot2,2,0,5,0,4,2,0,0); break;
case 6:  SV2(rot4,2,0,5,0,4,2,0,0); break;
case 7:  SV2(rot4,0,3,1,2,5,1,3,0); break;
case 8:  SV2(rot2,0,3,3,0,5,1,1,2); break;
case 9:  SV2(rot4,0,3,3,0,5,1,1,2); break;
case 10: SV2(rot4,2,2,5,2,4,0,0,2); break;
case 11: SV2(rot2,2,2,5,2,4,0,0,2); break;
case 12: SV2(rot4,2,2,0,2,4,0,5,2); break;
case 13: SV2(rot4,0,1,3,2,5,3,1,0); break;
case 14: SV2(rot2,0,1,3,2,5,3,1,0); break;
case 15: SV2(rot4,0,1,1,0,5,3,3,2); break;
case 16: SV2(rot4,4,3,3,3,2,3,1,3); break;
case 17: SV2(rot2,1,3,2,3,3,3,4,3); break;
case 18: SV2(rot4,4,3,1,3,2,3,3,3); break;
         }
      } else {
         switch (smv.face * 3 + smv.twist) {
case 1:  SH(rot4,1,0,2,0,3,0,4,0); SV(rot4,1,1,2,1,3,1,4,1); break;
case 2:  SH(rot2,1,0,2,0,3,0,4,0); SV(rot2,1,1,2,1,3,1,4,1); break;
case 3:  SH(rot4,4,0,3,0,2,0,1,0); SV(rot4,4,1,3,1,2,1,1,1); break;
case 4:  SH(rot4,2,3,0,3,4,1,5,3); SV(rot4,2,0,0,0,4,2,5,0); break;
case 5:  SH(rot2,2,3,5,3,4,1,0,3); SV(rot2,2,0,5,0,4,2,0,0); break;
case 6:  SH(rot4,2,3,5,3,4,1,0,3); SV(rot4,2,0,5,0,4,2,0,0); break;
case 7:  SH(rot4,0,2,1,1,5,0,3,3); SV(rot4,0,3,1,2,5,1,3,0); break;
case 8:  SH(rot2,0,2,3,3,5,0,1,1); SV(rot2,0,3,3,0,5,1,1,2); break;
case 9:  SH(rot4,0,2,3,3,5,0,1,1); SV(rot4,0,3,3,0,5,1,1,2); break;
case 10: SH(rot4,2,1,5,1,4,3,0,1); SV(rot4,2,2,5,2,4,0,0,2); break;
case 11: SH(rot2,2,1,5,1,4,3,0,1); SV(rot2,2,2,5,2,4,0,0,2); break;
case 12: SH(rot4,2,1,0,1,4,3,5,1); SV(rot4,2,2,0,2,4,0,5,2); break;
case 13: SH(rot4,0,0,3,1,5,2,1,3); SV(rot4,0,1,3,2,5,3,1,0); break;
case 14: SH(rot2,0,0,3,1,5,2,1,3); SV(rot2,0,1,3,2,5,3,1,0); break;
case 15: SH(rot4,0,0,1,3,5,2,3,1); SV(rot4,0,1,1,0,5,3,3,2); break;
case 16: SH(rot4,4,2,3,2,2,2,1,2); SV(rot4,4,3,3,3,2,3,1,3); break;
case 17: SH(rot2,1,2,2,2,3,2,4,2); SV(rot2,1,3,2,3,3,3,4,3); break;
case 18: SH(rot4,4,2,1,2,2,2,3,2); SV(rot4,4,3,1,3,2,3,3,3); break;
         }
      }
#undef SH
#undef SV
#undef SV2
   }

   // dosolve convention: std::rotate (LEFT rotate by fr[f])
   static void apply_fr_dosolve(vector<vector<xorbit>> &srows, int fr[6]) {
      for (auto &row : srows)
         for (auto &orb : row)
            for (int f = 0; f < 6; f++)
               if (fr[f] & 3)
                  rotate(orb.a + 4*f, orb.a + 4*f + (fr[f] & 3), orb.a + 4*f + 4) ;
      for (int f = 0; f < 6; f++) fr[f] = 0 ;
   }

   // normalizefacerots convention: rot4/rot2/rot4i (RIGHT rotate by fr[f])
   static void apply_fr_normfr(vector<vector<xorbit>> &srows, int fr[6]) {
      for (auto &row : srows)
         for (auto &orb : row)
            for (int f = 0; f < 6; f++) {
               switch (fr[f] & 3) {
               case 0: break ;
               case 1: orb.rot4i(4*f, 4*f+1, 4*f+2, 4*f+3) ; break ;
               case 2: orb.rot2(4*f, 4*f+1, 4*f+2, 4*f+3) ; break ;
               case 3: orb.rot4(4*f, 4*f+1, 4*f+2, 4*f+3) ; break ;
               }
            }
      for (int f = 0; f < 6; f++) fr[f] = 0 ;
   }

   // Replay pre-normalized movesets into thread-local srows, then move to cube.rows.
   // Uses separate fr arrays per phase to match the serial path's two-step convention:
   //   - orig_mvs + dosolve: std::rotate(fr_orig)
   //   - prep_before + normalizefacerots: rot4/rot2/rot4i(fr_corner)
   //   - prep_after: wing cycles (zero net face rotation, so no final apply needed)
   void fill_strip_from_moveset(xcube &cube, ll strip_start, ll pw,
                                const vector<mv> &orig_mvs,
                                const vector<mv> &prep_before,
                                const vector<mv> &prep_after) {
      vector<vector<xorbit>> srows((size_t)pw,
                                   vector<xorbit>((size_t)cube.Mx)) ;
      for (auto &row : srows)
         for (auto &orb : row)
            oclear(orb) ;
      // Phase 1: original moveset (dosolve convention)
      int fr_orig[6] = {0,0,0,0,0,0} ;
      for (const auto &smv : orig_mvs)
         apply_strip_move(smv, fr_orig, srows, strip_start, pw, cube.My) ;
      apply_fr_dosolve(srows, fr_orig) ;  // matches dosolve's std::rotate
      // Phase 2: corner-fix moves (normalizefacerots convention)
      int fr_corner[6] = {0,0,0,0,0,0} ;
      for (const auto &smv : prep_before)
         apply_strip_move(smv, fr_corner, srows, strip_start, pw, cube.My) ;
      apply_fr_normfr(srows, fr_corner) ;  // matches normalizefacerots's rot4/rot2/rot4i
      // Phase 3: parity fix + wing cycles (net zero face rotation, no final apply)
      int fr_after[6] = {0,0,0,0,0,0} ;
      for (const auto &smv : prep_after)
         apply_strip_move(smv, fr_after, srows, strip_start, pw, cube.My) ;
      // fr_after should be 0 here (wing algos are balanced); apply anyway to be safe
      apply_fr_dosolve(srows, fr_after) ;
      for (ll wi = 0; wi < pw; wi++)
         cube.rows[(size_t)(strip_start + wi)] = std::move(srows[(size_t)wi]) ;
   }

   void release_row(xcube &cube, ll row) {
      cube.checkrow((int)row) ;
      vector<xorbit> empty ;
      swap(empty, cube.rows[(size_t)row]) ;
   }

   void trycancellation_local(mv &m) {
      if (prevmvs.empty()) {
         prevmvs.push_back(m) ;
         return ;
      }
      if (m.dep > 1) {
         prevmvs.clear() ;
         return ;
      }
      mv merged ;
      int r = trymerge(prevmvs.back(), m, merged) ;
      if (r == 2)
         prevmvs.push_back(m) ;
      else if (r == 1) {
         cancellations++ ;
         prevmvs.back() = merged ;
      } else {
         cancellations += 2 ;
         prevmvs.pop_back() ;
      }
   }

   ll doblock_fast(blockalgo &alg) {
      ll oldcancellations = cancellations ;
      block_count++ ;
      for (int i = 0; i < alg.c; i++)
         trycancellation_local(alg.mvs[i]) ;
      prevmvs.clear() ;
      for (int i = alg.c - 1; i >= 0; i--)
         trycancellation_local(alg.mvs[alg.mvs.size() - 1 - i]) ;
      return alg.o + alg.v * (ll)xs.size() + alg.h * (ll)ys.size() + alg.c -
             (cancellations - oldcancellations) ;
   }

   ll solve_strip(xcube &cube, ll strip_start, ll pw,
                  const vector<mv> *norm_mvs = nullptr,
                  const vector<mv> *prep_before = nullptr,
                  const vector<mv> *prep_after = nullptr) {
      ll r = 0 ;
      if (!alwaysinmem) {
         if (norm_mvs)
            fill_strip_from_moveset(cube, strip_start, pw,
                                    *norm_mvs, *prep_before, *prep_after) ;
         else
            for (ll wi = 0; wi < pw; wi++)
               fill_row(cube, strip_start + wi) ;
      }
      int phasebase = 0 ;
      for (auto &f : localplan) {
         for (ll ii = 0; ii < pw; ii++)
            for (ll j = 0; j < (ll)cube.Mx; j++) {
               movemasks[(size_t)ii][(size_t)j] = f.net->getplan(cube.rows[strip_start + ii][j]) ;
               f.net->doplan(movemasks[(size_t)ii][(size_t)j], cube.rows[strip_start + ii][j]) ;
            }
         for (int pi = f.net->STAGE - 1; pi >= 0; pi--, phasebase++) {
            auto &alg = f.net->algs[pi] ;
            auto &wplan = f.plan[pi] ;
            wplan.plan((cubesizetype)pw) ;
            if (stats) {
               if (phasebase >= (int)phase_stats.size())
                  phase_stats.resize(phasebase + 1) ;
               phase_stats[phasebase].expectedcost += wplan.cost ;
            }
            ll ii = 0 ;
            for (ll iii = 0; iii < (ll)(wplan.count1 + wplan.count2); iii++) {
               int w = iii < wplan.count1 ? wplan.w1 : wplan.w2 ;
               bits.assign((size_t)cube.Mx, 0) ;
               for (int wi = 0; wi < w; wi++)
                  for (ll j = 0; j < (ll)cube.Mx; j++)
                     if (movemasks[(size_t)(ii + wi)][(size_t)j].test(pi))
                        bits[(size_t)j] |= 1LL << wi ;
               warn_wide_strip(w) ;
               prv.assign(1LL << w, -1) ;
               nxt.resize((size_t)cube.Mx) ;
               for (ll i = 0; i < (ll)cube.Mx; i++) {
                  nxt[(size_t)i] = prv[(size_t)bits[(size_t)i]] ;
                  prv[(size_t)bits[(size_t)i]] = (int)i ;
               }
               for (ll bp = 1; bp < (1LL << w); bp++) {
                  if (prv[bp] < 0)
                     continue ;
                  ys.clear() ;
                  for (int wi = 0; wi < w; wi++)
                     if ((bp >> wi) & 1)
                        ys.push_back((int)(strip_start + ii + wi)) ;
                  xs.clear() ;
                  for (int i = prv[bp]; i >= 0; i = nxt[i])
                     xs.push_back(i) ;
                  ll mvs = doblock_fast(alg) ;
                  if (stats) {
                     phase_stats[phasebase].hits += xs.size() * ys.size() ;
                     phase_stats[phasebase].moves += mvs ;
                  }
                  r += mvs ;
               }
               ii += w ;
            }
         }
      }
      for (ll wi = 0; wi < pw; wi++)
         cube.checkrelease(strip_start + wi) ;
      return r ;
   }
} ;
ll solve(xcube &cube, cubesizetype phasewidth) {
   if (phasewidth > cube.My)
      phasewidth = cube.My ;
   ll r = 0 ;
   // For streaming parallel solve, capture all prep moves so each strip can replay them.
   const bool streaming = (!alwaysinmem && readmoves) ;
   vector<mv> prep_before_norm, prep_after_norm ;
   // g_prep_recorder is always cleared to nullptr (line below) before these locals go out of scope.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdangling-pointer"
#endif
   if (streaming) g_prep_recorder = &prep_before_norm ;
   solvecorners(cube) ;
   cube.normalizefacerots() ;
   if (streaming) g_prep_recorder = &prep_after_norm ;
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
   wingmoves = 0 ;
   bool dowings = !extrapolate ; // previously had alwaysinmem condition
   // Wing parity fix
   if (dowings){
      for (ll k=0; k<cube.My; k++) {
         xorbit p = cube.wingorbits[k] ;
         int par = 0 ;
         for (int i=0; i<24; i++)
            while (p.a[i] != i) {
               swap(p.a[i], p.a[p.a[i]]) ; // this flips through the array to check its parity
               par = !par ;
            }
         if (par) {
            mv pm = { (int)(k+2), 0, 1 } ;       // quarter-turn U slice at depth k+2
            if (writemoves)
               curblock.push_back(pm) ;
            domove(pm, cube) ;
            r++ ;
            wingmoves++ ;
            runningmovecount++ ;
         }
      }
   }
   if (dowings){
      r += do_wing_cycles(cube) ;
   }
   if (streaming) g_prep_recorder = nullptr ;
   if (fast && nprob_threads > 1) {
      for (auto &f : reductionplan)
         f.net->plan() ;
      // Pre-normalize moveset for streaming replay (readmoves + parallel only)
      vector<mv> norm_moveset ;
      if (!alwaysinmem && readmoves) {
         norm_moveset = moveset ;
         for (auto &smv : norm_moveset) {
            if (smv.dep > cube.Mx + cube.My + 3 - smv.dep) {
               smv.dep = cube.Mx + cube.My + 3 - smv.dep ;
               smv.face = oppface[smv.face] ;
               smv.twist = (4 - smv.twist) & 3 ;
            }
         }
      }
      ll num_strips = ((ll)cube.My + (ll)phasewidth - 1) / (ll)phasewidth ;
      atomic<ll> next_si{0} ;
      atomic<ll> total_mvs{0} ;
      atomic<ll> total_cancellations{0} ;
      atomic<ll> total_blocks{0} ;
      if (stats) {
         int phasebase = 0 ;
         for (auto &f : reductionplan) {
            for (int pi = f.net->STAGE-1; pi >= 0; pi--, phasebase++) {
               auto &wp = f.plan[pi] ;
               wp.plan((cubesizetype)phasewidth) ;
               if (phasebase >= (int)byphase.size()) {
                  byphase.resize(phasebase+1) ;
                  byphase[phasebase].w1 = wp.w1 ;
                  byphase[phasebase].w2 = wp.w2 ;
                  byphase[phasebase].count1 = wp.count1 ;
                  byphase[phasebase].count2 = wp.count2 ;
               }
               byphase[phasebase].ba = &(f.net->algs[pi]) ;
            }
         }
      }
      // non-null only for readmoves streaming; random mode uses fill_row (null)
      const vector<mv> *norm_mvs_ptr  = streaming ? &norm_moveset    : nullptr ;
      const vector<mv> *prep_bef_ptr  = streaming ? &prep_before_norm : nullptr ;
      const vector<mv> *prep_aft_ptr  = streaming ? &prep_after_norm  : nullptr ;
      mutex stats_mu ;
      auto worker = [&]() {
         StripSolver ss((ll)phasewidth, cube.Mx) ;
         for (;;) {
            ll si = next_si.fetch_add(1) ;
            if (si >= num_strips) break ;
            ll strip_start = si * (ll)phasewidth ;
            ll pw = (ll)phasewidth ;
            if (strip_start + pw > (ll)cube.My)
               pw = (ll)cube.My - strip_start ;
            total_mvs.fetch_add(ss.solve_strip(cube, strip_start, pw,
                                               norm_mvs_ptr, prep_bef_ptr, prep_aft_ptr)) ;
         }
         total_cancellations.fetch_add(ss.cancellations) ;
         total_blocks.fetch_add(ss.block_count) ;
         if (stats) {
            lock_guard<mutex> lk(stats_mu) ;
            for (int pb = 0; pb < (int)ss.phase_stats.size(); pb++) {
               byphase[pb].hits += ss.phase_stats[pb].hits ;
               byphase[pb].moves += ss.phase_stats[pb].moves ;
               byphase[pb].expectedcost += ss.phase_stats[pb].expectedcost ;
            }
         }
      } ;
      vector<thread> threads ;
      for (int t = 0; t < nprob_threads; t++)
         threads.emplace_back(worker) ;
      for (auto &t : threads)
         t.join() ;
      r += total_mvs.load() ;
      cancellations += total_cancellations.load() ;
      blocks += total_blocks.load() ;
   } else {
      vector<int> xs ;
      vector<int> ys ;
      vector<ll> bits ;
      vector<int> next, prev ;
      vector<vector<planmask>> movemasks((size_t)phasewidth) ;
      for (ll i=0; i<(ll)phasewidth; i++)
         movemasks[(size_t)i].resize((size_t)cube.Mx) ;
      for (ll i=0; i<(ll)cube.My; i += (ll)phasewidth) {
         ll pw = (ll)phasewidth ;
         if (i + pw > (ll)cube.My)
            pw = (ll)cube.My - i ;
         for (ll wi=0; wi<pw; wi++)
            cube.getrow((ll)(i+wi)) ;
         int phasebase = 0 ;
         for (auto &f: reductionplan) {
            for (ll ii=0; ii<pw; ii++)
               for (ll j=0; j<(ll)cube.Mx; j++) {
                  movemasks[ii][j] = f.net->getplan(cube.rows[i+ii][j]) ;
                  if (fast && !writemoveinc)
                     f.net->doplan(movemasks[ii][j], cube.rows[i+ii][j]) ;
               }
            for (int pi=f.net->STAGE-1; pi>=0; pi--, phasebase++) {
               auto &alg = f.net->algs[pi] ;
               auto &wplan = f.plan[pi] ;
               wplan.plan((cubesizetype)pw) ;
               if (phasebase >= (int)byphase.size()) {
                  byphase.resize(phasebase+1) ;
                  byphase[phasebase].w1 = wplan.w1 ;
                  byphase[phasebase].w2 = wplan.w2 ;
                  byphase[phasebase].count1 = wplan.count1 ;
                  byphase[phasebase].count2 = wplan.count2 ;
               }
               byphase[phasebase].expectedcost += wplan.cost ;
               byphase[phasebase].ba = &(f.net->algs[pi]) ;
               if (showdetails) {
                  cout << "Alg " << phasebase ;
                  alg.show() ;
                  cout << endl ;
               }
               ll ii = 0 ;
               for (ll iii=0; iii<(ll)(wplan.count1 + wplan.count2); iii++) {
                  int w = iii < wplan.count1 ? wplan.w1 : wplan.w2 ;
                  bits.clear() ;
                  bits.resize((size_t)cube.Mx) ;
                  for (int wi=0; wi<w; wi++)
                     for (ll j=0; j<(ll)cube.Mx; j++)
                        if (movemasks[(size_t)(ii+wi)][(size_t)j].test(pi))
                           bits[(size_t)j] |= 1LL<<wi ;
                  prev.clear() ;
                  warn_wide_strip(w) ;
                  prev.resize(1LL<<w, -1) ;
                  next.resize((size_t)cube.Mx) ;
                  for (ll i=0; i<(ll)cube.Mx; i++) {
                     next[(size_t)i] = prev[bits[(size_t)i]] ;
                     prev[bits[(size_t)i]] = (int)i ;
                  }
                  for (ll bp=1; bp<(1LL<<w); bp++) {
                     if (prev[bp] < 0)
                        continue ;
                     ys.clear() ;
                     for (int wi=0; wi<w; wi++)
                        if ((bp >> wi) & 1)
                           ys.push_back(i+ii+wi) ;
                     xs.clear() ;
                     for (int i=prev[bp]; i>=0; i=next[i])
                        xs.push_back(i) ;
                     byphase[phasebase].hits += xs.size() * ys.size() ;
                     ll mvs = doblock(cube, alg, ys, xs) ;
                     if (writemoveinc && fast)
                        for (auto y: ys)
                           for (auto x: xs)
                              f.net->doplan(planmask{}.set(pi), cube.rows[y][x]) ;
                     byphase[phasebase].moves += mvs ;
                     r += mvs ;
                  }
                  ii += w ;
               }
            }
         }

         for (ll wi=0; wi<pw; wi++)
            cube.checkrelease((ll)(i+wi)) ;
         if (extrapolate)
            break ;
      }
   }
   if (dowings)
      for (ll k=0; k<cube.My; k++) {
         vector<int> w = cube.get_wings(k)[0] ;
         for (int t=0; t<24; t++)
            if (w[t] != t) {
               cerr << "wings NOT solved: orbit " << k << " slot " << t << endl ;
               exit(1) ;
            }
      }
   return r ;
}
ll dosolve(cubesizetype N, cubesizetype phasewidth) {
   prevmvs.clear() ;
   cubesizetype My = (N - 2) >> 1 ;
   cubesizetype Mx = (N - 2) - My ;
   blocks = 0 ;
   cancellations = 0 ;
   xcube cube(Mx, My, (readmoves || randmoves) ? 'I' : 'R') ;
   int fr[6] ; 
   for (int f=0; f<6; f++) { 
      fr[f] = cube.facerots[f] ; 
      cube.facerots[f] = 0 ; 
   }
   for (auto &row : cube.rows)
      for (auto &orb : row)
         for (int f=0; f<6; f++)
            rotate(orb.a + 4*f, orb.a + 4*f + (fr[f] & 3), orb.a + 4*f + 4) ;
   //cout << "=== wing orbits ===" << endl ;
   //cube.show_wings() ;
   //cout << "===================" << endl ;
#if WRITEFRAMES
   runningmovecount = 0 ;
   if (writemoveinc)
      writeframe(cube) ;
#endif
   ll r = solve(cube, phasewidth) ;
   sendblock() ; // flush out any remaining moves
   if (alwaysinmem)
      for (int i=0; i<cube.My; i++)
         cube.checkrow(i) ;
#if WRITEFRAMES
   if (writemoveinc) {
      writeframe(cube) ;
      cout << endl ;
   }
#endif
   return r ;
}
struct basicstat {
   basicstat(const char *n_) : name(n_), n(0), sx(0), sxx(0), hi(0), lo(0) {}
   void add(double x) {
      x *= extrapolatemul ;
      n++ ;
      if (n == 1)
         hi = lo = x ;
      sx += x ;
      sxx += x * x ;
      hi = max(hi, x) ;
      lo = min(lo, x) ;
   }
   void report() {
      double av = sx / n ;
      double stddev = sqrt(n * sxx - sx * sx) / n ;
      cout << name << " " << av << " stddev " << stddev << " range " << lo << " .. " << hi ;
   }
   void avreport() {
      double av = sx / n ;
      cout << name << " " << av ;
   }
   const char *name ;
   ll n ;
   double sx, sxx, hi, lo ;
} ;
basicstat mvstat("moves"), cancelstat("cancels"), blockstat("blocks") ;
void fullreport(ll at) {
   cout << "N " << N << " phasewidth " << phasewidth << " cnt " << at << " " ;
   mvstat.report() ; cout << " " ;
   cancelstat.avreport() ; cout << " " ;
   blockstat.avreport() ; cout << endl;
}
void addcounts(ll v) {
   mvstat.add(v)  ;
   cancelstat.add(cancellations) ;
   blockstat.add(blocks) ;
   if (solvecount > 1 && (mvstat.n & (mvstat.n - 1)) == 0 && mvstat.n != solvecount)
      fullreport(mvstat.n) ;
}
void usage() {
   cout << "Brobdicube (C) 2026 Tomas Rokicki." << endl <<
      endl <<
      "Usage:  brobdicube [options] size" << endl <<
      endl <<
      "Options:" << endl <<
      endl <<
      "-d      show some internal information on the strategy." << endl <<
      "-i      read scramble moves from stdin." << endl <<
      "-e      plan and estimate but do not solve." << endl <<
      "-r n    scramble with n random moves (printed as SCRAMBLE: ...)." << endl <<
      "-R      scramble with an auto-picked length (50*N) random moves." << endl <<
      "-M n    do this many solves and report aggregate stats." << endl <<
      "-o      write solution to stdout (on lines starting with a space)." << endl <<
      "-p n    set the phase chunking width." << endl <<
      "-s      actually (slowly) simulate moves; for testing." << endl <<
      "-x      extrapolate statistics from a single strip." << endl <<
      "-a x y  select first phase of length x and second phase of length y." << endl << 
      "-S      show stats for each phase separately." << endl <<
      "-g      use only greedy planning (skip optimal)." << endl <<
      "-show_wings print the edge permutations of the scrambled cube." << endl << 
#if WRITEFRAMES
      "-v n    write a png file of the cube state every n moves." << endl <<
#endif
      "-w      optimize plan for worst-case, not average, solves." << endl <<
      "-D dir  set the algs directory (default: ./algs3)." << endl <<
      "-t n    use n threads for probability cache precomputation (default: hardware_concurrency)." << endl <<
      "--describealgorithms" << endl <<
      "        for each loaded algorithm file, print (greedy and optimal separately):" << endl <<
      "        strategy, filename, depth (non-zero-prob algs), avg activations," << endl <<
      "        max activations, total cost (v/h/o/c 4-digit fields)," << endl <<
      "        and max single-alg cost.  No cube size required." << endl <<
      endl ;
   exit(0) ;
}
static string fmt_cost(ll cost) {
   ll v = cost / 1000000000000LL ;
   ll h = cost / 100000000LL % 10000LL ;
   ll o = cost / 10000LL % 10000LL ;
   ll c = cost % 10000LL ;
   char buf[32] ;
   snprintf(buf, sizeof(buf), "%04lld/%04lld/%04lld/%04lld", v, h, o, c) ;
   return buf ;
}
void describe_algorithms() {
   // Carry canonical key alongside each netgen so it can appear in output.
   vector<pair<string,netgen*>> all_ng ;
   for (auto& [ckey, gens] : algset)
      for (auto* ng : gens)
         all_ng.emplace_back(ckey, ng) ;
   sort(all_ng.begin(), all_ng.end(), [](const auto& a, const auto& b) {
      if (a.second->filename != b.second->filename)
         return a.second->filename < b.second->filename ;
      return (int)a.second->optimal < (int)b.second->optimal ; // greedy first
   }) ;
   int total = (int)all_ng.size() ;
   if (total == 0) return ;
   int nthreads = nprob_threads > 0 ? nprob_threads :
                  (int)thread::hardware_concurrency() ;
   if (nthreads < 1) nthreads = 1 ;
   if (nthreads > total) nthreads = total ;
   atomic<int> next_idx{0} ;
   auto worker = [&]() {
      for (;;) {
         int idx = next_idx.fetch_add(1, memory_order_relaxed) ;
         if (idx >= total) break ;
         auto& [canonical_key, ng] = all_ng[idx] ;
         if (ng->max_activations_seen < 0)
            ng->ensureprobs() ;
         double avg_act = 0 ;
         ll total_cost = 0, max_cost = 0 ;
         int depth = 0 ;
         for (int i = 0; i < (int)ng->algcosts.size(); i++) {
            if (ng->stageprob[i] > 0) {
               avg_act += ng->stageprob[i] ;
               depth++ ;
               total_cost += ng->algcosts[i] ;
               if (ng->algcosts[i] > max_cost)
                  max_cost = ng->algcosts[i] ;
            }
         }
         string mode = ng->optimal ? "optimal" : "greedy" ;
         lock_guard<mutex> lk(io_mutex) ;
         cout << mode
              << " key=" << ng->key
              << " file=" << ng->filename
              << " depth=" << depth
              << " avg_act=" << avg_act
              << " max_act=" << ng->max_activations_seen
              << " total_cost=" << fmt_cost(total_cost)
              << " max_alg_cost=" << fmt_cost(max_cost)
              << "\n" ;
      }
   } ;
   vector<thread> threads ;
   threads.reserve(nthreads) ;
   for (int i = 0; i < nthreads; i++)
      threads.emplace_back(worker) ;
   for (auto& t : threads)
      t.join() ;
}
void preload_probs() {
   vector<netgen*> all_ng ;
   for (auto& [key, gens] : algset)
      for (auto* ng : gens)
         all_ng.push_back(ng) ;
   int total = (int)all_ng.size() ;
   if (total == 0) return ;
   // Pre-scan: count netgens that need a cache computed (large enough to cache,
   // but cache file not yet present).  Only those will generate output.
   int need_compute = 0 ;
   for (auto* ng : all_ng) {
      if (ng->goal <= netgen::MINWRITE) continue ;
      FILE* f = ng->openprobcache("r") ;
      if (f) fclose(f) ; else need_compute++ ;
   }
   int nthreads = nprob_threads > 0 ? nprob_threads :
                  (int)thread::hardware_concurrency() ;
   if (nthreads < 1) nthreads = 1 ;
   if (nthreads > total) nthreads = total ;
   if (need_compute > 0)
      cout << "Pre-computing probabilities for " << need_compute
           << " algorithm sets using " << nthreads << " threads" << endl ;
   atomic<int> next_idx{0} ;
   atomic<int> computed{0} ;
   auto worker = [&]() {
      for (;;) {
         int idx = next_idx.fetch_add(1, memory_order_relaxed) ;
         if (idx >= total) break ;
         bool was_computed = all_ng[idx]->ensureprobs() ;
         if (was_computed && all_ng[idx]->goal > netgen::MINWRITE) {
            int done = computed.fetch_add(1, memory_order_relaxed) + 1 ;
            lock_guard<mutex> lk(io_mutex) ;
            cout << "  probabilities: " << done << "/" << need_compute << " done" << endl ;
         }
      }
   } ;
   vector<thread> threads ;
   threads.reserve(nthreads) ;
   for (int i = 0; i < nthreads; i++)
      threads.emplace_back(worker) ;
   for (auto& t : threads)
      t.join() ;
   int bad_count = 0 ;
   for (auto ng : all_ng)
      if (ng->bad) bad_count++ ;
   if (bad_count > 0)
      cerr << "WARNING: " << bad_count << " inconsistent algorithm file(s) skipped." << endl ;
   if (need_compute > 0)
      cout << "All probability caches ready in " << duration() << endl ;
}
int main(int argc, char *argv[]) {
   duration() ;
   cout << "-" ;
   for (int i=0; i<argc; i++)
      cout << " " << argv[i] ;
   cout << endl ;
   while (argc > 1 && argv[1][0] == '-') {
      argc-- ;
      argv++ ;
      switch (argv[0][1]) {
case 'x':
         extrapolate = 1 ;
         break ;
case 'e':
         planonly = 1 ;
         break ;
case 's':
         fast = 0 ;
         break ;
case 'S':
         stats = 1 ;
         break ;
case 'g':
         greedyonly = 1 ;
         break ;
case 'd':
         showdetails = 1 ;
         break ;
case 'w':
         useavg = 0 ;
         break ;
case 'p':
         argc-- ;
         argv++ ;
         phasewidth = atocsz(argv[0]) ;
         break ;
case 'M':
         argc-- ;
         argv++ ;
         solvecount = atoll(argv[0]) ;
         break ;
#if WRITEFRAMES
case 'v':
         argc-- ;
         argv++ ;
         writemoveinc = atoll(argv[0]) ;
         break ;
#endif
case 'o':
         writemoves = 1 ;
         break ;
case 'i':
         readmoves = 1 ;
         break ;
case 'r':
         argc-- ;
         argv++ ;
         randmoves = atoll(argv[0]) ;
         break ;
case 'R':
         randmoves = -1 ;   // auto-pick length
         break ;
case 'P':
         argc-- ;
         argv++ ;
         plan_range_start = atocsz(argv[0]) ;
         argc-- ;
         argv++ ;
         plan_range_end = atocsz(argv[0]) ;
         break ;
case 'D':
         argc-- ;
         argv++ ;
         algsdir = argv[0] ;
         break ;
case 't':
         argc-- ;
         argv++ ;
         nprob_threads = atoi(argv[0]) ;
         nprob_threads_set_by_user = 1 ;
         break ;
default:
         if (strcmp(argv[0], "--describealgorithms") == 0) {
            describealgorithms = true ;
            break ;
         }
         error("! option not understood") ;
      }
   }
   if (writemoves && nprob_threads > 1) {
      if (nprob_threads_set_by_user)
         cerr << "WARNING: -o (emit moves) requires single-threaded solving; "
                 "ignoring -t " << nprob_threads << " and using 1 thread." << endl ;
      nprob_threads = 1 ;
   }
   if (!fast)
      nprob_threads = 1 ;
   expandrotations() ;
   build_wing_cycle_index() ;
   loadwingalgs() ;
   if (!fast && extrapolate)
      error("! cannot go slow and extrapolate at the same time") ;
   if (plan_range_start == 0 && !describealgorithms) {
      if (argc < 2)
         usage() ;
      if (argc > 2)
         error("! extra arguments given on command line") ;
      N = atocsz(argv[1]) ;
      if (randmoves < 0) // -1 means auto-pick a length of scramble
         randmoves = 50 * N ;
   }
   cout << setprecision(15) ;
   readalgs() ;
   preload_probs() ;
   if (describealgorithms) {
      describe_algorithms() ;
      return 0 ;
   }
   if (plan_range_start > 0) {
      const char *costname = useavg ? "average" : "worst-case" ;
      vector<cubesizetype> sizes ;
      for (cubesizetype sz = plan_range_start; sz <= plan_range_end; sz *= 2)
         sizes.push_back(sz) ;
      int ntasks = (int)sizes.size() ;
      vector<string> outbufs(ntasks) ;
      vector<bool> ready(ntasks, false) ;
      int next_to_print = 0 ;
      mutex result_mu ;
      auto worker = [&](int idx) {
         cubesizetype sz = sizes[idx] ;
         cubesizetype My = (sz - 2) / 2 ;
         cubesizetype Mx = (sz - 2) - My ;
         cubesizetype pw = (phasewidth > 0) ? phasewidth : My ;
         map<string,double> cache ;
         map<string,reductionwplan> planmap ;
         vector<reductionwplan> rplan ;
         ostringstream oss ;
         oss << setprecision(15) ;
         double cc = bestcost("000000", Mx, pw, useavg ? 'a' : 'w', cache, planmap) ;
         if (pw != My)
            cc = cc * My / pw ;
         oss << "N=" << sz << " Estimated " << costname << " cost is " << cc << "\n" ;
         explain("000000", planmap, rplan, oss) ;
         lock_guard<mutex> lk(result_mu) ;
         outbufs[idx] = oss.str() ;
         ready[idx] = true ;
         while (next_to_print < ntasks && ready[next_to_print]) {
            cout << outbufs[next_to_print] ;
            outbufs[next_to_print].clear() ;
            next_to_print++ ;
         }
         cout.flush() ;
      } ;
      int nthreads = min(ntasks, max(1, nprob_threads)) ;
      atomic<int> next_idx{0} ;
      vector<thread> threads ;
      threads.reserve(nthreads) ;
      for (int i = 0; i < nthreads; i++)
         threads.emplace_back([&]() {
            for (;;) {
               int idx = next_idx.fetch_add(1) ;
               if (idx >= ntasks) break ;
               worker(idx) ;
            }
         }) ;
      for (auto &t : threads)
         t.join() ;
      exit(0) ;
   }
   cubesizetype My = (N - 2) / 2 ;
   cubesizetype Mx = (N - 2) - My ;
   if (phasewidth <= 0)
      phasewidth = My ;
   if (extrapolate)
      extrapolatemul = My / (double)phasewidth ;
   if (!fast || writemoveinc || randmoves || (readmoves && nprob_threads <= 1))
      alwaysinmem = 1 ;
   if ((readmoves || writemoves || writemoveinc) && solvecount > 1)
      error("! cannot write frames and read or write moves, or do multiple solves at the same time") ;
   double cc = bestcost("000000", Mx, phasewidth, useavg ? 'a' : 'w') ;
   if (randmoves)
      random_move_scramble(N, randmoves) ;
   else if (readmoves)
      readmoveset() ;
   else if (phasewidth != My)
      cc = cc * My / phasewidth ;
   cout << "Estimated " << (useavg ? "average" : "worst-case") << " cost is " << cc << endl ;
   reductionplan.clear() ;
   explain("000000") ;
   if (planonly)
      exit(0) ;
   ll mmc = 0 ;
   for (ll ii=1; ii<=solvecount; ii++) {
      ll t = dosolve(N, phasewidth) ;
      addcounts(t) ;
      mmc += t ;
   }
   if (solvecount == 1) {
      ll r = mmc ;
      if (extrapolate) {
         if (stats)
            for (int i=0; i<(int)byphase.size(); i++)
               byphase[i].showstats(i, My*phasewidth) ;
         lll lll_r = (lll)r * My / phasewidth ;
 cout << "r " << r << " My " << My << " lll_r " << lll_r << endl ;
         lll lll_cancellations = (lll)cancellations * My / phasewidth ;
         lll lll_blocks = (lll)blocks * My / phasewidth ;
         cout << "N " << N << " width " << phasewidth
              << " moves " << lll_r << " cancel " << lll_cancellations
              << " blocks " << lll_blocks << endl ;
      } else {
         if (stats)
            for (int i=0; i<(int)byphase.size(); i++)
               byphase[i].showstats(i, Mx*My) ;
         cout << "N " << N
              << " moves " << r << " cancel " << cancellations
              << " blocks " << blocks
              << " wingmoves " << wingmoves
              << " centermoves " << (r - wingmoves) << endl ;
      }
   } else {
      fullreport(solvecount) ;
   }
}
