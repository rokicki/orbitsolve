# fastsolve-x86: fastsolve (~/fast/proto, the solver session's code) ported to x86-64 Linux for arena

2026-11-11.  src/ = ~/fast/proto's solve.cpp + headers with the changes in x86.diff (unified diff against
~/fast/proto; ~/fast/proto itself is untouched):

- kern.h: on non-ARM, NEON intrinsics come from SIMDe (header-only, v0.8.2, `git clone --depth 1 --branch v0.8.2
  https://github.com/simd-everywhere/simde.git`); `aesMix(h, k)` = AESE+AESMC on ARM, and AESENC(h ^ k, 0) with
  AES-NI on x86 (the same function, so hashes match).  fulltab.h's 7 AES sites call aesMix.
- perf.h: Linux counters via perf_event_open (user space, inherited by threads; needs
  kernel.perf_event_paranoid <= 2, else 0).
- solve.cpp: RAM via sysconf on Linux; cache dir ~/.cache/fastsolve; the shape-specialization self-compile uses
  $CXX (default c++) with -march=native and the SIMDe include (FS_SIMDE, set at build time).
- coset.h: #include <cmath> (gcc).
- fulltab.h: FTSHM=path keeps the table in a file (/dev/shm on arena: RAM, huge pages with the
  `huge=within_size` mount): the build writes path (MAP_SHARED, built in place), path.ex (exact set), then
  path.meta (tws fingerprint, size, LAYERGB, E, D); a later run with the same tws/FTGB/LAYERGB maps it read-only.
  Fill progress line every PROGSEC seconds (default 120) during the depth 8/9 DFS levels.

Build on arena (Xeon E5-2690 v2, Ivy Bridge: SSSE3/SSE4.2/AVX/AES-NI, no AVX2):

    H=~/fastsolve-x86/simde-src
    g++ -std=c++20 -O3 -march=native -I$H -Wno-unknown-pragmas -DFS_SIMDE="\"$H\"" -pthread -o solve2 solve.cpp

Run (see ../../fastsolve.md for inputs/outputs):

    FTSHM=/dev/shm/2ud FTGB=34 LAYERGB=8 numactl --interleave=all ./solve2 2ud-si.tws LINES.txt 1000000 40

Validation: on arena the 2ud-si exact layers 1..7 match the laptop exactly (2, 24, 295, 5192, 110190,
2663600, 68615014).  Also here: probebench.cpp (random-probe rate: /dev/shm vs anonymous 4K vs THP),
go1000.sh (the certorder N=1000 series on arena).
