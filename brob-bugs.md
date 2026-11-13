# Two orientation bugs in brobdicube (`~/brob-private`)

Found while building `full/fullsolve` on top of brobdicube's `xcube`. Both
bugs make brobdicube's internal model disagree with a physical cube in a
self-consistent way. Its internal checks, including `-s`, therefore pass,
while the printed solution does not solve the scrambled cube.

**Measured impact.** On 24 random 60-move scrambles (N = 7–12), the printed
`-o` solution of the unmodified brobdicube solves **0/24** when replayed on an
independent simulator. With the two fixes below it solves **24/24**.

---

## How it was checked

- `full/brobcheck.cpp` replays the scramble and then every move line of
  brobdicube's `-o` output on an independent sticker simulator,
  `pair/cube.h`. That simulator is 3-D coordinate based and shares no code
  with brobdicube. It then reports wrong stickers grouped by piece type.
- The same simulator validated the pair-algorithm pools against twsearch2's
  output, and it verifies every `gullicube` run.
- The fixes were tested on a patched *copy* of the source; `~/brob-private` is
  unchanged.

To reproduce:

```sh
cd ~/brob-private
echo "2F U" > /tmp/s.txt
./brobdicube -D algs3 -i -o -t 1 8 < /tmp/s.txt > /tmp/o.txt
~/orbitsolve/full/brobcheck 8 /tmp/s.txt /tmp/o.txt
```

---

## Bug 1: centre-centre tracker rotates the wrong way for middle-layer turns (odd N)

**Where**
- `src/threecube.h:112`: `rot4c`
- `src/threecube.h:174–202`: `threecube::domove`, cases 6–11 (the middle
  layer, called as depth 2 of the 3×3 model). The calls are
  `rot4c(4,3,2,1)`, `rot4c(0,2,5,4)`, `rot4c(0,3,5,1)`, `rot4c(4,5,2,0)`,
  `rot4c(1,5,3,0)` and `rot4c(1,2,3,4)`.
- Compare with `src/xcube.h:124–131` (`rot4ver2`), which rotates
  `xcube::ccs` in the physically correct direction. `xcube::get()` reads
  centre centres from `ccs` (`src/xcube.h:86`).
- It is used in `src/cornersolver.cpp:10–28`: the odd-cube corner phase picks
  middle-layer moves that reduce `cc.countbadcenters()`, which comes from
  `threecube::centercenters`.

**What happens.** For every middle-layer quarter turn,
`threecube::centercenters` is permuted in the opposite direction to
`xcube::ccs` and to a physical cube. `full/cctest.cpp` prints both after each
of `4U 4L 4F 4R 4B 4D` on a 7×7×7. For example, after `4U`:
- `ccs` = 0 2 3 4 1 5, so L now holds F's colour (correct).
- `centercenters` = 0 4 1 2 3 5, so L holds B's colour.

A single turn happens to be fixed correctly, because it is inverted twice. But
for two non-commuting middle-layer turns the orientation fix is wrong:

| N | scramble | brobdicube's centre-centre fix | correct fix | result |
|---|---|---|---|---|
| 7 | `4U 4F` | `4U' 4L'` | `4F' 4U'` | 4 centre-centre stickers wrong |
| 7 | `4U 4R` | `4U' 4F'` | — | 4 centre-centre stickers wrong |
| 9 | `5U 5R` | `5U' 5F'` | — | 4 centre-centre stickers wrong |
| 7 | `4F 4U` | `4U' 4F'` | — | correct (happens to be the true inverse) |

**Fix used for testing.** Reverse the cycle order of the six `rot4c` calls
(apply `rot4c(d,c,b,a)` where the code has `rot4c(a,b,c,d)`):

```diff
-            rot4c(4, 3, 2, 1) ;     // case 6, 2U
+            rot4c(1, 2, 3, 4) ;
-            rot4c(0, 2, 5, 4) ;     // case 7, 2L
+            rot4c(4, 5, 2, 0) ;
-            rot4c(0, 3, 5, 1) ;     // case 8, 2F
+            rot4c(1, 5, 3, 0) ;
-            rot4c(4, 5, 2, 0) ;     // case 9, 2R
+            rot4c(0, 2, 5, 4) ;
-            rot4c(1, 5, 3, 0) ;     // case 10, 2B
+            rot4c(0, 3, 5, 1) ;
-            rot4c(1, 2, 3, 4) ;     // case 11, 2D
+            rot4c(4, 3, 2, 1) ;
```

The middle-edge cycles in the same cases are correct; with this fix the
checker finds no wrong middle edges.

---

## Bug 2: `normalizefacerots()` rotates opposite to `get()` and `dosolve()`

**Where**
- `src/xcube.h:67–80`: `xcube::normalizefacerots()`. Case 1 applies
  `xo.rot4(...)` (line 74) and case 3 applies `xo.rot4i(...)` (line 76).
- `src/xcube.h:82–89`: `xcube::get()` reads quadrant `oz` of face `f` as
  `a[4f + ((oz + facerots[f]) & 3)]`.
- `src/brobdicube.cpp:744` (`dosolve`) normalizes the scramble's face
  rotations with `std::rotate(..., + fr[f], ...)`, a left rotation that
  matches `get()`.
- `src/brobdicube.cpp:526` (`solve`) calls `cube.normalizefacerots()`
  straight after `solvecorners()`.

**What happens.** With `facerots = 1`, `get()` means a′[oz] = a[oz + 1], but
`rot4(b,c,d,e)` produces a′[b] = a[e], i.e. a′[oz] = a[oz − 1]. So case 1 and
case 3 are exchanged, and every face whose corner-phase net rotation is an odd
number of quarter turns ends up with its centre orbits rotated 180° relative
to the real cube. Half turns (case 2, `rot2`) are symmetric, so they're
unaffected. brobdicube then solves its rotated picture of those centres.

| N | scramble | corner solution | result |
|---|---|---|---|
| 8 | `2F 3R 2U` | (none) | correct |
| 8 | `2F U` | `U'` | 12 centre stickers wrong (diagonal and oblique) |
| 8 | `2F U'` | `U` | 12 centre stickers wrong |
| 8 | `2F U2` | `U2` | correct |

Any random scramble almost surely needs a quarter turn on some face in its
corner solution, which is why 0/24 random scrambles came out right.

**Fix used for testing.** Swap the two cases:

```diff
-                  case 1: xo.rot4(4*f, 4*f+1, 4*f+2, 4*f+3) ; break ;
+                  case 1: xo.rot4i(4*f, 4*f+1, 4*f+2, 4*f+3) ; break ;
                   case 2: xo.rot2(4*f, 4*f+1, 4*f+2, 4*f+3) ; break ;
-                  case 3: xo.rot4i(4*f, 4*f+1, 4*f+2, 4*f+3) ; break ;
+                  case 3: xo.rot4(4*f, 4*f+1, 4*f+2, 4*f+3) ; break ;
```

**Also update the streaming path.** `src/brobdicube.cpp:343–391` rebuilds
strips for streaming (not-in-memory) solves. `apply_fr_normfr` there
deliberately reproduces `normalizefacerots`'s right-rotate convention,
separately from `dosolve`'s left rotate. The README calls this "Correct
two-convention rotation … must not be conflated". After fixing
`normalizefacerots`, that function must switch to the `std::rotate`
convention too, or streaming solves will diverge from in-memory ones. I only
tested the in-memory path (`-t 1`).

---

## Why brobdicube's own checks don't catch these

- **What `-s` does.** `-s` (`fast = 0`, `src/brobdicube.cpp:973`) executes
  every block with real `domove` calls on the in-memory cube. `checkrow`
  (`src/xcube.h`; called at `src/brobdicube.cpp:403, 508, 713, 757`) then
  checks that cube. For both triggers above, `-s` exits 0 with no complaint
  while the printed solution fails the independent replay.
- **Why that passes for bug 2.** The in-memory cube goes through the same
  mis-directed `normalizefacerots()` mid-solve. Every later move is applied to
  that consistently-wrong picture, so the final internal state is "solved".
- **Why that passes for bug 1.** Centre centres are tracked only in the
  inverted `threecube::centercenters`. `checkrow` checks the centre orbits
  (`rows`), not `ccs`.

**The fixes don't break `-s`.** The patched build passes `-s` (exit 0, no
errors) on every triggering scramble and on all 24 random scrambles, and its
printed solutions also pass the independent replay (24/24). The unpatched build
passes `-s` on the same inputs, so `-s` can't tell the two builds apart.

**What would catch them.** An end-to-end check that replays *the original
scramble plus the printed moves* on a fresh cube:
- It must never call `normalizefacerots()` mid-sequence.
- It must read everything through `get()`, with centre centres from `ccs`.
- It must check wings and corners too.

Even brobdicube's own `domove` would be enough for such a replay, since both
bugs sit in the mid-solve normalization and the extra tracker, not in
`domove`'s sticker updates. An independent simulator such as
`full/brobcheck.cpp` is stronger.

## Files

| file | purpose |
|---|---|
| `full/brobcheck.cpp` | replay scramble + brobdicube `-o` output on the independent simulator; report wrong stickers by type |
| `full/brobcompare.sh`, `full/brobcompare.txt` | original vs patched on the triggering scrambles and 24 random scrambles |
| `full/cctest.cpp` | `ccs` vs `threecube::centercenters` after each middle-layer turn |
| `full/facetest.cpp`, `full/layouttest.cpp` | how the `normalizefacerots` / `get()` mismatch first showed up (the orbit read-out needed face twist 3 after normalizing) |

`full/fullsolve` avoids both bugs without changing brobdicube. It orients
centre centres from `ccs` before `solvecorners()` and syncs `tc`, never calls
`normalizefacerots()`, and reads orbits in `get()`'s convention
(`full/layout.h: readOrbit`).
