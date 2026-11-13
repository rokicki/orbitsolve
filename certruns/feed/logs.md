# arena twsearch logs: how much has been processed

Each log is append-only.  "processed" is the byte size of the log (every line ends in a newline)
up to which its lines have been fed to the stages listed.  To bring over only the new part, on
arena:

    tail -c +$((SIZE + 1)) 7pairup-2.log > 7pairup-2.from-SIZE.log

and check the join first: `head -c SIZE 7pairup-2.log | tail -1` must equal the "last line"
below, and `head -c SIZE 7pairup-2.log | tail -c 1` must be a newline.  Feed the tail as a log
of its own (devtools/pairfeed.sh MODE TAIL.log STAGE...), then add its size to the entry here.

| log | processed (bytes) | lines | last line at that point | fed to (with merges) |
|---|---:|---:|---|---|
| 7pair.log | 1,123,669,075 | 28,124,646 | ` 2R' D' 3F' L2 2D' L2 3F D L2 2D 2R` | all four stages; complete (lengths to 11) |
| 7pairud.log | 2,846,489,829 | 62,441,267 | ` 3D 2L2 3D' F 3D R' 2D' R 3D' R' 2D F' 2L2` | aDbD, a ring, b ring; complete (lengths to 13) |
| 7pairudb.log | 119,746,902 | 2,631,220 | ` 2F2 R 2F2 L' 2D 2U R2 2D' 3B2 2U' F2 2U 3B2 2U` | b ring, aDbD; complete (lengths to 14) |
| 7pairudb-2.log | see processed.tsv (still growing) | | | b ring (hourly tails, G 6) |
| 7pairud-2.log | 12,007,884,518 (final: stopped on arena 2026-10-19) | 247,096,688 | (end of file) | a ring (G 2), b ring (G 6) to the end; aDbD to 9,661,521,446 |
| 7pairup-2.log | see processed.tsv (final size 52,347,976,564: stopped on arena 2026-10-19) | | | aDbD (run15, run18; tails G 2) |
| 7pairupu-2.log | see processed.tsv (final size 1,410,987,175: stopped on arena 2026-10-15) | | | aDbD (run37, from 2026-10-20; G 2) |
| 7pair-2.log | not fed (1,123,663,174 bytes, stopped 2026-10-16) | | ` 2R' D' 3B' L2 2D' L2 3B D L2 2D 2R` | a rerun of 7pair.log (same opening and lengths, complete to 11): nothing new |

Line counts are the algorithms pairlogcheck parsed (one per line).  The 7pair*.log files hold
every sequence shorter than their longest length, so only the newest lines (at the top length)
are new.  The seen sets (*.seen) are not needed for tails.

Since 2026-10-19 the processed byte counts live in certruns/feed/processed.tsv (one line per log),
advanced by certruns/feed/pulltail.sh after each fed slice; the local copies of the logs are gone.
7pairudb-2.log is still growing (15s) and is pulled hourly by certruns/feed/udbloop.sh.
