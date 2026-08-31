"""Direct-call tests for tools.analysis.crossbuild_context."""

import json
from pathlib import Path

from tools.analysis import crossbuild_context as subject


def _write_fixture(root: Path) -> None:
  (root / "artifacts" / "punpckhdq_import").mkdir(parents=True)
  (root / "kb.json").write_text(json.dumps({"objects": [{
    "name": "real_math.obj",
    "source": "math/real_math.c",
    "functions": [{"addr": "0x10bd70", "decl": "int FUN_0010bd70(float *point);"}],
  }]}))
  (root / "artifacts" / "punpckhdq_import" / "name_proposals.json").write_text(json.dumps([{
    "our_addr": "0x10bd70",
    "proposed_name": "point_in_rectangle2d",
    "their_source": "source/math/real_math.c",
    "their_object": "real_math.obj",
    "confidence": "high",
    "real_name": True,
    "ordinal": 12,
    "size_their": 69,
    "size_ours": 69,
  }]))


def test_context_reports_high_confidence_candidate(tmp_path: Path) -> None:
  old_root = subject.ROOT
  old_proposals = subject.PROPOSALS
  try:
    _write_fixture(tmp_path)
    subject.ROOT = tmp_path
    subject.PROPOSALS = tmp_path / "artifacts" / "punpckhdq_import" / "name_proposals.json"
    context = subject.build_context(subject.resolve_target("0x10bd70"))
    assert context["status"] == "high_confidence_candidate"
    assert context["candidates"][0]["name"] == "point_in_rectangle2d"
    assert context["evidence_label"] == "INFERRED"
  finally:
    subject.ROOT = old_root
    subject.PROPOSALS = old_proposals


def test_context_marks_missing_proposal_as_unavailable(tmp_path: Path) -> None:
  old_root = subject.ROOT
  old_proposals = subject.PROPOSALS
  try:
    _write_fixture(tmp_path)
    subject.ROOT = tmp_path
    subject.PROPOSALS = tmp_path / "missing.json"
    context = subject.build_context(subject.resolve_target("FUN_0010bd70"))
    assert context["status"] == "unavailable"
  finally:
    subject.ROOT = old_root
    subject.PROPOSALS = old_proposals
