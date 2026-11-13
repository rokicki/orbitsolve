#include "pool.h"
int main(){ Pool p; p.load("p6.txt"); std::mt19937_64 r(1); State s=randomState(r);
 for(size_t a=0;a<5;a++){ State t=s.apply(p,a); printf("%d %d\n", correctAfter(p.S[a], s.M), t.correct()); }
 printf("start %d\n", s.correct()); }
