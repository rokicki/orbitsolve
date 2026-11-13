#!/usr/bin/env python3
"""Echo only those 6x6x6 sequences whose every slice nets to zero twist.

Reads sequences (one per line) from the named files or from standard input.
A move is a face letter UDLRFB, optionally prefixed by a depth of 1, 2 or 3
(no prefix means the outer layer, depth 1) and optionally suffixed by 2 or '
for a half turn or a counterclockwise quarter turn.

Each depth/face pair is a distinct slice of the cube, so U, 2U and 3U are
tracked separately, as are U and D.  A line is echoed unchanged if every
slice it mentions has been turned a multiple of four quarter turns.
"""

import fileinput
import re
import sys

MOVE = re.compile(r"([123]?)([UDLRFB])([2']?)$")
AMOUNT = {"": 1, "2": 2, "'": 3}

for line in fileinput.input():
    twist = {}
    for move in line.split():
        m = MOVE.match(move)
        if not m:
            sys.exit("%s:%d: cannot parse move %s" %
                     (fileinput.filename(), fileinput.filelineno(), move))
        depth, face, suffix = m.groups()
        slice_ = (depth or "1") + face
        twist[slice_] = (twist.get(slice_, 0) + AMOUNT[suffix]) & 3
    if twist and not any(twist.values()):
        sys.stdout.write(line)
        sys.stdout.flush()
