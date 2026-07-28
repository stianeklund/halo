#!/usr/bin/env python3
"""Tests for the retrieval-server singleton lock.

Regression cover for the leak that took the dev box to <300 MB free on
2026-07-28: 14 orphaned servers at ~1.3 GB each. Two bugs compounded --

  1. the Unix socket was the only liveness signal available to spawners, but
     it is created AFTER the index rebuild, so a server that was up and
     consuming 1.3 GB looked absent for minutes and every probe in that window
     started another one; and
  2. each new server unlinked SOCK_PATH/PID_PATH unconditionally, severing the
     running server's socket and leaving it alive but unreachable forever.

Uses an isolated lock path so it cannot disturb servers running on this host.
"""
import os
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import server  # noqa: E402


HOLD_SCRIPT = """
import os, sys, time
sys.path.insert(0, {mod!r})
import server
server.LOCK_PATH = __import__('pathlib').Path({lock!r})
assert server.acquire_singleton_lock() is not None, 'child failed to take lock'
print('HELD', flush=True)
time.sleep(120)
"""


class SingletonLockTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="retrsrv-test-")
        self.lock = Path(self.tmp) / "test.lock"
        self._orig = server.LOCK_PATH
        server.LOCK_PATH = self.lock
        server._lock_fd = None
        self.child = None

    def tearDown(self):
        if self.child:
            if self.child.poll() is None:
                self.child.kill()
                self.child.wait(timeout=10)
            if self.child.stdout:
                self.child.stdout.close()
        if server._lock_fd is not None:
            os.close(server._lock_fd)
            server._lock_fd = None
        server.LOCK_PATH = self._orig

    def _spawn_holder(self):
        code = HOLD_SCRIPT.format(
            mod=str(Path(server.__file__).resolve().parent), lock=str(self.lock))
        self.child = subprocess.Popen(
            [sys.executable, "-c", code],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        line = self.child.stdout.readline().strip()
        self.assertEqual(line, "HELD", f"child did not take the lock: {line}")
        return self.child.pid

    def test_free_lock_reports_no_holder(self):
        self.assertIsNone(server.singleton_holder_pid())

    def test_probe_sees_a_holder_and_reports_its_pid(self):
        pid = self._spawn_holder()
        self.assertEqual(server.singleton_holder_pid(), pid)

    def test_second_acquire_is_refused_while_held(self):
        """The core fix: a would-be second server must lose the race."""
        self._spawn_holder()
        self.assertIsNone(server.acquire_singleton_lock())

    def test_probe_does_not_consume_the_lock(self):
        """Probing must be non-destructive -- otherwise the check itself would
        hand the lock to whichever spawner asked last."""
        pid = self._spawn_holder()
        for _ in range(3):
            self.assertEqual(server.singleton_holder_pid(), pid)
        self.assertIsNone(server.acquire_singleton_lock())

    def test_lock_is_released_when_holder_is_SIGKILLed(self):
        """flock is released by the kernel, so an OOM-killed server -- exactly
        how these died -- cannot wedge every future start the way a stale PID
        file would."""
        self._spawn_holder()
        self.child.kill()
        self.child.wait(timeout=10)
        for _ in range(50):
            if server.singleton_holder_pid() is None:
                break
            time.sleep(0.1)
        self.assertIsNone(server.singleton_holder_pid())
        self.assertIsNotNone(server.acquire_singleton_lock())

    def test_holder_records_its_pid_in_the_lock_file(self):
        fd = server.acquire_singleton_lock()
        self.assertIsNotNone(fd)
        self.assertEqual(self.lock.read_text().strip(), str(os.getpid()))


if __name__ == "__main__":
    unittest.main(verbosity=2)
