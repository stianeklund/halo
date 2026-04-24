#!/usr/bin/env python3
"""Content-hash check for cmake generated-code invalidation.

Hashes the input files and compares with a stored stamp.  If the hash
differs (or the stamp is missing), writes the new hash and exits 0 —
cmake sees the stamp as updated and regenerates downstream outputs.
If the hash matches, the stamp is left untouched so cmake skips
regeneration.

Usage: python3 tools/check_gen_hash.py <stamp_path> <input_file> [<input_file> ...]
"""
import hashlib
import os
import sys


def hash_files(paths):
    h = hashlib.sha256()
    for p in sorted(paths):
        with open(p, "rb") as f:
            while True:
                chunk = f.read(1 << 16)
                if not chunk:
                    break
                h.update(chunk)
    return h.hexdigest()


def main():
    if len(sys.argv) < 3:
        print(f"usage: {sys.argv[0]} <stamp_path> <input_file> ...", file=sys.stderr)
        return 1

    stamp_path = sys.argv[1]
    input_files = sys.argv[2:]
    current_hash = hash_files(input_files)

    old_hash = None
    if os.path.isfile(stamp_path):
        with open(stamp_path) as f:
            old_hash = f.read().strip()

    if old_hash == current_hash:
        return 0

    with open(stamp_path, "w") as f:
        f.write(current_hash + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
