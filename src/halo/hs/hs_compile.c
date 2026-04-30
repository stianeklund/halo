/* Type-check an HS syntax node (0xc7d80).
 * If the node is untyped (type==0), sets its type to check_type and
 * dispatches to FUN_000c73a0 (function-call nodes) or FUN_000c74c0
 * (expression nodes) based on the node's flag bit 0. Returns true
 * if the node was already typed. */
bool hs_type_check(int datum_index, int16_t check_type)
{
  char *node;

  node = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);

  if (*(int *)0x46b6fc != 0) {
    display_assert("!hs_compile_globals.error",
                   "c:\\halo\\SOURCE\\hs\\hs_compile.c", 0x48e, 1);
    system_exit(-1);
  }

  if (!((check_type >= 4 && check_type <= 0x30) || check_type == 1 ||
        check_type == 0)) {
    display_assert(
      "hs_type_valid(expected_type) || expected_type==_hs_special_form"
      " || expected_type==_hs_unparsed",
      "c:\\halo\\SOURCE\\hs\\hs_compile.c", 0x491, 1);
    system_exit(-1);
  }

  if (*(int16_t *)(node + 4) != 0)
    return true;

  *(int16_t *)(node + 4) = check_type;

  {
    char *node2 = (char *)datum_get(*(data_t **)0x5aa6c8, datum_index);
    if (*(uint8_t *)(node2 + 6) & 1) {
      *(int16_t *)(node + 2) = check_type;
      return FUN_000c73a0(datum_index);
    }
  }

  return FUN_000c74c0(datum_index);
}
