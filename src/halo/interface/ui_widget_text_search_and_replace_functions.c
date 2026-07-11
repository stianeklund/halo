/*
 * ui_widget_text_search_and_replace_functions.c
 *
 * TU: c:\halo\SOURCE\interface\ui_widget_text_search_and_replace_functions.c
 *   (recovered from the __FILE__ assert string @ 0x0028a748, confirmed).
 *
 * Widget "text search and replace" resolver dispatch.  A UI text widget may
 * embed reference tokens that are resolved at display time by dispatching
 * through a small function-pointer table indexed by a per-token function
 * index; each resolver returns the wide-character replacement text.
 *
 * Recovered layout (cachebeta.xbe, v01.10.12.2276):
 *
 *   0x000f5290  ui_widget_text_search_and_replace_function_invoke  (this fn)
 *   0x000f52e0  text_search_and_replace_function_table[0]  (FUN_000f52e0)
 *   0x000f52f0  text_search_and_replace_function_table[1]  (FUN_000f52f0)
 *
 *   text_search_and_replace_function_table @ 0x0031e5a4
 *     array[2] of  wchar_t *(*)(void *widget)
 *       [0] -> 0x000f52e0
 *       [1] -> 0x000f52f0
 *
 * Dispatcher ABI (0x000f5290, __cdecl):
 *   arg0  void *widget           [ESP+4]  asserted non-NULL ("widget", line 45)
 *   arg1  unsigned short index    [ESP+8]  signed (short) for the >=0 lower
 *                                          bound (JL) and unsigned for the <2
 *                                          upper bound (JNC); MOVSX for index
 *   ret   wchar_t *              the resolver's result, else the literal
 *                                L"<invalid>" (@ 0x0028a730) when the index
 *                                is outside the [0, 2) range.
 *
 * The resolver targets (FUN_000f52e0 / FUN_000f52f0) are reached through the
 * binary's own function-pointer table (HDATA extern), so they keep resolving
 * to the original ROM code until each is individually lifted.  The sole
 * argument passed at the indirect call site is the cdecl stack arg 'widget'
 * (PUSH ESI; CALL [EAX*4 + table]; ADD ESP,4) -- no register arguments.
 *
 * Sibling TU precedent: interface/ui_widget_game_data_input_functions.c
 * (ui_widget_game_data_function_invoke + ui_widget_game_data_function_table).
 */
wchar_t *
ui_widget_text_search_and_replace_function_invoke(void *widget,
                                                  unsigned short function_index)
{
  assert_halt(widget);

  if ((short)function_index >= 0 && function_index < 2) {
    return text_search_and_replace_function_table[(short)function_index](
      widget);
  }
  return L"<invalid>";
}
