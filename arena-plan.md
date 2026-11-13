# Arena plan (written 2026-11-04, for when we are home)

Arena: two sockets, 20 cores / 40 threads, old high-latency memory, **1.5 TB**, little disk.  So: much
deeper twsearch pruning tables, built in memory and never written, at **12-24 h per deep table**.

## Use each table to the full before the next

A table costs a day to build and is lost when its holder stops, so:

- **Everything ready before the fill starts.**  Build the whole work queue for that puzzle first,
  in order of value:
  - the tails of the top lengths at every tail length the table makes affordable (12 up to 16),
    distinct up to the stage symmetries (mktails.py), longest lines first;
  - then the full lines it makes affordable (e.g. 2ud 14s), top lengths first.
- **Fold while the table is held.**  Folding needs no table: splice3, certextend with --proptop
  pulls, certcheck.  Then make the next round's new tails from the folded stage and feed them to
  the same table (as opt2ud/loop.sh does).  Stop only when a round brings little new work.
- **One table per puzzle definition.**  1ud and 2ud need separate tables, as do 3ax and 4op.  A
  table for a finer labelling overestimates distances to a coarser goal (not admissible); one for
  a coarser labelling is admissible but weaker.
- **Memory layout (Linux):** start the holder under `numactl --interleave=all`, so the table is
  spread over both sockets' memory.  Run 40 workers (one per hardware thread): lookups wait on
  memory, so hyperthreads help.
- **Keep the holder safe.**  No reboots, and the holder must not be run in a terminal that might
  close (nohup).  Remember: `kill -INT` removes the table, and that is a day lost.
- **Measure before committing:** the first fill's depth-by-depth times and the hit rate on a sample
  of each queue.

## What worked on the laptop (2026-11-03..04)

In order of effect:

1. **Cheap structure tricks.**
   - Tail finds spliced into every line by position, under all the stage symmetries
     (opt1ud/splice/splice3.py).
   - Closing the table with `certextend --proptop 16` pulls.  Push propagation only expands classes
     shortened in the same run, so splices and merges were never propagated.
   - Dropping goal-fixing last moves (certextend fix).
   - Result: 1ud 17s 107k -> 5.3k, 2ud 18s 686k -> 288k, in minutes.
2. **Short searches with the full move set.**  all-moves `-si` on lines up to 12-13, and on 12- and
   13-move tails.  Hit rates 22-68%.
3. **Deep full-line searches: hopeless past ~13 with a depth-8 table.**  About 10x per extra move.
   hq23 on the 3ax 13s/14s found only 2.4%/5.9%.

## Tables in memory only

```
twsearch -M <size> -t <cores> --nowrite --startprunedepth D --shmhold NAME ...   # holder
twsearch --shmattach NAME -t 1 --prefixslack 0 -si ...                           # each worker
```

- `--startprunedepth D` fills to depth D before any search (as far as memory allows).
- `--shmhold` builds the table in shared memory, and `--nowrite` keeps it off the disk.
- Use a build with the table-load fix: twsearch main c73aa20 or later, or our twsi4.
- **Test this flow on the laptop first, with a small -M.**  Check: forced depth, shared memory, no
  write, attach, and the same answers as a plain run.
- The table lives only while the holder runs; a reboot or a kill loses it.  So treat each table as
  a campaign: fill once, do all the work for that puzzle and move set, then drop it.
- One table at a time.
- Each extra depth costs about one branching factor in fill time, and makes every search about
  that much cheaper.

## Move sets and depths (estimates from laptop branching; measure the first fill at home)

| stage | move set | laptop (40 GB) | arena (1.5 TB) | fill time (arena: 12-24 h for the deep ones) |
|---|---|---|---|---|
| 1ud, 2ud | full (all moves) | depth 8 | depth 9 (~500 GB) | ~10-12 h |
| 3ax | full for tails <= 13; **hq23** for deeper | hq23 depth 9 | hq23 depth 10 (~300 GB) | ~5-6 h |
| 4op | **h** or h23 for deep tails | h depth 12 | h depth 13-14 (~200 GB-1 TB) | hours |

Why these sets:
- 1ud/2ud: the high all-moves hit rates mean the old lines missed moves outside the old generating
  set, so keep the full set.
- 3ax: every line needs U/D-slice quarter turns, and 75% fit HQ23.
- 4op: 48% of lines are H-only.
- Move-set census and branching table: twsearch.md.  Never `--newcanon` with the full move set.

## Campaign order

1. **3ax (hq23 depth 10) and 4op (h or h23, depth 13+), toward boundary 40.**
   - Search 14-16-move tails of the 3ax 21-23s and the 4op 20-22s.
   - Splice them by position under the 16 stage symmetries across the whole table, then re-merge
     with axbound.
   - Re-run axbx 40, then axbcheck.
2. **1ud/2ud (full set, depth 9).**  Search 14-move tails of whatever 2ud 18s and 1ud 16/17s remain
   after the laptop's tail loop.  Target UD 33: its cost is about proportional to 1ud 16s x
   2ud 18 positions.
3. Meanwhile the laptop keeps the UD side going: the tail loop, pulls, then UD 33.

## Before trusting a boundary result

- Run axbcheck / boundcheck in full.  41 is only covered (axbx), and only a 1/50 axbcheck sample was
  run.
- Also do one end-to-end replay that shares no code with certcube.h.  Sample bad positions, replay
  the chosen line plus the next stage's line on the .tws definitions, and check it is solved
  within X.

## State to pick up from (see upperbound5.md, journal.md)

- 1ud/2ud: bcert/ud34/snap (certcheck PASS); UD 34 dry run in bcert/ud34.
- 1ud table: opt1ud/cache/tws9-1ud-si-39G.dat.  The run can resume: shm2/full from chunk caaagh,
  tails2 15s.
- 2ud: opt2ud (table cache/tws9-2ud-si-39G.dat, shm owner /ud2si), loop.sh rounds r2ud-si3, ...
- 3ax: opt3ax/merge/ax-m3.tab; 4op: op/4op-m1.tab; boundary runs in bnd/.

## Running Claude Code on arena (Remote Control; docs: https://code.claude.com/docs/en/remote-control.md)

- Install Claude Code.  Run `claude`, then `/login` with the claude.ai account (API keys don't work).
- In the project directory, inside tmux: `claude remote-control --name arena`.  Do this once at the
  terminal: it asks to enable Remote Control and to trust the directory, and the trust prompt fails
  without a terminal.  Trust is not saved for the home directory itself, so use a project directory.
- Connect from claude.ai/code or the app (the server prints a URL / QR code).  It needs only
  outbound HTTPS, no inbound ports, so working only from home is fine.
- It reconnects after short drops.  It exits after ~10 min offline while awake.
  `claude remote-control --continue` resumes within ~4 h.
- Permission prompts wait (no timeout) for a connected device, so long jobs can stall unattended.
  Choose `--permission-mode` and allow rules up front, and turn on push notifications for prompts
  (`/config`).
- Copy the memory notes (~/.claude/projects/<laptop path>/memory/*.md) to arena's matching project
  memory directory, or have the session read them first.
