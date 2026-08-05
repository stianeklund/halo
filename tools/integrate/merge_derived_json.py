#!/usr/bin/env python3
"""3-way merge for derived JSON dict caches during a rebase.

Several tracked files are generated caches keyed by function/addr
(kb_reg_baseline.json, vc71_scores.json, leaf_cache.json...). Both `main` and a
session branch append to them, so every rebase collides. The merge is
well-defined per key:

    ours == base       -> take theirs
    theirs == base     -> take ours
    ours == theirs     -> take either
    absent from base   -> an ADDITION by whichever side has it (NOT a deletion
                          by the other); records added on both sides merge
                          field-wise against an empty base
    deleted vs base    -> deletion wins
    otherwise          -> real conflict; report and exit non-zero, unless
                          --prefer picks a side (every use is printed)

Run with --self-test to verify the routing rules; it pins the two cases that
made purely additive changes look unresolvable, whose natural workaround
(resolving with --ours/--theirs) silently drops the other side's entries.

Usage: merge_derived_json.py <path> [--indent N] [--prefer ours|theirs]
       merge_derived_json.py --self-test

Run from the worktree mid-merge: it reads stages :1:/:2:/:3: of <path> from the
index and overwrites the working-tree file with the resolution.
"""
import json
import subprocess
import sys

PATH = sys.argv[1] if len(sys.argv) > 1 else ""
INDENT = 2
if "--indent" in sys.argv:
    INDENT = int(sys.argv[sys.argv.index("--indent") + 1])

MISSING = object()


def stage(n):
    cp = subprocess.run(["git", "show", f":{n}:{PATH}"],
                        capture_output=True, check=True)
    return json.loads(cp.stdout)


def merge(base, ours, theirs, where, conflicts, prefer=None, resolved=None):
    """Recursive 3-way merge of nested dicts; leaves compared by equality."""
    if not (isinstance(base, dict) and isinstance(ours, dict)
            and isinstance(theirs, dict)):
        if ours == base:
            return theirs
        if theirs == base:
            return ours
        if ours == theirs:
            return ours
        # Both sides re-measured and disagree. Only --prefer resolves this, and
        # every use is printed: a derived cache silently picking a winner is how
        # a stale measurement outlives the source it was measured against.
        if prefer:
            win = theirs if prefer == "theirs" else ours
            if resolved is not None:
                resolved.append((where, ours, theirs, win))
            return win
        conflicts.append((where, ours, theirs))
        return MISSING

    out = {}
    # base order first, then new keys (theirs before ours) for a minimal diff.
    keys = list(base)
    keys += [k for k in theirs if k not in base]
    keys += [k for k in ours if k not in base and k not in theirs]
    for k in keys:
        b = base.get(k, MISSING)
        o = ours.get(k, MISSING)
        t = theirs.get(k, MISSING)
        if o is MISSING and t is MISSING:
            continue
        if o is MISSING:                      # ours lacks it
            # base also lacked it => theirs ADDED a new key/field. That is not a
            # deletion by us, and reporting it as a conflict is what made a
            # purely additive schema change (`opnd_percent`) look unresolvable
            # 159 times.
            if b is MISSING:
                out[k] = t
                continue
            if t == b:
                continue                      # ours deleted it; deletion wins
            conflicts.append((f"{where}/{k}", "<deleted>", t))
            continue
        if t is MISSING:                      # theirs lacks it
            if b is MISSING:
                out[k] = o                    # ours ADDED it
                continue
            if o == b:
                continue                      # theirs deleted it; deletion wins
            conflicts.append((f"{where}/{k}", o, "<deleted>"))
            continue
        if b is MISSING:
            if o == t:
                out[k] = o
                continue
            # Added on both sides with different content. For records this is a
            # field-wise merge against an empty base, not a whole-record
            # conflict -- two sides adding the same function with different
            # metadata fields still union cleanly.
            nb = {} if isinstance(o, dict) and isinstance(t, dict) else MISSING
            v = merge(nb, o, t, f"{where}/{k}", conflicts, prefer, resolved)
            if v is not MISSING:
                out[k] = v
            continue
        v = merge(b, o, t, f"{where}/{k}", conflicts, prefer, resolved)
        if v is not MISSING:
            out[k] = v
    return out


def self_test():
    """Pin the routing rules, especially the two that silently lost data.

    Both bugs below produced a *fail-closed* wrong answer (exit 1, refusing to
    write), which is survivable -- but the operator's next move is to resolve
    with --ours or --theirs, and THAT drops the other side's entries. So a false
    conflict here is one step away from real data loss.
    """
    fails = []

    def check(name, got, want):
        if got != want:
            fails.append(f"{name}: got {got!r} want {want!r}")

    # BUG 1 (2026-08-05): ours adds a new field, theirs branched before it
    # existed. Reported as "theirs deleted it" 159x on vc71_scores.json's new
    # opnd_percent field. Must be taken as an addition, not a conflict.
    c = []
    got = merge({"f": {"score": 1}}, {"f": {"score": 1, "opnd": 9}},
                {"f": {"score": 1}}, "", c)
    check("ours-added-field", got, {"f": {"score": 1, "opnd": 9}})
    check("ours-added-field conflicts", c, [])

    # Symmetric: theirs adds a field ours never had.
    c = []
    got = merge({"f": {"score": 1}}, {"f": {"score": 1}},
                {"f": {"score": 1, "opnd": 9}}, "", c)
    check("theirs-added-field", got, {"f": {"score": 1, "opnd": 9}})
    check("theirs-added-field conflicts", c, [])

    # BUG 2: both sides add the same record with different fields -> union
    # field-wise, not a whole-record conflict.
    c = []
    got = merge({}, {"f": {"score": 1}}, {"f": {"ref": "synth"}}, "", c)
    check("both-added-record", got, {"f": {"score": 1, "ref": "synth"}})
    check("both-added-record conflicts", c, [])

    # A REAL disagreement must still fail closed without --prefer...
    c = []
    merge({"f": 1}, {"f": 2}, {"f": 3}, "", c)
    check("real-conflict detected", len(c), 1)
    # ...and be resolvable, and reported, with it.
    c, r = [], []
    got = merge({"f": 1}, {"f": 2}, {"f": 3}, "", c, "theirs", r)
    check("prefer-theirs value", got, {"f": 3})
    check("prefer-theirs no conflict", c, [])
    check("prefer-theirs reported", len(r), 1)

    # A genuine deletion relative to base is still honoured, not resurrected.
    c = []
    got = merge({"f": 1}, {"f": 1}, {}, "", c)
    check("theirs-deleted", got, {})
    check("theirs-deleted conflicts", c, [])

    # One-sided change routes to the side that changed it.
    c = []
    check("theirs-changed", merge({"f": 1}, {"f": 1}, {"f": 5}, "", c), {"f": 5})
    check("ours-changed", merge({"f": 1}, {"f": 5}, {"f": 1}, "", c), {"f": 5})
    check("one-sided conflicts", c, [])

    for f in fails:
        print(f"FAIL {f}")
    print(f"self-test: {'FAILED' if fails else 'all routing rules OK'}")
    return 1 if fails else 0


def main():
    if "--self-test" in sys.argv:
        return self_test()
    if not PATH:
        print(__doc__)
        return 2
    prefer = None
    if "--prefer" in sys.argv:
        prefer = sys.argv[sys.argv.index("--prefer") + 1]
        if prefer not in ("ours", "theirs"):
            print(f"--prefer must be ours|theirs, got {prefer!r}")
            return 2
    base, ours, theirs = stage(1), stage(2), stage(3)
    conflicts = []
    resolved = []
    merged = merge(base, ours, theirs, "", conflicts, prefer, resolved)
    if resolved:
        print(f"--prefer {prefer} resolved {len(resolved)} real conflict(s):")
        for w, o, t, win in resolved:
            print(f"  {w}: ours={o} theirs={t} -> {win}")
    if conflicts:
        print(f"UNRESOLVABLE ({len(conflicts)}):")
        for w, o, t in conflicts[:20]:
            print(f"  {w}\n    ours:   {o}\n    theirs: {t}")
        return 1
    with open(PATH, "w") as fh:
        json.dump(merged, fh, indent=INDENT)
        fh.write("\n")

    def count(d):
        for v in d.values():
            if isinstance(v, dict):
                return len(v)
        return len(d)

    print(f"merged OK: base={count(base)} ours={count(ours)} "
          f"theirs={count(theirs)} -> {count(merged)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
