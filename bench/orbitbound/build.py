#!/usr/bin/env python3
"""Build the orbit-bound write-up: page.html with the certificate JSON files
(from devtools/orbitcert) and the explanatory text below inlined.

    python3 build.py OUT.html diag=diag_cert.json [mid=mid_cert.json] ...
"""
import json, sys

TEXT = {
    "diag": dict(
        tab="X-centres (diagonal)",
        headline="Every X-centre scramble can be solved in {bound} moves",
        lede="On a 5×5×5 the X-centres are the 24 centre pieces on the diagonals. Suppose everything else is solved "
             "and only the X-centres are scrambled. How many moves can it take to fix them, if you may only use moves "
             "that leave everything else alone? A simple argument gives 144. This page shows {bound} is enough and lets "
             "you check every step.",
        puzzle="<p>The highlighted stickers are one <b>orbit</b>: 24 pieces that turns only ever move among themselves. "
               "On a 5×5×5 they are the X-centres. On a 4×4×4 they are all of the centres. On any bigger cube, each "
               "diagonal ring of centres (2nd, 3rd, ... layer) is an orbit of the same kind, and the same algorithms "
               "work with the slice moved to the matching layer.</p>"
               "<p>Pieces of the same colour look identical, so a face is solved once it shows four pieces of its "
               "colour. We count <b>every turn as one move</b>: an outer face turn, a slice turn like <code>2R</code>, "
               "quarter or half.</p>"
               "<p><b>The easy bound.</b> Any arrangement can be solved with at most 12 three-cycles, and every "
               "three-cycle of X-centres has a commutator of at most 12 moves, so 12 × 12 = 144 moves always suffice.</p>",
        method="<p>We solve the orbit in three stages. In each stage we may only use algorithms that leave the faces already solved "
               "in place. They may still swap pieces within a solved face, since those pieces look alike.</p>"
               "<ol><li><b>Stage 1:</b> bring the four D pieces home.</li>"
               "<li><b>Stage 2:</b> bring the four U pieces home without disturbing D.</li>"
               "<li><b>Stage 3:</b> solve the four side faces together, keeping U and D.</li></ol>"
               "<p>Each stage has a manageable number of positions (stage 3 has 63 million). For each stage a computer "
               "tried every position and found the cheapest way to solve it as a chain of whole algorithms from the "
               "lists below. The hardest position of each stage sets that stage's bound, and the total bound is the sum.</p>"
               "<p>The algorithms come from a complete computer search of every X-centre algorithm up to 10 moves, plus "
               "some longer ones. Each stage list here is trimmed to just the algorithms needed to reach every position "
               "in the stated number of moves.</p>"
               "<p>Nothing is counted twice and nothing is assumed: the moves of consecutive algorithms might cancel, which would "
               "only make a real solution shorter.</p>",
        notes="<ul><li>This is an upper bound, not the exact answer. The true worst case is probably lower: the stages add up "
              "their worst cases even though one position is rarely worst for every stage, and cancellation between "
              "algorithms is ignored.</li>"
              "<li>Stages 1 and 2 check in your browser in seconds. Stage 3 visits 63 million positions with about 2,500 "
              "algorithm images each, which takes several hours in a browser; it was checked offline by the same method.</li>"
              "<li>The same three-stage plan (D, then U, then the four sides together) also gives the best bounds we "
              "found for the other centre orbits.</li></ul>",
    ),
}


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    out = sys.argv[1]
    data = []
    for arg in sys.argv[2:]:
        kind, fn = arg.split("=", 1)
        cert = json.load(open(fn))
        t = {k: v.format(bound=cert["bound"]) for k, v in TEXT[kind].items()}
        cert.update(t)
        cert["kind"] = kind
        data.append(cert)
    here = __file__.rsplit("/", 1)[0]
    page = open(here + "/page.html").read().replace("/*DATA*/", json.dumps(data, separators=(",", ":")).replace("</", "<\\/"))
    open(out, "w").write(page)
    print(f"{out}: {len(page) / 1e3:.0f} kB")


if __name__ == "__main__":
    main()
