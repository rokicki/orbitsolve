# lsplit.py N PREFIX: split stdin into files of N lines (PREFIXaa, PREFIXab, ...), buffered; much
# faster than macOS split -l
import sys, itertools, string
n, prefix = int(sys.argv[1]), sys.argv[2]
names = (''.join(p) for p in itertools.product(string.ascii_lowercase, repeat=2))
inp, out, left, buf = sys.stdin.buffer, None, 0, b''
while True:
    blk = inp.read(1 << 24)
    if not blk: break
    while blk:
        if out is None: out, left = open(prefix + next(names), 'wb'), n
        cnt = blk.count(b'\n')
        if cnt < left:
            out.write(blk); left -= cnt; blk = b''
        else:
            i = -1
            for _ in range(left): i = blk.index(b'\n', i + 1)
            out.write(blk[:i + 1]); out.close(); out = None; blk = blk[i + 1:]
if out: out.close()
