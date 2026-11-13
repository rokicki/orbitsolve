// probebench GB THREADS MODE: random byte probes over a GB-sized table, MODE = shm (file in /dev/shm),
// anon4k (anonymous, no huge pages), anonthp (anonymous, MADV_HUGEPAGE).  Prints probes/s and the AnonHuge/ShmemHuge use.
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
int main(int argc, char **argv) {
  if (argc != 4) { fprintf(stderr, "usage: probebench GB THREADS shm|anon4k|anonthp\n"); return 2; }
  printf("# probebench %s %s %s\n", argv[1], argv[2], argv[3]);
  uint64_t bytes = (uint64_t)(atof(argv[1]) * 1e9) & ~((1ULL << 21) - 1); int T = atoi(argv[2]); std::string mode = argv[3];
  uint8_t *tab;
  if (mode == "shm") {
    int fd = open("/dev/shm/probebench", O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0 || ftruncate(fd, bytes)) { perror("shm"); return 1; }
    tab = (uint8_t *)mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); close(fd);
  } else {
    tab = (uint8_t *)mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (tab != MAP_FAILED) madvise(tab, bytes, mode == "anonthp" ? MADV_HUGEPAGE : MADV_NOHUGEPAGE);
  }
  if (tab == MAP_FAILED) { perror("mmap"); return 1; }
  double t = now();
  { std::vector<std::thread> th; for (int i = 0; i < T; i++) th.emplace_back([&, i] { uint64_t a = bytes / T * i, b = i == T - 1 ? bytes : a + bytes / T; for (uint64_t x = a; x < b; x += 4096) tab[x] = (uint8_t)x; }); for (auto &x : th) x.join(); }
  printf("fill %.1fs\n", now() - t);
  system("grep -E 'AnonHugePages|ShmemHugePages|ShmemPmdMapped|PageTables' /proc/meminfo");
  const long N = 50000000; std::atomic<uint64_t> sink{0};
  t = now();
  { std::vector<std::thread> th; for (int i = 0; i < T; i++) th.emplace_back([&, i] {
      uint64_t s = 0x9E3779B97F4A7C15ULL * (i + 1), acc = 0;
      for (long k = 0; k < N; k += 8) {   // 8 independent probes in flight
        uint64_t idx[8];
        for (int j = 0; j < 8; j++) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; idx[j] = (uint64_t)(((__uint128_t)s * bytes) >> 64); }
        for (int j = 0; j < 8; j++) acc += tab[idx[j]];
      }
      sink += acc; }); for (auto &x : th) x.join(); }
  double d = now() - t;
  printf("%s %.0f GB %d threads: %.1f M probes/s total, %.1f ns per probe per thread\n", mode.c_str(), bytes / 1e9, T, N * (double)T / d / 1e6, d * 1e9 / N);
  munmap(tab, bytes);
  if (mode == "shm") unlink("/dev/shm/probebench");
}
