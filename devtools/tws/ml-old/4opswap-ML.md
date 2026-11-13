# Training data: every 4op class with an orbit-axis fully swapped (85,388 classes)

The 4op tail has the same structure as the 3ax tail that the 3axswap-trained net just cleared:
**all 271 remaining 4op 23s** and 25,178 of the 39,257 22s have, in some orbit, an axis fully swapped
(all four F pieces of that orbit on B and all four B pieces on F, or the same for L and R).
`4opswap.scr` / `4opswap.sol` hold every symmetry class of that shape with the best solving sequence
we have (the certificate's line; many of the 21s and 22s are arena's optimal solves in the reduced
move set F R B L 2U2 3U2 2D2 3D2, otherwise not proven optimal).  By length: 12: 1, 13: 2, 14: 14,
15: 23, 16: 36, 17: 266, 18: 846, 19: 2,633, 20: 12,236, 21: 43,882, 22: 25,178, 23: 271.  Names
`4opswap-NNN-LNN`, the longest first.  Puzzle: `obl8op.tws`.  Every scramble and solution has been
replayed on it.

Use them to train on this shape, then go after the 271 23s (`4op23-t45.scr` minus the two already
lowered, or simply the L23 entries here, goal <= 22) and then the 22s.  Append any shorter line to
`~/mlcube/4opswap_live.txt`, one solving sequence per line, moves only.

**Superseded 2026-10-31 08:00 by axis-c9-ML.md** (current open lists at fold c9).
