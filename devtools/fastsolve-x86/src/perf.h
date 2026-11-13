// Per-process instruction and cycle counters.  Apple Silicon: proc_pid_rusage (no root needed).
// Linux: perf_event_open, user space only, inherited by threads created later (opened by a static
// initializer, before main; a thread's counts are added to the total when it exits, so totals are
// exact once worker threads are joined).  Needs kernel.perf_event_paranoid <= 2; otherwise 0.
#pragma once
#include <sys/resource.h>
#include <unistd.h>
#include <chrono>
#if defined(__APPLE__)
#include <libproc.h>
#elif defined(__linux__)
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <cstring>
struct PerfFds {
  int ins = -1, cyc = -1;
  static int open1(uint64_t config) {
    perf_event_attr a; memset(&a, 0, sizeof a);
    a.size = sizeof a; a.type = PERF_TYPE_HARDWARE; a.config = config;
    a.inherit = 1; a.exclude_kernel = 1; a.exclude_hv = 1;
    return (int)syscall(SYS_perf_event_open, &a, 0, -1, -1, 0);
  }
  PerfFds() { ins = open1(PERF_COUNT_HW_INSTRUCTIONS); cyc = open1(PERF_COUNT_HW_CPU_CYCLES); }
  static unsigned long long rd(int fd) { unsigned long long v = 0; if (fd >= 0 && read(fd, &v, sizeof v) != sizeof v) v = 0; return v; }
};
inline PerfFds gPerfFds;
#endif
struct PerfSnap { unsigned long long ins, cyc; double t; };
inline PerfSnap perfNow() {
  double t = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
#if defined(__APPLE__)
  rusage_info_v4 ri;
  proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri);
  return {ri.ri_instructions, ri.ri_cycles, t};
#elif defined(__linux__)
  return {PerfFds::rd(gPerfFds.ins), PerfFds::rd(gPerfFds.cyc), t};
#else
  return {0, 0, t};
#endif
}
struct PerfDelta { double ins, cyc, sec; };
inline PerfDelta perfSince(const PerfSnap &a) {
  PerfSnap b = perfNow();
  return {(double)(b.ins - a.ins), (double)(b.cyc - a.cyc), b.t - a.t};
}
