/* 0xc0c30 — HS script function handler: apply an encounter state change.
 * Evaluates the macro arguments; on success the result block holds an
 * encounter handle at +0x0 (int) and a state value at +0x4 (int16). Calls
 * FUN_00057aa0(encounter_handle, state) then returns void to the HS thread
 * via hs_return(thread_datum, 0). The +0x4 read is a narrow int16 load. */
void FUN_000c0c30(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    FUN_00057aa0(result[0], *(short *)(result + 1));
    hs_return(thread_datum, 0);
  }
}

/* 0xc0c70 — HS script function handler: apply an encounter state change.
 * Evaluates the macro arguments; on success the result block holds an
 * encounter handle at +0x0 (int) and a byte value at +0x4. Calls
 * FUN_00057c70(encounter_handle, value) then returns void to the HS thread
 * via hs_return(thread_datum, 0). The +0x4 read is a narrow byte (char) load
 * — result is int*, so (result + 1) = +4 bytes, cast to char*. */
void FUN_000c0c70(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    FUN_00057c70(result[0], *(char *)(result + 1));
    hs_return(thread_datum, 0);
  }
}

/* 0xc0cb0 — game_time HaloScript function evaluator. Runs the game-time helper
 * at 0x57c60 for its side effect, then commits a 0 result to the calling
 * script thread (a void-returning script builtin).
 *
 * Callees (both cdecl, ported):
 *   0x57c60 = FUN_00057c60(void) — game-time side effect
 *   0xcbf80 = hs_return(thread_handle, value)
 *
 * ABI (verified against disassembly 0xc0cb0-0xc0cc7): cdecl, plain RET. The
 * body reads only [EBP+0xc] = thread_datum (arg 2); function_index and init
 * complete the standard hs-evaluator signature (matches 0xc0c30) but are
 * unused in this body. */
void FUN_000c0cb0(int16_t function_index, int thread_datum, char init)
{
  FUN_00057c60();
  hs_return(thread_datum, 0);
}

/* 0xc0cd0 — HS script function handler: apply a state change (int value).
 * Twin of 0xc0c30, but the result block's +0x4 field is read as a full
 * int32 here (not the narrow int16 the 0xc0c30 twin uses). Evaluates the
 * macro arguments; on success the result block holds a handle at +0x0 and a
 * value at +0x4. Calls FUN_00057d00(handle, value) then returns void to the
 * HS thread via hs_return(thread_datum, 0). */
void FUN_000c0cd0(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    FUN_00057d00(result[0], result[1]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc0d10 — HS script function handler: set a vehicle entry's enterable
 * distance. Twin of 0xc0c30/0xc0cd0, but the result block's +0x4 field is a
 * float (the distance), passed by its raw IEEE-754 bits — NOT an int->float
 * numeric conversion. Verified against disassembly: the original does
 * `fld dword [eax+4]; mov edx,[eax]; push ecx; fstp dword [esp]; push edx`
 * — a true float lvalue read (FLD) at +0x4 and an int handle read (MOV) at
 * +0x0, i.e. the result block is a {int handle; float distance} pair.
 * Structural 92% ceiling: our VC71 /O2 build copies the untouched float arg
 * via integer MOV/PUSH instead of the original's FLD/FSTP — bit-exact either
 * way (tried int* pun, struct field, volatile local, double round-trip; all
 * end at MOV or score lower).
 * Evaluates the macro arguments; on success calls
 * FUN_00057f90(handle, distance) then returns void to the HS thread via
 * hs_return(thread_datum, 0). */
struct hs_handle_distance_result {
  int handle; /* +0x0 */
  float distance; /* +0x4 */
};

void FUN_000c0d10(int16_t function_index, int thread_datum, char init)
{
  struct hs_handle_distance_result *result;

  result = (struct hs_handle_distance_result *)hs_macro_function_evaluate(
    function_index, thread_datum, init);
  if (result != NULL) {
    FUN_00057f90(result->handle, result->distance);
    hs_return(thread_datum, 0);
  }
}

/* 0xc0d50 — HS script function handler: apply a change via FUN_00057fd0.
 * Twin of 0xc0c30 (identical codegen; differs only in the dispatch callee).
 * Evaluates the macro arguments; on success the result block holds a handle
 * at +0x0 (int) and a state value at +0x4 (narrow int16 — verified against
 * disassembly 0xc0d50-0xc0d88: XOR EDX,EDX; MOV DX,[EAX+0x4], a 16-bit load,
 * matching FUN_00057fd0's `short` second parameter). Calls
 * FUN_00057fd0(handle, state) then returns void to the HS thread via
 * hs_return(thread_datum, 0). */
void FUN_000c0d50(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    FUN_00057fd0(result[0], *(short *)(result + 1));
    hs_return(thread_datum, 0);
  }
}

/* 0xc0d90 — HS script function handler: apply a change via FUN_00058020.
 * Twin of 0xc0cd0/0xc0d50 (identical codegen; differs only in the dispatch
 * callee). Evaluates the macro arguments; on success the result block holds
 * a handle at +0x0 (int) and a state value at +0x4 (narrow int16 — matches
 * FUN_00058020's `short` second parameter, and the decompile reads the field
 * as a 16-bit load). Calls FUN_00058020(handle, state) then returns void to
 * the HS thread via hs_return(thread_datum, 0). */
void FUN_000c0d90(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    FUN_00058020(result[0], *(short *)(result + 1));
    hs_return(thread_datum, 0);
  }
}

/* 0xc0dd0 — HS script function handler: apply a change via FUN_00058070.
 * Twin of 0xc0cd0 (identical codegen; differs only in the dispatch callee).
 * Evaluates the macro arguments; on success the result block holds a handle
 * at +0x0 (int) and a value at +0x4, both read as full int32 (puVar1[1] on
 * an undefined4* — a 4-byte load — matching FUN_00058070's `int` second
 * parameter). Calls FUN_00058070(handle, value) then returns void to the
 * HS thread via hs_return(thread_datum, 0). */
void FUN_000c0dd0(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    FUN_00058070(result[0], result[1]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc0e10 — HS script function handler: apply a change via FUN_00058110.
 * Same evaluate-then-dispatch shape as the 0xc0d50/0xc0dd0 twins, but the
 * result block is consumed with a single dword load: `MOV EDX,[EAX]; PUSH EDX`
 * in the original passes only *result (result[0], the first int) to
 * FUN_00058110 — no second field is read. Evaluates the macro arguments; on
 * success calls FUN_00058110(*result) then returns void to the HS thread via
 * hs_return(thread_datum, 0). */
void FUN_000c0e10(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    FUN_00058110(*result);
    hs_return(thread_datum, 0);
  }
}

/* 0xc0e50 — HS script function handler: apply a change via FUN_000581b0.
 * Same family as 0xc0cd0/0xc0dd0 (identical codegen; differs only in the
 * dispatch callee). Evaluates the macro arguments; on success the result
 * block holds a handle at +0x0 (int) and a value at +0x4, both read as full
 * int32 (puVar1[1] on an undefined4* — a 4-byte load — matching
 * FUN_000581b0's `int` second parameter). Calls FUN_000581b0(handle, value)
 * then returns void to the HS thread via hs_return(thread_datum, 0). */
void FUN_000c0e50(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    FUN_000581b0(result[0], result[1]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc0e90 — HS script function handler: evaluate a macro function and dispose
 * its result. Twin of 0xc0c30's family (identical evaluator/return skeleton),
 * but instead of dispatching a handle+value pair it derefs the first dword of
 * the result block and passes that value to FUN_00058220 (a dispose/release
 * helper). On success calls FUN_00058220(result[0]) then returns void to the
 * HS thread via hs_return(thread_datum, 0). */
void FUN_000c0e90(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    FUN_00058220(result[0]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc0ed0 — Evaluate an HS macro-function call and post its result to the
 * calling thread. Dispatches to hs_macro_function_evaluate; when that returns
 * a non-NULL result record, forwards the record's value (dword @ +0x0) and its
 * type byte (@ +0x4) to the result-commit helper, then signals the thread to
 * return with value 0.
 *
 * Callees (hardcoded addresses, all ported):
 *   0xcc560 = hs_macro_function_evaluate(int16 function_index,
 *                                        int thread_datum, char init)
 *               -> result record* or NULL
 *   0x58270 = FUN_00058270(int value, char type) -> void
 *   0xcbf80 = hs_return(int thread_datum, int value) -> void
 *
 * Pointer arith: result is int* (dword-strided), so (result + 1) addresses the
 * byte at +0x4. The second arg to FUN_00058270 is a single BYTE (char-width),
 * NOT a dword. thread_datum is forwarded unchanged to both callees.
 */
void FUN_000c0ed0(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != (int *)0) {
    FUN_00058270(*result, *(char *)(result + 1));
    hs_return(thread_datum, 0);
  }
  return;
}

/* Evaluate a script macro function for side effects, discarding its value,
 * then commit a zero return to the calling thread.
 *
 * Calls hs_macro_function_evaluate(function_index, thread_datum, init), which
 * returns a pointer to the evaluated value block (or NULL). When non-NULL, the
 * first dword of that block is passed to FUN_00058310 (0x58310), then the
 * thread result is committed as 0 via hs_return(thread_datum, 0).
 *
 * The evaluator's return is declared int in kb.json (0xcc560) but is used here
 * as a pointer, so it is cast.
 *
 * Callees:
 *   0xcc560 = hs_macro_function_evaluate(short, int, char) -> void* (used as
 * ptr) 0x58310 = FUN_00058310(uint) -> void 0xcbf80 = hs_return(int
 * thread_handle, int value) -> void [ported]
 */
void FUN_000c0f10(int16_t function_index, int thread_datum, char init)
{
  unsigned int *result;

  result = (unsigned int *)hs_macro_function_evaluate(function_index,
                                                      thread_datum, init);
  if (result != (unsigned int *)0) {
    FUN_00058310(*result);
    hs_return(thread_datum, 0);
  }
  return;
}

/* Evaluate a script macro function for side effects, discarding its value,
 * then commit a zero return to the calling thread.
 *
 * Calls hs_macro_function_evaluate(function_index, thread_datum, init), which
 * returns a pointer to the evaluated value block (or NULL). When non-NULL, the
 * first dword of that block is passed to FUN_00058390 (0x58390), then the
 * thread result is committed as 0 via hs_return(thread_datum, 0).
 *
 * Identical in shape to FUN_000c0f10 (0xc0f10); the only difference is the
 * per-value-block callee (0x58390 here vs 0x58310 there).
 *
 * The evaluator's return is declared int in kb.json (0xcc560) but is used here
 * as a pointer, so it is cast.
 *
 * Callees:
 *   0xcc560 = hs_macro_function_evaluate(short, int, char) -> void* (used as
 * ptr) 0x58390 = FUN_00058390(uint) -> void 0xcbf80 = hs_return(int
 * thread_handle, int value) -> void [ported]
 */
void FUN_000c0f50(int16_t function_index, int thread_datum, char init)
{
  unsigned int *result;

  result = (unsigned int *)hs_macro_function_evaluate(function_index,
                                                      thread_datum, init);
  if (result != (unsigned int *)0) {
    FUN_00058390(*result);
    hs_return(thread_datum, 0);
  }
  return;
}

/* 0xc0f90 — HS native-function-call evaluator. Drives
 * hs_macro_function_evaluate to evaluate the call's argument expressions; when
 * the values array is ready (non-null return), invokes the native builtin
 * FUN_00058410 with the first two evaluated argument dwords, then commits a
 * zero result to the thread via hs_return. While arguments are still being
 * evaluated the return is null and nothing is dispatched this tick. */
void FUN_000c0f90(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != (int *)0x0) {
    FUN_00058410((unsigned int)result[0], result[1]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc0fd0 — HS script function handler. Twin of 0xc0f90: evaluates the call's
 * argument expressions via hs_macro_function_evaluate; when the values array is
 * ready (non-null return), invokes the side-effect helper FUN_000584a0 with the
 * first two evaluated argument dwords (result[0] as a handle/unsigned,
 * result[1] as an int value), then commits a zero result to the thread via
 * hs_return. While arguments are still being evaluated the return is null and
 * nothing is dispatched this tick.
 *
 * Callees (all cdecl, ported): 0xcc560 hs_macro_function_evaluate,
 * 0x584a0 FUN_000584a0(unsigned int, int), 0xcbf80 hs_return(thread, value).
 * Both result fields are full dwords here (decompile shows *puVar1 and
 * puVar1[1] as undefined4) — not a narrow int16/char variant. */
void FUN_000c0fd0(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != (int *)0x0) {
    FUN_000584a0((unsigned int)result[0], result[1]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc1010 — HS native-function-call evaluator (float-argument variant). Drives
 * hs_macro_function_evaluate to evaluate the call's argument expressions; when
 * the values array is ready (non-null return), invokes the native builtin
 * FUN_00058550 with the first evaluated dword as an object/handle and the
 * second evaluated dword reinterpreted as a float (MSVC passes it via
 * FLD+FSTP[ESP], so the raw bits must be read as float, not int-converted),
 * then commits a zero result to the thread via hs_return. While arguments are
 * still being evaluated the return is null and nothing is dispatched this tick.
 */
void FUN_000c1010(int16_t function_index, int thread_datum, char init)
{
  unsigned int *result;

  result = (unsigned int *)hs_macro_function_evaluate(function_index,
                                                      thread_datum, init);
  if (result != (unsigned int *)0x0) {
    FUN_00058550(result[0], ((float *)result)[1]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc1050 — HS native-function-call evaluator (16-bit result variant). Drives
 * hs_macro_function_evaluate to evaluate the call's argument expressions; when
 * the values array is ready (non-null return), reads the first evaluated value
 * as a zero-extended 16-bit quantity (original: xor edx,edx; mov dx,[result],
 * so the low 16 bits are the payload and the value widens unsigned to int) and
 * passes it to the native builtin FUN_00058640, then commits a zero result to
 * the thread via hs_return. While arguments are still being evaluated the
 * return is null and nothing is dispatched this tick. */
void FUN_000c1050(int16_t function_index, int thread_datum, char init)
{
  unsigned short *result;

  result = (unsigned short *)hs_macro_function_evaluate(function_index,
                                                        thread_datum, init);
  if (result != (unsigned short *)0x0) {
    FUN_00058640(*result);
    hs_return(thread_datum, 0);
  }
}

/* 0xc1090 — HS native-function-call evaluator (16-bit result variant). Twin of
 * 0xc1050 (identical codegen; differs only in the dispatch callee). Drives
 * hs_macro_function_evaluate to evaluate the call's argument expressions; when
 * the values array is ready (non-null return), reads the first evaluated value
 * as a zero-extended 16-bit quantity and passes it to the native builtin
 * FUN_000586a0, then commits a zero result to the thread via hs_return. While
 * arguments are still being evaluated the return is null and nothing is
 * dispatched this tick. */
void FUN_000c1090(int16_t function_index, int thread_datum, char init)
{
  unsigned short *result;

  result = (unsigned short *)hs_macro_function_evaluate(function_index,
                                                        thread_datum, init);
  if (result != (unsigned short *)0x0) {
    FUN_000586a0(*result);
    hs_return(thread_datum, 0);
  }
}

/* 0xc10d0 — Evaluate an HS macro (built-in) function on a thread, then
 * consume its two-dword result. hs_macro_function_evaluate returns (in EAX)
 * a pointer to a 2-dword result record when the call produced a value;
 * dword[0] and dword[1] are forwarded to FUN_00058720, after which
 * hs_return(thread_datum, 0) commits/cleans up the thread. Returns nothing.
 *
 * Callees:
 *   0xcc560 = hs_macro_function_evaluate(int16 function_index, int
 * thread_datum, char init) -> int (result-record ptr in EAX) 0x58720 =
 * FUN_00058720(unsigned int, int) 0xcbf80 = hs_return(int thread_handle, int
 * value)
 */
void FUN_000c10d0(int16_t function_index, int thread_datum, char init)
{
  int *result_ptr;

  result_ptr =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result_ptr != (int *)0x0) {
    FUN_00058720((unsigned int)result_ptr[0], result_ptr[1]);
    hs_return(thread_datum, 0);
  }
  return;
}

/* 0xc1110 — Evaluate an HS built-in function call, then dispatch the result
 * to the ai_berserk script command (FUN_000587d0) and commit a 0 result to
 * the calling thread. hs_macro_function_evaluate returns a pointer to the
 * evaluated-argument record (int cast); when non-NULL, its dword@+0x0 and
 * byte@+0x4 are passed to FUN_000587d0, then hs_return(thread_datum, 0)
 * acknowledges the command.
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560 = hs_macro_function_evaluate(int16 function_index, int
 * thread_datum, char init) 0x587d0 = FUN_000587d0(int, int)  (ai_berserk script
 * command) 0xcbf80 = hs_return(int thread_handle, int value)
 */
void FUN_000c1110(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != (int *)0x0) {
    FUN_000587d0(*result, *(unsigned char *)((char *)result + 4));
    hs_return(thread_datum, 0);
  }
}

/* 0xc1150 — HS script function handler: evaluate a macro function and dispatch
 * the result to FUN_00058860. Twin of the 0xc0c30 int16 family (identical
 * evaluate-then-dispatch skeleton). On success the result block holds an
 * encounter handle at +0x0 (int) and a team value at +0x4 (int16). The +0x4
 * read is a narrow 16-bit ZERO-extended load: the original does
 * `xor edx,edx; mov dx,WORD PTR [eax+0x4]` (disassembly 0xc1150), i.e. the
 * +0x4 field is treated as an UNSIGNED 16-bit team value, so the faithful
 * lift is *(unsigned short *)(result + 1). Then FUN_00058860(handle, team)
 * and hs_return(thread_datum, 0) acknowledges the command.
 * VC71 emits the compact movzwl for this read where the original used the
 * two-instruction xor+movw idiom, leaving a permanent 1-insn (~94%) gap
 * shared by the whole 0xc1050/0xc1110/0xc1150 high-address cluster.
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560 = hs_macro_function_evaluate(int16 function_index, int
 * thread_datum, char init) 0x58860 = FUN_00058860(int encounter_handle, int
 * team) 0xcbf80 = hs_return(int thread_handle, int value)
 */
void FUN_000c1150(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != (int *)0x0) {
    FUN_00058860(result[0], *(unsigned short *)(result + 1));
    hs_return(thread_datum, 0);
  }
}

/* 0xc1190 — HS script function handler: evaluate a macro function and dispatch
 * the result to FUN_00057030. Twin of the 0xc1150 skeleton (identical
 * evaluate-then-dispatch shape), differing only in the +0x4 field width and
 * the dispatch target. On success the result block holds an int at +0x0 and
 * an 8-bit flag at +0x4. The +0x4 read is a narrow BYTE ZERO-extended load:
 * the original does `xor edx,edx; mov dl,BYTE PTR [eax+0x4]` (disassembly
 * 0xc1190), so the +0x4 field is an UNSIGNED byte and the faithful lift is
 * *(unsigned char *)(result + 1) (result is int*, so +1 == byte offset +4,
 * NOT +1). Then FUN_00057030(value, flag) and hs_return(thread_datum, 0)
 * acknowledges the command.
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560 = hs_macro_function_evaluate(int16 function_index, int
 * thread_datum, char init) 0x57030 = FUN_00057030(int param_1, char param_2)
 * 0xcbf80 = hs_return(int thread_handle, int value)
 */
void FUN_000c1190(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != (int *)0x0) {
    FUN_00057030(result[0], *(unsigned char *)(result + 1));
    hs_return(thread_datum, 0);
  }
}

/* 0xc11d0 — HS script function handler, third twin of the 0xc1150 / 0xc1190
 * evaluate-then-dispatch skeleton (identical shape, differing only in dispatch
 * target). Evaluates a macro function for the thread; on a non-NULL result
 * block it reads an int at +0x0 and an 8-bit flag at +0x4 (narrow byte load;
 * modeled zero-extended like the 0xc1190 twin, matching FUN_000588d0's char
 * param), forwards both to FUN_000588d0, then acknowledges the command via
 * hs_return(thread_datum, 0). result is int*, so `result + 1` == byte offset
 * +4 (NOT +1).
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560 = hs_macro_function_evaluate(int16 function_index, int
 * thread_datum, char init) 0x588d0 = FUN_000588d0(int param_1, char param_2)
 *   0xcbf80 = hs_return(int thread_handle, int value)
 */
void FUN_000c11d0(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != (int *)0x0) {
    FUN_000588d0(result[0], *(unsigned char *)(result + 1));
    hs_return(thread_datum, 0);
  }
}

/* 0xc1210 — HS built-in evaluator: dispatch a macro function call and commit
 * an AI-reference predicate result to the thread. Evaluates the macro function
 * via hs_macro_function_evaluate; if it produces a non-null result record,
 * reads the record's first dword as an AI object reference, tests it with
 * FUN_000556f0 (ai_ref-valid predicate, returns bool in AL), and returns the
 * boolean (zero-extended to int) to the thread via hs_return.
 *
 * thread_datum is forwarded unchanged as both the thread argument to
 * hs_macro_function_evaluate and the thread handle to hs_return (the original
 * reuses the same value for both — consistent).
 *
 * Callees:
 *   0xcc560 = hs_macro_function_evaluate(int16_t, int, char) -> result ptr
 *   0x556f0 = FUN_000556f0(unsigned int ai_ref) -> bool
 *   0xcbf80 = hs_return(int thread_handle, int value)
 */
void FUN_000c1210(int16_t function_index, int thread_datum, char init)
{
  unsigned int *result;
  /* volatile forces the AL->stack spill+reload MSVC emits for the bool
   * result (VC71 shape lever; value 0/1 is preserved unchanged). */
  volatile unsigned char valid;

  result = (unsigned int *)hs_macro_function_evaluate(function_index,
                                                      thread_datum, init);
  if (result != NULL) {
    valid = FUN_000556f0(*result);
    hs_return(thread_datum, (int)(uint8_t)valid);
  }
  do {
  } while (0);
}

/* 0xc1260 — HS built-in evaluator: dispatch a macro function call and commit a
 * 16-bit query result to the thread. Evaluates the macro function via
 * hs_macro_function_evaluate; if it produces a non-null result record, reads
 * the record's first dword and passes it to FUN_00057380 (a query returning a
 * 16-bit value in AX), then commits that value — zero-extended to int — to the
 * thread via hs_return.
 *
 * The original keeps a dword stack temp pre-initialized to 0 (mov dword
 * [ebp-4],0), stores only the low 16 bits of the AX result into it (mov word
 * [ebp-4],ax), then reads the full dword back (mov eax,[ebp-4]) so the upper
 * 16 bits stay 0 (unsigned widen). Modeled here with an int/short union to
 * preserve that zero-init-then-narrow-store shape (match-sensitive).
 *
 * Callees:
 *   0xcc560 = hs_macro_function_evaluate(int16_t, int, char) -> result ptr
 *   0x57380 = FUN_00057380(int value) -> 16-bit result in AX
 *   0xcbf80 = hs_return(int thread_handle, int value)
 */
void FUN_000c1260(int16_t function_index, int thread_datum, char init)
{
  int *result;
  union {
    int i;
    short s;
  } value;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    value.i = 0;
    value.s = FUN_00057380(*result);
    hs_return(thread_datum, value.i);
  }
}

/* 0xc12b0 — HS script function handler: evaluate the macro arguments; on
 * success the result block holds a handle at +0x0 (int). Passes result[0] to
 * FUN_00056880 (returns short), then returns that short to the HS thread via
 * hs_return(thread_datum, value). Structurally identical to FUN_000c1260
 * (0xc1260); the only difference is the callee (FUN_00056880 vs FUN_00057380).
 * The union preserves the original's int-slot-zeroed-then-16-bit-store shape:
 * value.i = 0 clears the full 4-byte slot, value.s writes only the low word,
 * so the value passed to hs_return is the short in the low 16 bits with a
 * zeroed upper half (NOT a sign-extended short). */
void FUN_000c12b0(int16_t function_index, int thread_datum, char init)
{
  int *result;
  union {
    int i;
    short s;
  } value;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    value.i = 0;
    value.s = FUN_00056880(*result);
    hs_return(thread_datum, value.i);
  }
}

/* 0xc1300 — HS macro-function result commit. Evaluates a built-in HS macro
 * function via hs_macro_function_evaluate; if it yields a non-NULL result
 * record, reads the first dword (an AI reference) from that record, resolves
 * it through FUN_00055660 (count_type 0 "start"/min accessor), and commits
 * the resulting 16-bit value to the thread via hs_return.
 *
 * The thread_datum argument is reused for both the evaluate call (arg 2) and
 * the hs_return call (arg 1) — a single value flows to both.
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560 = hs_macro_function_evaluate(int16 fn_index, int thread_datum,
 *             char init) -> int* (result record, NULL on failure)
 *   0x55660 = FUN_00055660(unsigned int ai_ref) -> int (16-bit count/index)
 *   0xcbf80 = hs_return(int thread_handle, int value)
 *
 * The committed value is a 16-bit quantity zero-extended into a dword: the
 * disassembly zero-inits the full dword slot ([EBP-4] = 0), stores only the
 * low word (MOV [EBP-4],AX) from FUN_00055660's return, then reloads the full
 * dword — so the high 16 bits stay 0. Modeled here with a int/uint16 union.
 */
void FUN_000c1300(int16_t function_index, int thread_datum, char init)
{
  int *result_ptr;
  union {
    int dw;
    uint16_t w;
  } value;

  value.dw = 0;
  result_ptr =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result_ptr != (int *)0) {
    value.w = (uint16_t)FUN_00055660((unsigned int)*result_ptr);
    hs_return(thread_datum, value.dw);
  }
}

/*
 * FUN_000c1350 @ 0xc1350 (hs.obj)
 *
 * HaloScript macro-function trampoline: evaluate a macro function and, if it
 * produced a result record, read the record's first dword as an ai_ref,
 * convert it to a float via FUN_00055680, and commit that float to the calling
 * HS thread via hs_return.
 *
 * The thread_datum argument is reused for both the evaluate call (arg 2) and
 * the hs_return call (arg 1) — a single value flows to both.
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560 = hs_macro_function_evaluate(int16 fn_index, int thread_datum,
 *             char init) -> int* (result record, NULL on failure)
 *   0x55680 = FUN_00055680(unsigned int ai_ref) -> float (returned in ST0)
 *   0xcbf80 = hs_return(int thread_handle, int value)
 *
 * The returned float is committed as its raw 32-bit bit pattern, NOT a numeric
 * int conversion: the disassembly does FSTP [EBP-4] (store float) then
 * MOV EAX,[EBP-4] (reload the same dword) before PUSH EAX into hs_return. This
 * is a type-pun, modeled here with a float/int union — a numeric (int)f cast
 * would truncate the value and commit the wrong bits.
 */
void FUN_000c1350(int16_t function_index, int thread_datum, char init)
{
  int *result_ptr;
  union {
    float f;
    int dw;
  } value;

  result_ptr =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result_ptr != (int *)0) {
    value.f = FUN_00055680((unsigned int)*result_ptr);
    hs_return(thread_datum, value.dw);
  }
}

/*
 * FUN_000c1390 @ 0xc1390 (hs.obj)
 *
 * HaloScript macro-function trampoline. Twin of FUN_000c1350 (0xc1350):
 * evaluate a macro function and, if it produced a result record, read the
 * record's first dword as an ai_ref, convert it to a float via FUN_000556c0,
 * and commit that float to the calling HS thread via hs_return. The only
 * difference from the 0xc1350 twin is the float accessor callee
 * (FUN_000556c0 vs FUN_00055680).
 *
 * The thread_datum argument is reused for both the evaluate call (arg 2) and
 * the hs_return call (arg 1) — a single value flows to both.
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560 = hs_macro_function_evaluate(int16 fn_index, int thread_datum,
 *             char init) -> int* (result record, NULL on failure)
 *   0x556c0 = FUN_000556c0(unsigned int ai_ref) -> float (returned in ST0)
 *   0xcbf80 = hs_return(int thread_handle, int value)
 *
 * The returned float is committed as its raw 32-bit bit pattern, NOT a numeric
 * int conversion: the disassembly does FSTP [EBP-4] (store float) then
 * MOV EAX,[EBP-4] (reload the same dword) before PUSH EAX into hs_return. This
 * is a type-pun, modeled here with a float/int union — a numeric (int)f cast
 * would truncate the value and commit the wrong bits.
 */
void FUN_000c1390(int16_t function_index, int thread_datum, char init)
{
  int *result_ptr;
  union {
    float f;
    int dw;
  } value;

  result_ptr =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result_ptr != (int *)0) {
    value.f = FUN_000556c0((unsigned int)*result_ptr);
    hs_return(thread_datum, value.dw);
  }
}

/* 0xc13d0 — HS built-in evaluator wrapper. Dispatches to the macro-function
 * evaluator; on a non-null result, reads the first dword of the returned
 * record as an AI reference, converts it via FUN_00055620 (result narrowed
 * to 16 bits), and returns that value on the thread. Same evaluator ABI
 * (function_index, thread_datum, init) as the other hs_evaluate_* handlers.
 *
 * Callees:
 *   0xcc560 = hs_macro_function_evaluate -> void* (record ptr, null on fail)
 *   0x55620 = FUN_00055620 (unsigned ai_ref) -> int (narrowed to int16)
 *   0xcbf80 = hs_return (thread_datum, value) */
void FUN_000c13d0(int16_t function_index, int thread_datum, char init)
{
  int *result;
  int value = 0;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != (int *)0) {
    /* Narrow the AI-reference conversion to 16 bits via the low word of a
     * zero-initialized dword slot (matches original: MOV dword,0 / MOV
     * word,AX / MOV dword,EAX — a zero-extended int16, not a MOVSX cast). */
    *(short *)&value = (short)FUN_00055620(*(unsigned int *)result);
    hs_return(thread_datum, value);
  }
}

/* 0xc1420 — HS macro-function call site: evaluate a built-in HS function and,
 * if it produced a result, commit that value back to the calling thread.
 *
 * Forwards (function_index, thread_datum, init) to hs_macro_function_evaluate.
 * On a non-null result pointer, reads the first dword of the result, passes it
 * through FUN_00055640 (ai count_type-2 accessor), and delivers the result via
 * hs_return(thread_datum, value). thread_datum is reused as both the evaluate
 * arg and the hs_return thread handle.
 *
 * The result slot is a 4-byte stack local zero-initialized up front (the
 * disasm `mov DWORD PTR [ebp-4],0` at entry); only its low 16 bits are then
 * overwritten from FUN_00055640's AX (`mov WORD PTR [ebp-4],ax`), and the full
 * dword is read back (`mov eax,[ebp-4]`) — the high word stays 0. A union
 * reproduces this partial-store / wide-read exactly.
 *
 * Callees:
 *   0xcc560 = hs_macro_function_evaluate -> result pointer (in EAX)
 *   0x55640 = FUN_00055640(ai_ref) -> int (low 16 bits consumed)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c1420(int16_t function_index, int thread_datum, char init)
{
  int result;
  union {
    int i;
    short s;
  } value;

  value.i = 0;
  result = hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != 0) {
    value.s = (short)FUN_00055640(*(unsigned int *)result);
    hs_return(thread_datum, value.i);
  }
}

/* 0xc1470 — HS macro-function call site (encounter-handle variant): evaluate a
 * built-in HS function and, if it produced a result, commit that value back to
 * the calling thread.
 *
 * Twin of FUN_000c1420 (0xc1420): forwards (function_index, thread_datum, init)
 * to hs_macro_function_evaluate; on a non-null result pointer, reads the first
 * dword of the result and passes it through FUN_000547c0 (encounter-handle
 * accessor), delivering the full-dword result via hs_return(thread_datum,
 * value). The one difference from the 0xc1420 twin: the accessor result is the
 * full dword (no 16-bit partial store / wide read here) and the callee is
 * 0x547c0 rather than 0x55640. thread_datum is reused as both the evaluate arg
 * and the hs_return thread handle.
 *
 * Callees:
 *   0xcc560 = hs_macro_function_evaluate -> result pointer (in EAX)
 *   0x547c0 = FUN_000547c0(encounter_handle) -> int
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c1470(int16_t function_index, int thread_datum, char init)
{
  int result;
  int value;

  result = hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != 0) {
    value = FUN_000547c0(*(unsigned int *)result);
    hs_return(thread_datum, value);
  }
}

/* 0xc14b0 — HS macro-function call site (ai_status short variant): evaluate a
 * built-in HS function and, if it produced a result, commit that value back to
 * the calling thread.
 *
 * Twin of FUN_000c1420 (0xc1420): forwards (function_index, thread_datum, init)
 * to hs_macro_function_evaluate; on a non-null result pointer, reads the first
 * dword of the result and passes it through FUN_00057bc0 (ai_status accessor,
 * returns a short in AX), delivering the value via hs_return(thread_datum,
 * value). Like the 0xc1420 twin (and unlike 0xc1470), the accessor result is a
 * 16-bit value: the result slot is a 4-byte stack local zero-initialized up
 * front (`mov DWORD PTR [ebp-4],0`), only its low 16 bits are overwritten from
 * AX (`mov WORD PTR [ebp-4],ax`), then the full dword is read back
 * (`mov eax,[ebp-4]`) — the high word stays 0. A union reproduces this
 * partial-store / wide-read exactly. The one difference from the 0xc1420 twin
 * is the accessor callee: 0x57bc0 rather than 0x55640. thread_datum is reused
 * as both the evaluate arg and the hs_return thread handle.
 *
 * Callees:
 *   0xcc560 = hs_macro_function_evaluate -> result pointer (in EAX)
 *   0x57bc0 = FUN_00057bc0(encounter_handle) -> short (low 16 bits consumed)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c14b0(int16_t function_index, int thread_datum, char init)
{
  int result;
  union {
    int i;
    short s;
  } value;

  value.i = 0;
  result = hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != 0) {
    value.s = FUN_00057bc0(*(unsigned int *)result);
    hs_return(thread_datum, value.i);
  }
}

/* 0xc1500 — HS macro-function call site (byte-accessor variant): evaluate a
 * built-in HS function and, if it produced a result, commit that value back to
 * the calling thread.
 *
 * Twin of FUN_000c1420 (0xc1420): forwards (function_index, thread_datum, init)
 * to hs_macro_function_evaluate; on a non-null result pointer, reads the FIRST
 * 16-BIT field of the result (disasm `xor edx,edx; mov dx,WORD PTR [eax]` — a
 * zero-extended uint16 load, NOT the full dword the other twins read) and
 * passes it through FUN_000585d0, delivering the value via
 * hs_return(thread_datum, value).
 *
 * FUN_000585d0's kb decl is understated as `void`, but it returns a value in
 * EAX (its body tail-returns FUN_00046b60's int) and this call site consumes
 * the low byte: `mov BYTE PTR [ebp-4],al`. The result slot is a 4-byte stack
 * local zero-initialized up front (`mov DWORD PTR [ebp-4],0`); only its low 8
 * bits are overwritten from AL, then the full dword is read back
 * (`mov eax,[ebp-4]`) — the high 3 bytes stay 0. A union reproduces this
 * partial-store / wide-read exactly. thread_datum is reused as both the
 * evaluate arg and the hs_return thread handle.
 *
 * Callees:
 *   0xcc560 = hs_macro_function_evaluate -> result pointer (in EAX)
 *   0x585d0 = FUN_000585d0(uint16 field) -> int (low byte consumed in AL)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c1500(int16_t function_index, int thread_datum, char init)
{
  int result;
  union {
    int i;
    unsigned char b;
  } value;

  value.i = 0;
  result = hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != 0) {
    value.b = (unsigned char)FUN_000585d0(*(unsigned short *)result);
    hs_return(thread_datum, value.i);
  }
}

/* 0xc1550 — HS macro-function call site (16-bit-accessor variant): evaluate a
 * built-in HS function and, if it produced a result, commit that value back to
 * the calling thread.
 *
 * Twin of FUN_000c1500 (0xc1500): forwards (function_index, thread_datum, init)
 * to hs_macro_function_evaluate; on a non-null result pointer, reads the FIRST
 * 16-BIT field of the result (disasm 0xc1574 `xor edx,edx; mov dx,WORD PTR
 * [eax]` — a zero-extended uint16 load of offset 0) and passes it through
 * FUN_00058700, delivering the value via hs_return(thread_datum, value).
 *
 * FUN_00058700's kb decl is understated as `void(void)`, but disasm shows one
 * zero-extended uint16 stack arg (`push edx`) and a value returned in AX that
 * this call site consumes: `mov WORD PTR [ebp-4],ax` (0xc157f) — a 16-bit
 * store, wider than the 0xc1500 twin's `mov [ebp-4],al` byte store. The result
 * slot is a 4-byte stack local zero-initialized up front (`mov DWORD PTR
 * [ebp-4],0` at 0xc1561); only its low 16 bits are overwritten from AX, then
 * the full dword is read back for the hs_return arg (high 2 bytes stay 0). A
 * union reproduces this partial-store / wide-read exactly. thread_datum is
 * reused as both the evaluate arg and the hs_return thread handle. cdecl
 * throughout; the trailing ADD ESP,0xc at 0xc158d batch-cleans the outstanding
 * pushes.
 *
 * Callees:
 *   0xcc560 = hs_macro_function_evaluate -> result pointer (in EAX)
 *   0x58700 = FUN_00058700(uint16 field) -> int (low word consumed in AX)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c1550(int16_t function_index, int thread_datum, char init)
{
  int result;
  union {
    int i;
    unsigned short w;
  } value;

  value.i = 0;
  result = hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != 0) {
    value.w = (unsigned short)FUN_00058700(*(unsigned short *)result);
    hs_return(thread_datum, value.i);
  }
}

/* 0xc15a0 — HS script command: evaluate a macro-function argument and commit
 * a converted 16-bit result. Dispatches to hs_macro_function_evaluate; if it
 * yields a non-NULL result record, the first 16-bit word of that record is
 * passed to ai_conversation_status (via frame thunk FUN_00058710) and the
 * 16-bit status it returns is committed to the thread with hs_return.
 *
 * Confirmed (disasm 0xc15a0):
 *   - forwards (function_index, thread_datum, init) to
 * hs_macro_function_evaluate (all three loaded as full dwords: MOV ECX/ESI/EAX
 * from [EBP+8/0xc/0x10]).
 *   - result == NULL -> nothing committed.
 *   - result word read zero-extended (XOR EDX,EDX; MOV DX,[EAX]) -> unsigned.
 *   - FUN_00058710 is a frame thunk to ai_conversation_status; its 16-bit AX
 *     return is written low-word (MOV word[EBP-4],AX) into a dword slot that
 * was pre-initialized to 0, then the full dword is passed to hs_return.
 * Inferred: param widths treated as int/undefined4 (caller uses dword loads).
 */
void FUN_000c15a0(int function_index, int thread_datum, int init)
{
  unsigned short *result;
  int value;

  value = 0;
  result = (unsigned short *)hs_macro_function_evaluate(function_index,
                                                        thread_datum, init);
  if (result != (unsigned short *)0x0) {
    *(short *)&value = FUN_00058710(*result);
    hs_return(thread_datum, value);
  }
}

/* 0xc15f0 — HS script command: evaluate a macro-function argument pair and
 * commit a boolean result. Dispatches to hs_macro_function_evaluate; if it
 * yields a non-NULL result record, the record's first two 16-bit fields are
 * passed to FUN_000567e0 (a two-team allied/friendly predicate returning a
 * bool in AL), and that boolean is committed to the thread with hs_return.
 *
 * Confirmed (disasm 0xc15f0):
 *   - forwards (function_index, thread_datum, init) to
 * hs_macro_function_evaluate (MOV ECX/ESI/EAX from [EBP+8/0xc/0x10]; cdecl, ADD
 * ESP,0xc).
 *   - result == NULL -> nothing committed.
 *   - result +0x0 read SIGN-extended (MOVSX EAX,WORD PTR [EAX]) -> signed
 * int16.
 *   - result +0x4 read ZERO-extended (XOR EDX,EDX; MOV DX,WORD PTR [EAX+4]) ->
 *     unsigned int16.
 *   - call FUN_000567e0(sign16, zero16); its bool AL is stored as a byte into a
 *     dword stack slot pre-initialized to 0 (MOV [EBP-4],0 then MOV
 * [EBP-4],AL), and the full dword is passed to hs_return. A union reproduces
 * the partial-byte-store / wide-read exactly. The +0x4 read leaves the same
 * permanent ~1-insn gap (compact movzwl vs the original xor+movw idiom) as the
 * 0xc1150 int16 cluster.
 *
 * Callees (all cdecl, in kb.json):
 *   0xcc560 = hs_macro_function_evaluate(int16 function_index, int
 * thread_datum, char init) -> result pointer 0x567e0 = FUN_000567e0(int16 a,
 * int16 b) -> bool (low byte consumed in AL) 0xcbf80 = hs_return(int
 * thread_handle, int value)
 */
void FUN_000c15f0(int16_t function_index, int thread_datum, char init)
{
  int *result;
  union {
    int i;
    unsigned char b;
  } value;

  value.i = 0;
  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != (int *)0x0) {
    value.b = (unsigned char)FUN_000567e0(*(short *)result,
                                          *(unsigned short *)(result + 1));
    hs_return(thread_datum, value.i);
  }
}

/* 0xc1640 — HS script function handler: dispatch to director_script_camera.
 * Twin of the 0xc0c30 dispatch family, but reads a SINGLE zero-extended byte
 * from OFFSET +0x0 of the macro result block (not +0x4 like the encounter
 * twins). Verified against disassembly 0xc1640-...: after the NULL check,
 * XOR EDX,EDX; MOV DL,byte[EAX] (a movzx byte load at +0x0); PUSH EDX;
 * CALL 0x86cb0. Then PUSH 0; PUSH ESI(thread_datum); CALL 0xcbf80; a single
 * ADD ESP,0xc cleans all three preceding pushes.
 *
 * Callees (all cdecl):
 *   0xcc560 = hs_macro_function_evaluate(int16 function_index, int
 * thread_datum, char init) -> result pointer
 *   0x86cb0 = director_script_camera(int) — receives the +0x0 byte,
 * zero-extended 0xcbf80 = hs_return(int thread_datum, int value)
 */
void FUN_000c1640(int16_t function_index, int thread_datum, char init)
{
  unsigned char *result;

  result = (unsigned char *)hs_macro_function_evaluate(function_index,
                                                       thread_datum, init);
  if (result != NULL) {
    director_script_camera(*result);
    hs_return(thread_datum, 0);
  }
}

/* 0xc1680 — HS macro-function evaluator that forwards two 16-bit fields.
 * Evaluates the macro function for this HS function_index. If it produced a
 * result record (returned as a short* in EAX), invokes FUN_00085260 with the
 * signed short at offset 0 and the unsigned short at offset 4 of that record,
 * then commits a 0 result to the thread. Does nothing if the macro returned
 * NULL.
 *
 * Callees:
 *   0xcc560 = hs_macro_function_evaluate (short, int, char) -> short* (EAX)
 *   0x85260 = FUN_00085260 (short arg0, short arg1)
 *   0xcbf80 = hs_return (int thread_datum, int value)
 */
void FUN_000c1680(int16_t function_index, int thread_datum, char init)
{
  short *result;

  result =
    (short *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    /* offset 0 read as signed short (MOVSX), offset 4 read as unsigned
     * short (XOR/MOV DX) per disassembly. */
    FUN_00085260(result[0], ((unsigned short *)result)[2]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc16c0 — HS script function handler: evaluate a macro function and dispatch
 * its result to FUN_00085180. Twin of the 0xc0c30 evaluate-then-dispatch
 * family. Evaluates the call via hs_macro_function_evaluate; when it returns a
 * non-NULL result block, reads three fields and dispatches, then commits 0 to
 * the calling HS thread via hs_return(thread_datum, 0).
 *
 * Field widths (verified against disassembly 0xc16c0-...): after the NULL
 * check, the +0x0 and +0x4 fields are loaded as zero-extended 16-bit values
 * (XOR reg,reg; MOV DX,[EAX] and MOV CX,[EAX+4]), and the +0x8 field is a full
 * 32-bit load (MOV EDX,[EAX+8]). Matches FUN_00085180(short, short, int).
 * A single ADD ESP,0x14 cleans the two trailing calls' pushes.
 *
 * Callees (all cdecl):
 *   0xcc560 = hs_macro_function_evaluate(int16 function_index, int
 * thread_datum, char init) -> result pointer
 *   0x85180 = FUN_00085180(short +0x0, short +0x4, int +0x8)
 *   0xcbf80 = hs_return(int thread_datum, int value)
 */
void FUN_000c16c0(int16_t function_index, int thread_datum, char init)
{
  unsigned short *result;

  result = (unsigned short *)hs_macro_function_evaluate(function_index,
                                                        thread_datum, init);
  if (result != NULL) {
    FUN_00085180(result[0], result[2], *(int *)(result + 4));
    hs_return(thread_datum, 0);
  }
}

/* HaloScript (hs) subsystem — scripting engine init/dispose/update/evaluate. */

/* Allocate and initialize the hs_syntax data table used to store script
 * nodes.  If a scenario is loaded and its script syntax data address field
 * (offset +0x474) already points to the magic value 0x5ccac, the existing
 * table is reused.  Otherwise a fresh data table is allocated (name
 * "script node", max 0x4a39 entries, datum size 0x14) and, if a scenario
 * is present, the old data pointer at scenario+0x480 is freed and the
 * scenario fields are updated to point at the new table. */
void hs_scripts_initialize(void)
{
  char *scenario_tag;

  if (*(int *)0x326a08 == -1)
    scenario_tag = 0;
  else {
    scenario_tag = (char *)global_scenario_get();
    if (scenario_tag != 0 && *(int *)(scenario_tag + 0x474) == 0x5ccac) {
      return;
    }
  }

  *(void **)0x5aa6c8 = (void *)data_new("script node", 0x4a39, 0x14);
  if (*(void **)0x5aa6c8 == 0) {
    error(0, "couldn't allocate script syntax data");
    return;
  }

  data_delete_all(*(data_t *volatile *)0x5aa6c8);

  if (scenario_tag == 0) {
    *(uint8_t *)0x46b6d9 = 1;
    return;
  }

  debug_free(*(void **)(scenario_tag + 0x480), "c:\\halo\\SOURCE\\hs\\hs.c",
             0x150);
  *(int *)(scenario_tag + 0x480) = *(int *)0x5aa6c8;
  *(int *)(scenario_tag + 0x474) = 0x5ccac;
  tag_data_resize((void *)(scenario_tag + 0x488), 0x400);
  tag_block_resize((void *)(scenario_tag + 0x49c), 0);
}

/* Dispose: clean up runtime state. */
void hs_dispose(void)
{
  hs_runtime_dispose_from_old_map();
}

/* Per-tick script update with optional profiling. */
void hs_update(void)
{
  if (*(uint8_t *)0x449ef1 != 0 && *(uint8_t *)0x2f1c18 != 0)
    profile_enter_private((void *)0x2f1c10);

  ((void (*)(void))0xcde00)();

  if (*(uint8_t *)0x449ef1 != 0 && *(uint8_t *)0x2f1c18 != 0)
    profile_exit_private((void *)0x2f1c10);
}

/* Dispose runtime script node data that isn't marked as persistent (bit 3
 * of the flags byte at datum offset +6).  Iterates over all live datums in
 * the hs_syntax data table at 0x5aa6c8 and deletes any whose flag byte
 * does NOT have bit 0x8 set. */
void hs_scripts_dispose(void)
{
  int index;

  for (index = data_next_index(*(data_t *volatile *)0x5aa6c8, NONE);
       index != NONE;
       index = data_next_index(*(data_t *volatile *)0x5aa6c8, index)) {
    void *datum = datum_get(*(data_t *volatile *)0x5aa6c8, index);
    if ((*(uint8_t *)((char *)datum + 6) & 0x8) == 0)
      datum_delete(*(data_t *volatile *)0x5aa6c8, index);
  }
}

/* 0xc3d00 — Look up an hs built-in function descriptor by index.
 * Returns a pointer into the function table at 0x2f1588 (0x1a2 entries). */
void *hs_function_table_get(int16_t function_index)
{
  if (function_index < 0 || function_index >= 0x1a2) {
    display_assert(
      "function_index>=0 && function_index<hs_function_table_count",
      "c:\\halo\\SOURCE\\hs\\hs.c", 0x20a, 1);
    system_exit(-1);
  }
  return ((void **)0x2f1588)[function_index];
}

/* 0xc3d50 — Find a scenario script by name. Iterates the scripts tag_block
 * at scenario+0x49c (element size 0x5c), comparing with csstrcmp.
 * Returns the zero-based script index, or -1 if not found. */
int16_t hs_find_script_by_name(const char *name)
{
  int16_t i;
  char *scenario;
  void *element;

  if (*(int *)0x326a08 == -1)
    return -1;

  scenario = (char *)global_scenario_get();
  for (i = 0; (int)i < *(int *)(scenario + 0x49c); i++) {
    element = tag_block_get_element((void *)(scenario + 0x49c), (int)i, 0x5c);
    if (csstrcmp(name, (const char *)element) == 0)
      return i;
  }

  return -1;
}

/* 0xc3e10 — Bounds-checked accessor for the external-global descriptor table.
 * Table at 0x2f3708, count at 0x27d504. */
void *hs_external_global_get(int16_t global_index)
{
  int16_t external_count = *(int16_t *)0x27d504;
  if (global_index < 0 || global_index >= external_count) {
    display_assert("global_index>=0 && global_index<hs_external_global_count",
                   "c:\\halo\\SOURCE\\hs\\hs.c", 0x240, 1);
    system_exit(-1);
  }
  return ((void **)0x2f3708)[global_index];
}

/* 0xc3e60 — Return the HS type of a global variable by script_ref.
 * Bit 15 set: external global (descriptor+0x4).
 * Bit 15 clear: scenario global (element+0x20, block at scenario+0x4a8). */
int16_t hs_global_get_type(uint16_t script_ref)
{
  char *scenario;
  void *element;
  void *desc;

  if (script_ref & 0x8000) {
    desc = hs_external_global_get((int16_t)(script_ref & 0x7fff));
    return *(int16_t *)((char *)desc + 0x4);
  }
  scenario = (char *)global_scenario_get();
  element = tag_block_get_element((void *)(scenario + 0x4a8),
                                  (int)(script_ref & 0x7fff), 0x5c);
  return *(int16_t *)((char *)element + 0x20);
}

/* 0xc3ea0 — Return the name string of a global variable by ref.
 * External globals (bit 15 set): name pointer at descriptor+0x0.
 * Scenario globals (bit 15 clear): name at element+0x0 in the globals
 * tag_block at scenario+0x4a8. */
const char *hs_global_get_name(uint16_t global_ref)
{
  void *desc;
  char *scenario;

  if (global_ref & 0x8000) {
    desc = hs_external_global_get((int16_t)(global_ref & 0x7fff));
    return *(const char **)desc;
  }
  scenario = (char *)global_scenario_get();
  return (const char *)tag_block_get_element((void *)(scenario + 0x4a8),
                                             (int)(global_ref & 0x7fff), 0x5c);
}

/* Find a HaloScript global variable by name.  Searches external globals
 * first (table at 0x2f3708, count at 0x27d504), returning the index with
 * bit 15 set (| 0x8000).  Then searches scenario globals (tag_block at
 * scenario+0x4a8, element size 0x5c), returning the index with bit 15
 * clear (& 0x7FFF).  Returns NONE (-1) if not found. */
int16_t hs_find_global_by_name(const char *name)
{
  int16_t external_count = *(int16_t *)0x27d504;
  int16_t i;
  char *scenario_tag;

  /* Search external globals */
  for (i = 0; i < external_count; i++) {
    if (i < 0 || i >= external_count) {
      display_assert("global_index>=0 && global_index<hs_external_global_count",
                     "c:\\halo\\SOURCE\\hs\\hs.c", 0x240, 1);
      system_exit(-1);
    }
    if (crt_stricmp(name, *(const char **)(((void **)0x2f3708)[i])) == 0) {
      return (int16_t)((uint16_t)i | 0x8000);
    }
  }

  /* Search scenario globals */
  if (*(int *)0x326a08 != NONE) {
    scenario_tag = (char *)global_scenario_get();
    for (i = 0; (int)i < *(int *)(scenario_tag + 0x4a8); i++) {
      const char *global_name = (const char *)tag_block_get_element(
        (void *)(scenario_tag + 0x4a8), (int)i, 0x5c);
      if (crt_stricmp(name, global_name) == 0) {
        return (int16_t)((uint16_t)i & 0x7FFF);
      }
    }
  }

  return NONE;
}

/* 0xc3fc0 — Search the HS function table (0x2f1588, 0x1a2 entries) for a
 * function whose name (at descriptor+4) matches case-insensitively.
 * Returns the zero-based function index, or -1 if not found. */
int16_t hs_find_function_by_name(const char *name)
{
  int16_t i;
  for (i = 0; i < 0x1a2; i++) {
    if (crt_stricmp(*(const char **)((char *)((void **)0x2f1588)[i] + 4),
                    name) == 0)
      return i;
  }
  return -1;
}

/* Load a single HaloScript source file into the scenario's source file list.
 * The file_ref is passed via EBX (register argument).
 *
 * Steps:
 *   1. Verify the file exists on disk via file_exists.
 *   2. Add a new element to the scenario's source files tag_block at +0x4c0.
 *   3. Read the file contents into a temporary memory buffer.
 *   4. Resize the element's tag_data (at element+0x20) to hold the file.
 *   5. Copy the file's short name (up to 0x1f chars) into element+0x00.
 *   6. Copy the file data from the temp buffer into the tag_data region.
 *
 * Returns true if the file was loaded successfully, false on any failure.
 * Error messages are emitted via error() at severity 2. */
bool hs_load_source_file(void *file_ref)
{
  char *scenario_tag;
  void *scripts_block;
  int16_t element_index;
  char *element;
  void *buffer;
  int file_size;
  void *tag_data_ptr;
  void *dest;
  char name_buf[256];

  scenario_tag = (char *)global_scenario_get();

  if (!file_exists((file_ref_t *)file_ref))
    return 0;

  scripts_block = (void *)(scenario_tag + 0x4c0);
  element_index = tag_block_add_element(scripts_block);
  if (element_index == -1) {
    error(2, "maximum source files per scenario exceeded.");
    return 0;
  }

  element =
    (char *)tag_block_get_element(scripts_block, (int)element_index, 0x34);

  buffer = file_read_into_buffer((file_ref_t *)file_ref, &file_size);
  if (buffer == NULL) {
    error(2, "couldn't read source file into memory.");
    return 0;
  }

  tag_data_ptr = (void *)(element + 0x20);
  if (!tag_data_resize(tag_data_ptr, file_size)) {
    error(2, "maximum source file size exceeded.");
    return 0;
  }

  file_reference_get_name((file_ref_t *)file_ref, 4, name_buf);
  csstrncpy(element, name_buf, 0x1f);
  *(uint8_t *)(element + 0x1f) = 0;

  dest = tag_data_get_pointer(tag_data_ptr, 0, file_size);
  csmemcpy(dest, buffer, file_size);

  return 1;
}

/* Check whether the scenario's HaloScript source files have changed on
 * disk since they were last compiled.  Searches for "data\global_scripts.hsc"
 * and all .hsc files in the scenario's "data\<mapname>\scripts\" directory.
 * For each file found, calls FUN_0xc4660 (hs_load_source_file, @EBX=file_ref)
 * to attempt loading.  Returns true (1) if all source files loaded
 * successfully, false (0) if any failed (indicating recompilation needed).
 *
 * Uses find_files to enumerate .hsc files, sorts them with qsort using
 * stricmp-based comparison (FUN_0xc4770), then iterates.  The file extension
 * "hsc" at 0x27ba34 is compared with csstrcmp to filter results. */
bool hs_needs_recompile(void)
{
  uint8_t result;
  char *scenario_tag;
  char path[260]; /* EBP+0xfffffef8 => EBP-0x108, size 0x104 */
  char global_ref[268]; /* EBP+0xfffffdf0 => EBP-0x210, file_ref_t */
  char dir_ref[268]; /* EBP+0xfffffbe4 => EBP-0x41c, file_ref_t */
  char results[2144]; /* EBP+0xfffff384 => EBP-0xc7c, 8*0x10c */
  char name_buf[256]; /* EBP+0xfffffcf0 => EBP-0x310 */
  int16_t count;
  int16_t i;
  const char *name_result;
  char *ebx_ptr;

  result = 1;
  scenario_tag = (char *)global_scenario_get();

  /* Reset scenario source file list at +0x4c0 */
  tag_block_resize((void *)(scenario_tag + 0x4c0), 0);

  /* Get the tag name for the current scenario and build the scripts path */
  name_result = tag_get_name(*(int *)0x326a08);
  crt_sprintf(path, "data\\%s", name_result);
  {
    char *last_sep = strrchr(path, 0x5c);
    crt_sprintf(last_sep + 1, "scripts");
  }

  /* Check for global_scripts.hsc */
  file_reference_create_from_path((file_ref_t *)global_ref,
                                  "data\\global_scripts.hsc", 0);
  if (file_exists((file_ref_t *)global_ref)) {
    result = (uint8_t)hs_load_source_file((void *)global_ref);
  }

  /* Find all .hsc files in the scripts directory */
  file_reference_create_from_path((file_ref_t *)dir_ref, path, 1);
  count = find_files(0, (file_ref_t *)dir_ref, 8, (file_ref_t *)results);

  /* Sort the results by name using the comparison callback at 0xc4770 */
  qsort((void *)results, (size_t)(int16_t)count, 0x10c,
        (int (*)(const void *, const void *))0xc4770);

  if ((int16_t)count > 0) {
    ebx_ptr = results;
    for (i = (uint16_t)count; i != 0; i--) {
      file_reference_get_name((file_ref_t *)ebx_ptr, 8, name_buf);
      if (csstrcmp(name_buf, (const char *)0x27ba34) == 0) {
        if (!hs_load_source_file((void *)ebx_ptr))
          result = 0;
      }
      ebx_ptr += 0x10c;
    }
  }

  return (bool)result;
}

/* Report a HaloScript compile error with optional line-number context.
 *
 * Register arguments: error_text@ESI, script_element@EBX, source_start@EDI.
 * Stack argument: error_info (the error description string).
 *
 * If error_text is non-NULL, truncates it at the first newline.  If both
 * script_element and a newline were found, counts newlines backward from the
 * newline position to source_start to determine the line number, then prints:
 *   "[<script_element> line <N>] <error_info>: <error_text>"
 * Otherwise prints the simpler:
 *   "<error_info>: <error_text>"
 *
 * The line counter is treated as int16_t (MOVSX ECX,CX in the original). */
void hs_report_compile_error(void *error_info, char *error_text,
                             char *script_element, void *source_start)
{
  char *nl;
  char *ptr;
  int16_t line;

  nl = NULL;
  if (error_text != NULL) {
    nl = crt_strchr(error_text, '\n');
    if (nl != NULL)
      *nl = '\0';
  }

  if (script_element != NULL && nl != NULL) {
    line = 1;
    ptr = nl;
    if (ptr > (char *)source_start) {
      do {
        if (*ptr == '\n')
          line++;
        ptr--;
      } while (ptr > (char *)source_start);
    }
    error(2, "[%s line %d] %s: %s", script_element, (int)line, error_info,
          error_text);
    return;
  }

  error(2, (const char *)0x259f2c, error_info, error_text);
}

/* Recompile all scenario scripts.  Iterates over each script source entry
 * in the scenario's script sources tag_block at offset +0x4c0 (element size
 * 0x34).  For each, calls tag_data_get_pointer to get the source text, then
 * hs_compile_source to compile it.  If compilation fails, calls the error
 * reporter at 0xc4900 (which takes 3 register args: @ESI, @EBX, @EDI plus
 * 1 stack arg).  Returns true if all scripts compiled successfully.
 * Prints "scripts successfully compiled." on success via console_printf. */
bool hs_mark_recompile(void)
{
  char *scenario_tag;
  void *scripts_block; /* &scenario+0x4c0 */
  char *element;
  void *tag_data_ptr; /* &element[0x20] */
  int source_size;
  void *source_ptr;
  char *error_info;
  char *error_text;
  uint8_t ok;
  int loop_index;
  int i;

  scenario_tag = (char *)global_scenario_get();
  ok = 1;
  hs_syntax_reset(1);

  scripts_block = (void *)(scenario_tag + 0x4c0);
  i = 0;
  loop_index = 0;

  if (*(int *)scripts_block > 0) {
    do {
      element = (char *)tag_block_get_element(scripts_block, i, 0x34);

      tag_data_ptr = (void *)(element + 0x20);
      source_size = *(int *)(element + 0x20);

      source_ptr = tag_data_get_pointer(tag_data_ptr, 0, source_size);
      hs_compile_source(*(int *)tag_data_ptr, source_ptr, &error_info,
                        &error_text);

      if (error_info != 0) {
        /* Get source start pointer again for error reporting */
        void *src_start =
          tag_data_get_pointer(tag_data_ptr, 0, *(int *)tag_data_ptr);
        hs_report_compile_error(error_info, error_text, element, src_start);
        ok = 0;
      }

      loop_index = loop_index + 1;
      i = (int)(int16_t)loop_index;
    } while (i < *(int *)scripts_block);

    if (ok == 0)
      goto cleanup;
  }

  console_printf(0, "scripts successfully compiled.");

cleanup:
  hs_compile_cleanup();
  return (bool)ok;
}

/* 0xc4b40 — HS console command handler: set the recompile flag.
 * Sets the global recompile flag at 0x46b6d8 to 1, then returns void
 * to the HS thread via hs_return(thread_datum, 0). */
void FUN_000c4b40(int16_t function_index, int thread_datum, char init)
{
  *(uint8_t *)0x46b6d8 = 1;
  hs_return(thread_datum, 0);
}

/* 0xc4b60 — HS script function handler: random integer in range.
 * Evaluates the macro arguments to get (min, max) short values from the
 * hs_macro_function_evaluate result. Advances the global random seed and
 * returns a random value in [min, max] to the HS thread. */
void FUN_000c4b60(int16_t function_index, int thread_datum, char init)
{
  short *result;
  int16_t value;

  result =
    (short *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    value = random_range((unsigned int *)get_global_random_seed_address(),
                         result[0], result[2]);
    hs_return(thread_datum, (int)value);
  }
}

/* Load scenario scripts from the scenario tag.  Allocates a fresh syntax
 * data table via hs_scripts_initialize, then either validates existing
 * compiled scripts or recompiles from source.  If the scenario has no
 * pre-existing globals (offset +0x49c count == 0) but has scripts (offset
 * +0x4c0 count > 0), a full recompile is triggered.  On failure, resets
 * the script tables to empty.
 *
 * If preserve_syntax is non-zero, restores the original hs_syntax_data
 * pointer on exit (used during console evaluate recompile). */
bool hs_load_scenario_scripts(int preserve_syntax)
{
  char *scenario_tag;
  void *old_syntax_data;
  bool needs_recompile;
  bool ok;
  char *error_info;
  char *error_text;

  ok = 1;
  scenario_tag = (char *)global_scenario_get();
  old_syntax_data = *(void **)0x5aa6c8;

  hs_scripts_initialize();

  /* Determine if recompilation is needed: no existing globals but
   * scripts exist */
  if (*(int *)(scenario_tag + 0x49c) == 0 && *(int *)(scenario_tag + 0x4c0) > 0)
    needs_recompile = 1;
  else
    needs_recompile = 0;

  /* Set up syntax data pointer from scenario tag data */
  *(void **)0x5aa6c8 = *(void **)(scenario_tag + 0x480);
  {
    char *syn = *(char **)0x5aa6c8;
    *(int *)(syn + 0x34) = (int)(syn + 0x38);
  }

  if (needs_recompile) {
    /* Scenarios were merged — must recompile */
    error(0, "recompiling scripts after scenarios were merged.");
  } else {
    /* Try to validate existing compiled syntax tree */
    if (hs_validate_syntax(&error_info, &error_text)) {
      /* Validation succeeded — grow string data if needed */
      if (*(int *)(scenario_tag + 0x488) < 0x400) {
        ok = tag_data_resize((void *)(scenario_tag + 0x488),
                             *(int *)(scenario_tag + 0x488) + 0x400);
      }
      goto done;
    }

    /* Validation failed — report the error.
     * Note: when both strings present, error_text is printed first
     * in the "%s: %s" format, matching the original push order. */
    if (error_info == 0) {
      error(0, "an unspecified error occurred loading scripts");
    } else if (error_text == 0) {
      error(0, (const char *)0x257984, error_info);
    } else {
      error(0, (const char *)0x259f2c, error_text, error_info);
    }
  }

  /* Recompile path */
  if (hs_mark_recompile()) {
    if (hs_validate_syntax(&error_info, &error_text)) {
      ok = 1;
      goto done;
    }
  }

  /* Recompile failed or validation still fails — reset everything */
  data_make_valid(*(data_t *volatile *)0x5aa6c8);
  if (!tag_block_resize((void *)(scenario_tag + 0x4a8), 0))
    goto reset_error;
  if (!tag_block_resize((void *)(scenario_tag + 0x49c), 0))
    goto reset_error;
  {
    char *s2 = (char *)global_scenario_get();
    if (!tag_data_resize((void *)(s2 + 0x488), 0x400))
      goto reset_error;
  }
  goto reset_ok;

reset_error:
  error(0, "couldn't reset scripts.");

reset_ok:
  ok = 0;

done:
  if (preserve_syntax != 0)
    *(void **)0x5aa6c8 = old_syntax_data;

  return ok;
}

/* Initialize hs for a new map: set up the script environment from the
 * scenario's script data if present. */
void hs_initialize_for_new_map(void)
{
  char *scenario_tag;

  if (*(int *)0x326a08 == -1)
    scenario_tag = 0;
  else
    scenario_tag = (char *)global_scenario_get();

  hs_scripts_initialize();

  if (scenario_tag != 0 && *(int *)(scenario_tag + 0x474) != 0)
    hs_load_scenario_scripts(0);

  hs_runtime_initialize();
  hs_runtime_initialize_for_new_map();
}

/* Dispose from old map: clean up script threads and runtime data. */
void hs_dispose_from_old_map(void)
{
  if (*(void **)0x5aa6c8 != 0) {
    hs_scripts_dispose();
    if (*(uint8_t *)0x46b6d9 != 0) {
      data_make_invalid(*(data_t **)0x5aa6c8);
      data_dispose(*(data_t **)0x5aa6c8);
      *(uint8_t *)0x46b6d9 = 0;
    }
    *(void **)0x5aa6c8 = 0;
  }
  hs_runtime_dispose_from_old_map();
  hs_runtime_dispose();
}

/* 0xc4ff0 — HS console command handler: print documentation.
 * Calls hs_doc() to dump HaloScript documentation to the console,
 * then returns void to the HS thread. */
void FUN_000c4ff0(int16_t function_index, int thread_datum, char init)
{
  hs_doc();
  hs_return(thread_datum, 0);
}

/* 0xc5010 — HS console command handler: context-sensitive help.
 * Evaluates the macro argument via hs_macro_function_evaluate. If
 * evaluation succeeds (non-zero return), calls hs_help() and returns
 * void to the HS thread. */
void FUN_000c5010(int16_t function_index, int thread_datum, char init)
{
  int result;

  result = hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != 0) {
    hs_help();
    hs_return(thread_datum, 0);
  }
}

/* Initialize the scripting engine: validate type names, set up
 * runtime tables, and initialize for the current map. */
void hs_initialize(void)
{
  char *scenario_tag;

  if (*(void **)0x2f1568 == 0) {
    display_assert("you can't add an hs type without defining its name.",
                   "c:\\halo\\SOURCE\\hs\\hs.c", 0xf5, 1);
    system_exit(-1);
  }

  ((void (*)(void))0xce150)();
  ((void (*)(void))0xca700)();

  if (*(int *)0x326a08 == -1)
    scenario_tag = 0;
  else
    scenario_tag = (char *)global_scenario_get();

  hs_scripts_initialize();

  if (scenario_tag != 0 && *(int *)(scenario_tag + 0x474) != 0)
    hs_load_scenario_scripts(0);

  hs_runtime_initialize();
  hs_runtime_initialize_for_new_map();
}

/*
 * hs_console_evaluate — parse and evaluate a console command as HaloScript.
 *
 * Strips comments at ';', skips leading whitespace, then wraps the command
 * in S-expression syntax if needed:
 *   - If the command starts with '(', pass it through as-is.
 *   - If the first word is a known global variable:
 *     - With arguments: wrap as "(set <command>)" to assign.
 *     - Without arguments: pass through to read the global.
 *   - Otherwise: wrap as "(<command>)" to call as a function.
 *
 * After evaluation, if the recompile flag (0x46b6d8) is set, tears down
 * and reinitializes the script environment.
 *
 * Returns true if the command compiled and executed successfully.
 *
 * Confirmed: csstrncpy(copy, command, 0x400), null-terminate at [0x3ff].
 * Confirmed: strchr for ';' zeroes copy[0] (treats ';' lines as comments).
 * Confirmed: isspace loop to skip leading whitespace.
 * Confirmed: strchr for ' ' splits first word, hs_find_global_by_name.
 * Confirmed: sprintf with "(%s)" at 0x27bb64 or "(set %s)" at 0x27bb6c.
 * Confirmed: interleaved pushes — csstrlen(1 arg) then hs_compile(4 args).
 * Confirmed: error(2, "%s: %s", ...) at 0x259f2c on parse failure.
 * Confirmed: assert at line 0x507 for unreachable switch case.
 * Confirmed: post-eval recompile mirrors hs_initialize_for_new_map logic.
 */
bool hs_console_evaluate(const char *command)
{
  char wrapped[1024];
  char copy[1024];
  int error_info;
  char *error_text;
  bool result;
  const char *source;
  char *space;
  int16_t mode;
  int compiled;
  int executed;
  char *scenario_tag;

  result = 0;
  csstrncpy(copy, command, 0x400);
  copy[0x3ff] = 0;

  /* strip comments: if ';' is present anywhere, blank the command */
  if (crt_strchr(copy, ';') != NULL) {
    copy[0] = 0;
  }

  /* skip leading whitespace */
  source = copy;
  while (*source != 0) {
    if (crt_isspace((int)(unsigned char)*source) == 0)
      break;
    source++;
  }
  if (*source == 0)
    goto post_eval;

  mode = 0;
  hs_syntax_reset(0);

  if (copy[0] != '(') {
    space = crt_strchr(copy, ' ');
    if (space != NULL)
      *space = 0;

    if (hs_find_global_by_name(copy) == -1) {
      mode = 1;
    } else {
      if (space == NULL)
        goto skip_wrap;
      mode = 2;
    }

    if (space != NULL)
      *space = ' ';
  }

skip_wrap:
  if (mode != 0) {
    if (mode == 1) {
      crt_sprintf(wrapped, (const char *)0x27bb64, copy);
    } else if (mode == 2) {
      crt_sprintf(wrapped, (const char *)0x27bb6c, copy);
    } else {
      display_assert(NULL, "c:\\halo\\SOURCE\\hs\\hs.c", 0x507, 1);
      system_exit(-1);
    }
    source = wrapped;
  }

  compiled = csstrlen(source);
  executed = hs_compile(compiled, source, &error_info, &error_text);

  if (executed != -1) {
    result = 1;
    hs_runtime_execute(executed);
  } else {
    if (error_info != 0) {
      if (error_text != NULL) {
        char *nl = crt_strchr(error_text, '\n');
        if (nl != NULL)
          *nl = 0;
      }
      error(2, (const char *)0x259f2c, error_info, error_text);
    }
  }

  hs_compile_cleanup();

post_eval:
  if (*(uint8_t *)0x46b6d8 != 0) {
    if (hs_needs_recompile()) {
      hs_mark_recompile();
      if (*(void **)0x5aa6c8 != 0) {
        hs_scripts_dispose();
        if (*(uint8_t *)0x46b6d9 != 0) {
          data_make_invalid(*(data_t **)0x5aa6c8);
          data_dispose(*(data_t **)0x5aa6c8);
          *(uint8_t *)0x46b6d9 = 0;
        }
        *(void **)0x5aa6c8 = 0;
      }
      hs_runtime_dispose_from_old_map();
      hs_runtime_dispose();

      if (*(int *)0x326a08 == -1) {
        scenario_tag = 0;
      } else {
        scenario_tag = (char *)global_scenario_get();
      }

      hs_scripts_initialize();

      if (scenario_tag != 0 && *(int *)(scenario_tag + 0x474) != 0)
        hs_load_scenario_scripts(0);

      hs_runtime_initialize();
      hs_runtime_initialize_for_new_map();
    }
    *(uint8_t *)0x46b6d8 = 0;
  }

  return result;
}

/* Reset the HaloScript compile state.  Asserts that the compiler is not
 * already initialized (global at 0x46b6e0).  Zeroes compile globals and
 * stores the recompile flag (param_1) at 0x46b805.  If param_1 is non-zero,
 * also resets several scenario tag_block and tag_data structures (globals,
 * scripts, source files) and resets the syntax data table index. */
void hs_syntax_reset(int param_1)
{
  char *scenario_tag;

  if (*(uint8_t *)0x46b6e0 != 0) {
    display_assert("!hs_compile_globals.initialized",
                   "c:\\halo\\SOURCE\\hs\\hs_compile.c", 0x5b, 1);
    system_exit(-1);
  }

  *(uint8_t *)0x46b6e0 = 1;
  *(int *)0x46b6e8 = 0;
  *(int *)0x46b6e4 = 0;
  *(uint8_t *)0x46b805 = (uint8_t)param_1;
  *(uint8_t *)0x46b6f8 = 0;
  *(int *)0x46b6fc = 0;

  if ((uint8_t)param_1 != 0) {
    scenario_tag = (char *)global_scenario_get();
    tag_block_resize((void *)(scenario_tag + 0x49c), 0);
    tag_block_resize((void *)(scenario_tag + 0x4a8), 0);
    tag_block_resize((void *)(scenario_tag + 0x4b4), 0);
    tag_data_resize((void *)(scenario_tag + 0x488), 0);
    data_make_valid(*(data_t *volatile *)0x5aa6c8);
  }
}

/* 0xc57a0 — Check whether a source offset is within the valid range
 * [0, hs_compile_globals.source_size). Sets error message on failure. */
bool hs_source_offset_valid(int offset)
{
  if (offset < 0 || offset >= *(int *)0x46b6e4) {
    *(const char **)0x46b6fc = "bad source offset (you need to recompile.)";
    return false;
  }
  return true;
}
