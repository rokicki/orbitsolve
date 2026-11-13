// mkbin FILE...: load each pool file once, writing its binary cache FILE.bin
// (poolcache.h), so it can be streamed into finish tables or read on demand.
#include "poolcache.h"
int main(int argc, char **argv) {
  if (argc < 2) { fprintf(stderr, "usage: mkbin POOLFILE...   write POOLFILE.bin for each\n"); return 1; }
  printf("#");
  for (int i = 0; i < argc; i++) printf(" %s", argv[i]);
  printf("\n");
  for (int i = 1; i < argc; i++) {
    Pool p;
    loadPoolCached(p, argv[i]);
    printf("%s: %zu algorithms\n", argv[i], p.size());
  }
}
