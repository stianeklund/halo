#!/usr/bin/env python3
"""DEPRECATED alias — this tool was renamed `ab_check.py` ("gate" -> "check").

Kept only so an in-flight run that still invokes `ab_gate.py` doesn't break; it will
be removed shortly. Use `tools/equivalence/ab_check.py`.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ab_check import main  # noqa: E402

if __name__ == "__main__":
    sys.exit(main())
