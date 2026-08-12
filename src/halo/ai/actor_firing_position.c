/* actor_firing_position.c — AI firing-position group selection.
 *
 * Corresponds to actor_firing_position.obj. Source path confirmed via
 * __FILE__ assert xref: c:\halo\SOURCE\ai\actor_firing_position.c
 * (the assert at 0x24b3d stamps that literal).
 *
 * Ported:
 *   FUN_00024370 (0x24370) — score one candidate firing position: resolve the
 *     actor's 'actr' tag, then either accumulate a penalty (no candidate) or
 *     test 3D path availability to the candidate and either credit it via
 *     FUN_00024000 or mark it rejected.
 *   FUN_00024770 (0x24770) — the twin evaluator from the same dispatch table:
 *     score a candidate by its aiming kind (0 / 1 / default) rather than by
 *     path availability, crediting the same FUN_00024000 accumulator.
 *   actor_get_firing_position_group (0x24a60) — map an actor plus a
 *     group-selector and a searching-state override onto one of the squad
 *     definition's firing-position group indices, returned as the int stored
 *     at squad + 0x54 + group*4.
 *   actor_clear_discarded_firing_positions (0x24b80) — reset the discarded-
 *     firing-position ring buffer at actor+0x3c8.
 *   FUN_00024be0 (0x24be0) — push one discarded firing position onto that
 *     ring buffer and cache the position's first 12 bytes at actor+0x3dc.
 *
 * Layout used here (all offsets read directly from the listing):
 *   actor_t  +0x034  uint32  encounter index, -1 == NONE (CMP dword,-1)
 *            +0x03a  int16   squad index      (MOVSX word)
 *            +0x098  char    "searching" state flag (TEST CL,CL)
 *            +0x374  char    "currently defending" flag
 *   scenario +0x42c  tag_block encounters, element stride 0xb0
 *   encounter_definition +0x80 squads tag_block, element stride 0xe8
 *   squad    +0x54 .. +0x6c  array of 7 firing-position group indices (int)
 *
 * The binary returns early for selector 1 and selector 5, loading
 * [squad+0x68] and [squad+0x6c] directly. Those are 0x54 + 5*4 and
 * 0x54 + 6*4 — i.e. MSVC tail-duplicated the common indexed load with the
 * constant folded in, not two distinct source statements. The single-tail
 * shape below is the source form.
 */

#include "../../common.h"
#include "encounters.h"

/* Bound from the assert's own range check: CMP SI,7 / JL. The identifier is
 * verbatim from the assert predicate string in the XBE. */
#define NUMBER_OF_FIRING_POSITION_GROUPS 7

/* Sort context consumed by FUN_00024950. The look-scoring pass in
 * actor_looking.c publishes the candidate-record array base and the record
 * count into these two adjacent globals (stores at 0x26de8/0x26def) just
 * before handing FUN_00024950 to the generic sort FUN_00091ef0.
 *
 * Both identifiers are verbatim from the assert predicate strings in the XBE
 * (0x254d88 / 0x254d40 / 0x254cf8), so they are string-proven, not inferred.
 *
 * Widths are load-bearing and differ between the two: the count is a 16-bit
 * global, read with MOVSX ECX,word ptr [0x331f00] at 0x24995/0x249c7; the
 * array base is a full dword, read with MOV EAX,[0x331f04] at 0x24953. */
#define global_temporary_sort_firing_position_array (*(char **)0x331f04)
#define global_temporary_sort_firing_position_count (*(int16_t *)0x331f00)

/* The float clamp at 0x241de..0x241ef keeps both candidate values live on the
 * x87 stack and throws one away (FLD 0.0f; FCOMP ST(1); then either FSTP ST(0)
 * + FLD 0.0f, or fall through with the computed value still in ST(0)). A plain
 * `if (v < 0.0f) v = 0.0f;` would have stored v to memory and compared against
 * it, so the original had to be a max macro over a single expression. The two
 * int16 accumulators in the second pass reduce the same way (both arms of the
 * memory-resident one store, at 0x242e1 / 0x242e6). */
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

/* FUN_00024060 (0x24060) — the "close is good" distance ramp over a whole
 * candidate firing-position array.
 *
 * Four cdecl stack parameters, same shape as FUN_00024130 below (its twin in
 * the same dispatch family): [EBP+8] is never read in the body — a dead
 * parameter kept for the shared signature — [EBP+0xc] is the eval_state
 * pointer (only field touched is the float at +0x18), [EBP+0x10] is read as
 * MOV AX,word ptr / TEST AX,AX / JLE, i.e. a signed int16 count, and
 * [EBP+0x14] is the candidate array base. kb.json previously declared this
 * void(void); that was wrong.
 *
 * The PUSH EBX/ESI/EDI saves sit *inside* the count>0 branch, after the JLE —
 * the empty-count path touches only EBP, which is what a plain leading
 * `for (i = 0; i < count; i++)` compiles to once MSVC inverts the loop
 * (MOVZX EBX,AX then DEC EBX / JNZ).
 *
 * After the prologue MSVC reuses the incoming parameter home slots as float
 * scratch: [EBP+0x10] holds `evaluation` and [EBP+0x14] holds `threshold`.
 * Ghidra's `_in_stack_0000000c` is that spilled local, not a write back
 * through a parameter.
 *
 * ESI is loaded as firing_positions + 8, so every ESI-relative displacement in
 * the listing is 8 short of the true record offset. The record is the same
 * 0x3c-stride candidate FUN_00024130 / FUN_00024370 / FUN_00024950 walk:
 *   +0x08  float  the candidate's distance
 *   +0x30  char   usable flag
 *   +0x38  float  the accumulated score
 *
 * Branch senses are both the MSVC `<` idiom (FLD lhs; FCOMP rhs; TEST AH,0x5;
 * JP over-the-then-block), so the JP-taken edge is the >= case and the
 * fallthrough is the `<` case — the 8.0f store at 0x240b3 is the then-block of
 * `distance < threshold`, not an else.
 *
 * Constants read out of .rdata rather than guessed: [0x253398] = 0.5f,
 * [0x2533c8] = 1.0f, [0x253f78] = 8.0f, [0x2533c0] = 0.0f, [0x254cb8] =
 * 1000.0f. The 8.0f fallback is stored as the immediate 0x41000000.
 *
 * x87 association is load-bearing: 0x2408f..0x240a0 evaluate (eval_state+0x18
 * minus distance) first (FLD [EDI+0x18]; FSUB [ESI]), then form the reciprocal
 * factor on a fresh stack slot (FLD 1.0f; FDIV threshold) and FMULP, then
 * FMUL 8.0f. Ghidra printed the two factors in the opposite order. */
void FUN_00024060(int actor_handle, char *eval_state,
                  short firing_position_count, char *firing_positions)
{
  char *firing_position;
  float threshold;
  float evaluation;
  short i;

  (void)actor_handle;

  firing_position = firing_positions;
  for (i = 0; i < firing_position_count; i++) {
    if (firing_position[0x30] != '\0') {
      evaluation = 0.0f;
      threshold = *(float *)(eval_state + 0x18) * 0.5f;
      if (*(float *)(firing_position + 8) < threshold) {
        evaluation = 8.0f;
      } else if (*(float *)(firing_position + 8) <
                 *(float *)(eval_state + 0x18)) {
        evaluation =
          (*(float *)(eval_state + 0x18) - *(float *)(firing_position + 8)) *
          (1.0f / threshold) * 8.0f;
        assert_halt_at("c:\\halo\\SOURCE\\ai\\actor_firing_position.c", 0x81,
                       (evaluation >= 0.0f) && (evaluation < 1e+03f));
      }
      *(float *)(firing_position + 0x38) += evaluation;
    }
    firing_position += 0x3c;
  }
}

/* FUN_00024130 (0x24130) — run the two shared scoring passes over a whole
 * candidate firing-position array.
 *
 * Confirmed from the listing at 0x24130:
 *   MOV EAX,[0x6325a4] (actor_data); PUSH ESI (actor_handle) -> datum_get. The
 *   resolved datum is discarded — EAX is overwritten by the very next call at
 *   0x24149 — so it was a dead local in the original source. The call itself is
 *   kept: it is a real call with the pool's own bounds asserts behind it.
 *   PUSH ESI -> actor_combat_get_firing_variant_definition, result into ECX.
 *
 * Four cdecl stack parameters, read at [EBP+8], [EBP+0xc], [EBP+0x10] (MOV BX,
 * word ptr — an int16) and [EBP+0x14]. kb.json previously declared this
 * void(void); that was wrong.
 *
 * Pass 1 (0x241b1..0x24248) adds a distance-ramp score scaled by a weight
 * shared across the whole array; pass 2 (0x24265..0x2435a) adds an aiming
 * blockage score. Both walk the same 0x3c-stride candidate record that
 * FUN_00024370 and FUN_00024950 use:
 *   +0x00  int    the object the aiming/blockage test is run against
 *   +0x08  float  the candidate's distance
 *   +0x30  char   usable flag
 *   +0x38  float  the accumulated score
 *
 * eval_state offsets, all straight off the listing; meanings are unproven, so
 * they stay as offsets:
 *   +0x018 float  distance normaliser for pass 1
 *   +0x254 int16  number of 0x1c-byte aiming records
 *   +0x256 int16  guard — pass 2 is skipped entirely when this is <= 0
 *   +0x25c        base of the 0x1c-byte aiming-record array (LEA ESI,[EAX+EDX]
 *                 after IMUL EAX,EAX,0x1c, then disp 0x25c/0x260/0x26c), each
 *                 record being an int16 kind at +0x00 and two 12-byte vectors
 *                 at +0x04 and +0x10
 *   +0x5fc char   enables the pass-1 weight ramp
 *   +0x600 float  the value that ramp is computed from
 *
 * FPU direction is load-bearing: 0x24188 and 0x241d8 are FSUBR against the
 * 1.0f at 0x2533c8, so both ramps are (1.0f - x/y), never (x/y - 1.0f).
 *
 * The blockage callee returns an int16 in AX (CMP CX,AX at 0x242dc, CMP BX,AX /
 * MOV EBX,EAX at 0x242f1) and takes four cdecl arguments (ADD ESP,0x10 at
 * 0x242d1). kb.json declared it void(void); corrected as part of this port.
 * Its kind field is re-read from the record after the call (MOV SI,[ESI+0x25c]
 * at 0x242ca) rather than reusing the value the guard loaded. */
void FUN_00024130(int actor_handle, char *eval_state,
                  short firing_position_count, char *firing_positions)
{
  char *definition;
  char *firing_position;
  char *aim_record;
  float weight;
  float evaluation;
  float blockage_score;
  short worst_kind0;
  short worst_kind1;
  short blockage;
  short i;
  short j;

  datum_get(actor_data, actor_handle);
  definition = actor_combat_get_firing_variant_definition(actor_handle);

  weight = 8.0f;
  if ((*(char *)(eval_state + 0x5fc) == '\0' ||
       (*(float *)(eval_state + 0x600) <= *(float *)(definition + 0x74) &&
        (weight = (1.0f - *(float *)(eval_state + 0x600) /
                            *(float *)(definition + 0x74)) *
                  8.0f) > 0.0f)) &&
      firing_position_count > 0) {
    firing_position = firing_positions;
    for (i = 0; i < firing_position_count; i++) {
      if (firing_position[0x30] != '\0' &&
          *(float *)(firing_position + 8) < *(float *)(eval_state + 0x18)) {
        evaluation = MAX(0.0f, 1.0f - *(float *)(firing_position + 8) /
                                        *(float *)(eval_state + 0x18)) *
                     weight;
        assert_halt_at("c:\\halo\\SOURCE\\ai\\actor_firing_position.c", 0x81,
                       (evaluation >= 0.0f) && (evaluation < 1e+03f));
        *(float *)(firing_position + 0x38) += evaluation;
      }
      firing_position += 0x3c;
    }
  }

  if (*(short *)(eval_state + 0x256) > 0 && firing_position_count > 0) {
    firing_position = firing_positions;
    for (i = 0; i < firing_position_count; i++) {
      if (firing_position[0x30] != '\0') {
        worst_kind0 = 0;
        worst_kind1 = 0;
        for (j = 0; j < *(short *)(eval_state + 0x254); j++) {
          aim_record = eval_state + j * 0x1c;
          if (*(short *)(aim_record + 0x25c) == 0 ||
              *(short *)(aim_record + 0x25c) == 1) {
            blockage = actor_perception_aiming_vector_test_blockage(
              (float *)(aim_record + 0x260), (float *)(aim_record + 0x26c),
              *(int *)firing_position, 0);
            if (*(short *)(aim_record + 0x25c) == 0)
              worst_kind0 = MAX(worst_kind0, blockage);
            else if (*(short *)(aim_record + 0x25c) == 1)
              worst_kind1 = MAX(worst_kind1, blockage);
          }
        }
        if (worst_kind1 >= 2)
          blockage_score = 0.0f;
        else if (worst_kind1 >= 1)
          blockage_score = 1.5f;
        else if (worst_kind0 >= 2)
          blockage_score = 6.0f;
        else if (worst_kind0 >= 1)
          blockage_score = 8.5f;
        else
          blockage_score = 10.0f;
        *(float *)(firing_position + 0x38) += blockage_score;
      }
      firing_position += 0x3c;
    }
  }
}

/* FUN_00024370 (0x24370) — score one candidate firing position for an actor.
 *
 * Confirmed from the listing at 0x24370:
 *   MOV EAX,[0x6325a4] (actor_data); PUSH EBX (actor_handle) -> datum_get.
 *   EDI = the actor datum. PUSH [EDI+0x58]; PUSH 0x61637472 ('actr');
 *   CALL tag_get — the result is discarded, the call is made for its side
 *   effect. The single ADD ESP,0x10 at 0x2439e pays for BOTH calls' pushes,
 *   so it is not evidence of a 4-argument tag_get.
 *
 *   The addend at 0x243b4 is FADD DWORD PTR [0x254cc0]; that address holds
 *   0x41700000 == 15.0f — VC71's literal pool for the same 15.0f pushed to
 *   FUN_00024000 at 0x2440d. It is a source literal, not a game global.
 *
 *   `dist` occupies the (dead) third parameter's home slot in the original:
 *   MOV DWORD PTR [EBP+0x10],0 / LEA EDX,[EBP+0x10] while ESI still holds
 *   firing_position. It is a distinct local — Ghidra's
 *   `fVar = (float)in_stack_0000000c` is the param-slot-reuse artifact.
 *
 *   path_3d_available's third parameter is declared int in kb.json but here
 *   carries a float bit pattern (MOV EDX,[EBP+0x10]; PUSH EDX reads back the
 *   float actor_path_3d_available wrote). Reinterpret the bits; converting
 *   would truncate.
 *
 *   Both `*(float **)firing_position` loads are re-read from memory in the
 *   original (MOV EAX,[ESI] at 0x243c9, MOV ECX,[ESI] at 0x243e4), so the
 *   expression appears twice here rather than being cached.
 *
 *   Return is 32 bits wide (MOV EAX,1 twice, MOVZX EAX,BYTE PTR [ESI+0x30]
 *   once), so the return type is int-width, not char.
 *
 * Offsets used (raw; struct identities not yet proven):
 *   actor           +0x058 int      tag index handed to tag_get('actr')
 *                   +0x12c float[3] actor position (path start point)
 *   eval_state      +0x014 char     "keep rejected positions" gate
 *                   +0x044 char     master gate
 *                   +0x660 float    rejected-position accumulator
 *   firing_position +0x000 float *  candidate position
 *                   +0x030 char     usable flag (also the return value)
 *                   +0x031 char     rejected flag
 */
int FUN_00024370(int actor_handle, char *eval_state, char *firing_position)
{
  char *actor;
  float dist;

  actor = (char *)datum_get(actor_data, actor_handle);
  tag_get(0x61637472 /* 'actr' */, *(int *)(actor + 0x58));

  if (*(char *)(eval_state + 0x44) != '\0') {
    if (firing_position == (char *)0) {
      *(float *)(eval_state + 0x660) += 15.0f;
      return 1;
    }

    dist = 0.0f;
    if (actor_path_3d_available(actor_handle, *(float **)firing_position,
                                &dist) != '\0' &&
        path_3d_available((int)scenario_get(), (int *)(actor + 0x12c),
                          *(int *)&dist, *(int **)firing_position,
                          (unsigned char *)0, (float *)0) != '\0') {
      FUN_00024000(eval_state, 15.0f, 0x19, firing_position);
    } else {
      *(char *)(firing_position + 0x31) = 1;
      if (*(char *)(eval_state + 0x14) == '\0')
        *(char *)(firing_position + 0x30) = '\0';
    }
  }

  if (firing_position == (char *)0)
    return 1;
  return *(unsigned char *)(firing_position + 0x30);
}

/* FUN_00024770 (0x24770) — score one candidate firing position against the
 * actor's aiming state. Sibling of FUN_00024370: both addresses are stored as
 * function pointers eight bytes apart in the evaluator dispatch table
 * (0x254c30 holds 0x24370, 0x254c38 holds 0x24770), and neither is reached by
 * a direct CALL anywhere in the image, so the three-parameter prototype and
 * the int-width return are the twin's shape.
 *
 * The first parameter is DEAD here: nothing in 0x24770..0x24848 touches
 * [EBP+0x8]. The name is carried over from the table's other entries, not
 * proven from this listing.
 *
 * Confirmed from the listing:
 *   0x24773  MOV EAX,[EBP+0xC]  — eval_state stays in EAX for the whole body.
 *   0x2477f  MOV ESI,[EBP+0x10] — firing_position stays in ESI and is
 *            re-tested at the shared tail 0x24835 to choose the return value.
 *            Ghidra collapses that tail to `if (x == 0) return; return;` and
 *            loses the value entirely.
 *
 *   0x24796/0x247b0 hold the only x87 in the function, and they load the
 *   CONSTANT first: FLD [0x254640] / FADD [EAX+0x660] / FSTP [EAX+0x660].
 *   That is `field = C + field`, not `field += C` — the twin emits the
 *   compound form at 0x243b4, where the constant is instead the FADD operand.
 *   Writing `+=` here swaps FLD and FADD. Both addresses are VC71 literal-pool
 *   slots rather than game globals: [0x254640] == 0x40C00000 == 6.0f and
 *   [0x254CC0] == 0x41700000 == 15.0f, the latter shared with the twin.
 *
 *   0x247ce  SUB ECX,0 / JE, then DEC ECX / JE — MSVC's switch decrement
 *   chain, not two CMPs, so the kind dispatch is a switch and not an
 *   if/else-if ladder. The `score = 0.0f` seed is scheduled into the branch
 *   shadow at 0x247d1, ahead of the first JE, which is why the default arm
 *   reaches the call with zero without ever storing a score.
 *
 *   `score` reuses the second parameter's home slot: [EBP+0xC] is rewritten
 *   with float bit patterns at 0x247fc/0x24805/0x24816/0x2481f long after EAX
 *   already holds eval_state. It is a distinct float local — Ghidra's
 *   `in_stack_00000008 = (void *)0x40c00000` is that slot reuse, and passing
 *   it as a pointer would convert the value instead of reinterpreting it.
 *
 *   0x2482d pushes 0xE, then score, then eval_state (ADD ESP,0xC pays for
 *   exactly three) while firing_position is still live in ESI, matching
 *   FUN_00024000's @<esi> fourth parameter.
 *
 * Offsets used (raw; struct identities not yet proven):
 *   eval_state      +0x014 char   "keep rejected positions" gate (as the twin)
 *                   +0x5fc char   master gate
 *                   +0x628 char   selects the smaller score in every arm
 *                   +0x660 float  rejected-position accumulator
 *   firing_position +0x006 int16  aim kind, dispatched 0 / 1 / default
 *                   +0x030 char   usable flag (also the return value)
 *                   +0x031 char   rejected flag
 */
int FUN_00024770(int actor_handle, char *eval_state, char *firing_position)
{
  float score;

  (void)actor_handle; /* DEAD: no [EBP+8] access in disasm */

  if (*(char *)(eval_state + 0x5fc) != '\0') {
    if (firing_position == (char *)0) {
      if (*(char *)(eval_state + 0x628) != '\0')
        *(float *)(eval_state + 0x660) = 6.0f + *(float *)(eval_state + 0x660);
      else
        *(float *)(eval_state + 0x660) = 15.0f + *(float *)(eval_state + 0x660);
      return 1;
    }

    score = 0.0f;
    switch (*(short *)(firing_position + 6)) {
    case 0:
      score = (*(char *)(eval_state + 0x628) != '\0') ? 6.0f : 15.0f;
      break;
    case 1:
      score = (*(char *)(eval_state + 0x628) != '\0') ? 2.5f : 5.0f;
      break;
    default:
      if (*(char *)(eval_state + 0x628) == '\0') {
        *(char *)(firing_position + 0x31) = 1;
        if (*(char *)(eval_state + 0x14) == '\0')
          *(char *)(firing_position + 0x30) = '\0';
      }
      break;
    }
    FUN_00024000(eval_state, score, 0xe, firing_position);
  }

  if (firing_position == (char *)0)
    return 1;
  return *(unsigned char *)(firing_position + 0x30);
}

/* FUN_00024950 (0x24950) — ordering predicate for the candidate
 * firing-position records built by actor_looking.c. Handed to the generic sort
 * FUN_00091ef0 as its comparator, which passes indices (not pointers), so the
 * record array and its count travel in the two globals above.
 *
 * Returns a bool in AL, not an int: the two float exits at 0x24a4f/0x24a51 are
 * a bare `MOV AL,1` / `XOR AL,AL` with the upper three bytes of EAX left
 * undefined. (The two byte-flag exits do clear EAX first, but that is just
 * MSVC's partial-register-stall idiom in front of SETcc.)
 *
 * Record layout, read straight off the listing — element stride 0x3c from
 * IMUL ESI,ESI,0x3c at 0x24966, matching the 0x200 * 0x3c record buffer in
 * actor_looking.c. Field meanings are unproven, so they stay as offsets:
 *   +0x30  char   the same flag FUN_00024370 clears on a rejected candidate
 *   +0x31  char   the same flag FUN_00024370 sets on a rejected candidate
 *   +0x38  float  the accumulated score
 *
 * The two byte-flag branches are written with an explicit 1/-1 rank compared
 * against zero because that is what the binary computes, and the round trip is
 * visible in the codegen rather than folded away:
 *   0x249fa  XOR EDX,EDX / TEST AL,AL / SETE DL      ; rank = (flag == 0)
 *   0x24a06  LEA EDX,[EDX+EDX-1]                     ; rank = 2*rank - 1
 *   0x24a0a  TEST EDX,EDX / SETG AL                  ; return rank > 0
 * A plain `return p1[0x30] == 0;` collapses to SETE AL and loses the LEA, so
 * the ternary form is the faithful one. Note the two branches use OPPOSITE
 * senses — SETE at +0x30 (0x249fe) but SETNE at +0x31 (0x24a1d) — despite
 * looking symmetric.
 *
 * The float tail nets out to one strict `<`, but it is written as a nested
 * `<=` then `<` because that is the guard shape the binary emits, and the two
 * FLD/FCOMP pairs are two distinct source comparisons, not one re-load:
 *   0x24a38  TEST AH,0x41 / JZ  -> mask C3|C0. The jump is TAKEN when the mask
 *            is zero, i.e. exactly when a > b, and it lands on the shared
 *            XOR AL,AL. The guard is therefore the NEGATED greater-than, not
 *            `a <= b`: the two differ only for NaN, which sets all of C3/C2/C0
 *            and so leaves the mask non-zero and ENTERS the block. Writing
 *            `a <= b` makes VC71 emit JP instead of JE to exclude NaN up
 *            front, which costs the last instruction of the match. NaN still
 *            returns 0 either way, because the inner `a < b` rejects it.
 *   0x24a45  TEST AH,0x5  / JP  -> mask C0|C2. a<b gives 0x01 (PF clear, no
 *            jump, MOV AL,1); a==b gives 0x00 and NaN gives 0x05 (PF set, both
 *            jump to XOR AL,AL). So NaN orders false, as a strict `<` should.
 * Writing the flattened `if (a > b) return 0; return a < b;` is semantically
 * identical but inverts the first guard to JNE and costs six instructions of
 * branch layout — vc71_verify flags it as [FCOM-WARN] bound-sense.
 *
 * The record pointers are formed before the asserts because that is the
 * emitted order: the array base is loaded at 0x24953 and both IMUL/ADD pairs
 * complete at 0x2496b, ahead of the TEST EAX,EAX at 0x2496d that guards the
 * first assert. Nothing is dereferenced until after all three asserts pass. */
bool FUN_00024950(long index1, long index2)
{
  char *record1;
  char *record2;

  record1 = global_temporary_sort_firing_position_array + index1 * 0x3c;
  record2 = global_temporary_sort_firing_position_array + index2 * 0x3c;

  assert_halt_at("c:\\halo\\SOURCE\\ai\\actor_firing_position.c", 0x50f,
                 global_temporary_sort_firing_position_array);
  assert_halt_at("c:\\halo\\SOURCE\\ai\\actor_firing_position.c", 0x510,
                 (index1 >= 0) &&
                   (index1 < global_temporary_sort_firing_position_count));
  assert_halt_at("c:\\halo\\SOURCE\\ai\\actor_firing_position.c", 0x511,
                 (index2 >= 0) &&
                   (index2 < global_temporary_sort_firing_position_count));

  if (record1[0x30] != record2[0x30])
    return ((record1[0x30] == 0) ? 1 : -1) > 0;
  if (record1[0x31] != record2[0x31])
    return ((record1[0x31] != 0) ? 1 : -1) > 0;

  if (!(*(float *)(record1 + 0x38) > *(float *)(record2 + 0x38))) {
    if (*(float *)(record1 + 0x38) < *(float *)(record2 + 0x38))
      return 1;
  }
  return 0;
}

int actor_get_firing_position_group(int actor_handle, short param_2,
                                    int param_3)
{
  actor_t *actor;
  encounter_definition *encounter;
  char *squad;
  char searching;
  short group_index;

  actor = (actor_t *)datum_get(actor_data, actor_handle);
  if (actor->field_034 == 0xffffffff)
    return 0;

  encounter = (encounter_definition *)tag_block_get_element(
    (char *)global_scenario_get() + 0x42c, actor->field_034 & 0xffff, 0xb0);
  squad =
    (char *)tag_block_get_element(&encounter->squads, actor->field_03a, 0xe8);

  /* The override is compared 16 bits wide in the original (CMP with a word
   * operand); do not widen it to a full int compare. */
  searching = actor->field_098;
  if ((short)param_3 == 1)
    searching = 1;
  else if ((short)param_3 == 2)
    searching = 0;

  if (param_2 == 1) {
    group_index = 5;
  } else if (param_2 == 4) {
    /* Branchless in the original: NEG AL / SBB EAX,EAX / AND EAX,3 / ADD EAX,2.
     * Written either as `5 : 2` or as `2 + (cond ? 3 : 0)`, VC71 emits the
     * identical sequence (both measured at 88.9%), so the simpler form stands.
     */
    group_index = (short)(actor->field_374 != 0 ? 5 : 2);
  } else if (param_2 == 5) {
    group_index = 6;
  } else if (actor->field_374 != 0) {
    group_index = (short)(3 + (searching != 0));
  } else {
    group_index = (short)(searching != 0);
  }

  /* Signed range check: TEST SI,SI / JL then CMP SI,7 / JL. */
  if (group_index < 0 || group_index >= NUMBER_OF_FIRING_POSITION_GROUPS) {
    display_assert("(index >= 0) && (index < NUMBER_OF_FIRING_POSITION_GROUPS)",
                   "c:\\halo\\SOURCE\\ai\\actor_firing_position.c", 0x584, 1);
    system_exit(-1);
  }

  return *(int *)(squad + 0x54 + group_index * 4);
}

/* actor_clear_discarded_firing_positions (0x24b80) — reset the actor's memory
 * of firing positions it has already discarded.
 *
 * Confirmed from the listing at 0x24b80 (95 bytes, bare PUSH EBP/MOV EBP,ESP
 * frame, no locals, no FPU, one CALL):
 *   MOV EAX,[EBP+0x8]; MOV ECX,[0x6325a4]; PUSH EAX; PUSH ECX; CALL datum_get;
 *   ADD ESP,0x8  ->  datum_get(actor_data, actor_handle).
 *   MOV word ptr [EAX+0x3c6],0 — a 16-bit store, not a byte or a dword.
 *   LEA ECX,[EAX+0x3ca]; MOV EDX,4; {MOV word ptr [ECX],0xffff; ADD ECX,0x4;
 *   DEC EDX; JNZ} — four int16 NONE stores on a 4-byte stride, touching only
 *   the low half of each 4-byte slot. See the actor_t comment at +0x3ca: the
 *   record boundary is unproven, so the table is walked by explicit byte
 *   stride rather than indexed through a made-up element type.
 *
 *   The tail is a short-circuit chain of byte compares, in this exact order:
 *   CL=[EAX+0x3d8] (zero -> return); CL=[EBP+0xc] (zero -> take the store);
 *   CL=[EAX+0x3d9] (zero -> return, else store). Reordering the && / || here
 *   changes the branch layout, so the shape below is deliberate.
 *
 *   param2 is declared int in kb.json but only its low byte is ever read
 *   (MOV CL,[EBP+0xc]); both in-repo call sites pass 0 or 1. The (char) cast
 *   reproduces the byte compare without touching the immutable declaration.
 */
void actor_clear_discarded_firing_positions(int actor_handle, int param2)
{
  actor_t *actor;
  int16_t *index;
  int count;

  actor = (actor_t *)datum_get(actor_data, actor_handle);

  actor->field_3c6 = 0;

  index = &actor->field_3c8[0].field_02;
  count = 4;
  do {
    *index = (int16_t)NONE;
    index = (int16_t *)((char *)index + 4);
    count--;
  } while (count != 0);

  if (actor->field_3d8 != '\0' &&
      ((char)param2 == '\0' || actor->field_3d9 != '\0')) {
    actor->field_3d8 = 0;
  }
}

/* FUN_00024be0 (0x24be0) — push one firing position onto the actor's
 * discarded-position ring buffer and latch it as the actor's current
 * "avoid this position" record, caching the position's first 12 bytes.
 *
 * Confirmed from the listing at 0x24be0 (0x24be0-0x24c9f, 4 CALLs, no FPU):
 *   PUSH EBP / MOV EBP,ESP / PUSH EDI; DI = [EBP+0xc]; CMP DI,-1; JZ 0x24c9d.
 *   EBX and ESI are pushed *inside* the taken branch, so the whole body is a
 *   single `if (param_2 != NONE) { ... }` guard. An inverted early `return`
 *   hoists those saves into the prologue and changes the frame shape.
 *
 *   MOV ECX,[0x6325a4]; PUSH EAX(handle); PUSH ECX -> the cdecl argument
 *   order is datum_get(actor_data, actor_handle); ESI holds the actor datum.
 *   BL = [EBP+0x10] is loaded *after* that call and reused at 0x24c75 —
 *   MSVC parked param_3 in a callee-saved register, it is not a second read.
 *
 *   The ring cursor at +0x3c6 is re-loaded by a separate MOVSX at 0x24c09,
 *   0x24c17 and 0x24c26 — three reads of the field, not one cached copy.
 *   Hoisting it into a local collapses them and adds a frame slot.
 *
 *   The wrap is DEC / OR 0xfffffffc / INC guarded by the sign of
 *   (cursor + 1) & 0x80000003 — MSVC's expansion of a *signed* `% 4`. It is
 *   deliberately not written as `& 3`: the two differ for a negative cursor
 *   and the original keeps the signed form.
 *
 *   The encounter index is a 32-bit load of [ESI+0x34] masked to 16 bits,
 *   not a 16-bit load. PUSH 0xb0 and PUSH EDX(index) both precede
 *   CALL global_scenario_get, so the block pointer is the last argument
 *   evaluated — plain right-to-left C argument evaluation reproduces that.
 *
 *   The single ADD ESP,0x18 at 0x24c84 pays for BOTH tag_block_get_element
 *   calls (2 x 3 dwords). It is not evidence of a 6-argument callee, and the
 *   ARG_COUNT hazard raised against 0x24c70 is that mis-grouping.
 *
 *   Tail: ADD ESI,0x3dc then three dword MOVs from the firing-position
 *   element's first 12 bytes (its position) into +0x3dc/+0x3e0/+0x3e4.
 */
void FUN_00024be0(int actor_handle, short param_2, char param_3)
{
  actor_t *actor;
  encounter_definition *encounter;
  int32_t *firing_position;

  if (param_2 != (short)NONE) {
    actor = (actor_t *)datum_get(actor_data, actor_handle);

    actor->field_3c8[actor->field_3c6].field_00 = param_3;
    actor->field_3c8[actor->field_3c6].field_02 = param_2;
    actor->field_3c6 = (short)((actor->field_3c6 + 1) % 4);

    encounter = (encounter_definition *)tag_block_get_element(
      (char *)global_scenario_get() + 0x42c, actor->field_034 & 0xffff, 0xb0);
    firing_position = (int32_t *)tag_block_get_element(
      &encounter->firing_positions, param_2, 0x18);

    actor->field_3d9 = param_3;
    actor->field_3d8 = 1;
    actor->field_3dc = firing_position[0];
    actor->field_3e0 = firing_position[1];
    actor->field_3e4 = firing_position[2];
  }
}
