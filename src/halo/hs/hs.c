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

/* 0xc1700 — HS script function handler. Evaluates the macro arguments; on
 * success the result block holds an int at +0x0 (result[0]) and a char*
 * string pointer at +0x4 (result[1]). Calls FUN_00085000(int, const char*)
 * with those two fields, then returns void to the HS thread via
 * hs_return(thread_datum, 0). Matches the byte pattern of the sibling HS
 * handlers in this TU. */
void FUN_000c1700(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    FUN_00085000(result[0], (const char *)result[1]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc1740 — HS script function handler: evaluate a macro function and dispatch
 * the result's first field to FUN_000850d0. On success the result block holds
 * an int at +0x0 (result[0]); calls FUN_000850d0(result[0]), then returns void
 * to the HS thread via hs_return(thread_datum, 0). Matches the byte pattern of
 * the sibling HS handlers in this TU. */
void FUN_000c1740(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    FUN_000850d0(result[0]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc1780 — HS script function handler: evaluate a macro function and dispatch
 * the result's first field to FUN_00085110 (switches to first-person camera
 * mode 3).  The original consumes the result block with a single dword load
 * (`MOV EDX,[EAX]; PUSH EDX`) — only result[0] is read, unlike the 0xc0d90 /
 * 0xc0dd0 twins which also read result[1].  On success calls
 * FUN_00085110(result[0]), then returns void to the HS thread via
 * hs_return(thread_datum, 0). */
void FUN_000c1780(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    FUN_00085110(result[0]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc17c0 — HS script function handler (zero-argument built-in): query a
 * global engine value and commit it to the calling thread.
 *
 * Unlike its siblings in this TU this handler takes no script arguments at
 * all, so it never calls hs_macro_function_evaluate and has no NULL check.
 * It simply calls FUN_000853a0() (cdecl, no args, result in EAX) and returns
 * that value to the thread.
 *
 * The value is zero-extended from 16 bits: disasm zero-initializes the whole
 * 4-byte stack local up front (`mov DWORD PTR [ebp-4],0` at the top of the
 * frame), then overwrites only its low word from AX (`mov WORD PTR
 * [ebp-4],ax`), then reads the full dword back (`mov eax,[ebp-4]`) for the
 * hs_return argument — the high 2 bytes stay 0.  A union reproduces this
 * partial-store / wide-read exactly; a signed `short` cast would emit MOVSX
 * and diverge.
 *
 * The function_index and init parameters are never read (EBP+8 and EBP+0x10
 * are untouched); they are kept so the stack shape matches the TU's other
 * script-function handlers.  thread_datum is read from EBP+0xc into ECX and
 * pushed as the first hs_return argument (cdecl: last PUSH = first arg).
 *
 * Callees:
 *   0x853a0 = FUN_000853a0(void) -> int (low word consumed in AX)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c17c0(int16_t function_index, int thread_datum, char init)
{
  union {
    int i;
    unsigned short w;
  } value;

  value.i = 0;
  value.w = (unsigned short)FUN_000853a0();
  hs_return(thread_datum, value.i);
}

/* 0xc17f0 — HS script function handler (zero-argument, void-returning
 * built-in): save the current director camera, then complete the calling script
 * thread.
 *
 * Like its immediate neighbour 0xc17c0 this handler takes no script arguments,
 * so it never calls hs_macro_function_evaluate and has no NULL check.  The body
 * is 8 instructions: plain EBP frame with no locals, a zero-argument call, then
 * hs_return with a constant 0 result.
 *
 * Disassembly (0xc17f0-0xc1806):
 *   PUSH EBP / MOV EBP,ESP      plain frame, no locals, no _chkstk
 *   CALL 0x00086360             director_save_camera(), zero args
 *   MOV EAX,[EBP+0xc]           thread_datum (arg 2)
 *   PUSH 0x0                    hs_return arg2 = 0 (pushed first => last arg)
 *   PUSH EAX                    hs_return arg1 = thread_datum
 *   CALL 0x000cbf80             hs_return(thread_datum, 0)
 *   ADD ESP,0x8                 cdecl cleanup, 2 args
 *   POP EBP / RET               cdecl, plain RET
 *
 * function_index (EBP+0x8) and init (EBP+0x10) are never read; they are kept
 * so the stack shape matches this TU's other script-function handlers, which
 * are all dispatched through the same hs function table with three arguments.
 *
 * Callees (both cdecl):
 *   0x86360 = director_save_camera(void)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c17f0(int16_t function_index, int thread_datum, char init)
{
  director_save_camera();
  hs_return(thread_datum, 0);
}

/* 0xc1810 — HaloScript script-function handler: load the saved director camera
 * and immediately commit the calling thread.  Direct twin of FUN_000c17f0
 * (0xc17f0, director_save_camera); this one dispatches to director_load_camera.
 * It takes no HS arguments, so it never calls hs_macro_function_evaluate and
 * has no NULL/pending check — the thread is always committed on this call.
 *
 * Disassembly (0xc1810-0xc1827), 10 instructions, plain EBP frame, no locals:
 *   PUSH EBP / MOV EBP,ESP      plain frame, no locals, no _chkstk
 *   CALL 0x00086900             director_load_camera(), zero args
 *   MOV EAX,[EBP+0xc]           thread_datum (arg 2)
 *   PUSH 0x0                    hs_return arg2 = 0 (pushed first => last arg)
 *   PUSH EAX                    hs_return arg1 = thread_datum
 *   CALL 0x000cbf80             hs_return(thread_datum, 0)
 *   ADD ESP,0x8                 cdecl cleanup, 2 args
 *   POP EBP / RET               cdecl, plain RET (caller cleans)
 *
 * function_index (EBP+0x8) and init (EBP+0x10) are never read; they are kept
 * so the stack shape matches this TU's other script-function handlers, which
 * are all dispatched through the same hs function table with three arguments.
 *
 * Callees (both cdecl, no register args):
 *   0x86900 = director_load_camera(void)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c1810(int16_t function_index, int thread_datum, char init)
{
  director_load_camera();
  hs_return(thread_datum, 0);
}

/* 0xc1830 — HaloScript script-function handler: set the game time speed.
 * Evaluates the macro arguments through hs_macro_function_evaluate; on a
 * non-NULL result block the first dword is the new speed, read as a float and
 * handed to game_time_set_speed, after which the calling thread is committed
 * with hs_return(thread_datum, 0).  A NULL result means the arguments are
 * still pending, so the thread is left uncommitted and nothing is applied —
 * the same evaluate/NULL-check/act/hs_return skeleton as the other handlers
 * in this TU (see FUN_000c0c30 / FUN_000c0c70).
 *
 * Disassembly (0xc1830-0xc1861), plain EBP frame plus a saved ESI, no locals:
 *   PUSH EBP / MOV EBP,ESP      plain frame, no locals, no _chkstk
 *   MOV EAX,[EBP+0x10]          init      (arg 3)
 *   MOV ECX,[EBP+0x8]           function_index (arg 1)
 *   PUSH ESI / MOV ESI,[EBP+0xc]  thread_datum held in ESI for the whole body
 *   PUSH EAX / PUSH ESI / PUSH ECX   first PUSH is the LAST cdecl arg, so the
 *                               call order is (function_index, thread_datum,
 * init) CALL 0x000cc560             hs_macro_function_evaluate(...) ADD ESP,0xc
 * cdecl cleanup, 3 args TEST EAX,EAX / JZ           plain NULL check on the
 * result block pointer MOV EDX,[EAX] / PUSH EDX    result[0] pushed as raw
 * 32-bit float bits — no FPU instruction appears here, so the value must stay
 * float-typed in C; an int local would emit FILD and silently change the
 * argument CALL 0x000b5d00             game_time_set_speed(result[0]) PUSH 0x0
 * / PUSH ESI         hs_return arg2 = 0, arg1 = thread_datum CALL 0x000cbf80
 * hs_return(thread_datum, 0) ADD ESP,0xc                 ONE coalesced cleanup
 * for BOTH calls (1 + 2 pushes); the call-site audit's "cleanup=3 vs decl=2"
 * note on hs_return is that coalescing, not an argument-count mismatch POP ESI
 * / POP EBP / RET     cdecl, plain RET (caller cleans)
 *
 * Callees (all cdecl, no register args):
 *   0xcc560 = hs_macro_function_evaluate(function_index, thread_datum, init)
 *   0xb5d00 = game_time_set_speed(float)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c1830(int16_t function_index, int thread_datum, char init)
{
  float *result;

  result =
    (float *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    game_time_set_speed(result[0]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc1870 - HS script function handler: select the multiplayer game variant by
 * name.  Same three-call shape as FUN_000c0f10 (0xc0f10) and FUN_000c1830
 * (0xc1830); the only difference is the type of the first dword of the
 * evaluated result block, which here is a `const char *` variant name.
 *
 * Disassembly trace (0xc1870-0xc18a1, 22 instructions):
 *   PUSH EBP / MOV EBP,ESP / PUSH ESI  no `sub esp` - one local, no spills
 *   EAX=[EBP+0x10] init, ECX=[EBP+0x08] function_index,
 *   ESI=[EBP+0x0C] thread_datum (kept live in ESI across the whole body)
 *   PUSH EAX / PUSH ESI / PUSH ECX     cdecl, first push is the LAST arg, so
 *                                      the call is (function_index,
 *                                      thread_datum, init)
 *   CALL 0x000cc560 / ADD ESP,0xc      hs_macro_function_evaluate, own cleanup
 *   TEST EAX,EAX / JZ                  plain NULL check on the result block
 *   MOV EDX,[EAX] / PUSH EDX           the argument is the FIRST DWORD OF THE
 *                                      BLOCK, dereferenced - *(char **)result,
 *                                      not the block pointer itself
 *   CALL 0x000a78e0                    game_set_game_variant_from_name(*result)
 *   PUSH 0x0 / PUSH ESI                hs_return arg2 = 0, arg1 = thread_datum
 *   CALL 0x000cbf80                    hs_return(thread_datum, 0)
 *   ADD ESP,0xc                        ONE coalesced cleanup for BOTH calls
 *                                      (1 + 2 pushes); the call-site audit's
 *                                      "cleanup=3 vs decl=2" note on hs_return
 *                                      is that coalescing, not an
 * argument-count mismatch POP ESI / POP EBP / RET            cdecl, plain RET
 * (caller cleans)
 *
 * The evaluator's return is declared int in kb.json (0xcc560) but is used here
 * as a pointer, so it is cast.
 *
 * Callees (all cdecl, no register args):
 *   0xcc560 = hs_macro_function_evaluate(function_index, thread_datum, init)
 *   0xa78e0 = game_set_game_variant_from_name(const char *)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c1870(int16_t function_index, int thread_datum, char init)
{
  char **result;

  result =
    (char **)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    game_set_game_variant_from_name(*result);
    hs_return(thread_datum, 0);
  }
}

/* 0xc18b0 - HS script function handler: return the current game time to the
 * calling script thread.  A pure forwarder - no `init` gating and no macro
 * evaluation, because the builtin takes no script arguments.
 *
 * Disassembly trace (0xc18b0-0xc18c6, 10 instructions):
 *   PUSH EBP / MOV EBP,ESP             no `sub esp` - no locals, no spills,
 *                                      no callee-saved registers pushed
 *   CALL 0x000b5aa0                    game_time_get(), result in EAX
 *   PUSH EAX                           first PUSH is the LAST cdecl arg, so
 *                                      this is hs_return's `value`
 *   MOV EAX,[EBP+0x0C] / PUSH EAX      thread_datum - the SECOND stack
 *                                      argument.  Ghidra names this
 *                                      `in_stack_00000008` (arg 1); the
 *                                      [EBP+0x0C] displacement proves it is
 *                                      arg 2, matching the standard
 *                                      hs-evaluator slot layout
 *                                      (function_index@+8, thread_datum@+0xC,
 *                                      init@+0x10).
 *   CALL 0x000cbf80 / ADD ESP,0x8      hs_return(thread_datum, game_time)
 *   POP EBP / RET                      cdecl, plain RET (caller cleans)
 *
 * The call order in the binary (game_time_get first, then the [EBP+0xC] load)
 * is exactly MSVC's right-to-left cdecl argument evaluation for the single
 * expression below; no temporary is needed to reproduce it.
 *
 * function_index and init are unused by this body but complete the standard
 * hs-evaluator signature (same shape as FUN_000c0cb0 at 0xc0cb0).
 *
 * Callees (both cdecl, no register args, both ported):
 *   0xb5aa0 = game_time_get(void)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c18b0(int16_t function_index, int thread_datum, char init)
{
  hs_return(thread_datum, game_time_get());
}

/* 0xc18d0 — HaloScript function evaluator that returns the 16-bit result of
 * the helper at 0xa7470 to the calling script thread.
 *
 * Disassembly (0xc18d0-0xc18f7, 15 instructions):
 *   PUSH EBP / MOV EBP,ESP / PUSH ECX   single 4-byte local at EBP-4
 *   MOV dword [EBP-4],0                 zero the FULL dword first
 *   CALL 0x000a7470                     no args pushed -> void-arg helper
 *   MOV ECX,[EBP+0xc]                   thread_datum (hs-evaluator arg 2)
 *   MOV word [EBP-4],AX                 store only the LOW WORD of the result
 *   MOV EAX,[EBP-4]                     reload the (now zero-extended) dword
 *   PUSH EAX / PUSH ECX                 cdecl: first PUSH is the last arg, so
 *   CALL 0x000cbf80 / ADD ESP,8         hs_return(thread_datum, value)
 *   MOV ESP,EBP / POP EBP / RET         cdecl, plain RET (caller cleans)
 *
 * The zero-dword-then-word-store is load bearing: the 16-bit helper result is
 * ZERO-extended into the 32-bit hs_return slot, so the high word is
 * deliberately 0.  A `(short)` cast would emit MOVSX and produce a different
 * value for results >= 0x8000 — hence the explicit zeroed slot plus a 16-bit
 * store rather than a widening assignment.
 *
 * [EBP+0x8] (function_index) and [EBP+0x10] (init) are never read by this
 * body; they complete the standard hs-evaluator signature shared by every
 * other handler in this TU (same shape as FUN_000c18b0 at 0xc18b0).
 *
 * Callees (both cdecl, no register args):
 *   0xa7470 = FUN_000A7470(void) — returns a 16-bit value in AX (unported;
 *             its kb decl was widened from void to int16_t so the result is
 *             not silently discarded)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c18d0(int16_t function_index, int thread_datum, char init)
{
  int32_t value;

  value = 0;
  *(int16_t *)&value = FUN_000A7470();
  hs_return(thread_datum, value);
}

/* 0xc1900 — HaloScript function evaluator "game_difficulty_get": returns the
 * current campaign difficulty level to the calling script thread.
 *
 * Disassembly (0xc1900-0xc1927, 15 instructions, 0x28 bytes):
 *   PUSH EBP / MOV EBP,ESP / PUSH ECX   single 4-byte local at EBP-4
 *   MOV dword [EBP-4],0                 zero the FULL dword first
 *   CALL 0x000a7460                     game_difficulty_level_get(void) -> AX
 *   MOV ECX,[EBP+0xc]                   thread_datum (hs-evaluator arg 2)
 *   MOV word [EBP-4],AX                 store only the LOW WORD of the result
 *   MOV EAX,[EBP-4]                     reload the (now zero-extended) dword
 *   PUSH EAX / PUSH ECX                 cdecl: first PUSH is the last arg, so
 *   CALL 0x000cbf80 / ADD ESP,8         hs_return(thread_datum, value)
 *   MOV ESP,EBP / POP EBP / RET         cdecl, plain RET (caller cleans)
 *
 * Byte-for-byte the same shape as FUN_000c18d0 at 0xc18d0; only the producer
 * call differs.  The zero-dword-then-word-store is load bearing: the 16-bit
 * difficulty is ZERO-extended into the 32-bit hs_return slot.  A `(short)`
 * cast would emit MOVSX instead.
 *
 * [EBP+0x8] (function_index) and [EBP+0x10] (init) are never read by this
 * body; they complete the standard hs-evaluator signature shared by every
 * other handler in this TU.
 *
 * Callees (both cdecl, no register args, both ported):
 *   0xa7460 = game_difficulty_level_get(void) — returns int16_t in AX
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c1900(int16_t function_index, int thread_datum, char init)
{
  int32_t value;

  value = 0;
  *(int16_t *)&value = game_difficulty_level_get();
  hs_return(thread_datum, value);
}

/* 0xc1930 — HaloScript script command "players_unzoom_all": drop every player's
 * zoom level back to unzoomed, then commit a void result to the calling script
 * thread.
 *
 * Disassembly (0xc1930-0xc194?, 10 instructions):
 *   PUSH EBP / MOV EBP,ESP              no locals: no SUB ESP, no PUSH ECX
 *   CALL 0x000b69d0                     players_unzoom_all(void), no args
 *   MOV EAX,[EBP+0xc]                   thread_datum (hs-evaluator arg 2)
 *   PUSH 0x0                            cdecl: first PUSH is the LAST arg
 *   PUSH EAX                            -> hs_return(thread_datum, 0)
 *   CALL 0x000cbf80 / ADD ESP,8         cdecl, 2 dword args
 *   POP EBP / RET                       plain RET (caller cleans) => __cdecl
 *
 * The frame is PUSH EBP / MOV EBP,ESP only — the epilogue is POP EBP, not
 * MOV ESP,EBP / POP EBP — so this body must declare NO local variable, unlike
 * the int16-result handlers above it.  [EBP+0x8] (function_index) and
 * [EBP+0x10] (init) are never read; they complete the standard hs-evaluator
 * signature shared by every other handler in this TU.
 *
 * Callees (both cdecl, no register args, both ported):
 *   0xb69d0 = players_unzoom_all(void)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c1930(int16_t function_index, int thread_datum, char init)
{
  players_unzoom_all();
  hs_return(thread_datum, 0);
}

/* 0xc1950 — HS script function handler: enable/disable player input.
 * Evaluates the macro arguments; on success the result block holds a single
 * boolean byte at +0x0.  Calls player_input_enable(*(char *)result) then
 * returns void to the HS thread via hs_return(thread_datum, 0).
 *
 * Disassembly notes (0xc1950-0xc1983, 52 bytes):
 *   PUSH EBP / MOV EBP,ESP / PUSH ESI    no SUB ESP => no stack locals
 *   PUSH EAX([EBP+0x10]) / PUSH ESI([EBP+0xc]) / PUSH ECX([EBP+0x8])
 *   CALL 0x000cc560 / ADD ESP,0xc        cdecl: first PUSH is the LAST arg
 *                                        -> hs_macro_function_evaluate(
 *                                             function_index, thread_datum,
 * init) TEST EAX,EAX / JZ end XOR EDX,EDX / MOV DL,byte ptr [EAX] zero-extended
 * BYTE load at result+0 PUSH EDX / CALL 0x000ba6d0           ->
 * player_input_enable(*(char *)result) PUSH 0x0 / PUSH ESI / CALL 0x000cbf80
 *                                        -> hs_return(thread_datum, 0)
 *   ADD ESP,0xc                          one cleanup covers BOTH calls (4 + 8)
 *   POP ESI / POP EBP / RET              plain RET (caller cleans) => __cdecl
 *
 * The load at +0x0 is a narrow byte read, not an int32 — result is int*, so
 * the value must be taken through a char* cast.
 *
 * Callees (all cdecl, no register args, all ported):
 *   0xcc560 = hs_macro_function_evaluate(function_index, thread_datum, init)
 *   0xba6d0 = player_input_enable(bool)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c1950(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    player_input_enable(*(char *)result);
    hs_return(thread_datum, 0);
  }
}

/* 0xc1990 — HS script function handler: set the scripted-camera-control flag
 * and echo the flag back to the calling script thread.
 *
 * Evaluates the macro arguments; on success the result block holds a single
 * boolean byte at +0x0.  That byte is handed to
 * scripted_player_control_set_camera_control() and is ALSO the value returned
 * to the HS thread (unlike the neighbouring handlers, which return 0).
 *
 * Disassembly notes (0xc1990, 20 instructions):
 *   PUSH EBP / MOV EBP,ESP / PUSH ECX      PUSH ECX => exactly ONE dword local
 *   PUSH ESI                               ESI holds thread_datum throughout
 *   MOV dword ptr [EBP-4],0                the local is pre-zeroed as a dword
 *   PUSH EAX([EBP+0x10]) / PUSH ESI([EBP+0xc]) / PUSH ECX([EBP+0x8])
 *   CALL 0x000cc560 / ADD ESP,0xc          cdecl: first PUSH is the LAST arg
 *                                          -> hs_macro_function_evaluate(
 *                                               function_index, thread_datum,
 *                                               init)
 *   TEST EAX,EAX / JZ end                  EAX is a result-record POINTER
 *   XOR EDX,EDX / MOV DL,byte ptr [EAX]    zero-extended BYTE load at result+0
 *   PUSH EDX / CALL 0x000b6430             -> set_camera_control(*result)
 *   MOV byte ptr [EBP-4],AL                only the LOW BYTE of the local is
 *                                          written; the pre-zero supplies the
 *                                          upper three bytes
 *   PUSH dword ptr [EBP-4] / PUSH ESI
 *   CALL 0x000cbf80                        -> hs_return(thread_datum, value)
 *   ADD ESP,0xc                            ONE cleanup covers the leftover
 *                                          PUSH EDX of the 0xb6430 call (4)
 *                                          plus hs_return's two args (8).
 *                                          A call-site audit reading this as
 *                                          "hs_return takes 3 args" is a false
 *                                          positive.
 *   POP ESI / MOV ESP,EBP / POP EBP / RET  plain RET (caller cleans) => cdecl
 *
 * The AL consumed by `MOV byte ptr [EBP-4],AL` is NOT garbage and NOT a real
 * return value: 0xb6430's first instruction is `MOV AL,[EBP+8]`, so it leaves
 * its own argument byte in AL by accident of codegen.  The stored byte is
 * therefore provably *(unsigned char *)result.  The callee stays void-declared
 * here; the byte is re-loaded from `result` instead of read out of AL.
 *
 * The `*(char *)&value` store reproduces the original's pre-zeroed-dword /
 * narrow byte-store pair: only [EBP-4]'s low byte is written, and the earlier
 * `value = 0` supplies the upper three bytes that the later dword PUSH reads.
 * Re-loading the byte rather than taking it out of AL
 * costs one extra byte load and keeps `result` live across the 0xb6430 call
 * (so VC71 parks it in a callee-saved register).  Passing the byte straight out
 * of the dereference — rather than through the local — is what reproduces the
 * reference's `XOR EDX,EDX / MOV DL,(reg) / PUSH EDX` argument sequence; it is
 * the same idiom the 100%-matching sibling at 0xc1950 uses.
 *
 * Callees (all cdecl, no register args, all ported):
 *   0xcc560 = hs_macro_function_evaluate(function_index, thread_datum, init)
 *               declared int in kb.json but used as a record pointer
 *   0xb6430 = scripted_player_control_set_camera_control(bool)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c1990(int16_t function_index, int thread_datum, char init)
{
  int *result;
  int value;

  value = 0;
  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != (int *)0) {
    scripted_player_control_set_camera_control(*(char *)result);
    *(char *)&value = *(char *)result;
    hs_return(thread_datum, value);
  }
}

/* 0xc19e0 — HS script function handler: clears the recorded player control
 * action-test state, then returns 0 to the calling script thread (a
 * void-returning script builtin).
 *
 * Callees (both cdecl, no register args, both ported):
 *   0xb6a90 = player_control_action_test_reset(void)
 *   0xcbf80 = hs_return(thread_handle, value)
 *
 * ABI (verified against disassembly 0xc19e0-0xc19f7): cdecl, plain RET, frame
 * is PUSH EBP / MOV EBP,ESP with no `sub esp` (zero locals).  The body reads
 * only [EBP+0xc] = thread_datum (arg 2); function_index and init complete the
 * standard hs-evaluator signature shared by the sibling handlers but are
 * unused here.  There is no hs_macro_function_evaluate call, so this is the
 * no-argument variant (same shape as 0xc0cb0). */
void FUN_000c19e0(int16_t function_index, int thread_datum, char init)
{
  player_control_action_test_reset();
  hs_return(thread_datum, 0);
}

/* 0xc1a00 — HS script function handler: reports whether the recorded player
 * control jump action fired, returning the boolean to the calling script
 * thread.
 *
 * Callees (both cdecl, no register args, both ported):
 *   0xb6b10 = player_control_action_test_jump(void) -> bool in AL
 *   0xcbf80 = hs_return(thread_handle, value)
 *
 * ABI (verified against disassembly 0xc1a00-0xc1a26): cdecl, plain RET, frame
 * is PUSH EBP / MOV EBP,ESP / PUSH ECX (one 4-byte local at EBP-0x4).  The
 * body reads only [EBP+0xc] = thread_datum (arg 2); function_index and init
 * complete the standard hs-evaluator signature shared by the sibling handlers
 * but are unused here.
 *
 * The result local is zero-initialised as a full dword (MOV dword [EBP-4],0)
 * and then only its low byte is overwritten with AL (MOV byte [EBP-4],AL)
 * before the whole dword is re-read and pushed.  The `*(char *)&value` store
 * reproduces that pair; a direct call-in-argument or a (unsigned char) widen
 * would emit MOVZX instead.  This is the same idiom as the sibling at
 * 0xc1990. */
void FUN_000c1a00(int16_t function_index, int thread_datum, char init)
{
  int value;

  value = 0;
  *(char *)&value = (char)player_control_action_test_jump();
  hs_return(thread_datum, value);
}

/* 0xc1a30 — HS script function handler: reports whether the recorded player
 * control primary-trigger (fire) action fired, returning the boolean to the
 * calling script thread.
 *
 * Callees (both cdecl, no register args, both ported):
 *   0xb6b20 = player_control_action_test_primary_trigger(void) -> bool in AL
 *   0xcbf80 = hs_return(thread_handle, value)
 *
 * ABI (verified against disassembly 0xc1a30-0xc1a56, 15 instructions): cdecl,
 * plain RET, frame is PUSH EBP / MOV EBP,ESP / PUSH ECX (one 4-byte local at
 * EBP-0x4).  The body reads only [EBP+0xc] = thread_datum (arg 2);
 * function_index and init complete the standard hs-evaluator signature shared
 * by the sibling handlers but are unused here.
 *
 * The result local is zero-initialised as a full dword (MOV dword [EBP-4],0)
 * and then only its low byte is overwritten with AL (MOV byte [EBP-4],AL)
 * before the whole dword is re-read and pushed (MOV EAX,[EBP-4] / PUSH EAX).
 * The `*(char *)&value` store reproduces that pair; a direct
 * call-in-argument or a (unsigned char) widen would emit MOVZX instead.  This
 * is a BYTE store, unlike the WORD-store siblings at 0xc18d0/0xc1900.  Same
 * idiom as the adjacent handler at 0xc1a00. */
void FUN_000c1a30(int16_t function_index, int thread_datum, char init)
{
  int value;

  value = 0;
  *(char *)&value = (char)player_control_action_test_primary_trigger();
  hs_return(thread_datum, value);
}

/* 0xc1a60 — HS script function handler: reports whether the recorded player
 * control grenade-trigger action fired, returning the boolean to the calling
 * script thread.
 *
 * Callees (both cdecl, no register args, both ported):
 *   0xb6b30 = player_control_action_test_grenade_trigger(void) -> bool in AL
 *   0xcbf80 = hs_return(thread_handle, value)
 *
 * ABI (verified against disassembly 0xc1a60-0xc1a86, 15 instructions): cdecl,
 * plain RET, frame is PUSH EBP / MOV EBP,ESP / PUSH ECX (one 4-byte local at
 * EBP-0x4; no SUB ESP).  The body reads only [EBP+0xc] = thread_datum
 * (arg 2); function_index and init complete the standard hs-evaluator
 * signature shared by the sibling handlers but are unused here.
 *
 * The result local is zero-initialised as a full dword (MOV dword [EBP-4],0)
 * and then only its low byte is overwritten with AL (MOV byte [EBP-4],AL)
 * before the whole dword is re-read and pushed (MOV EAX,[EBP-4] / PUSH EAX).
 * The `*(char *)&value` store reproduces that pair; a direct
 * call-in-argument or a (unsigned char) widen would emit MOVZX instead.
 * Identical idiom to the adjacent handlers at 0xc1a00/0xc1a30. */
void FUN_000c1a60(int16_t function_index, int thread_datum, char init)
{
  int value;

  value = 0;
  *(char *)&value = (char)player_control_action_test_grenade_trigger();
  hs_return(thread_datum, value);
}

/* 0xc1a90 — HS script function handler: reports whether the recorded player
 * control zoom action fired, returning the boolean to the calling script
 * thread.
 *
 * Callees (both cdecl, no register args, both ported):
 *   0xb6b40 = player_control_action_test_zoom(void) -> bool in AL
 *   0xcbf80 = hs_return(thread_handle, value)
 *
 * ABI (verified against disassembly 0xc1a90-0xc1ab6, 15 instructions): cdecl,
 * plain RET, frame is PUSH EBP / MOV EBP,ESP / PUSH ECX (one 4-byte local at
 * EBP-0x4).  The body reads only [EBP+0xc] = thread_datum (arg 2);
 * function_index and init complete the standard hs-evaluator signature shared
 * by the sibling handlers but are unused here.
 *
 * The result local is zero-initialised as a full dword (MOV dword [EBP-4],0)
 * and then only its low byte is overwritten with AL (MOV byte [EBP-4],AL)
 * before the whole dword is re-read and pushed (MOV EAX,[EBP-4] / PUSH EAX).
 * The `*(char *)&value` store reproduces that pair; a direct
 * call-in-argument or a (unsigned char) widen would emit MOVZX instead.
 * Identical idiom to the adjacent handlers at 0xc1a00/0xc1a30/0xc1a60. */
void FUN_000c1a90(int16_t function_index, int thread_datum, char init)
{
  int value;

  value = 0;
  *(char *)&value = (char)player_control_action_test_zoom();
  hs_return(thread_datum, value);
}

/* 0xc1ac0 — HS script function handler: reports whether the recorded player
 * control "action" (use/interact) action fired, returning the boolean to the
 * calling script thread.
 *
 * Callees (both cdecl, no register args, both ported):
 *   0xb6af0 = player_control_action_test_action(void) -> bool in AL
 *   0xcbf80 = hs_return(thread_handle, value)
 *
 * ABI (verified against disassembly 0xc1ac0-0xc1ae6, 15 instructions): cdecl,
 * plain RET, frame is PUSH EBP / MOV EBP,ESP / PUSH ECX (one 4-byte local at
 * EBP-0x4; no SUB ESP).  The body reads only [EBP+0xc] = thread_datum
 * (arg 2); function_index and init complete the standard hs-evaluator
 * signature shared by the sibling handlers but are unused here.  The Ghidra
 * `void FUN_000c1ac0(void)` prototype is wrong — the frame reads [EBP+0xc],
 * which Ghidra surfaces as `in_stack_00000008`.
 *
 * The result local is zero-initialised as a full dword (MOV dword [EBP-4],0)
 * and then only its low byte is overwritten with AL (MOV byte [EBP-4],AL)
 * before the whole dword is re-read and pushed (MOV EAX,[EBP-4] / PUSH EAX).
 * The `*(char *)&value` store reproduces that pair; a direct
 * call-in-argument or a (unsigned char) widen would emit MOVZX instead.
 * Push order at 0xc1ad9 (PUSH EAX ; PUSH ECX, ECX = [EBP+0xc]) confirms
 * thread_datum is hs_return's first argument.  Identical idiom to the
 * adjacent handlers at 0xc1a00/0xc1a30/0xc1a60/0xc1a90. */
void FUN_000c1ac0(int16_t function_index, int thread_datum, char init)
{
  int value;

  value = 0;
  *(char *)&value = (char)player_control_action_test_action();
  hs_return(thread_datum, value);
}

/* 0xc1af0 — HS script function handler: reports whether the recorded player
 * control "accept" action fired, returning the boolean to the calling script
 * thread.
 *
 * Callees (both cdecl, no register args, both ported):
 *   0xb6ab0 = player_control_action_test_accept(void) -> bool in AL
 *   0xcbf80 = hs_return(thread_handle, value)
 *
 * ABI (verified against disassembly 0xc1af0-0xc1b16, 15 instructions): cdecl,
 * plain RET, frame is PUSH EBP / MOV EBP,ESP / PUSH ECX (one 4-byte local at
 * EBP-0x4; no SUB ESP).  The body reads only [EBP+0xc] = thread_datum
 * (arg 2); function_index and init complete the standard hs-evaluator
 * signature shared by the sibling handlers but are unused here.  The Ghidra
 * `void FUN_000c1af0(void)` prototype is wrong — the frame reads [EBP+0xc],
 * which Ghidra surfaces as `in_stack_00000008`.
 *
 * The result local is zero-initialised as a full dword (MOV dword [EBP-4],0)
 * and then only its low byte is overwritten with AL (MOV byte [EBP-4],AL)
 * before the whole dword is re-read and pushed (MOV EAX,[EBP-4] / PUSH EAX).
 * The `*(char *)&value` store reproduces that pair; a direct
 * call-in-argument or a (unsigned char) widen would emit MOVZX instead.
 * Push order at 0xc1b09 (PUSH EAX ; PUSH ECX, ECX = [EBP+0xc]) confirms
 * thread_datum is hs_return's first argument.  Identical idiom to the
 * adjacent handlers at 0xc1a00/0xc1a30/0xc1a60/0xc1a90/0xc1ac0. */
void FUN_000c1af0(int16_t function_index, int thread_datum, char init)
{
  int value;

  value = 0;
  *(char *)&value = (char)player_control_action_test_accept();
  hs_return(thread_datum, value);
}

/* 0xc1b20 — HS script function handler: reports whether the recorded player
 * control "back" action fired, returning the boolean to the calling script
 * thread.
 *
 * Callees (both cdecl, no register args, both ported):
 *   0xb6ad0 = player_control_action_test_back(void) -> bool in AL
 *   0xcbf80 = hs_return(thread_handle, value)
 *
 * ABI (verified against disassembly 0xc1b20-0xc1b46, 15 instructions): cdecl,
 * plain RET, frame is PUSH EBP / MOV EBP,ESP / PUSH ECX (one 4-byte local at
 * EBP-0x4; no SUB ESP).  The body reads only [EBP+0xc] = thread_datum
 * (arg 2); function_index and init complete the standard hs-evaluator
 * signature shared by the sibling handlers but are unused here.  The Ghidra
 * `void FUN_000c1b20(void)` prototype is wrong — the frame reads [EBP+0xc],
 * which Ghidra surfaces as `in_stack_00000008`.  No ADD ESP follows the
 * 0xb6ad0 CALL, confirming player_control_action_test_back takes no args.
 *
 * The result local is zero-initialised as a full dword (MOV dword [EBP-4],0
 * at 0xc1b24) and then only its low byte is overwritten with AL (MOV byte
 * [EBP-4],AL at 0xc1b33) before the whole dword is re-read and pushed (MOV
 * EAX,[EBP-4] / PUSH EAX).  The `*(char *)&value` store reproduces that pair;
 * a direct call-in-argument or a (unsigned char) widen would emit MOVZX
 * instead.  Push order at 0xc1b39 (PUSH EAX ; PUSH ECX, ECX = [EBP+0xc])
 * confirms thread_datum is hs_return's first argument, and the ADD ESP,0x8 at
 * 0xc1b40 confirms hs_return's two cdecl args.  Identical idiom to the
 * adjacent handlers at 0xc1a00/0xc1a30/0xc1a60/0xc1a90/0xc1ac0/0xc1af0. */
void FUN_000c1b20(int16_t function_index, int thread_datum, char init)
{
  int value;

  value = 0;
  *(char *)&value = (char)player_control_action_test_back();
  hs_return(thread_datum, value);
}

/* 0xc1b50 — HS script function handler: reports whether the recorded player
 * control "look relative up" action fired, returning the boolean to the
 * calling script thread.
 *
 * Callees (both cdecl, no register args, both ported):
 *   0xb6bb0 = player_control_action_test_look_relative_up(void) -> bool in AL
 *   0xcbf80 = hs_return(thread_handle, value)
 *
 * ABI (verified against disassembly 0xc1b50-0xc1b76, 15 instructions): cdecl,
 * plain RET, frame is PUSH EBP / MOV EBP,ESP / PUSH ECX (one 4-byte local at
 * EBP-0x4; no SUB ESP).  The body reads only [EBP+0xc] = thread_datum
 * (arg 2); function_index and init complete the standard hs-evaluator
 * signature shared by the sibling handlers but are unused here.  The Ghidra
 * `void FUN_000c1b50(void)` prototype is wrong — the frame reads [EBP+0xc],
 * which Ghidra surfaces as `in_stack_00000008`, and Ghidra's `extraout_AL` is
 * the AL return of the predicate, not a register argument.  No ADD ESP
 * follows the 0xb6bb0 CALL, confirming the predicate takes no args.
 *
 * The result local is zero-initialised as a full dword (MOV dword [EBP-4],0)
 * and then only its low byte is overwritten with AL (MOV byte [EBP-4],AL)
 * before the whole dword is re-read and pushed (MOV EAX,[EBP-4] / PUSH EAX).
 * The `*(char *)&value` store reproduces that pair; a direct
 * call-in-argument or a (unsigned char) widen would emit MOVZX instead.
 * Push order (PUSH EAX ; PUSH ECX, ECX = [EBP+0xc]) confirms thread_datum is
 * hs_return's first argument, and the trailing ADD ESP,0x8 confirms
 * hs_return's two cdecl args.  Identical idiom to the adjacent handlers at
 * 0xc1a00/0xc1a30/0xc1a60/0xc1a90/0xc1ac0/0xc1af0/0xc1b20. */
void FUN_000c1b50(int16_t function_index, int thread_datum, char init)
{
  int value;

  value = 0;
  *(char *)&value = (char)player_control_action_test_look_relative_up();
  hs_return(thread_datum, value);
}

/* 0xc1b80 — HS script function handler: reports whether the recorded player
 * control "look relative down" action fired, returning the boolean to the
 * calling script thread.
 *
 * Callees (both cdecl, no register args, both ported):
 *   0xb6bc0 = player_control_action_test_look_relative_down(void) -> bool in AL
 *   0xcbf80 = hs_return(thread_handle, value)
 *
 * ABI (verified against disassembly 0xc1b80-0xc1ba6, 15 instructions): cdecl,
 * plain RET, frame is PUSH EBP / MOV EBP,ESP / PUSH ECX (one 4-byte local at
 * EBP-0x4; no SUB ESP).  The body reads only [EBP+0xc] = thread_datum
 * (arg 2); function_index and init complete the standard hs-evaluator
 * signature shared by the sibling handlers but are unused here.  The Ghidra
 * `void FUN_000c1b80(void)` prototype is wrong — the frame reads [EBP+0xc],
 * which Ghidra surfaces as `in_stack_00000008`, and Ghidra's `extraout_AL` is
 * the AL return of the predicate, not a register argument.  No ADD ESP
 * follows the 0xb6bc0 CALL, confirming the predicate takes no args.
 *
 * The result local is zero-initialised as a full dword (MOV dword [EBP-4],0)
 * and then only its low byte is overwritten with AL (MOV byte [EBP-4],AL)
 * before the whole dword is re-read and pushed (MOV EAX,[EBP-4] / PUSH EAX).
 * The `*(char *)&value` store reproduces that pair; a direct
 * call-in-argument or a (unsigned char) widen would emit MOVZX instead.
 * Push order (PUSH EAX ; PUSH ECX, ECX = [EBP+0xc]) confirms thread_datum is
 * hs_return's first argument, and the trailing ADD ESP,0x8 confirms
 * hs_return's two cdecl args.  Identical idiom to the adjacent handlers at
 * 0xc1a00/0xc1a30/0xc1a60/0xc1a90/0xc1ac0/0xc1af0/0xc1b20/0xc1b50, completing
 * the look-relative direction quartet (left/right/up/down). */
void FUN_000c1b80(int16_t function_index, int thread_datum, char init)
{
  int value;

  value = 0;
  *(char *)&value = (char)player_control_action_test_look_relative_down();
  hs_return(thread_datum, value);
}

/* 0xc1bb0 - HaloScript evaluator for the "look relative left" control test.
 * Calls the zero-argument predicate and commits its bool result to the
 * calling script thread.
 *
 *   0xb6b90 = player_control_action_test_look_relative_left(void) -> bool in AL
 *   0xcbf80 = hs_return(thread_datum, value)
 *
 * ABI (verified against disassembly 0xc1bb0-0xc1bd6, 15 instructions): cdecl,
 * plain RET, frame is PUSH EBP / MOV EBP,ESP / PUSH ECX (one 4-byte local at
 * EBP-0x4; no SUB ESP).  The body reads only [EBP+0xc] = thread_datum
 * (arg 2); function_index and init complete the standard hs-evaluator
 * signature shared by the sibling handlers but are unused here.  The Ghidra
 * `void FUN_000c1bb0(void)` prototype is wrong — the frame reads [EBP+0xc],
 * which Ghidra surfaces as `in_stack_00000008`, and Ghidra's `extraout_AL` is
 * the AL return of the predicate, not a register argument.  No ADD ESP
 * follows the 0xb6b90 CALL, confirming the predicate takes no args.
 *
 * The result local is zero-initialised as a full dword (MOV dword [EBP-4],0)
 * and then only its low byte is overwritten with AL (MOV byte [EBP-4],AL)
 * before the whole dword is re-read and pushed (MOV EAX,[EBP-4] / PUSH EAX).
 * The `*(char *)&value` store reproduces that pair; a direct
 * call-in-argument or a (unsigned char) widen would emit MOVZX instead.
 * Push order (PUSH EAX ; PUSH ECX, ECX = [EBP+0xc]) confirms thread_datum is
 * hs_return's first argument, and the trailing ADD ESP,0x8 confirms
 * hs_return's two cdecl args.  Unlike most hs evaluators this one has no
 * hs_macro_function_evaluate call and no NULL-result check — the body
 * contains exactly two CALLs.  Identical idiom to the adjacent handlers at
 * 0xc1a00 through 0xc1b80. */
void FUN_000c1bb0(int16_t function_index, int thread_datum, char init)
{
  int value;

  value = 0;
  *(char *)&value = (char)player_control_action_test_look_relative_left();
  hs_return(thread_datum, value);
}

/* 0xc1be0 - HaloScript evaluator for the "look relative right" control test.
 * Direct twin of 0xc1bb0 (left); differs only in the predicate callee.
 * Calls the zero-argument predicate and commits its bool result to the
 * calling script thread.
 *
 *   0xb6ba0 = player_control_action_test_look_relative_right(void) -> bool in
 * AL 0xcbf80 = hs_return(thread_datum, value)
 *
 * ABI (verified against disassembly 0xc1be0-0xc1c06, 15 instructions): cdecl,
 * plain RET, frame is PUSH EBP / MOV EBP,ESP / PUSH ECX (one 4-byte local at
 * EBP-0x4; no SUB ESP).  The body reads only [EBP+0xc] = thread_datum
 * (arg 2); function_index and init complete the standard hs-evaluator
 * signature shared by the sibling handlers but are unused here.  The Ghidra
 * `void FUN_000c1be0(void)` prototype is wrong — the frame reads [EBP+0xc],
 * which Ghidra surfaces as `in_stack_00000008`, and Ghidra's `extraout_AL` is
 * the AL return of the predicate, not a register argument.  No ADD ESP
 * follows the 0xb6ba0 CALL, confirming the predicate takes no args.
 *
 * The result local is zero-initialised as a full dword (MOV dword [EBP-4],0)
 * and then only its low byte is overwritten with AL (MOV byte [EBP-4],AL)
 * before the whole dword is re-read and pushed (MOV EAX,[EBP-4] / PUSH EAX).
 * The `*(char *)&value` store reproduces that pair; a direct
 * call-in-argument or a (unsigned char) widen would emit MOVZX instead.
 * Push order (PUSH EAX ; PUSH ECX, ECX = [EBP+0xc]) confirms thread_datum is
 * hs_return's first argument, and the trailing ADD ESP,0x8 confirms
 * hs_return's two cdecl args.  Like its 0xc1bb0 twin this handler has no
 * hs_macro_function_evaluate call and no NULL-result check — the body
 * contains exactly two CALLs.  Completes the look-relative direction quartet
 * (left/right/up/down) at 0xc1a00 through 0xc1be0. */
void FUN_000c1be0(int16_t function_index, int thread_datum, char init)
{
  int value;

  value = 0;
  *(char *)&value = (char)player_control_action_test_look_relative_right();
  hs_return(thread_datum, value);
}

/* 0xc1c10 — HS script function handler: reports whether the recorded player
 * control produced look movement in any relative direction, returning the
 * boolean to the calling script thread.
 *
 * Callees (both cdecl, no register args, both ported):
 *   0xb6b70 = player_control_action_test_look_relative_all_directions(void)
 *             -> bool in AL
 *   0xcbf80 = hs_return(thread_handle, value)
 *
 * ABI (verified against disassembly 0xc1c10-0xc1c36, 15 instructions): cdecl,
 * plain RET, frame is PUSH EBP / MOV EBP,ESP / PUSH ECX (one 4-byte local at
 * EBP-0x4; no SUB ESP).  The body reads only [EBP+0xc] = thread_datum
 * (arg 2); function_index and init complete the standard hs-evaluator
 * signature shared by the sibling handlers but are unused here.  The Ghidra
 * `void FUN_000c1c10(void)` prototype is wrong — the frame reads [EBP+0xc],
 * which Ghidra surfaces as `in_stack_00000008`, and Ghidra's `extraout_AL` is
 * the AL return of the predicate, not a register argument.  No ADD ESP
 * follows the 0xb6b70 CALL, confirming the predicate takes no args.
 *
 * The result local is zero-initialised as a full dword (MOV dword [EBP-4],0)
 * and then only its low byte is overwritten with AL (MOV byte [EBP-4],AL)
 * before the whole dword is re-read and pushed (MOV EAX,[EBP-4] / PUSH EAX).
 * The `*(char *)&value` store reproduces that pair; a direct
 * call-in-argument or a (unsigned char) widen would emit MOVZX instead.  This
 * is a BYTE store, unlike the WORD-store siblings at 0xc18d0/0xc1900.  The
 * original loads ECX = [EBP+0xc] between the CALL and the AL store (MSVC
 * scheduling); C cannot express that ordering and it has no semantic effect.
 * Push order (PUSH EAX ; PUSH ECX, ECX = [EBP+0xc]) confirms thread_datum is
 * hs_return's first argument, and the trailing ADD ESP,0x8 confirms
 * hs_return's two cdecl args.  Like the 0xc1bb0/0xc1be0 pair this handler has
 * no hs_macro_function_evaluate call and no NULL-result check — the body
 * contains exactly two CALLs.  Follows the look-relative direction quartet at
 * 0xc1a00 through 0xc1be0 with the any-direction aggregate. */
void FUN_000c1c10(int16_t function_index, int thread_datum, char init)
{
  int value;

  value = 0;
  *(char *)&value =
    (char)player_control_action_test_look_relative_all_directions();
  hs_return(thread_datum, value);
}

/* 0xc1c40 — HS script-function handler: query whether the local player is
 * applying movement input in any direction, and commit the boolean to the
 * calling script thread.  Structural twin of 0xc1c10 (look-relative
 * aggregate); the two differ only in the predicate callee — this one drives
 * the move-relative aggregate at 0xb6b50.
 *
 * ABI (verified against disassembly 0xc1c40-0xc1c66, 15 instructions): cdecl,
 * plain RET, one 4-byte local (PUSH ECX).  The body reads only [EBP+0xc] =
 * thread_datum (the SECOND stack argument); function_index and init complete
 * the standard hs-evaluator signature (same shape as 0xc0cb0/0xc1c10) but are
 * unused here.  Ghidra reports the thread handle as `in_stack_00000008`
 * (argument 1) — that is wrong, the load is [EBP+0xc].  Ghidra's
 * `extraout_AL` is the AL return of the predicate, not a register argument;
 * no ADD ESP follows the 0xb6b50 CALL, confirming the predicate takes no
 * args.
 *
 * The result local is zero-initialised as a full dword (MOV dword [EBP-4],0)
 * and then only its low byte is overwritten with AL (MOV byte [EBP-4],AL)
 * before the whole dword is re-read and pushed (MOV EAX,[EBP-4] / PUSH EAX).
 * The `*(char *)&value` store reproduces that pair; a direct
 * call-in-argument or a (unsigned char) widen would emit MOVZX instead.  The
 * original loads ECX = [EBP+0xc] between the CALL and the AL store (MSVC
 * scheduling); C cannot express that ordering and it has no semantic effect.
 * Push order (PUSH EAX ; PUSH ECX, ECX = [EBP+0xc]) confirms thread_datum is
 * hs_return's first argument, and the trailing ADD ESP,0x8 confirms
 * hs_return's two cdecl args.  Like 0xc1bb0/0xc1be0/0xc1c10 this handler has
 * no hs_macro_function_evaluate call and no NULL-result check — the body
 * contains exactly two CALLs. */
void FUN_000c1c40(int16_t function_index, int thread_datum, char init)
{
  int value;

  value = 0;
  *(char *)&value =
    (char)player_control_action_test_move_relative_all_directions();
  hs_return(thread_datum, value);
}

/* 0xc1c70 — HS script-function handler for `player_add_equipment`.  Evaluates
 * the call's argument expressions via hs_macro_function_evaluate; when a
 * non-NULL argument record comes back, unpacks the three packed arguments and
 * invokes player_add_equipment, then commits a 0 return to the calling thread.
 * Same template as 0xc0ed0/0xc0f10 — the only differences are the unpacked
 * field widths and the dispatch target.
 *
 * ABI (verified against disassembly): cdecl, PUSH EBP / MOV EBP,ESP / PUSH
 * ESI.  ESI caches thread_datum ([EBP+0xc]) across the first CALL; that is the
 * only callee-saved register used.  Ghidra reports the parameters as
 * `in_stack_00000004/8/c` because the stale kb declaration was `void (void)` —
 * the real slots are [EBP+0x8] = function_index (int16), [EBP+0xc] =
 * thread_datum, [EBP+0x10] = init (char).
 *
 * hs_macro_function_evaluate is declared `int` in kb.json but returns a
 * pointer to the evaluated-argument record; TEST EAX,EAX / JZ is a NULL check,
 * not a boolean test.  Cast at the call site exactly as the siblings do.
 *
 * Ghidra dropped player_add_equipment's arguments entirely (they are loaded
 * off the returned record).  The disassembly shows all three, and the load
 * widths are explicit — do NOT widen them to int:
 *   MOV EDX,[EAX]                  -> unit_handle     (dword, record +0x0)
 *   XOR ECX,ECX ; MOV CX,[EAX+0x4] -> equipment_index (WORD,  record +0x4)
 *   XOR EDX,EDX ; MOV DL,[EAX+0x8] -> reset_flag      (BYTE,  record +0x8)
 * The zero-extending XOR pairs are how MSVC materialises the sub-dword loads
 * for the cdecl push slots; they are not separate arguments.
 *
 * Cleanup for both calls is coalesced into a single ADD ESP,0x14 (0xc + 0x8)
 * after the hs_return CALL — ordinary MSVC codegen, not a stack imbalance. */
void FUN_000c1c70(int16_t function_index, int thread_datum, char init)
{
  int *args;

  args = (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (args != (int *)0) {
    player_add_equipment(args[0], *(int16_t *)(args + 1), *(char *)(args + 2));
    hs_return(thread_datum, 0);
  }
  return;
}

/* FUN_000c1cb0 @ 0x000c1cb0 — HaloScript builtin handler: evaluate the script
 * function's arguments and, on a non-NULL evaluation record, teleport a
 * player, then commit a 0 result to the calling HS thread.
 *
 * ABI (verified against the disassembly at 0xc1cb0): cdecl, plain RET.  The
 * frame is PUSH EBP; MOV EBP,ESP; PUSH ESI — no locals and no SUB ESP; ESI is
 * the only callee-saved register used and caches thread_datum so it can be
 * reused as hs_return's first argument.  Ghidra's `void FUN_000c1cb0(void)`
 * prototype came from the stale kb declaration, which is why it reported the
 * parameters as in_stack_00000004/8/c; the real slots are [EBP+0x8] =
 * function_index (int16, arrives in ECX), [EBP+0xc] = thread_datum,
 * [EBP+0x10] = init (char, arrives in EAX).  These are ordinary stack
 * parameters, not register arguments.
 *
 * hs_macro_function_evaluate is declared `int` in kb.json but returns a
 * pointer to the evaluated-argument record; TEST EAX,EAX / JZ is a NULL check,
 * not a boolean test.  Cast at the call site exactly as the siblings do.
 *
 * Ghidra dropped debug_player_teleport's arguments entirely (it modelled the
 * callee as no-arg).  The disassembly loads both off the returned record, with
 * deliberately different extension widths — do NOT type them the same:
 *   MOVSX EAX,word ptr [EAX]        -> arg1, SIGN-extended int16 at record +0
 *   XOR EDX,EDX ; MOV DX,[EAX+0x4]  -> arg2, ZERO-extended uint16 at record +4
 * Push order is PUSH EDX then PUSH EAX, so the +0 field is the first argument.
 *
 * Cleanup for the teleport and hs_return calls is coalesced into a single
 * ADD ESP,0x10 (0x8 + 0x8) — ordinary MSVC adjacent-call codegen, not a stack
 * imbalance, and the reason the ARG_COUNT enrichment hazard reported against
 * hs_return (cleanup=4 vs decl=2) is a false positive. */
void FUN_000c1cb0(int16_t function_index, int thread_datum, char init)
{
  char *args;

  args = (char *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (args != (char *)0) {
    debug_player_teleport((int)*(int16_t *)args, (int)*(uint16_t *)(args + 4));
    hs_return(thread_datum, 0);
  }
  return;
}

/* 0xc1cf0 — HaloScript function evaluator that resets the currently loaded
 * map.  Runs main_reset_map (0x1002a0, no arguments) for its side effect, then
 * commits a 0 result to the calling script thread (a void-returning script
 * builtin).  Same shape as FUN_000c0cb0.
 *
 * ABI (verified against disassembly 0xc1cf0-0xc1d07): cdecl, plain RET, caller
 * cleans (ADD ESP,0x8 after the hs_return call).  The body reads only
 * [EBP+0xc] = thread_datum (argument 2); function_index and init complete the
 * standard hs-evaluator signature but are never read here.  Ghidra's
 * `in_stack_00000008` is a phantom of its wrong (void) prototype — it is
 * [EBP+0xc], i.e. argument 2, not argument 1.
 *
 * Push order at 0xc1cfb is PUSH 0x0 then PUSH EAX, so under cdecl (first PUSH
 * is the last C argument) the call is hs_return(thread_datum, 0). */
void FUN_000c1cf0(int16_t function_index, int thread_datum, char init)
{
  main_reset_map();
  hs_return(thread_datum, 0);
  return;
}

/* 0xc1d10 — HaloScript function evaluator that sets the pending map name.
 * Evaluates the script macro function's argument list via
 * hs_macro_function_evaluate; when that returns a non-NULL result record, the
 * record's FIRST DWORD (a const char* string pointer) is forwarded to
 * main_set_map_name, then a 0 result is committed to the calling thread.
 *
 * Callees (hardcoded addresses, all ported):
 *   0xcc560 = hs_macro_function_evaluate(int16 function_index,
 *                                        int thread_datum, char init)
 *               -> result record* or NULL
 *   0xfffa0 = main_set_map_name(const char *name) -> void
 *   0xcbf80 = hs_return(int thread_datum, int value) -> void
 *
 * ABI (verified against disassembly 0xc1d10-0xc1d40): cdecl, frame is
 * PUSH EBP; MOV EBP,ESP; PUSH ESI with no locals and no `sub esp`.  ESI holds
 * thread_datum ([EBP+0xc]) across all three calls.  Ghidra's
 * `void FUN_000c1d10(void)` prototype is wrong — the three `in_stack_*`
 * phantoms are [EBP+8]/[EBP+0xc]/[EBP+0x10], i.e. the standard hs-evaluator
 * argument triple.
 *
 * Ghidra also printed `FUN_000fffa0()` with no argument; the disassembly at
 * 0xc1d2f is `MOV EDX,[EAX]; PUSH EDX`, so the record is dereferenced and the
 * resulting pointer is the map-name string.  The single `ADD ESP,0xc` at
 * 0xc1d3c is MSVC's merged cleanup for BOTH the 1-argument main_set_map_name
 * call and the 2-argument hs_return call (1 + 2 = 3 dwords) — it is not
 * evidence of a 3-argument hs_return.
 *
 * hs_macro_function_evaluate is declared returning `int` in kb.json but is
 * used here as a pointer, so it is cast (same as FUN_000c0ed0/FUN_000c0f10). */
void FUN_000c1d10(int16_t function_index, int thread_datum, char init)
{
  char **result;

  result =
    (char **)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != (char **)0) {
    main_set_map_name(*result);
    hs_return(thread_datum, 0);
  }
  return;
}

/* HaloScript builtin: set the multiplayer map name.
 *
 * Identical in shape to FUN_000c1d10 immediately above; the only difference is
 * the callee, 0x100010 main_set_multiplayer_map_name instead of 0xfffa0
 * main_set_map_name.
 *
 * Callees:
 *   0xcc560 = hs_macro_function_evaluate(int16_t, int, char) -> int
 *   0x100010 = main_set_multiplayer_map_name(const char *name) -> void
 *   0xcbf80 = hs_return(int thread_datum, int value) -> void
 *
 * ABI (verified against disassembly 0xc1d50-0xc1d80): cdecl, frame is
 * PUSH EBP; MOV EBP,ESP; PUSH ESI with no locals and no `sub esp`.  ESI is
 * loaded once at 0xc1d5a and holds thread_datum ([EBP+0xc]) across the body.
 * Ghidra's `void FUN_000c1d50(void)` prototype is wrong — the three
 * `in_stack_*` phantoms are [EBP+8]/[EBP+0xc]/[EBP+0x10], i.e. the standard
 * hs-evaluator argument triple.
 *
 * The push order at 0xc1d60 is PUSH EAX([EBP+0x10]); PUSH ESI([EBP+0xc]);
 * PUSH ECX([EBP+8]), so under cdecl the arguments are
 * (function_index, thread_datum, init) — matching the kb declaration.
 *
 * Ghidra printed `FUN_00100010()` with no argument; the disassembly at
 * 0xc1d6f is `MOV EDX,[EAX]; PUSH EDX`, so the record is dereferenced and the
 * resulting pointer is the map-name string, not the record address.
 *
 * The single `ADD ESP,0xc` at 0xc1d7c is MSVC's merged cleanup for BOTH the
 * 1-argument main_set_multiplayer_map_name call and the 2-argument hs_return
 * call (1 + 2 = 3 dwords) — it is not evidence of a 3-argument hs_return, and
 * the call-site audit's ARG_COUNT warning on hs_return here is a false
 * positive from that coalescing.
 *
 * hs_macro_function_evaluate is declared returning `int` in kb.json but is
 * used here as a pointer, so it is cast (same as FUN_000c1d10). */
void FUN_000c1d50(int16_t function_index, int thread_datum, char init)
{
  char **result;

  result =
    (char **)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != (char **)0) {
    main_set_multiplayer_map_name(*result);
    hs_return(thread_datum, 0);
  }
  return;
}

/* 0xc1d90 — HS script function handler: set the campaign difficulty.
 *
 * Standard hs macro-function wrapper.  The prologue pushes only ESI, has no
 * locals and no `sub esp`; [EBP+8]/[EBP+0xc]/[EBP+0x10] are the three cdecl
 * parameters.  The call at 0xc1da0 pushes EAX([EBP+0x10]), ESI([EBP+0xc]),
 * PUSH ECX([EBP+8]), so under cdecl the arguments are
 * (function_index, thread_datum, init) — matching the kb declaration.
 *
 * Ghidra printed `FUN_00100060()` with no argument, but the disassembly at
 * 0xc1dac is `XOR EDX,EDX; MOV DX,word ptr [EAX]; PUSH EDX` — a zero-extended
 * 16-bit load from offset +0x0 of the result block, so the difficulty is a
 * narrow int16 field, not an int.
 *
 * The single `ADD ESP,0xc` at 0xc1dbf is MSVC's merged cleanup for BOTH the
 * 1-argument main_set_difficulty call and the 2-argument hs_return call
 * (1 + 2 = 3 dwords) — it is not evidence of a 3-argument hs_return, and the
 * call-site audit's ARG_COUNT warning on hs_return here is a false positive
 * from that coalescing.
 *
 * hs_macro_function_evaluate is declared returning `int` in kb.json but is
 * used here as a pointer, so it is cast (same as FUN_000c1d50). */
void FUN_000c1d90(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != (int *)0) {
    main_set_difficulty(*(short *)result);
    hs_return(thread_datum, 0);
  }
  return;
}

/* HaloScript builtin: deliberately crash the game.
 *
 * Same wrapper shape as FUN_000c1d10 / FUN_000c1d50 above: evaluate the macro
 * arguments, and on a non-NULL result block dereference its first dword and
 * hand it to the callee, then return void to the calling script thread via
 * hs_return(thread_datum, 0).  The hs_return call is unreachable in practice
 * because main_crash faults first; it is emitted unconditionally by the
 * original all the same, so it is transcribed here.
 *
 * Callees:
 *   0xcc560 = hs_macro_function_evaluate(int16_t, int, char) -> int
 *   0x101cb0 = main_crash(const char *reason) -> void
 *   0xcbf80 = hs_return(int thread_datum, int value) -> void
 *
 * ABI (verified against disassembly 0xc1dd0-0xc1e01): cdecl, plain RET, no
 * FPU.  Frame is PUSH EBP; MOV EBP,ESP; PUSH ESI with no locals and no
 * `sub esp`.  ESI is loaded from [EBP+0xc] before the first call and holds
 * thread_datum across the whole body, which is why the same value feeds both
 * hs_macro_function_evaluate and hs_return.  Ghidra's `void FUN_000c1dd0(void)`
 * prototype comes from the stale kb declaration; the three `in_stack_*`
 * phantoms are [EBP+8]/[EBP+0xc]/[EBP+0x10], the standard hs-evaluator triple.
 *
 * The push order is PUSH EAX([EBP+0x10]); PUSH ESI([EBP+0xc]); PUSH ECX
 * ([EBP+8]), so under cdecl the evaluate call takes (function_index,
 * thread_datum, init) — matching the kb declaration.
 *
 * Ghidra printed `FUN_00101cb0()` with no argument, but the disassembly at
 * 0xc1dea is `MOV EDX,[EAX]; PUSH EDX`, so the result record is dereferenced
 * and the string it points at is passed to main_crash.  main_crash ignores it
 * (its two-instruction body just stores a literal through NULL), but an
 * ignored cdecl parameter is invisible in the callee's codegen — the caller's
 * PUSH is the evidence, and 0xc1def is main_crash's only call site in the XBE.
 * Its kb declaration was corrected from `(void)` to `(const char *)` with this
 * lift.
 *
 * The single `ADD ESP,0xc` at 0xc1dfb is MSVC's merged cleanup for BOTH the
 * 1-argument main_crash call and the 2-argument hs_return call (1 + 2 = 3
 * dwords); it is not evidence of a 3-argument hs_return, and the call-site
 * audit's ARG_COUNT warning on hs_return here is a false positive from that
 * coalescing.
 *
 * hs_macro_function_evaluate is declared returning `int` in kb.json but is
 * used here as a pointer, so it is cast (same as FUN_000c1d50). */
void FUN_000c1dd0(int16_t function_index, int thread_datum, char init)
{
  char **result;

  result =
    (char **)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != (char **)0) {
    main_crash(*result);
    hs_return(thread_datum, 0);
  }
  return;
}

/* 0xc1e10 — HS script function handler: switch the active structure BSP.
 *
 * Standard hs macro-function wrapper, identical in shape to FUN_000c1d90
 * above.  The prologue is PUSH EBP; MOV EBP,ESP; PUSH ESI with no locals and
 * no `sub esp`; [EBP+8]/[EBP+0xc]/[EBP+0x10] are the three cdecl parameters.
 * ESI is loaded from [EBP+0xc] before the first call and holds thread_datum
 * across the whole body, which is why the same value feeds both
 * hs_macro_function_evaluate and hs_return.
 *
 * The push order for the evaluate call is PUSH EAX([EBP+0x10]); PUSH
 * ESI([EBP+0xc]); PUSH ECX([EBP+8]), so under cdecl the arguments are
 * (function_index, thread_datum, init) — matching the kb declaration.
 *
 * Ghidra printed `FUN_0018eb40()` with no argument, but the disassembly is
 * `XOR EDX,EDX; MOV DX,word ptr [EAX]; PUSH EDX` — a zero-extended 16-bit
 * load from offset +0x0 of the result block, so the BSP index is a narrow
 * int16 field, not an int (matching the callee's `__int16` parameter).
 *
 * scenario_switch_structure_bsp returns bool in AL and the original discards
 * it, so the result is not assigned here.
 *
 * The single `ADD ESP,0xc` at 0xc1e3f is MSVC's merged cleanup for BOTH the
 * 1-argument scenario_switch_structure_bsp call and the 2-argument hs_return
 * call (1 + 2 = 3 dwords) — it is not evidence of a 3-argument hs_return, and
 * the call-site audit's ARG_COUNT warning on hs_return here is a false
 * positive from that coalescing.
 *
 * hs_macro_function_evaluate is declared returning `int` in kb.json but is
 * used here as a pointer, so it is cast (same as FUN_000c1d90). */
void FUN_000c1e10(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != (int *)0) {
    scenario_switch_structure_bsp(*(short *)result);
    hs_return(thread_datum, 0);
  }
  return;
}

/* 0xc1e50 — HaloScript script command "structure_bsp_index_get": report the
 * index of the currently active structure BSP back to the calling script
 * thread.
 *
 * Disassembly (15 instructions).  Frame is PUSH EBP; MOV EBP,ESP; PUSH ECX —
 * a single 4-byte local at [EBP-0x4] and no `sub esp`.  Body:
 *
 *   MOV dword ptr [EBP-0x4],0   ; zero the FULL dword first
 *   CALL 0x18f080               ; global_structure_bsp_index_get(), result in
 * AX MOV ECX,[EBP+0xc]           ; thread_datum, loaded before the word store
 *   MOV word ptr [EBP-0x4],AX   ; store only the low 16 bits
 *   MOV EAX,[EBP-0x4]
 *   PUSH EAX                    ; arg2 = value
 *   PUSH ECX                    ; arg1 = thread_datum  (cdecl: last PUSH first)
 *   CALL 0xcbf80                ; hs_return
 *   ADD ESP,0x8
 *
 * The zero-dword-then-store-word sequence means the int16 result is
 * ZERO-extended into the 32-bit HS value slot, not sign-extended: a plain
 * `hs_return(thread_datum, global_structure_bsp_index_get())` would promote
 * the `short` return with MOVSX and is wrong.  Same idiom as FUN_000c1900.
 *
 * [EBP+0x8] (function_index) and [EBP+0x10] (init) are never read by this
 * body; they complete the standard hs-evaluator signature shared by every
 * other handler in this TU.
 *
 * Callees (both cdecl, no register args, both ported):
 *   0x18f080 = global_structure_bsp_index_get(void) — returns int16_t in AX
 *   0xcbf80  = hs_return(thread_handle, value)
 */
void FUN_000c1e50(int16_t function_index, int thread_datum, char init)
{
  int32_t value;

  value = 0;
  *(int16_t *)&value = global_structure_bsp_index_get();
  hs_return(thread_datum, value);
}

/* 0xc1e80 — HaloScript script command handler: print the engine version
 * banner, then complete the calling script thread with the value 0.
 *
 * Disassembly (10 instructions).  Frame is PUSH EBP; MOV EBP,ESP only — no
 * locals and no `sub esp`.  Body:
 *
 *   CALL 0x101cc0        ; main_print_version(), no args, no cleanup
 *   MOV EAX,[EBP+0xc]    ; thread_datum
 *   PUSH 0x0             ; hs_return arg2 = value
 *   PUSH EAX             ; hs_return arg1 = thread_datum (cdecl: last PUSH
 * first) CALL 0xcbf80         ; hs_return ADD ESP,0x8          ; cdecl cleanup,
 * 2 dwords
 *
 * main_print_version leaves whatever it likes in EAX; the original never
 * reads it after the CALL, so the result is discarded here as well.
 *
 * [EBP+0x8] (function_index) and [EBP+0x10] (init) are never read by this
 * body; they complete the standard hs-evaluator signature shared by every
 * other handler in this TU (same situation as FUN_000c1e50).  Ghidra
 * mis-prototypes this as void(void) and reports the [EBP+0xc] read as the
 * phantom local `in_stack_00000008`.
 *
 * Callees (both cdecl, no register args, both ported):
 *   0x101cc0 = main_print_version(void)
 *   0xcbf80  = hs_return(thread_handle, value)
 */
void FUN_000c1e80(int16_t function_index, int thread_datum, char init)
{
  main_print_version();
  hs_return(thread_datum, 0);
}

/* hs script-function handler: switch the game connection over to film
 * playback, then return void to the calling script thread.
 *
 * Body is two calls and nothing else — the frame is a bare
 * PUSH EBP / MOV EBP,ESP with no `sub esp`, so there are no locals at all.
 *
 * 000c1ea3  CALL 0x1006e0        ; main_set_game_connection_to_film_playback()
 * 000c1ea8  MOV  EAX,[EBP+0xc]   ; thread_datum (SECOND stack arg)
 * 000c1eab  PUSH 0x0             ; value  (cdecl: last arg pushed first)
 * 000c1ead  PUSH EAX             ; thread_handle
 * 000c1eae  CALL 0xcbf80         ; hs_return(thread_datum, 0)
 * 000c1eb3  ADD  ESP,0x8         ; cdecl cleanup, 2 dwords
 *
 * main_set_game_connection_to_film_playback takes no arguments and its EAX
 * is never read after the CALL, so the result is discarded here as well.
 *
 * [EBP+0x8] (function_index) and [EBP+0x10] (init) are never read by this
 * body; they complete the standard hs-evaluator signature shared by every
 * other handler in this TU (same situation as FUN_000c1e80 immediately
 * above).  Ghidra mis-prototypes this as void(void) and reports the
 * [EBP+0xc] read as the phantom local `in_stack_00000008`.
 *
 * Callees (both cdecl, no register args, both ported):
 *   0x1006e0 = main_set_game_connection_to_film_playback(void)
 *   0xcbf80  = hs_return(thread_handle, value)
 */
void FUN_000c1ea0(int16_t function_index, int thread_datum, char init)
{
  main_set_game_connection_to_film_playback();
  hs_return(thread_datum, 0);
}

/* 0xc1ec0 — HaloScript script command handler: flush the texture cache, then
 * complete the calling script thread with the value 0.
 *
 * Body is two calls and nothing else — the frame is a bare
 * PUSH EBP / MOV EBP,ESP with no `sub esp`, so there are no locals at all.
 *
 * 000c1ec3  CALL 0x1bed30        ; texture_cache_flush(), no args, no cleanup
 * 000c1ec8  MOV  EAX,[EBP+0xc]   ; thread_datum (SECOND stack arg)
 * 000c1ecb  PUSH 0x0             ; value  (cdecl: last arg pushed first)
 * 000c1ecd  PUSH EAX             ; thread_handle
 * 000c1ece  CALL 0xcbf80         ; hs_return(thread_datum, 0)
 * 000c1ed3  ADD  ESP,0x8         ; cdecl cleanup, 2 dwords
 * 000c1ed7  RET                  ; plain RET => cdecl, caller cleans args
 *
 * texture_cache_flush takes no arguments and its EAX is never read after the
 * CALL, so the result is discarded here as well.
 *
 * [EBP+0x8] (function_index) and [EBP+0x10] (init) are never read by this
 * body; they complete the standard hs-evaluator signature shared by every
 * other handler in this TU (same situation as FUN_000c1ea0 immediately
 * above).  Ghidra mis-prototypes this as void(void) and reports the
 * [EBP+0xc] read as the phantom local `in_stack_00000008`.
 *
 * Callees (both cdecl, no register args):
 *   0x1bed30 = texture_cache_flush(void)   [not yet ported; called via thunk]
 *   0xcbf80  = hs_return(thread_handle, value)
 */
void FUN_000c1ec0(int16_t function_index, int thread_datum, char init)
{
  texture_cache_flush();
  hs_return(thread_datum, 0);
}

/* 0xc1ee0 — HaloScript script command handler: flush the sound cache, then
 * complete the calling script thread with the value 0.
 *
 * Body is two calls and nothing else — the frame is a bare
 * PUSH EBP / MOV EBP,ESP with no `sub esp`, so there are no locals at all.
 * Exact twin of FUN_000c1ec0 immediately above (identical codegen; differs
 * only in the cache-flush callee).
 *
 * 000c1ee3  CALL 0x1be490        ; sound_cache_flush(), no args, no cleanup
 * 000c1ee8  MOV  EAX,[EBP+0xc]   ; thread_datum (SECOND stack arg)
 * 000c1eeb  PUSH 0x0             ; value  (cdecl: last arg pushed first)
 * 000c1eed  PUSH EAX             ; thread_handle
 * 000c1eee  CALL 0xcbf80         ; hs_return(thread_datum, 0)
 * 000c1ef3  ADD  ESP,0x8         ; cdecl cleanup, 2 dwords
 * 000c1ef7  RET                  ; plain RET => cdecl, caller cleans args
 *
 * sound_cache_flush takes no arguments and its EAX is never read after the
 * CALL, so the result is discarded here as well.
 *
 * [EBP+0x8] (function_index) and [EBP+0x10] (init) are never read by this
 * body; they complete the standard hs-evaluator signature shared by every
 * other handler in this TU (same situation as FUN_000c1ec0 immediately
 * above).  Ghidra mis-prototypes this as void(void) and reports the
 * [EBP+0xc] read as the phantom local `in_stack_00000008`.
 *
 * Callees (both cdecl, no register args):
 *   0x1be490 = sound_cache_flush(void)   [not yet ported; called via thunk]
 *   0xcbf80  = hs_return(thread_handle, value)
 */
void FUN_000c1ee0(int16_t function_index, int thread_datum, char init)
{
  sound_cache_flush();
  hs_return(thread_datum, 0);
}

/* 0xc1f00 — HaloScript script-function stub (no-argument, void-result form).
 * Twin of FUN_000c1ee0 immediately above: it takes no script arguments, so it
 * never calls hs_macro_function_evaluate and has no null-check branch.  The
 * body unconditionally invokes the cseries/errors.c helper FUN_0008f1e0 and
 * then commits a zero result to the calling thread via hs_return.
 *
 * [EBP+0x8] (function_index) and [EBP+0x10] (init) are never read by this
 * body; they complete the standard hs-evaluator signature shared by every
 * other handler in this TU.  Ghidra mis-prototypes this as void(void) and
 * reports the [EBP+0xc] read as the phantom local `in_stack_00000008`.
 *
 * Callees (both cdecl, no register args):
 *   0x8f1e0  = FUN_0008f1e0(void)   [errors.c]
 *   0xcbf80  = hs_return(thread_handle, value)
 */
void FUN_000c1f00(int16_t function_index, int thread_datum, char init)
{
  FUN_0008f1e0();
  hs_return(thread_datum, 0);
}

/* 0xc1f20 — HaloScript script-function stub (no-argument, void-result form).
 * Twin of FUN_000c1f00 immediately above: it takes no script arguments, so it
 * never calls hs_macro_function_evaluate and has no null-check branch.  The
 * body unconditionally invokes debug_dump_memory_by_file and then commits a
 * zero result to the calling thread via hs_return.
 *
 * [EBP+0x8] (function_index) and [EBP+0x10] (init) are never read by this
 * body; they complete the standard hs-evaluator signature shared by every
 * other handler in this TU.  Ghidra mis-prototypes this as void(void) and
 * reports the [EBP+0xc] read as the phantom local `in_stack_00000008`.
 *
 * Callees (both cdecl, no register args):
 *   0x8ec60  = debug_dump_memory_by_file(void)
 *   0xcbf80  = hs_return(thread_handle, value)
 */
void FUN_000c1f20(int16_t function_index, int thread_datum, char init)
{
  debug_dump_memory_by_file();
  hs_return(thread_datum, 0);
}

/* 0xc1f40 — HaloScript script-function handler for the single-string form of
 * the memory dump command.  Evaluates the macro arguments; on success the
 * result block holds a `const char *` at +0x0 (the tag/file name filter),
 * which is forwarded to debug_dump_memory_for_file.  The thread is then
 * completed with a zero result via hs_return(thread_datum, 0).
 *
 * `thread_datum` lives in ESI across the whole body in the original, so it is
 * still available for the hs_return after the evaluate call.  MSVC coalesces
 * the stack cleanup for debug_dump_memory_for_file (1 arg) and hs_return
 * (2 args) into a single `ADD ESP,0xc` at 0xc1f6c; the call-site audit reads
 * that as a 3-argument hs_return, which is a false positive.
 *
 * Ghidra mis-prototypes this as void(void) and surfaces the three cdecl stack
 * params as the phantom locals in_stack_00000004/8/c.
 *
 * Callees (all cdecl, no register args):
 *   0xcc560  = hs_macro_function_evaluate(fn_index, thread_datum, init)
 *              -> int * (result record, NULL on failure)
 *   0x8eb80  = debug_dump_memory_for_file(const char *tag_filter)
 *   0xcbf80  = hs_return(thread_handle, value)
 */
void FUN_000c1f40(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    debug_dump_memory_for_file((const char *)result[0]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc1fa0 — HaloScript script-function stub (no-argument, void-result form).
 * Same shape as FUN_000c1f00/FUN_000c1f20 above: it takes no script
 * arguments, so it never calls hs_macro_function_evaluate and has no
 * null-check branch.  The body unconditionally invokes the errors.c
 * error-ring-buffer reset helper FUN_0008f630 and then commits a zero result
 * to the calling thread via hs_return.
 *
 * [EBP+0x8] (function_index) and [EBP+0x10] (init) are never read by this
 * body; they complete the standard hs-evaluator signature shared by every
 * other handler in this TU.  Ghidra mis-prototypes this as void(void) and
 * reports the [EBP+0xc] read as the phantom local `in_stack_00000008`.
 *
 * ABI (verified against the full 10-instruction body at 0xc1fa0): cdecl,
 * plain RET.  Push order for hs_return is `PUSH 0` then `PUSH EAX` where EAX
 * was loaded from [EBP+0xc], so arg1 = thread_datum and arg2 = 0.  The
 * `ADD ESP,8` belongs to hs_return alone.
 *
 * Callees (both cdecl, no register args):
 *   0x8f630  = FUN_0008f630(void)   [errors.c] — reset error ring buffer
 *   0xcbf80  = hs_return(thread_handle, value)
 */
void FUN_000c1fa0(int16_t function_index, int thread_datum, char init)
{
  FUN_0008f630();
  hs_return(thread_datum, 0);
}

/* 0xc1fc0 — HaloScript builtin "profile_dump" (one string argument).
 *
 * The script-function definition record at 0x2718f0 gives return_type = 4
 * (void), num_params = 1, param_types[0] = 9 (string), name = "profile_dump",
 * help = "dumps profile based on a substring."  The handler therefore takes
 * the standard hs evaluator triple, pulls the single evaluated argument out
 * of the macro-function result block, and forwards it as a `const char *`
 * substring filter.
 *
 * Ghidra mis-prototypes this as void(void) and surfaces the three cdecl stack
 * params as the phantom locals in_stack_00000004/8/c ([EBP+0x8], [EBP+0xc],
 * [EBP+0x10]).  It also drops the inner callee's argument: the disassembly at
 * 0xc1fdc does `MOV EDX,dword ptr [EAX]; PUSH EDX` before the CALL, so the
 * first dword of the result block is passed.  `thread_datum` is kept in ESI
 * across the whole body.
 *
 * The single `ADD ESP,0xc` at 0xc1fec is MSVC's merged cleanup for both tail
 * calls (1 dword + 2 dwords); hs_return still takes exactly 2 arguments.
 *
 * Callees (all cdecl, no register args):
 *   0xcc560  = hs_macro_function_evaluate(fn_index, thread_datum, init)
 *              -> int * (result record, NULL on failure)
 *   0x90650  = profile_dump_to_file(const char *substring)
 *   0xcbf80  = hs_return(thread_handle, value)
 */
void FUN_000c1fc0(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    profile_dump_to_file((const char *)result[0]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc2000 (hs.obj) — HaloScript function handler: activate profile sections.
 *
 * Evaluates the macro arguments; on success the result block holds a single
 * dword at +0x0 which is passed straight through to 0x90860
 * (profile_sections_activate).  0xc2010 does `MOV EDX,dword ptr [EAX];
 * PUSH EDX` before the CALL, so exactly one argument is passed — Ghidra's
 * decompile drops it.  There is no +0x4 read in this handler, unlike most of
 * its neighbours.  `thread_datum` is kept in ESI across the whole body.
 *
 * The single `ADD ESP,0xc` at 0xc202c is MSVC's merged cleanup for both tail
 * calls (1 dword + 2 dwords); hs_return still takes exactly 2 arguments.
 *
 * Callees (all cdecl, no register args):
 *   0xcc560  = hs_macro_function_evaluate(fn_index, thread_datum, init)
 *              -> int * (result record, NULL on failure)
 *   0x90860  = profile_sections_activate(const char *substring)
 *   0xcbf80  = hs_return(thread_handle, value)
 */
void FUN_000c2000(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    profile_sections_activate((const char *)result[0]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc2040 (hs.obj) — HaloScript function handler: deactivate profile sections.
 *
 * The exact deactivate twin of FUN_000c2000 above; byte-identical in shape,
 * differing only in which result-consumer it calls (0x90880
 * profile_sections_deactivate instead of 0x90860 profile_sections_activate).
 * Evaluates the macro arguments; while hs_macro_function_evaluate returns NULL
 * the evaluation is still pending and nothing is committed.  On success the
 * result record holds a single dword at +0x0 (a profile-section name
 * substring) which is passed straight through.
 *
 * cdecl frame (PUSH EBP; MOV EBP,ESP; PUSH ESI; no _chkstk, no locals, no FPU,
 * plain RET so the caller cleans — 0x32 bytes total):
 *   function_index  int16_t  [EBP+0x08]  -> evaluate arg1 (loaded to ECX)
 *   thread_datum    int      [EBP+0x0c]  -> evaluate arg2; held in ESI across
 *                                           the whole body, reused as
 *                                           hs_return arg1
 *   init            char     [EBP+0x10]  -> evaluate arg3 (loaded to EAX)
 *
 * The evaluate call pushes EAX([+0x10]), ESI([+0x0c]), ECX([+0x08]) and cleans
 * with ADD ESP,0xc, so its first argument is [EBP+0x08].  The returned EAX is
 * tested (TEST EAX,EAX / JZ 0xc206f) and then dereferenced at offset 0
 * (MOV EDX,[EAX]; PUSH EDX) as the single profile_sections_deactivate
 * argument — the kb decl `int hs_macro_function_evaluate(...)` really returns
 * a record POINTER, so cast at the call site rather than widening the callee
 * decl.  hs_return's arg1 comes from the preserved ESI (the ORIGINAL
 * thread_datum), not from the record.
 *
 * The single `ADD ESP,0xc` at 0xc206c is MSVC's merged cleanup for both tail
 * calls (1 dword + 2 dwords); the ARG_COUNT warning on 0xcbf80 ("cleanup=3 vs
 * decl=2") is that merge, and the PUSH count proves hs_return still takes
 * exactly 2 arguments (do NOT "fix" either decl).  Ghidra modeled this
 * void(void), so the three cdecl params surfaced as in_stack_* — they are
 * stack args, not @<reg>.
 *
 * 0x90880's kb decl was `void profile_sections_deactivate(void)`, which
 * contradicts the one pushed dword.  Its disassembly settles it: it is
 * `PUSH EBP; MOV EBP,ESP; PUSH EDI; MOV EDI,[EBP+8]; PUSH 0; CALL 0x907c0;
 * ADD ESP,4; POP EDI; POP EBP; RET` — one stack argument, forwarded in EDI to
 * the shared worker 0x907c0, which strcmp/prefix-matches it against the
 * profile-section name table at 0x3361b4 and stores the pushed enable byte
 * (0 here, 1 from profile_sections_activate 0x90860) into section+0x8.  The
 * argument is therefore a name substring, matching the sibling's existing
 * `const char *` decl; the kb decl is corrected to take it.
 *
 * Callees (all cdecl, no register args):
 *   0xcc560  = hs_macro_function_evaluate(fn_index, thread_datum, init)
 *              -> int * (result record, NULL on failure)
 *   0x90880  = profile_sections_deactivate(const char *substring)
 *   0xcbf80  = hs_return(thread_handle, value)
 */
void FUN_000c2040(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    profile_sections_deactivate((const char *)result[0]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc2080 (hs.obj) — HaloScript function handler: "profile_graph_toggle".
 *
 * Evaluates the macro arguments; on success the result block holds a single
 * dword at +0x0 which is passed straight through to 0xdf350
 * (profile_graph_toggle).  0xc209c does `MOV EDX,dword ptr [EAX]; PUSH EDX`
 * before the CALL, so exactly one argument is passed — Ghidra's decompile
 * drops it and also mis-prototypes the handler as `void (void)`.  There is no
 * +0x4 read in this handler.  `thread_datum` is kept in ESI across the whole
 * body because it is reused by the hs_return tail call.
 *
 * The single `ADD ESP,0xc` at 0xc20ac is MSVC's merged cleanup for both tail
 * calls (1 dword + 2 dwords); hs_return still takes exactly 2 arguments, so
 * the call-site audit's ARG_COUNT finding on 0xcbf80 is a false positive.
 *
 * Argument type is arbitrated from the hs_function_definition record at
 * 0x271950 (the sole data xref to this handler): name="profile_graph_toggle",
 * return_type=4 (void), num_params=1, param_types[0]=9 (string).  The pushed
 * dword is therefore a `const char *` value name, and the kb decl for 0xdf350
 * is corrected from `(void)` to take it.
 *
 * Callees (all cdecl, no register args):
 *   0xcc560  = hs_macro_function_evaluate(fn_index, thread_datum, init)
 *              -> int * (result record, NULL on failure)
 *   0xdf350  = profile_graph_toggle(const char *value_name)
 *   0xcbf80  = hs_return(thread_handle, value)
 */
void FUN_000c2080(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    profile_graph_toggle((const char *)result[0]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc20c0 (hs.obj) — HaloScript function handler: "debug_pvs".
 *
 * Evaluates the macro arguments; on success the result block holds a single
 * BYTE at +0x0 which is zero-extended and passed to 0x1965d0 (debug_pvs).
 * The disassembly does `XOR EDX,EDX; MOV DL,byte ptr [EAX]; PUSH EDX`, so the
 * load is one byte wide — not the dword shape used by the 0xc2000/0xc2040/
 * 0xc2080 siblings above.  `result` is therefore `unsigned char *`; an `int *`
 * deref here would emit a dword load and be a width bug.
 *
 * `thread_datum` is held in ESI across the whole body because it is reused by
 * the hs_return tail call.  The single `ADD ESP,0xc` at 0xc20ee is MSVC's
 * merged cleanup for both tail calls (1 dword + 2 dwords); hs_return still
 * takes exactly 2 arguments, so an ARG_COUNT finding on 0xcbf80 is a false
 * positive.
 *
 * Ghidra mis-prototypes the handler as `void (void)` and reports the three
 * stack parameters as in_stack_00000004/8/c; the real signature is cdecl
 * (int16_t function_index, int thread_datum, char init).
 *
 * Callees (all cdecl, no register args):
 *   0xcc560  = hs_macro_function_evaluate(fn_index, thread_datum, init)
 *              -> result record pointer (NULL on failure)
 *   0x1965d0 = debug_pvs(uint8_t enabled)
 *   0xcbf80  = hs_return(thread_handle, value)
 */
void FUN_000c20c0(int16_t function_index, int thread_datum, char init)
{
  unsigned char *result;

  result = (unsigned char *)hs_macro_function_evaluate(function_index,
                                                       thread_datum, init);
  if (result != NULL) {
    debug_pvs(*result);
    hs_return(thread_datum, 0);
  }
}

/* 0xc2100 (hs.obj) — HaloScript function handler: "radiosity_start".
 *
 * The script-function record at 0x271990 names this handler: return_type 4
 * (void), name "radiosity_start", help "starts radiosity computation.",
 * parse 0xc7e50 (the shared no-argument parser for this family),
 * evaluate 0xc2100 (this function), num_params 0.  The command therefore
 * takes no script arguments, which is why there is no
 * hs_macro_function_evaluate call and no result NULL check — identical in
 * shape to FUN_000c2140 directly below.  In this build the handler body
 * performs no side effect of its own: it only completes the calling script
 * thread with the value 0.  The symbol keeps its address name because
 * "radiosity_start" names the script command, not this wrapper.
 *
 * Disassembly (9 instructions).  Frame is PUSH EBP; MOV EBP,ESP only — no
 * locals and no `sub esp`.  Body:
 *
 *   MOV EAX,[EBP+0xc]    ; thread_datum
 *   PUSH 0x0             ; hs_return arg2 = value
 *   PUSH EAX             ; hs_return arg1 = thread_datum (cdecl: the last
 *                        ; PUSH is the first C argument)
 *   CALL 0xcbf80         ; hs_return
 *   ADD ESP,0x8          ; cdecl cleanup, 2 dwords
 *   POP EBP; RET         ; plain RET => caller-cleanup cdecl, no register args
 *
 * [EBP+0x8] (function_index) and [EBP+0x10] (init) are never read by this
 * body; they complete the standard hs-evaluator signature shared by every
 * other handler in this TU.  Ghidra mis-prototypes this as void(void) and
 * reports the [EBP+0xc] read as the phantom local `in_stack_00000008` —
 * that phantom is ARG 2, not arg 1.  Binding it to function_index would pass
 * a script-function index as the thread handle to hs_return, completing the
 * wrong HS thread with no crash and no VC71 delta.
 *
 * Callees (cdecl, no register args, ported):
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c2100(int16_t function_index, int thread_datum, char init)
{
  hs_return(thread_datum, 0);
}

/* 0xc2140 (hs.obj) — HaloScript function handler, no-op body.
 *
 * The smallest handler shape in this TU: the command takes no script
 * arguments (no hs_macro_function_evaluate call) and performs no side effect
 * of its own — it only completes the calling script thread with the value 0.
 * Compared with FUN_000c2160 directly below it is the same sub-shape minus
 * the leading debug-toggle CALL.  Which script command this record belongs to
 * is not established from the binary here, so the function keeps its address
 * name.
 *
 * Disassembly (8 instructions).  Frame is PUSH EBP; MOV EBP,ESP only — no
 * locals and no `sub esp`.  Body:
 *
 *   MOV EAX,[EBP+0xc]    ; thread_datum
 *   PUSH 0x0             ; hs_return arg2 = value
 *   PUSH EAX             ; hs_return arg1 = thread_datum (cdecl: the last
 *                        ; PUSH is the first C argument)
 *   CALL 0xcbf80         ; hs_return
 *   ADD ESP,0x8          ; cdecl cleanup, 2 dwords
 *   POP EBP; RET         ; plain RET => caller-cleanup cdecl, no register args
 *
 * [EBP+0x8] (function_index) and [EBP+0x10] (init) are never read by this
 * body; they complete the standard hs-evaluator signature shared by every
 * other handler in this TU.  Ghidra mis-prototypes this as void(void) and
 * reports the [EBP+0xc] read as the phantom local `in_stack_00000008` —
 * that phantom is ARG 2, not arg 1.  Binding it to function_index would pass
 * a script-function index as the thread handle to hs_return, completing the
 * wrong HS thread with no crash and no VC71 delta.
 *
 * Callees (cdecl, no register args, ported):
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c2140(int16_t function_index, int thread_datum, char init)
{
  hs_return(thread_datum, 0);
}

/* 0xc2160 (hs.obj) — HaloScript function handler: "ai_lines".
 *
 * Script-function table record at 0x2719e4: name "ai_lines", parse 0xc7e50,
 * help "cycles through AI line-spray modes", return_type 4 (void),
 * num_params 0.  Because the command takes no script arguments there is no
 * hs_macro_function_evaluate call — the handler runs the debug toggle and
 * completes the calling thread with the value 0.  Same minimal 10-instruction
 * sub-shape as FUN_000c1e80 / FUN_000c1ee0 / FUN_000c1f20 above.
 *
 * Disassembly (10 instructions).  Frame is PUSH EBP; MOV EBP,ESP only — no
 * locals and no `sub esp`.  Body:
 *
 *   CALL 0x53890         ; FUN_00053890(), no args, no cleanup
 *   MOV EAX,[EBP+0xc]    ; thread_datum
 *   PUSH 0x0             ; hs_return arg2 = value
 *   PUSH EAX             ; hs_return arg1 = thread_datum (cdecl: last PUSH
 *                        ; is the first C argument)
 *   CALL 0xcbf80         ; hs_return
 *   ADD ESP,0x8          ; cdecl cleanup, 2 dwords
 *
 * FUN_00053890 returns int16_t in AX; the original overwrites EAX with the
 * [EBP+0xc] load on the very next instruction, so the result is discarded
 * here as well — do not bind it to a local.
 *
 * [EBP+0x8] (function_index) and [EBP+0x10] (init) are never read by this
 * body; they complete the standard hs-evaluator signature shared by every
 * other handler in this TU.  Ghidra mis-prototypes this as void(void) and
 * reports the [EBP+0xc] read as the phantom local `in_stack_00000008`.
 *
 * Callees (both cdecl, no register args, both ported):
 *   0x53890 = FUN_00053890(void) -> int16_t   (AI line-spray mode cycle)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c2160(int16_t function_index, int thread_datum, char init)
{
  FUN_00053890();
  hs_return(thread_datum, 0);
}

/* 0xc2180 (hs.obj) — HaloScript function handler: AI sound-point debug toggle.
 *
 * Zero-argument script builtin, so there is no hs_macro_function_evaluate
 * call — the handler runs the debug toggle and completes the calling thread
 * with the value 0.  Structural twin of FUN_000c2160 directly above (same
 * 10-instruction sub-shape, same frame).
 *
 * Disassembly (10 instructions, 0xc2180-0xc2197).  Frame is PUSH EBP;
 * MOV EBP,ESP only — no locals and no `sub esp`.  Body:
 *
 *   CALL 0x49270         ; ai_debug_sound_point_set(), no args, no cleanup
 *   MOV EAX,[EBP+0xc]    ; thread_datum
 *   PUSH 0x0             ; hs_return arg2 = value
 *   PUSH EAX             ; hs_return arg1 = thread_datum (cdecl: last PUSH
 *                        ; is the first C argument)
 *   CALL 0xcbf80         ; hs_return
 *   ADD ESP,0x8          ; cdecl cleanup, 2 dwords
 *   POP EBP; RET         ; plain cdecl RET, no RET n
 *
 * [EBP+0x8] (function_index) and [EBP+0x10] (init) are never read by this
 * body; they complete the standard hs-evaluator signature shared by every
 * other handler in this TU.  Ghidra mis-prototypes this as void(void) and
 * reports the [EBP+0xc] read as the phantom local `in_stack_00000008`.
 *
 * Callees (both cdecl, no register args):
 *   0x49270 = ai_debug_sound_point_set(void)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c2180(int16_t function_index, int thread_datum, char init)
{
  ai_debug_sound_point_set();
  hs_return(thread_datum, 0);
}

/* 0xc21a0 (hs.obj) — HaloScript function handler: ai_debug_vocalize.
 *
 * Standard two-string macro-function wrapper.  The arguments are evaluated
 * by hs_macro_function_evaluate(function_index, thread_datum, init); on
 * success it returns a pointer to the evaluated result block, which here
 * holds two full-width string pointers:
 *
 *   result[0] (+0x0) = vocalization name
 *   result[1] (+0x4) = vocalization type name
 *
 * Disassembly (0xc21a0-0xc21d5, 25 bytes).  PUSH EBP; MOV EBP,ESP; PUSH ESI
 * — ESI is the only callee-saved register used, and holds thread_datum live
 * across both calls.  Body:
 *
 *   MOV ECX,[EBP+0x8]    ; function_index
 *   MOV ESI,[EBP+0xc]    ; thread_datum
 *   MOV EAX,[EBP+0x10]   ; init
 *   PUSH EAX; PUSH ESI; PUSH ECX
 *   CALL 0xcc560         ; hs_macro_function_evaluate(fn_index, thread, init)
 *   ADD ESP,0xc
 *   TEST EAX,EAX; JZ 0xc21d3   ; EAX is the result-block POINTER
 *   MOV EDX,[EAX+0x4]    ; result[1] (type name)
 *   MOV EAX,[EAX]        ; result[0] (name)
 *   PUSH EDX; PUSH EAX   ; cdecl: last PUSH is the first C argument
 *   CALL 0x49f60         ; ai_debug_vocalize(result[0], result[1])
 *   PUSH 0x0; PUSH ESI
 *   CALL 0xcbf80         ; hs_return(thread_datum, 0)
 *   ADD ESP,0x10         ; single MERGED cleanup for both calls (2+2 dwords)
 *   POP ESI; POP EBP; RET
 *
 * Both +0x0 and +0x4 are 32-bit loads with no MOVSX/MOVZX, so they are
 * pointers, not narrow ints — hence `const char **result` rather than the
 * `int *` used by the narrow-field siblings (FUN_000c0c30/FUN_000c0c70).
 *
 * The combined ADD ESP,0x10 is why the call-site audit reports an ARG_COUNT
 * hazard on hs_return (cleanup=4 dwords vs decl=2): it is a merged-cleanup
 * artifact, not an arity mismatch.  hs_return takes exactly 2 arguments.
 *
 * Ghidra mis-prototypes this as void(void) and reports the three stack
 * parameters as the phantom locals in_stack_00000004/8/c; it also drops
 * both arguments to ai_debug_vocalize entirely.
 *
 * Callees (all cdecl, no register args):
 *   0xcc560 = hs_macro_function_evaluate(function_index, thread_datum, init)
 *   0x49f60 = ai_debug_vocalize(vocalization_name, vocalization_type_name)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c21a0(int16_t function_index, int thread_datum, char init)
{
  const char **result;

  result = (const char **)hs_macro_function_evaluate(function_index,
                                                     thread_datum, init);
  if (result != NULL) {
    ai_debug_vocalize(result[0], result[1]);
    hs_return(thread_datum, 0);
  }
}

/* HaloScript builtin: evaluate the macro-function argument block, then hand
 * the first dword of the result record to the AI debug teleport command.
 *
 * Disassembly (0xc21e0):
 *   PUSH EBP; MOV EBP,ESP; PUSH ESI
 *   MOV ESI,[EBP+0xc]    ; thread_datum, kept in ESI across both calls
 *   MOV EAX,[EBP+0x10]   ; init
 *   MOV ECX,[EBP+0x8]    ; function_index (full dword load, no MOVSX)
 *   PUSH EAX; PUSH ESI; PUSH ECX
 *   CALL 0xcc560         ; hs_macro_function_evaluate(fn_index, thread, init)
 *   ADD ESP,0xc
 *   TEST EAX,EAX; JZ epilogue
 *   MOV EDX,dword ptr [EAX]; PUSH EDX
 *   CALL 0x4b0f0         ; ai_debug_teleport_to(result[0])
 *   PUSH 0x0; PUSH ESI
 *   CALL 0xcbf80         ; hs_return(thread_datum, 0)
 *   ADD ESP,0xc          ; single MERGED cleanup for both calls (1+2 dwords)
 *   POP ESI; POP EBP; RET
 *
 * The result block is read at +0x0 as a full 32-bit load with no MOVSX/MOVZX,
 * matching ai_debug_teleport_to's `int` parameter.
 *
 * The one ADD ESP,0xc covers ai_debug_teleport_to's single argument plus
 * hs_return's two, which is why the call-site audit reports an ARG_COUNT
 * hazard on hs_return (cleanup=3 dwords vs decl=2).  That is a merged-cleanup
 * artifact, not an arity mismatch — hs_return takes exactly 2 arguments.
 *
 * Ghidra mis-prototypes this as void(void) and surfaces the three cdecl
 * parameters as the phantom locals in_stack_00000004/8/c; it also drops the
 * argument to ai_debug_teleport_to entirely.
 *
 * Callees (all cdecl, no register args):
 *   0xcc560 = hs_macro_function_evaluate(function_index, thread_datum, init)
 *   0x4b0f0 = ai_debug_teleport_to(encounter_index)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c21e0(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    ai_debug_teleport_to(result[0]);
    hs_return(thread_datum, 0);
  }
}

/* Script macro-function wrapper for ai_debug_speak.
 *
 * Evaluates the macro function's single argument, then — if the evaluator
 * returned a non-NULL value block — passes that block's first dword (a char *
 * string, the name to speak) to ai_debug_speak and commits a zero result to
 * the calling script thread.
 *
 * Disassembly (0xc2220):
 *   PUSH EBP; MOV EBP,ESP; PUSH ESI
 *   MOV ECX,[EBP+0x8]    ; function_index
 *   MOV ESI,[EBP+0xc]    ; thread_datum  (kept live in ESI across both calls)
 *   MOV EAX,[EBP+0x10]   ; init
 *   PUSH EAX; PUSH ESI; PUSH ECX; CALL 0xcc560; ADD ESP,0xc
 *   TEST EAX,EAX; JZ end
 *   MOV EDX,[EAX]        ; result[0] — the char * name
 *   PUSH EDX; CALL 0x4a220
 *   PUSH 0x0; PUSH ESI; CALL 0xcbf80
 *   ADD ESP,0xc          ; single MERGED cleanup for both calls (1+2 dwords)
 *   POP ESI; POP EBP; RET
 *
 * The one ADD ESP,0xc covers ai_debug_speak's single argument plus
 * hs_return's two, which is why the call-site audit reports an ARG_COUNT
 * hazard on hs_return (cleanup=3 dwords vs decl=2).  That is a merged-cleanup
 * artifact, not an arity mismatch — hs_return takes exactly 2 arguments.
 *
 * Ghidra mis-prototypes this as void(void) and surfaces the three cdecl
 * parameters as the phantom locals in_stack_00000004/8/c; it also drops the
 * argument to ai_debug_speak entirely.  The evaluator's return is declared
 * int in kb.json (0xcc560) but is used here as a pointer, so it is cast.
 *
 * Callees (all cdecl, no register args):
 *   0xcc560 = hs_macro_function_evaluate(function_index, thread_datum, init)
 *   0x4a220 = ai_debug_speak(name)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c2220(int16_t function_index, int thread_datum, char init)
{
  char **result;

  result =
    (char **)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    ai_debug_speak(result[0]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc2260 (hs.obj) — HaloScript function handler: ai_debug_speak_list.
 *
 * Single-string macro-function wrapper, identical in shape to 0xc2220
 * (ai_debug_speak).  The arguments are evaluated by
 * hs_macro_function_evaluate(function_index, thread_datum, init); on success
 * it returns a pointer to the evaluated result block whose FIRST dword
 * (+0x0) is the dialogue-list name string pointer.
 *
 * Disassembly (0xc2260-0xc2292, 25 instructions).  PUSH EBP; MOV EBP,ESP;
 * PUSH ESI — no `sub esp`, so there are no stack locals; ESI is the only
 * callee-saved register used and caches thread_datum live across the first
 * call.  Body:
 *
 *   MOV EAX,[EBP+0x10]   ; init
 *   MOV ECX,[EBP+0x8]    ; function_index
 *   MOV ESI,[EBP+0xc]    ; thread_datum
 *   PUSH EAX; PUSH ESI; PUSH ECX
 *   CALL 0xcc560         ; hs_macro_function_evaluate(fn_index, thread, init)
 *   ADD ESP,0xc
 *   TEST EAX,EAX; JZ 0xc228f  ; EAX is the result-block POINTER
 *   MOV EDX,[EAX]        ; *(char **)result — deref, NOT the record pointer
 *   PUSH EDX
 *   CALL 0x4a290         ; ai_debug_speak_list(result[0])
 *   PUSH 0x0; PUSH ESI   ; cdecl: last PUSH is the first C argument
 *   CALL 0xcbf80         ; hs_return(thread_datum, 0)
 *   ADD ESP,0xc          ; one coalesced cleanup for BOTH calls (1 + 2 args)
 *   POP ESI; POP EBP; RET
 *
 * Note: the Ghidra decompile rendered the middle call as `FUN_0004a290()`
 * with no argument — it dropped the `MOV EDX,[EAX]` deref.  The argument is
 * the dword at result+0x0, not the record pointer.
 *
 * Callees (all cdecl, no register args):
 *   0xcc560 = hs_macro_function_evaluate(function_index, thread_datum, init)
 *   0x4a290 = ai_debug_speak_list(list_name)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c2260(int16_t function_index, int thread_datum, char init)
{
  char **result;

  result =
    (char **)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    ai_debug_speak_list(result[0]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc22a0 (hs.obj) — HaloScript function handler: screen effect fade in.
 *
 * Standard macro-function wrapper: hs_macro_function_evaluate() parses and
 * evaluates the script arguments and returns a pointer to the evaluated
 * result block (NULL on failure / while the thread is still evaluating).
 * On success the four fields of that block are handed to
 * player_effect_screen_fade_in() and the script call returns 0.
 *
 * Result block layout (0x0e bytes read, derived from the disassembly):
 *   +0x00  int     effect / definition handle   (MOV EAX,[EAX])
 *   +0x04  float   scale a                      (FLD dword [EAX+4])
 *   +0x08  float   scale b                      (FLD dword [EAX+8])
 *   +0x0c  uint16  flags / index                (XOR EDX,EDX; MOV DX,[EAX+0xc])
 *
 * Disassembly (0xc22a0-0xc22e8, 73 bytes).  PUSH EBP; MOV EBP,ESP; PUSH ESI —
 * no `sub esp`, so there are no stack locals; ESI is the only callee-saved
 * register used and caches thread_datum across the first call.  Body:
 *
 *   MOV ECX,[EBP+0x8]    ; function_index
 *   MOV ESI,[EBP+0xc]    ; thread_datum
 *   MOV EAX,[EBP+0x10]   ; init
 *   PUSH EAX; PUSH ESI; PUSH ECX
 *   CALL 0xcc560         ; hs_macro_function_evaluate(fn_index, thread, init)
 *   ADD ESP,0xc
 *   TEST EAX,EAX; JZ 0xc22e4  ; EAX is the result-block POINTER
 *   FLD  dword [EAX+0x8]      ; NOTE: the floats are loaded in the OPPOSITE
 *   XOR  EDX,EDX              ; order from the stack slots they end up in
 *   MOV  DX,word [EAX+0xc]
 *   PUSH EDX                  ; -> [ESP+0xc] = arg 4 (zero-extended uint16)
 *   SUB  ESP,0x8
 *   FSTP dword [ESP+0x4]      ; -> arg 3 = *(float *)(result + 2)
 *   FLD  dword [EAX+0x4]
 *   MOV  EAX,[EAX]            ; base pointer consumed LAST
 *   FSTP dword [ESP]          ; -> arg 2 = *(float *)(result + 1)
 *   PUSH EAX                  ; -> [ESP]   = arg 1 = result[0]
 *   CALL 0xa2970              ; player_effect_screen_fade_in(...)
 *   PUSH 0x0; PUSH ESI        ; cdecl: last PUSH is the first C argument
 *   CALL 0xcbf80              ; hs_return(thread_datum, 0)
 *   ADD ESP,0x18              ; one coalesced cleanup for BOTH calls (4 + 2)
 *   POP ESI; POP EBP; RET
 *
 * The Ghidra decompile prototyped this as `void FUN_000c22a0(void)` with
 * `in_stack_*` phantoms and rendered the middle call as `FUN_000a2970()` with
 * no arguments — it dropped all four (both floats are hidden behind the
 * PUSH/FSTP idiom).  The ARG_COUNT hazard on hs_return (cleanup=6 vs decl=2)
 * is a false positive from the single merged `ADD ESP,0x18`.
 *
 * Callees (all cdecl, no register args):
 *   0xcc560 = hs_macro_function_evaluate(function_index, thread_datum, init)
 *   0xa2970 = player_effect_screen_fade_in(effect, scale_a, scale_b, flags)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c22a0(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    player_effect_screen_fade_in(result[0], *(float *)(result + 1),
                                 *(float *)(result + 2),
                                 *(uint16_t *)(result + 3));
    hs_return(thread_datum, 0);
  }
}

/* 0xc22f0 (hs.obj) — HaloScript function handler: screen effect fade out.
 *
 * Script-function table record at 0x271ac0 (the sole data xref to 0xc22f0 is
 * the +0x0c evaluate slot):
 *   return_type = 4 (void),  name = "fade_out",  parse = 0xc7e50,
 *   help = "does a screen fade out to a particular color",
 *   num_params = 4,  param_types = (6, 6, 6, 7) = (real, real, real, short).
 *
 * Structural twin of FUN_000c22a0 (fade_in) directly above: evaluate the four
 * script arguments, and when the evaluator returns a completed argument block,
 * forward it to player_effect_screen_fade_out() and return 0 to the script.
 *
 * Disassembly (0xc22f0-0xc2338, 31 instructions).  PUSH EBP; MOV EBP,ESP;
 * PUSH ESI — no locals, no `sub esp` for the frame.  ESI = [EBP+0xC] =
 * thread_datum, held live across all three calls.
 *
 *   MOV EAX,[EBP+0x10]        ; init
 *   MOV ECX,[EBP+0x8]         ; function_index
 *   MOV ESI,[EBP+0xC]         ; thread_datum
 *   PUSH EAX; PUSH ESI; PUSH ECX
 *   CALL 0xcc560              ; hs_macro_function_evaluate(index, thread, init)
 *   ADD ESP,0xC; TEST EAX,EAX; JZ end   ; EAX = argument block pointer
 *   FLD  dword [EAX+0x8]
 *   XOR  EDX,EDX; MOV DX,word [EAX+0xC] ; zero-extended -> arg 4
 *   PUSH EDX                  ; [ESP+0xC] = arg 4 = *(uint16 *)(result + 3)
 *   SUB  ESP,0x8
 *   FSTP dword [ESP+0x4]      ; -> arg 3 = *(float *)(result + 2)
 *   FLD  dword [EAX+0x4]
 *   MOV  EAX,[EAX]            ; base pointer consumed LAST
 *   FSTP dword [ESP]          ; -> arg 2 = *(float *)(result + 1)
 *   PUSH EAX                  ; [ESP]   = arg 1 = result[0]
 *   CALL 0xa29c0              ; player_effect_screen_fade_out(...)
 *   PUSH 0x0; PUSH ESI        ; cdecl: last PUSH is the first C argument
 *   CALL 0xcbf80              ; hs_return(thread_datum, 0)
 *   ADD  ESP,0x18             ; one coalesced cleanup for BOTH calls (4 + 2)
 *   POP ESI; POP EBP; RET
 *
 * The Ghidra decompile prototyped this as `void FUN_000c22f0(void)` with
 * `in_stack_*` phantoms and rendered the middle call as `FUN_000a29c0()` with
 * no arguments — the PUSH/FSTP idiom hides both float arguments.  The
 * ARG_COUNT hazard on hs_return (cleanup=6 vs decl=2) is a false positive from
 * the single merged `ADD ESP,0x18`.
 *
 * kb.json declared 0xa29c0 as `void player_effect_screen_fade_out(void)`,
 * which would have compiled this call site to a zero-argument call and left
 * the callee reading stack garbage.  Corrected to the four-parameter form,
 * mirroring its already-correct sibling 0xa2970 (fade_in).  0xc22f0 is the
 * only caller of 0xa29c0 in the XBE.
 *
 * Callees (all cdecl, no register args):
 *   0xcc560 = hs_macro_function_evaluate(function_index, thread_datum, init)
 *   0xa29c0 = player_effect_screen_fade_out(effect, scale_a, scale_b, flags)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c22f0(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    player_effect_screen_fade_out(result[0], *(float *)(result + 1),
                                  *(float *)(result + 2),
                                  *(uint16_t *)(result + 3));
    hs_return(thread_datum, 0);
  }
}

/* 0xc2340 (hs.obj) — HaloScript function handler: cinematic start.
 *
 * Zero-argument script builtin, so there is no hs_macro_function_evaluate
 * call — the handler runs cinematic_start() and completes the calling thread
 * with the value 0.  Structural twin of FUN_000c2180 above (same
 * 10-instruction sub-shape, same frame).
 *
 * Disassembly (10 instructions).  Frame is PUSH EBP; MOV EBP,ESP only — no
 * locals and no `sub esp`.  Body:
 *
 *   CALL 0x92e20         ; cinematic_start(), no args pushed, no cleanup
 *   MOV EAX,[EBP+0xc]    ; thread_datum
 *   PUSH 0x0             ; hs_return arg2 = value
 *   PUSH EAX             ; hs_return arg1 = thread_datum (cdecl: last PUSH
 *                        ; is the first C argument)
 *   CALL 0xcbf80         ; hs_return
 *   ADD ESP,0x8          ; cdecl cleanup, 2 dwords
 *   POP EBP; RET         ; plain cdecl RET, no RET n
 *
 * [EBP+0x8] (function_index) and [EBP+0x10] (init) are never read by this
 * body; they complete the standard hs-evaluator signature shared by every
 * other handler in this TU (a cdecl parameter the callee ignores emits no
 * code, so the disassembly cannot distinguish 2 params from 3 — the sibling
 * handlers arbitrate).  Ghidra mis-prototypes this as void(void) and reports
 * the [EBP+0xc] read as the phantom local `in_stack_00000008`.
 *
 * Callees (both cdecl, no register args):
 *   0x92e20 = cinematic_start(void)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c2340(int16_t function_index, int thread_datum, char init)
{
  cinematic_start();
  hs_return(thread_datum, 0);
}

/* 0xc2360 (hs.obj) — HaloScript function handler: cinematic stop.
 *
 * Exact mirror of FUN_000c2340 above with the inner callee swapped from
 * cinematic_start (0x92e20) to cinematic_stop (0x93050).  Zero-argument
 * script builtin, so there is no hs_macro_function_evaluate call — the
 * handler stops the cinematic and completes the calling thread with 0.
 *
 * Disassembly (10 instructions, 0x18 bytes).  Frame is PUSH EBP;
 * MOV EBP,ESP only — no locals, no `sub esp`, no FPU, no SEH.  Body:
 *
 *   CALL 0x93050         ; cinematic_stop(), no args pushed, no cleanup
 *   MOV EAX,[EBP+0xc]    ; thread_datum
 *   PUSH 0x0             ; hs_return arg2 = value
 *   PUSH EAX             ; hs_return arg1 = thread_datum (cdecl: last PUSH
 *                        ; is the first C argument)
 *   CALL 0xcbf80         ; hs_return
 *   ADD ESP,0x8          ; cdecl cleanup, 2 dwords -> hs_return takes 2 args
 *   POP EBP; RET         ; plain cdecl RET, no RET n
 *
 * [EBP+0x8] (function_index) and [EBP+0x10] (init) are never read by this
 * body; they complete the standard hs-evaluator signature shared by every
 * handler in this TU (a cdecl parameter the callee ignores emits no code, so
 * the disassembly cannot distinguish 2 params from 3 — the sibling handlers
 * arbitrate).  Ghidra mis-prototypes this as void(void) and reports the
 * [EBP+0xc] read as the phantom local `in_stack_00000008`.
 *
 * Unlike the merged `ADD ESP,0xc` at 0xc206c, the cleanup here is a single
 * un-merged `ADD ESP,0x8`, so no ARG_COUNT warning is expected on 0xcbf80
 * from this call site.
 *
 * Callees (both cdecl, no register args):
 *   0x93050 = cinematic_stop(void)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c2360(int16_t function_index, int thread_datum, char init)
{
  cinematic_stop();
  hs_return(thread_datum, 0);
}

/* 0xc2380 (hs.obj) — HaloScript function handler: cinematic skip start.
 *
 * Third member of the cinematic_start / cinematic_stop trio above; an exact
 * mirror of FUN_000c2360 with the inner callee swapped from cinematic_stop
 * (0x93050) to cinematic_skip_start (0x92e70).  Zero-argument script builtin,
 * so there is no hs_macro_function_evaluate call — the handler arms the
 * cinematic skip and completes the calling thread with 0.
 *
 * Disassembly (10 instructions).  Frame is PUSH EBP; MOV EBP,ESP only — no
 * locals, no `sub esp`, no FPU, no SEH, no buffers.  Body:
 *
 *   CALL 0x92e70         ; cinematic_skip_start(), no args pushed, no cleanup
 *   MOV EAX,[EBP+0xc]    ; thread_datum
 *   PUSH 0x0             ; hs_return arg2 = value
 *   PUSH EAX             ; hs_return arg1 = thread_datum (cdecl: last PUSH
 *                        ; is the first C argument)
 *   CALL 0xcbf80         ; hs_return
 *   ADD ESP,0x8          ; cdecl cleanup, 2 dwords -> hs_return takes 2 args
 *   POP EBP; RET         ; plain cdecl RET, no RET n
 *
 * [EBP+0x8] (function_index) and [EBP+0x10] (init) are never read by this
 * body; they complete the standard hs-evaluator signature shared by every
 * handler in this TU (a cdecl parameter the callee ignores emits no code, so
 * the disassembly cannot distinguish 2 params from 3 — the sibling handlers
 * arbitrate).  Ghidra mis-prototypes this as void(void) and reports the
 * [EBP+0xc] read as the phantom local `in_stack_00000008`; the kb decl was
 * corrected with this lift.
 *
 * The absence of any cleanup after CALL 0x92e70 confirms cinematic_skip_start
 * takes no arguments, matching its kb decl.  The `ADD ESP,0x8` is un-merged,
 * so no ARG_COUNT warning is expected on 0xcbf80 from this call site.
 *
 * Callees (both cdecl, no register args):
 *   0x92e70 = cinematic_skip_start(void)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c2380(int16_t function_index, int thread_datum, char init)
{
  cinematic_skip_start();
  hs_return(thread_datum, 0);
}

/* 0xc23a0 (hs.obj) — HaloScript function handler: cinematic skip stop.
 *
 * Fourth and final member of the cinematic quartet (start 0xc2340, stop
 * 0xc2360, skip_start 0xc2380, skip_stop here); an exact mirror of
 * FUN_000c2380 with the inner callee swapped from cinematic_skip_start
 * (0x92e70) to cinematic_skip_stop (0x92e80).  Zero-argument script builtin,
 * so there is no hs_macro_function_evaluate call — the handler disarms the
 * cinematic skip and completes the calling thread with 0.
 *
 * Disassembly (10 instructions).  Frame is PUSH EBP; MOV EBP,ESP only — no
 * locals, no `sub esp`, no FPU, no SEH, no buffers.  Body:
 *
 *   CALL 0x92e80         ; cinematic_skip_stop(), no args pushed, no cleanup
 *   MOV EAX,[EBP+0xc]    ; thread_datum
 *   PUSH 0x0             ; hs_return arg2 = value
 *   PUSH EAX             ; hs_return arg1 = thread_datum (cdecl: last PUSH
 *                        ; is the first C argument)
 *   CALL 0xcbf80         ; hs_return
 *   ADD ESP,0x8          ; cdecl cleanup, 2 dwords -> hs_return takes 2 args
 *   POP EBP; RET         ; plain cdecl RET, no RET n
 *
 * [EBP+0x8] (function_index) and [EBP+0x10] (init) are never read by this
 * body; they complete the standard hs-evaluator signature shared by every
 * handler in this TU (a cdecl parameter the callee ignores emits no code, so
 * the disassembly cannot distinguish 2 params from 3 — the sibling handlers
 * arbitrate).  Ghidra mis-prototypes this as void(void) and reports the
 * [EBP+0xc] read as the phantom local `in_stack_00000008`; the kb decl was
 * corrected with this lift.
 *
 * The absence of any cleanup after CALL 0x92e80 confirms cinematic_skip_stop
 * takes no arguments, matching its kb decl.  The `ADD ESP,0x8` is un-merged,
 * so no ARG_COUNT warning is expected on 0xcbf80 from this call site.
 *
 * Callees (both cdecl, no register args):
 *   0x92e80 = cinematic_skip_stop(void)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c23a0(int16_t function_index, int thread_datum, char init)
{
  cinematic_skip_stop();
  hs_return(thread_datum, 0);
}

/* 0xc23c0 — HaloScript handler: evaluate the macro-function argument and use
 * its result to toggle the cinematic letterbox bars.
 *
 * hs_macro_function_evaluate returns a pointer to the evaluated result record
 * (or NULL while the argument is still being evaluated).  Only the BYTE at
 * offset +0x0 of that record is read here (XOR EDX,EDX; MOV DL,[EAX]; PUSH
 * EDX), i.e. the boolean value of the single argument, which is forwarded to
 * cinematic_show_letterbox.  The thread is then completed with a zero result.
 *
 * kb notes: 0xcc560 is declared returning `int` but is used as a pointer at
 * every call site in this TU, so it is cast locally (established hs.c idiom
 * rather than editing the shared decl).  The kb decl for 0x92e90 said
 * `void cinematic_show_letterbox(void)` but the disassembly pushes one
 * byte-width argument before the CALL; the decl was corrected with this lift.
 *
 * The 4 bytes pushed for cinematic_show_letterbox are not popped after its
 * CALL — MSVC merged that cleanup into the `ADD ESP,0xc` following hs_return,
 * which therefore covers 3 pushes (EDX + 0 + ESI).  The call-site audit's
 * ARG_COUNT warning on hs_return (cleanup=3 vs decl=2) is a false positive
 * from that merge; hs_return really does take 2 arguments.
 *
 * Callees (all cdecl, no register args):
 *   0xcc560 = hs_macro_function_evaluate(function_index, thread_datum, init)
 *   0x92e90 = cinematic_show_letterbox(char show)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c23c0(int16_t function_index, int thread_datum, char init)
{
  char *result;

  result =
    (char *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != (char *)0) {
    cinematic_show_letterbox(*result);
    hs_return(thread_datum, 0);
  }
  return;
}

/* 0xc2400 (hs.obj) — HaloScript handler: evaluate the single macro-function
 * argument and forward its 16-bit value to FUN_00093640.
 *
 * Structurally identical to the sibling at 0xc23c0 (letterbox toggle): the
 * argument is evaluated, the NULL result means "still evaluating" and the
 * handler returns without completing the thread; a non-NULL result is
 * dereferenced and forwarded, then the thread is completed with 0.  The only
 * differences from 0xc23c0 are the width of the load (WORD here vs BYTE
 * there) and the inner callee.
 *
 * Disassembly.  Frame is PUSH EBP; MOV EBP,ESP; PUSH ESI — no `sub esp`, no
 * locals, no buffers, no FPU, no SEH, no register args.  Plain cdecl RET
 * (no RET n).  Parameters (cdecl, first PUSH is the last C argument):
 *
 *   [EBP+0x08] = function_index (int16_t; loaded into ECX as a dword)
 *   [EBP+0x0c] = thread_datum   (int; cached in ESI across the whole body,
 *                                the natural codegen for a param read twice)
 *   [EBP+0x10] = init           (char; loaded into EAX as a dword)
 *
 *   0xc2410  PUSH EAX / PUSH ESI / PUSH ECX ; CALL 0xcc560 ; ADD ESP,0xc
 *            -> hs_macro_function_evaluate(function_index, thread_datum, init)
 *            TEST EAX,EAX ; JZ end
 *   0xc241e  XOR EDX,EDX ; MOV DX,word ptr [EAX]
 *            -> EAX is DEREFERENCED, so 0xcc560's result is a POINTER to the
 *               evaluated 16-bit script value, not a scalar, and the TEST is
 *               a NULL-pointer test.  XOR+MOV DX is an UNSIGNED widening, so
 *               the load must be uint16_t (a signed load would emit MOVSX).
 *   0xc2422  PUSH EDX ; CALL 0x93640
 *   0xc242a  PUSH 0x0 ; PUSH ESI ; CALL 0xcbf80  -> hs_return(thread_datum, 0)
 *   0xc242f  ADD ESP,0xc ; POP ESI ; POP EBP ; RET
 *
 * The single `ADD ESP,0xc` is MSVC's merge of 0x93640's one push with
 * hs_return's two.  The call-site audit therefore reports a spurious
 * ARG_COUNT warning on hs_return (cleanup=3 vs decl=2) — hs_return really
 * does take 2 arguments; the extra dword belongs to 0x93640.  Conversely the
 * merge is proof that 0x93640 takes exactly one stack argument, so its kb
 * decl (`void FUN_00093640(void)`) was corrected with this lift.
 *
 * kb note: 0xcc560 is declared returning `int` but is used as a pointer at
 * every call site in this TU, so it is cast locally (established hs.c idiom)
 * rather than editing the shared decl, which other callers depend on.
 *
 * Callees (all cdecl, no register args):
 *   0xcc560 = hs_macro_function_evaluate(function_index, thread_datum, init)
 *   0x93640 = FUN_00093640(int value)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c2400(int16_t function_index, int thread_datum, char init)
{
  uint16_t *result;

  result =
    (uint16_t *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != (uint16_t *)0) {
    FUN_00093640((int)*result);
    hs_return(thread_datum, 0);
  }
  return;
}

/* 0xc2440 (hs.obj) — HaloScript handler: evaluate the macro-function argument
 * and forward its two-field result record to cinematic_set_title_delayed.
 *
 * Same shape as the siblings at 0xc23c0 (byte field) and 0xc2400 (word field),
 * but the first handler in this cluster that forwards a FLOAT out of the
 * result record: the argument is evaluated, a NULL result means "still
 * evaluating" and the handler returns without completing the thread, and a
 * non-NULL result is dereferenced twice and forwarded before the thread is
 * completed with 0.
 *
 * Disassembly (27 instructions, 0xc2440-0xc247b).  Frame is PUSH EBP;
 * MOV EBP,ESP; PUSH ESI — no `sub esp`, no locals, no buffers, no SEH, no
 * _chkstk, no register args.  Plain cdecl RET (no RET n).  Parameters
 * (cdecl, first PUSH is the last C argument):
 *
 *   [EBP+0x08] = function_index (int16_t; loaded into ECX as a dword)
 *   [EBP+0x0c] = thread_datum   (int; cached in ESI across the whole body,
 *                                the natural codegen for a param read twice)
 *   [EBP+0x10] = init           (char; loaded into EAX as a dword)
 *
 *   0xc2450  PUSH EAX / PUSH ESI / PUSH ECX ; CALL 0xcc560 ; ADD ESP,0xc
 *            -> hs_macro_function_evaluate(function_index, thread_datum, init)
 *               The un-merged 0xc cleanup confirms the 3-param cdecl decl.
 *            TEST EAX,EAX ; JZ 0xc2479
 *            -> EAX is DEREFERENCED below, so 0xcc560's result is a POINTER
 *               to the evaluated result record and the TEST is a NULL test.
 *   0xc2460  FLD dword ptr [EAX+0x4]   ; float field at record +0x4
 *            XOR EDX,EDX ; MOV DX,[EAX]; ZERO-extended 16-bit field at +0x0.
 *               Unlike the sibling handler at 0xbe030 (which uses MOVSX), this
 *               is an unsigned widening, so the load must be uint16_t.
 *   0xc2469  PUSH ECX                  ; dummy dword slot; ECX is irrelevant
 *            FSTP dword ptr [ESP]      ; the real arg2 = record float @ +0x4
 *            PUSH EDX                  ; arg1 = record u16 @ +0x0
 *            CALL 0x930b0              ; cinematic_set_title_delayed
 *   0xc2471  PUSH 0x0 ; PUSH ESI ; CALL 0xcbf80 -> hs_return(thread_datum, 0)
 *   0xc2476  ADD ESP,0x10 ; POP ESI ; POP EBP ; RET
 *
 * The PUSH-then-FSTP at 0xc2469 is MSVC's float-argument idiom: the pushed
 * register only reserves the stack dword, which FSTP then overwrites with the
 * FPU value.  Ghidra reads the dummy PUSH as the argument and consequently
 * printed the call as `FUN_000930b0()` with no arguments at all; the kb decl
 * `void cinematic_set_title_delayed(void)` inherited that error and was
 * corrected with this lift to (int, float).
 *
 * The single `ADD ESP,0x10` is MSVC's merge of cinematic_set_title_delayed's
 * two pushes with hs_return's two.  The call-site audit therefore reports a
 * spurious ARG_COUNT warning on hs_return (cleanup=4 vs decl=2) — hs_return
 * really does take 2 arguments; the other two dwords belong to 0x930b0.
 *
 * The lone FLD/FSTP pair is a pure pass-through of a memory float onto the
 * argument stack: no arithmetic, so no operand-order hazard.
 *
 * kb note: 0xcc560 is declared returning `int` but is used as a pointer at
 * every call site in this TU, so it is cast locally (established hs.c idiom)
 * rather than editing the shared decl, which other callers depend on.
 *
 * Callees (all cdecl, no register args):
 *   0xcc560 = hs_macro_function_evaluate(function_index, thread_datum, init)
 *   0x930b0 = cinematic_set_title_delayed(int index, float value)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c2440(int16_t function_index, int thread_datum, char init)
{
  uint16_t *result;

  result =
    (uint16_t *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != (uint16_t *)0) {
    cinematic_set_title_delayed((int)*result, *(float *)(result + 2));
    hs_return(thread_datum, 0);
  }
  return;
}

/* 0xc2480 (hs.obj) - HaloScript handler: evaluate the single macro-function
 * argument and forward it to cinematic_suppress_bsp_object_creation.
 *
 * Structural twin of the 0xc1640 dispatch family: the argument is evaluated,
 * a NULL result means "still evaluating" and the handler returns without
 * completing the thread, and a non-NULL result has a SINGLE zero-extended
 * byte read from OFFSET +0x0 of the result record, which becomes the only
 * argument to the dispatch callee.  The thread is then completed with 0.
 *
 * Disassembly (0xc2480-0xc24b3, 52 bytes).  Frame is PUSH EBP; MOV EBP,ESP;
 * PUSH ESI - no `sub esp`, no locals, no buffers, no SEH, no _chkstk, no
 * register args.  Plain cdecl RET (no RET n).  Parameters (cdecl, first PUSH
 * is the last C argument):
 *
 *   [EBP+0x08] = function_index (int16_t; loaded into ECX as a dword)
 *   [EBP+0x0c] = thread_datum   (int; cached in ESI across the whole body,
 *                                the natural codegen for a param read twice)
 *   [EBP+0x10] = init           (char; loaded into EAX as a dword)
 *
 *   PUSH EAX / PUSH ESI / PUSH ECX ; CALL 0xcc560 ; ADD ESP,0xc
 *     -> hs_macro_function_evaluate(function_index, thread_datum, init).
 *        EAX is DEREFERENCED below, so the result is a POINTER to the
 *        evaluated result record and the TEST EAX,EAX is a NULL test.
 *   XOR EDX,EDX ; MOV DL,byte ptr [EAX]
 *     -> ZERO-extended 8-bit field at record +0x0, so the load must be
 *        unsigned char - not int, and not signed char (a signed load would
 *        be MOVSX).
 *   PUSH EDX ; CALL 0x93030  -> cinematic_suppress_bsp_object_creation(byte)
 *   PUSH 0x0 ; PUSH ESI ; CALL 0xcbf80 -> hs_return(thread_datum, 0)
 *   ADD ESP,0xc ; POP ESI ; POP EBP ; RET
 *
 * The single `ADD ESP,0xc` is MSVC's merge of 0x93030's one push with
 * hs_return's two.  The call-site audit therefore reports a spurious
 * ARG_COUNT warning on hs_return (cleanup=3 vs decl=2) - hs_return really
 * does take 2 arguments; the third dword belongs to 0x93030.
 *
 * kb note: 0x93030's decl was `void cinematic_suppress_bsp_object_creation(
 * void)`, which contradicts the PUSH EDX at this call site; it is corrected
 * to take the single byte with this lift.  0xcc560 is declared returning
 * `int` but is used as a pointer at every call site in this TU, so it is
 * cast locally (the established hs.c idiom) rather than editing the shared
 * decl that other callers depend on.
 *
 * Callees (all cdecl, no register args):
 *   0xcc560 = hs_macro_function_evaluate(function_index, thread_datum, init)
 *   0x93030 = cinematic_suppress_bsp_object_creation(unsigned char suppress)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c2480(int16_t function_index, int thread_datum, char init)
{
  unsigned char *result;

  result = (unsigned char *)hs_macro_function_evaluate(function_index,
                                                       thread_datum, init);
  if (result != (unsigned char *)0) {
    cinematic_suppress_bsp_object_creation(*result);
    hs_return(thread_datum, 0);
  }
  return;
}

/* 0xc24c0 — HaloScript function evaluator: run the event-manager tab advance
 * for its side effect, then commit a 0 result to the calling script thread
 * (a void-returning script builtin).
 *
 * Structural twin of 0xc0cb0 — the two bodies differ only in the first CALL
 * target.
 *
 * Callees (both cdecl, ported):
 *   0xdc140 = event_manager_tab_process(void) — no arguments
 *   0xcbf80 = hs_return(thread_handle, value)
 *
 * ABI (verified against disassembly 0xc24c0-0xc24d7, 10 instructions): cdecl,
 * plain RET, frame is PUSH EBP; MOV EBP,ESP only — no `sub esp`, no locals,
 * no buffers, no FPU, no _chkstk, no register args. The body reads only
 * [EBP+0xc] = thread_datum (arg 2); function_index ([EBP+0x8]) and init
 * ([EBP+0x10]) are never read but complete the uniform hs-evaluator dispatch
 * signature. Push order at the second call (PUSH 0x0 then PUSH EAX, ADD
 * ESP,0x8) confirms hs_return(thread_datum, 0), not (0, thread_datum). */
void FUN_000c24c0(int16_t function_index, int thread_datum, char init)
{
  event_manager_tab_process();
  hs_return(thread_datum, 0);
}

/* 0xc24e0 — HaloScript function evaluator: flag the current map as won for
 * the main menu / campaign progression, then commit a 0 result to the calling
 * script thread (a void-returning script builtin).
 *
 * Structural twin of 0xc24c0 and 0xc0cb0 — the three bodies differ only in
 * the first CALL target.
 *
 * Callees (both cdecl, ported):
 *   0x100370 = main_won_map(void) — no arguments
 *   0xcbf80  = hs_return(thread_handle, value)
 *
 * ABI (verified against disassembly 0xc24e0-0xc24f7, 10 instructions): cdecl,
 * plain RET, frame is PUSH EBP; MOV EBP,ESP only — no `sub esp`, no locals,
 * no buffers, no FPU, no _chkstk, no register args. The body reads only
 * [EBP+0xc] = thread_datum (arg 2); function_index ([EBP+0x8]) and init
 * ([EBP+0x10]) are never read but complete the uniform hs-evaluator dispatch
 * signature shared with the ported neighbours at 0xc2400-0xc24c0. Push order
 * at the second call (PUSH 0x0 then PUSH EAX, ADD ESP,0x8) confirms
 * hs_return(thread_datum, 0), not (0, thread_datum).
 *
 * kb note: the prior decl was `void FUN_000c24e0(void);`, which contradicts
 * the MOV EAX,[EBP+0xc] at 0xc24e8; it is corrected to the 3-argument
 * evaluator signature with this lift. */
void FUN_000c24e0(int16_t function_index, int thread_datum, char init)
{
  main_won_map();
  hs_return(thread_datum, 0);
}

/* 0xc2500 — HS script function handler: invoke the main-loop transition
 * helper at 0x100380 for its side effect, then commit a 0 result to the
 * calling script thread (a void-returning script builtin).
 *
 * Callees (both cdecl, ported):
 *   0x100380 = FUN_00100380(void) — main map/menu transition helper
 *   0xcbf80  = hs_return(thread_handle, value)
 *
 * ABI (verified against disassembly 0xc2500-0xc2517): cdecl, plain RET. The
 * body reads only [EBP+0xc] = thread_datum (arg 2); function_index and init
 * complete the standard hs-evaluator signature (matches 0xc0cb0 / 0xc24e0)
 * but are unused in this body. */
void FUN_000c2500(int16_t function_index, int thread_datum, char init)
{
  FUN_00100380();
  hs_return(thread_datum, 0);
}

/* 0xc2520 — HS script function handler: query whether the game is currently
 * safe to save, and return that boolean to the calling script thread.
 *
 * Takes no script arguments, so there is no hs_macro_function_evaluate call
 * and no result-record null check — it is the value-returning member of the
 * bare side-effect wrapper family (compare 0xc2500 directly above, which
 * returns a constant 0).
 *
 * Callees (both cdecl, ported):
 *   0xa7530 = game_safe_to_save(void) -> bool in AL
 *   0xcbf80 = hs_return(thread_handle, value)
 *
 * ABI (verified against disassembly 0xc2520-0xc2545): cdecl, plain RET. The
 * body reads only [EBP+0xc] = thread_datum (arg 2); function_index and init
 * complete the standard hs-evaluator signature but are unused here.
 *
 * The single 4-byte local (frame is `PUSH ECX`) is zeroed as a full dword
 * BEFORE the call, then only its low byte is overwritten with AL — the
 * type-pun widening idiom, not a movzx conversion. */
void FUN_000c2520(int16_t function_index, int thread_datum, char init)
{
  int value;

  value = 0;
  *(bool *)&value = game_safe_to_save();
  hs_return(thread_datum, value);
}

/* 0xc2550 — HS script function handler: query whether the game world is
 * currently "all quiet" (no active combat / hostile contact) and return that
 * boolean to the calling script thread.
 *
 * Structurally identical to 0xc2520 directly above: no script arguments, so
 * there is no hs_macro_function_evaluate call and no result-record null
 * check.  Only the queried predicate differs.
 *
 * Callees (both cdecl, ported):
 *   0xa74f0 = game_all_quiet(void) -> bool in AL
 *   0xcbf80 = hs_return(thread_handle, value)
 *
 * ABI (verified against disassembly 0xc2550-0xc2576, 15 instructions): cdecl,
 * plain RET (caller cleans).  The body reads only [EBP+0xc] = thread_datum
 * (arg 2); function_index ([EBP+8]) and init ([EBP+0x10]) complete the
 * standard hs-evaluator signature but are never referenced here.  Keeping
 * them in the prototype is what puts thread_datum at the correct offset.
 *
 * The single 4-byte local (frame is `PUSH ECX`, no `sub esp`) is zeroed as a
 * full dword BEFORE the call, then only its low byte is overwritten with AL
 * — the type-pun widening idiom, not a movzx conversion.  Declaring the local
 * as bool/char would drop the dword zeroing and lose the match. */
void FUN_000c2550(int16_t function_index, int thread_datum, char init)
{
  int value;

  value = 0;
  *(bool *)&value = game_all_quiet();
  hs_return(thread_datum, value);
}

/* 0xc2580 — HS script function handler: query whether the game is currently
 * safe for characters to speak (dialogue gating) and return that boolean to
 * the calling script thread.
 *
 * Structurally identical to 0xc2550 and 0xc2520 directly above: the builtin
 * takes no script arguments, so there is no hs_macro_function_evaluate call
 * and no result-record null check.  Only the queried predicate differs.
 *
 * Callees (both cdecl, ported):
 *   0xa7670 = game_safe_to_speak(void) -> bool in AL
 *   0xcbf80 = hs_return(thread_handle, value)
 *
 * ABI (verified against disassembly 0xc2580-0xc25a6, 39 bytes): cdecl, plain
 * RET (caller cleans; `ADD ESP,8` after the hs_return call covers its two
 * cdecl args).  The body reads only [EBP+0xc] = thread_datum (arg 2);
 * function_index ([EBP+8]) and init ([EBP+0x10]) complete the standard
 * hs-evaluator signature but are never referenced here.  Keeping them in the
 * prototype is what puts thread_datum at the correct offset.
 *
 * The single 4-byte local (frame is `PUSH ECX`, no `sub esp`) is zeroed as a
 * full dword BEFORE the call (MOV dword [EBP-4],0), then only its low byte is
 * overwritten with AL (MOV byte [EBP-4],AL) and the whole dword reloaded (MOV
 * EAX,[EBP-4]) — the type-pun widening idiom, not a movzx conversion.
 * Declaring the local as bool/char would drop the dword zeroing and lose the
 * match. */
void FUN_000c2580(int16_t function_index, int thread_datum, char init)
{
  int value;

  value = 0;
  *(bool *)&value = game_safe_to_speak();
  hs_return(thread_datum, value);
}

/* 0xc25b0 — HS script function handler: query whether the current game is
 * running in cooperative mode and return that boolean to the calling script
 * thread.
 *
 * Structurally identical to 0xc2580 and 0xc2550 directly above: the builtin
 * takes no script arguments, so there is no hs_macro_function_evaluate call
 * and no result-record null check.  Only the queried predicate differs.
 *
 * Callees (both cdecl, ported):
 *   0xa7690 = game_is_cooperative(void) -> bool in AL
 *   0xcbf80 = hs_return(thread_handle, value)
 *
 * ABI (verified against disassembly 0xc25b0-0xc25d6, 15 instructions): cdecl,
 * plain RET (caller cleans; `ADD ESP,8` after the hs_return call covers its
 * two cdecl args).  The body reads only [EBP+0xc] = thread_datum (arg 2);
 * function_index ([EBP+8]) and init ([EBP+0x10]) complete the standard
 * hs-evaluator signature but are never referenced here.  Keeping them in the
 * prototype is what puts thread_datum at the correct offset.
 *
 * The single 4-byte local (frame is `PUSH ECX`, no `sub esp`) is zeroed as a
 * full dword BEFORE the call (MOV dword [EBP-4],0), then only its low byte is
 * overwritten with AL (MOV byte [EBP-4],AL) and the whole dword reloaded (MOV
 * EAX,[EBP-4]) — the type-pun widening idiom, not a movzx conversion.
 * Declaring the local as bool/char would drop the dword zeroing and lose the
 * match.
 *
 * `MOV ECX,[EBP+0xc]` is scheduled before the AL store; that is instruction
 * scheduling, not argument order.  Push order is PUSH EAX (value) then PUSH
 * ECX (thread_datum), so under cdecl right-to-left the call is
 * hs_return(thread_datum, value). */
void FUN_000c25b0(int16_t function_index, int thread_datum, char init)
{
  int value;

  value = 0;
  *(bool *)&value = game_is_cooperative();
  hs_return(thread_datum, value);
}

/* 0xc25e0 — HaloScript handler: save the map (safe/deferred variant) and
 * complete the calling script thread with a zero result.
 *
 * Disassembly (whole body):
 *   PUSH EBP; MOV EBP,ESP
 *   CALL 0x100330       ; main_save_map_safe(), no args pushed, no cleanup
 *   MOV EAX,[EBP+0xc]   ; thread_datum
 *   PUSH 0x0            ; hs_return arg2 = value
 *   PUSH EAX            ; hs_return arg1 = thread_datum (cdecl: last PUSH
 *                       ; is the first C argument)
 *   CALL 0xcbf80        ; hs_return
 *   ADD ESP,0x8         ; cdecl cleanup, 2 dwords -> hs_return takes 2 args
 *   POP EBP; RET        ; plain cdecl RET, no RET n
 *
 * [EBP+0x8] (function_index) and [EBP+0x10] (init) are never read by this
 * body; they complete the standard hs-evaluator signature shared by every
 * handler in this TU (a cdecl parameter the callee ignores emits no code, so
 * the disassembly cannot distinguish 2 params from 3 — the sibling handlers
 * arbitrate).  Ghidra mis-prototypes this as void(void) and reports the
 * [EBP+0xc] read as the phantom local `in_stack_00000008`; the kb decl was
 * corrected with this lift.
 *
 * The absence of any cleanup after CALL 0x100330 confirms main_save_map_safe
 * takes no arguments, matching its kb decl.  The `ADD ESP,0x8` is un-merged,
 * so no ARG_COUNT warning is expected on 0xcbf80 from this call site.
 *
 * Callees (both cdecl, no register args):
 *   0x100330 = main_save_map_safe(void)
 *   0xcbf80  = hs_return(thread_handle, value)
 */
void FUN_000c25e0(int16_t function_index, int thread_datum, char init)
{
  main_save_map_safe();
  hs_return(thread_datum, 0);
}

/* 0xc2600 — HaloScript evaluator that cancels an in-progress save, then
 * commits a 0 result to the calling script thread (a void-returning script
 * builtin).
 *
 * Callees (both cdecl, ported, no register args):
 *   0x100320 = main_save_cancel(void) — no args pushed
 *   0xcbf80  = hs_return(thread_handle, value)
 *
 * ABI (verified against disassembly 0xc2600-0xc2617, 24 bytes): cdecl, plain
 * RET, frame is PUSH EBP / MOV EBP,ESP only (no SUB ESP, no FPU, no locals).
 * The single argument read is `MOV EAX,[EBP+0xc]` = thread_datum (arg 2);
 * Ghidra's `in_stack_00000008` label is a decompiler artifact and would
 * wrongly pass function_index here. function_index and init are unused in
 * this body but complete the uniform hs-evaluator signature (matches the
 * sibling evaluators throughout this TU, e.g. 0xc0cb0 and 0xc25e0). */
void FUN_000c2600(int16_t function_index, int thread_datum, char init)
{
  main_save_cancel();
  hs_return(thread_datum, 0);
}

/* 0xc2620 — HaloScript handler: request a map save with the save timeout
 * disabled, then complete the calling script thread with a zero result
 * (a void-returning script builtin).
 *
 * Disassembly (whole body, 0xc2620-0xc2637, 10 instructions):
 *   PUSH EBP; MOV EBP,ESP   ; bare frame — no SUB ESP, so NO locals
 *   CALL 0x101ec0           ; main_save_map_no_timeout(); no args pushed and
 *                           ; no cleanup after → confirms void(void)
 *   MOV EAX,[EBP+0xc]       ; thread_datum (2nd cdecl param)
 *   PUSH 0x0                ; hs_return arg2 = value
 *   PUSH EAX                ; hs_return arg1 = thread_datum — cdecl pushes
 *                           ; right-to-left, so the LAST push is the FIRST
 *                           ; C argument: hs_return(thread_datum, 0)
 *   CALL 0xcbf80            ; hs_return
 *   ADD ESP,0x8             ; un-merged cdecl cleanup, 2 dwords → 2 args
 *   POP EBP; RET            ; plain RET, no RET n → cdecl
 *
 * No FPU ops, no struct access, no locals.  [EBP+0x8] (function_index) and
 * [EBP+0x10] (init) are never read by this body; a cdecl parameter the callee
 * ignores emits no code, so the disassembly alone cannot distinguish 2 params
 * from 3 — the sibling handlers in this TU (0xc25b0/0xc25e0/0xc2600) arbitrate
 * the uniform hs-evaluator triple.  Ghidra mis-prototypes this as void(void)
 * and reports the [EBP+0xc] read as the phantom local `in_stack_00000008`,
 * which would wrongly pass function_index; the kb decl was widened with this
 * lift.
 *
 * Callees (both cdecl, ported, no register args):
 *   0x101ec0 = main_save_map_no_timeout(void)
 *   0xcbf80  = hs_return(thread_handle, value)
 */
void FUN_000c2620(int16_t function_index, int thread_datum, char init)
{
  main_save_map_no_timeout();
  hs_return(thread_datum, 0);
}

/* hs script-function handler: unconditionally saves the map via the nonsafe
 * path, then returns 0 to the calling script thread.  Structural twin of
 * FUN_000c2620 directly above with the save callee swapped from
 * main_save_map_no_timeout to main_save_map_nonsafe; the whole body is 10
 * instructions.
 *
 * Disassembly (0xc2640..0xc2658, complete):
 *   PUSH EBP; MOV EBP,ESP   ; no SUB ESP, no _chkstk → zero locals
 *   CALL 0x100300           ; main_save_map_nonsafe(); no pushes before it and
 *                           ; no cleanup after → confirms void(void)
 *   MOV EAX,[EBP+0xc]       ; thread_datum (2nd cdecl param), NOT [EBP+0x8]
 *   PUSH 0x0                ; hs_return arg2 = value
 *   PUSH EAX                ; hs_return arg1 = thread_datum — cdecl pushes
 *                           ; right-to-left, so the LAST push is the FIRST
 *                           ; C argument: hs_return(thread_datum, 0)
 *   CALL 0xcbf80            ; hs_return
 *   ADD ESP,0x8             ; un-merged cdecl cleanup, 2 dwords → 2 args, all
 *                           ; belonging to hs_return (main_save_map_nonsafe
 *                           ; takes none)
 *   POP EBP; RET            ; plain RET, no RET n → cdecl
 *
 * No FPU ops, no struct access, no locals, no buffers.  [EBP+0x8]
 * (function_index) and [EBP+0x10] (init) are never read by this body; a cdecl
 * parameter the callee ignores emits no code, so the disassembly alone cannot
 * distinguish 2 params from 3 — the sibling handlers in this TU
 * (0xc25e0/0xc2600/0xc2620) arbitrate the uniform hs-evaluator triple.
 * Ghidra mis-prototypes this as void(void) and surfaces the [EBP+0xc] read as
 * the phantom local `in_stack_00000008`; taking that at face value would pass
 * function_index as the thread handle.  The kb decl was widened from
 * `void FUN_000c2640(void);` with this lift.
 *
 * Callees (both cdecl, ported, no register args):
 *   0x100300 = main_save_map_nonsafe(void)
 *   0xcbf80  = hs_return(thread_handle, value)
 */
void FUN_000c2640(int16_t function_index, int thread_datum, char init)
{
  main_save_map_nonsafe();
  hs_return(thread_datum, 0);
}

/* 0xc2660 — HS script function handler: reports whether a map save is
 * currently in progress, returning the boolean to the calling script thread.
 * Reads no macro arguments; the queried state lives entirely in main_*.
 *
 * Ghidra mis-prototypes this as void(void) and surfaces the [EBP+0xc] read as
 * the phantom local `in_stack_00000008`; taking that at face value would pass
 * function_index as the thread handle.  The kb decl was widened from
 * `void FUN_000c2660(void);` with this lift.  The `extraout_AL` Ghidra reports
 * is main_saving_map's bool-in-AL return, NOT a register argument.
 *
 * Callees (both cdecl, no register args, both ported):
 *   0x100310 = main_saving_map(void) -> bool in AL
 *   0xcbf80  = hs_return(thread_handle, value)
 *
 * ABI (verified against disassembly 0xc2660-0xc2687, 15 instructions): cdecl,
 * plain RET, frame is PUSH EBP / MOV EBP,ESP / PUSH ECX (one 4-byte local at
 * EBP-0x4).  The body reads only [EBP+0xc] = thread_datum (arg 2);
 * function_index and init complete the standard hs-evaluator signature shared
 * by the sibling handlers but are unused here.  hs_return's two pushes are
 * PUSH EAX (=[EBP-4], the value) then PUSH ECX (=[EBP+0xc], the thread), so
 * the first PUSH is the last C argument; ADD ESP,0x8 confirms exactly 2 args.
 *
 * The result local is zero-initialised as a full dword (MOV dword [EBP-4],0)
 * *before* the call, and only afterwards is its low byte overwritten with AL
 * (MOV byte [EBP-4],AL) before the whole dword is re-read and pushed (MOV
 * EAX,[EBP-4] / PUSH EAX).  The `*(char *)&value` store reproduces that pair;
 * a direct call-in-argument or a (unsigned char) widen would emit MOVZX
 * instead.  Same idiom as the siblings at 0xc1a00 / 0xc1a30. */
void FUN_000c2660(int16_t function_index, int thread_datum, char init)
{
  int value;

  value = 0;
  *(char *)&value = (char)main_saving_map();
  hs_return(thread_datum, value);
}

/* 0xc2690 — map-revert HaloScript function evaluator.  Runs main_revert_map()
 * for its side effect, then commits a 0 result to the calling script thread (a
 * void-returning script builtin).  Structurally identical to FUN_000c0cb0 at
 * 0xc0cb0 with the side-effect callee swapped.
 *
 * kb.json carried the placeholder decl `void FUN_000c2690(void);`; widened to
 * the standard hs-evaluator triple with this lift, since the body reads the
 * stack argument at [EBP+0xc].
 *
 * Callees (both cdecl, no register args, both ported):
 *   0x1002c0 = main_revert_map(void)
 *   0xcbf80  = hs_return(thread_handle, value)
 *
 * ABI (verified against disassembly 0xc2690-0xc26a8, 10 instructions): cdecl,
 * plain RET, frame is PUSH EBP / MOV EBP,ESP with no locals (no SUB ESP).  The
 * body reads only [EBP+0xc] = thread_datum (arg 2), and does so with a full
 * 32-bit MOV EAX, so the parameter is `int`, never int16 — the LOADW trap that
 * distinguishes the 0xc0c30/0xc0cd0 pair does not apply here.  function_index
 * and init complete the standard hs-evaluator signature shared by the sibling
 * handlers but are unused in this body.  hs_return's pushes are PUSH 0x0 (the
 * value) then PUSH EAX (=[EBP+0xc], the thread), so the first PUSH is the last
 * C argument; ADD ESP,0x8 confirms exactly 2 args. */
void FUN_000c2690(int16_t function_index, int thread_datum, char init)
{
  main_revert_map();
  hs_return(thread_datum, 0);
}

/* 0xc26b0 — core-save/load HaloScript function evaluator.  Runs
 * main_load_core() for its side effect, then commits a 0 result to the calling
 * script thread (a void-returning script builtin).  Structurally identical to
 * FUN_000c2690 directly above and to FUN_000c0cb0 at 0xc0cb0, with the
 * side-effect callee swapped.
 *
 * kb.json carried the placeholder decl `void FUN_000c26b0(void);`; widened to
 * the standard hs-evaluator triple with this lift, since the body reads the
 * stack argument at [EBP+0xc].  Under the void(void) decl Ghidra surfaced the
 * read as the artifact local `in_stack_00000008`.
 *
 * Callees (both cdecl, no register args, both ported):
 *   0x100420 = main_load_core(void)
 *   0xcbf80  = hs_return(thread_handle, value)
 *
 * ABI (verified against disassembly 0xc26b0-0xc26c7, 24 bytes / 9
 * instructions): cdecl, plain RET, frame is PUSH EBP / MOV EBP,ESP with no
 * locals (no SUB ESP, no _chkstk).  The body reads only [EBP+0xc] =
 * thread_datum (arg 2), and does so with a full 32-bit MOV EAX, so the
 * parameter is `int`, never int16 — the LOADW trap that distinguishes the
 * 0xc0c30/0xc0cd0 pair does not apply here.  function_index and init complete
 * the standard hs-evaluator signature shared by the sibling handlers but are
 * unused in this body.  hs_return's pushes are PUSH 0x0 (the value) then PUSH
 * EAX (=[EBP+0xc], the thread), so the first PUSH is the last C argument;
 * ADD ESP,0x8 confirms exactly 2 args.  No FPU ops and no conditional jumps —
 * there is no null-check branch here, unlike the hs_macro_function_evaluate
 * shape at 0xc0c30. */
void FUN_000c26b0(int16_t function_index, int thread_datum, char init)
{
  main_load_core();
  hs_return(thread_datum, 0);
}

/* 0xc26d0 — core-load-at-startup HaloScript function evaluator.  Runs
 * main_load_core_at_startup() for its side effect, then commits a 0 result to
 * the calling script thread (a void-returning script builtin).  Structurally
 * identical to FUN_000c26b0 directly above and to FUN_000c0cb0 at 0xc0cb0,
 * with the side-effect callee swapped.
 *
 * kb.json carried the placeholder decl `void FUN_000c26d0(void);`; widened to
 * the standard hs-evaluator triple with this lift, since the body reads the
 * stack argument at [EBP+0xc].  Under the void(void) decl Ghidra surfaced the
 * read as the artifact local `in_stack_00000008`.
 *
 * Callees (both cdecl, no register args, both ported):
 *   0x100440 = main_load_core_at_startup(void)
 *   0xcbf80  = hs_return(thread_handle, value)
 *
 * ABI (verified against disassembly 0xc26d0-0xc26e7, 24 bytes / 10
 * instructions): cdecl, plain RET, frame is PUSH EBP / MOV EBP,ESP with no
 * locals (no SUB ESP, no _chkstk).  The body reads only [EBP+0xc] =
 * thread_datum (arg 2), and does so with a full 32-bit MOV EAX, so the
 * parameter is `int`, never int16 — the LOADW trap that distinguishes the
 * 0xc0c30/0xc0cd0 pair does not apply here.  function_index and init complete
 * the standard hs-evaluator signature shared by the sibling handlers but are
 * unused in this body.  hs_return's pushes are PUSH 0x0 (the value) then PUSH
 * EAX (=[EBP+0xc], the thread), so the first PUSH is the last C argument;
 * ADD ESP,0x8 confirms exactly 2 args.  No FPU ops and no conditional jumps —
 * there is no null-check branch here, and no call to
 * hs_macro_function_evaluate, so no argument evaluation belongs in this
 * body. */
void FUN_000c26d0(int16_t function_index, int thread_datum, char init)
{
  main_load_core_at_startup();
  hs_return(thread_datum, 0);
}

/* 0xc26f0 — core-load-by-name HaloScript function evaluator.  Evaluates the
 * script function's arguments and, on a non-NULL evaluation record, loads the
 * named core, then commits a 0 result to the calling script thread.  Same
 * shape as FUN_000c1cb0 at 0xc1cb0 (evaluate / null-check / one side-effect
 * callee reading the record / hs_return) with the side-effect callee and the
 * record field width swapped.
 *
 * kb.json carried the placeholder decl `void FUN_000c26f0(void);`; widened to
 * the standard hs-evaluator triple with this lift, since the body reads all
 * three stack slots.  Under the void(void) decl Ghidra surfaced them as the
 * artifact locals in_stack_00000004/8/c — those are ordinary stack parameters,
 * NOT register arguments.
 *
 * ABI (verified against disassembly 0xc26f0-0xc2721): cdecl, plain RET.  Frame
 * is PUSH EBP / MOV EBP,ESP / PUSH ESI — no locals, no SUB ESP, no _chkstk.
 * ESI is the only callee-saved register and caches thread_datum across the
 * whole body so it can be reused as hs_return's first argument.  Slots are
 * [EBP+0x8] = function_index (int16, arrives in ECX), [EBP+0xc] = thread_datum
 * (into ESI), [EBP+0x10] = init (char, arrives in EAX).
 *
 * Callees (all cdecl, no register args, all ported):
 *   0xcc560  = hs_macro_function_evaluate(function_index, thread_datum, init)
 *              — pushes EAX(+0x10), ESI(+0xc), ECX(+0x8), i.e. cdecl
 *              right-to-left, and ADD ESP,0xc confirms 3 args.  kb.json
 *              declares an `int` return but the value is a POINTER to the
 *              evaluated-argument record; TEST EAX,EAX / JZ 0xc271f is a NULL
 *              check, not a boolean test.  Cast locally at the call site as
 *              the siblings do — do not mutate the shared kb decl.
 *   0x100460 = main_load_core_name(const char *name).  Ghidra modelled this as
 *              no-arg and dropped the argument entirely; the disassembly has
 *              MOV EDX,dword ptr [EAX] (a single dereference of record +0, as
 *              a pointer, not a 16-bit field like the 0xc1cb0 sibling) and
 *              then PUSH EDX before the CALL.  Transcribing the decompile
 *              verbatim would have called it with whatever happened to be on
 *              the stack.
 *   0xcbf80  = hs_return(thread_datum, 0) — PUSH 0x0 then PUSH ESI, so the
 *              first PUSH is the last C argument.
 *
 * The trailing ADD ESP,0xc at 0xc271c covers THREE pushes: the one for
 * main_load_core_name plus the two for hs_return, coalesced as ordinary MSVC
 * adjacent-call codegen.  main_load_core_name's single argument is not popped
 * separately.  The ARG_COUNT hazard reported against hs_return (cleanup=3 vs
 * decl=2) is therefore a false positive from mis-grouping that shared cleanup
 * — hs_return really does take 2 args and must not be widened.
 *
 * No FPU ops, no local buffers, and no struct offsets beyond the char* field
 * at record +0. */
void FUN_000c26f0(int16_t function_index, int thread_datum, char init)
{
  char *args;

  args = (char *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (args != (char *)0) {
    main_load_core_name(*(const char **)args);
    hs_return(thread_datum, 0);
  }
  return;
}

/* 0xc2730 — core-load-by-name-at-startup HaloScript function evaluator.
 * Evaluates the script function's arguments and, on a non-NULL evaluation
 * record, requests the named core be loaded at the next startup, then commits
 * a 0 (void) result to the calling script thread.  Structurally identical to
 * the immediately-preceding sibling FUN_000c26f0 at 0xc26f0 (evaluate /
 * null-check / one side-effect callee reading record +0 as a char* / hs_return)
 * with only the side-effect callee swapped from main_load_core_name to
 * main_load_core_name_at_startup.
 *
 * kb.json carried the placeholder decl `void FUN_000c2730(void);`; widened to
 * the standard hs-evaluator triple with this lift, since the body reads all
 * three stack slots.  Under the void(void) decl Ghidra surfaced them as the
 * artifact locals in_stack_00000004/8/c — those are ordinary stack parameters,
 * NOT register arguments, and leaving the no-arg decl in place would have been
 * the void-decl ESP-drift footgun.
 *
 * ABI (verified against disassembly 0xc2730-0xc2762, 23 instructions): cdecl,
 * plain RET.  Frame is PUSH EBP / MOV EBP,ESP / PUSH ESI — no locals, no SUB
 * ESP, no _chkstk.  ESI is the only callee-saved register and caches
 * thread_datum across the whole body so it can be reused as hs_return's first
 * argument.  Slots are [EBP+0x8] = function_index (int16, loaded into ECX),
 * [EBP+0xc] = thread_datum (into ESI), [EBP+0x10] = init (char, into EAX).
 *
 * Callees (all cdecl, no register args, all ported):
 *   0xcc560  = hs_macro_function_evaluate(function_index, thread_datum, init)
 *              — pushes EAX(+0x10), ESI(+0xc), ECX(+0x8), i.e. cdecl
 *              right-to-left, and ADD ESP,0xc at 0xc2745 confirms 3 args.
 *              kb.json declares an `int` return but the value is a POINTER to
 *              the evaluated-argument record; TEST EAX,EAX / JZ 0xc275f is a
 *              NULL check, not a boolean test.  Cast locally at the call site
 *              as the siblings do — do not mutate the shared kb decl.
 *   0x1004b0 = main_load_core_name_at_startup(const char *name).  Ghidra
 *              modelled this as no-arg and dropped the argument entirely; the
 *              disassembly has MOV EDX,dword ptr [EAX] at 0xc274c (a single
 *              dereference of record +0, as a pointer) and then PUSH EDX
 *              before the CALL.  Transcribing the decompile verbatim would
 *              have called it with whatever happened to be on the stack.
 *   0xcbf80  = hs_return(thread_datum, 0) — PUSH 0x0 then PUSH ESI, so the
 *              first PUSH is the last C argument.
 *
 * The trailing ADD ESP,0xc at 0xc275c covers THREE pushes: the one for
 * main_load_core_name_at_startup plus the two for hs_return, coalesced as
 * ordinary MSVC adjacent-call codegen.  The side-effect callee's single
 * argument is not popped separately.  The ARG_COUNT hazard reported against
 * hs_return (cleanup=3 vs decl=2) is therefore a false positive from
 * mis-grouping that shared cleanup — hs_return really does take 2 args and
 * must not be widened.
 *
 * No FPU ops, no local buffers, and no struct offsets beyond the char* field
 * at record +0. */
void FUN_000c2730(int16_t function_index, int thread_datum, char init)
{
  char *args;

  args = (char *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (args != (char *)0) {
    main_load_core_name_at_startup(*(const char **)args);
    hs_return(thread_datum, 0);
  }
  return;
}

/* 0xc2770 — core-save HaloScript function evaluator.  Requests a core save
 * unconditionally and commits a 0 (void) result to the calling script thread.
 * No argument evaluation and no null check: unlike the load-by-name siblings
 * above, this handler takes no script arguments, so the body is just the
 * side-effect callee followed by hs_return.  Structurally identical to
 * FUN_000c0cb0 at 0xc0cb0 with the side-effect callee swapped from
 * FUN_00057c60 to main_save_core.
 *
 * kb.json carried the placeholder decl `void FUN_000c2770(void);`; widened to
 * the standard hs-evaluator triple with this lift, since the body reads
 * [EBP+0xc].  Under the void(void) decl Ghidra surfaced that slot as the
 * artifact local in_stack_00000008 — an ordinary stack parameter, NOT a
 * register argument.  Leaving the no-arg decl in place would have read garbage
 * for thread_datum (the void-decl ESP-drift footgun).
 *
 * ABI (verified against disassembly 0xc2770-0xc2787, 0x18 bytes / 9
 * instructions): cdecl, plain RET.  Frame is PUSH EBP / MOV EBP,ESP with no
 * SUB ESP and no _chkstk — zero locals, and no callee-saved register is
 * spilled.  Do not introduce a temporary here: a spilled local would add a SUB
 * ESP and change the frame shape, which dominates the match on a body this
 * short.  Slots: [EBP+0x8] = function_index and [EBP+0x10] = init are never
 * read and exist only to complete the standard hs-evaluator signature;
 * [EBP+0xc] = thread_datum is loaded into EAX at 0xc2778.
 *
 * Callees (both cdecl, no register args, both ported):
 *   0x1003b0 = main_save_core() — called with no arguments and no stack
 *              cleanup after it, confirming the void(void) decl.
 *   0xcbf80  = hs_return(thread_datum, 0) — PUSH 0x0 then PUSH EAX, so the
 *              first PUSH is the last C argument; ADD ESP,0x8 confirms 2 args.
 *
 * No FPU ops, no local buffers, no struct access. */
void FUN_000c2770(int16_t function_index, int thread_datum, char init)
{
  main_save_core();
  hs_return(thread_datum, 0);
}

/* 0xc2790 — core-save-by-name HaloScript function evaluator.  Evaluates the
 * script function's arguments and, on a non-NULL evaluation record, saves the
 * core under the named file, then commits a 0 (void) result to the calling
 * script thread.  Structurally identical to the load-by-name siblings
 * FUN_000c26f0 at 0xc26f0 and FUN_000c2730 at 0xc2730 (evaluate / null-check /
 * one side-effect callee reading record +0 as a char* / hs_return) with only
 * the side-effect callee swapped to main_save_core_name.
 *
 * kb.json carried the placeholder decl `void FUN_000c2790(void);`; widened to
 * the standard hs-evaluator triple with this lift, since the body reads all
 * three stack slots.  Under the void(void) decl Ghidra surfaced them as the
 * artifact locals in_stack_00000004/8/c — those are ordinary stack parameters,
 * NOT register arguments, and leaving the no-arg decl in place would have been
 * the void-decl ESP-drift footgun.
 *
 * ABI (verified against disassembly 0xc2790-0xc27c1, 0x32 bytes): cdecl, plain
 * RET.  Frame is PUSH EBP / MOV EBP,ESP / PUSH ESI — no locals, no SUB ESP, no
 * _chkstk.  ESI is the only callee-saved register and caches thread_datum
 * across the whole body so it can be reused as hs_return's first argument
 * without re-reading [EBP+0xc].  Slots are [EBP+0x8] = function_index (int16,
 * loaded into ECX), [EBP+0xc] = thread_datum (into ESI), [EBP+0x10] = init
 * (char, loaded as a dword into EAX).
 *
 * Callees (all cdecl, no register args, all ported):
 *   0xcc560  = hs_macro_function_evaluate(function_index, thread_datum, init)
 *              — pushes EAX(+0x10), ESI(+0xc), ECX(+0x8), i.e. cdecl
 *              right-to-left, and ADD ESP,0xc at 0xc27a5 confirms 3 args.
 *              kb.json declares an `int` return but the value is a POINTER to
 *              the evaluated-argument record; TEST EAX,EAX / JZ 0xc27bf is a
 *              NULL check, not a boolean test.  Cast locally at the call site
 *              as the siblings do — do not mutate the shared kb decl.
 *   0x1003d0 = main_save_core_name(const char *name).  Ghidra modelled this as
 *              no-arg and dropped the argument entirely; the disassembly has
 *              MOV EDX,dword ptr [EAX] at 0xc27ac (a single dereference of
 *              record +0, as a pointer) and then PUSH EDX before the CALL.
 *              Transcribing the decompile verbatim would have called it with
 *              whatever happened to be on the stack, or passed the record
 *              address in place of the string it points at.
 *   0xcbf80  = hs_return(thread_datum, 0) — PUSH 0x0 then PUSH ESI, so the
 *              first PUSH is the last C argument.
 *
 * The trailing ADD ESP,0xc at 0xc27bc covers THREE pushes: the one for
 * main_save_core_name plus the two for hs_return, coalesced as ordinary MSVC
 * adjacent-call codegen.  main_save_core_name's single argument is not popped
 * separately.  The ARG_COUNT hazard reported against hs_return (cleanup=3 vs
 * decl=2) is therefore a false positive from mis-grouping that shared cleanup
 * — hs_return really does take 2 args and must not be widened.
 *
 * No FPU ops, no local buffers, and no struct offsets beyond the char* field
 * at record +0. */
void FUN_000c2790(int16_t function_index, int thread_datum, char init)
{
  char *args;

  args = (char *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (args != (char *)0) {
    main_save_core_name(*(const char **)args);
    hs_return(thread_datum, 0);
  }
  return;
}

/* 0xc27d0 — HS script function handler: main-loop skip request. Structural
 * twin of 0xc1050/0xc1090 (identical codegen; differs only in the dispatch
 * callee). Drives hs_macro_function_evaluate to evaluate the call's argument
 * expressions; when the values array is ready (non-null return), reads the
 * first evaluated value as a zero-extended 16-bit quantity (original:
 * XOR EDX,EDX; MOV DX,word ptr [EAX], so the low 16 bits are the payload and
 * the value widens unsigned to int) and passes it to main_skip at 0x100560,
 * then commits a zero result to the thread via hs_return. While arguments are
 * still being evaluated the return is null and nothing is dispatched.
 *
 * ABI (verified against disassembly 0xc27d0-0xc2805): cdecl, plain RET. Frame
 * is PUSH EBP / MOV EBP,ESP / PUSH ESI; the params are pure stack slots that
 * Ghidra drops entirely (it reports `void FUN_000c27d0(void)` with
 * in_stack_* locals):
 *   [EBP+0x8]  = function_index (int16), loaded into ECX
 *   [EBP+0xc]  = thread_datum, held in ESI across both calls
 *   [EBP+0x10] = init (char), loaded into EAX
 *   0xcc560 = hs_macro_function_evaluate — PUSH EAX / PUSH ESI / PUSH ECX,
 *             right-to-left, and ADD ESP,0xc confirms 3 args.
 *   0x100560 = main_skip — ONE stack arg (PUSH EDX after the zero-extending
 *             word load). Ghidra shows a bare `FUN_00100560()` and kb.json
 *             declared `void main_skip(void)`; both are wrong, and calling it
 *             as (void) would leave the callee reading whatever was on the
 *             stack. The decl is widened here with that push as the evidence.
 *   0xcbf80 = hs_return(thread_datum, 0) — PUSH 0x0 then PUSH ESI, so the
 *             first PUSH is the last C argument.
 *
 * The trailing ADD ESP,0xc at 0xc27ff covers THREE pushes: the one for
 * main_skip plus the two for hs_return, coalesced as ordinary MSVC
 * adjacent-call codegen. main_skip's single argument is not popped separately,
 * which also proves it is cdecl and not stdcall. The ARG_COUNT hazard reported
 * against hs_return (cleanup=3 vs decl=2) is therefore a false positive from
 * mis-grouping that shared cleanup — hs_return really does take 2 args and
 * must not be widened.
 *
 * No FPU ops, no local buffers, and no struct offsets beyond the int16 field
 * at record +0. */
void FUN_000c27d0(int16_t function_index, int thread_datum, char init)
{
  unsigned short *result;

  result = (unsigned short *)hs_macro_function_evaluate(function_index,
                                                        thread_datum, init);
  if (result != (unsigned short *)0x0) {
    main_skip(*result);
    hs_return(thread_datum, 0);
  }
}

/* 0xc2810 — HS script function handler: query whether the game state was
 * reverted (the player just triggered a saved-game revert), and commit that
 * boolean back to the calling script thread.
 *
 * Callees (both cdecl, ported):
 *   0x1bf9e0 = game_state_reverted(void) -> bool in AL, no arguments
 *   0xcbf80  = hs_return(thread_handle, value)
 *
 * ABI (verified against disassembly 0xc2810-0xc2837, 15 instructions): cdecl,
 * plain RET, frame is PUSH EBP; MOV EBP,ESP; PUSH ECX — exactly one 4-byte
 * local, no `sub esp`, no buffers, no FPU, no _chkstk, no register args. The
 * body reads only [EBP+0xc] = thread_datum (arg 2); function_index
 * ([EBP+0x8]) and init ([EBP+0x10]) are never read but complete the uniform
 * hs-evaluator dispatch signature shared with the ported neighbours at
 * 0xc2400-0xc27d0.
 *
 * Match-sensitive shape: the local is zeroed as a full dword
 * (MOV dword [EBP-4],0) BEFORE the call, and only its low byte receives AL
 * (MOV byte [EBP-4],AL); it is then re-read as a full dword
 * (MOV EAX,[EBP-4]).  The union below reproduces that narrow-store-over-
 * zeroed-dword pair exactly — the same idiom already used at 0xc1420. A plain
 * `bool` local would emit a MOVZX widening read and drop the zeroing store.
 *
 * Push order at the hs_return call (PUSH EAX = value, then PUSH ECX =
 * [EBP+0xc], ADD ESP,0x8) confirms hs_return(thread_datum, value), not
 * (value, thread_datum).
 *
 * kb note: the prior decl was the placeholder `void FUN_000c2810(void);`,
 * which contradicts the MOV ECX,[EBP+0xc] at 0xc2820; it is corrected to the
 * 3-argument evaluator signature with this lift. */
void FUN_000c2810(int16_t function_index, int thread_datum, char init)
{
  union {
    int i;
    bool b;
  } value;

  value.i = 0;
  value.b = game_state_reverted();
  hs_return(thread_datum, value.i);
}

/* 0xc2840 — HS script function handler: start a scripted sound.
 * Evaluates the macro arguments; on success the result block holds two full
 * dwords at +0x0/+0x4 and a float at +0x8 (FLD dword [EAX+8]).  Calls
 * scripted_sound_new(a, b, f) then returns void to the calling script thread
 * via hs_return(thread_datum, 0).
 *
 * ABI (verified against disassembly 0xc2840-0xc287c, 29 instructions): cdecl,
 * plain RET, frame is PUSH EBP; MOV EBP,ESP; PUSH ESI — no locals, no
 * _chkstk, no SEH.  thread_datum ([EBP+0xc]) is held in ESI across both
 * calls.
 *
 * Match-sensitive shape: the third argument to scripted_sound_new is a FLOAT,
 * passed with the MSVC push-then-fstp idiom (PUSH ECX as a dummy slot, then
 * FSTP dword [ESP]).  ECX at that point is scratch, not an argument — Ghidra
 * dropped all three arguments because the kb decl for 0x1c7f80 claimed
 * `void (void)`; that decl is widened to (int,int,float) with this lift.  A
 * float declared as int there would emit FILD and truncate the value.
 *
 * Note the original coalesces both callee cleanups into one `add esp,0x14` at
 * 0xc2877 (0xc for scripted_sound_new + 0x8 for hs_return); a naive cdecl
 * reading of that single cleanup mis-sizes either call's argument list.
 *
 * kb note: the prior decl was the placeholder `void FUN_000c2840(void);`,
 * which contradicts the reads of [EBP+0x8]/[EBP+0xc]/[EBP+0x10]; it is
 * corrected to the 3-argument evaluator signature shared with the ported
 * neighbours at 0xc2810/0xc2880. */
void FUN_000c2840(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    scripted_sound_new(result[0], result[1], *(float *)(result + 2));
    hs_return(thread_datum, 0);
  }
}

/* 0xc2880 — HS script function handler: scripted-sound time query.
 * Evaluates the macro arguments; on success the result block holds a single
 * handle at +0x0, read as a full dword (MOV EDX,[EAX]) — there is no narrow
 * +0x4 field like the 0xc0c30/0xc0c70 twins have, so the block is one handle.
 * Calls scripted_sound_time(handle) and commits its EAX return to the calling
 * script thread via hs_return(thread_datum, value).
 *
 * The value is computed inline immediately before its PUSH in the original, so
 * it is nested in the hs_return argument rather than spilled to a temporary.
 * Note the original coalesces both callee cleanups into one `add esp,0xc` at
 * 0xc28ab; a naive cdecl reading of that makes hs_return look like it takes 3
 * stack args, but it takes 2. */
void FUN_000c2880(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    hs_return(thread_datum, scripted_sound_time(result[0]));
  }
}

/* 0xc28c0 — HS script function handler: stop a scripted sound.
 * Evaluates the macro arguments; on success the result block holds a single
 * handle at +0x0, read as a full dword (MOV EDX,[EAX]) — the +0x4 type byte
 * that the 0xc0ed0 twin reads is not touched here, so the block is one handle.
 * Calls scripted_sound_stop(handle), then commits a zero return to the calling
 * script thread.
 *
 * ABI (verified against disassembly 0xc28c0-0xc28f1, 50 bytes): cdecl, frame
 * is PUSH EBP; MOV EBP,ESP; PUSH ESI with no locals and no `sub esp`.  ESI
 * holds thread_datum ([EBP+0xc]) across both calls.  Ghidra's
 * `void FUN_000c28c0(void)` prototype is wrong — the three `in_stack_*`
 * phantoms are [EBP+8]/[EBP+0xc]/[EBP+0x10], the standard hs-evaluator triple;
 * the push order is PUSH EAX(init); PUSH ESI(thread_datum); PUSH ECX(index).
 *
 * Ghidra also printed `FUN_001c7550()` with no argument; the disassembly is
 * `MOV EDX,[EAX]; PUSH EDX`, so the record's first dword is passed.  kb.json
 * declared 0x1c7550 as `void scripted_sound_stop(void)`; the callee's own
 * prologue reads [EBP+8] (it forwards that dword to 0x1c3bb0 twice), so the
 * declaration is widened to take the handle — matching the sibling
 * scripted_sound_time(int handle) at 0x1c7500.
 *
 * The single `add esp,0xc` at 0xc28ec is MSVC's merged cleanup for BOTH the
 * 1-argument scripted_sound_stop call and the 2-argument hs_return call
 * (1 + 2 = 3 dwords) — it is not evidence of a 3-argument hs_return.
 *
 * hs_macro_function_evaluate is declared returning `int` in kb.json but is
 * used here as a pointer, so it is cast (same as FUN_000c2840/FUN_000c2880). */
void FUN_000c28c0(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    scripted_sound_stop(result[0]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc2900 — HS script function handler: predict (pre-roll) a scripted foley.
 * Structurally identical to the 0xc28c0 twin above: evaluate the macro
 * argument list, and on success read a single full dword from the result
 * block (MOV EDX,[EAX]) and forward it to scripted_foley_predict, then commit
 * a zero return to the calling script thread.  No +0x4 field is touched, so
 * the argument block is one dword wide.
 *
 * ABI (verified against disassembly 0xc2900-0xc2931, 50 bytes): cdecl, plain
 * RET, frame is PUSH EBP; MOV EBP,ESP; PUSH ESI — no locals, no `sub esp`, no
 * _chkstk, no SEH.  ESI holds thread_datum ([EBP+0xc]) across both calls.
 * Ghidra's `void FUN_000c2900(void)` prototype is wrong: the three
 * `in_stack_*` phantoms are [EBP+8]/[EBP+0xc]/[EBP+0x10], the standard
 * hs-evaluator triple.  Push order at the evaluator call is
 * PUSH EAX([EBP+0x10]=init); PUSH ESI([EBP+0xc]=thread_datum);
 * PUSH ECX([EBP+8]=function_index) — cdecl right-to-left, so the C argument
 * order is (function_index, thread_datum, init).
 *
 * Ghidra printed `FUN_001c75a0()` with no argument; the disassembly is
 * `MOV EDX,[EAX]; PUSH EDX`, so the record's first dword is passed.  kb.json
 * declared 0x1c75a0 as `void scripted_foley_predict(void)`; that decl is
 * widened to take the dword with this lift, exactly as 0x1c7550
 * (scripted_sound_stop) was widened for the 0xc28c0 twin.  Calling it as
 * (void) with a stray PUSH would desync the stack shape.
 *
 * The single `add esp,0xc` at 0xc292c is MSVC's merged cleanup for BOTH the
 * 1-argument scripted_foley_predict call and the 2-argument hs_return call
 * (1 + 2 = 3 dwords) — it is not evidence of a 3-argument hs_return.
 *
 * hs_macro_function_evaluate is declared returning `int` in kb.json but its
 * EAX result is dereferenced here, so it is cast to a pointer (same as
 * FUN_000c2840/FUN_000c2880/FUN_000c28c0). */
void FUN_000c2900(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    scripted_foley_predict(result[0]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc2940 — HS script function handler: start a looping sound.
 * Evaluates the macro arguments; on success the result block holds two full
 * dwords at +0x0/+0x4 and a float at +0x8 (FLD dword ptr [EAX+8]).  Calls
 * sound_looping_start(a, b, f) then returns void to the calling script
 * thread via hs_return(thread_datum, 0).  Direct twin of the 0xc2840
 * scripted_sound_new handler — same block layout, same call shape.
 *
 * ABI (verified against disassembly 0xc2940-0xc297d, 29 instructions): cdecl,
 * plain RET, frame is PUSH EBP; MOV EBP,ESP; PUSH ESI — no locals, no
 * `sub esp`, no _chkstk, no SEH.  ESI holds thread_datum ([EBP+0xc]) across
 * both calls.  Ghidra's `void FUN_000c2940(void)` prototype is wrong: its
 * three `in_stack_*` phantoms are [EBP+8]/[EBP+0xc]/[EBP+0x10], the standard
 * hs-evaluator triple.  Push order at the evaluator call is
 * PUSH EAX([EBP+0x10]=init); PUSH ESI([EBP+0xc]=thread_datum);
 * PUSH ECX([EBP+8]=function_index) — cdecl right-to-left, so the C argument
 * order is (function_index, thread_datum, init).
 *
 * Match-sensitive shape: the third argument to sound_looping_start is a
 * FLOAT already stored as a float in the result block, passed with the MSVC
 * push-then-fstp idiom (`PUSH ECX` reserves a dummy slot, then
 * `FSTP dword ptr [ESP]` overwrites it).  ECX there is scratch, not an
 * argument.  Ghidra rendered this as `(float)piVar1[2]`, an int-to-float
 * conversion that would emit FILD and turn the raw IEEE bits into a garbage
 * scale; the binary does FLD on a dword float, so it must be read as
 * `*(float *)(result + 2)`.
 *
 * The single `add esp,0x14` in the epilogue is MSVC's merged cleanup for
 * BOTH calls (0xc for sound_looping_start + 0x8 for hs_return); a naive cdecl
 * reading of that one cleanup makes hs_return look like it takes 5 stack
 * args, but it takes 2. */
void FUN_000c2940(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    sound_looping_start(result[0], result[1], *(float *)(result + 2));
    hs_return(thread_datum, 0);
  }
}

/* 0xc2980 — HS script function handler: stop a looping sound.
 * Evaluates the macro arguments; on success the result block's first dword
 * (+0x0) is the looping-sound tag index.  Calls sound_looping_stop(tag) then
 * returns void to the calling script thread via hs_return(thread_datum, 0).
 * Same single-dword-argument shape as the 0xc2900 scripted_foley_predict
 * handler, and the natural counterpart of the 0xc2940 sound_looping_start
 * handler directly above.
 *
 * ABI (verified against disassembly 0xc2980-0xc29b1, 50 bytes): cdecl, plain
 * RET, frame is PUSH EBP; MOV EBP,ESP; PUSH ESI — no locals, no `sub esp`, no
 * _chkstk, no SEH, no FPU.  ESI holds thread_datum ([EBP+0xc]) across both
 * calls.  Ghidra's `void FUN_000c2980(void)` prototype is wrong: its three
 * `in_stack_*` phantoms are [EBP+8]/[EBP+0xc]/[EBP+0x10], the standard
 * hs-evaluator triple.  Push order at the evaluator call is
 * PUSH EAX([EBP+0x10]=init); PUSH ESI([EBP+0xc]=thread_datum);
 * PUSH ECX([EBP+8]=function_index) — cdecl right-to-left, so the C argument
 * order is (function_index, thread_datum, init), cleaned with `add esp,0xc`.
 *
 * Match-sensitive shape: the evaluator's return value is DEREFERENCED before
 * the call — `MOV EDX,[EAX]; PUSH EDX` — so sound_looping_stop receives
 * result[0], not result.  Passing the pointer itself would compile cleanly and
 * silently stop a garbage tag index.
 *
 * The single `add esp,0xc` at 0xc29ac is MSVC's merged cleanup for BOTH calls
 * (0x4 for sound_looping_stop + 0x8 for hs_return); a naive cdecl reading of
 * that one cleanup makes hs_return look like it takes 3 stack args, but it
 * takes 2.  The call-site audit's ARG_COUNT warning here is that false
 * positive. */
void FUN_000c2980(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    sound_looping_stop(result[0]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc29c0 — HS script function handler: set a scripted looping sound's scale.
 * Third member of the looping-sound handler run (0xc2940 start, 0xc2980 stop,
 * this one set-scale).  Evaluates the macro arguments; on success the result
 * block is a {int handle; float scale} pair — verified against disassembly
 * 0xc29c0-0xc29f8 (25 instructions): `FLD dword ptr [EAX+4]; MOV EDX,[EAX];
 * PUSH ECX; FSTP dword ptr [ESP]; PUSH EDX; CALL 0x1c7650`.  The +0x4 read is
 * a true float lvalue (FLD on a dword float), NOT an int->float numeric
 * conversion; writing `(float)result[1]` would emit FILD and turn the raw
 * IEEE-754 bits into a garbage scale.
 *
 * ABI (verified against the same disassembly): cdecl, plain RET, frame is
 * PUSH EBP; MOV EBP,ESP; PUSH ESI — no locals, no `sub esp`, no _chkstk, no
 * SEH.  ESI holds thread_datum ([EBP+0xc]) across both calls.  Ghidra's
 * `void FUN_000c29c0(void)` prototype is wrong: its three `in_stack_*`
 * phantoms are [EBP+8]/[EBP+0xc]/[EBP+0x10], the standard hs-evaluator
 * triple.  Push order at the evaluator call is PUSH EAX([EBP+0x10]=init);
 * PUSH ESI([EBP+0xc]=thread_datum); PUSH ECX([EBP+8]=function_index) —
 * cdecl right-to-left, so the C argument order is (function_index,
 * thread_datum, init).
 *
 * Match-sensitive shape: the scale argument uses the MSVC push-then-fstp
 * idiom (`PUSH ECX` reserves a dummy slot, then `FSTP dword ptr [ESP]`
 * overwrites it).  ECX there is scratch, not an argument — Ghidra renders
 * the call with ZERO arguments, and kb.json's old
 * `void scripted_looping_sound_set_scale(void)` decl matched that mistake;
 * the real callee is `void(int, float)`.
 *
 * The single `add esp,0x10` in the epilogue is MSVC's merged cleanup for BOTH
 * calls (0x8 for scripted_looping_sound_set_scale + 0x8 for hs_return); a
 * naive cdecl reading of that one cleanup makes hs_return look like it takes
 * 4 stack args, but it takes 2.  The call-site audit's ARG_COUNT warning here
 * is that false positive.
 *
 * Structural ~94% ceiling shared with the 0xc0d10/0xc2940 float twins: our
 * VC71 /O2 build copies the untouched float argument via integer MOV/PUSH
 * instead of the original's FLD/FSTP — bit-exact either way. */
void FUN_000c29c0(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    scripted_looping_sound_set_scale(result[0], *(float *)(result + 1));
    hs_return(thread_datum, 0);
  }
}

/* HaloScript handler shim for the looping-sound "set alternate" macro
 * function — the boolean twin of FUN_000c29c0 (set-scale) directly above.
 * Evaluates the macro arguments; on a non-null result block the pair is
 * {int looping_sound_handle; bool alternate}, then returns 0 to the script
 * thread.
 *
 * Verified against disassembly 0xc2a00-0xc2a37 (22 instructions):
 *   xor edx,edx ; mov dl, byte ptr [eax+4] ; mov eax, dword ptr [eax]
 *   push edx ; push eax ; call 0x1c76c0
 * The +0x4 field is a ZERO-extended BYTE (xor/mov dl), not a sign-extended
 * byte and not a dword — reading it as `result[1]` would be a LOADW-class
 * field-width bug.  arg2 (the byte) is materialized before arg1, matching
 * MSVC's right-to-left cdecl evaluation.
 *
 * ABI: cdecl, plain RET, frame is PUSH EBP; MOV EBP,ESP; PUSH ESI — no
 * locals, no `sub esp`, no _chkstk, no FPU.  ESI holds thread_datum
 * ([EBP+0xc]) across both trailing calls.  Ghidra's `void FUN_000c2a00(void)`
 * prototype is wrong: its three `in_stack_*` phantoms are [EBP+8] /
 * [EBP+0xc] / [EBP+0x10], the standard hs-evaluator triple.  Push order at
 * the evaluator is PUSH EAX([EBP+0x10]=init); PUSH ESI([EBP+0xc]=
 * thread_datum); PUSH ECX([EBP+8]=function_index), so the C argument order
 * is (function_index, thread_datum, init).
 *
 * kb.json's old `void scripted_looping_sound_set_alternate(void)` decl was
 * wrong and had to be widened before this compiles: 0x1c76c0 reads
 * [EBP+8] as a dword handle (compared against -1 = NONE) and [EBP+0xc] as a
 * BYTE (`mov cl,byte ptr [ebp+0xc]; test cl,cl`), then sets or clears flag
 * bit 0x8 on the resolved 'lsnd' (0x6c736e64) instance — hence `bool`.
 *
 * The single `add esp,0x10` in the epilogue is MSVC's merged cleanup for
 * BOTH calls (0x8 for set_alternate + 0x8 for hs_return); a naive cdecl
 * reading of that one cleanup makes hs_return look like it takes 4 stack
 * args, but it takes 2.  The call-site audit's ARG_COUNT warning here is
 * that false positive. */
void FUN_000c2a00(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    scripted_looping_sound_set_alternate(result[0], *(bool *)(result + 1));
    hs_return(thread_datum, 0);
  }
}

/* HaloScript handler shim for the "debug sound classes" macro function.
 * Evaluates the macro arguments; on a non-null result block the pair is
 * {char *pattern; char enable} — unlike the encounter/looping-sound
 * siblings above, the +0x0 field is a STRING pointer, not an int handle
 * (`mov eax, dword ptr [eax]` feeding the first push of a routine whose
 * first parameter is a name-substring pattern).  Then returns 0 to the
 * script thread.
 *
 * Verified against disassembly 0xc2a40-0xc2a77 (27 instructions):
 *   xor edx,edx ; mov dl, byte ptr [eax+4] ; mov eax, dword ptr [eax]
 *   push edx ; push eax ; call 0x1c8a40
 * The +0x4 field is a ZERO-extended BYTE (xor/mov dl), not a dword —
 * reading it as `result[1]` would be a LOADW-class field-width bug.  The
 * sibling FUN_000c0c30 reads int16 at this same offset and FUN_000c0c70
 * reads a byte; the width is per-handler and must come from this
 * function's own disassembly.
 *
 * ABI: cdecl, plain RET, frame is PUSH EBP; MOV EBP,ESP; PUSH ESI — no
 * locals, no `sub esp`, no _chkstk, no FPU.  ESI holds thread_datum
 * ([EBP+0xc]) across both trailing calls.  Ghidra's `void FUN_000c2a40(void)`
 * prototype is wrong: its three `in_stack_*` phantoms are [EBP+8] /
 * [EBP+0xc] / [EBP+0x10], the standard hs-evaluator triple.  Push order at
 * the evaluator is PUSH EAX([EBP+0x10]=init); PUSH ESI([EBP+0xc]=
 * thread_datum); PUSH ECX([EBP+8]=function_index), so the C argument order
 * is (function_index, thread_datum, init).
 *
 * The single `add esp,0x10` at 0xc2a72 is MSVC's merged cleanup for BOTH
 * calls (0x8 for debug_sound_classes_enable + 0x8 for hs_return); a naive
 * cdecl reading of that one cleanup makes hs_return look like it takes 4
 * stack args, but it takes 2.  The call-site audit's ARG_COUNT warning
 * here is that false positive. */
void FUN_000c2a40(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    debug_sound_classes_enable((char *)result[0], *(char *)(result + 1));
    hs_return(thread_datum, 0);
  }
}

/* 0xc2ad0 — HS script function handler: set the wet (reverb send) level for
 * the sound classes matching a name pattern.  Evaluates the macro arguments;
 * on success the result block holds a char* pattern string at +0x0 and a
 * float wet level at +0x4.  Calls debug_sound_classes_set_wet(pattern, wet)
 * then returns void to the HS thread via hs_return(thread_datum, 0).
 *
 * Verified against disassembly 0xc2ad0-0xc2b08 (0x39 bytes):
 *   fld dword ptr [eax+4] ; mov edx, dword ptr [eax]
 *   push ecx ; fstp dword ptr [esp] ; push edx ; call 0x1c8ae0
 * The `push ecx` is a DUMMY slot reservation immediately overwritten by
 * `fstp dword ptr [esp]` — this is MSVC's float-argument idiom.  Ghidra
 * rendered it `FUN_001c8ae0((char *)*puVar1,(float)puVar1[1])`; writing
 * `(float)result[1]` in C would emit FILD (integer->float conversion) and
 * silently corrupt the value.  The correct form is the raw dword float load
 * `*(float *)(result + 1)` (lift-learnings §6, float-smuggling).  The FPU_ARG
 * hazard reported by the call-site audit is that real trap, handled here.
 *
 * ABI: cdecl, plain RET, frame is PUSH EBP; MOV EBP,ESP; PUSH ESI — no
 * locals, no `sub esp`, no _chkstk, and the only x87 use is the FLD/FSTP
 * argument pass (no arithmetic).  ESI holds thread_datum ([EBP+0xc]) across
 * both trailing calls.  Ghidra's `void FUN_000c2ad0(void)` prototype is
 * wrong: its three `in_stack_*` phantoms are [EBP+8] / [EBP+0xc] /
 * [EBP+0x10], the standard hs-evaluator triple.  Push order at the evaluator
 * is PUSH EAX([EBP+0x10]=init); PUSH ESI([EBP+0xc]=thread_datum); PUSH
 * ECX([EBP+8]=function_index), so the C argument order is (function_index,
 * thread_datum, init).
 *
 * The single `add esp,0x10` at 0xc2b03 is MSVC's merged cleanup for BOTH
 * calls (0x8 for debug_sound_classes_set_wet + 0x8 for hs_return); a naive
 * cdecl reading of that one cleanup makes hs_return look like it takes 4
 * stack args, but it takes 2.  The call-site audit's ARG_COUNT warning here
 * is that false positive (same as FUN_000c22a0 / FUN_000c2a40). */
void FUN_000c2ad0(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    debug_sound_classes_set_wet((char *)result[0], *(float *)(result + 1));
    hs_return(thread_datum, 0);
  }
}

/* HaloScript handler shim for the "set music volume" macro function — the
 * three-field cousin of FUN_000c2ad0 (debug_sound_classes_set_wet) directly
 * above.  Evaluates the macro arguments; on a non-null result block the
 * triple is {const char *sound_name; float volume; uint16 transition_ticks},
 * then returns 0 to the script thread.
 *
 * Verified against disassembly 0xc2b10-0xc2b4f (32 instructions):
 *   mov eax,[eax]                    ; +0x0 -> sound_name (char *)
 *   fld  dword ptr [eax+4]           ; +0x4 -> volume, a FLOAT LOAD
 *   xor edx,edx ; mov dx,[eax+8]     ; +0x8 -> ZERO-extended 16-bit ticks
 *   push edx ; push ecx ; fstp dword ptr [esp] ; push eax ; call 0x1c8c80
 * Two field-width traps here.  (1) Ghidra renders +0x4 as `(float)puVar1[1]`,
 * an int-to-float CONVERSION; the instruction is `fld dword ptr`, a raw load,
 * so the lift must bit-reinterpret (`*(float *)(result + 1)`).  Writing the
 * cast literally would emit FILD and silently scale the music volume by
 * ~2^23 — no assert, no VC71 delta, box-only oracle (lift-learnings §6).
 * (2) +0x8 is zero-extended (xor/mov dx), so it is read through uint16_t;
 * a MOVSX would be a LOADW-class field-width bug.  The `push ecx` before the
 * FSTP is MSVC's push-then-fstp float idiom: ECX is a stale scratch dummy
 * whose slot the FSTP overwrites, NOT a fourth argument.
 *
 * ABI: cdecl, plain RET, frame is PUSH EBP; MOV EBP,ESP; PUSH ESI — no
 * locals, no `sub esp`, no _chkstk.  ESI holds thread_datum ([EBP+0xc])
 * across both trailing calls.  Ghidra's `void FUN_000c2b10(void)` prototype
 * is wrong: its three `in_stack_*` phantoms are [EBP+8] / [EBP+0xc] /
 * [EBP+0x10], the standard hs-evaluator triple.  Push order at the evaluator
 * is PUSH EAX([EBP+0x10]=init); PUSH ESI([EBP+0xc]=thread_datum); PUSH
 * ECX([EBP+8]=function_index), so the C argument order is (function_index,
 * thread_datum, init).
 *
 * The single `add esp,0x14` at 0xc2b4a is MSVC's merged cleanup for BOTH
 * calls (0xc for game_sound_set_music_volume + 0x8 for hs_return); a naive
 * cdecl reading of that one cleanup makes hs_return look like it takes 5
 * stack args, but it takes 2.  The call-site audit's ARG_COUNT warning here
 * is that false positive (same as FUN_000c2ad0 / FUN_000c29c0). */
void FUN_000c2b10(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    game_sound_set_music_volume((const char *)result[0], *(float *)(result + 1),
                                *(uint16_t *)(result + 2));
    hs_return(thread_datum, 0);
  }
}

/* 0xc2b50 — HS script function handler: enable or disable sound output.
 * Evaluates the macro arguments; on success the result block holds a boolean
 * byte at +0x0.  Calls sound_enable(value) then returns void to the HS thread
 * via hs_return(thread_datum, 0).  The +0x0 read is a zero-extended byte load
 * (XOR EDX,EDX / MOV DL,[EAX] / PUSH EDX), hence the unsigned bool cast. */
void FUN_000c2b50(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    sound_enable(*(bool *)result);
    hs_return(thread_datum, 0);
  }
}

/* 0xc2b90 — HS script function handler: apply a byte-valued setting to a
 * handle.  Evaluates the macro arguments; on success the result block holds a
 * handle at +0x0 (int) and a byte value at +0x4.  Calls
 * FUN_001b5610(handle, value) then returns void to the HS thread via
 * hs_return(thread_datum, 0).  The +0x4 read is a ZERO-extended byte load
 * (XOR EDX,EDX / MOV DL,[EAX+4]) — unsigned char, matching the callee's
 * uint8_t param; the signed `char` used by sibling FUN_000c0c70 would
 * sign-extend.  result is int*, so (result + 1) = +4 bytes.
 *
 * ABI (verified against disassembly 0xc2b90-0xc2bc8): cdecl, plain RET, no
 * locals/FPU/SEH.  The single `ADD ESP,0x10` after the two 2-arg calls is a
 * merged cdecl cleanup (8+8), not a 4-arg call — do not widen hs_return. */
void FUN_000c2b90(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    FUN_001b5610(result[0], *(unsigned char *)(result + 1));
    hs_return(thread_datum, 0);
  }
}

/* HaloScript handler: evaluate the macro function's single argument block and
 * forward its first byte to scripted_show_hud, returning that call's result to
 * the calling script thread.
 *
 * Argument deref is a zero-extended BYTE load (`XOR EDX,EDX ; MOV DL,[EAX]`),
 * so the argument block's first field is read as unsigned char, not a dword.
 *
 * The return value is staged through a 4-byte slot at EBP-4 that MSVC zeroes
 * BEFORE the evaluate call (`MOV dword [EBP-4],0`, scheduled between the arg
 * pushes and the CALL), then overwrites only its low byte with AL
 * (`MOV byte [EBP-4],AL`), then reloads in full (`MOV EAX,dword [EBP-4]`) to
 * pass to hs_return.  The union models that byte-into-zeroed-dword shape; a
 * plain `hs_return(thread_datum, scripted_show_hud(*result))` would drop the
 * pre-call zeroing and change the extension width.
 *
 * ABI (verified against disassembly 0xc2bd0-0xc2c12): cdecl, ESI holds
 * thread_datum across the body, plain RET.  The trailing `ADD ESP,0xc` is a
 * merged cdecl cleanup covering the 1-arg scripted_show_hud call plus the
 * 2-arg hs_return call (4+8) — do not read it as a 3-arg hs_return. */
void FUN_000c2bd0(int16_t function_index, int thread_datum, char init)
{
  union {
    char boolean_value;
    int long_value;
  } value;
  int *result;

  value.long_value = 0;
  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    value.boolean_value = scripted_show_hud(*(unsigned char *)result);
    hs_return(thread_datum, value.long_value);
  }
}

/* 0xc2c20 — HaloScript handler: evaluate the macro function's single argument
 * block and forward its first byte to scripted_show_hud_help_text, returning
 * that call's result to the calling script thread.  Structurally identical to
 * the immediately preceding handler FUN_000c2bd0, differing only in the callee.
 *
 * Argument deref is a zero-extended BYTE load (`XOR EDX,EDX ; MOV DL,[EAX]`),
 * so the argument block's first field is read as unsigned char, not a dword.
 *
 * The return value is staged through a 4-byte slot at EBP-4 that MSVC zeroes
 * BEFORE the evaluate call (`MOV dword [EBP-4],0` at 0xc2c31, scheduled among
 * that call's arg pushes), then overwrites only its low byte with AL
 * (`MOV byte [EBP-4],AL`), then reloads in full (`MOV EAX,dword [EBP-4]`) to
 * pass to hs_return.  The union models that byte-into-zeroed-dword shape; a
 * plain `hs_return(thread_datum, scripted_show_hud_help_text(*result))` would
 * drop the pre-call zeroing and change the extension width.
 *
 * ABI (verified against disassembly 0xc2c20-0xc2c62): cdecl, ESI holds
 * thread_datum across the body, plain RET.  The trailing `ADD ESP,0xc` at
 * 0xc2c5b is a merged cdecl cleanup covering the 1-arg
 * scripted_show_hud_help_text call plus the 2-arg hs_return call (4+8) — do
 * not read it as a 3-arg hs_return. */
void FUN_000c2c20(int16_t function_index, int thread_datum, char init)
{
  union {
    char boolean_value;
    int long_value;
  } value;
  int *result;

  value.long_value = 0;
  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    value.boolean_value = scripted_show_hud_help_text(*(unsigned char *)result);
    hs_return(thread_datum, value.long_value);
  }
}

/* 0xc2c70 — HS script function handler: set the HUD's flashing state.
 * Evaluates the macro arguments; on success the result block holds a byte
 * value at +0x0.  Calls scripted_hud_set_flashing_state(value) then returns
 * void to the HS thread via hs_return(thread_datum, 0).
 *
 * The +0x0 read is a ZERO-extended byte load (`XOR EDX,EDX ; MOV DL,[EAX]`
 * at 0xc2c8c) — unsigned char, not a dword and not a sign-extended char.
 *
 * ABI (verified against disassembly 0xc2c70-0xc2ca3): cdecl, ESI holds
 * thread_datum across the evaluate call and is reused for hs_return, plain
 * RET, no locals/FPU/SEH.  The single `ADD ESP,0xc` at 0xc2c9e is a merged
 * cdecl cleanup covering the 1-arg scripted_hud_set_flashing_state call plus
 * the 2-arg hs_return call (4+8) — do not read it as a 3-arg hs_return. */
void FUN_000c2c70(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    scripted_hud_set_flashing_state(*(unsigned char *)result);
    hs_return(thread_datum, 0);
  }
}

/* 0xc2cb0 — HS script function handler: restart the HUD's flash cycle.
 * This is a ZERO-ARGUMENT script function: unlike its neighbours it never
 * calls hs_macro_function_evaluate and has no NULL-result guard — the body is
 * an unconditional call to scripted_hud_restart_flashing() followed by
 * hs_return(thread_datum, 0).
 *
 * ABI (verified against disassembly 0xc2cb0-0xc2cc8, 24 bytes / 10
 * instructions, matching the committed bounds entry end=0xc2cc8): cdecl,
 * `PUSH EBP ; MOV EBP,ESP` with no SUB ESP (no locals), no FPU, no SEH, plain
 * `POP EBP ; RET`.  thread_datum is re-read from the frame (`MOV EAX,[EBP+0xc]`
 * at 0xc2cba) rather than cached in ESI, and the trailing `ADD ESP,0x8` is the
 * cleanup for the single 2-arg hs_return call — scripted_hud_restart_flashing
 * takes no arguments and contributes nothing to it.
 *
 * Ghidra types this `void FUN_000c2cb0(void)` and surfaces the [EBP+0xc] read
 * as a phantom `in_stack_00000008`; the real prototype is the standard HS
 * script-function ABI shared by every sibling in this file
 * ([EBP+8]=function_index, [EBP+0xc]=thread_datum, [EBP+0x10]=init), of which
 * only thread_datum is used here. */
void FUN_000c2cb0(int16_t function_index, int thread_datum, char init)
{
  scripted_hud_restart_flashing();
  hs_return(thread_datum, 0);
}

/* 0xc2cd0 (hs.obj) — HaloScript function handler: set an object nav point for
 * a unit's player.
 *
 * Structural twin of the other four-argument handlers in this file: evaluate
 * the script arguments, and when the evaluator hands back a completed argument
 * block, forward the four fields to FUN_000d6490 (0xd6490, "set object nav
 * point for a unit's player") and return 0 to the calling script thread.
 * While arguments are still being evaluated the return is NULL and nothing is
 * dispatched this tick.
 *
 * Argument-block layout, read directly off the disassembly (base EAX = the
 * hs_macro_function_evaluate return value):
 *   +0x00  uint16  (zero-extended: XOR EDX,EDX ; MOV DX,[EAX])
 *   +0x04  int32
 *   +0x08  uint16  (zero-extended: XOR EDX,EDX ; MOV DX,[EAX+8])
 *   +0x0c  float   (FLD dword [EAX+0xc] ... FSTP dword [ESP])
 *
 * Disassembly (0xc2cd0-0xc2d17).  PUSH EBP ; MOV EBP,ESP ; PUSH ESI — no
 * locals, no `sub esp`.  ESI = [EBP+0xC] = thread_datum, held live across both
 * calls.
 *
 *   MOV EAX,[EBP+0x10]        ; init
 *   MOV ECX,[EBP+0x8]         ; function_index
 *   MOV ESI,[EBP+0xC]         ; thread_datum
 *   PUSH EAX; PUSH ESI; PUSH ECX
 *   CALL 0xcc560              ; hs_macro_function_evaluate(index, thread, init)
 *   ADD ESP,0xC; TEST EAX,EAX; JZ end
 *   FLD  dword [EAX+0xc]
 *   PUSH ECX                  ; dummy slot for the float
 *   FSTP dword [ESP]          ; -> arg 4 = *(float *)(result + 3)
 *   XOR  EDX,EDX; MOV DX,[EAX+8]; PUSH EDX  ; -> arg 3 (zero-extended)
 *   MOV  ECX,[EAX+4]; PUSH ECX              ; -> arg 2
 *   XOR  EDX,EDX; MOV DX,[EAX];   PUSH EDX  ; -> arg 1 (zero-extended)
 *   CALL 0xd6490
 *   PUSH 0x0; PUSH ESI        ; cdecl: last PUSH is the first C argument
 *   CALL 0xcbf80              ; hs_return(thread_datum, 0)
 *   ADD  ESP,0x18             ; one coalesced cleanup for BOTH calls (4 + 2)
 *   POP ESI; POP EBP; RET
 *
 * Decompiler traps corrected here:
 *   - Ghidra prototypes this `void FUN_000c2cd0(void)` and surfaces the three
 *     frame reads as `in_stack_*` phantoms; the real prototype is the standard
 *     HS script-function ABI shared by every sibling in this file.
 *   - Push-then-fstp: Ghidra renders arg 4 as `*(int *)(puVar1 + 6)`, i.e. the
 *     dummy `PUSH ECX` value.  The real argument is the float that FSTP writes
 *     over that slot, so 0xd6490's fourth parameter is a `float`, not an
 *     `int` — widened in kb.json accordingly (see the note there).
 *   - Both 16-bit loads are ZERO-extended (XOR/MOV DX), not sign-extended, so
 *     they are read through `uint16_t *`.
 *   - The ARG_COUNT hazard on hs_return (cleanup=6 vs decl=2) is a false
 *     positive from the single merged `ADD ESP,0x18`.
 *
 * Callees (all cdecl, no register args):
 *   0xcc560 = hs_macro_function_evaluate(function_index, thread_datum, init)
 *   0xd6490 = FUN_000d6490(nav_type_value, unit_handle, object_handle, real)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c2cd0(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    FUN_000d6490(*(uint16_t *)result, result[1], *(uint16_t *)(result + 2),
                 *(float *)(result + 3));
    hs_return(thread_datum, 0);
  }
}

/* 0xc2d20 (hs.obj) — HaloScript function handler: set an ENEMY nav point for
 * a unit's player.  The enemy-side counterpart of 0xc2cd0 immediately above,
 * and its structural twin: evaluate the script arguments, and when the
 * evaluator hands back a completed argument block, forward the four fields to
 * FUN_000d64c0 (0xd64c0, "set enemy nav point for a unit's player") and return
 * 0 to the calling script thread.  While arguments are still being evaluated
 * the return is NULL and nothing is dispatched this tick.
 *
 * Argument-block layout, read directly off the disassembly (base EAX = the
 * hs_macro_function_evaluate return value):
 *   +0x00  uint16  (zero-extended: XOR EDX,EDX ; MOV DX,[EAX])
 *   +0x04  int32
 *   +0x08  int32   (full dword — this is the ONE shape difference from 0xc2cd0,
 *                   which loads its third field as a zero-extended word)
 *   +0x0c  float   (FLD dword [EAX+0xc] ... FSTP dword [ESP])
 *
 * Disassembly (0xc2d20-0xc2d64).  PUSH EBP ; MOV EBP,ESP ; PUSH ESI — no
 * locals, no `sub esp`, no _chkstk, no SEH.  ESI = [EBP+0xC] = thread_datum,
 * held live across both calls.
 *
 *   MOV EAX,[EBP+0x10]        ; init
 *   MOV ECX,[EBP+0x8]         ; function_index
 *   MOV ESI,[EBP+0xC]         ; thread_datum
 *   PUSH EAX; PUSH ESI; PUSH ECX
 *   CALL 0xcc560              ; hs_macro_function_evaluate(index, thread, init)
 *   ADD ESP,0xC; TEST EAX,EAX; JZ end
 *   FLD  dword [EAX+0xc]
 *   PUSH ECX                  ; dummy slot for the float
 *   FSTP dword [ESP]          ; -> arg 4 = *(float *)(result + 3)
 *   MOV  EDX,[EAX+8]; PUSH EDX              ; -> arg 3 (full dword)
 *   MOV  ECX,[EAX+4]; PUSH ECX              ; -> arg 2
 *   XOR  EDX,EDX; MOV DX,[EAX];   PUSH EDX  ; -> arg 1 (zero-extended)
 *   CALL 0xd64c0
 *   PUSH 0x0; PUSH ESI        ; cdecl: last PUSH is the first C argument
 *   CALL 0xcbf80              ; hs_return(thread_datum, 0)
 *   ADD  ESP,0x18             ; one coalesced cleanup for BOTH calls (4 + 2)
 *   POP ESI; POP EBP; RET
 *
 * The dword-vs-word third field is corroborated by the function's own extent:
 * 0xc2d20-0xc2d64 is 68 bytes against 0xc2cd0's 71, and `MOV EDX,[EAX+8]` (3
 * bytes) versus `XOR EDX,EDX ; MOV DX,[EAX+8]` (6 bytes) accounts for exactly
 * that 3-byte difference.
 *
 * Decompiler traps corrected here:
 *   - Ghidra prototypes this `void FUN_000c2d20(void)` and surfaces the three
 *     frame reads as `in_stack_*` phantoms; kb.json inherited that wrong
 *     `(void)` declaration.  The real prototype is the standard HS
 *     script-function ABI shared by every sibling in this file
 *     ([EBP+8]=function_index, [EBP+0xc]=thread_datum, [EBP+0x10]=init).
 *   - Push-then-fstp: Ghidra renders arg 4 as `*(int *)(puVar1 + 6)`, i.e. the
 *     dummy `PUSH ECX` value.  The real argument is the float that FSTP writes
 *     over that slot, so 0xd64c0's fourth parameter is a `float`, not an
 *     `int` — widened in kb.json to match, exactly as its sibling 0xd6490
 *     already was for the 0xc2cd0 call site.
 *   - The ARG_COUNT hazard on hs_return (cleanup=6 vs decl=2) is a false
 *     positive from the single merged `ADD ESP,0x18`.
 *
 * Callees (all cdecl, no register args):
 *   0xcc560 = hs_macro_function_evaluate(function_index, thread_datum, init)
 *   0xd64c0 = FUN_000d64c0(nav_type_value, unit_handle, param_3, real)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c2d20(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    FUN_000d64c0(*(uint16_t *)result, result[1], result[2],
                 *(float *)(result + 3));
    hs_return(thread_datum, 0);
  }
}

/* 0xc2d70 — HS script function handler `activate_team_nav_point_flag`
 * (script-function record at 0x2720c4: name="activate_team_nav_point_flag",
 * return_type=4 (void), num_params=4, param_types=(21, 33, 12, 6)).
 * Evaluate the macro arguments and forward the resulting nav-point record to
 * FUN_000d6220, then commit a void (0) return to the calling thread.
 *
 * Exact twin of FUN_000c2dc0 (`activate_team_nav_point_object`) below; the
 * only difference is the +0x08 field, which the flag variant reads as a
 * zero-extended 16-bit cutscene-flag index instead of a full 32-bit object
 * handle — matching FUN_000d6220's `short` 3rd parameter.
 *
 * Result-record layout (derived from the disassembly at 000c2d8c..000c2da7,
 * NOT from the decompiler's ushort* index arithmetic):
 *   +0x00  uint16  XOR EDX,EDX; MOV DX,[EAX]      -> arg 1 (type_value)
 *   +0x04  uint16  XOR ECX,ECX; MOV CX,[EAX+4]    -> arg 2 (team)
 *   +0x08  uint16  XOR EDX,EDX; MOV DX,[EAX+8]    -> arg 3 (flag index)
 *   +0x0c  dword   FLD [EAX+0xc]; PUSH ECX;
 *                  FSTP [ESP]                     -> arg 4 (extra)
 * All three 16-bit loads are zero-extending, so those fields are unsigned.
 *
 * The +0x0c slot is materialised through the FPU (FLD/FSTP [ESP]) and the
 * script table types param 4 as `real`, i.e. the value is semantically a
 * float.  It is nevertheless carried as an opaque dword the whole way down
 * the chain (0xd6220 -> 0xd6180 -> 0xd6030, whose 5th parameter is `int` and
 * whose existing callers bit-pun floats into it), so it is forwarded here as
 * the raw dword; a numeric `(int)*(float *)` cast would truncate the value
 * instead of preserving the bit pattern the original pushes.  Retyping the
 * callee's 4th parameter `float` was measured on the twin (0xc2dc0/0xd6250):
 * MSVC71 lowers a float lvalue copy to the same `MOV`/`PUSH` pair, so it is
 * 0.00pp — the reference's FLD/FSTP form is not reachable from either
 * spelling.
 *
 * Callees (all cdecl, all ported):
 *   0xcc560 = hs_macro_function_evaluate(int16 function_index,
 *                                        int thread_datum, char init)
 *   0xd6220 = FUN_000d6220(int type_value, int team, short object_handle,
 *                          int extra)
 *   0xcbf80 = hs_return(int thread_datum, int value)
 *
 * The single `ADD ESP,0x18` at 000c2db4 cleans up BOTH the 4 pushes for
 * FUN_000d6220 and the 2 pushes for hs_return; hs_return still takes 2 args
 * (the ARG_COUNT "cleanup=6" hazard is that merged cleanup, a false
 * positive).
 */
void FUN_000c2d70(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    FUN_000d6220(*(uint16_t *)result, *(uint16_t *)(result + 1),
                 *(uint16_t *)(result + 2), result[3]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc2dc0 — HS script function handler: evaluate the macro arguments and
 * forward the resulting nav-point record to FUN_000d6250 (set enemy nav point
 * for all players on a team), then commit a void (0) return to the calling
 * thread.
 *
 * Result-record layout (derived from the disassembly at 000c2dda..000c2df4,
 * NOT from the decompiler's ushort* index arithmetic):
 *   +0x00  uint16  XOR EDX,EDX; MOV DX,[EAX]      -> arg 1 (type_value)
 *   +0x04  uint16  XOR ECX,ECX; MOV CX,[EAX+4]    -> arg 2 (team)
 *   +0x08  int32   MOV EDX,[EAX+8]                -> arg 3 (object_handle)
 *   +0x0c  dword   FLD [EAX+0xc]; PUSH ECX;
 *                  FSTP [ESP]                     -> arg 4 (extra)
 * Both 16-bit loads are zero-extending, so those fields are unsigned.
 *
 * The +0x0c slot is materialised through the FPU (FLD/FSTP [ESP]), i.e. the
 * value is semantically a float.  It is nevertheless carried as an opaque
 * dword the whole way down the chain (0xd6250 -> 0xd6180 -> 0xd6030, where
 * FUN_000d6030's 5th parameter is `int` and existing callers bit-pun floats
 * into it), so it is forwarded here as the raw dword.  A numeric
 * `(int)*(float *)` cast would truncate the value instead of preserving the
 * bit pattern the original pushes.  Declaring FUN_000d6250's 4th parameter
 * `float` and passing `*(float *)(result + 3)` was measured: MSVC71 lowers a
 * float lvalue copy to the same `MOV`/`PUSH` pair, so it is 0.00pp on both
 * this function and 0xd6250 — the reference's FLD/FSTP form is not reachable
 * from either spelling.
 *
 * Callees (all cdecl, all ported):
 *   0xcc560 = hs_macro_function_evaluate(int16 function_index,
 *                                        int thread_datum, char init)
 *   0xd6250 = FUN_000d6250(int type_value, int team, int object_handle,
 *                          int extra)
 *   0xcbf80 = hs_return(int thread_datum, int value)
 *
 * The single `ADD ESP,0x18` at 000c2e01 cleans up BOTH the 4 pushes for
 * FUN_000d6250 and the 2 pushes for hs_return; hs_return still takes 2 args.
 */
void FUN_000c2dc0(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    FUN_000d6250(*(uint16_t *)result, *(uint16_t *)(result + 1), result[2],
                 result[3]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc2e10 — HS script function handler: evaluate the macro arguments and
 * forward a (dword, uint16) pair from the result block to FUN_000d64f0.
 *
 * Same skeleton as the rest of this family: evaluate, bail on NULL, dispatch,
 * then commit a void (0) return to the calling thread.
 *
 * Result-block layout (from disassembly at 000c2e2c..000c2e34):
 *   +0x00  dword   -> FUN_000d64f0 arg 1  (MOV EAX, [EAX])
 *   +0x04  uint16  -> FUN_000d64f0 arg 2  (XOR EDX,EDX; MOV DX, [EAX+4])
 * The +0x04 load is zero-extending, so the field is unsigned 16-bit even
 * though the callee's parameter is declared `short`; read through uint16_t so
 * the narrowing happens at the call rather than emitting a MOVSX.
 *
 * Callees (all ported):
 *   0xcc560 = hs_macro_function_evaluate(int16 function_index,
 *                                        int thread_datum, char init)
 *   0xd64f0 = FUN_000d64f0(int, short)
 *   0xcbf80 = hs_return(int thread_datum, int value)
 *
 * Note: the single `ADD ESP,0x10` at 000c2e43 cleans up BOTH the 2 pushes for
 * FUN_000d64f0 and the 2 pushes for hs_return — hs_return really takes 2 args.
 */
void FUN_000c2e10(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    FUN_000d64f0(result[0], *(uint16_t *)(result + 1));
    hs_return(thread_datum, 0);
  }
}

/* 0xc2e50 — HaloScript function handler: clear a unit's player enemy nav
 * point.  Same skeleton as the rest of this family: evaluate the macro
 * arguments, bail on a NULL result block, dispatch, then commit a void (0)
 * return to the calling thread.
 *
 * Result-block layout (from disassembly at 000c2e6d..000c2e70):
 *   +0x00  dword -> FUN_000d6520 arg 1  (MOV EAX, [EAX])
 *   +0x04  dword -> FUN_000d6520 arg 2  (MOV EDX, [EAX+4])
 * Both loads are full 32-bit dwords — unlike siblings 0xc0c30 (int16) and
 * 0xc0c70 (char), there is no narrowing here.
 *
 * Callees (all cdecl, ported):
 *   0xcc560 = hs_macro_function_evaluate(int16 function_index,
 *                                        int thread_datum, char init)
 *   0xd6520 = FUN_000d6520(int, int)
 *   0xcbf80 = hs_return(int thread_datum, int value)
 *
 * ABI (verified against disassembly 0xc2e50-0xc2e86): plain RET, no locals,
 * no _chkstk; ESI caches thread_datum across the body.  `function_index` and
 * `init` are only forwarded to hs_macro_function_evaluate.  The single
 * `ADD ESP,0x10` at 000c2e80 is shared cleanup for BOTH 2-arg calls
 * (FUN_000d6520 and hs_return) — hs_return really takes 2 args.
 */
void FUN_000c2e50(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    FUN_000d6520(result[0], result[1]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc2e90 — HS script function handler: evaluate the macro arguments and
 * forward an (int16, uint16) pair from the result block to FUN_000d6450.
 *
 * Same skeleton as immediate neighbours 0xc2e10 / 0xc2e50: evaluate, bail on a
 * NULL result block, dispatch, then commit a void (0) return to the calling
 * thread.
 *
 * Result-block layout (from disassembly at 000c2eaa..000c2eb6) — note the
 * width/sign asymmetry, which is what distinguishes this handler from its
 * neighbours:
 *   +0x00  int16   -> FUN_000d6450 arg 1  (MOVSX EAX, word ptr [EAX])
 *   +0x04  uint16  -> FUN_000d6450 arg 2  (XOR EDX,EDX; MOV DX, [EAX+4])
 * +0x00 is SIGN-extended (MOVSX) so it must be read through a signed short;
 * +0x04 is ZERO-extended so it must be read through uint16_t.  Widening +0x04
 * to a dword (as sibling 0xc2e50 does) or sign-extending it would be wrong.
 *
 * Callees (all cdecl, ported):
 *   0xcc560 = hs_macro_function_evaluate(int16 function_index,
 *                                        int thread_datum, char init)
 *   0xd6450 = FUN_000d6450(int, short)
 *   0xcbf80 = hs_return(int thread_datum, int value)
 *
 * ABI (verified against disassembly 0xc2e90-0xc2eca): plain RET, no locals, no
 * _chkstk, no SEH; ESI caches thread_datum across the body.  `function_index`
 * and `init` are only forwarded to hs_macro_function_evaluate.  The single
 * `ADD ESP,0x10` at 000c2ec4 is shared cleanup for BOTH 2-arg calls
 * (FUN_000d6450 and hs_return) — hs_return really takes 2 args, and the second
 * one is the literal 0, not a forwarded result (FUN_000d6450 returns void).
 */
void FUN_000c2e90(int16_t function_index, int thread_datum, char init)
{
  short *result;

  result =
    (short *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    FUN_000d6450((int)*result, *(uint16_t *)(result + 2));
    hs_return(thread_datum, 0);
  }
}

/* 0xc2ed0 — HS script function handler: evaluate the macro arguments and
 * forward an (int16, int32) pair from the result block to FUN_000d6470.
 *
 * Same skeleton as immediate neighbours 0xc2e10 / 0xc2e50 / 0xc2e90:
 * evaluate, bail on a NULL result block, dispatch, then commit a void (0)
 * return to the calling thread.
 *
 * Result-block layout (from disassembly at 000c2eea..000c2ef3) — note the
 * width asymmetry, which is what distinguishes this handler from 0xc2e90:
 *   +0x00  int16 -> FUN_000d6470 arg 1  (MOVSX EAX, word ptr [EAX])
 *   +0x04  int32 -> FUN_000d6470 arg 2  (MOV EDX, dword ptr [EAX + 0x4])
 * +0x00 is SIGN-extended (MOVSX) so it must be read through a signed short.
 * +0x04 is a FULL dword read here, not the zero-extended 16-bit read sibling
 * 0xc2e90 performs; narrowing it to uint16_t would be wrong.  `result + 2` is
 * short-pointer arithmetic = byte offset +0x4.
 *
 * Callees (all cdecl, ported):
 *   0xcc560 = hs_macro_function_evaluate(int16 function_index,
 *                                        int thread_datum, char init)
 *   0xd6470 = FUN_000d6470(int, int)
 *   0xcbf80 = hs_return(int thread_datum, int value)
 *
 * ABI (verified against disassembly 0xc2ed0-0xc2f06): PUSH EBP / MOV EBP,ESP /
 * PUSH ESI, no `sub esp` (zero locals), no _chkstk, no SEH; plain RET.  ESI
 * caches thread_datum across the whole body.  `function_index` and `init` are
 * only forwarded to hs_macro_function_evaluate.  The single `ADD ESP,0x10` at
 * 000c2f01 is shared cleanup for BOTH 2-arg calls (FUN_000d6470 and hs_return)
 * — hs_return really takes 2 args, and the second one is the literal 0, not a
 * forwarded result (FUN_000d6470 returns void).
 */
void FUN_000c2ed0(int16_t function_index, int thread_datum, char init)
{
  short *result;

  result =
    (short *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    FUN_000d6470((int)*result, *(int *)(result + 2));
    hs_return(thread_datum, 0);
  }
}

/* 0xc2f10 — HaloScript handler: show the debug terminal, then complete the
 * calling script thread with a zero result (a void-returning script builtin).
 *
 * Disassembly (whole body, 0xc2f10-0xc2f28, 10 instructions):
 *   PUSH EBP; MOV EBP,ESP   ; bare frame — no SUB ESP, no _chkstk → NO locals
 *   CALL 0xe34a0            ; terminal_show(); no args pushed before it and no
 *                           ; cleanup after → confirms void(void)
 *   MOV EAX,[EBP+0xc]       ; thread_datum (2nd cdecl param), NOT [EBP+0x8]
 *   PUSH 0x0                ; hs_return arg2 = value
 *   PUSH EAX                ; hs_return arg1 = thread_datum — cdecl pushes
 *                           ; right-to-left, so the LAST push is the FIRST
 *                           ; C argument: hs_return(thread_datum, 0)
 *   CALL 0xcbf80            ; hs_return
 *   ADD ESP,0x8             ; un-merged cdecl cleanup, 2 dwords → 2 args, all
 *                           ; belonging to hs_return (terminal_show takes none)
 *   POP EBP; RET            ; plain RET, no RET n → cdecl
 *
 * No FPU ops, no struct access, no locals, no buffers.  [EBP+0x8]
 * (function_index) and [EBP+0x10] (init) are never read by this body; a cdecl
 * parameter the callee ignores emits no code, so the disassembly alone cannot
 * distinguish 2 params from 3 — the sibling handlers in this TU
 * (0xc2620/0xc2640, structural twins with the leading callee swapped)
 * arbitrate the uniform hs-evaluator triple.  Ghidra mis-prototypes this as
 * void(void) and surfaces the [EBP+0xc] read as the phantom local
 * `in_stack_00000008`; taking that at face value would pass function_index as
 * the thread handle.  The kb decl was widened from `void FUN_000c2f10(void);`
 * with this lift.
 *
 * Callees (both cdecl, ported, no register args):
 *   0xe34a0 = terminal_show(void)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c2f10(int16_t function_index, int thread_datum, char init)
{
  terminal_show();
  hs_return(thread_datum, 0);
}

/* HaloScript handler shim for the error-overflow-suppression macro function —
 * the single-boolean-argument member of the hs-evaluator shim family.
 * Evaluates the macro arguments; on a non-null result block the record is a
 * lone { bool suppress } at offset +0x0, which is handed to
 * errors_overflow_suppression_enable, then 0 is returned to the script thread.
 *
 * Verified against disassembly 0xc2f30-0xc2f63 (24 instructions, 0x34 bytes):
 *   PUSH EBP; MOV EBP,ESP; PUSH ESI   ; bare frame — no SUB ESP, no _chkstk,
 *                                     ; so NO locals; ESI is the one
 *                                     ; callee-saved register
 *   MOV ECX,[EBP+0x8]   ; function_index (int16_t per the evaluator's decl)
 *   MOV ESI,[EBP+0xc]   ; thread_datum — cached in ESI precisely because it is
 *                       ; re-read after the evaluator call for hs_return
 *   MOV EAX,[EBP+0x10]  ; init
 *   PUSH EAX; PUSH ESI; PUSH ECX; CALL 0xcc560
 *   TEST EAX,EAX; JZ 0xc2f61          ; null-result guard, jumps to epilogue
 *   XOR EDX,EDX; MOV DL,byte ptr [EAX]; PUSH EDX; CALL 0x8f210
 *   PUSH 0x0; PUSH ESI; CALL 0xcbf80
 *   ADD ESP,0xc; POP ESI; POP EBP; RET
 *
 * cdecl pushes right-to-left, so the last push is the first C argument: the
 * evaluator call is (function_index, thread_datum, init), matching its kb decl
 * with no operand swap.  Ghidra mis-prototypes this function as
 * `void FUN_000c2f30(void)` and therefore reports the three parameters as
 * phantom `in_stack_*` locals whose offsets are all 4 too low; the kb decl was
 * widened from that void(void) form with this lift.  Taking Ghidra's offsets at
 * face value would pass function_index as the thread handle.
 *
 * The dereference is a ZERO-extended single BYTE at offset +0x0
 * (`xor edx,edx; mov dl,[eax]`) — not a dword and not a sign-extended byte.
 * Reading it as `*(int *)result` would be a LOADW-class field-width bug, and
 * the evaluator's `int` kb return type is really a record POINTER, so it is
 * cast, never used as a value.  hs_return's second argument is the literal 0
 * (PUSH 0x0), not a computed result.
 *
 * The single `add esp,0xc` in the epilogue is MSVC's merged cleanup for BOTH
 * calls (0x4 for errors_overflow_suppression_enable + 0x8 for hs_return); a
 * naive cdecl reading of that one cleanup makes hs_return look like it takes 3
 * stack args, but it takes 2.  The call-site audit's ARG_COUNT warning here is
 * that false positive (same as FUN_000c2a00 / FUN_000c2b10 above).
 *
 * Callees (all cdecl, ported, no register args):
 *   0xcc560 = hs_macro_function_evaluate(function_index, thread_datum, init)
 *   0x8f210 = errors_overflow_suppression_enable(suppress)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c2f30(int16_t function_index, int thread_datum, char init)
{
  bool *result;

  result =
    (bool *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    errors_overflow_suppression_enable(*result);
    hs_return(thread_datum, 0);
  }
}

/* Zero-argument HaloScript builtin handler.  Structural twin of
 * FUN_000c2160 / FUN_000c2180 above: no hs_macro_function_evaluate call
 * (the builtin takes no script arguments), so the handler just runs its
 * side-effecting callee and completes the calling thread with the value 0.
 *
 *   PUSH EBP; MOV EBP,ESP        ; no locals, no `sub esp`
 *   CALL 0x1954d0                ; no args, no cleanup
 *   MOV EAX,[EBP+0xc]            ; thread_datum (SECOND stack param)
 *   PUSH 0x0                     ; hs_return arg2 = value
 *   PUSH EAX                     ; hs_return arg1 = thread_datum (cdecl:
 *                                ; last PUSH is the first C argument)
 *   CALL 0xcbf80                 ; hs_return
 *   ADD ESP,0x8                  ; cdecl cleanup, 2 dwords
 *   POP EBP; RET                 ; plain cdecl RET, no RET n
 *
 * Ghidra mis-prototypes this as `void FUN_000c2f70(void)` and reports the
 * [EBP+0xc] read as the phantom local `in_stack_00000008`; that name says
 * +8 but the MOV reads +0xc.  EBP+0x8 is function_index (never read),
 * EBP+0xc is thread_datum.  The kb decl was widened from that void(void)
 * form with this lift — leaving it would have passed function_index as the
 * thread handle from the script dispatch table.
 *
 * Callees (both cdecl, ported, no register args):
 *   0x1954d0 = FUN_001954d0(void)               (still unnamed in kb.json)
 *   0xcbf80  = hs_return(thread_handle, value)
 */
void FUN_000c2f70(int16_t function_index, int thread_datum, char init)
{
  FUN_001954d0();
  hs_return(thread_datum, 0);
}

/* 0xc3030 — HaloScript macro-function handler that forwards an evaluated
 * argument record to the scripted-player-effect rumble routine.  Same three
 * parameter dispatch shape as every other handler in this table; Ghidra
 * models it as `void FUN_000c3030(void)` and reports the three cdecl stack
 * parameters as phantom `in_stack_*` locals, so the kb decl was widened from
 * that void(void) form with this lift.
 *
 * Disassembly (0xc3030..0xc3068):
 *   PUSH EBP; MOV EBP,ESP; PUSH ESI  ; only the ESI save, no locals
 *   [EBP+0x8]  = function_index (int16, loaded into ECX)
 *   [EBP+0xc]  = thread_datum   (cached in ESI across both calls)
 *   [EBP+0x10] = init           (loaded into EAX)
 *   PUSH EAX; PUSH ESI; PUSH ECX     ; cdecl: last PUSH is the first C arg,
 *                                    ; i.e. (function_index, thread_datum,
 * init) CALL 0xcc560                     ; hs_macro_function_evaluate ADD
 * ESP,0xc TEST EAX,EAX; JZ 0xc3066         ; plain early-out, no else branch
 *   FLD  dword ptr [EAX+0x4]         ; record field at +4 is a FLOAT
 *   MOV  EDX,dword ptr [EAX]         ; record field at +0 is an int
 *   PUSH ECX                         ; dummy slot for the float argument
 *   FSTP dword ptr [ESP]             ; push-then-fstp: float overwrites it
 *   PUSH EDX                         ; int argument
 *   CALL 0xa2920                     ; scripted_player_effect_set_rumble
 *   PUSH 0x0; PUSH ESI
 *   CALL 0xcbf80                     ; hs_return(thread_datum, 0)
 *   ADD  ESP,0x10                    ; COMBINED cleanup for BOTH calls
 *                                    ; (rumble 8 + hs_return 8) — this is NOT
 *                                    ; a four-argument hs_return, so the
 *                                    ; call-site ARG_COUNT warning is a false
 *                                    ; positive; hs_return's decl stays (2).
 *   POP ESI; POP EBP; RET
 *
 * The `PUSH ECX; FSTP [ESP]` pair is the MSVC float-argument idiom, so Ghidra
 * shows the rumble call as taking no arguments at all (its kb decl was
 * `void scripted_player_effect_set_rumble(void)`, widened to (int, float)
 * here from this call site).  Reading the record's +4 field as an int instead
 * of a float would leave the rumble silently doing nothing — no assert and no
 * VC71 signal.  `result` is `int *`, so that field is reached through a
 * (char *) byte offset, not `result[1]`.
 *
 * Callees (all cdecl, in kb.json, no register arguments):
 *   0xcc560 = hs_macro_function_evaluate(fn_index, thread_datum, init)
 *             (declared `int`; EAX is dereferenced as a record pointer)
 *   0xa2920 = scripted_player_effect_set_rumble(int, float)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c3030(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != 0) {
    scripted_player_effect_set_rumble(result[0],
                                      *(float *)((char *)result + 4));
    hs_return(thread_datum, 0);
  }
}

/* 0xc3070 — HaloScript macro-function handler that forwards an evaluated
 * argument record to the scripted-player-effect start routine.  Instruction
 * for instruction this is the twin of 0xc3030 above; only the consumer call
 * target differs (0xa2df0 here vs 0xa2920 there).  Ghidra models it as
 * `void FUN_000c3070(void)` and reports the three cdecl stack parameters as
 * phantom `in_stack_*` locals, so the kb decl was widened from that void(void)
 * form with this lift.
 *
 * Disassembly (0xc3070..0xc30a8, 57 bytes):
 *   PUSH EBP; MOV EBP,ESP            ; no `sub esp` — zero stack locals
 *   MOV  EAX,[EBP+0x10]              ; init           (arg 3, char)
 *   MOV  ECX,[EBP+0x8]               ; function_index (arg 1, int16)
 *   PUSH ESI                         ; the only callee-saved register used
 *   MOV  ESI,[EBP+0xc]               ; thread_datum   (arg 2), cached in ESI
 *                                    ; because it is needed AGAIN after the
 *                                    ; first call; fn_index and init are dead
 *                                    ; from there on
 *   PUSH EAX; PUSH ESI; PUSH ECX     ; cdecl: last PUSH is the first C arg,
 *                                    ; i.e. (function_index, thread_datum,
 *                                    ;       init)
 *   CALL 0xcc560                     ; hs_macro_function_evaluate
 *   ADD  ESP,0xc                     ; 3 args
 *   TEST EAX,EAX; JZ 0xc30a6         ; plain early-out, no else branch; on a
 *                                    ; NULL record NEITHER tail call runs —
 *                                    ; in particular there is no hs_return
 *   FLD  dword ptr [EAX+0x4]         ; record field at +4 is a FLOAT
 *   MOV  EDX,dword ptr [EAX]         ; record field at +0 is an int
 *   PUSH ECX                         ; dummy slot for the float argument
 *   FSTP dword ptr [ESP]             ; push-then-fstp: float overwrites it
 *   PUSH EDX                         ; int argument
 *   CALL 0xa2df0                     ; scripted_player_effect_start
 *   PUSH 0x0; PUSH ESI
 *   CALL 0xcbf80                     ; hs_return(thread_datum, 0)
 *   ADD  ESP,0x10                    ; COMBINED cleanup for BOTH calls
 *                                    ; (start 8 + hs_return 8) — this is NOT
 *                                    ; a four-argument hs_return, so the
 *                                    ; call-site ARG_COUNT warning is a false
 *                                    ; positive; hs_return's decl stays (2).
 *   POP ESI; POP EBP; RET
 *
 * The `PUSH ECX; FSTP [ESP]` pair is the MSVC float-argument idiom, so Ghidra
 * shows the start call as taking no arguments at all (its kb decl was
 * `void scripted_player_effect_start(void)`, widened to (int, float) here from
 * this call site).  ECX's value at the PUSH is irrelevant — the slot is only
 * being reserved.  Reading the record's +4 field as an int instead of a float
 * would leave the effect silently doing nothing: no assert, no crash, and no
 * VC71 signal.  `result` is `int *`, so that field is reached through a
 * (char *) byte offset, not `result[1]`.
 *
 * Only one local (`result`) is declared, matching the zero-`sub esp` frame;
 * fn_index/init must not be spilled into extra locals or the frame diverges.
 *
 * Callees (all cdecl, in kb.json, no register arguments):
 *   0xcc560 = hs_macro_function_evaluate(fn_index, thread_datum, init)
 *             (declared `int`; EAX is dereferenced as a record pointer)
 *   0xa2df0 = scripted_player_effect_start(int, float)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c3070(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != 0) {
    scripted_player_effect_start(result[0], *(float *)((char *)result + 4));
    hs_return(thread_datum, 0);
  }
}

/* 0xc30b0 — HaloScript macro-function handler that forwards an evaluated
 * argument record to the scripted-player-effect stop routine.  Same family as
 * 0xc3030 and 0xc3070 above; only the consumer call target (0xa2e40) and its
 * argument count differ — stop takes the record's +0 int and nothing else, so
 * there is no float and therefore no push-then-fstp pair here.  Ghidra models
 * this as `void FUN_000c30b0(void)` and reports the three cdecl stack
 * parameters as phantom `in_stack_*` locals, so the kb decl was widened from
 * that void(void) form with this lift.
 *
 * Disassembly (0xc30b0..0xc30e1, 0x32 bytes):
 *   PUSH EBP; MOV EBP,ESP            ; no `sub esp` — zero stack locals
 *   MOV  ECX,[EBP+0x8]               ; function_index (arg 1, int16)
 *   MOV  EAX,[EBP+0x10]              ; init           (arg 3, char)
 *   PUSH ESI                         ; the only callee-saved register used
 *   MOV  ESI,[EBP+0xc]               ; thread_datum   (arg 2), cached in ESI
 *                                    ; because it is
 *                                    ; needed AGAIN at 0xc30d6 after the first
 *                                    ; call, while fn_index and init are dead
 *   PUSH EAX; PUSH ESI; PUSH ECX     ; cdecl: last PUSH is the first C arg,
 *                                    ; i.e. (function_index, thread_datum,
 *                                    ;       init)
 *   CALL 0xcc560                     ; hs_macro_function_evaluate
 *   ADD  ESP,0xc                     ; 3 args — cleanup belongs SOLELY to this
 *                                    ; call
 *   TEST EAX,EAX; JZ 0xc30df         ; plain early-out, no else branch; on a
 *                                    ; NULL record NEITHER tail call runs —
 *                                    ; in particular there is no hs_return.
 *                                    ; The tested EAX is then DEREFERENCED, so
 *                                    ; this is a NULL-pointer guard, not a
 *                                    ; boolean test.
 *   MOV  EDX,dword ptr [EAX]         ; record field at +0 is an int
 *   PUSH EDX                         ; ...and it is the stop call's ONE
 *                                    ; argument.  Ghidra drops this push and
 *                                    ; shows 0xa2e40 as a no-arg call; the kb
 *                                    ; decl was `void
 *                                    ; scripted_player_effect_stop(void)` and
 *                                    ; was widened to (int) from this site.
 *   CALL 0xa2e40                     ; scripted_player_effect_stop
 *   PUSH 0x0; PUSH ESI
 *   CALL 0xcbf80                     ; hs_return(thread_datum, 0)
 *   ADD  ESP,0xc                     ; COMBINED cleanup for BOTH calls
 *                                    ; (stop 4 + hs_return 8) — this is NOT a
 *                                    ; three-argument hs_return, so the
 *                                    ; call-site ARG_COUNT warning is a false
 *                                    ; positive; hs_return's decl stays (2).
 *   POP ESI; POP EBP; RET            ; no `RET n` — cdecl, caller cleans
 *
 * Only one local (`result`) is declared, matching the zero-`sub esp` frame;
 * fn_index/init must not be spilled into extra locals or the frame diverges.
 *
 * Callees (all cdecl, in kb.json, no register arguments):
 *   0xcc560 = hs_macro_function_evaluate(fn_index, thread_datum, init)
 *             (declared `int`; EAX is dereferenced as a record pointer)
 *   0xa2e40 = scripted_player_effect_stop(int)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c30b0(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != 0) {
    scripted_player_effect_stop(result[0]);
    hs_return(thread_datum, 0);
  }
}

/* FUN_000c30f0 (0xc30f0) — HaloScript function handler: toggle HUD visibility.
 *
 * Evaluates the macro's single argument; on success the result block holds a
 * boolean BYTE at +0x0, which is handed to FUN_000d7440 (show_hud), then the
 * thread is resumed with hs_return(thread_datum, 0).
 *
 * Ghidra mis-prototypes this as `void FUN_000c30f0(void)` and reports the three
 * cdecl stack parameters as phantom `in_stack_*` locals, so the kb decl was
 * widened from that void(void) form with this lift.
 *
 * Disassembly (0xc30f0..0xc3123, 0x34 bytes):
 *   PUSH EBP; MOV EBP,ESP            ; no `sub esp` — zero stack locals
 *   MOV  ECX,[EBP+0x8]               ; function_index (arg 1, int16)
 *   MOV  EAX,[EBP+0x10]              ; init           (arg 3, char)
 *   PUSH ESI                         ; the only callee-saved register used
 *   MOV  ESI,[EBP+0xc]               ; thread_datum (arg 2), cached in ESI
 *                                    ; because it is needed AGAIN after the
 *                                    ; first call, while fn_index and init are
 *                                    ; dead
 *   PUSH EAX; PUSH ESI; PUSH ECX     ; cdecl: last PUSH is the first C arg,
 *                                    ; i.e. (function_index, thread_datum,
 *                                    ;       init)
 *   CALL 0xcc560                     ; hs_macro_function_evaluate
 *   ADD  ESP,0xc                     ; 3 args — cleanup belongs SOLELY to this
 *                                    ; call
 *   TEST EAX,EAX; JZ 0xc3121         ; plain early-out, no else branch; on a
 *                                    ; NULL record NEITHER tail call runs — in
 *                                    ; particular there is no hs_return.  The
 *                                    ; tested EAX is then DEREFERENCED, so this
 *                                    ; is a NULL-pointer guard, not a boolean
 *                                    ; test.
 *   XOR  EDX,EDX; MOV DL,byte [EAX]  ; record field at +0 is a BYTE and it is
 *                                    ; ZERO-extended (movzx idiom), not
 *                                    ; sign-extended — hence
 *                                    ; `*(unsigned char *)result`; plain `char`
 *                                    ; is signed here and would emit MOVSX.
 *                                    ; Note the offset is +0x0 (deref of the
 *                                    ; record pointer itself), unlike the
 *                                    ; 0xc0c30 family which reads +0x4.
 *   PUSH EDX                         ; ...the show_hud call's ONE argument
 *   CALL 0xd7440                     ; FUN_000d7440 (show_hud)
 *   PUSH 0x0; PUSH ESI
 *   CALL 0xcbf80                     ; hs_return(thread_datum, 0)
 *   ADD  ESP,0xc                     ; COMBINED cleanup for BOTH calls
 *                                    ; (show_hud 4 + hs_return 8).  There is NO
 *                                    ; `ADD ESP,4` after CALL 0xd7440 — do not
 *                                    ; misread the single 0xc as a
 *                                    ; three-argument hs_return; hs_return's
 *                                    ; decl stays (2) and FUN_000d7440's
 *                                    ; stays (1).
 *   POP ESI; POP EBP; RET            ; no `RET n` — cdecl, caller cleans
 *
 * Only one local (`result`) is declared, matching the zero-`sub esp` frame;
 * fn_index/init must not be spilled into extra locals or the frame diverges.
 *
 * Callees (all cdecl, in kb.json, no register arguments):
 *   0xcc560 = hs_macro_function_evaluate(fn_index, thread_datum, init)
 *             (declared `int`; EAX is dereferenced as a record pointer)
 *   0xd7440 = FUN_000d7440(char)  — show_hud
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c30f0(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != 0) {
    FUN_000d7440(*(unsigned char *)result);
    hs_return(thread_datum, 0);
  }
}

/* 0xc3130 — HaloScript function evaluator wrapper.  Evaluates the call's
 * argument expressions via hs_macro_function_evaluate; when that returns a
 * non-NULL record pointer, feeds the record's first BYTE to FUN_000d7470 and
 * commits a 0 result to the calling script thread.  Structurally identical to
 * FUN_000c30f0 at 0xc30f0 with the consumer swapped from 0xd7440 to 0xd7470.
 *
 * Disassembly (0xc3130-0xc3163, 24 instructions):
 *   PUSH EBP; MOV EBP,ESP; PUSH ESI  ; no `SUB ESP` — one local only
 *   MOV ECX,[EBP+0x08]               ; function_index (int16)
 *   MOV ESI,[EBP+0x0c]               ; thread_datum (reused for hs_return)
 *   MOV EAX,[EBP+0x10]               ; init
 *   PUSH EAX; PUSH ESI; PUSH ECX     ; cdecl: first PUSH is last C arg, so
 *   CALL 0xcc560                     ; hs_macro_function_evaluate(fn_index,
 *   ADD ESP,0xc                      ;   thread_datum, init)
 *   TEST EAX,EAX; JZ 0xc3161         ; NULL record => no work, fall to epilogue
 *   XOR EDX,EDX; MOV DL,[EAX]        ; ZERO-EXTENDED BYTE load of record[0] —
 *   PUSH EDX                         ;   NOT a dword; `int *` deref would emit
 *   CALL 0xd7470                     ;   MOV EDX,[EAX] and lose the match
 *   PUSH 0x0; PUSH ESI               ; hs_return(thread_datum, 0)
 *   CALL 0xcbf80
 *   ADD ESP,0xc                      ; COMBINED cleanup for 0xd7470's 1 arg +
 *                                    ; hs_return's 2 args (cdecl ADD ESP
 *                                    ; mis-grouping); the hazard scanner's
 *                                    ; ARG_COUNT cleanup=3 vs decl=2 warning on
 *                                    ; hs_return is benign — it really takes 2.
 *   POP ESI; POP EBP; RET            ; plain RET — cdecl, caller cleans
 *
 * ABI: the kb decl was widened from `void FUN_000c3130(void);`.  Ghidra's
 * (void) prototype surfaces the three real cdecl stack arguments as the phantom
 * locals in_stack_00000004/8/c.  Only one local (`result`) is declared,
 * matching the zero-`sub esp` frame.
 *
 * Callees (all cdecl, in kb.json, no register arguments):
 *   0xcc560 = hs_macro_function_evaluate(fn_index, thread_datum, init)
 *             (declared `int`; EAX is dereferenced as a record pointer)
 *   0xd7470 = FUN_000d7470(char)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c3130(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != 0) {
    FUN_000d7470(*(unsigned char *)result);
    hs_return(thread_datum, 0);
  }
}

/* 0xc3170 — HaloScript function evaluator taking a single byte-wide argument.
 * Evaluates the macro argument block; on success the block holds one byte at
 * +0x0, which is handed to FUN_000d74a0, then a 0 result is committed to the
 * calling script thread (a void-returning script builtin).  Structurally
 * identical to FUN_000c3130 at 0xc3130 with the side-effect callee swapped
 * from FUN_000d7470 to FUN_000d74a0.
 *
 * Disassembly (0xc3170-0xc31a3, 24 instructions):
 *   PUSH EBP; MOV EBP,ESP; PUSH ESI  ; no `SUB ESP` — one register-resident
 *                                    ; local (`result`), ESI holds thread_datum
 *   MOV ECX,[EBP+0x8]                ; arg 1 = function_index
 *   MOV ESI,[EBP+0xc]                ; arg 2 = thread_datum (live across CALL)
 *   MOV EAX,[EBP+0x10]               ; arg 3 = init
 *   PUSH EAX; PUSH ESI; PUSH ECX     ; cdecl: first PUSH is the LAST C arg, so
 *                                    ; reverse order = (fn_index, thread, init)
 *   CALL 0xcc560                     ; hs_macro_function_evaluate
 *   ADD ESP,0xc                      ; cdecl cleanup, 3 args
 *   TEST EAX,EAX; JZ 0x000c31a1      ; NULL result — skip the whole body
 *   XOR EDX,EDX; MOV DL,byte [EAX]   ; ZERO-EXTENDED SINGLE BYTE from +0x0;
 *                                    ; reading it as an int would be a width
 *                                    ; bug, and the XOR/MOV DL pair (not MOVSX)
 *                                    ; makes the load UNSIGNED
 *   PUSH EDX
 *   CALL 0xd74a0
 *   PUSH 0x0                         ; hs_return arg 2 = value = 0
 *   PUSH ESI                         ; hs_return arg 1 = thread_datum
 *   CALL 0xcbf80
 *   ADD ESP,0xc                      ; COMBINED cleanup for 0xd74a0's 1 arg +
 *                                    ; hs_return's 2 args (cdecl ADD ESP
 *                                    ; mis-grouping); the hazard scanner's
 *                                    ; ARG_COUNT cleanup=3 vs decl=2 warning on
 *                                    ; hs_return is benign — it really takes 2.
 *   POP ESI; POP EBP; RET            ; plain RET — cdecl, caller cleans
 *
 * ABI: the kb decl was widened from `void FUN_000c3170(void);`.  Ghidra's
 * (void) prototype surfaces the three real cdecl stack arguments as the phantom
 * locals in_stack_00000004/8/c.  Only one local (`result`) is declared,
 * matching the zero-`sub esp` frame.
 *
 * Callees (all cdecl, in kb.json, no register arguments):
 *   0xcc560 = hs_macro_function_evaluate(fn_index, thread_datum, init)
 *             (declared `int`; EAX is dereferenced as a record pointer)
 *   0xd74a0 = FUN_000d74a0(char)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c3170(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != 0) {
    FUN_000d74a0(*(unsigned char *)result);
    hs_return(thread_datum, 0);
  }
}

/* 0xc31b0 — HaloScript function handler in the scripted-HUD family.  Evaluates
 * its macro argument; on success the returned result block holds a single
 * boolean/byte in its first HS argument slot, which is handed to FUN_000d74d0,
 * then the script thread is completed with hs_return(thread_datum, 0).
 * Structural twin of FUN_000c34d0 at 0xc34d0 with the action callee swapped.
 *
 * Disassembly (0xc31b0-0xc31e3, 52 bytes, 24 instructions):
 *   PUSH EBP; MOV EBP,ESP; PUSH ESI  ; no `SUB ESP`, no _chkstk — zero locals;
 *                                    ; ESI carries thread_datum ([EBP+0xc])
 *                                    ; across the whole body
 *   PUSH EAX  ([EBP+0x10] = init)    ; cdecl: first PUSH is the LAST C arg
 *   PUSH ESI  ([EBP+0xc]  = thread_datum)
 *   PUSH ECX  ([EBP+0x8]  = function_index)
 *   CALL 0xcc560                     ; hs_macro_function_evaluate
 *   ADD ESP,0xc                      ; cleans THIS call alone (3 args)
 *   TEST EAX,EAX; JZ 0xc31e1         ; early-out when the evaluation failed
 *   XOR EDX,EDX                      ; \ ZERO-extended BYTE load of result[0];
 *   MOV DL, byte ptr [EAX]           ; / MOVSX (signed char *) or a dword load
 *                                    ;   (int *) would both be silent
 *                                    ;   LOADW-class width bugs, so the result
 *                                    ;   pointer is typed `unsigned char *`
 *   PUSH EDX
 *   CALL 0xd74d0                     ; FUN_000d74d0(result[0])
 *   PUSH 0x0                         ; hs_return arg 2 = value = 0
 *   PUSH ESI                         ; hs_return arg 1 = thread_datum
 *   CALL 0xcbf80                     ; hs_return
 *   ADD ESP,0xc                      ; COMBINED cleanup for FUN_000d74d0's 1
 *                                    ; arg + hs_return's 2 args (cdecl ADD ESP
 *                                    ; mis-grouping); the call-site audit's
 *                                    ; "hs_return ARG_COUNT cleanup=3 vs
 *                                    ; decl=2" finding is that merge, not a
 *                                    ; wider hs_return — the real push count
 *                                    ; for hs_return is 2.
 *   POP ESI; POP EBP; RET            ; plain RET — cdecl, caller cleans
 *
 * ABI: the kb decl was widened from `void FUN_000c31b0(void);` to the uniform
 * three-parameter hs handler shape used by every sibling in this TU.  Ghidra's
 * (void) prototype surfaces the real cdecl stack arguments as the phantom
 * locals in_stack_00000004/8/c — that is the tell for dropped stack params,
 * NOT for register arguments; this function takes none.  Note Ghidra's
 * `in_stack_00000008` is off by a slot: the value it names lives at [EBP+0xc],
 * i.e. thread_datum, the SECOND argument.
 *
 * No FPU ops, no struct access, no buffers, single branch.
 *
 * Callees (all cdecl, in kb.json, no register arguments):
 *   0xcc560 = hs_macro_function_evaluate(fn_index, thread_datum, init)
 *   0xd74d0 = FUN_000d74d0(char)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c31b0(int16_t function_index, int thread_datum, char init)
{
  unsigned char *result;

  result = (unsigned char *)hs_macro_function_evaluate(function_index,
                                                       thread_datum, init);
  if (result != 0) {
    FUN_000d74d0(result[0]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc31f0 — HaloScript function handler: evaluate the macro argument and hand
 * its single-byte result to FUN_000d7500, then complete the script thread.
 *
 * Byte-shape twin of the immediately preceding FUN_000c31b0 at 0xc31b0 (same
 * frame, same three-callee sequence); only the dispatch target differs
 * (0xd74d0 -> 0xd7500).  0xd7500 has no binary-backed semantic name yet, so it
 * stays FUN_000d7500 per naming-confidence.
 *
 * Ghidra mis-prototypes this as `void FUN_000c31f0(void)` and surfaces the
 * three stack arguments as `in_stack_00000004/8/c`.  Those labels are
 * misnumbered by one slot and are the tell for dropped cdecl stack params, NOT
 * for register arguments — this function takes none.  Parameter positions come
 * from the disassembly:
 *   MOV EAX,[EBP+0x10]   ; arg 3 = init
 *   MOV ECX,[EBP+0x08]   ; arg 1 = function_index
 *   MOV ESI,[EBP+0x0c]   ; arg 2 = thread_datum (kept in ESI across the call)
 *
 * Disassembly (0xc31f0-0xc3223, 24 instructions, 52 bytes):
 *   PUSH EBP; MOV EBP,ESP              ; bare frame, no locals, no _chkstk
 *   PUSH ESI                           ; ESI carries thread_datum
 *   PUSH EAX; PUSH ESI; PUSH ECX       ; cdecl: last C arg pushed first
 *   CALL 0xcc560                       ; hs_macro_function_evaluate
 *   ADD ESP,0xc                        ; cleanup for the 3-arg call ONLY
 *   TEST EAX,EAX; JZ 0xc3221           ; NULL-result early exit
 *   XOR EDX,EDX; MOV DL,byte ptr [EAX] ; ZERO-extended byte load of result[0]
 *   PUSH EDX; CALL 0xd7500             ; FUN_000d7500(result[0])
 *   PUSH 0x0; PUSH ESI; CALL 0xcbf80   ; hs_return(thread_datum, 0)
 *   ADD ESP,0xc                        ; MERGED cleanup: 1 slot + 2 slots
 *   POP ESI; POP EBP; RET              ; plain RET — cdecl, caller cleans
 *
 * Two decoding hazards, both already documented by the siblings in this TU:
 *   - The trailing ADD ESP,0xc is a single merged cleanup for TWO adjacent
 *     calls (1 push for FUN_000d7500 + 2 for hs_return).  The "hs_return
 *     ARG_COUNT cleanup=3, decl=2" finding is that cdecl merge, not a wider
 *     hs_return.
 *   - XOR EDX,EDX / MOV DL is a ZERO-extending byte load, so the result block
 *     is typed `unsigned char *`.  A signed `char *` emits MOVSX and an `int *`
 *     emits a dword load; both are silent LOADW-class bugs the hazard scanner
 *     would not flag.
 *
 * function_index and init are forwarded once and never re-read, but must stay
 * declared: the dispatcher calls every table entry with the same three-argument
 * cdecl shape, and dropping them would move the [EBP+0xc] load.
 *
 * Callees (all cdecl, in kb.json, no register arguments):
 *   0xcc560 = hs_macro_function_evaluate(fn_index, thread_datum, init)
 *   0xd7500 = FUN_000d7500(char)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c31f0(int16_t function_index, int thread_datum, char init)
{
  unsigned char *result;

  result = (unsigned char *)hs_macro_function_evaluate(function_index,
                                                       thread_datum, init);
  if (result != 0) {
    FUN_000d7500(result[0]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc3230 — HaloScript function handler: forward one byte-wide argument to
 * FUN_000d7530 (hud_messaging.c).  Evaluates the macro-function arguments, and
 * on success reads a single zero-extended BYTE from the result block at +0x0,
 * hands it to FUN_000d7530, then commits a 0 result to the calling script
 * thread.  Instruction-for-instruction the twin of FUN_000c3130 at 0xc3130
 * above; only the side-effect callee differs (0xd7470 -> 0xd7530).
 *
 * Disassembly (0xc3230-0xc3263, 24 instructions):
 *   PUSH EBP; MOV EBP,ESP; PUSH ESI  ; no `SUB ESP` — one register local only
 *   MOV EAX,[EBP+0x10]               ; init            (arg 3)
 *   MOV ESI,[EBP+0xc]                ; thread_datum    (arg 2)
 *   MOV ECX,[EBP+0x8]                ; function_index  (arg 1, int16)
 *   PUSH EAX; PUSH ESI; PUSH ECX     ; cdecl: last arg pushed first
 *   CALL 0xcc560                     ; hs_macro_function_evaluate
 *   ADD ESP,0xc
 *   TEST EAX,EAX; JZ <epilogue>      ; NULL guard skips BOTH calls
 *   XOR EDX,EDX; MOV DL,[EAX]        ; zero-extended BYTE at result+0x0 (NOT
 *                                    ; +0x4 as in the neighbouring handlers,
 *                                    ; and NOT a dword)
 *   PUSH EDX; CALL 0xd7530           ; FUN_000d7530(byte)
 *   PUSH 0x0; PUSH ESI               ; hs_return(thread_datum, 0)
 *   CALL 0xcbf80
 *   ADD ESP,0xc                      ; COMBINED cleanup for 0xd7530's 1 arg +
 *                                    ; hs_return's 2 args (MSVC merged the two
 *                                    ; cdecl cleanups); the hazard scanner's
 *                                    ; ARG_COUNT cleanup=3 vs decl=2 warning on
 *                                    ; hs_return is benign — it really takes 2.
 *   POP ESI; POP EBP; RET            ; plain RET — cdecl, caller cleans
 *
 * ABI: the kb decl was widened from `void FUN_000c3230(void);`.  Ghidra's
 * (void) prototype surfaces the three real cdecl stack arguments as the phantom
 * locals in_stack_00000004/8/c.  ESI is properly saved and restored, so there
 * is no callee-saved hazard.
 *
 * Callees (all cdecl, in kb.json, no register arguments):
 *   0xcc560 = hs_macro_function_evaluate(fn_index, thread_datum, init)
 *             (declared `int`; EAX is dereferenced as a record pointer)
 *   0xd7530 = FUN_000d7530(char)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c3230(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != 0) {
    FUN_000d7530(*(unsigned char *)result);
    hs_return(thread_datum, 0);
  }
}

/* 0xc3270 — HaloScript function evaluator wrapper.  Evaluates the call's
 * argument expressions via hs_macro_function_evaluate; when that returns a
 * non-NULL record pointer, feeds the record's first BYTE to FUN_000d8b90 and
 * commits a 0 result to the calling script thread.  Structural twin of
 * FUN_000c30f0 at 0xc30f0 with the consumer swapped from 0xd7440 to 0xd8b90.
 *
 * Disassembly (0xc3270-0xc32a3, 24 instructions):
 *   PUSH EBP; MOV EBP,ESP            ; no `SUB ESP` — zero stack locals
 *   MOV  ECX,[EBP+0x8]               ; function_index (arg 1, int16)
 *   MOV  EAX,[EBP+0x10]              ; init           (arg 3, char)
 *   PUSH ESI                         ; the only callee-saved register used
 *   MOV  ESI,[EBP+0xc]               ; thread_datum (arg 2), cached in ESI
 *                                    ; because it is live again after the
 *                                    ; first call while fn_index/init are dead
 *   PUSH EAX; PUSH ESI; PUSH ECX     ; cdecl: last PUSH is the first C arg,
 *                                    ; i.e. (function_index, thread_datum,
 *                                    ;       init)
 *   CALL 0xcc560                     ; hs_macro_function_evaluate
 *   ADD  ESP,0xc                     ; 3 args — cleanup belongs SOLELY to this
 *                                    ; call
 *   TEST EAX,EAX; JZ 0xc32a1         ; plain early-out, no else branch; on a
 *                                    ; NULL record NEITHER remaining call runs
 *                                    ; — in particular there is no hs_return.
 *                                    ; The tested EAX is then DEREFERENCED, so
 *                                    ; this is a NULL-pointer guard, not a
 *                                    ; boolean test.
 *   XOR  EDX,EDX; MOV DL,byte [EAX]  ; record field at +0x0 is a BYTE and it is
 *                                    ; ZERO-extended (movzx idiom), hence
 *                                    ; `*(unsigned char *)result`; plain `char`
 *                                    ; is signed here and would emit MOVSX.
 *                                    ; The neighbouring FUN_000c32d0 reads a
 *                                    ; WORD at this same +0x0 — do not copy
 *                                    ; that variant here.
 *   PUSH EDX                         ; ...the consumer call's ONE argument
 *   CALL 0xd8b90                     ; FUN_000d8b90(char)
 *   PUSH 0x0; PUSH ESI
 *   CALL 0xcbf80                     ; hs_return(thread_datum, 0)
 *   ADD  ESP,0xc                     ; COMBINED cleanup for BOTH calls
 *                                    ; (FUN_000d8b90 4 + hs_return 8).  There
 *                                    ; is NO `ADD ESP,4` after CALL 0xd8b90 —
 *                                    ; do not misread the single 0xc as a
 *                                    ; three-argument hs_return; hs_return's
 *                                    ; decl stays (2) and FUN_000d8b90's
 *                                    ; stays (1).  call_site_audit reports
 *                                    ; "ARG_COUNT: hs_return cleanup=3 stack
 *                                    ; args" here; that is a false positive.
 *   POP ESI; POP EBP; RET            ; no `RET n` — cdecl, caller cleans
 *
 * ABI: the kb decl was widened from `void FUN_000c3270(void);`.  Ghidra
 * surfaces the three cdecl stack parameters as phantom `in_stack_*` locals
 * under that void(void) prototype; they must be declared or the frame and the
 * [EBP+0x8]/[EBP+0xc]/[EBP+0x10] loads diverge.  Only one local (`result`) is
 * declared, matching the zero-`SUB ESP` frame; function_index and init must not
 * be spilled into extra locals.
 *
 * Callees (all cdecl, in kb.json, no register arguments):
 *   0xcc560 = hs_macro_function_evaluate(fn_index, thread_datum, init)
 *             (declared `int`; EAX is dereferenced as a record pointer)
 *   0xd8b90 = FUN_000d8b90(char)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c3270(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != 0) {
    FUN_000d8b90(*(unsigned char *)result);
    hs_return(thread_datum, 0);
  }
}

/* 0xc32b0 — HaloScript function evaluator that clears the scripted HUD message
 * queue.  Runs scripted_hud_messages_clear() for its side effect, then commits
 * a 0 result to the calling script thread (a void-returning script builtin).
 * Structurally identical to FUN_000c0cb0 at 0xc0cb0 with the side-effect callee
 * swapped from FUN_00057c60 to scripted_hud_messages_clear.
 *
 * Disassembly (0xc32b0-0xc32c7, 10 instructions):
 *   PUSH EBP; MOV EBP,ESP            ; no `SUB ESP` — zero locals
 *   CALL 0xd5120                     ; scripted_hud_messages_clear(); EAX is
 *                                    ; immediately overwritten below, so the
 *                                    ; `_BYTE *` result is genuinely discarded
 *   MOV EAX,[EBP+0xc]                ; arg 2 = thread_datum (NOT [EBP+8])
 *   PUSH 0x0                         ; hs_return arg 2 = value = 0
 *   PUSH EAX                         ; hs_return arg 1 = thread_datum
 *   CALL 0xcbf80                     ; hs_return
 *   ADD ESP,0x8                      ; cdecl cleanup, exactly 2 args
 *   POP EBP; RET                     ; plain RET — cdecl, caller cleans
 *
 * ABI: the kb decl was widened from `void FUN_000c32b0(void);`.  The body's
 * only real read is [EBP+0xc], i.e. the SECOND stack argument — Ghidra
 * surfaces that as the phantom local `in_stack_00000008` under the (void)
 * prototype.  function_index and init complete the standard hs-evaluator
 * triple (matching 0xc0c30/0xc0c70/0xc0cb0/0xc0cd0) but are unused here; they
 * must still be declared or the frame and the [EBP+0xc] load diverge.
 *
 * Callees (both cdecl, in kb.json, no register arguments):
 *   0xd5120 = scripted_hud_messages_clear(void)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c32b0(int16_t function_index, int thread_datum, char init)
{
  scripted_hud_messages_clear();
  hs_return(thread_datum, 0);
}

/* FUN_000c32d0 (0xc32d0) — HaloScript function handler: set the scripted HUD
 * state message.
 *
 * Instruction-for-instruction the twin of FUN_000c30f0 at 0xc30f0 above: same
 * evaluate / NULL-guard / forward-one-field / hs_return shape.  Only two things
 * differ — the record field read at +0x0 is a WORD here (not a BYTE), and the
 * consumer is scripted_hud_set_state_message (0xd46f0) instead of 0xd7440.
 *
 * Ghidra mis-prototypes this as `void FUN_000c32d0(void)` and surfaces the
 * three cdecl stack parameters as phantom `in_stack_*` locals; the kb decl was
 * widened from that void(void) form with this lift.
 *
 * Disassembly (0xc32d0..0xc3304, 0x35 bytes):
 *   PUSH EBP; MOV EBP,ESP            ; no `sub esp` — zero stack locals
 *   MOV  EAX,[EBP+0x10]              ; init           (arg 3, char)
 *   MOV  ECX,[EBP+0x8]               ; function_index (arg 1, int16)
 *   PUSH ESI                         ; the only callee-saved register used
 *   MOV  ESI,[EBP+0xc]               ; thread_datum (arg 2), cached in ESI
 *                                    ; because it is live ACROSS the first
 *                                    ; call and reused for hs_return, while
 *                                    ; function_index and init are dead after
 *                                    ; it
 *   PUSH EAX; PUSH ESI; PUSH ECX     ; cdecl: last PUSH is the first C arg,
 *                                    ; i.e. (function_index, thread_datum,
 *                                    ;       init)
 *   CALL 0xcc560                     ; hs_macro_function_evaluate
 *   ADD  ESP,0xc                     ; 3 args — cleanup belongs SOLELY to this
 *                                    ; call
 *   TEST EAX,EAX; JZ 0xc3302         ; plain early-out, no else branch.  On a
 *                                    ; NULL record NEITHER tail call runs — in
 *                                    ; particular there is no hs_return.  The
 *                                    ; tested EAX is then DEREFERENCED, so this
 *                                    ; is a NULL-pointer guard, not a boolean
 *                                    ; test on a returned value.
 *   XOR  EDX,EDX; MOV DX,word [EAX]  ; record field at +0 is a WORD and it is
 *                                    ; ZERO-extended (movzx idiom), not
 *                                    ; sign-extended — hence
 *                                    ; `*(unsigned short *)result`.  A plain
 *                                    ; `short` deref is signed here and would
 *                                    ; emit MOVSX, diverging.  The callee's
 *                                    ; `short` parameter type does NOT settle
 *                                    ; the signedness; the XOR/MOV pair does.
 *   PUSH EDX                         ; ...the set_state_message call's ONE arg
 *   CALL 0xd46f0                     ; scripted_hud_set_state_message
 *   PUSH 0x0; PUSH ESI
 *   CALL 0xcbf80                     ; hs_return(thread_datum, 0)
 *   ADD  ESP,0xc                     ; COMBINED cleanup for BOTH calls
 *                                    ; (set_state_message 4 + hs_return 8).
 *                                    ; There is NO `ADD ESP,4` after
 *                                    ; CALL 0xd46f0 — do not misread the single
 *                                    ; 0xc as a three-argument hs_return.  The
 *                                    ; call-site audit's "hs_return cleanup=3,
 *                                    ; decl=2" finding is this merged cleanup
 *                                    ; and is a false positive; hs_return's
 *                                    ; decl stays (2) and
 *                                    ; scripted_hud_set_state_message's
 *                                    ; stays (1).
 *   POP ESI; POP EBP; RET            ; no `RET n` — cdecl, caller cleans
 *
 * Only one local (`result`) is declared, matching the zero-`sub esp` frame;
 * function_index/init must not be spilled into extra locals or the frame
 * diverges.
 *
 * Callees (all cdecl, in kb.json, no register arguments):
 *   0xcc560 = hs_macro_function_evaluate(fn_index, thread_datum, init)
 *             (declared `int`; EAX is dereferenced as a record pointer, so the
 *             cast lives at the call site — the callee decl is left alone
 *             because it is already ported and has other call sites)
 *   0xd46f0 = scripted_hud_set_state_message(short)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c32d0(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != 0) {
    scripted_hud_set_state_message(*(unsigned short *)result);
    hs_return(thread_datum, 0);
  }
}

/* HaloScript function handler: set the scripted HUD objective.
 *
 * Structural twin of FUN_000c32d0 above; only the action callee differs
 * (0xd47c0 scripted_hud_set_objective instead of 0xd46f0
 * scripted_hud_set_state_message).  As there, the kb decl was a stale
 * `void FUN_000c3310(void);` and Ghidra surfaces the three cdecl stack
 * parameters as `in_stack_00000004/8/c`; they are plain stack args at
 * ebp+8/+0xc/+0x10, NOT register arguments.
 *
 * Disassembly shape (PUSH EBP / MOV EBP,ESP / PUSH ESI — one callee-saved
 * register, no _chkstk, no `sub esp`):
 *   MOV ECX,[EBP+0x08]               ; function_index
 *   MOV ESI,[EBP+0x0c]               ; thread_datum (held in ESI across the
 *                                    ; evaluate call and reused below)
 *   MOV EAX,[EBP+0x10]               ; init
 *   PUSH EAX; PUSH ESI; PUSH ECX
 *   CALL 0xcc560                     ; hs_macro_function_evaluate
 *   ADD  ESP,0xc                     ; 3 args
 *   TEST EAX,EAX; JZ end             ; NULL guard on the result record
 *   XOR  EDX,EDX; MOV DX,word [EAX]  ; ZERO-extended 16-bit load from +0x0 —
 *                                    ; must stay an unsigned 16-bit read; an
 *                                    ; `int` read emits a full dword load and
 *                                    ; a signed `short` read emits MOVSX.
 *   PUSH EDX
 *   CALL 0xd47c0                     ; scripted_hud_set_objective(objective)
 *   PUSH 0x0; PUSH ESI
 *   CALL 0xcbf80                     ; hs_return(thread_datum, 0)
 *   ADD  ESP,0xc                     ; COMBINED cleanup for BOTH calls
 *                                    ; (set_objective 4 + hs_return 8).  There
 *                                    ; is no `ADD ESP,4` after CALL 0xd47c0 —
 *                                    ; the call-site audit's "hs_return
 *                                    ; cleanup=3, decl=2" finding is this
 *                                    ; merged cleanup and is a false positive.
 *   POP ESI; POP EBP; RET            ; cdecl, caller cleans
 *
 * Only one local (`result`) is declared, matching the zero-`sub esp` frame.
 *
 * Callees (all cdecl, in kb.json, no register arguments):
 *   0xcc560 = hs_macro_function_evaluate(fn_index, thread_datum, init)
 *   0xd47c0 = scripted_hud_set_objective(short)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c3310(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != 0) {
    scripted_hud_set_objective(*(unsigned short *)result);
    hs_return(thread_datum, 0);
  }
}

/* 0xc3350 — HaloScript function handler: set the scripted HUD timer time.
 *
 * Ghidra mis-prototypes this as `void FUN_000c3350(void)` and surfaces the
 * three stack arguments as `in_stack_00000004/8/c`; the kb decl was widened to
 * the standard hs handler shape used by every sibling in this TU.
 *
 * Evaluates the macro arguments; on success the returned result block is read
 * as two 16-bit fields and handed to scripted_hud_set_timer_time, then the
 * script thread is completed with hs_return(thread_datum, 0).
 *
 * Narrow-load signedness is load-bearing and asymmetric here (disassembly, not
 * the decompiler, is the authority):
 *   0xc336c  XOR EDX,EDX / MOV DX, word ptr [EAX+0x4]   ; ZERO-extended -> arg2
 *   0xc3372  MOVSX EAX, word ptr [EAX]                  ; SIGN-extended -> arg1
 * So +0x0 is a signed short and +0x4 is an unsigned short.  Reading +0x4 as
 * signed is a silent bug (a timer time above 0x7fff would go negative) that
 * neither the hazard scanner nor VC71 would flag.
 *
 * The two 16-bit loads are emitted in right-to-left cdecl argument order
 * (arg2's zero-extended load precedes arg1's sign-extended load), which the
 * natural C expression order reproduces.
 *
 * ADD ESP,0x10 at 0xc3384 is a single merged cleanup for BOTH calls
 * (2 pushes for scripted_hud_set_timer_time + 2 for hs_return); the
 * "hs_return ARG_COUNT cleanup=4, decl=2" finding is that merge, not a
 * wider hs_return.
 *
 * Frame is EBP-based with no locals and no _chkstk (PUSH EBP / MOV EBP,ESP /
 * PUSH ESI); ESI carries thread_datum across the body.
 *
 * Callees (all cdecl, in kb.json, no register arguments):
 *   0xcc560 = hs_macro_function_evaluate(fn_index, thread_datum, init)
 *   0xd4860 = scripted_hud_set_timer_time(short, short)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c3350(int16_t function_index, int thread_datum, char init)
{
  short *result;

  result =
    (short *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != 0) {
    scripted_hud_set_timer_time(result[0], *(unsigned short *)(result + 2));
    hs_return(thread_datum, 0);
  }
}

/* 0xc3390 — HaloScript function handler: set the scripted HUD timer warning
 * cutoff.
 *
 * Structurally identical to FUN_000c3350 (0xc3350); the only difference is the
 * consumer callee (0xd48e0 scripted_hud_set_timer_warning_cutoff here vs
 * 0xd4860 scripted_hud_set_timer_time there).
 *
 * Ghidra mis-prototypes this as `void FUN_000c3390(void)` and surfaces the
 * three stack arguments as `in_stack_00000004/8/c`; the kb decl was widened to
 * the standard hs handler shape used by every sibling in this TU.  Those
 * `in_stack_*` names are the tell for dropped cdecl stack params, NOT for
 * register arguments — this function takes none.
 *
 * Evaluates the macro arguments; on success the returned result block is read
 * as two 16-bit fields and handed to scripted_hud_set_timer_warning_cutoff,
 * then the script thread is completed with hs_return(thread_datum, 0).
 *
 * Narrow-load signedness is load-bearing and asymmetric here (disassembly, not
 * the decompiler, is the authority):
 *   0xc33ac  XOR EDX,EDX / MOV DX, word ptr [EAX+0x4]   ; ZERO-extended -> arg2
 *   0xc33b2  MOVSX EAX, word ptr [EAX]                  ; SIGN-extended -> arg1
 * So +0x0 is a signed short and +0x4 is an unsigned short.  Ghidra's
 * `psVar1[2]` is the halfword at +4 (short-indexed), not a third element;
 * reading it as signed is a silent bug that neither the hazard scanner nor
 * VC71 would flag.
 *
 * ADD ESP,0x10 at 0xc33c4 is a single merged cleanup for BOTH calls (2 pushes
 * for scripted_hud_set_timer_warning_cutoff + 2 for hs_return); the
 * "hs_return ARG_COUNT cleanup=4, decl=2" finding is that cdecl merge, not a
 * wider hs_return.
 *
 * Frame is EBP-based with no locals and no _chkstk (PUSH EBP / MOV EBP,ESP /
 * PUSH ESI); ESI carries thread_datum across the body.
 *
 * Callees (all cdecl, in kb.json, no register arguments):
 *   0xcc560 = hs_macro_function_evaluate(fn_index, thread_datum, init)
 *   0xd48e0 = scripted_hud_set_timer_warning_cutoff(short, short)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c3390(int16_t function_index, int thread_datum, char init)
{
  short *result;

  result =
    (short *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != 0) {
    scripted_hud_set_timer_warning_cutoff(result[0],
                                          *(unsigned short *)(result + 2));
    hs_return(thread_datum, 0);
  }
}

/* 0xc33d0 — HaloScript function handler: set the scripted HUD timer position.
 *
 * Ghidra mis-prototypes this as `void FUN_000c33d0(void)` and surfaces the
 * three stack arguments as `in_stack_00000004/8/c`; the kb decl was widened to
 * the standard hs handler shape used by every sibling in this TU.  Those
 * `in_stack_*` names are the tell for dropped cdecl stack params, NOT for
 * register arguments — this function takes none.
 *
 * Evaluates the macro arguments; on success the returned result block is read
 * as three 16-bit fields (one per 4-byte HS argument slot) and handed to
 * scripted_hud_set_timer_position, then the script thread is completed with
 * hs_return(thread_datum, 0).
 *
 * Narrow-load signedness is load-bearing, and here it diverges from the two
 * preceding siblings (disassembly, not the decompiler, is the authority):
 *   0xc33ec  XOR EDX,EDX / MOV DX, word ptr [EAX+0x8]  ; ZERO-extended -> arg3
 *   0xc33f2  XOR ECX,ECX / MOV CX, word ptr [EAX+0x4]  ; ZERO-extended -> arg2
 *   0xc33f9  XOR EDX,EDX / MOV DX, word ptr [EAX]      ; ZERO-extended -> arg1
 * All three are UNSIGNED, whereas FUN_000c3350/FUN_000c3390 sign-extend their
 * +0x0 field with MOVSX.  Copying those siblings' `result[0]` for arg1 would
 * emit MOVSX here and is a silent bug the hazard scanner would not flag.
 *
 * Ghidra renders the three loads as psVar1[0]/[2]/[4] on a short*, i.e. BYTE
 * offsets 0/4/8 — reading them as element indices 0/1/2 on a short* would
 * silently fetch +0/+2/+4.
 *
 * The loads are emitted in right-to-left cdecl argument order (+0x8 first,
 * then +0x4, then +0x0), which the natural C expression order reproduces; EDX
 * is reused for +0x8 and +0x0, but +0x8 is already pushed by then so there is
 * no aliasing hazard.
 *
 * ADD ESP,0x14 at 0xc340d is a single merged cleanup for BOTH calls (3 pushes
 * for scripted_hud_set_timer_position + 2 for hs_return); the "hs_return
 * ARG_COUNT cleanup=5, decl=2" finding is that cdecl merge, not a wider
 * hs_return.
 *
 * Frame is EBP-based with no locals and no _chkstk (PUSH EBP / MOV EBP,ESP /
 * PUSH ESI); ESI carries thread_datum across the body.
 *
 * Callees (all cdecl, in kb.json, no register arguments):
 *   0xcc560 = hs_macro_function_evaluate(fn_index, thread_datum, init)
 *   0xd4900 = scripted_hud_set_timer_position(short, short, short)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c33d0(int16_t function_index, int thread_datum, char init)
{
  short *result;

  result =
    (short *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != 0) {
    scripted_hud_set_timer_position(*(unsigned short *)result,
                                    *(unsigned short *)(result + 2),
                                    *(unsigned short *)(result + 4));
    hs_return(thread_datum, 0);
  }
}

/* 0xc3420 — HaloScript function handler: show/hide the scripted HUD timer.
 *
 * Ghidra mis-prototypes this as `void FUN_000c3420(void)` and surfaces the
 * three stack arguments as `in_stack_00000004/8/c`; the kb decl was widened to
 * the standard hs handler shape used by every sibling in this TU.  Those
 * `in_stack_*` names are the tell for dropped cdecl stack params, NOT for
 * register arguments — this function takes none.
 *
 * Byte-shape twin of FUN_000c3230/FUN_000c3270 (same frame, same three-callee
 * shape); only the consumer differs (0xd4960 here).
 *
 * Narrow-load signedness is load-bearing and comes from the disassembly, not
 * the decompiler: the load is a single byte at offset 0, `XOR EDX,EDX /
 * MOV DL, byte ptr [EAX]`, i.e. a ZERO-extending promotion, so the pointer
 * must be `unsigned char *`.  Typing it `char *` would promote with MOVSX and
 * diverge.  The callee's parameter type does not settle the pointer's
 * signedness — the load width and extension in the caller does.
 *
 * ADD ESP,0xc after the CALL to hs_return is a single merged cleanup for BOTH
 * calls (1 push for scripted_hud_show_timer + 2 for hs_return); there is no
 * `ADD ESP,4` after 0xd4960.  Do not read that merge as hs_return taking 3
 * arguments.
 *
 * Frame is EBP-based with no `sub esp` (PUSH EBP / MOV EBP,ESP / PUSH ESI), so
 * exactly one local is declared; ESI carries thread_datum across the first
 * call for reuse by hs_return.
 *
 * Callees (all cdecl, in kb.json, no register arguments):
 *   0xcc560 = hs_macro_function_evaluate(fn_index, thread_datum, init)
 *             (declared `int`; EAX is dereferenced as a record pointer, so it
 *             is cast at the call site rather than retyping the callee)
 *   0xd4960 = scripted_hud_show_timer(unsigned char)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c3420(int16_t function_index, int thread_datum, char init)
{
  unsigned char *result;

  result = (unsigned char *)hs_macro_function_evaluate(function_index,
                                                       thread_datum, init);
  if (result != 0) {
    scripted_hud_show_timer(*result);
    hs_return(thread_datum, 0);
  }
}

/* 0xc3460 — HaloScript function handler: pause/resume the scripted HUD timer.
 *
 * Ghidra mis-prototypes this as `void FUN_000c3460(void)` and surfaces the
 * three stack arguments as `in_stack_00000004/8/c`; the kb decl was widened to
 * the standard hs handler shape used by every sibling in this TU.  Those
 * `in_stack_*` names are the tell for dropped cdecl stack params, NOT for
 * register arguments — this function takes none.
 *
 * Byte-shape twin of FUN_000c3420 (same frame, same three-callee shape); only
 * the consumer differs (0xd4980 here rather than 0xd4960).
 *
 * Narrow-load signedness is load-bearing and comes from the disassembly, not
 * the decompiler: the load is a single byte at offset 0, `XOR EDX,EDX /
 * MOV DL, byte ptr [EAX]`, i.e. a ZERO-extending promotion, so the pointer
 * must be `unsigned char *`.  Typing it `char *` would promote with MOVSX and
 * diverge.  The callee's parameter type (`char`) does not settle the pointer's
 * signedness — the load width and extension in the caller does.
 *
 * ADD ESP,0xc at 0xc348e is a single merged cleanup for BOTH calls (1 push for
 * scripted_hud_pause_timer + 2 for hs_return); there is no `ADD ESP,4` after
 * 0xd4980.  Do not read that merge as hs_return taking 3 arguments — the
 * "ARG_COUNT cleanup=3, decl=2" hazard finding is that cdecl merge.
 *
 * Frame is EBP-based with no `sub esp` (PUSH EBP / MOV EBP,ESP / PUSH ESI), so
 * exactly one local is declared; ESI carries thread_datum across the first
 * call for reuse by hs_return.
 *
 * Callees (all cdecl, in kb.json, no register arguments):
 *   0xcc560 = hs_macro_function_evaluate(fn_index, thread_datum, init)
 *             (declared `int`; EAX is dereferenced as a record pointer, so it
 *             is cast at the call site rather than retyping the callee)
 *   0xd4980 = scripted_hud_pause_timer(char)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c3460(int16_t function_index, int thread_datum, char init)
{
  unsigned char *result;

  result = (unsigned char *)hs_macro_function_evaluate(function_index,
                                                       thread_datum, init);
  if (result != 0) {
    scripted_hud_pause_timer(*result);
    hs_return(thread_datum, 0);
  }
}

/* 0xc34a0 — HaloScript function handler: return the scripted HUD timer's
 * remaining tick count to the calling script thread.
 *
 * Takes no script arguments (there is no hs_macro_function_evaluate call and
 * no guard); it simply queries the timer and commits the result.
 *
 * Ghidra mis-prototypes this as `void FUN_000c34a0(void)` and surfaces the one
 * stack argument it does read as `in_stack_00000008` ([EBP+0xc] = arg 2).  The
 * kb decl was widened to the standard hs handler shape used by every sibling in
 * this TU; function_index and init are unread here, exactly as in FUN_000c0cb0
 * at 0xc0cb0.  The `in_stack_*` name is the tell for dropped cdecl stack
 * params, NOT for register arguments — this function takes none.
 *
 * The 16-bit return is staged through a 4-byte slot at EBP-4 that MSVC zeroes
 * BEFORE the call (`MOV dword [EBP-4],0` at 0xc34a4), then overwrites only its
 * low word with AX (`MOV word [EBP-4],AX`), then reloads in full
 * (`MOV EAX,dword [EBP-4]`) to pass to hs_return.  The union models that
 * word-into-zeroed-dword shape; a plain
 * `hs_return(thread_datum, scripted_hud_get_timer_ticks())` would drop the
 * pre-call zeroing and promote with MOVSX/MOVZX instead.  Same idiom as
 * FUN_000c2bd0 at 0xc2bd0, with a word member rather than a byte one.
 *
 * ABI (verified against disassembly 0xc34a0-0xc34c7): cdecl, plain RET.  Frame
 * is `PUSH EBP / MOV EBP,ESP / PUSH ECX` — one 4-byte local, no _chkstk, no
 * buffers.  `ADD ESP,0x8` after 0xcbf80 confirms hs_return takes exactly two
 * cdecl arguments; the first PUSH (EAX = value) is therefore the LAST C
 * argument, giving hs_return(thread_datum, value).
 *
 * Callees (both cdecl, in kb.json, no register arguments):
 *   0xd49d0 = scripted_hud_get_timer_ticks(void) -> short
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c34a0(int16_t function_index, int thread_datum, char init)
{
  union {
    short short_value;
    int long_value;
  } value;

  value.long_value = 0;
  value.short_value = scripted_hud_get_timer_ticks();
  hs_return(thread_datum, value.long_value);
}

/* 0xc34d0 — HaloScript function handler: show/hide the scripted HUD time code.
 *
 * Ghidra mis-prototypes this as `void FUN_000c34d0(void)` and surfaces the
 * three stack arguments as `in_stack_00000004/8/c`; the kb decl was widened to
 * the standard hs handler shape used by every sibling in this TU.  Those
 * `in_stack_*` names are the tell for dropped cdecl stack params, NOT for
 * register arguments — this function takes none.
 *
 * Evaluates the macro argument; on success the returned result block holds a
 * single boolean in its first HS argument slot, which is handed to
 * scripted_hud_time_code_show, then the script thread is completed with
 * hs_return(thread_datum, 0).
 *
 * Narrow-load signedness is load-bearing (disassembly, not the decompiler, is
 * the authority):
 *   0xc34ec  XOR EDX,EDX / MOV DL, byte ptr [EAX]   ; ZERO-extended byte
 * Reading the field through an `int *` or a signed `char *` would emit a dword
 * load or MOVSX; both are silent LOADW-class bugs the hazard scanner would not
 * flag, so the result pointer is typed `unsigned char *`.
 *
 * ADD ESP,0xc at 0xc34fe is a single merged cleanup for BOTH calls (1 push for
 * scripted_hud_time_code_show + 2 for hs_return); the "hs_return ARG_COUNT
 * cleanup=3, decl=2" finding is that cdecl merge, not a wider hs_return.
 *
 * Frame is EBP-based with no locals and no _chkstk (PUSH EBP / MOV EBP,ESP /
 * PUSH ESI); ESI carries thread_datum across the body.
 *
 * Callees (all cdecl, in kb.json, no register arguments):
 *   0xcc560 = hs_macro_function_evaluate(fn_index, thread_datum, init)
 *   0xd4a20 = scripted_hud_time_code_show(bool)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c34d0(int16_t function_index, int thread_datum, char init)
{
  unsigned char *result;

  result = (unsigned char *)hs_macro_function_evaluate(function_index,
                                                       thread_datum, init);
  if (result != 0) {
    scripted_hud_time_code_show(result[0]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc3510 — HaloScript function handler: start the scripted HUD time code.
 *
 * Byte-for-byte the same shape as the preceding handler at 0xc34d0, differing
 * only in the dispatch target (0xd4a50 scripted_hud_time_code_start instead of
 * 0xd4a20 scripted_hud_time_code_show).
 *
 * Ghidra mis-prototypes this as `void FUN_000c3510(void)` and surfaces the
 * three stack arguments as `in_stack_00000004/8/c`; the kb decl was widened to
 * the standard hs handler shape used by every sibling in this TU.  Those
 * `in_stack_*` names are the tell for dropped cdecl stack params, NOT for
 * register arguments — this function takes none.
 *   [EBP+0x08] -> ECX -> arg1 int16_t function_index
 *   [EBP+0x0C] -> ESI -> arg2 int     thread_datum   (ESI across the body)
 *   [EBP+0x10] -> EAX -> arg3 char    init
 * Push order at the 0xcc560 call is PUSH EAX / PUSH ESI / PUSH ECX, i.e. the
 * C argument order (function_index, thread_datum, init).
 *
 * hs_macro_function_evaluate's kb decl returns `int`, but the result is used
 * here as a pointer to the evaluated HS argument block, so it is cast rather
 * than truncated.
 *
 * Narrow-load signedness is load-bearing (disassembly, not the decompiler and
 * not the callee prototype, is the authority):
 *   0xc352c  XOR EDX,EDX / MOV DL, byte ptr [EAX]   ; ZERO-extended byte
 * Reading the field through an `int *` or a signed `char *` would emit a dword
 * load or MOVSX; both are silent LOADW-class bugs the hazard scanner would not
 * flag, so the result pointer is typed `unsigned char *`.
 *
 * ADD ESP,0xc at 0xc353e is a single merged cleanup for BOTH calls (1 push for
 * scripted_hud_time_code_start + 2 for hs_return); the "hs_return ARG_COUNT
 * cleanup=3, decl=2" finding is that cdecl merge, not a wider hs_return.
 *
 * Frame is EBP-based with no locals and no _chkstk (PUSH EBP / MOV EBP,ESP /
 * PUSH ESI); ESI carries thread_datum across the body.
 *
 * Callees (all cdecl, in kb.json, no register arguments):
 *   0xcc560 = hs_macro_function_evaluate(fn_index, thread_datum, init)
 *   0xd4a50 = scripted_hud_time_code_start(bool)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c3510(int16_t function_index, int thread_datum, char init)
{
  unsigned char *result;

  result = (unsigned char *)hs_macro_function_evaluate(function_index,
                                                       thread_datum, init);
  if (result != 0) {
    scripted_hud_time_code_start(result[0]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc3550 — HaloScript function handler: reset the scripted HUD time code.
 *
 * The last member of the scripted_hud handler run, and the only one that takes
 * no HaloScript arguments: because there is nothing to evaluate it skips the
 * 0xcc560 hs_macro_function_evaluate call entirely and unconditionally invokes
 * the dispatch target, then completes the script thread.
 *
 * Ghidra mis-prototypes this as `void FUN_000c3550(void)` and surfaces the one
 * stack argument it can see as `in_stack_00000008`; that label is misleading —
 * the MOV reads [EBP+0x0C], i.e. the SECOND cdecl stack slot, which is
 * thread_datum under the handler convention every sibling in this TU uses:
 *   [EBP+0x08] arg1 int16_t function_index  (never read here)
 *   [EBP+0x0C] -> EAX -> arg2 int thread_datum
 *   [EBP+0x10] arg3 char init               (never read here)
 * The kb decl was widened to that three-parameter shape rather than to the two
 * slots the body happens to touch: the dispatcher calls every handler in the
 * table uniformly, and unread trailing cdecl params emit no code, so the
 * narrower decl would buy nothing and misstate the ABI.
 *
 * Full body, 24 bytes (0xc3550-0xc3567):
 *   PUSH EBP / MOV EBP,ESP           ; bare frame, no locals, no _chkstk
 *   CALL 0xd4a90                     ; scripted_hud_time_code_reset(), 0 args
 *   MOV EAX, dword ptr [EBP+0xc]     ; thread_datum
 *   PUSH 0x0 / PUSH EAX              ; cdecl: last arg pushed first
 *   CALL 0xcbf80                     ; hs_return(thread_datum, 0)
 *   ADD ESP,0x8 / POP EBP / RET
 * Push order proves hs_return(thread_datum, 0), not the reverse; ADD ESP,0x8
 * is the cleanup for that single 2-argument call, so unlike the siblings there
 * is no merged-cleanup ARG_COUNT false positive here.
 *
 * No FPU ops, no narrow loads (so no signedness question), no struct access,
 * no buffers, no branches.
 *
 * Callees (both cdecl, in kb.json, no register arguments):
 *   0xd4a90 = scripted_hud_time_code_reset(void)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c3550(int16_t function_index, int thread_datum, char init)
{
  scripted_hud_time_code_reset();
  hs_return(thread_datum, 0);
}

/* 0xc3570 — HaloScript function handler: invoke the rasterizer-decals thunk.
 *
 * Structurally identical to the 0xc3550 handler above: it takes no HaloScript
 * arguments, so it skips the 0xcc560 hs_macro_function_evaluate call entirely,
 * unconditionally invokes its dispatch target, then completes the script
 * thread.  The target here is 0x17cac0, a 0-argument tail-call thunk into the
 * rasterizer decals module (FUN_0015b1e0); the thunk is called by its own
 * address because the CALL at 0xc3573 is literally 0x17cac0 — calling through
 * to 0x15b1e0 directly would collapse a real instruction.  What the underlying
 * decals routine does is not established here, so neither it nor this handler
 * is given a semantic name.
 *
 * Ghidra mis-prototypes this as `void FUN_000c3570(void)` and surfaces the one
 * stack argument it can see as `in_stack_00000008`; that label is misleading —
 * the MOV reads [EBP+0x0C], i.e. the SECOND cdecl stack slot, which is
 * thread_datum under the handler convention every sibling in this TU uses:
 *   [EBP+0x08] arg1 int16_t function_index  (never read here)
 *   [EBP+0x0C] -> EAX -> arg2 int thread_datum
 *   [EBP+0x10] arg3 char init               (never read here)
 * The kb decl was widened to that three-parameter shape rather than to the two
 * slots the body happens to touch: the dispatcher calls every handler in the
 * table uniformly, and unread trailing cdecl params emit no code, so the
 * narrower decl would buy nothing and misstate the ABI.
 *
 * Full body, 24 bytes (0xc3570-0xc3588):
 *   PUSH EBP / MOV EBP,ESP           ; bare frame, no locals, no _chkstk
 *   CALL 0x17cac0                    ; FUN_0017cac0(), 0 args
 *   MOV EAX, dword ptr [EBP+0xc]     ; thread_datum
 *   PUSH 0x0 / PUSH EAX              ; cdecl: last arg pushed first
 *   CALL 0xcbf80                     ; hs_return(thread_datum, 0)
 *   ADD ESP,0x8 / POP EBP / RET
 * Push order proves hs_return(thread_datum, 0), not the reverse.  Both PUSHes
 * follow the 0x17cac0 CALL, and ADD ESP,0x8 is the cleanup for that single
 * 2-argument call, so there is no merged-cleanup ARG_COUNT false positive and
 * no cdecl argument-stealing to unwind.
 *
 * No FPU ops, no narrow loads (so no signedness question), no struct access,
 * no buffers, no branches.
 *
 * Callees (both cdecl, in kb.json, no register arguments):
 *   0x17cac0 = FUN_0017cac0(void)
 *   0xcbf80  = hs_return(thread_handle, value)
 */
void FUN_000c3570(int16_t function_index, int thread_datum, char init)
{
  FUN_0017cac0();
  hs_return(thread_datum, 0);
}

/* 0xc3590 — HaloScript function handler: invoke the 0x17ed30 thunk.
 *
 * Third member of the no-argument handler run that begins at 0xc3550: it takes
 * no HaloScript arguments, so it skips the hs_macro_function_evaluate call
 * entirely, unconditionally invokes its dispatch target, then completes the
 * script thread.  The target here is 0x17ed30, which is unnamed and unported;
 * it is NOT the 0xd4a90 scripted_hud_time_code_reset that the 0xc3550 twin
 * calls, so no semantic name is assigned to either the callee or this handler.
 *
 * Ghidra mis-prototypes this as `void FUN_000c3590(void)` and surfaces the one
 * stack argument it can see as `in_stack_00000008`; that label is misleading —
 * the MOV reads [EBP+0x0C], i.e. the SECOND cdecl stack slot, which is
 * thread_datum under the handler convention every sibling in this TU uses:
 *   [EBP+0x08] arg1 int16_t function_index  (never read here)
 *   [EBP+0x0C] -> EAX -> arg2 int thread_datum
 *   [EBP+0x10] arg3 char init               (never read here)
 * The kb decl was widened to that three-parameter shape rather than to the two
 * slots the body happens to touch: the dispatcher calls every handler in the
 * table uniformly, and unread trailing cdecl params emit no code, so the
 * narrower decl would buy nothing and misstate the ABI.
 *
 * Full body, 24 bytes (0xc3590-0xc35a7):
 *   PUSH EBP / MOV EBP,ESP           ; bare frame, no locals, no _chkstk
 *   CALL 0x17ed30                    ; FUN_0017ed30(), 0 args
 *   MOV EAX, dword ptr [EBP+0xc]     ; thread_datum
 *   PUSH 0x0 / PUSH EAX              ; cdecl: last arg pushed first
 *   CALL 0xcbf80                     ; hs_return(thread_datum, 0)
 *   ADD ESP,0x8 / POP EBP / RET
 * Push order proves hs_return(thread_datum, 0), not the reverse; ADD ESP,0x8
 * is the cleanup for that single 2-argument call, so there is no merged-cleanup
 * ARG_COUNT false positive here.
 *
 * No FPU ops, no narrow loads (so no signedness question), no struct access,
 * no buffers, no branches.  The lift declares no locals so the bare
 * PUSH EBP / MOV EBP,ESP prologue is reproduced exactly.
 *
 * Callees (both cdecl, in kb.json, no register arguments):
 *   0x17ed30 = FUN_0017ed30(void)
 *   0xcbf80  = hs_return(thread_handle, value)
 */
void FUN_000c3590(int16_t function_index, int thread_datum, char init)
{
  FUN_0017ed30();
  hs_return(thread_datum, 0);
}

/* 0xc3600 — HaloScript function handler: invoke the 0x181150 dispatch target.
 *
 * Same one-shot handler shape as the 0xc3550/0xc3570/0xc3590 siblings above:
 * it takes no HaloScript arguments, so it skips the 0xcc560
 * hs_macro_function_evaluate call entirely, unconditionally invokes its
 * 0-argument dispatch target, then completes the script thread.  What the
 * target at 0x181150 does is not established here, so it keeps its kb name
 * rather than being given a semantic one.
 *
 * Ghidra mis-prototypes this as `void FUN_000c3600(void)` and surfaces the one
 * stack argument it can see as `in_stack_00000008`; that label is misleading —
 * the MOV reads [EBP+0x0C], i.e. the SECOND cdecl stack slot, which is
 * thread_datum under the handler convention every sibling in this TU uses:
 *   [EBP+0x08] arg1 int16_t function_index  (never read here)
 *   [EBP+0x0C] -> EAX -> arg2 int thread_datum
 *   [EBP+0x10] arg3 char init               (never read here)
 * The kb decl was widened to that three-parameter shape rather than to the two
 * slots the body happens to touch: the dispatcher calls every handler in the
 * table uniformly, and unread trailing cdecl params emit no code, so the
 * narrower decl would buy nothing and misstate the ABI.
 *
 * Full body, 24 bytes (0xc3600-0xc3617):
 *   PUSH EBP / MOV EBP,ESP           ; bare frame, no locals, no _chkstk
 *   CALL 0x181150                    ; 0 args
 *   MOV EAX, dword ptr [EBP+0xc]     ; thread_datum
 *   PUSH 0x0 / PUSH EAX              ; cdecl: last arg pushed first
 *   CALL 0xcbf80                     ; hs_return(thread_datum, 0)
 *   ADD ESP,0x8 / POP EBP / RET
 * Push order proves hs_return(thread_datum, 0), not the reverse; ADD ESP,0x8
 * is the cleanup for that single 2-argument call, so unlike some siblings in
 * this run there is no merged-cleanup ARG_COUNT false positive here.
 *
 * No FPU ops, no narrow loads (so no signedness question), no struct access,
 * no buffers, no branches.
 *
 * Callees (both cdecl, in kb.json, no register arguments):
 *   0x181150 = FUN_00181150(void)
 *   0xcbf80  = hs_return(thread_handle, value)
 */
void FUN_000c3600(int16_t function_index, int thread_datum, char init)
{
  FUN_00181150();
  hs_return(thread_datum, 0);
}

/* 0xc3620 — HaloScript function handler: forward one evaluated (uint16, float)
 * pair to the routine at 0x17d9a0.
 *
 * Same shape as the sibling handler at 0xc3660: it drives
 * hs_macro_function_evaluate over its HaloScript argument expressions and, on
 * a non-NULL evaluation record, dispatches to a single target before
 * completing the script thread with a 0 result.  Only the target and the
 * argument widths differ, so neither the target nor this handler gets a
 * semantic name.
 *
 * Ghidra mis-prototypes this as `void FUN_000c3620(void)` and surfaces the
 * three stack arguments as `in_stack_00000004/8/c` — the tell for dropped
 * cdecl stack params, not for register arguments; this function takes none.
 * Real frame offsets:
 *   [EBP+0x08] -> ECX -> arg1 int16_t function_index
 *   [EBP+0x0C] -> ESI -> arg2 int     thread_datum   (ESI across the body)
 *   [EBP+0x10] -> EAX -> arg3 char    init
 * Push order at the 0xcc560 call is PUSH EAX / PUSH ESI / PUSH ECX, i.e. the C
 * argument order (function_index, thread_datum, init).
 *
 * hs_macro_function_evaluate's kb decl returns `int`, but the result is used
 * here as a pointer to the evaluated HS argument block, so it is cast rather
 * than truncated.  Two fields of that block are read, and both widths are
 * load-bearing (taken from the disassembly, not the decompiler):
 *   +0x00  XOR EDX,EDX / MOV DX, word ptr [EAX]  ; ZERO-extended uint16
 *   +0x04  FLD dword ptr [EAX+4]                 ; true float lvalue
 * Reading +0x00 through an `int *` would emit a dword load and reading it
 * through a signed `short *` would emit MOVSX; both are silent LOADW-class
 * bugs, so the result pointer is typed `unsigned short *`.  The float at +0x04
 * is passed by its raw IEEE-754 bits — MSVC emits the push-then-FSTP idiom
 * (PUSH ECX as a dummy slot, then FSTP dword ptr [ESP]), so an int-typed read
 * would FILD-convert and silently change the value.
 *
 * Ghidra DROPPED both of those arguments, rendering the call as
 * `FUN_0017d9a0()`, because kb declared the callee `void FUN_0017d9a0(void)`.
 * The disassembly (PUSH dummy + FSTP [ESP] for the float, then PUSH EDX for
 * the uint16, immediately before CALL 0x17d9a0) proves two cdecl stack
 * arguments, so the kb decl is widened to
 * `void FUN_0017d9a0(int16_t param_1, float param_2)`.  The callee is cdecl and
 * this caller cleans, so widening cannot drift ESP.  Its param widths are read
 * off the callee's own prologue in the pristine XBE (0x17d9a0):
 *   mov ax, word ptr [ebp+8] / test ax,ax / jl / cmp ax,4 / jge  -> SIGNED
 *     16-bit formal, range-checked to 0..3
 *   fld dword ptr [ebp+0xc] / fstp dword ptr [ecx+eax*4+0x64]    -> float
 *     formal, stored into a 4-entry float array hanging off the global at
 *     0x47e4d4
 * so `int16_t`/`float` is the binary-backed spelling, not `int`.
 *
 * ADD ESP,0x10 after the second call is a single merged cleanup for BOTH
 * trailing calls (2 pushes for FUN_0017d9a0 + 2 for hs_return); the
 * "hs_return ARG_COUNT cleanup=4, decl=2" finding is that cdecl merge, not a
 * wider hs_return.
 *
 * Frame is EBP-based with no locals and no _chkstk (PUSH EBP / MOV EBP,ESP /
 * PUSH ESI); ESI carries thread_datum across the body.
 *
 * No FPU arithmetic (the float is a pure load/store passthrough), no struct
 * writes, no buffers.
 *
 * Callees (all cdecl, in kb.json, no register arguments):
 *   0xcc560  = hs_macro_function_evaluate(fn_index, thread_datum, init)
 *   0x17d9a0 = FUN_0017d9a0(int, float)
 *   0xcbf80  = hs_return(thread_handle, value)
 */
void FUN_000c3620(int16_t function_index, int thread_datum, char init)
{
  unsigned short *result;

  result = (unsigned short *)hs_macro_function_evaluate(function_index,
                                                        thread_datum, init);
  if (result != 0) {
    FUN_0017d9a0(result[0], ((float *)result)[1]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc3660 — HaloScript function handler: forward one evaluated boolean-ish
 * byte argument to the rasterizer-sprites routine at 0x17da00.
 *
 * Structurally identical to the scripted_hud handlers at 0xc34d0/0xc3510: it
 * evaluates its single HaloScript argument, and on a non-NULL evaluation
 * record forwards the record's first byte to the dispatch target, then
 * completes the script thread with a 0 result.  Only the target differs
 * (0x17da00, in rasterizer_sprites.obj).  What that routine does is not
 * established here, so neither it nor this handler is given a semantic name.
 *
 * Ghidra mis-prototypes this as `void FUN_000c3660(void)` and surfaces the
 * three stack arguments as `in_stack_00000004/8/c` — those names are the tell
 * for dropped cdecl stack params, NOT for register arguments; this function
 * takes none.  Real frame offsets:
 *   [EBP+0x08] -> ECX -> arg1 int16_t function_index
 *   [EBP+0x0C] -> ESI -> arg2 int     thread_datum   (ESI across the body)
 *   [EBP+0x10] -> EAX -> arg3 char    init
 * Push order at the 0xcc560 call is PUSH EAX / PUSH ESI / PUSH ECX, i.e. the C
 * argument order (function_index, thread_datum, init).
 *
 * hs_macro_function_evaluate's kb decl returns `int`, but the result is used
 * here as a pointer to the evaluated HS argument block, so it is cast rather
 * than truncated.
 *
 * Narrow-load signedness is load-bearing (disassembly, not the decompiler and
 * not the callee prototype, is the authority):
 *   0xc367e  XOR EDX,EDX / MOV DL, byte ptr [EAX]   ; ZERO-extended byte
 * Reading the field through an `int *` or a signed `char *` would emit a dword
 * load or MOVSX; both are silent LOADW-class bugs the hazard scanner would not
 * flag, so the result pointer is typed `unsigned char *`.
 *
 * Ghidra DROPPED that byte argument entirely, rendering the call as
 * `FUN_0017da00()`, because kb declared the callee `void FUN_0017da00(void)`.
 * The disassembly (PUSH EDX immediately before CALL 0x17da00) proves one stack
 * argument, so the kb decl is widened to `void FUN_0017da00(char param_1)` —
 * the same shape the sibling handlers use for scripted_hud_time_code_show /
 * _start.  The callee is cdecl and this caller cleans, so widening cannot
 * drift ESP.
 *
 * ADD ESP,0xc at 0xc368e is a single merged cleanup for BOTH trailing calls
 * (1 push for FUN_0017da00 + 2 for hs_return); the "hs_return ARG_COUNT
 * cleanup=3, decl=2" finding is that cdecl merge, not a wider hs_return.
 *
 * Frame is EBP-based with no locals and no _chkstk (PUSH EBP / MOV EBP,ESP /
 * PUSH ESI); ESI carries thread_datum across the body.
 *
 * No FPU ops, no struct writes, no buffers.
 *
 * Callees (all cdecl, in kb.json, no register arguments):
 *   0xcc560 = hs_macro_function_evaluate(fn_index, thread_datum, init)
 *   0x17da00 = FUN_0017da00(char)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c3660(int16_t function_index, int thread_datum, char init)
{
  unsigned char *result;

  result = (unsigned char *)hs_macro_function_evaluate(function_index,
                                                       thread_datum, init);
  if (result != 0) {
    FUN_0017da00(result[0]);
    hs_return(thread_datum, 0);
  }
}

/* 0xc37b0 — HaloScript function handler: evaluate this call's script arguments,
 * then forward the two evaluated values to rasterizer_screen_effect_set_video
 * and commit a void result to the calling thread.
 *
 * Ghidra mis-prototypes this as `void FUN_000c37b0(void)` and surfaces the
 * three cdecl stack slots as in_stack_00000004/8/c.  Those are ordinary stack
 * parameters, not register arguments; the kb decl is widened to the uniform
 * handler shape every sibling in this TU uses:
 *   [EBP+0x08] arg1 int16_t function_index  -> ECX
 *   [EBP+0x0C] arg2 int     thread_datum    -> ESI (live across the body)
 *   [EBP+0x10] arg3 char    init            -> EAX
 *
 * hs_macro_function_evaluate's kb decl returns `int`, but the returned value is
 * dereferenced here as a pointer to the evaluated-argument block, exactly as in
 * the sibling handlers.  Block layout, read straight from the disassembly:
 *   +0x00  uint16  ; XOR EDX,EDX / MOV DX,word ptr [EAX]  (zero-extended)
 *   +0x04  float   ; FLD dword ptr [EAX+0x4]
 * Nothing else in the block is touched.
 *
 * Argument order for the 0x17db40 call is proven by the pushes, not by Ghidra
 * (which drops both arguments because of the dummy-slot float idiom):
 *   FLD dword ptr [EAX+0x4]   ; float loaded first
 *   PUSH ECX / FSTP dword ptr [ESP]   ; dummy slot overwritten by the float =>
 * arg2 PUSH EDX                          ; zero-extended uint16 => arg1 cdecl
 * pushes last argument first, so the call is
 * rasterizer_screen_effect_set_video(uint16_field, float_field).  Its kb decl
 * was `void (void)` and is widened to `(int mode, float value)` to match; the
 * callee is unported, so no implementation churn follows.
 *
 * ADD ESP,0x10 at 0xc37e6 is a single merged cleanup for BOTH trailing calls
 * (8 bytes each); the "hs_return ARG_COUNT cleanup=4, decl=2" finding is that
 * cdecl merge, not a wider hs_return.
 *
 * Frame is EBP-based with no locals and no _chkstk.  No struct writes, no
 * buffers, no loops, no branches beyond the single NULL test.
 *
 * Callees (all cdecl, in kb.json, no register arguments):
 *   0xcc560  = hs_macro_function_evaluate(function_index, thread_datum, init)
 *   0x17db40 = rasterizer_screen_effect_set_video(int, float)
 *   0xcbf80  = hs_return(thread_handle, value)
 */
void FUN_000c37b0(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    rasterizer_screen_effect_set_video(*(uint16_t *)result,
                                       *(float *)(result + 1));
    hs_return(thread_datum, 0);
  }
}

/* 0xc37f0 — HaloScript function handler: invoke the 0x17dc60 thunk.
 *
 * Structurally identical to the 0xc3550/0xc3570 handlers above: it takes no
 * HaloScript arguments, so it skips the 0xcc560 hs_macro_function_evaluate
 * call entirely, unconditionally invokes its dispatch target, then completes
 * the script thread.  What 0x17dc60 does is not established here, so neither
 * it nor this handler is given a semantic name.
 *
 * Ghidra mis-prototypes this as `void FUN_000c37f0(void)` and surfaces the one
 * stack argument it can see as `in_stack_00000008`; that label is misleading —
 * the MOV reads [EBP+0x0C], i.e. the SECOND cdecl stack slot, which is
 * thread_datum under the handler convention every sibling in this TU uses:
 *   [EBP+0x08] arg1 int16_t function_index  (never read here)
 *   [EBP+0x0C] -> EAX -> arg2 int thread_datum
 *   [EBP+0x10] arg3 char init               (never read here)
 * The kb decl was widened to that three-parameter shape rather than to the two
 * slots the body happens to touch: the dispatcher calls every handler in the
 * table uniformly, and unread trailing cdecl params emit no code, so the
 * narrower decl would buy nothing and misstate the ABI.
 *
 * Full body, 24 bytes (0xc37f0-0xc3807):
 *   PUSH EBP / MOV EBP,ESP           ; bare frame, no locals, no _chkstk
 *   CALL 0x17dc60                    ; 0 args
 *   MOV EAX, dword ptr [EBP+0xc]     ; thread_datum
 *   PUSH 0x0 / PUSH EAX              ; cdecl: last arg pushed first
 *   CALL 0xcbf80                     ; hs_return(thread_datum, 0)
 *   ADD ESP,0x8 / POP EBP / RET
 * Push order proves hs_return(thread_datum, 0), not the reverse; ADD ESP,0x8
 * is the cleanup for that single 2-argument call, so there is no merged-cleanup
 * ARG_COUNT false positive here.
 *
 * No FPU ops, no narrow loads (so no signedness question), no struct access,
 * no buffers, no branches.
 *
 * Callees (both cdecl, in kb.json, no register arguments):
 *   0x17dc60 = FUN_0017dc60(void)
 *   0xcbf80  = hs_return(thread_handle, value)
 */
void FUN_000c37f0(int16_t function_index, int thread_datum, char init)
{
  FUN_0017dc60();
  hs_return(thread_datum, 0);
}

/* 0xc3810 — HaloScript function handler: evaluate this call's script
 * arguments, then forward the first evaluated argument to FUN_0017dec0 and
 * commit a void result to the calling thread.
 *
 * Ghidra mis-prototypes this as `void FUN_000c3810(void)` and surfaces the
 * three cdecl stack slots as `in_stack_00000004/8/c`.  Those labels are slot
 * offsets, not argument indices; the reads are [EBP+8], [EBP+0xc] and
 * [EBP+0x10], i.e. the standard three-parameter hs handler shape used by every
 * sibling in this TU:
 *   [EBP+0x08] arg1 int16_t function_index -> ECX
 *   [EBP+0x0c] arg2 int     thread_datum   -> ESI (held across both calls)
 *   [EBP+0x10] arg3 char    init           -> EAX
 * The kb decl was widened to that shape, matching FUN_000c3550 at 0xc3550.
 *
 * Two decompiler drops are corrected here, both verified against the
 * disassembly at 0xc3810-0xc3841:
 *
 * 1. hs_macro_function_evaluate is declared `int` in kb.json but the result is
 *    DEREFERENCED before use (`MOV EDX,dword ptr [EAX]` at 0xc382c) — it is a
 *    pointer to the evaluated-argument block.  A full 32-bit load, so the
 *    element is `int`, not a narrow field (contrast FUN_000c3460 at 0xc3460,
 *    which does a byte load and therefore holds the result as `unsigned
 * char*`). Cast at the call site, as the siblings in this TU do.
 *
 * 2. Ghidra prints the next call as `FUN_0017dec0()` with no argument.  The
 *    argument is real: `PUSH EDX` at 0xc382e passes *result.  Dropping it would
 *    leave a stale dword on the stack and mis-clean ESP.
 *
 * ABI (verified 0xc3810-0xc3841, 50 bytes): cdecl, plain RET.  Frame is
 * `PUSH EBP / MOV EBP,ESP / PUSH ESI` — one callee-saved register, no locals,
 * no _chkstk, no buffers.  Push order at 0xc381d-0xc381f is PUSH EAX (init) /
 * PUSH ESI (thread_datum) / PUSH ECX (function_index); first push is the last C
 * argument, giving hs_macro_function_evaluate(function_index, thread_datum,
 * init), and `ADD ESP,0xc` at 0xc3825 confirms its three cdecl arguments.
 *
 * The `ADD ESP,0xc` at 0xc383c is a MERGED cleanup covering the single argument
 * to FUN_0017dec0 plus the two to hs_return — it is NOT evidence that hs_return
 * takes three.  hs_return's own pushes (`PUSH 0` then `PUSH ESI`) give
 * hs_return(thread_datum, 0); ESI still holds [EBP+0xc] from entry, so the
 * first argument is thread_datum, not function_index.
 *
 * No FPU ops, no struct stores, no buffers.
 *
 * Callees (all cdecl, in kb.json, no register arguments):
 *   0xcc560  = hs_macro_function_evaluate(short, int, char) -> pointer
 *   0x17dec0 = FUN_0017dec0(int)
 *   0xcbf80  = hs_return(thread_handle, value)
 */
void FUN_000c3810(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != 0) {
    FUN_0017dec0(*result);
    hs_return(thread_datum, 0);
  }
}

/* HaloScript macro handler: run the saved-game refresh at 0x1c58f0, then
 * return 0 to the calling script thread.
 *
 * Ghidra mis-prototypes this as `void FUN_000c3850(void)` and surfaces the one
 * stack argument it can see as `in_stack_00000008`; that label is misleading —
 * the MOV at 0xc3858 reads [EBP+0x0C], i.e. the SECOND cdecl stack slot, which
 * is thread_datum under the handler convention every sibling in this TU uses.
 * The kb decl was widened to the standard three-parameter shape rather than to
 * the two slots the body happens to touch: the dispatcher calls every handler
 * in the table uniformly, and unread trailing cdecl params emit no code, so a
 * narrower decl would buy nothing and misstate the ABI.  Structurally this is
 * byte-for-byte the same shape as FUN_000c3550 at 0xc3550, differing only in
 * the dispatch target.
 *
 * ABI (verified 0xc3850-0xc3867, 24 bytes): cdecl, plain RET.  Frame is a bare
 * `PUSH EBP / MOV EBP,ESP` — no callee-saved registers, no locals, no _chkstk,
 * no buffers.  The `ADD ESP,0x8` at 0xc3863 is the cleanup for the single
 * two-argument hs_return call only; it is not a merged cleanup, so there is no
 * ARG_COUNT ambiguity here (contrast FUN_000c3810 above).  Push order at
 * 0xc385b-0xc385d is PUSH 0 then PUSH EAX; first push is the last C argument,
 * giving hs_return(thread_datum, 0), not the reverse.
 *
 * No FPU ops, no narrow loads, no struct stores, no branches, no SEH.
 *
 * Callees (both cdecl, in kb.json, no register arguments):
 *   0x1c58f0 = FUN_001c58f0(void) — thin wrapper onto FUN_001c5010
 *   0xcbf80  = hs_return(thread_handle, value)
 */
void FUN_000c3850(int16_t function_index, int thread_datum, char init)
{
  FUN_001c58f0();
  hs_return(thread_datum, 0);
}

/* HaloScript macro handler: invoke the routine at 0x1c4f30, then return 0 to
 * the calling script thread.  What 0x1c4f30 actually does is not established
 * from the binary here (it is unported, takes no arguments, and returns
 * nothing), so it is deliberately left as FUN_001c4f30 rather than given a
 * speculative name.
 *
 * Ghidra mis-prototypes this as `void FUN_000c3870(void)` and surfaces the one
 * stack argument it can see as `in_stack_00000008`; that label is misleading —
 * the MOV at 0xc3878 reads [EBP+0x0C], i.e. the SECOND cdecl stack slot, which
 * is thread_datum under the handler convention every sibling in this TU uses.
 * Lifting against the stale two-slot reading of the kb decl would have dropped
 * the parameter and passed garbage to hs_return.  The kb decl was widened to
 * the standard three-parameter shape rather than to the two slots the body
 * happens to touch: the dispatcher calls every handler in the table uniformly,
 * and unread trailing cdecl params emit no code, so a narrower decl would buy
 * nothing and misstate the ABI.  Structurally this is byte-for-byte the same
 * shape as FUN_000c3850 immediately above, differing only in the dispatch
 * target.
 *
 * ABI (verified 0xc3870-0xc3887, 24 bytes): cdecl, plain RET.  Frame is a bare
 * `PUSH EBP / MOV EBP,ESP` — no `SUB ESP`, no callee-saved registers, no
 * locals, no _chkstk, no buffers.  The [EBP+0x0C] load at 0xc3878 is folded
 * straight into the PUSH at 0xc387d, so no local temp is introduced here.  The
 * `ADD ESP,0x8` at 0xc3883 is the cleanup for the single two-argument
 * hs_return call only; it is not a merged cleanup, so there is no ARG_COUNT
 * ambiguity.  Push order at 0xc387b-0xc387d is PUSH 0 then PUSH EAX; the first
 * push is the last C argument, giving hs_return(thread_datum, 0), not the
 * reverse.  The CALL at 0xc3873 has no pushes before it and no cleanup after,
 * confirming FUN_001c4f30 takes zero arguments.
 *
 * No FPU ops, no narrow loads, no struct stores, no branches, no SEH.
 *
 * Callees (both cdecl, in kb.json, no register arguments):
 *   0x1c4f30 = FUN_001c4f30(void)
 *   0xcbf80  = hs_return(thread_handle, value)
 */
void FUN_000c3870(int16_t function_index, int thread_datum, char init)
{
  FUN_001c4f30();
  hs_return(thread_datum, 0);
}

/* HaloScript function handler at 0xc3890: invokes the zero-argument routine
 * player_ui_fast_setup_network_server (0xe0cd0) and then completes the calling
 * script thread with the value 0.  The handler itself has no return value; the
 * script-visible result is delivered through hs_return, as in every sibling
 * handler in this table.
 *
 * Ghidra mis-prototypes this as `void FUN_000c3890(void)` and surfaces the one
 * stack argument it can see as `in_stack_00000008`; that label is misleading —
 * the MOV at 0xc3898 reads [EBP+0x0C], i.e. the SECOND cdecl stack slot, which
 * is thread_datum under the handler convention every sibling in this TU uses.
 * Lifting against the stale `(void)` kb decl would have dropped the parameter
 * and passed garbage to hs_return.  The kb decl was widened to the standard
 * three-parameter shape rather than to the two slots the body happens to
 * touch: the dispatcher calls every handler in the table uniformly, and unread
 * trailing cdecl params emit no code, so a narrower decl would buy nothing and
 * misstate the ABI.  Structurally this is byte-for-byte the same shape as
 * FUN_000c3870 immediately above, differing only in the dispatch target.
 *
 * ABI (verified 0xc3890-0xc38a7, 24 bytes): cdecl, plain RET.  Frame is a bare
 * `PUSH EBP / MOV EBP,ESP` — no `SUB ESP`, no callee-saved registers, no
 * locals, no _chkstk, no buffers.  The [EBP+0x0C] load at 0xc3898 is folded
 * straight into the PUSH at 0xc389d, so no local temp is introduced here.  The
 * `ADD ESP,0x8` at 0xc38a3 is the cleanup for the single two-argument
 * hs_return call only; it is not a merged cleanup, so there is no ARG_COUNT
 * ambiguity.  Push order at 0xc389b-0xc389d is PUSH 0 then PUSH EAX; the first
 * push is the last C argument, giving hs_return(thread_datum, 0), not the
 * reverse.  The CALL at 0xc3893 has no pushes before it and no cleanup after,
 * confirming player_ui_fast_setup_network_server takes zero arguments.
 *
 * No FPU ops, no narrow loads, no struct stores, no branches, no SEH.
 *
 * Callees (both cdecl, in kb.json, no register arguments):
 *   0xe0cd0  = player_ui_fast_setup_network_server(void)
 *   0xcbf80  = hs_return(thread_handle, value)
 */
void FUN_000c3890(int16_t function_index, int thread_datum, char init)
{
  player_ui_fast_setup_network_server();
  hs_return(thread_datum, 0);
}

/* 0xc38b0 — HaloScript builtin that activates all solo (campaign) levels in
 * the player-UI level table, then commits a 0 result to the calling script
 * thread (a void-returning script builtin).
 *
 * Ghidra mis-prototypes this as `void FUN_000c38b0(void)` — that prototype is
 * echoed from the stale kb.json decl, not derived from the code.  The body
 * reads [EBP+0x0C], i.e. the SECOND stack argument, so the real shape is the
 * standard three-argument hs-evaluator signature used throughout this TU.
 * Ghidra surfaces the [EBP+0x0C] read as a bogus `in_stack_00000008` local
 * precisely because its prototype has no parameters to attribute it to.
 * Structurally this is byte-for-byte the same shape as FUN_000c3890
 * immediately above, differing only in the dispatch target.
 *
 * ABI (verified 0xc38b0-0xc38c7, 24 bytes): cdecl, plain RET.  Frame is a bare
 * `PUSH EBP / MOV EBP,ESP` — no `SUB ESP`, no callee-saved registers, no
 * locals, no _chkstk, no buffers.  The [EBP+0x0C] load at 0xc38b8 is folded
 * straight into the PUSH at 0xc38bd, so no local temp is introduced here.  The
 * `ADD ESP,0x8` at 0xc38c3 is the cleanup for the single two-argument
 * hs_return call only; it is not a merged cleanup, so there is no ARG_COUNT
 * ambiguity.  Push order at 0xc38bb-0xc38bd is PUSH 0 then PUSH EAX; the first
 * push is the last C argument, giving hs_return(thread_datum, 0), not the
 * reverse.  The CALL at 0xc38b3 has no pushes before it and no cleanup after,
 * confirming player_ui_activate_all_solo_levels takes zero arguments.
 *
 * function_index ([EBP+0x08]) and init ([EBP+0x10]) are never read by the
 * original; they exist only to complete the evaluator signature.  Do not add
 * a use — it would change codegen.
 *
 * No FPU ops, no narrow loads, no struct stores, no branches, no SEH.
 *
 * Callees (both cdecl, in kb.json, no register arguments):
 *   0xe0fd0  = player_ui_activate_all_solo_levels(void)
 *   0xcbf80  = hs_return(thread_handle, value)
 */
void FUN_000c38b0(int16_t function_index, int thread_datum, char init)
{
  player_ui_activate_all_solo_levels();
  hs_return(thread_datum, 0);
}

/* 0xc38d0 (hs.obj) — HaloScript handler for the script command
 * "player0_look_invert_pitch" (help text "invert player0's look").  Identified
 * from the hs_function_definition record at 0x2726dc, whose evaluate slot
 * (+0x0c) is the only reference to this address in the XBE: return_type = 4
 * (void), num_params = 1, param_types[0] = 5 (boolean), parse = 0xc7e50 (the
 * shared single-argument parser used by this whole wrapper family).
 *
 * Shape is the standard macro-function wrapper: evaluate the one script
 * argument, and on a non-NULL result record forward its boolean byte to the
 * player_ui setter, then return void to the script thread.
 *
 * ABI (cdecl): frame is `PUSH EBP / MOV EBP,ESP / PUSH ESI` — no SUB ESP, no
 * locals, no _chkstk, no buffers.  ESI holds thread_datum ([EBP+0x0C]) across
 * the evaluate call.  The evaluate call pushes EAX(init), ESI(thread_datum),
 * ECX(function_index) and is cleaned with its own `ADD ESP,0xC`, giving the C
 * order hs_macro_function_evaluate(function_index, thread_datum, init).
 *
 * The `int` return of hs_macro_function_evaluate is used as a POINTER here:
 * `TEST EAX,EAX / JZ` is the NULL check, and `XOR EDX,EDX / MOV DL,byte ptr
 * [EAX]` zero-extends the first byte of the result record, which is the
 * boolean argument.  Hence `char *result` and `*result`, matching the
 * byte-deref sibling at 0xc23c0.
 *
 * The single `ADD ESP,0xC` after the hs_return call is a MERGED cleanup for
 * FUN_000e1770's one push plus hs_return's two.  check_lift_hazards.py reports
 * ARG_COUNT "cleanup=3 vs decl=2" against hs_return (0xcbf80) for this — it is
 * a false positive; hs_return really takes two arguments.  Do not widen it.
 *
 * FUN_000e1770's kb.json declaration was `void FUN_000e1770(void)`, which is
 * an under-declaration: the caller does `PUSH EDX` before the CALL with no
 * cleanup of its own, so the callee consumes one stack argument.  A cdecl
 * parameter the callee ignores is byte-identical to no parameter at all, so
 * the callee body is not evidence against it — only this PUSH is.  Widened to
 * `void FUN_000e1770(char invert)` (the boolean from the table above); the
 * symbol is deliberately NOT renamed, and it has no other callers in src/.
 *
 * function_index ([EBP+0x08]) and init ([EBP+0x10]) are forwarded to the
 * evaluator only.  Side-effect order is load-bearing: FUN_000e1770 runs BEFORE
 * hs_return.  No FPU ops, no narrow loads, no struct stores, no SEH.
 *
 * Callees (all cdecl, in kb.json, no register arguments):
 *   0xcc560 = hs_macro_function_evaluate(function_index, thread_datum, init)
 *   0xe1770 = FUN_000e1770(invert)
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c38d0(int16_t function_index, int thread_datum, char init)
{
  char *result;

  result =
    (char *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != (char *)0) {
    FUN_000e1770(*result);
    hs_return(thread_datum, 0);
  }
  return;
}

/* HaloScript function handler at 0xc3910: queries the zero-argument predicate
 * player0_look_pitch_is_inverted (0xe1050) and completes the calling script
 * thread with that boolean as the script-visible result.
 *
 * Ghidra mis-prototypes this as `void FUN_000c3910(void)` and surfaces the one
 * stack slot it can see as `in_stack_00000008`; that label is misleading — the
 * MOV at 0xc3920 reads [EBP+0x0C], i.e. the SECOND cdecl stack slot, which is
 * thread_datum under the handler convention every sibling in this TU uses.
 * The kb decl was widened to the standard three-parameter shape rather than to
 * the slots the body happens to touch: the dispatcher calls every handler in
 * the table uniformly, and unread trailing cdecl params emit no code.  The
 * symbol is deliberately NOT renamed, matching its siblings.
 *
 * ABI (verified 0xc3910-0xc3936 against the pristine XBE, 15 instructions):
 * cdecl, plain RET, `PUSH EBP / MOV EBP,ESP / PUSH ECX` — one 4-byte local at
 * [EBP-4], no callee-saved registers, no _chkstk, no buffers.  The CALL at
 * 0xc391b has no pushes before it and no cleanup after, confirming the
 * predicate takes zero arguments.  The `ADD ESP,0x8` at 0xc3930 is the cleanup
 * for the single two-argument hs_return call only — not a merged cleanup, so
 * there is no ARG_COUNT ambiguity.  Push order at 0xc3929-0xc392a is PUSH EAX
 * (the result) then PUSH ECX (thread_datum); the first push is the last C
 * argument, giving hs_return(thread_datum, result), not the reverse.  Unlike
 * FUN_000c38d0 there is no NULL check — hs_return is unconditional.
 *
 * The result slot is written at TWO widths and that is load-bearing:
 *   0xc3914  MOV dword ptr [EBP-4],0   <- whole slot zeroed
 *   0xc3923  MOV byte  ptr [EBP-4],AL  <- only the LOW BYTE overwritten
 *   0xc3926  MOV EAX,dword ptr [EBP-4] <- read back as a full dword
 * This is MSVC's zero-then-byte-store widening of a 1-byte return into a
 * 4-byte slot, so the value reaching hs_return is zero-extended, never
 * sign-extended.  Writing it as a plain `int result = predicate();` would
 * instead emit a MOVZX into a register and drop the stack slot, so the byte
 * store is expressed explicitly through the slot's address.
 *
 * kb.json declared 0xe1050 as `void player0_look_pitch_is_inverted(void)`,
 * which is an under-declaration: the callee is `MOV AL,byte ptr [0x46bf0b] /
 * RET`, and 0xc3923 consumes AL immediately.  Widened to `unsigned char
 * player0_look_pitch_is_inverted(void)`; it has no other callers in src/.
 *
 * function_index ([EBP+0x08]) and init ([EBP+0x10]) are never read here.
 * No FPU ops, no narrow loads, no struct stores, no branches, no SEH.
 *
 * Callees (both cdecl, in kb.json, no register arguments):
 *   0xe1050 = player0_look_pitch_is_inverted(void) -> bool in AL
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c3910(int16_t function_index, int thread_datum, char init)
{
  int result;

  result = 0;
  *(unsigned char *)&result = player0_look_pitch_is_inverted();
  hs_return(thread_datum, result);
}

/* 0xc3940 — HaloScript function evaluator that returns a player0 control
 * setting as a boolean script result.  Direct sibling of 0xc3910 above: it
 * calls the byte-returning accessor at 0xe1060 and commits the result to the
 * calling script thread.
 *
 * kb.json declared 0xe1060 as `void FUN_000e1060(void);`, which is an
 * under-declaration: the callee is
 *   MOV AL,byte ptr [0x46bf09] / TEST AL,AL / JE .t / CMP AL,1 / JE .t /
 *   XOR EAX,EAX / RET   .t: MOV EAX,1 / RET
 * i.e. it yields 1 when the setting byte is 0 or 1, else 0, and 0xc3953
 * consumes AL immediately.  Widened to `unsigned char FUN_000e1060(void)`,
 * matching the 0xe1050 precedent in the same accessor cluster.
 *
 * The result local at [EBP-0x4] is zero-initialized as a dword
 * (MOV dword [EBP-0x4],0) and then only its LOW BYTE is overwritten with AL
 * (MOV byte [EBP-0x4],AL), so the committed value is a zero-extended byte.
 *
 * function_index ([EBP+0x08]) and init ([EBP+0x10]) are never read here.
 * No FPU ops, no narrow loads, no struct stores, no branches, no SEH.
 *
 * Callees (both cdecl, in kb.json, no register arguments):
 *   0xe1060 = FUN_000e1060(void) -> bool in AL
 *   0xcbf80 = hs_return(thread_handle, value)
 */
void FUN_000c3940(int16_t function_index, int thread_datum, char init)
{
  int result;

  result = 0;
  *(unsigned char *)&result = FUN_000e1060();
  hs_return(thread_datum, result);
}

/* 0xc3970 — HaloScript function handler: writes a single byte setting taken
 * from the evaluated macro-argument block.  Standard hs evaluator shape:
 * hs_macro_function_evaluate(function_index, thread_datum, init) returns the
 * argument block (NULL on failure/deferral), and on success the FIRST BYTE of
 * that block is passed to the byte setter at 0xe3ca0 before the script thread
 * is completed with hs_return(thread_datum, 0).
 *
 * ABI (verified 0xc3970-0xc39a3 against the pristine XBE, 20 instructions):
 * cdecl, plain RET, no return value.  ESI caches [EBP+0xc] (thread_datum)
 * across both calls.  Push order at 0xc397d-0xc397f is EAX([EBP+0x10]),
 * ESI([EBP+0xc]), ECX([EBP+0x8]) so the left-to-right argument order to
 * 0xcc560 is (function_index, thread_datum, init) — same as every sibling.
 *
 * Two kb.json under-declarations had to be corrected for this site:
 *   - 0xcc560 is declared `int`, but 0xc398e does `MOV DL,byte ptr [EAX]`,
 *     i.e. the result is dereferenced.  Cast at the call site, matching the
 *     `(int *)` casts used by the 0xc0c30 family above.
 *   - 0xe3ca0 was declared `void (void)`, but 0xc398c-0xc3990 emit
 *     `XOR EDX,EDX / MOV DL,[EAX] / PUSH EDX`, one zero-extended byte
 *     argument.  The callee is 6 instructions:
 *       PUSH EBP / MOV EBP,ESP / MOV AL,byte ptr [EBP+8] /
 *       MOV byte ptr [0x46cc84],AL / POP EBP / RET
 *     confirming cdecl with exactly one byte parameter.  Widened to
 *     `void ui_widget_debug_show_path(unsigned char value);` (name kept as
 *     stored in kb.json — it is unproven and looks unrelated to hs).
 *
 * The single `ADD ESP,0xc` at 0xc399e is MSVC coalescing the cleanup for the
 * 0xe3ca0 push and hs_return's two pushes; hs_return still takes 2 args.
 */
void FUN_000c3970(int16_t function_index, int thread_datum, char init)
{
  unsigned char *result;

  result = (unsigned char *)hs_macro_function_evaluate(function_index,
                                                       thread_datum, init);
  if (result != NULL) {
    ui_widget_debug_show_path(result[0]);
    hs_return(thread_datum, 0);
  }
}

/* HS script function handler: display a scenario help message.
 *
 * Evaluates the macro arguments; on success the returned block holds the
 * help/string index as an int16 at +0x0.  0xc39cc-0xc39d1 emit
 * `XOR EDX,EDX / MOV DX,word ptr [EAX] / PUSH EDX`, a zero-extended 16-bit
 * load — Ghidra drops this argument entirely and shows `FUN_000e8e20()`.
 * The callee's kb declaration already takes one int16 parameter.
 *
 * The single `ADD ESP,0xc` at 0xc39df is MSVC coalescing the cleanup for the
 * 0xe8e20 push and hs_return's two pushes; hs_return still takes 2 args. */
void FUN_000c39b0(int16_t function_index, int thread_datum, char init)
{
  int *result;

  result =
    (int *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    ui_widget_display_scenario_help(*(short *)result);
    hs_return(thread_datum, 0);
  }
}

/* HS script function handler: no-argument builtin that performs a single
 * engine action and returns void to the calling script thread.
 *
 * Unlike its siblings this stub never calls hs_macro_function_evaluate — it
 * takes no script arguments — so the whole body is:
 *   CALL 0x0012a7a0            ; plain cdecl void(void), no arguments
 *   MOV EAX,[EBP+0xc]          ; SECOND stack slot = thread_datum
 *   PUSH 0x0 / PUSH EAX / CALL hs_return / ADD ESP,0x8
 * Ghidra mis-prototypes it as `void FUN_000c39f0(void)` and reports the
 * hs_return argument as `in_stack_00000008` ([EBP+8]); the disassembly loads
 * from [EBP+0xc], so the value forwarded is the second cdecl argument.  The
 * parameter list is the hs builtin triple used by every sibling in this file;
 * `function_index` and `init` are unread here (this builtin ignores the init
 * pass), which is why the frame is the bare PUSH EBP / MOV EBP,ESP with no
 * locals and no `sub esp`. */
void FUN_000c39f0(int16_t function_index, int thread_datum, char init)
{
  FUN_0012a7a0();
  hs_return(thread_datum, 0);
}

/* 0xc3a10 — HS built-in evaluator, sibling of FUN_000c39f0 above.  Evaluates a
 * single macro-function argument via hs_macro_function_evaluate; while that
 * returns NULL the evaluation is still pending and nothing is committed on this
 * call.  Once it yields a non-NULL result record, the first dword of the record
 * (a string pointer) is forwarded to xbox_set_machine_name and the calling
 * thread is committed with hs_return(thread_datum, 0).
 *
 * Plain cdecl (caller cleans, RET with no immediate).  Three stack params, the
 * standard hs builtin triple used by every sibling in this file:
 *   param1 @ EBP+0x8  = function_index (int16_t), loaded into ECX
 *   param2 @ EBP+0xc  = thread_datum, held in ESI across both calls
 *   param3 @ EBP+0x10 = init (char), loaded into EAX
 * Frame is PUSH EBP / MOV EBP,ESP / PUSH ESI — no locals, no `sub esp`.
 *
 * Ghidra mis-prototypes this as `void FUN_000c3a10(void)` and surfaces the
 * three arguments as in_stack_00000004/8/c phantoms.
 *
 * Callees (all in kb.json):
 *   0xcc560  = hs_macro_function_evaluate(function_index, thread_datum, init)
 *              -> result record ptr in EAX (NULL while evaluation pending)
 *   0x12aa80 = xbox_set_machine_name(record[0]) — MOV EDX,[EAX]; PUSH EDX at
 *              0xc3a2d before the CALL, so the record is dereferenced.
 *   0xcbf80  = hs_return(thread_datum, 0)
 *
 * The single ADD ESP,0xc at 0xc3a3c coalesces the cleanup for both tail calls
 * (1 dword + 2 dwords); the ARG_COUNT hazard this raises on hs_return is a
 * false positive — the disassembly shows exactly two pushes for it. */
void FUN_000c3a10(int16_t function_index, int thread_datum, char init)
{
  const char **record;

  record = (const char **)hs_macro_function_evaluate(function_index,
                                                     thread_datum, init);
  if (record != 0) {
    xbox_set_machine_name(*record);
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

/* 0xc3db0 — Find a scenario tag reference by tag index. Iterates the references
 * tag_block at scenario+0x4b4 (element size 0x28), comparing element+0x24.
 * Returns the zero-based reference index, or -1 if not found. */
int16_t hs_find_tag_reference_by_index(int tag_index)
{
  int16_t i;
  char *scenario;
  int *references;
  void *element;

  if (*(int *)0x326a08 != -1) {
    scenario = (char *)global_scenario_get();
    references = (int *)(scenario + 0x4b4);
    i = 0;
    if (*references > 0) {
      do {
        element = tag_block_get_element(references, (int)i, 0x28);
        if (*(int *)((char *)element + 0x24) == tag_index)
          return i;
        i++;
      } while ((int)i < *references);
    }
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

/* 0xc4010 — Comparator over an array of `char *` (qsort/bsearch shape).
 * Both arguments are pointers into that array, so each is dereferenced once
 * before the case-insensitive compare.  Ghidra prototypes this as
 * `void FUN_000c4010(void)` and drops the implicit EAX return: the function
 * has no epilogue of its own beyond POP EBP/RET, so crt_stricmp's EAX falls
 * through as the return value. */
int FUN_000c4010(const char **a, const char **b)
{
  return crt_stricmp(*a, *b);
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

/* 0xc4770 — qsort comparator over an array of file_ref_t (stride 0x10c), used
 * by hs_needs_recompile to sort the .hsc files returned by find_files.  Each
 * reference is expanded with flag 4 (name only) into its own 256-byte stack
 * buffer, then the two names are compared case-insensitively.
 *
 * Ghidra prototypes this as `void FUN_000c4770(void)` and drops both stack
 * params (read as MOV ECX,[EBP+8] / MOV EAX,[EBP+0xc]) plus the implicit EAX
 * return: there is no write to EAX after CALL crt_stricmp, so its result falls
 * straight through the MOV ESP,EBP / POP EBP / RET epilogue.  Same shape as the
 * sibling comparator FUN_000c4010 above.  Argument order is load-bearing for
 * the sort direction: the first parameter's name is crt_stricmp's first arg. */
int FUN_000c4770(file_ref_t *a, file_ref_t *b)
{
  char name_a[256];
  char name_b[256];

  file_reference_get_name(a, 4, name_a);
  file_reference_get_name(b, 4, name_b);
  return crt_stricmp(name_a, name_b);
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
        (int (*)(const void *, const void *))FUN_000c4770);

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

/* 0xc4b00 — Look up a scenario script by name and start its thread.
 * Resolves the name through hs_find_script_by_name, then fetches the script
 * element from the scripts tag_block at scenario+0x49c (element size 0x5c) and
 * hands the thread/script index at element+0x24 to hs_runtime_execute.
 *
 * Ghidra prototypes this `void FUN_000c4b00(void)` and drops both the stack
 * parameter (MOV EAX,[EBP+8] at 000c4b03) and the bool return (MOV AL,1 on the
 * found path at 000c4b37 vs XOR AL,AL at 000c4b3b).  hs_runtime_execute's own
 * int return is discarded here — the returned flag is the found/not-found
 * status.  The ADD ESP,0x10 at 000c4b34 is one combined cdecl cleanup for
 * tag_block_get_element (0xc) plus hs_runtime_execute (0x4). */
bool hs_evaluate_by_name(const char *name)
{
  int16_t script_index;
  void *element;

  script_index = hs_find_script_by_name(name);
  if (script_index != -1) {
    /* global_scenario_get() is evaluated *inside* the argument list on purpose:
     * cdecl pushes right-to-left, so 0x5c and the index are pushed first and
     * the scenario pointer is produced last (CALL / ADD EAX,0x49c / PUSH EAX),
     * which is the original's instruction order.  Hoisting it into a local
     * forces the index to be spilled into a callee-saved register across the
     * call, which the original never does. */
    element = tag_block_get_element(
      (void *)((char *)global_scenario_get() + 0x49c), (int)script_index, 0x5c);
    hs_runtime_execute(*(int *)((char *)element + 0x24));

    return 1;
  }

  /* The not-found epilogue is the out-of-line tail block (XOR AL,AL at
   * 000c4b3b) reached by the JZ at 000c4b12 — written as an if/return rather
   * than an early `return 0` so the bail-out stays out of line. */
  return 0;
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

/* 0xc4bb0 — HS script function handler: random real in range.
 * Evaluates the macro arguments to get (min, max) reals from the
 * hs_macro_function_evaluate result block (two 4-byte cells: [EAX] = min,
 * [EAX+4] = max).  Advances the global random seed and returns a random
 * real in [min, max] to the HS thread.
 *
 * The result is committed to the thread as RAW 4-byte BITS, not as an
 * integer conversion: the original does FSTP [EBP-8]; MOV EDX,[EBP-8];
 * PUSH EDX.  There is no _ftol2 / truncation anywhere in the reference, so
 * the value is staged through a union rather than cast. */
void FUN_000c4bb0(int16_t function_index, int thread_datum, char init)
{
  float *result;
  union {
    float real;
    int bits;
  } value;

  result =
    (float *)hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != NULL) {
    value.real =
      random_real_range(get_global_random_seed_address(), result[0], result[1]);
    hs_return(thread_datum, value.bits);
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
 * evaluation succeeds (non-zero return), forwards the result record's first
 * dword to hs_help (MOV EDX,[EAX]; PUSH EDX at 0xc502c before the CALL; the
 * callee reads it at [EBP+8]) and returns void to the HS thread. */
void FUN_000c5010(int16_t function_index, int thread_datum, char init)
{
  int result;

  result = hs_macro_function_evaluate(function_index, thread_datum, init);
  if (result != 0) {
    hs_help(*(int *)result);
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
