/* shell_initialize (0x1910f0)
 *
 * Initializes the shell subsystem. First calls an unconditional pre-init
 * routine (0x8d830), then calls an inner create/setup helper (0x1911b0)
 * that returns a success flag. If that succeeds, calls four more subsystem
 * init routines in sequence; a fifth (0x17c790) returns a bool. If that
 * also succeeds, two additional routines are called (0xd01c0, 0x1cc710)
 * and the return value is set to true (1). An unconditional post-init call
 * (0x191210 -> 0x1bdc60) follows regardless of the inner bool. If the
 * outer create failed the whole block is skipped and false is returned.
 *
 * Disasm:
 *   001910f0: PUSH EBX
 *   001910f1: XOR  BL,BL
 *   001910f3: CALL 0x0008d830
 *   001910f8: CALL 0x001911b0       ; inner create -> bool in AL
 *   001910fd: TEST AL,AL
 *   001910ff: JZ   0x0019112f       ; skip all if create failed
 *   00191101: CALL 0x0008f370
 *   00191106: CALL 0x001b98c0       ; thunk -> 0x1bdb10
 *   0019110b: CALL 0x0010b5c0
 *   00191110: CALL 0x001c0070
 *   00191115: CALL 0x0017c790       ; -> bool in AL
 *   0019111a: TEST AL,AL
 *   0019111c: JZ   0x0019112a
 *   0019111e: CALL 0x000d01c0
 *   00191123: CALL 0x001cc710
 *   00191128: MOV  BL,0x1
 *   0019112a: CALL 0x00191210       ; thunk -> 0x1bdc60 (always if outer taken)
 *   0019112f: MOV  AL,BL
 *   00191131: POP  EBX
 *   00191132: RET
 */
int shell_initialize(void)
{
  char result;
  int success;

  success = 0;
  ((void (*)(void))0x8d830)();
  result = (char)FUN_001911b0();
  if (result != '\0') {
    ((void (*)(void))0x8f370)();
    ((void (*)(void))0x1b98c0)();
    ((void (*)(void))0x10b5c0)();
    ((void (*)(void))0x1c0070)();
    result = ((char (*)(void))0x17c790)();
    if (result != '\0') {
      ((void (*)(void))0xd01c0)();
      ((void (*)(void))0x1cc710)();
      success = 1;
    }
    ((void (*)(void))0x191210)();
  }
  return success;
}

/* shell_dispose (0x191140)
 *
 * Tears down the shell subsystem in sequence. Calls eight subsystem dispose /
 * shutdown routines and then tail-calls the final one (0x8d850 -> 0x8f1e0).
 * One callee (0x191220) is a single-RET stub that does nothing.
 * No stack frame, no arguments, no return value.
 *
 * Disasm (all instructions, no prologue/epilogue):
 *   00191140: CALL 0x001cb820
 *   00191145: CALL 0x000cf490
 *   0019114a: CALL 0x0017c940   ; thunk -> 0x155b90
 *   0019114f: CALL 0x0010b5d0
 *   00191154: CALL 0x001b98d0   ; thunk -> 0x1bc360
 *   00191159: CALL 0x0008f1f0   ; thunk -> 0x92440
 *   0019115e: CALL 0x00191220   ; single-RET stub
 *   00191163: JMP  0x0008d850   ; tail call, thunk -> 0x8f1e0
 */
void shell_dispose(void)
{
  ((void (*)(void))0x1cb820)();
  ((void (*)(void))0xcf490)();
  ((void (*)(void))0x17c940)();
  ((void (*)(void))0x10b5d0)();
  ((void (*)(void))0x1b98d0)();
  ((void (*)(void))0x8f1f0)();
  ((void (*)(void))0x191220)();
  ((void (*)(void))0x8d850)();
}

/* shell_application_is_paused (0x191170)
 *
 * Returns the current application-paused state. A non-zero value means the
 * shell has marked the application as paused (e.g. the game loop should skip
 * its normal per-frame work). The flag lives at 0x4d8a84 (one byte, bool).
 *
 * Disasm:
 *   00191170: MOV AL,[0x004d8a84]
 *   00191175: RET
 */
bool shell_application_is_paused(void)
{
  return *(bool *)0x4d8a84;
}

/* FUN_00191180 (0x191180)  [inferred: shell_application_set_paused]
 *
 * Setter for the application-paused flag at 0x4d8a84 (the byte read by
 * shell_application_is_paused at the adjacent 0x191170). Debounced: only the
 * low byte of the argument is compared against the current flag value. If it
 * is unchanged, the function returns immediately. If it differs, the global
 * is updated and FUN_00191230 is notified with the full dword argument via a
 * normal cdecl call. FUN_00191230 is a bare-RET stub in this build (single
 * 0xC3 at 0x191230) that ignores its argument, but the call is preserved with
 * its argument to match the original instruction sequence.
 *
 * Disasm:
 *   00191180: PUSH EBP / MOV EBP,ESP
 *   00191183: MOV  EAX,[EBP+0x8]          ; arg -> EAX
 *   00191186: CMP  byte ptr [0x4d8a84],AL ; compare low byte vs flag
 *   0019118c: JZ   0x19119c               ; unchanged -> return
 *   0019118e: PUSH EAX                     ; forward full dword (cdecl arg)
 *   0019118f: MOV  [0x4d8a84],AL          ; store low byte into flag
 *   00191194: CALL 0x191230               ; notify (stub)
 *   00191199: ADD  ESP,0x4                ; caller cleans 4 bytes (cdecl)
 *   0019119c: POP  EBP / RET
 */
void FUN_00191180(int param_1)
{
  if (*(char *)0x4d8a84 != (char)param_1) {
    *(char *)0x4d8a84 = (char)param_1;
    FUN_00191230(param_1);
  }
}

/* FUN_001911b0 (0x1911b0)  [inferred: shell demo-disc launch check]
 *
 * Checks whether the title was launched with XDEMOS launch data (i.e. booted
 * from a demo disc / demo launcher). Queries the Xbox launch-data page via the
 * XAPILIB import XGetLaunchInfo, passing the launch-data-type out-param and a
 * 3072-byte (0x300 dwords) LAUNCH_DATA buffer. On success XGetLaunchInfo
 * returns 0 and fills the buffer; the launch-data-type is also written to the
 * out-param.
 *
 * If the call succeeds (ret == 0) AND the launch-data-type is 0, the leading
 * bytes of the launch-data buffer are compared (csstrcmp) against "XDEMOS".
 * On an exact match, "xdemo " (6 chars) is concatenated into the shell command
 * string buffer at 0x4d8a88 (max_size 7 = 6 chars + NUL), and the byte at
 * 0x4d8a8f (command-buffer base + 7) is cleared to 0. The command buffer base
 * at 0x4d8a88 is the same buffer returned by shell_get_command_line (0x191240).
 *
 * XGetLaunchInfo is __stdcall (RET 0x8 at 0x1d257b); called via a typed
 * stdcall function-pointer cast to match its callee-cleanup ABI, consistent
 * with the XAPILIB import call style used in input_xbox.c. csstrcmp/csstrcat
 * are cdecl and are called by their kb names.
 *
 * The function unconditionally returns 1 (MOV AL,0x1 on every path); the
 * caller shell_initialize treats this as a success bool (TEST AL,AL; JZ).
 *
 * Disasm:
 *   001911b0: PUSH EBP / MOV EBP,ESP / SUB ESP,0xc04
 *   001911b9: LEA EAX,[EBP-0xc04]            ; &launch_data buffer (3072 bytes)
 *   001911bf: PUSH EAX                        ; arg2 = pLaunchData
 *   001911c0: LEA ECX,[EBP-0x4]              ; &launch_data_type
 *   001911c3: PUSH ECX                        ; arg1 = pdwLaunchDataType
 *   001911c4: CALL 0x1d2518                   ; XGetLaunchInfo (__stdcall)
 *   001911c9: TEST EAX,EAX / JNZ 0x191207     ; skip if ret != 0
 *   001911cd: MOV EAX,[EBP-0x4] / TEST / JNZ  ; skip if launch_data_type != 0
 *   001911d4: LEA EDX,[EBP-0xc04]
 *   001911da: PUSH 0x2b2550 ("XDEMOS")        ; arg2
 *   001911df: PUSH EDX                         ; arg1 = buffer
 *   001911e0: CALL 0x8dcb0                     ; csstrcmp (cdecl)
 *   001911e5: ADD ESP,0x8
 *   001911e8: TEST EAX,EAX / JNZ 0x191207      ; skip if not equal
 *   001911ec: PUSH 0x7                          ; arg3 = max_size
 *   001911ee: PUSH 0x2b2548 ("xdemo ")         ; arg2 = source
 *   001911f3: PUSH 0x4d8a88                     ; arg1 = destination
 *   001911f8: CALL 0x8dd30                      ; csstrcat (cdecl)
 *   001911fd: ADD ESP,0xc
 *   00191200: MOV byte ptr [0x4d8a8f],0x0       ; base+7 cleared
 *   00191207: MOV AL,0x1
 *   00191209: MOV ESP,EBP / POP EBP / RET
 */
int FUN_001911b0(void)
{
  int launch_data_type;
  char launch_data[3072];

  if ((((int(__stdcall *)(int *, void *))0x1d2518)(&launch_data_type,
                                                   launch_data) == 0) &&
      (launch_data_type == 0)) {
    if (csstrcmp(launch_data, "XDEMOS") == 0) {
      csstrcat((char *)0x4d8a88, "xdemo ", 7);
      *(char *)0x4d8a8f = 0;
    }
  }
  return 1;
}

/* shell_get_command_line (0x191240)
 *
 * Returns a pointer to the static command-line string buffer at 0x4d8a88.
 * The shell stores the application's startup command line here during init.
 *
 * Disasm:
 *   00191240: MOV EAX,0x4d8a88
 *   00191245: RET
 */
char *shell_get_command_line(void)
{
  return (char *)0x4d8a88;
}
