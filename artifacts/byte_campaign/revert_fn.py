#!/usr/bin/env python3
"""revert_fn.py <src.c> <fn> [<fn>...]: restore function bodies from HEAD, leave the rest."""
import re
import subprocess
import sys

path = sys.argv[1]
head = subprocess.check_output(["git", "show", "HEAD:" + path]).decode("utf-8", errors="surrogateescape")
cur = open(path, encoding="utf-8", errors="surrogateescape", newline="").read()


def span(s, fn):
    m = re.search(r"^[A-Za-z_][^\n;{}]*\b" + re.escape(fn) + r"\([^;{]*?\)\s*\{", s, re.M | re.S)
    if not m:
        sys.exit("function not found: " + fn)
    a = m.start()
    n = re.search(r"^\}\n", s[a:], re.M)
    return a, a + n.end()


for fn in sys.argv[2:]:
    ha, hb = span(head, fn)
    ca, cb = span(cur, fn)
    cur = cur[:ca] + head[ha:hb] + cur[cb:]
open(path, "w", encoding="utf-8", errors="surrogateescape", newline="").write(cur)
