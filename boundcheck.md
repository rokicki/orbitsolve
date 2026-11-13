# How boundcheck proves a boundary bound

`devtools/boundcheck` (437 lines of C++) checks a **boundary certificate** between two consecutive stages, A
and B, of the oblique-pair decomposition.  It establishes that every position is taken through both stages
in at most X moves, where X is less than the plain sum max dA + max dB.  This note explains what it proves
and why its checks are enough.  axbcheck does the same for 3ax/4op; it is described at the end.

The reader is assumed to know the stage decomposition: solve some pieces, then others, each stage with
explicit sequences that keep what earlier stages solved.

## 1. Objects

**Pieces and colourings.**
- The pair is two oblique orbits, a and b, of 24 centre facelets each: 48 facelets in all.
- A *group* is (orbit, home face): 12 groups of 4 pieces.
- A position of the pair is a *colouring*: each of the 48 facelets carries the group of the piece on it.
- A stage only tracks some groups; the others are written -1 and never inspected.
- Every move (face f, layer depth 1..3, amount 1..3) permutes the 48 pair facelets.  The tables
  `mp[f][d][a][k]` (where the piece at facelet k goes) come from certcube.h: plain 3-D geometry of the
  8x8x8, also used by certcheck, boundx and axbcheck.

**Stages.**
- A stage directory is named `<S>..<T>`: S is the set of groups already solved at its start, T the
  groups it solves.  Example: `none..aUbU` (1ud) solves both orbits' U groups from nothing.
  `aUbU..aDbD` (2ud) then solves the D groups with U kept.
- A *stage state* is where T's pieces are, among the facelets not taken by S's groups.
- boundcheck requires B to start where A ends: B.S = A.S + A.T.

**Certificates.**
- A stage certificate is a list of solving sequences, one per symmetry class.
- certcheck has already verified each stage on its own: coverage, legality, keeping S, and side
  effects on N = 6..10.
- `dA(x)` and `dB(y)` are the length of the line that solves a state, or the image of a line under a
  stage symmetry.

**Plain sum.**  Every position is solved by A's line for its A state, followed by B's line for the B state
that results.  So the total is at most max dA + max dB.  For UD that was 17 + 18 = 35.

**Boundary certificate.**
- The plain sum is pessimistic: it adds A's worst case to B's worst case, though they rarely occur
  together.
- The boundary file lists *blocks*.  A block is several solving sequences for one A state u: its first
  line L, and alternatives P1, P2, ...
- The solver chooses among them by looking at the whole position, i.e. at where the B pieces will
  end up.

## 2. The claim

> For every position Q of the pair (A's solved groups home), some A-solving sequence P is legal for
> stage A, and |P| + dB(P(Q)) <= X.  P is either the A certificate's line or a line of the block for
> Q's A state.  P(Q) is the B state that P leaves, and dB(P(Q)) is the B certificate's line for it.

Then A's sequence followed by B's line solves A and B in at most X moves, keeping everything both stages must
keep.

## 3. The key fact: one block, one B state per position

Fix an A state u and one sequence L that solves it.  Consider all positions Q whose A state is u.  They
differ only in where B's pieces are.

**Lemma.** The map Q -> (the B state that L leaves) is a bijection from {positions with A state u} onto
{all B states}.

**Proof.**
- L solves u, so after L every piece of A's groups (S and T) is home.  B's pieces therefore occupy
  exactly B's free facelets, so the result is a B state.
- L is a permutation of facelets.  Given a B state s, the position is recovered as Q[k] = s[L(k)] for
  every facelet k not holding an A piece, and Q[k] = u[k] for those that do.
- That recovered Q has A state u, and L takes it to s.  Distinct Q give distinct s.

So "every position with A state u" is the same as "every B state s, pulled back through L".  This is what
makes the check finite and exhaustive: for each block, boundcheck enumerates B states rather than positions.
boundcheck builds this pulled-back position explicitly (`J0` in the code) and plays each alternative on it.

## 4. The four checks

**Check 1: both stages are complete** (`Stage::init`).
- Every line of A and of B is parsed, and every move must have depth 1..3.
- Every line is run backwards from solved to get its state.
- Each line's state, under every symmetry of the stage, is ranked into a dense table that keeps the
  shortest line for each state.
- Every one of the `total` states must receive a line: C(24,4)^2 = 112,911,876 for 1ud,
  C(20,4)^2 = 23,474,025 for 2ud.  Any state without a line fails the run.
- So dA and dB are defined everywhere and are real line lengths.
- The symmetries are the 48 cube symmetries built from the facelet coordinates; a symmetry is a
  stage symmetry only if it maps S to S and T to T.
- Check 1 also requires A's symmetries to be a subset of B's, so that a block's proof for u carries
  over to every symmetric image of u (section 5).

**Check 2: every block line is a legal A-solving sequence for its state** (per block).
- The block's A state u is defined as its first line run backwards from A-solved.
- For every line P of the block, its facelet map D_P is computed, and for every tracked facelet k the
  piece's destination D_P(k) must lie in the piece's own group: P solves u and keeps A's solved groups.
- Side effects are replayed on N = 6..10, as in certcheck: every centre facelet outside the pair and
  the diagonals must keep its colour.  Under `--keep ud`: every such facelet on U stays on U, and every
  facelet off U and D stays off them.
- One failing line fails its block.

**Check 3: every A state that needs a block has one.**
- An A state x needs a block when dA(x) + max dB > X.  The plain sum covers all the others.
- Every block marks its state u and all its symmetric images in a bitmap.
- The check then walks every A state with dA > X - max dB and fails if any is unmarked.
- For UD 34 that is all 42,244 states at dA = 17.

**Check 4: every position of every block is within X** (the heart).
- For the block's first line L (length cL), every B state s with cL + dB(s) > X is a position that L
  alone does not bring within X.  These are the only positions that need anything; all others are
  within X through L.
- boundcheck enumerates all B states once (`B.forEach`, every placement of B's target pieces on B's
  free facelets) and keeps those with dB greater than X - maxL, grouped by distance (`byD`).
  maxL is at least every block's |L|, so every s a block needs is in the list.
- For each such s:
  1. Rebuild the whole position Q (section 3).
  2. For each line P of the block, shorter than X, play P on Q.
  3. Read the B state P leaves and look up dB.
  4. s counts as solved as soon as some P has |P| + dB <= X.
- Any s left unsolved fails the block, and the count is printed.
- **Frame check.**  For P = L, the B state reached must have distance exactly dB(s), since it is s
  itself.  This tests the position rebuild and the facelet-map conventions on every enumerated state;
  any disagreement is counted as a frame error, and frame errors fail the run.

**PASS** requires all of: no bad block (checks 2 and 4), no A state without a block (check 3), no frame
error, and both stages complete (check 1).

## 5. Why the four checks prove the claim

Take any position Q, and let x be its A state.

1. **If dA(x) + max dB <= X:** A's certificate line for x (check 1: it exists) followed by B's line
   gives at most dA(x) + dB(.) <= X.
2. **Otherwise,** by check 3 some stage symmetry g maps x to the state u of a block.
   - g(Q) is a position with A state u.  By the lemma it corresponds to the B state s that L leaves.
   - If |L| + dB(s) <= X, L works.  If not, s was enumerated in check 4, and some line P of the
     block has |P| + dB(P(g(Q))) <= X.
   - P is a legal A-solving sequence for u (check 2), so g^-1(P), the line with every move
     conjugated by g, is a legal A-solving sequence for x = g^-1(u).
   - B's distances are invariant under g: g is a stage symmetry of B too (check 1), and B's table was
     built over all symmetric images.  So dB(g^-1(P)(Q)) = dB(P(g(Q))), and the total is at most X.
   - The side conditions are symmetric under stage symmetries, so g^-1(P) keeps them as P does.
3. Either way, B's line then solves B from A's goal, keeping A's groups and its own side conditions
   (certcheck on B).  A's sequence and B's line each keep the non-pair centres, so their concatenation
   does too.

## 6. Every shortcut errs towards FAIL

Every simplification in the checker can only reject a valid certificate, never accept a bad one:

- Alternatives of length >= X are skipped (they could only work with dB = 0).
- A block with any bad line is rejected whole, even if its other lines would suffice.
- The B states checked are a superset of those needed: everything beyond X - maxL is enumerated, and
  each block uses those beyond X - |L|.
- Distances come from certificate lines, which are real solutions.  If they were shorter than the
  lines, checks 1 and 4 would be optimistic; they are not, because each distance is a measured line
  length.
- Nothing from the search tools is trusted.  boundx's choices, orders and pools are irrelevant:
  boundcheck re-derives every state, map and distance from the line text alone.

## 7. What it does trust, and how that is covered

- **certcube.h:** the move permutations and the symmetries, from facelet coordinates.  certcheck,
  boundx and axbcheck share this code, so an error there could be common to all of them.  Two
  independent cross-checks:
  - devtools/udindcheck.py and axindcheck.py re-verify every stage certificate (legality, keeping the
    other orbits, stage start, and coverage by counting) using only twsearch's .tws puzzle definitions.
    They PASS on all four stages of obl23pair-axis-2026-11-04.
  - The frame check above ties boundcheck's maps to its own stage tables on every enumerated state.
  - A GPU re-implementation (TODO.md item L, devtools/gpu/gpubound.mm) re-derives check 4 with
    different code, and agrees on UD 34.
- **Side effects** are checked on 6x6x6 .. 10x10x10, as certcheck does.  The obliques of larger cubes
  behave like those of these sizes.  That argument is the same one the stage certificates rely on.
- **The C++ compiler and machine.**  The same run gives the same counts on rerun, and boundx and
  boundcheck agree on every count they share.  The table-load bug in twsearch is irrelevant here:
  boundcheck uses no twsearch tables.

## 8. Negative tests

A checker that never fails proves nothing, so we break certificates on purpose.

- **axbcheck (3ax/4op), 2026-11-01:** blocks cut to their first line FAIL (positions unsolved), and a
  dropped block FAILs (16 states without a block).
- **boundcheck (UD 34), 2026-11-04** (bcert/ud34neg/neg.sh; the first 40 UD 34 blocks, so check 3
  always reports the other states as without a block; the block counts are the test):
  - **base**, the 40 good blocks: 0 bad blocks, 194,016,513 colourings tried, 0 not solved, 0 frame
    errors.  41,924 states without a block (those not in the sample).
  - **l0only**, one block cut to its first line: that block is reported with 2,299,768 colourings not
    solved within 34.  That is exactly the number of 2ud positions at 18: L alone (17 moves) cannot
    take any of them within 34.
  - **badmove**, one move of one alternative changed: "1 lines not solving the block's state".  The
    whole block is rejected, so its 8 symmetric states also appear without a block (41,932).
- **GPU check (UD 34), 2026-11-04**, by the other session: dropping line 1 or line 9 of a block, or
  dropping a whole block, or running X = 33 on the 34 blocks, each FAILs; see journal.md 09:12.

## 9. UD 34 (2026-11-04, dry run)

| | |
|---|---|
| Stages | 1ud = none..aUbU (14,119,851 lines, at most 17), 2ud = aUbU..aDbD (2,936,935 lines, at most 18), the stages of ~/orbitsolve-certsafe/obl23pair-axis-2026-11-04 |
| Blocks | 5,294 (one per 1ud 17-class), from boundx --deep 1 --slack 5, 304 s |
| boundcheck | 25,532,454,577 colourings tried, 0 not solved; 0 bad blocks; frame errors 0; 42,244 A states with dA > 16, 0 without a block; **PASS** (872 s, 1.2 GB) |
| Evidence | ~/orbitsolve-certsafe/obl23pair-UD34-evidence-2026-11-04 (blocks, logs, the checker's source, snapshot SHA-256) |

So 1ud + 2ud <= 34, and with the checked 3ax/4op 42, P <= 76.

## 10. axbcheck (3ax/4op)

Same structure, with stages that are not "groups onto faces":
- 3ax puts every side piece on its own axis.
- 4op solves the six faces.
- States are whole colourings of the 48 pair facelets: 12,870^2 for 3ax, 70^4 for 4op.
- 16 stage symmetries, those keeping the U-D axis.

Its checks 1-4 correspond one to one: stage coverage, block lines legal and solving, every 3ax state with
d3 > X - max d4 has a block, and per block every bad 4op state solved by some line, with the frame check.
Results: 42 PASS (3.83e11 positions, 0 not solved); 41 a 1/50 sample with every block valid (6.5e10
positions, 0 not solved); the full 41 check is still to run.
