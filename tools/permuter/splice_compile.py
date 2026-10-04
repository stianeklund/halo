#!/usr/bin/env python3
"""Compile a permuter candidate the way the raw-byte gate compiles the real TU.

The byte gate (tools/verify/raw_xbe_structural.py, _audit_tu) compiles the
WHOLE translation unit through vc71_verify.compile_vc71 at the function's
per-function optimization flags.  A standalone base.c cannot reproduce that:
/Ob1 inlining of static helpers, the fastcall/static_in_tu rewrites and the
TU's own declarations all change codegen.  This splices the candidate's
function body into a temporary copy of the real TU and compiles that copy,
so the unmodified base.c body scores exactly what the gate measures.

Only the body ({...}) is spliced.  The real signature stays, which keeps the
signature-keyed rewrites in compile_vc71 working.

The permuter's Compiler calls FastSplicer in-process (one per worker), so
modules load and the TU rewrites run once per worker, not once per candidate.

Usage (as the permuter compiler, via compile.sh):
    PERMUTER_SPLICE_SOURCE=<real .c> PERMUTER_SPLICE_FUNC=<name> \\
        splice_compile.py <candidate.c> -o <out.o>
    splice_compile.py --measure --source <real .c> --function <name> --address 0x...
"""

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import uuid
from collections import OrderedDict
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(REPO_ROOT / "tools" / "verify"))

import raw_xbe_structural as rxs  # noqa: E402
import vc71_verify as vc71  # noqa: E402


def _mask(text: str) -> str:
    """Blank comments and string/char literals, preserving offsets."""
    out = list(text)
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j == -1 else j + 2
        elif text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j == -1 else j
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, n)
        else:
            i += 1
            continue
        for k in range(i, j):
            if out[k] != "\n":
                out[k] = " "
        i = j
    return "".join(out)


def _match(masked: str, pos: int, open_c: str, close_c: str) -> int:
    level = 0
    for i in range(pos, len(masked)):
        if masked[i] == open_c:
            level += 1
        elif masked[i] == close_c:
            level -= 1
            if level == 0:
                return i
    return -1


def body_span(text: str, func: str) -> tuple[int, int] | None:
    """[start, end) of func's definition body, braces included."""
    masked = _mask(text)
    for m in re.finditer(r"\b%s\s*\(" % re.escape(func), masked):
        close = _match(masked, m.end() - 1, "(", ")")
        if close == -1:
            continue
        rest = masked[close + 1:]
        brace = close + 1 + (len(rest) - len(rest.lstrip()))
        if brace < len(masked) and masked[brace] == "{":
            end = _match(masked, brace, "{", "}")
            if end != -1:
                return brace, end + 1
    return None


def function_opt(source: Path, func: str) -> str:
    """The flags the byte gate compiles this function at."""
    return rxs._function_opt(source, func)


def compile_spliced(candidate_c: Path | None, source: Path, func: str, out: Path) -> bool:
    """Compile source with func's body replaced by candidate_c's (None = unmodified)."""
    text = source.read_text(encoding="utf-8", errors="surrogateescape")
    if candidate_c is not None:
        cand = candidate_c.read_text(encoding="utf-8", errors="surrogateescape")
        cspan = body_span(cand, func)
        sspan = body_span(text, func)
        if cspan is None or sspan is None:
            print("splice_compile: %s body not found in %s" % (
                func, candidate_c if cspan is None else source), file=sys.stderr)
            return False
        text = text[:sspan[0]] + cand[cspan[0]:cspan[1]] + text[sspan[1]:]
    # Next to the real source so relative includes resolve; unique per call
    # because the permuter compiles candidates in parallel.
    spliced = source.parent / (".permuter_splice_%s_%s" % (uuid.uuid4().hex[:12], source.name))
    try:
        spliced.write_text(text, encoding="utf-8", errors="surrogateescape")
        out.parent.mkdir(parents=True, exist_ok=True)
        ok = vc71.compile_vc71(spliced, out, opt=function_opt(source, func))
    finally:
        spliced.unlink(missing_ok=True)
    return ok and out.exists() and out.stat().st_size > 0


def _wsl_path(win: str) -> Path:
    """Inverse of vc71_verify.wsl_to_win for drive paths."""
    if re.match(r"^[A-Za-z]:\\", win):
        return Path("/mnt/%s/%s" % (win[0].lower(), win[3:].replace("\\", "/")))
    return Path(win)


class FastSplicer:
    """compile_spliced without the per-call Python work.

    One gate-path compile_vc71 call is recorded: the rewritten TU text it
    hands to CL.exe and the exact command line, including the per-process
    fastcall decl.h shadow directory.  Each later compile splices the new
    body into that rewritten text and reruns the recorded command.  The
    rewrites only match top-level signatures, so they do not depend on the
    body.  run.py's baseline guard checks this: base.o is compiled through
    this fast path and must score exactly what a gate-path compile scores.
    """

    CACHE_ENTRIES = 256

    def __init__(self, source: Path, func: str):
        self.source = source.resolve()
        self.func = func
        text = self.source.read_text(encoding="utf-8", errors="surrogateescape")
        seen = {}
        real_run = vc71.subprocess.run

        def recording_run(cmd, *args, **kwargs):
            if isinstance(cmd, list) and str(cmd[0]).lower().endswith("cl.exe"):
                seen["cmd"] = list(cmd)
                seen["text"] = _wsl_path(cmd[-1]).read_text(
                    encoding="utf-8", errors="surrogateescape")
            return real_run(cmd, *args, **kwargs)

        out = self.source.parent / (".permuter_template_%s.obj" % uuid.uuid4().hex[:12])
        spliced = self.source.parent / (".permuter_splice_%s_%s" % (
            uuid.uuid4().hex[:12], self.source.name))
        vc71.subprocess.run = recording_run
        try:
            spliced.write_text(text, encoding="utf-8", errors="surrogateescape")
            ok = vc71.compile_vc71(spliced, out, opt=function_opt(self.source, func))
        finally:
            vc71.subprocess.run = real_run
            spliced.unlink(missing_ok=True)
            out.unlink(missing_ok=True)
            vc71.obj_stamp_path(out).unlink(missing_ok=True)
        if not ok or "cmd" not in seen:
            raise RuntimeError("gate-path compile of %s failed" % self.source)
        self.cmd = seen["cmd"]
        self.template = seen["text"]
        self.span = body_span(self.template, func)
        if self.span is None:
            raise RuntimeError("%s body not found in the rewritten TU" % func)
        self._cache = OrderedDict()

    def compile_body(self, body: str, out: Path) -> bool:
        """Compile the TU with func's body replaced by body (braces included)."""
        key = hashlib.sha256(body.encode("utf-8", "surrogateescape")).digest()
        cached = self._cache.get(key)
        if cached is not None:
            self._cache.move_to_end(key)
            out.write_bytes(cached)
            return True
        text = self.template[:self.span[0]] + body + self.template[self.span[1]:]
        tmp = self.source.parent / (".permuter_fast_%s_%s" % (
            uuid.uuid4().hex[:12], self.source.name))
        obj = tmp.with_suffix(".obj")
        cmd = list(self.cmd)
        cmd[-1] = vc71.wsl_to_win(tmp)
        cmd = [("/Fo" + vc71.wsl_to_win(obj)) if str(a).startswith("/Fo") else a
               for a in cmd]
        try:
            tmp.write_text(text, encoding="utf-8", errors="surrogateescape")
            r = subprocess.run(cmd, capture_output=True)
            if r.returncode != 0 or not obj.exists() or obj.stat().st_size == 0:
                return False
            data = obj.read_bytes()
        finally:
            tmp.unlink(missing_ok=True)
            obj.unlink(missing_ok=True)
        out.write_bytes(data)
        self._cache[key] = data
        if len(self._cache) > self.CACHE_ENTRIES:
            self._cache.popitem(last=False)
        return True

    def compile_candidate(self, candidate_text: str, out: Path) -> bool:
        span = body_span(candidate_text, self.func)
        if span is None:
            return False
        return self.compile_body(candidate_text[span[0]:span[1]], out)


_splicers = {}


def splicer(source: Path, func: str) -> FastSplicer:
    """Per-process FastSplicer, built on first use."""
    key = (str(Path(source).resolve()), func)
    if key not in _splicers:
        _splicers[key] = FastSplicer(Path(source), func)
    return _splicers[key]


def raw_metrics(obj: Path, func: str, address: int) -> dict | None:
    """The gate's aligned_byte_match record for func in obj, or None."""
    try:
        record = rxs.audit(obj, func, address)
    except Exception as exc:  # NotComparable and parse errors
        print("splice_compile: audit failed: %s" % exc, file=sys.stderr)
        return None
    aligned = record.get("aligned_byte_match") or {}
    if aligned.get("status") != "scored":
        return None
    return aligned


def main(argv=None) -> int:
    argv = list(sys.argv[1:] if argv is None else argv)
    if argv and argv[0] == "--measure":
        ap = argparse.ArgumentParser()
        ap.add_argument("--measure", action="store_true")
        ap.add_argument("--source", type=Path, required=True)
        ap.add_argument("--function", required=True)
        ap.add_argument("--address", type=lambda v: int(v, 0), required=True)
        ap.add_argument("--object", type=Path, required=True)
        args = ap.parse_args(argv)
        if not compile_spliced(None, args.source.resolve(), args.function, args.object):
            return 1
        metrics = raw_metrics(args.object, args.function, args.address)
        if metrics is None:
            return 1
        print(json.dumps({k: v for k, v in metrics.items()
                          if not isinstance(v, (list, dict))}, sort_keys=True))
        return 0
    if len(argv) != 3 or argv[1] != "-o":
        print("usage: splice_compile.py <candidate.c> -o <out.o>", file=sys.stderr)
        return 1
    source = os.environ.get("PERMUTER_SPLICE_SOURCE")
    func = os.environ.get("PERMUTER_SPLICE_FUNC")
    if not source or not func:
        print("splice_compile: PERMUTER_SPLICE_SOURCE/FUNC not set", file=sys.stderr)
        return 1
    text = Path(argv[0]).read_text(encoding="utf-8", errors="surrogateescape")
    ok = splicer(Path(source), func).compile_candidate(text, Path(argv[2]))
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
