from pathlib import Path

from tools.analysis.build_ntsc_correspondence import (
    build_report,
    load_reference_symbols,
    load_reference_map,
    normalize_name,
)


def test_normalize_name_removes_c_convention_decoration():
    assert normalize_name("_vector_from_points3d") == "vector_from_points3d"
    assert normalize_name("_api_call@12") == "api_call"


def test_report_keeps_only_unique_meaningful_names():
    ntsc = [
        {"addr": "0x1000", "name": "FUN_00001000", "normalized_name": "FUN_00001000",
         "decl": "void FUN_00001000(void);", "ported": False, "object": "a.obj", "source": "a.c"},
        {"addr": "0x1010", "name": "vector_from_points3d", "normalized_name": "vector_from_points3d",
         "decl": "void vector_from_points3d(void);", "ported": False, "object": "b.obj", "source": "b.c"},
        {"addr": "0x1020", "name": "duplicate", "normalized_name": "duplicate",
         "decl": "void duplicate(void);", "ported": True, "object": "c.obj", "source": "c.c"},
        {"addr": "0x1030", "name": "duplicate", "normalized_name": "duplicate",
         "decl": "void duplicate(void);", "ported": True, "object": "d.obj", "source": "d.c"},
    ]
    reference = [
        {"file_offset": 0, "name": "_vector_from_points3d", "normalized_name": "vector_from_points3d"},
        {"file_offset": 1, "name": "_FUN_00001000", "normalized_name": "FUN_00001000"},
        {"file_offset": 2, "name": "_duplicate", "normalized_name": "duplicate"},
    ]

    report = build_report(ntsc, reference)

    assert report["summary"]["unique_meaningful_name_matches"] == 1
    assert report["summary"]["unported_name_only_candidates"] == 1
    candidate = report["candidates"][0]
    assert candidate["status"] == "NAME_ONLY"
    assert candidate["ntsc"]["name"] == "vector_from_points3d"


def test_load_pal_map_reads_function_va_and_owner(tmp_path: Path):
    map_path = tmp_path / "cachebeta.map"
    map_path.write_text(
        " 0001:0000d910       _actor_action_handle_combat_failure 0040dcf0 f   actions.obj\n",
        encoding="utf-8",
    )

    symbols = load_reference_map(map_path)

    assert symbols == [{
        "file_offset": None,
        "va": "0x0040dcf0",
        "object": "actions.obj",
        "name": "_actor_action_handle_combat_failure",
        "normalized_name": "actor_action_handle_combat_failure",
    }]


def test_load_reference_pdb_symbols_keeps_text_rva_and_rejects_data(tmp_path: Path):
    symbols_path = tmp_path / "pdb_symbols.json"
    symbols_path.write_text(
        '[{"name":"_action","rva":16,"section":".text","kind":"LABEL"},'
        '{"name":"_data","rva":32,"section":".data","kind":"DATA"}]',
        encoding="utf-8",
    )

    symbols = load_reference_symbols(symbols_path)

    assert symbols == [{
        "file_offset": None,
        "rva": 16,
        "section": ".text",
        "kind": "LABEL",
        "name": "_action",
        "normalized_name": "action",
    }]
