#include "cube.h"
#include <cstdio>
int main(){ Cube c(8); int S=c.pos.size(); int oA=-1,oB=-1;
 for(int i=0;i<S;i++) if(c.face[i]==0&&c.row[i]==1&&c.col[i]==2) oA=c.orbit[i];
 for(int i=0;i<S;i++){int dr=c.drow(i),dc=c.dcol(i); if(((dr==2&&dc==3)||(dr==3&&dc==2))&&c.orbit[i]!=oA) oB=c.orbit[i];}
 for(int w=0;w<2;w++){int k=0; for(int i=0;i<S;i++) if(c.orbit[i]==(w?oB:oA)) printf("%d %c %d %d %d %d %d\n", w*24+k++, FACECH[c.face[i]], c.row[i], c.col[i], c.pos[i][0], c.pos[i][1], c.pos[i][2]);}}
