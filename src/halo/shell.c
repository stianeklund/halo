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
  result = ((char (*)(void))0x1911b0)();
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
