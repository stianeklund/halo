#!/usr/bin/env python3
"""Tests for the embedder's input caps.

Regression cover for the 2026-10-07 OOM kills: Embedder.embed_one (the query
path used by research_bundle) had no token cap, so a large decompile ran the
model at its 8192-token default and one process spiked past 8 GB. embed_one
must apply the same caps as embed_batch, and restore the model's setting.

Uses a fake model, so it loads neither torch nor the jina weights.
"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import embed  # noqa: E402


class FakeVec:
    def tolist(self):
        return [0.0, 0.0, 0.0]


class FakeModel:
    def __init__(self, max_seq_length=8192, fail=False):
        self.max_seq_length = max_seq_length
        self.fail = fail
        self.seen = []

    def encode(self, text, **_kw):
        self.seen.append((self.max_seq_length, text))
        if self.fail:
            raise RuntimeError("boom")
        return FakeVec()


def make(model):
    e = embed.Embedder()
    embed.Embedder._model = model
    return e


class EmbedOneCaps(unittest.TestCase):
    def tearDown(self):
        embed.Embedder._model = None

    def test_caps_tokens_and_chars(self):
        m = FakeModel()
        make(m).embed_one("x" * (embed.MAX_CHARS * 3))
        self.assertEqual(m.seen[0][0], embed.MAX_TOKENS)
        self.assertEqual(len(m.seen[0][1]), embed.MAX_CHARS)

    def test_restores_model_setting(self):
        m = FakeModel()
        make(m).embed_one("abc")
        self.assertEqual(m.max_seq_length, 8192)

    def test_restores_on_encode_failure(self):
        m = FakeModel(fail=True)
        with self.assertRaises(RuntimeError):
            make(m).embed_one("abc")
        self.assertEqual(m.max_seq_length, 8192)

    def test_smaller_model_limit_untouched(self):
        m = FakeModel(max_seq_length=256)
        make(m).embed_one("abc")
        self.assertEqual(m.seen[0][0], 256)

    def test_empty_returns_none(self):
        self.assertIsNone(make(FakeModel()).embed_one(""))
        self.assertIsNone(make(FakeModel()).embed_one(None))


if __name__ == "__main__":
    unittest.main()
