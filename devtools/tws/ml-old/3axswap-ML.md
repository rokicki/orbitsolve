# Training data: every 3ax class with one orbit fully axis-swapped (1,703 classes)

All 7 remaining 3ax 25s and 516 of the 545 24s share one shape: **one orbit is fully axis-swapped**
(its 8 F/B pieces all on L and R, its 8 L/R pieces all on F and B); the other orbit is mostly in
place.  `3axswap.scr` / `3axswap.sol` hold every symmetry class of that shape (1,703 classes,
about 25,740 positions) with the best solving sequence we have for each (the certificate's line;
not proven optimal).  By length: 16: 1, 20: 36, 21: 112, 22: 433, 23: 599, 24: 515, 25: 7.  Names
`3axswap-NNN-LNN`.  Puzzle: `obl8ax.tws`.  Every scramble and solution has been replayed on it.

Use them to train on this shape.  Any shorter line for any of them is welcome: append to
`~/mlcube/3axswap_live.txt`, one solving sequence per line, moves only.

**Superseded 2026-10-31 08:00 by axis-c9-ML.md** (current open lists at fold c9).
