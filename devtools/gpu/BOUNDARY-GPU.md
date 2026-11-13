# GPU boundary tools (Metal, macOS)

Four programs that move the inner loops of the CPU boundary tools (`devtools/boundx.cpp`,
`boundcheck.cpp`, `axbx.cpp`, `axbcheck.cpp`) onto an Apple GPU.  Same inputs, same block-file
format, same claim; the checkers were validated against the CPU checkers (identical counts) and
also re-check a sample of blocks on the CPU in every run.

| program | CPU original | does |
|---|---|---|
| `gpuboundx` | `boundx` | **search**: writes blocks for a UD boundary (stages 1ud + 2ud) |
| `gpubound` | `boundcheck` | **check**: verifies a UD block file |
| `gpuaxbx` | `axbx` | **search**: writes blocks for an AX boundary (stages 3ax + 4op) |
| `gpuaxbcheck` | `axbcheck` | **check**: verifies an AX block file |

Results obtained with them (oblique chiral pair, P = UD + AX, dry runs): UD 33 and AX 41, 40, 39,
38, so **P <= 71** (plain sum 80).  Run logs, timings and the history are in `scoreboard.md` and
`journal.md`; the method is in `methods.md` ("Boundary passes").

## The claim

Two consecutive stages A then B, each with a stage certificate (a line per class; `dA`, `dB` =
line length over the symmetric images).  The plain bound is `max dA + max dB`.  A boundary
certificate proves a smaller total X: for every A class with line L0 (`|L0| = c0`) and every B
state s that L0 leaves (as carried by L0) with `c0 + dB(s) > X` (a *bad* state), some line P in
the class's block solves the A state and then `|P| + dB(state P leaves) <= X`.

A *block* is L0 followed by its alternative lines P (plain move sequences, blank line between
blocks).  Only classes with `c0 > X - max dB` need a block.

The trick that makes this GPU-friendly: since every P solves the same A state as L0, the B pieces
are just permuted relative to L0's frame, `M = D_P o D_L0^-1`.  A bad state is moved through each
line's M, ranked, and looked up in the B distance table: one GPU thread per (block, bad state).

* UD: stage B = 2ud, one target group per orbit; a state is which 4 of each orbit's free facelets
  hold the target pieces (colex rank of 4-subsets, two orbits).
* AX: stage B = 4op; a state is four 8-bit masks (orbit x axis), ranked by a table of the 70
  4-subsets of 8.  Stage A = 3ax, two 16-bit masks.

## Build

From `devtools/gpu` (needs only `../certcube.h`; same line for each program):

    clang++ -std=c++20 -O3 -g -Wall -fobjc-arc -I.. -framework Metal -framework Foundation \
      -o gpuaxbx gpuaxbx.mm -lpthread

The Metal kernels are compiled at run time from source strings in each `.mm`.

## The search tools: `gpuboundx`, `gpuaxbx`

    gpuboundx STAGEDIR_A STAGEDIR_B X --out FILE [--base POOL]... [--prop C (8)] [--slack L (5)] [--nosteps]
              [--deep S] [--deepprop C2] [--threads T (2)] [--batch K (256)] [--sample K] [--first N]
              [--only FILE] [--left FILE]
    gpuaxbx   STAGEDIR_3AX STAGEDIR_4OP X --out FILE[.zst] [--base POOL]... [--prop C (8)] [--slack L (2)]
              [--deep S] [--deepprop C2] [--threads T (2)] [--batch K (256)] [--sample K]
              [--only FILE] [--left FILE] [--order random|long|file] [--seed N]

Per class (in batches of `--batch`):

1. **One action** (GPU): every *action* a (outer turns plus the base pools' sequences of at most
   `--prop C` moves, all symmetric images) is applied to the class's A state u; a is kept when
   `|a| + dA(a(u)) <= c0 + slack`.
2. **Paths** (CPU, `--threads`): each kept a gives the path `a + line(a(u))`, simplified, plus the
   stage-symmetric copies of L0 that fix u.  Each path's map M is computed; paths are sorted
   cheapest first and a path whose M equals an earlier one's is dropped (it cannot cover anything
   new).
3. **Check** (GPU): every bad B state tries the paths in order; the first that solves it is marked
   used.  Uncovered states are collected.
4. gpuboundx only, **pass 2**: one action with more slack on the states left, the slack raised in
   steps 3, 4, ..., `--slack` (each step only on the actions new at that slack; `--nosteps` goes to
   `--slack` at once).
5. **`--deep S`**, for classes still with states left: two actions a1 a2 then the line, with
   `|a1| + dA(a1(u)) <= c0 + slack + S` and `|a1| + |a2| + dA <= c0 + slack`; the action kernel runs
   on every a1(u).  `--deepprop C2` uses only actions of at most C2 moves here (a prefix of the
   sorted action list); this is the expensive part, and C2 = 8 is ~100x cheaper per class than all
   actions at C = 10.

The block written for a class is L0 plus its *used* paths, in order.  A class with states left is
written anyway (its partial block) and its line goes to `--left FILE`.

Other options:

* `--base POOL`: `.cls` sequence pools for the actions (run from the directory holding `pools/`).
* `--only FILE`: only the classes whose line is in FILE (a previous pass's `--left`).
* `--sample K`: every Kth class (timing / testing).  gpuboundx `--first N`: the first N classes.
* gpuaxbx `--out FILE.zst`: compressed through `zstd` (AX block files are large: 37 GB plain for
  AX 38's broad pass).
* gpuaxbx `--order`: the order the classes are done in.  `random` (default) gives a steady rate and
  a predictable finish; `long` does the longest lines first (for a final run, so a failure shows
  early); `file` is the stage files' order (shortest first).  The seed is printed; `--seed N`
  repeats an order.  The order does not change the blocks (tested: identical block sets and `.left`
  on an AX 41 sample in all three orders).

Progress goes to stderr: a line about once a minute (`classes done / total, not covered, to the deep
pass, GPU/CPU times`), plus a line per deep-pass class that takes 5 s or more, ending
`covered` / `NOT covered`.  The summary line is `classes: N covered, M not ...`, followed by a
histogram of classes by positions left.

### A multi-pass run

One pass with generous settings on millions of classes is far too slow; most classes are easy.
The pattern that worked (AX 38: 7.26M classes):

| pass | options | AX 38 classes in -> left |
|---|---|---|
| a (broad) | `--prop 8 --deep 2` (UD: `--deep 1`) | 7,263,089 -> 749,148 |
| b | `--only a.left --prop 10 --slack 4 --deep 4 --deepprop 8` | 749,148 -> 7,030 |
| c | `--prop 10 --slack 4 --deep 4` | 7,030 -> 5,023 |
| d | `--prop 10 --slack 5 --deep 5` | 5,023 -> 164 |
| e | `--prop 11 --slack 6 --deep 6` | 164 -> 2 |
| f | `--prop 11 --slack 7 --deep 7 --deepprop 9` | 2 -> 0 |

Raising the slack (d, e) is what clears the near misses; using all actions at the same slack (c)
helps little.  Then merge and check:

    pypy3 mergeN.py OUT[.zst] a.txt a.left b.txt.zst b.left ... f.txt.zst
    gpuaxbcheck STAGEDIR_3AX STAGEDIR_4OP OUT.zst X --threads 16 --cpu-sample 5000

`mergeN.py` keeps each pass's blocks for the classes it covered (not in its `.left`) and all of the
last pass's, and checks that each pass's classes are exactly the previous pass's left classes.
It reads and writes `.zst`.

## The checkers: `gpubound`, `gpuaxbcheck`

    gpubound    STAGEDIR_A STAGEDIR_B BOUNDARYFILE X [--threads T (2)] [--keep u|ud] [--blocks K (16)]
                [--cpu-sample N] [--drop-line B:I] [--drop-block B]
    gpuaxbcheck STAGEDIR_3AX STAGEDIR_4OP BOUNDARYFILE[.zst] X [--threads T (2)] [--blocks K]
                [--cpu-sample N] [--drop-line B:I] [--drop-block B]

The same four checks as the CPU checkers:

1. both stage certificates cover their stages;
2. every block: legal moves, every line solves the first line's A state, side effects as
   `certcheck` / `axcheck` (gpubound `--keep u|ud` as boundcheck);
3. every A state with `dA > X - max dB` has a block for a symmetric image of it;
4. per block, every bad B state is solved by some line P with `|P| + dB <= X`.

Checks 1-3 run on the CPU (stage lines and blocks streamed, so memory stays near the tables'
size); check 4 runs on the GPU.  `--cpu-sample N` also checks every Nth block the CPU way and
compares the counts (any difference is reported).  `--drop-line B:I` / `--drop-block B` break the
input on purpose; the run must then FAIL (use this to test the checker).  The last line is `PASS:`
or `FAIL`.  gpuaxbcheck reads `.zst` block files through `zstd -dc`.

Example (AX 38 PASS, 10.6 h, Apple M3 Max, 16 threads + GPU):

    check 4 (GPU): 147437136202996 positions tried, 0 not solved in 0 blocks
    CPU sample: 1453 blocks, 0 differ from the GPU
    3ax states with d3 > 16: 116080766, 0 without a block
    PASS: every 3ax start is solved through 3ax and 4op in at most 38 moves

## Speed

Clean benchmark (UD 34, idle machine, 16 threads): `boundcheck` 378 s vs `gpubound` 6.9 s; the check-4
loop alone 424 s CPU vs 1.3 s GPU (~325x).  AX 40 search + check: ~1.3 h on the GPU against a CPU
estimate of 10-17 h + ~5 days.

## Things learned the hard way

* **Wrap each batch in `@autoreleasepool`.**  Metal command buffers and encoders are autoreleased;
  without a pool per batch they pile up over hours (wired memory, heavy compression).
* **Left-state buffer overflow.**  The uncovered states of a batch go to a fixed buffer; on overflow
  the batch is split in halves and rerun.  A single class can still overflow: gpuaxbx then grows the
  buffer (a class has at most the 24,010,000 4op states).  **gpuboundx still exits** in that case
  ("left states overflow for one class: raise leftCap", leftCap = 2^22); port the gpuaxbx fix if a
  UD run hits it.
* **Counters are 64-bit** (`positions tried` overflowed 32 bits on AX 41).
* Shared Metal buffers are wired memory; size the batch accordingly.
* Disk: AX block files are tens of GB; use `--out FILE.zst`.
* Not yet in gpuboundx / gpubound / gpuaxbcheck: `--order`, `.zst` (gpuboundx, gpubound).
