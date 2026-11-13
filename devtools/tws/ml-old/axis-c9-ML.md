# Axis split: open classes at c9 (2026-10-31 08:00)

Current tables (fold c9): 4op <= 23 with 60 at 23 and 16,237 at 22; 3ax <= 24 with 254 at 24 and 8,697 at 23.
These lists replace 4op23-t52, 3ax24-t16 and the older ones: classes already lowered are gone.
Full move set (the obl8op.tws / obl8ax.tws moves), as before.

| file (.scr / .sol) | classes | goal | append solving lines to |
|---|---|---|---|
| 4op23-c9 | 60 | <= 22 | ~/mlcube/4op23_live.txt |
| 3ax24-c9 | 254 | <= 23 | ~/mlcube/3ax24_live.txt |
| 4op22-c9 | 16,237 | <= 21 | ~/mlcube/4op22_live.txt (new) |
| 3ax23-c9 | 8,697 | <= 22 | ~/mlcube/3ax23_live.txt (new) |

Priority: 4op23-c9 and 3ax24-c9 first (each clears a stage top: plain sum 47 -> 46), then the 22s/23s
(they set the boundary-pass cost).  Arena is running 4op 23s with the 12-move set
F R B L 2U2 3U2 2D2 3D2 2F2 2R2 2B2 2L2 and lowering most of them, so 3ax24-c9 is where ML matters most.

Lines are folded hourly by /var/tmp/orbitsolve/op/foldloop (all four live files, plus 4opswap_live and
3axswap_live).  Please also put every result you get (even ones not below the goal) in the live file:
lines that are not shorter cost nothing, and about 9,000 results found only in runs/*/*.json were folded
by hand at c9 (4op 23s 65 -> 60).

**Superseded 2026-10-31 08:40 by axis-current-ML.md** (rewritten after every hourly fold; lists only classes still open).
