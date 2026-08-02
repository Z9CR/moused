#!/usr/bin/env python3
"""Convert `NAME = VALUE` enum lines into `X(NAME, VALUE)` X-macro lines.

*no // or /**/ is allowed in input str*

Usage   1. `python3 convert2xmacro.py {enum vals}`
        2. get result
"""

import sys


lns = sys.argv[1:]
ret = ''

for arg in lns:
    for ln in arg.splitlines():
        if not ln.strip():
            continue
        kv = ln.replace(' ', '').replace(',', '').split('=')
        if len(kv) != 2:
            print(f'skip: no "=" found in {ln!r}', file=sys.stderr)
            continue
        ret += f'X({kv[0]}, {kv[1]})\n'
    

print(ret, end='')
