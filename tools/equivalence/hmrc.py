"""Write halo-memory-viewer `.halorec` (HMRC) recordings.

The inverse of `halorec_to_snapshot.parse_halorec`. Capturing in this format
means every trajectory is BOTH Python-diffable (halorec_ai_diff / trajectory_diff,
halorec_to_snapshot -> unicorn_diff) AND loadable/scrubbable/diffable in the
halo-memory-viewer GUI for free.

Container (gzip):
    magic "HMRC" | version u32=1 | name(u32 len + utf8) | frame_count u32
    per frame:   t f64 | region_count u32
                 per region: addr u32 | len u32 | bytes[len]

All little-endian. Region addresses are guest virtual addresses.
"""

import gzip
import struct
from pathlib import Path

FORMAT_MAGIC = b"HMRC"
FORMAT_VERSION = 1


def encode_halorec(name, frames):
    """Serialize to the raw (pre-gzip) HMRC byte string.

    frames: iterable of (t: float, regions: iterable of (addr: int, data: bytes)).
    Duplicate addresses within a frame are kept as-is (the reader coalesces).
    """
    out = bytearray()
    out += FORMAT_MAGIC
    out += struct.pack("<I", FORMAT_VERSION)
    nb = name.encode("utf-8")
    out += struct.pack("<I", len(nb))
    out += nb
    frames = list(frames)
    out += struct.pack("<I", len(frames))
    for t, regions in frames:
        out += struct.pack("<d", float(t))
        regions = list(regions)
        out += struct.pack("<I", len(regions))
        for addr, data in regions:
            out += struct.pack("<II", addr & 0xFFFFFFFF, len(data))
            out += bytes(data)
    return bytes(out)


def write_halorec(path, name, frames):
    """Gzip-write an HMRC recording. Returns the number of frames written."""
    frames = list(frames)
    raw = encode_halorec(name, frames)
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with gzip.open(path, "wb") as fh:
        fh.write(raw)
    return len(frames)
