# Arena week run: runbook for the armory Claude (2026-11-13)

You run arena unattended while the user travels: keep both arena sockets searching, fold the results every ~2 h,
record them, push to GitHub.  Read SYNC.md, arena/README.md and the end of journal.md first.

## The plan

Goal: lower the oblique-pair stage tables (per-orbit upper bounds; upperbound5.md) by replacing the tails of the
longest stage lines with shorter ones found by fastsolve on arena, then folding (each shortening propagates to
many classes).

| stage | arena | now | longest lines (2026-11-13 11:00) |
|---|---|---|---|
| 1ud | node 0 (20 CPUs), table /dev/shm/1ud-300 (D=9) | run prod2: 16-move tails of the 17s, ~85% shorten | 17s 2,891 (from 5,272) |
| 2ud | node 1 (20 CPUs), table /dev/shm/2ud-300 (D=9) | run prod4: 15-move tails of the 18s, shuffled, ~5% shorten | 18s 136,081 (from 286,263) |
| 3ax | stopped (user, 11-13): 16-move tails take 7-8 h each | table kept in /dev/shm | 23s 546 |
| 4op | stopped (user, 11-12): 14/15-move tails shorten ~0.4% | table kept in /dev/shm | 22s 14,541 |

The original week plan (arena-week.md: all four stages, 1ud rebuilt to D=10) is superseded: 3ax and 4op were
stopped for low yield, so each socket runs one stage.  **No table rebuild, no 3ax/4op restart, no D=10 unless the
user asks.**

Next lists, in order (mktails.py; always from the latest fold):
- **1ud** after prod2: (a) whole 17-move lines, `mktails.py 1ud 17 17 --max 20` first, to time them (expected
  ~0.5 h each, maybe more); if each finishes in under ~2 h, queue the rest (`mktails.py 1ud 17 17`); otherwise
  report to the user and go to (b).  Every 17 removed counts: none left means 1ud's worst drops to 16.
  (b) 15-move tails of the 16s, shuffled, in lists of ~20,000: `mktails.py 1ud 16 15 --shuffle SEED --max 20000`.
- **2ud** after prod4: (a) the 15-move tails not yet queued (`mktails.py 2ud 18 15`, ~200); (b) 16-move tails of
  the 18s, shuffled, ~2,000 per list: `mktails.py 2ud 18 16 --shuffle SEED --max 2000` (expect minutes each; time
  the first list and size later lists to ~1 day).

## Tools (all in ~/orbitsolve/arena; pypy3 on PATH)

| command | does |
|---|---|
| `astatus.sh` | arena uptime/load, /dev/shm tables and mount, each stage's run: alive?, searched / total, shortened, rough time left |
| `refold.sh` | fetch outputs, fold every stage whose results changed, checks, upperbound5.md Working tables, arena/results/ |
| `astop.sh ST` | stop ST's run (exact pid) |
| `aclose.sh ST` | close ST's finished or stopped run (cuts all.txt to what was searched) |
| `mktails.py ST LEN K [--shuffle S] [--max N] > F` | the next list: K-move tails of the LEN-move lines of the latest fold, not queued before |
| `alaunch.sh ST NAME F` | start ST's next run on arena (name it prod5, prod6, ...; unique per stage) |
| `ar CMD`, `aget R L` | ssh / scp to arena (10.0.0.155, ~/fastsolve-x86) |

## Every ~2 hours

1. `cd ~/orbitsolve && git pull --rebase`
2. `arena/astatus.sh`.  Both runs alive?  If not: Problems below.
3. `arena/refold.sh > /var/tmp/orbitsolve/fsfold/refold-$(date +%m%d-%H%M).log 2>&1`, then read it: every stage
   that folded shows `PASS` and its new lengths (about 10 min for 1ud + 2ud; one refold at a time).
4. Append one line to journal.md, e.g.
   `11-14 10:00 [armory] fold: 1ud f18 17s 2,700; 2ud f18 18s 131,000 (vline/certcheck PASS); arena 1ud 4,100/6,687, 2ud 15,000/45,382.`
5. `git add journal.md upperbound5.md arena/results && git commit -m "armory fold ..." && git pull --rebase && git push`
   (if a push fails with "bad record mac": `git -c http.version=HTTP/1.1 push`).
6. Queue: if a stage's run has under ~6 h left, or has ended, prepare its next list; when it has ended:
   `arena/aclose.sh ST`, `pypy3 arena/mktails.py ... > /var/tmp/orbitsolve/optfs/ST/NAME.txt`,
   `arena/alaunch.sh ST NAME /var/tmp/orbitsolve/optfs/ST/NAME.txt`; journal it (list, size, what it is).
   Make the list after aclose (it then offers again whatever a stopped run did not search).

## Problems

- **ssh to arena fails**: the ISP drops connections, even on the LAN.  Retry for a while; never conclude arena is
  down from one failure.  Arena jobs are detached and keep running.
- **A run is dead before its list was done** (astatus "dead", searched < total): read arena
  `~/fastsolve-x86/runs/ST-NAME.err`.  Find the cause before restarting (don't guess).  Then `aclose.sh ST`, a new
  list (mktails.py offers the unsearched tails again), `alaunch.sh`.
- **Arena rebooted** (astatus: small uptime, /dev/shm tables gone, `huge=within_size` missing from the shm mount):
  tell the user (the shm remount needs their sudo: `sudo mount -o remount,size=1400G,huge=within_size /dev/shm`).
  Meanwhile you may rebuild 1ud and 2ud (only those) with `alaunch.sh --build ST NAME LIST` (after `aclose.sh ST`);
  without the remount the tables get 4 KB pages (slower searches) and /dev/shm holds at most ~750 GB (two tables).
  A build takes hours; astatus shows the run alive with 0 searched meanwhile.
- **A check fails** (refold.sh: vline not PASS, no PASS line, "outs ... != all.txt", fold.py error): stop folding,
  keep the last good fold (refold.sh keeps it), find the cause, tell the user.  Never commit a failed fold's tables.
- **armory disk**: each fold keeps only its latest dir (~1 GB); /var/tmp/orbitsolve needs ~10 GB.

## Rules

- Never `pkill -f` / `pgrep -f`; stop arena runs only with astop.sh (exact pids).  Don't touch another
  machine's or agent's processes.
- fastsolve runs with its default SLACK (improvements only).  Never SLACK=0.
- Don't edit a script while it runs (copy it, edit the copy, use it next time).
- Don't touch repo/ (rokicki/obliques), ~/orbitsolve-certsafe, or certificates; folds are working tables only.
- Record every result in journal.md in the same step (entries start with the time and `[armory]`), then push.
- Big files stay on armory.  What the laptop needs travels in git: journal.md, upperbound5.md (Working tables),
  arena/results/*-improved.txt (every shortened line; with them the laptop can rebuild any fold).
- Ask the user before anything outside this runbook (new stages, table rebuilds other than after a reboot,
  changing tools or settings).
