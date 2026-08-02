#!/usr/bin/env python3
"""Regression test for the inclusive player yaw endpoint.

The original binary accepts desired_angles.yaw == 2*pi.  This test drives the
smallest state needed by player_control_get_facing_angles through both the
MSVC oracle and the lifted object.  A strict '< 2*pi' candidate enters the
display_assert/system_exit path, which Unicorn's stub trace reports as a
behavioral divergence even though the returned pointer would otherwise match.
"""

import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools" / "equivalence"))

try:
    import unicorn_diff  # noqa: E402
except ImportError as exc:  # pragma: no cover - dependency-gated self-test
    unicorn_diff = None
    _UNICORN_IMPORT_ERROR = str(exc)
else:
    _UNICORN_IMPORT_ERROR = ""


def _snapshot(path):
    """Write a minimal player-control global and slot state snapshot."""
    slot = bytearray(0x40)
    struct.pack_into("<f", slot, 0x0c, 6.2831855)
    struct.pack_into("<f", slot, 0x10, 0.0)
    data = {
        "description": "player_control_get_facing_angles accepts yaw == 2*pi",
        "build_label": "synthetic",
        "regions": {
            # player_control_globals pointer used by the original function.
            "0x00457090": struct.pack("<II", 0x00700000, 0).hex(),
            # slot = globals + 0x10 for local player 0.
            "0x00700010": bytes(slot).hex(),
        },
        "arg_overrides": {"local_player_index": 0},
    }
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


@unittest.skipIf(unicorn_diff is None, "unicorn unavailable: %s" % _UNICORN_IMPORT_ERROR)
class PlayerControlAngleEndpointTest(unittest.TestCase):
    def test_yaw_two_pi_is_not_rejected(self):
        with tempfile.TemporaryDirectory(prefix="player_angle_") as tmp:
            snapshot = Path(tmp) / "yaw_endpoint.json"
            _snapshot(snapshot)
            result = unicorn_diff.run_diff(
                "player_control_get_facing_angles",
                num_seeds=1,
                base_seed=0,
                save_log=False,
                quiet=True,
                allow_stubs=True,
                mem_trace=True,
                no_concolic=True,
                state_snapshot=snapshot,
            )
            self.assertEqual(0, result)


if __name__ == "__main__":
    unittest.main(verbosity=2)
