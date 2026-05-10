"""Input seed generator for unicorn_diff.py.

Generates typed test vectors per parameter signature.  Each seed is a list of
Python values, one per parameter.  Pointer parameters receive a seed_index
value that is used by state.py to fill the scratch buffer deterministically.
"""

import math
import random
import struct


# ---------------------------------------------------------------------------
# Canonical seed sets per scalar type
# ---------------------------------------------------------------------------

INT_CORNERS = [
    0, 1, -1, 100, -100,
    0x7FFFFFFF,   # INT_MAX
    -0x80000000,  # INT_MIN
    0xFFFF,       # uint16 max
    0x7FFF,       # int16 max
    -0x8000,      # int16 min
    42, 255, 256, 1000, -1000,
    0x12345678,
]

FLOAT_CORNERS = [
    0.0, 1.0, -1.0,
    0.5, -0.5,
    2.0, -2.0,
    math.pi, -math.pi,
    1e-7, -1e-7,
    1e7, -1e7,
    float('inf'), float('-inf'),
    float('nan'),
    1.0 / 3.0,
    struct.unpack('<f', struct.pack('<I', 0xDEADBEEF))[0],  # bit-pattern chaos
]


def _pack_float32(v: float) -> int:
    """Return the bit-pattern of a float as a 32-bit int."""
    try:
        return struct.unpack('<I', struct.pack('<f', v))[0]
    except (struct.error, OverflowError):
        return 0x7F800000  # +inf


def _random_floats(rng: random.Random, n: int) -> list[float]:
    """Generate n random floats with varied exponents."""
    result = []
    for _ in range(n):
        # Mix: sometimes a normal float, sometimes a bit-pattern hack
        choice = rng.randint(0, 3)
        if choice == 0:
            result.append(rng.uniform(-1e6, 1e6))
        elif choice == 1:
            result.append(rng.uniform(-1.0, 1.0))
        elif choice == 2:
            bits = rng.getrandbits(32)
            try:
                v = struct.unpack('<f', struct.pack('<I', bits))[0]
                result.append(v)
            except Exception:
                result.append(0.0)
        else:
            result.append(rng.gauss(0, 100))
    return result


def _random_ints(rng: random.Random, n: int) -> list[int]:
    result = []
    for _ in range(n):
        choice = rng.randint(0, 2)
        if choice == 0:
            result.append(rng.randint(-0x80000000, 0x7FFFFFFF))
        elif choice == 1:
            result.append(rng.randint(0, 0xFFFF))
        else:
            result.append(rng.randint(-1000, 1000))
    return result


def generate_seeds(params: list, num_seeds: int = 100, base_seed: int = 0) -> list[list]:
    """Generate num_seeds test vectors for the given parameter list.

    Each element in the returned list is one test vector: a list of values
    (int, float, or bytes for pointer args) parallel to params.

    Pointer parameters get a bytes value: 64 floats (256 bytes) filled
    deterministically from the seed.

    The first min(len(INT_CORNERS), len(FLOAT_CORNERS)) seeds are the
    corner-case values; the rest are random.
    """
    rng = random.Random(base_seed)

    # Collect corner-case pool per param type
    # For pointer params, we generate byte blobs
    all_seeds = []

    # Corner seeds: iterate over corner pool size
    corner_len = max(len(INT_CORNERS), len(FLOAT_CORNERS))
    for ci in range(corner_len):
        vec = []
        for p in params:
            if p.is_pointer:
                # Fill with floats from corner set at index ci (cycling)
                slot_rng = random.Random(base_seed ^ (ci * 0x1234 + 1))
                blob = b""
                for fi in range(64):
                    fc = FLOAT_CORNERS[(ci + fi) % len(FLOAT_CORNERS)]
                    try:
                        blob += struct.pack('<f', fc)
                    except (struct.error, OverflowError):
                        blob += b'\x00\x00\x80\x7f'  # +inf
                vec.append(blob)
            elif p.is_float:
                vec.append(FLOAT_CORNERS[ci % len(FLOAT_CORNERS)])
            elif p.is_double:
                vec.append(float(FLOAT_CORNERS[ci % len(FLOAT_CORNERS)]))
            else:
                vec.append(INT_CORNERS[ci % len(INT_CORNERS)])
        all_seeds.append(vec)
        if len(all_seeds) >= num_seeds:
            break

    # Random seeds
    ri = 0
    while len(all_seeds) < num_seeds:
        vec = []
        for p in params:
            if p.is_pointer:
                slot_rng = random.Random(base_seed ^ (ri * 0xABCD + 7))
                floats = _random_floats(slot_rng, 64)
                blob = b""
                for fv in floats:
                    try:
                        blob += struct.pack('<f', fv)
                    except (struct.error, OverflowError):
                        blob += b'\x00\x00\x80\x7f'
                vec.append(blob)
            elif p.is_float:
                rv = _random_floats(rng, 1)[0]
                vec.append(rv)
            elif p.is_double:
                rv = rng.uniform(-1e15, 1e15)
                vec.append(rv)
            else:
                vec.append(_random_ints(rng, 1)[0])
        all_seeds.append(vec)
        ri += 1

    return all_seeds[:num_seeds]
