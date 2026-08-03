#!/usr/bin/env python3
"""Regression test for the inclusive player desired-angle endpoints.

The inlined valid_euler_angles2d() inside player_control_get_facing_angles
(0xb7e30) bounds both components with INCLUSIVE tests.  Each upper bound is
`FCOMP <bound> / FNSTSW AX / TEST AH,0x41 / JP <assert>`, and JP is taken only
when C0 and C3 are both clear (strictly greater) or the compare was unordered;
equality sets C3 alone and falls through to the pass path.  Each lower bound is
`TEST AH,1` on C0, which is likewise clear on equality.  So all four of

    pitch <= +1.4922565   pitch >= -1.4922565
    yaw   <= +6.2831855   yaw   >= 0.0

accept the endpoint exactly.

Every endpoint needs its own case.  This test originally pinned only the yaw
upper bound, and the pitch upper bound shipped as a strict `<` for a further
day: player_control_update_desired_angles clamps desired_angles.pitch with a
raw dword copy from pc->pitch_maximum, which is initialised to the exact bit
pattern of the bound (MOV [ESI+0x3C],0x3fbf0243 at 0xb7031) and in MP has no
seat camera limit pulling it lower.  Looking fully up therefore lands pitch ON
the bound, and the strict test asserted every frame.  VC71 scores 96.8% either
way, so byte-match is blind to this class -- these cases are the only gate.

A strict candidate enters the display_assert/system_exit path, which Unicorn's
stub trace reports as a behavioral divergence even though the returned pointer
would otherwise match.
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

# .rdata bounds the original compares against, as exact float32 values.
YAW_MAXIMUM = 6.2831855  # 0x40c90fdb @ 0x255a54
YAW_MINIMUM = 0.0  # 0x00000000 @ 0x2533c0
PITCH_MAXIMUM = 1.49225652217865  # 0x3fbf0243 @ 0x26e37c
PITCH_MINIMUM = -1.49225652217865  # 0xbfbf0243 @ 0x26e378


def _snapshot(path, yaw, pitch, description):
    """Write a minimal player-control global and slot state snapshot."""
    slot = bytearray(0x40)
    struct.pack_into("<f", slot, 0x0c, yaw)
    struct.pack_into("<f", slot, 0x10, pitch)
    data = {
        "description": description,
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
    def _check_endpoint(self, name, yaw, pitch):
        description = "player_control_get_facing_angles accepts %s" % name
        with tempfile.TemporaryDirectory(prefix="player_angle_") as tmp:
            snapshot = Path(tmp) / ("%s.json" % name)
            _snapshot(snapshot, yaw, pitch, description)
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
            self.assertEqual(0, result, "%s must not be rejected" % description)

    def test_yaw_two_pi_is_not_rejected(self):
        self._check_endpoint("yaw_maximum", YAW_MAXIMUM, 0.0)

    def test_yaw_zero_is_not_rejected(self):
        self._check_endpoint("yaw_minimum", YAW_MINIMUM, 0.0)

    def test_pitch_maximum_is_not_rejected(self):
        """The MP look-fully-up case: pitch lands exactly on +85.5 degrees."""
        self._check_endpoint("pitch_maximum", 0.0, PITCH_MAXIMUM)

    def test_pitch_minimum_is_not_rejected(self):
        """The look-fully-down case: pitch lands exactly on -85.5 degrees."""
        self._check_endpoint("pitch_minimum", 0.0, PITCH_MINIMUM)


if __name__ == "__main__":
    unittest.main(verbosity=2)
