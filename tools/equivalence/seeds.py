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

SAFE_INT_CORNERS = [
    0, 1, 2, 3, 4, 8, 16, 32,
    -1, -2, 42, 255,
]

SAFE_SIZE_CORNERS = [
    0, 1, 2, 3, 4, 8, 16, 32, 64, 128,
]

SAFE_FLOAT_CORNERS = [
    0.0, 1.0, -1.0,
    0.5, -0.5,
    2.0, -2.0,
    0.25, -0.25,
    1e-4, -1e-4,
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


def _is_size_like_param(name: str) -> bool:
    lower = name.lower()
    tokens = (
        'count', 'size', 'length', 'len', 'capacity',
        'max', 'limit', 'num', 'index', 'idx',
    )
    return any(tok in lower for tok in tokens)


def _sanitize_float(value: float) -> float:
    if not math.isfinite(value):
        return 0.0
    if value > 1e4:
        return 1e4
    if value < -1e4:
        return -1e4
    return value


def _sanitize_int(name: str, value: int) -> int:
    if _is_size_like_param(name):
        if value < 0:
            return 0
        if value > 128:
            return 128
    if value > 0x7FFFFFFF:
        return 0x7FFFFFFF
    if value < -0x80000000:
        return -0x80000000
    return value


def _sanitize_pointer_blob(blob: bytes) -> bytes:
    sanitized = bytearray()
    limit = min(len(blob), 0x400)
    i = 0

    while i + 4 <= limit:
        raw = blob[i:i + 4]
        value = struct.unpack('<f', raw)[0]
        sanitized += struct.pack('<f', _sanitize_float(value))
        i += 4

    if i < limit:
        sanitized += blob[i:limit]

    return bytes(sanitized).ljust(0x400, b'\x00')


def _sanitize_seed_vec(params: list, vec: list) -> list:
    sanitized = []
    for p, value in zip(params, vec):
        if p.is_pointer:
            if isinstance(value, (bytes, bytearray)):
                sanitized.append(_sanitize_pointer_blob(bytes(value)))
            else:
                sanitized.append(value)
        elif p.is_float or p.is_double:
            sanitized.append(_sanitize_float(float(value)))
        else:
            sanitized.append(_sanitize_int(p.name, int(value)))

    count_idx = None
    max_count_idx = None
    for i, p in enumerate(params):
        lower = p.name.lower()
        if count_idx is None and lower == 'count':
            count_idx = i
        if max_count_idx is None and lower in ('max_count', 'maximum_count'):
            max_count_idx = i

    if count_idx is not None and max_count_idx is not None:
        max_count = sanitized[max_count_idx]
        count = sanitized[count_idx]
        if max_count < 3:
            max_count = 3
        if count < 3:
            count = 3
        if max_count < count * 2:
            max_count = count * 2
        if max_count < 8:
            max_count = 8
        if max_count > 128:
            max_count = 128
        if count > max_count:
            count = max_count
        sanitized[count_idx] = count
        sanitized[max_count_idx] = max_count

    return sanitized


def generate_seeds(params: list, num_seeds: int = 100, base_seed: int = 0,
                   z3_seeds: list = None, safe_mode: bool = False) -> list[list]:
    """Generate num_seeds test vectors for the given parameter list.

    Each element in the returned list is one test vector: a list of values
    (int, float, or bytes for pointer args) parallel to params.

    Pointer parameters get a bytes value: 64 floats (256 bytes) filled
    deterministically from the seed.

    Priority order: z3_seeds first, then corner-case values, then random.
    """
    rng = random.Random(base_seed)

    all_seeds = []

    # Z3-generated branch-coverage seeds go first (highest priority)
    int_corners = SAFE_INT_CORNERS if safe_mode else INT_CORNERS
    float_corners = SAFE_FLOAT_CORNERS if safe_mode else FLOAT_CORNERS

    if z3_seeds:
        for sv in z3_seeds:
            if len(all_seeds) >= num_seeds:
                break
            if safe_mode:
                all_seeds.append(_sanitize_seed_vec(params, sv))
            else:
                all_seeds.append(sv)

    # Corner seeds: iterate over corner pool size
    corner_len = max(len(int_corners), len(float_corners))
    for ci in range(corner_len):
        vec = []
        for p in params:
            if p.is_pointer:
                # Fill with floats from corner set at index ci (cycling)
                blob = b""
                for fi in range(64):
                    fc = float_corners[(ci + fi) % len(float_corners)]
                    try:
                        blob += struct.pack('<f', fc)
                    except (struct.error, OverflowError):
                        blob += b'\x00\x00\x80\x7f'  # +inf
                vec.append(blob)
            elif p.is_float:
                vec.append(float_corners[ci % len(float_corners)])
            elif p.is_double:
                vec.append(float(float_corners[ci % len(float_corners)]))
            else:
                if safe_mode and _is_size_like_param(p.name):
                    vec.append(SAFE_SIZE_CORNERS[ci % len(SAFE_SIZE_CORNERS)])
                else:
                    vec.append(int_corners[ci % len(int_corners)])
        if safe_mode:
            all_seeds.append(_sanitize_seed_vec(params, vec))
        else:
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
                if safe_mode:
                    floats = []
                    for fi in range(64):
                        floats.append(float_corners[(ri + fi) % len(float_corners)])
                else:
                    floats = _random_floats(slot_rng, 64)
                blob = b""
                for fv in floats:
                    try:
                        blob += struct.pack('<f', fv)
                    except (struct.error, OverflowError):
                        blob += b'\x00\x00\x80\x7f'
                vec.append(blob)
            elif p.is_float:
                if safe_mode:
                    rv = float_corners[ri % len(float_corners)]
                else:
                    rv = _random_floats(rng, 1)[0]
                vec.append(rv)
            elif p.is_double:
                if safe_mode:
                    rv = float(float_corners[ri % len(float_corners)])
                else:
                    rv = rng.uniform(-1e15, 1e15)
                vec.append(rv)
            else:
                if safe_mode and _is_size_like_param(p.name):
                    vec.append(SAFE_SIZE_CORNERS[ri % len(SAFE_SIZE_CORNERS)])
                elif safe_mode:
                    vec.append(SAFE_INT_CORNERS[ri % len(SAFE_INT_CORNERS)])
                else:
                    vec.append(_random_ints(rng, 1)[0])
        if safe_mode:
            all_seeds.append(_sanitize_seed_vec(params, vec))
        else:
            all_seeds.append(vec)
        ri += 1

    return all_seeds[:num_seeds]
