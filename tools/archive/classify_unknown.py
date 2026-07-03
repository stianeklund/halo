#!/usr/bin/env python3
import sys, os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

"""Classify 30 unknown (null-named) functions into proper kb.json objects.

Based on binary evidence from Ghidra: __FILE__ strings, address ranges,
cross-references, and functional analysis.
"""

import json
import sys

CLASSIFICATIONS = {
    # addr -> target object name
    # Existing objects
    "0x83310": "transport_endpoint_set_winsock.obj",
    "0x8dae0": "cseries.obj",
    "0x9c750": "effects.obj",
    "0xde3b0": "first_person_weapons.obj",
    "0xf0ea0": "ui_widget_game_data_input_functions.obj",
    "0x184ba0": "render.obj",

    # New: weapons.obj
    "0xfae80": "weapons.obj",
    "0xfb0c0": "weapons.obj",
    "0xfb140": "weapons.obj",
    "0xfb3c0": "weapons.obj",
    "0xfb6e0": "weapons.obj",
    "0xfba20": "weapons.obj",
    "0xfcf20": "weapons.obj",

    # New: edit_text.obj
    "0x972b0": "edit_text.obj",
    "0x973a0": "edit_text.obj",

    # New: international_strings.obj
    "0x19d1b0": "international_strings.obj",
    "0x19d240": "international_strings.obj",
    "0x19d300": "international_strings.obj",

    # New: XAPILIB stubs
    "0x1d0da1": "XAPILIB:xvutil.obj",
    "0x1d0e96": "XAPILIB:fileio.obj",
    "0x1d2240": "XAPILIB:lasterr.obj",
    "0x1d36d4": "XAPILIB:physmem.obj",
    "0x1d371d": "XAPILIB:physmem.obj",

    # New: LIBCMT stubs
    "0x1d9179": "LIBCMT:snprintf.obj",
    "0x1d9d06": "LIBCMT:rand.obj",

    # New: D3D8 stubs
    "0x1ed7d0": "D3D8:d3dresource.obj",
    "0x1f4140": "D3D8:d3dsurface.obj",
    "0x1f44f0": "D3D8:d3dsurface.obj",

    # New: XNET stubs
    "0x225c20": "XNET:wsock.obj",
    "0x225cc6": "XNET:wsock.obj",
}


def main():
    with open("kb.json") as f:
        kb = json.load(f)

    objects = kb["objects"]

    # Find the null-named object
    null_idx = None
    for i, obj in enumerate(objects):
        if obj["name"] is None:
            null_idx = i
            break

    if null_idx is None:
        print("ERROR: No null-named object found", file=sys.stderr)
        sys.exit(1)

    null_obj = objects[null_idx]
    null_funcs = null_obj["functions"]

    # Build lookup of existing objects by name
    obj_by_name = {}
    for i, obj in enumerate(objects):
        if obj["name"] is not None:
            obj_by_name[obj["name"]] = i

    # Track functions to remove from null object and add to targets
    moves = {}  # target_name -> [func_entries]
    remaining = []
    moved_count = 0

    for func in null_funcs:
        addr = func.get("addr")
        target = CLASSIFICATIONS.get(addr)
        if target:
            moves.setdefault(target, []).append(func)
            moved_count += 1
        else:
            remaining.append(func)

    # Apply moves
    new_objects = []
    for target_name, funcs in sorted(moves.items()):
        if target_name in obj_by_name:
            idx = obj_by_name[target_name]
            objects[idx]["functions"].extend(funcs)
            # Sort by address
            objects[idx]["functions"].sort(
                key=lambda f: int(f["addr"], 16) if f.get("addr") else 0
            )
            print(f"  Extended {target_name}: +{len(funcs)} function(s)")
        else:
            new_obj = {"name": target_name, "functions": sorted(
                funcs, key=lambda f: int(f["addr"], 16) if f.get("addr") else 0
            )}
            new_objects.append(new_obj)
            print(f"  Created {target_name}: {len(funcs)} function(s)")

    # Update null object with remaining functions (and keep data)
    if remaining or null_obj.get("data"):
        null_obj["functions"] = remaining
        print(f"  Null object: {len(remaining)} function(s) remaining, {len(null_obj.get('data', []))} data entries kept")
    else:
        objects.pop(null_idx)
        print("  Removed empty null object")

    # Insert new objects (before the null/end)
    for new_obj in new_objects:
        objects.append(new_obj)

    kb["objects"] = objects

    with open("kb.json", "w") as f:
        json.dump(kb, f, indent=2)
        f.write("\n")

    print(f"\nDone: moved {moved_count} functions, created {len(new_objects)} new objects")


if __name__ == "__main__":
    main()
