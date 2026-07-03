#!/usr/bin/env python3
"""De-risk: inject a TRUE dual-oracle for biped_fix_position (0x1a1430) into
units.c. Oracle = the REAL original at 0x1a1430 (bipeds.c ported=false), called
by address. Candidate = a verbatim copy of our lifted biped_fix_position
(extracted from bipeds.c, renamed) so a MISMATCH cannot be a transcription error.

Output mode: final_position=&buf, dont_teleport=1 -> zero object mutation; the
collision-user-depth global 0x4761d8 is bracketed (functions assert caller
depth>=1). One-shot per session on the first live biped."""
import re

BIP = "src/halo/units/bipeds.c"
UNITS = "src/halo/units/units.c"

# --- 1. extract biped_fix_position verbatim, rename -> candidate -----------
src = open(BIP).read().splitlines(keepends=True)
start = next(i for i, l in enumerate(src) if l.startswith("char biped_fix_position("))
# find matching close brace by brace counting from the first '{' after sig
depth = 0
seen = False
end = None
for i in range(start, len(src)):
    depth += src[i].count("{") - src[i].count("}")
    if "{" in src[i]:
        seen = True
    if seen and depth == 0:
        end = i
        break
assert end is not None, "could not find end of biped_fix_position"
body = "".join(src[start:end + 1])
cand = body.replace("char biped_fix_position(",
                    "static char dualoracle_1430_candidate(", 1)

helper = '''
/* ========================================================================
 * TRUE DUAL-ORACLE — biped_fix_position (0x1a1430)  [DE-RISK; remove after]
 *
 * dualoracle_1430_candidate (above): a VERBATIM copy of our lifted
 *   biped_fix_position, extracted programmatically (not hand-typed) so a
 *   MISMATCH cannot be a transcription error. Tests our source-logic.
 * dualoracle_1430_true: calls the REAL original at 0x1a1430 (bipeds.c is
 *   ported=false, so the original machine code lives at that address) as the
 *   oracle, then the candidate copy, on the SAME live biped, and compares the
 *   computed final_position (3 floats) + char return.
 *
 * Clean-capture mode: final_position=&buf, dont_teleport=1 -> NO object
 * mutation (the obj+0xc write/translate at 0x1a1514 is gated by !dont_teleport),
 * so the two back-to-back calls see identical input. The collision-user-depth
 * stack at 0x4761d8 is bracketed to 1 (the function's epilogue asserts depth>=2,
 * i.e. requires caller context depth>=1) and restored afterwards.
 * ===================================================================== */
static void dualoracle_1430_true(int handle)
{
  typedef char (*fix_fn)(int, int, float *, float *, float, char, char, char);
  fix_fn orig = (fix_fn)0x1a1430;
  float final_o[3];
  float final_c[3];
  char ret_o;
  char ret_c;
  short saved_depth;
  int match;
  char buf[256];

  final_o[0] = final_o[1] = final_o[2] = 0.0f;
  final_c[0] = final_c[1] = final_c[2] = 0.0f;

  saved_depth = *(short *)0x4761d8;
  *(short *)0x4761d8 = 1; /* satisfy caller-context depth assert */
  ret_o = orig(handle, -1, (float *)0, final_o, 2.0f, 1, 1, 1);
  *(short *)0x4761d8 = 1; /* reset entry depth for the candidate call */
  ret_c = dualoracle_1430_candidate(handle, -1, (float *)0, final_c,
                                    2.0f, 1, 1, 1);
  *(short *)0x4761d8 = saved_depth; /* restore */

  match = (ret_o == ret_c) &&
          (*(unsigned int *)&final_o[0] == *(unsigned int *)&final_c[0]) &&
          (*(unsigned int *)&final_o[1] == *(unsigned int *)&final_c[1]) &&
          (*(unsigned int *)&final_o[2] == *(unsigned int *)&final_c[2]);

  crt_sprintf(buf,
    "DUALORACLE|1430|handle=%08X|ret(o=%d,c=%d)|"
    "fo=%08X,%08X,%08X|fc=%08X,%08X,%08X|%s\\n",
    handle, (int)ret_o, (int)ret_c,
    *(unsigned int *)&final_o[0], *(unsigned int *)&final_o[1],
    *(unsigned int *)&final_o[2],
    *(unsigned int *)&final_c[0], *(unsigned int *)&final_c[1],
    *(unsigned int *)&final_c[2],
    match ? "MATCH" : "MISMATCH");
  debug_string_to_display(buf, 0);
}

'''

u = open(UNITS).read()
if "dualoracle_1430_candidate" in u:
    raise SystemExit("already injected; aborting to avoid duplicate")

anchor = "static void dualoracle_2b10_tick_probe(void)"
assert anchor in u, "probe anchor not found"
u = u.replace(anchor, cand + "\n" + helper + anchor, 1)

# add a one-shot static + call inside the probe's per-biped block
u = u.replace("  static int pop_logged = 0;\n",
              "  static int pop_logged = 0;\n  static int did_1430 = 0;\n", 1)

call_anchor = "      samples += 1;\n      if (samples >= 5) {\n        done = 1;\n      }\n"
inject = ("      if (did_1430 == 0) {\n"
          "        did_1430 = 1;\n"
          "        dualoracle_1430_true(handle);\n"
          "      }\n")
assert call_anchor in u, "samples-increment anchor not found"
u = u.replace(call_anchor, inject + call_anchor, 1)

open(UNITS, "w").write(u)
print("injected dualoracle_1430_candidate (%d lines) + helper + probe call"
      % cand.count("\n"))
