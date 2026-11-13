# Keeping the laptop and armory in sync

Hub: https://github.com/rokicki/orbitsolve (public), branch `main`.  Both machines reach it outbound; armory has no
ingress, so nothing ever connects to it directly.

## What is in git

Notes (`*.md`), scripts, sources (`devtools/`, `brobfix/`, ...), small results, certificate manifests.  Not in git:
big data (stage tables, block files, pools, `/var/tmp/orbitsolve`), logs, binaries (rebuild from the sources),
`repo/` (its own git), `dist/gullicube/`.  `.githooks/pre-commit` rejects files over 5 MB and binaries; enable it
on a new clone with `git config core.hooksPath .githooks`.

## Routine (each agent)

    git pull --rebase            # before starting work
    ... work; record results in journal.md / scoreboard.md as usual ...
    git add -A && git commit -m "..." && git push    # after each recorded result (or at least every few hours)

If a push is rejected, `git pull --rebase` and push again.  `journal.md` merges by union (both machines' appended
entries are kept; they are timestamped).  Other notes merge normally; record each result on the machine that
produced it, so two machines rarely edit the same lines.  Start journal entries with the machine name when it
is not the laptop, e.g. `11-14 10:00 [armory] ...`.

## Big data

Stays on the machine that made it.  Only small outputs travel (scoreboard lines, sequence lists, summaries).  If a
big file must move, a run on that machine writes it in a small, plain form, or the user carries it.

## First clone on armory

    git clone https://github.com/rokicki/orbitsolve.git ~/orbitsolve   # or into the existing directory:
    cd ~/orbitsolve && git init && git remote add origin https://github.com/rokicki/orbitsolve.git \
      && git fetch && git checkout -t origin/main
    git config core.hooksPath .githooks
