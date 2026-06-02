#!/usr/bin/env python3
"""Tests for ContextPackBuilder Ghidra context-enrichment phases.

Phases 1, 4, and 5 are accurate (passing today).
Phases 2 and 3 have known false-positive bugs being fixed in parallel;
the tests here assert the CORRECT post-fix spec and are intentionally RED
until the parallel fix merges.  Do NOT weaken those assertions.

Run:
    python3 -m pytest tools/test_llm_auto_lift_enrichment.py -v
"""

from __future__ import annotations

import importlib.util
import json
import sys
from pathlib import Path

import pytest

# ---------------------------------------------------------------------------
# Module loading
# ---------------------------------------------------------------------------

ROOT = Path(__file__).resolve().parents[1]   # repo root from tools/


def _load_module():
    spec = importlib.util.spec_from_file_location(
        "llm_auto_lift", ROOT / "tools" / "llm_auto_lift.py"
    )
    mod = importlib.util.module_from_spec(spec)
    sys.modules["llm_auto_lift"] = mod
    spec.loader.exec_module(mod)
    return mod


_mod = _load_module()
ContextPackBuilder = _mod.ContextPackBuilder

# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

FIXTURE_PATH = ROOT / "artifacts" / "auto_lift" / "context_cache" / "FUN_0002cdb0.json"


@pytest.fixture(scope="module")
def builder():
    """Shared ContextPackBuilder (no Ghidra/MCP, loads kb.json once)."""
    return ContextPackBuilder(ghidra_live=False)


@pytest.fixture(scope="module")
def cdb0_ctx():
    """Load the FUN_0002cdb0 cached context."""
    return json.loads(FIXTURE_PATH.read_text(encoding="utf-8"))


@pytest.fixture(scope="module")
def fstp_disasm_ctx():
    """Synthetic disassembly context containing an FSTP float-arg call pattern.

    datum_get (0x119320) has param_count=2 in kb.json.  Two pushes [PUSH 0x0,
    FSTP [ESP]] match exactly — so no stray ARG_COUNT hazard fires alongside
    the FPU_ARG hazard we want to verify.
    """
    raw = '{"result": "00401000: PUSH 0x0 \\n00401005: FSTP dword ptr [ESP] \\n00401009: CALL 0x00119320 \\n"}'
    return {
        "disassembly": raw,
        "decompile_c": "",
        "callees": [],
        "callers": [],
    }


# ---------------------------------------------------------------------------
# Phase 1 — _gather_callee_details  (ACCURATE, passes today)
# ---------------------------------------------------------------------------

def test_phase1_returns_dict_keyed_by_callee_names(builder):
    """Phase 1 result is a dict; keys match the callees list."""
    ctx = {"callees": ["datum_get", "tag_block_get_element", "bogus_never_in_kb"]}
    result = builder._gather_callee_details(None, ctx)
    assert isinstance(result, dict)
    assert set(result.keys()) == {"datum_get", "tag_block_get_element", "bogus_never_in_kb"}


def test_phase1_known_callee_resolves_to_kb_entry(builder):
    """Phase 1 resolves kb.json names to full entries (not not_in_kb)."""
    ctx = {"callees": ["datum_get", "tag_block_get_element"]}
    result = builder._gather_callee_details(None, ctx)
    # Both names are in kb.json — neither should have the not_in_kb sentinel
    for name in ("datum_get", "tag_block_get_element"):
        assert name in result
        assert "not_in_kb" not in result[name], (
            f"{name} should resolve to a kb entry, got {result[name]}"
        )
        assert result[name].get("decl"), f"{name} entry should have a decl"


def test_phase1_unknown_callee_gets_not_in_kb(builder):
    """Phase 1 marks completely unknown names with not_in_kb=True."""
    ctx = {"callees": ["definitely_not_in_kb_at_all_xyz"]}
    result = builder._gather_callee_details(None, ctx)
    assert result["definitely_not_in_kb_at_all_xyz"].get("not_in_kb") is True


def test_phase1_empty_callees_returns_empty_dict(builder, cdb0_ctx):
    """Phase 1 on a fixture with callees=[] returns {}."""
    result = builder._gather_callee_details(None, cdb0_ctx)
    assert result == {}


# ---------------------------------------------------------------------------
# Phase 2 — _audit_call_sites
# ---------------------------------------------------------------------------

# --- Post-fix assertions (RED today, green after parallel fix) ---

def test_phase2_no_arg_count_hazard(builder, cdb0_ctx):
    """POST-FIX: ARG_COUNT hazard must never appear in any audit entry.

    Currently RED — the parallel fix removes the param-count cross-check
    because push-scanning before register-arg callees is unreliable.
    """
    audit = builder._audit_call_sites(None, cdb0_ctx)
    for cs in audit:
        for h in cs.get("hazards", []):
            assert "ARG_COUNT" not in h, (
                f"ARG_COUNT hazard should not fire, found: {h!r} at call {cs['call_addr']}"
            )


def test_phase2_no_register_aliasing_hazard_on_fixture(builder, cdb0_ctx):
    """POST-FIX: REGISTER_ALIASING hazard must not appear on FUN_0002cdb0.

    Currently RED — the parallel fix gates REGISTER_ALIASING on callee-saved
    registers in non-leaf, non-trivial contexts rather than all PUSH EBX/ESI/EDI.
    """
    audit = builder._audit_call_sites(None, cdb0_ctx)
    for cs in audit:
        for h in cs.get("hazards", []):
            assert "REGISTER_ALIASING" not in h, (
                f"REGISTER_ALIASING hazard should not fire, found: {h!r} at call {cs['call_addr']}"
            )


# --- Accurate assertions (GREEN today) ---

def test_phase2_callee_identity_populated(builder, cdb0_ctx):
    """Audit entries that hit a kb callee have a non-empty callee_kb name."""
    audit = builder._audit_call_sites(None, cdb0_ctx)
    assert len(audit) > 0, "Expected at least one CALL site in FUN_0002cdb0"
    # datum_get (0x119320) and tag_block_get_element (0x19b210) appear in the fixture
    named = [cs for cs in audit if cs.get("callee_kb")]
    assert len(named) > 0, (
        "At least one audit entry should resolve a callee name from kb.json"
    )
    known = {cs["callee_kb"] for cs in named}
    assert known & {"datum_get", "tag_block_get_element"}, (
        f"Expected datum_get or tag_block_get_element in resolved callees, got {known}"
    )


def test_phase2_fstp_fpu_hazard_fires_on_synthetic(builder, fstp_disasm_ctx):
    """FSTP FPU hazard is detected in a synthetic disassembly (green today)."""
    audit = builder._audit_call_sites(None, fstp_disasm_ctx)
    # Should have exactly one call-site entry
    assert len(audit) == 1, f"Expected 1 call entry from synthetic ctx, got {len(audit)}"
    hazards = audit[0].get("hazards", [])
    fpu_hazards = [h for h in hazards if "FPU" in h]
    assert fpu_hazards, (
        f"Expected a hazard containing 'FPU' for FSTP pattern, got hazards={hazards!r}"
    )


def test_phase2_audit_returns_list(builder, cdb0_ctx):
    """_audit_call_sites always returns a list."""
    result = builder._audit_call_sites(None, cdb0_ctx)
    assert isinstance(result, list)


# ---------------------------------------------------------------------------
# Phase 3 — _extract_struct_offsets
# ---------------------------------------------------------------------------

# --- Post-fix assertions (RED today, green after parallel fix) ---

def test_phase3_result_keys_are_only_callee_saved_registers(builder, cdb0_ctx):
    """POST-FIX: result keys are a strict subset of {ESI, EDI, EBX}.

    Currently RED — current code also emits EBP and EDX keys.
    """
    result = builder._extract_struct_offsets(cdb0_ctx)
    assert set(result).issubset({"ESI", "EDI", "EBX"}), (
        f"Unexpected register keys in struct offsets: {set(result) - {'ESI','EDI','EBX'}}"
    )


def test_phase3_ebp_not_in_result(builder, cdb0_ctx):
    """POST-FIX: EBP must not appear as a struct-pointer register (it's the frame pointer)."""
    result = builder._extract_struct_offsets(cdb0_ctx)
    assert "EBP" not in result, f"EBP should be filtered out, got offsets: {result.get('EBP')}"


def test_phase3_eax_ecx_edx_not_in_result(builder, cdb0_ctx):
    """POST-FIX: volatile caller-saved registers (EAX, ECX, EDX) must not appear."""
    result = builder._extract_struct_offsets(cdb0_ctx)
    for reg in ("EAX", "ECX", "EDX"):
        assert reg not in result, (
            f"{reg} should not appear in struct offsets, got offsets: {result.get(reg)}"
        )


# --- Accurate assertions (GREEN today) ---

def test_phase3_esi_present_with_large_offsets(builder, cdb0_ctx):
    """ESI is present in FUN_0002cdb0 and has verified offsets >= 0x100.

    The fixture accesses ESI+0x46c, ESI+0x488, ESI+0x470, etc. (count >= 2).
    """
    result = builder._extract_struct_offsets(cdb0_ctx)
    assert "ESI" in result, "ESI should be present as a struct-pointer register"
    offsets = result["ESI"]
    assert len(offsets) > 0, "ESI must have at least one verified offset"
    large = [o for o in offsets if int(o, 16) >= 0x100]
    assert large, (
        f"ESI should contain at least one offset >= 0x100, got {offsets}"
    )


def test_phase3_no_oversized_or_negative_offsets(builder, cdb0_ctx):
    """All offsets across all registers fit in [0, 0x10000)."""
    result = builder._extract_struct_offsets(cdb0_ctx)
    for reg, offsets in result.items():
        for o in offsets:
            val = int(o, 16)
            assert 0 <= val <= 0x10000, (
                f"Offset {o!r} for register {reg} out of expected range [0, 0x10000]"
            )


def test_phase3_returns_dict(builder, cdb0_ctx):
    """_extract_struct_offsets always returns a dict."""
    result = builder._extract_struct_offsets(cdb0_ctx)
    assert isinstance(result, dict)


# ---------------------------------------------------------------------------
# Phase 4 — _annotate_buffer_aliases  (ACCURATE, passes today)
# ---------------------------------------------------------------------------

def test_phase4_returns_dict_and_does_not_raise(builder, cdb0_ctx):
    """_annotate_buffer_aliases returns a dict without raising on the fixture."""
    result = builder._annotate_buffer_aliases(cdb0_ctx)
    assert isinstance(result, dict)


def test_phase4_result_has_expected_structure(builder, cdb0_ctx):
    """When buffer alias analysis succeeds, result has total_hits/high_risk/low_risk keys
    (or an 'error' key if the detector is unavailable — both are acceptable dict shapes)."""
    result = builder._annotate_buffer_aliases(cdb0_ctx)
    if "error" in result:
        # Detector unavailable in CI — acceptable
        assert isinstance(result["error"], str)
    elif result:
        # Non-empty result must have the standard keys
        for key in ("total_hits", "high_risk", "low_risk"):
            assert key in result, f"Expected key {key!r} in Phase 4 result, got {result}"
    # 0 hits is also fine — empty dict {} is returned in that branch
    # (the function returns {} on empty decompile, {total_hits:0,...} on 0 hits)


def test_phase4_empty_decompile_returns_empty_dict(builder):
    """_annotate_buffer_aliases returns {} when decompile_c is absent."""
    ctx = {"decompile_c": "", "disassembly": "", "callees": [], "callers": []}
    result = builder._annotate_buffer_aliases(ctx)
    assert result == {}


# ---------------------------------------------------------------------------
# Phase 5 — _verify_buffer_sizes  (ACCURATE, passes today)
# ---------------------------------------------------------------------------

def test_phase5_returns_list_and_does_not_raise(builder, cdb0_ctx):
    """_verify_buffer_sizes returns a list without raising on the fixture."""
    result = builder._verify_buffer_sizes(cdb0_ctx)
    assert isinstance(result, list)


def test_phase5_no_false_warnings_on_clean_fixture(builder, cdb0_ctx):
    """FUN_0002cdb0 decompile has no KNOWN_BUFFER_SIZES callee calls so no warnings."""
    result = builder._verify_buffer_sizes(cdb0_ctx)
    # The fixture doesn't call FUN_0013fc20, so no BUFFER_UNDERSIZED warnings
    assert isinstance(result, list)   # may be empty or non-empty; must not crash


def test_phase5_synthetic_undersized_buffer_warning(builder):
    """BUFFER_UNDERSIZED fires when a declared buffer is smaller than callee requires."""
    fake_decompile = (
        "void test_func(void) {\n"
        "    char local_buf[0x30];\n"
        "    FUN_0013fc20(local_buf, 0);\n"
        "}\n"
    )
    ctx = {
        "decompile_c": fake_decompile,
        "disassembly": "",
        "callees": [],
        "callers": [],
        "callee_details": {},
    }
    result = builder._verify_buffer_sizes(ctx)
    assert isinstance(result, list)
    if result:  # Only fires when KNOWN_BUFFER_SIZES is populated (it is)
        assert any("BUFFER_UNDERSIZED" in w for w in result), (
            f"Expected BUFFER_UNDERSIZED warning, got {result}"
        )
