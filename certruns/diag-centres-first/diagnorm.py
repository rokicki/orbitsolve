import sys, os, re
# rewrite depth-3 placeholders as depth 2 and combine adjacent turns of one layer
src, dst = sys.argv[1], sys.argv[2]
os.makedirs(dst, exist_ok=True)
amt = {'': 1, '2': 2, "'": 3}
name = {1: '', 2: '2', 3: "'"}
tok = re.compile(r"^([1-9]?)([ULFRBD])(['2]?)$")
changed = 0
for fn in sorted(os.listdir(src)):
    if not (fn.startswith('w') and fn.endswith('.txt')): continue
    out = []
    for line in open(os.path.join(src, fn)):
        line = line.rstrip('\n')
        st = []
        for t in line.split():
            m = tok.match(t)
            d = m.group(1) or '1'
            if d == '3': d = '2'
            if st and st[-1][0] == d and st[-1][1] == m.group(2):
                a = (st[-1][2] + amt[m.group(3)]) % 4
                st.pop()
                if a: st.append((d, m.group(2), a))
            else:
                st.append((d, m.group(2), amt[m.group(3)]))
        new = ' '.join(('' if d == '1' else d) + f + name[a] for d, f, a in st)
        changed += new != line
        out.append(new)
    open(os.path.join(dst, fn), 'w').write(''.join(l + '\n' for l in out))
print('lines changed', changed)
