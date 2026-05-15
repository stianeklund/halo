#!/usr/bin/env python3
"""
FUN_ Pipeline: Systematically classify, prioritize, and name unidentified functions.

Stages:
  1. reclassify  — Move <common> FUN_ functions into named objects via address proximity + __FILE__ evidence
  2. prioritize  — Rank FUN_ functions by impact (called by ported code = P0, has params = P1, etc.)
  3. propose     — Use Ghidra MCP to decompile each FUN_, extract strings/callees/callers, propose a name
  4. apply       — Write proposed names/decls back to kb.json under review

Usage:
  python3 tools/analysis/fun_pipeline.py reclassify [--apply]
  python3 tools/analysis/fun_pipeline.py prioritize [--format json|text] [--priority P0|P1|P2]
  python3 tools/analysis/fun_pipeline.py propose [--limit N] [--object OBJ] [--priority P0]
  python3 tools/analysis/fun_pipeline.py apply [--dry-run]
  python3 tools/analysis/fun_pipeline.py status

Pipeline stages are designed to be run incrementally. Each stage produces output
that feeds the next. The 'propose' stage requires Ghidra MCP access for decompilation.
"""

import json
import sys
import os
import re
import argparse
from collections import Counter, defaultdict

TOOLS_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KB_PATH = os.path.join(os.path.dirname(TOOLS_DIR), "kb.json")


def load_kb():
    with open(KB_PATH) as f:
        return json.load(f)


def save_kb(kb):
    with open(KB_PATH, "w") as f:
        json.dump(kb, f, indent=2)
        f.write("\n")


def parse_decl(decl):
    """Extract return type, name, and params from a C declaration."""
    m = re.match(r'^(.*?)\b(\w+)\s*\((.*)\)\s*;?\s*$', decl, re.DOTALL)
    if not m:
        return None
    ret = m.group(1).strip()
    name = m.group(2)
    params_raw = m.group(3).strip()
    has_params = params_raw and params_raw != "void"
    has_return = ret and ret != "void"
    return {
        "return_type": ret,
        "name": name,
        "params_raw": params_raw,
        "has_params": has_params,
        "has_return": has_return,
        "is_fun": name.startswith("FUN_"),
    }


def get_all_functions(kb):
    """Flatten all functions with their parent object name."""
    funcs = []
    for o in kb.get("objects", []):
        obj_name = o.get("name") or ""
        for f in o.get("functions", []):
            entry = dict(f)
            entry["_object"] = obj_name
            entry["_parsed"] = parse_decl(f.get("decl", ""))
            funcs.append(entry)
    return funcs


# =============================================================================
# Stage 1: Reclassify
# =============================================================================

def cmd_reclassify(args):
    """Move <common> FUN_ functions into named objects using address proximity."""
    kb = load_kb()
    classify_json = _run_classify_common()

    if not classify_json:
        print("ERROR: classify_common.py --json returned no data", file=sys.stderr)
        return 1

    # Load classification results
    high_conf = [e for e in classify_json if e.get("confidence") == "high"]
    med_conf = [e for e in classify_json if e.get("confidence") == "medium"]
    candidates = high_conf + med_conf

    # Filter: only move if proposed target is NOT <common> and NOT a LIBC/XDK bucket
    moves = []
    for entry in candidates:
        target = entry.get("proposed_target", "")
        if " or " in target:
            # Ambiguous — skip for now, or take first option
            target = target.split(" or ")[0]
        if target.startswith("<") and target.endswith(">"):
            continue  # Skip stub buckets
        if target.startswith("LIBCMT") or target.startswith("XAPILIB") or target.startswith("D3D8") or target.startswith("XNET"):
            continue  # Skip platform stubs
        moves.append(entry)

    print(f"Proposed reclassifications: {len(moves)} (of {len(candidates)} candidates)")
    print(f"  High confidence: {len(high_conf)}")
    print(f"  Medium confidence: {len(med_conf)}")
    print(f"  Filtered (non-stub, non-ambiguous): {len(moves)}")

    if not args.apply:
        print("\nDry run — use --apply to write changes to kb.json")
        print("Sample moves:")
        for entry in moves[:10]:
            print(f"  {entry['addr']} -> {entry.get('proposed_target', '?')} ({entry.get('confidence')})")
        return 0

    # Apply moves
    obj_by_name = {o.get("name"): o for o in kb["objects"] if o.get("name")}
    common_obj = None
    common_idx = None
    for i, o in enumerate(kb["objects"]):
        if o.get("name") == "<common>":
            common_obj = o
            common_idx = i
            break

    if common_obj is None:
        print("ERROR: No <common> object found", file=sys.stderr)
        return 1

    addr_to_move = {e["addr"]: e for e in moves}
    moved_funcs = []
    remaining_funcs = []

    for f in common_obj.get("functions", []):
        addr = f.get("addr", "")
        if addr in addr_to_move:
            target = addr_to_move[addr].get("proposed_target", "")
            if " or " in target:
                target = target.split(" or ")[0]
            if target in obj_by_name:
                obj = obj_by_name[target]
                if "functions" not in obj:
                    obj["functions"] = []
                obj["functions"].append(f)
                obj["functions"].sort(key=lambda x: int(x.get("addr", "0x0"), 16))
                moved_funcs.append((addr, target))
            else:
                # Create new object
                new_obj = {"name": target, "functions": [f], "data": []}
                kb["objects"].append(new_obj)
                obj_by_name[target] = new_obj
                moved_funcs.append((addr, target))
        else:
            remaining_funcs.append(f)

    common_obj["functions"] = remaining_funcs
    print(f"\nMoved {len(moved_funcs)} functions from <common> to named objects")
    print(f"Remaining in <common>: {len(remaining_funcs)}")

    # Count by target
    target_counts = Counter(t for _, t in moved_funcs)
    print("\nTop targets:")
    for target, count in target_counts.most_common(15):
        print(f"  {target}: +{count}")

    save_kb(kb)
    print("\nkb.json updated.")
    return 0


def _run_classify_common():
    """Run classify_common.py --json and return parsed results."""
    import subprocess
    result = subprocess.run(
        [sys.executable, os.path.join(TOOLS_DIR, "analysis", "classify_common.py"), "--json"],
        capture_output=True, text=True, cwd=os.path.dirname(KB_PATH)
    )
    if result.returncode != 0:
        print(f"ERROR running classify_common.py: {result.stderr[:500]}", file=sys.stderr)
        return None
    try:
        return json.loads(result.stdout)
    except json.JSONDecodeError:
        return None


# =============================================================================
# Stage 2: Prioritize
# =============================================================================

def cmd_prioritize(args):
    """Rank FUN_ functions by impact and priority tier."""
    kb = load_kb()
    all_funcs = get_all_functions(kb)

    fun_funcs = [f for f in all_funcs if f.get("_parsed") and f["_parsed"]["is_fun"]]
    ported_addrs = set(f["addr"] for f in all_funcs if f.get("ported"))

    # Build caller graph ported set for P0 detection
    # (In a full implementation, this would use Ghidra xrefs. For now, use heuristics.)

    tiers = {"P0": [], "P1": [], "P2": [], "P3": []}

    for f in fun_funcs:
        addr = f.get("addr", "")
        parsed = f["_parsed"]
        obj_name = f.get("_object", "")
        is_common = obj_name == "<common>"

        # P0: FUN_ in a named object (not <common>) — already attributed
        # In a full impl, P0 would be "called by ported code"
        # For now, P0 = attributed + has params
        # P1 = attributed + has real signature (params or return)
        # P2 = attributed + void(void) but in a named object
        # P3 = in <common> (needs classification first)

        if is_common:
            tiers["P3"].append(f)
        elif parsed["has_params"] or parsed["has_return"]:
            tiers["P0"].append(f)
        elif not is_common:
            tiers["P2"].append(f)
        else:
            tiers["P3"].append(f)

    if args.format == "json":
        output = {}
        for tier, funcs in tiers.items():
            output[tier] = [
                {"addr": f["addr"], "decl": f.get("decl", ""), "object": f.get("_object", "")}
                for f in funcs
            ]
        print(json.dumps(output, indent=2))
    else:
        print("=== FUN_ Function Prioritization ===\n")
        print(f"{'Tier':4s} {'Count':>6s}  Description")
        print("-" * 60)
        print(f"{'P0':4s} {len(tiers['P0']):6d}  Attributed + has signature (params/return)")
        print(f"{'P1':4s} {len(tiers['P1']):6d}  (reserved for 'called by ported code')")
        print(f"{'P2':4s} {len(tiers['P2']):6d}  Attributed but void f(void)")
        print(f"{'P3':4s} {len(tiers['P3']):6d}  In <common> (classify first)")
        print(f"{'Total':4s} {sum(len(v) for v in tiers.values()):6d}")

        # Show top P0 objects
        if args.priority in ("P0", None):
            obj_counts = Counter(f.get("_object", "?") for f in tiers["P0"])
            print(f"\n--- P0 by Object (top 20) ---")
            for obj, count in obj_counts.most_common(20):
                print(f"  {obj:45s} {count:4d}")

        if args.priority in ("P2", None):
            obj_counts = Counter(f.get("_object", "?") for f in tiers["P2"])
            print(f"\n--- P2 by Object (top 20) ---")
            for obj, count in obj_counts.most_common(20):
                print(f"  {obj:45s} {count:4d}")

    return 0


# =============================================================================
# Stage 3: Propose (requires Ghidra MCP)
# =============================================================================

def cmd_propose(args):
    """Generate a work queue of FUN_ functions for Ghidra-based naming.

    Outputs a JSON list of functions to process, with context like
    parent object, string references, and caller info where available.
    """
    kb = load_kb()
    all_funcs = get_all_functions(kb)

    fun_funcs = [f for f in all_funcs if f.get("_parsed") and f["_parsed"]["is_fun"]]

    # Filter by priority / object
    candidates = fun_funcs
    if args.object:
        candidates = [f for f in candidates if f.get("_object") == args.object]
    if args.priority == "P0":
        candidates = [f for f in candidates
                      if f.get("_object") != "<common>"
                      and (f["_parsed"]["has_params"] or f["_parsed"]["has_return"])]
    elif args.priority == "P2":
        candidates = [f for f in candidates
                      if f.get("_object") != "<common>"
                      and not f["_parsed"]["has_params"]]

    # Sort by object for batch processing
    candidates.sort(key=lambda f: (f.get("_object", ""), f.get("addr", "")))

    if args.limit:
        candidates = candidates[:args.limit]

    # Build proposal entries
    proposals = []
    for f in candidates:
        entry = {
            "addr": f["addr"],
            "decl": f.get("decl", ""),
            "object": f.get("_object", ""),
            "priority": "P0" if (f.get("_object") != "<common>" and (f["_parsed"]["has_params"] or f["_parsed"]["has_return"])) else
                        "P2" if f.get("_object") != "<common>" else "P3",
            "has_signature": f["_parsed"]["has_params"] or f["_parsed"]["has_return"],
            "ghidra_actions": [
                f"decompile {f['addr']}",
                f"get_callers {f['addr']}",
                f"get_strings_referenced_from {f['addr']}",
            ],
        }

        # Infer module prefix from object name
        obj = f.get("_object", "")
        if obj and obj != "<common>":
            # e.g. "game_engine.obj" -> "game_engine_"
            module_prefix = obj.replace(".obj", "").replace(".", "_") + "_"
            entry["module_prefix"] = module_prefix

        proposals.append(entry)

    print(json.dumps(proposals, indent=2))
    print(f"\n# Generated {len(proposals)} proposals", file=sys.stderr)

    if args.output:
        with open(args.output, "w") as f:
            json.dump(proposals, f, indent=2)
        print(f"# Written to {args.output}", file=sys.stderr)

    return 0


# =============================================================================
# Stage 4: Apply proposed names
# =============================================================================

def cmd_apply(args):
    """Apply name proposals from a JSON file back to kb.json.

    Input format: [{"addr": "0x...", "decl": "void foo(int x);", "object": "obj_name"}, ...]
    """
    if not args.input:
        print("ERROR: --input required", file=sys.stderr)
        return 1

    with open(args.input) as f:
        proposals = json.load(f)

    kb = load_kb()
    addr_map = {}
    for o in kb.get("objects", []):
        for func in o.get("functions", []):
            addr = func.get("addr", "")
            if addr:
                addr_map[addr] = (o, func)

    applied = 0
    skipped = 0

    for prop in proposals:
        addr = prop.get("addr", "")
        new_decl = prop.get("decl", "")
        new_name = prop.get("name", "")

        if addr not in addr_map:
            skipped += 1
            continue

        obj, func = addr_map[addr]
        old_decl = func.get("decl", "")

        if not args.dry_run:
            if new_decl:
                func["decl"] = new_decl
            elif new_name:
                # Replace FUN_ name in existing decl
                parsed = parse_decl(old_decl)
                if parsed:
                    func["decl"] = old_decl.replace(parsed["name"], new_name, 1)
        else:
            print(f"  DRY RUN: {addr} {old_decl[:50]} -> {new_decl or new_name}")

        applied += 1

    if not args.dry_run and applied > 0:
        save_kb(kb)
        print(f"Applied {applied} name proposals ({skipped} skipped, not found)")
    else:
        print(f"Dry run: {applied} would be applied ({skipped} skipped)")

    return 0


# =============================================================================
# Status
# =============================================================================

def cmd_status(args):
    """Print current pipeline status."""
    kb = load_kb()
    all_funcs = get_all_functions(kb)

    total = len(all_funcs)
    ported = sum(1 for f in all_funcs if f.get("ported"))
    fun_funcs = [f for f in all_funcs if f.get("_parsed") and f["_parsed"]["is_fun"]]
    identified = [f for f in all_funcs if f.get("_parsed") and not f["_parsed"]["is_fun"]]
    identified_ported = sum(1 for f in identified if f.get("ported"))
    fun_ported = sum(1 for f in fun_funcs if f.get("ported"))

    common_funcs = [f for f in all_funcs if f.get("_object") == "<common>"]
    common_fun = [f for f in common_funcs if f.get("_parsed") and f["_parsed"]["is_fun"]]

    print("=== FUN_ Pipeline Status ===\n")
    print(f"Total functions:          {total:,}")
    print(f"Ported:                  {ported:,} ({ported/total*100:.1f}%)")
    print(f"Identified (named):      {len(identified):,} ({identified_ported}/{len(identified)} ported = {identified_ported/len(identified)*100:.1f}%)")
    print(f"FUN_ (unnamed):          {len(fun_funcs):,} ({fun_ported}/{len(fun_funcs)} ported = {fun_ported/len(fun_funcs)*100:.1f}%)")
    print()
    print(f"<common> total:          {len(common_funcs):,}")
    print(f"<common> FUN_:            {len(common_fun):,}")
    print()
    print(f"Identified remaining:    {len(identified) - identified_ported}")
    print(f"FUN_ remaining:           {len(fun_funcs) - fun_ported}")
    print()

    # Tier breakdown
    p0 = [f for f in fun_funcs if f.get("_object") != "<common>" and (f["_parsed"]["has_params"] or f["_parsed"]["has_return"])]
    p2 = [f for f in fun_funcs if f.get("_object") != "<common>" and not f["_parsed"]["has_params"]]
    p3 = [f for f in fun_funcs if f.get("_object") == "<common>"]

    print(f"Priority Tiers:")
    print(f"  P0 (attributed + signature):  {len(p0):,}")
    print(f"  P2 (attributed, void void):    {len(p2):,}")
    print(f"  P3 (in <common>):              {len(p3):,}")
    print()

    # Suggest next action
    if len(common_fun) > 1000:
        print("Next: Run 'reclassify --apply' to move <common> functions into named objects")
    elif len(p0) > 0:
        print("Next: Run 'propose --priority P0 --limit 50' to start naming attributed FUN_ functions")
    else:
        print("Next: Run 'propose --limit 50' to start naming FUN_ functions")


def main():
    parser = argparse.ArgumentParser(description="FUN_ Pipeline: Classify, prioritize, and name unidentified functions")
    sub = parser.add_subparsers(dest="command")

    p_reclassify = sub.add_parser("reclassify", help="Move <common> FUN_ functions into named objects")
    p_reclassify.add_argument("--apply", action="store_true", help="Write changes to kb.json (default: dry run)")

    p_prioritize = sub.add_parser("prioritize", help="Rank FUN_ functions by priority tier")
    p_prioritize.add_argument("--format", choices=["json", "text"], default="text")
    p_prioritize.add_argument("--priority", choices=["P0", "P1", "P2", "P3"])

    p_propose = sub.add_parser("propose", help="Generate work queue for Ghidra-based naming")
    p_propose.add_argument("--limit", type=int, help="Max proposals to generate")
    p_propose.add_argument("--object", help="Filter by object name")
    p_propose.add_argument("--priority", choices=["P0", "P2"], help="Filter by priority")
    p_propose.add_argument("--output", help="Write proposals to file")

    p_apply = sub.add_parser("apply", help="Apply name proposals to kb.json")
    p_apply.add_argument("--input", required=True, help="JSON file with name proposals")
    p_apply.add_argument("--dry-run", action="store_true")

    sub.add_parser("status", help="Show pipeline status")

    args = parser.parse_args()
    if args.command == "reclassify":
        return cmd_reclassify(args)
    elif args.command == "prioritize":
        return cmd_prioritize(args)
    elif args.command == "propose":
        return cmd_propose(args)
    elif args.command == "apply":
        return cmd_apply(args)
    elif args.command == "status":
        return cmd_status(args)
    else:
        parser.print_help()
        return 1


if __name__ == "__main__":
    sys.exit(main())