#!/usr/bin/env python3
"""Performance database for gullicube: cube sizes 4, 8, ..., 1024 and beam
widths 1, 2, 4, ..., run in increasing order of (estimated) wall time.

    python3 perfdb.py [--sizes 4,8,...,1024] [--maxtime 600] [--seed 1] [--idle 80] [--pause] [--out DIR]

Each run is `gullicube -b WIDTH --seed SEED N` under /usr/bin/time.  The next
run is always the pending one with the smallest estimated wall time: every
size starts at width 1, and when (N, w) finishes, (N, 2w) is queued with an
estimate of its load time + 2.2 x its solve time.  Widths stop doubling once a run's estimate
exceeds --maxtime seconds.  Before each run the script waits until the CPU is
at least --idle percent idle (macOS `top`; skipped where unavailable).  With
--pause, other processes of this user using over 20% CPU are stopped (SIGSTOP)
for the duration of each run and resumed afterwards (POSIX only).

Output, in DIR (default: perfdb next to this script):
  logs/N<n>_w<w>.log   the run's full output (gullicube stdout + time report)
  perfdb.txt           one space-separated line per run, appended as runs finish:
      N width wall_s solve_s load_s user_s sys_s cpu_s instructions cycles moves maxrss_bytes
The script is resumable: runs already in perfdb.txt are skipped (their times
seed the estimates), so a larger --maxtime continues where an earlier one
stopped.  Instruction and cycle counts need macOS's /usr/bin/time -l; on Linux
(/usr/bin/time -v) they are written as -1.
"""
import argparse, heapq, os, platform, re, signal, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
GULLI = os.path.join(HERE, "..", "dist", "gullicube")
EXE = os.path.join(GULLI, "gullicube" + (".exe" if os.name == "nt" else ""))
FIELDS = "N width wall_s solve_s load_s user_s sys_s cpu_s instructions cycles moves maxrss_bytes"


def cpu_idle():
    """Percent CPU idle over about one second, or None if unknown."""
    if platform.system() != "Darwin":
        return None
    try:
        out = subprocess.run(["top", "-l", "2", "-n", "0", "-s", "1"], capture_output=True, text=True, timeout=30).stdout
        m = re.findall(r"CPU usage:.*?([\d.]+)% idle", out)
        return float(m[-1]) if m else None
    except (OSError, subprocess.SubprocessError):
        return None


def wait_idle(threshold):
    while True:
        idle = cpu_idle()
        if idle is None or idle >= threshold:
            return
        print(f"  waiting: CPU {idle:.0f}% idle (need {threshold:.0f}%)", flush=True)
        time.sleep(10)


def hogs():
    """This user's other processes using over 20% CPU (POSIX ps)."""
    try:
        out = subprocess.run(["ps", "-Ao", "pid,pcpu,user,comm"], capture_output=True, text=True).stdout.splitlines()[1:]
    except OSError:
        return []
    me, user, r = os.getpid(), os.environ.get("USER"), []
    for line in out:
        f = line.split(None, 3)
        if len(f) == 4 and f[2] == user and float(f[1]) > 20 and int(f[0]) != me and "claude" not in f[3] and "gullicube" not in f[3]:
            r.append(int(f[0]))
    return r


def run(n, w, seed, logdir):
    darwin = platform.system() == "Darwin"
    timer = ["/usr/bin/time", "-l"] if darwin else ["/usr/bin/time", "-v"]
    cmd = timer + [EXE, "-b", str(w), "--seed", str(seed), str(n)]
    t0 = time.time()
    p = subprocess.run(cmd, capture_output=True, text=True, cwd=GULLI)
    wall = time.time() - t0
    log = os.path.join(logdir, f"N{n}_w{w}.log")
    with open(log, "w") as f:
        f.write("# " + " ".join(cmd) + "\n" + p.stdout + "\n# --- time ---\n" + p.stderr)
    out, err = p.stdout, p.stderr
    m = re.search(r"TOTAL\s+(\d+) moves\s+\(load ([\d.]+)s.*?wall ([\d.]+)s", out)
    if p.returncode != 0 or not m:
        raise RuntimeError(f"N={n} w={w} failed (exit {p.returncode}); see {log}")
    moves, load, solve = int(m.group(1)), float(m.group(2)), float(m.group(3))

    def num(pat, default=-1.0):
        mm = re.search(pat, err)
        return float(mm.group(1)) if mm else default

    if darwin:
        real = num(r"([\d.]+)\s+real")
        user = num(r"([\d.]+)\s+user")
        sys_ = num(r"([\d.]+)\s+sys")
        instr = int(num(r"(\d+)\s+instructions retired"))
        cycles = int(num(r"(\d+)\s+cycles elapsed"))
        rss = int(num(r"(\d+)\s+maximum resident set size"))
    else:
        real = wall
        user = num(r"User time \(seconds\): ([\d.]+)")
        sys_ = num(r"System time \(seconds\): ([\d.]+)")
        instr = cycles = -1
        rss = int(num(r"Maximum resident set size \(kbytes\): (\d+)") * 1024)
    if real < 0:
        real = wall
    return [n, w, f"{real:.3f}", f"{solve:.3f}", f"{load:.3f}", f"{user:.3f}", f"{sys_:.3f}", f"{user + sys_:.3f}",
            instr, cycles, moves, rss]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--sizes", default="4,8,16,32,64,128,256,512,1024")
    ap.add_argument("--maxtime", type=float, default=600.0, help="skip runs estimated above this many seconds")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--idle", type=float, default=80.0, help="wait for this CPU idle percent before each run")
    ap.add_argument("--pause", action="store_true", help="stop other CPU-heavy processes of this user during each run")
    ap.add_argument("--out", default=os.path.join(HERE, "perfdb"))
    a = ap.parse_args()
    print("# perfdb.py " + " ".join(sys.argv[1:]), flush=True)
    if not os.path.exists(EXE):
        sys.exit(f"build gullicube first: {EXE} not found")
    sizes = [int(x) for x in a.sizes.split(",")]
    logdir = os.path.join(a.out, "logs")
    os.makedirs(logdir, exist_ok=True)
    data = os.path.join(a.out, "perfdb.txt")
    done = {}
    if os.path.exists(data):
        for line in open(data):
            f = line.split()
            if f and f[0].isdigit():
                done[(int(f[0]), int(f[1]))] = float(f[4]) + 2.2 * float(f[3])  # estimate for the next width
    else:
        with open(data, "w") as f:
            f.write(FIELDS + "\n")
    # pending runs: (estimated wall, N, width); start each size at its first width not yet done
    heap = []
    for n in sizes:
        w, est = 1, 0.0
        while (n, w) in done:
            est = done[(n, w)]
            w *= 2
        heapq.heappush(heap, (est, n, w))
    while heap:
        est, n, w = heapq.heappop(heap)
        if est > a.maxtime:
            print(f"stop N={n}: width {w} estimated at {est:.0f}s > --maxtime {a.maxtime:.0f}s", flush=True)
            continue
        paused = hogs() if a.pause else []
        for p in paused:
            try: os.kill(p, signal.SIGSTOP)
            except ProcessLookupError: pass
        try:
            if not a.pause:
                wait_idle(a.idle)
            row = run(n, w, a.seed, logdir)
        finally:
            for p in paused:
                try: os.kill(p, signal.SIGCONT)
                except ProcessLookupError: pass
        with open(data, "a") as f:
            f.write(" ".join(str(x) for x in row) + "\n")
        wall, nextEst = float(row[2]), float(row[4]) + 2.2 * float(row[3])
        print(f"N={n:5d} w={w:6d}  wall {wall:8.2f}s  cpu {row[7]:>9}s  moves {row[10]}  (est {est:.1f}s)", flush=True)
        heapq.heappush(heap, (nextEst, n, 2 * w))
    print("done", flush=True)


if __name__ == "__main__":
    main()
