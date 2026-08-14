void cinematic_initialize(void)
{
  cinematic_globals = (cinematic_globals_t *)game_state_malloc(
    "cinematic globals", 0, sizeof(cinematic_globals_t));
  assert_halt(cinematic_globals);
}

void cinematic_dispose(void)
{
}

void cinematic_initialize_for_new_map(void)
{
  csmemset(cinematic_globals, 0, sizeof(cinematic_globals_t));
  csmemset(&cinematic_globals->field_0c, 0xFF, 16);
}

void cinematic_dispose_from_old_map(void)
{
  cinematic_globals->unk_8 = false;
  cinematic_globals->in_progress = false;
}

/**
 * Enter cinematic mode: lock out player input and AI dialogue triggers, stamp
 * the start time, flag the cinematic as in progress, and clear live
 * projectiles.
 *
 * The original re-reads the cinematic_globals pointer before each field store
 * (three separate loads of [0x0044df00]), so the accesses are written out
 * individually here rather than cached in a local.
 */
void cinematic_start(void)
{
  player_input_enable(false);
  ai_globals_dialogue_triggers_enabled(0);
  cinematic_globals->unk_8 = true;
  cinematic_globals->field_04 = game_time_get();
  cinematic_globals->in_progress = true;
  projectiles_delete_all();
}

bool cinematic_can_be_skipped(void)
{
  return cinematic_globals->can_be_skipped;
}

/**
 * Allow the player to skip the running cinematic.
 *
 * Single byte store into cinematic_globals+0x0A; the global is a pointer, so
 * the store goes through the loaded pointer (MOV EAX,[0x44df00]; MOV byte ptr
 * [EAX+0xA],1).
 */
void cinematic_skip_start(void)
{
  cinematic_globals->can_be_skipped = true;
}

/**
 * Disallow skipping the running cinematic.
 *
 * Mirror of cinematic_skip_start: a single byte store of 0 into
 * cinematic_globals+0x0A through the loaded global pointer
 * (MOV EAX,[0x44df00]; MOV byte ptr [EAX+0xA],0; RET). The original does not
 * null-check the pointer, so neither do we.
 */
void cinematic_skip_stop(void)
{
  cinematic_globals->can_be_skipped = false;
}

/**
 * Show or hide the cinematic letterbox bars.
 *
 * Stores the flag unconditionally at cinematic_globals+0x08, then — only when
 * turning the letterbox on — stamps the current game time at +0x04. The
 * original loads [0x0044df00] twice (once for the byte store, again after the
 * game_time_get() call), so the pointer is re-read here instead of cached.
 */
void cinematic_show_letterbox(char show)
{
  cinematic_globals->unk_8 = show;
  if (show) {
    cinematic_globals->field_04 = game_time_get();
  }
}

/**
 * Draw a solid-colour screen-space quad using the letterbox sprite bitmap.
 *
 * `rect` is the engine's standard 2D bounds record — four int16_t stored
 * {top, left, bottom, right} (y0, x0, y1, x1).  The four corners are emitted
 * x-first, so the corner pairs are (left,top), (right,top), (right,bottom),
 * (left,bottom).
 *
 * The sprite bitmap comes from the scenario globals tag block: element 0 of
 * game_globals+0x134 (0x1ac bytes per element) holds a 'bitm' tag index at
 * +0xb8; sprite element 1 of that bitmap's block at +0x60 (0x30 bytes per
 * element) is handed to the rasterizer.
 *
 * Faithful structural lift:
 *  - The global_scenario_get() result is discarded by the original (EAX is
 *    immediately overwritten by game_globals_get); the call is kept because it
 *    asserts/has side effects.
 *  - When the tag block count at game_globals+0x134 is zero the original sets
 *    the pointer to NULL and then dereferences it at +0xb8.  That null deref is
 *    reproduced rather than guarded.
 *  - DAT_00325652 is a 16-bit render-state global raised to 8 around the sprite
 *    render and cleared afterwards.
 *
 * Store offsets in the 0x8c-byte rasterizer descriptor are derived from the
 * disassembly relative to its base at EBP-0xb0 (offset = 0xb0 - |EBP disp|):
 *   +0x00 dword 0            (EBP-0xb0)
 *   +0x0c sprite element ptr (EBP-0xa4)
 *   +0x28/+0x2c 1.0f         (EBP-0x88 / EBP-0x84)
 *   +0x40/+0x44 1.0f         (EBP-0x70 / EBP-0x6c)
 *   +0x88 word 0             (EBP-0x28)
 *   +0x8a byte 0             (EBP-0x26)
 */
void draw_quad(int16_t *rect, int color)
{
  void *sprite;
  void *globals;
  float *src;
  float *dst;
  float y;
  int i;
  /* Four (x,y) screen-space corner pairs at EBP-0x24 .. EBP-0x8. */
  float corners[8];
  unsigned char render_data[0x8c];
  /* Four sprite vertices, 5 dwords each (x, y, 0, 0, colour), 0x14 stride. */
  float vertices[20];

  global_scenario_get();
  globals = game_globals_get();
  if (*(int *)((char *)globals + 0x134) != 0) {
    globals =
      tag_block_get_element((void *)((char *)globals + 0x134), 0, 0x1ac);
  } else {
    globals = (void *)0;
  }
  /* The 0x30 / 1 pushes precede the tag_get call in the original because the
   * tag_get result is the first (last-pushed) argument of this outer call. */
  sprite = tag_block_get_element(
    (void *)((char *)tag_get(0x6269746d /* 'bitm' */,
                             *(int *)((char *)globals + 0xb8)) +
             0x60),
    1, 0x30);

  *(int16_t *)0x325652 = 8;

  corners[0] = (float)rect[1]; /* left  */
  corners[1] = (float)rect[0]; /* top   */
  corners[2] = (float)rect[3]; /* right */
  corners[3] = (float)rect[0];
  corners[4] = (float)rect[3];
  corners[5] = (float)rect[2]; /* bottom */
  corners[6] = (float)rect[1];
  corners[7] = (float)rect[2];

  /* Original walks the destination from vertex+0x08 with a 0x14 stride and the
   * source pair-wise; the zero and colour dwords are integer stores. */
  src = corners;
  dst = vertices + 2;
  i = 4;
  do {
    dst[-2] = src[0];
    y = src[1];
    *(int *)(dst + 2) = color;
    *(int *)(dst + 0) = 0;
    *(int *)(dst + 1) = 0;
    dst[-1] = y;
    src = src + 2;
    dst = dst + 5;
    i = i - 1;
  } while (i != 0);

  csmemset(render_data, 0, 0x8c);
  *(unsigned short *)(render_data + 0x88) = 0;
  *(float *)(render_data + 0x44) = 1.0f;
  *(float *)(render_data + 0x40) = 1.0f;
  *(float *)(render_data + 0x2c) = 1.0f;
  *(float *)(render_data + 0x28) = 1.0f;
  *(unsigned int *)(render_data + 0x00) = 0;
  *(unsigned char *)(render_data + 0x8a) = 0;
  *(void **)(render_data + 0x0c) = sprite;
  rasterizer_sprites_render(render_data, vertices);

  *(int16_t *)0x325652 = 0;
}

/**
 * Force the cinematic title record to `title_index`, clearing the companion
 * 16-bit field at +0x0E.
 *
 * Both stores are word-sized (`MOV word ptr [EAX+0xc],CX` /
 * `MOV word ptr [EDX+0xe],0x0`), so the fields are int16_t, not int.
 *
 * The original loads the cinematic_globals pointer twice — once into EAX for
 * the +0x0C store, again into EDX for the +0x0E store — so the two stores are
 * written as independent statements rather than hoisted through a cached local
 * pointer.
 */
void cinematic_force_title(int16_t title_index)
{
  cinematic_globals->field_0c = title_index;
  cinematic_globals->field_0e = 0;
}

/**
 * Suppress (or re-enable) object creation from the BSP while a cinematic runs.
 *
 * Whole body is seven instructions: the byte parameter is read from [EBP+8],
 * the cinematic_globals pointer is loaded exactly once into ECX, and the byte
 * is stored at +0x0B (`MOV byte ptr [ECX+0xb],AL`). The store is byte-wide, so
 * the field must stay one byte; there are no locals (no `sub esp`) and the
 * original does not null-check the pointer, so neither do we.
 */
void cinematic_suppress_bsp_object_creation(unsigned char suppress)
{
  cinematic_globals->suppress_bsp_object_creation = suppress;
}

/* global_rasterizer_model_ambient_reflection_tint (DAT_0047e4d0): 0x10-byte
 * game-state allocation holding the model ambient reflection tint. Name is
 * taken verbatim from the assert message at 0x17c7aa (#cond string); the same
 * macro is defined in src/halo/rasterizer/rasterizer.c. The pointer is only
 * populated once rasterizer_window_set_fog has run, hence the null check. */
#define global_rasterizer_model_ambient_reflection_tint (*(void **)0x47e4d0)

/**
 * Leave cinematic mode: hand player input and AI dialogue triggers back to the
 * game, clear the in-progress flag, and reset the rasterizer's cinematic
 * screen-effect state.
 *
 * Store/call ordering is taken from the disassembly, not the decompiler:
 *   0x93050 MOV EAX,[0x44df00] / 0x93057 MOV byte [EAX+8],0   <- store first
 *   0x93055 PUSH 1 / 0x9305b CALL 0xba6d0                     (player input)
 *   0x93060 PUSH 1 / 0x93062 CALL 0x3f7b0                     (AI dialogue)
 *   0x9306d ADD ESP,8   <- ONE coalesced cleanup for BOTH 1-arg calls above,
 *                          not a 2-arg call; do not "fix" either declaration.
 *   0x93067 MOV ECX,[0x44df00] / 0x93070 MOV byte [ECX+9],0
 * The cinematic_globals pointer is loaded twice (EAX then ECX) rather than
 * cached across the two calls, so the two field stores are written out
 * separately here as well.
 *
 * Both flag stores are byte-wide (`MOV byte ptr`), so the fields must stay one
 * byte each. The csmemset length 0x10 is the literal PUSH 0x10 immediate.
 *
 * FUN_0017dec0 takes one stack argument (PUSH 0 / CALL / ADD ESP,4 at
 * 0x9308f); Ghidra's decompile dropped it and the kb declaration said (void).
 * check_arg_counts reports observed={1:1} over three call sites.
 *
 * The final call is a tail JMP to 0xe8db0 in the original; a plain trailing
 * call reproduces it.
 */
void cinematic_stop(void)
{
  cinematic_globals->unk_8 = false;
  player_input_enable(true);
  ai_globals_dialogue_triggers_enabled(1);
  cinematic_globals->in_progress = false;
  FUN_0017d950();
  if (global_rasterizer_model_ambient_reflection_tint != NULL) {
    csmemset(global_rasterizer_model_ambient_reflection_tint, 0, 0x10);
  }
  FUN_0017dec0(0);
  ui_widget_display_deferred_errors();
}

bool cinematic_in_progress(void)
{
  return cinematic_globals->in_progress;
}

/* Included here rather than at the top of the file on purpose: `assert_halt`
 *
 * embeds `__LINE__`, so inserting a line above cinematic_initialize would
 *
 * shift the assert line immediate baked into its already-verified codegen. */
#include "x87_math.h"

/* Chapter-title display queue at cinematic_globals+0x0C.
 *
 *
 * cinematic_initialize_for_new_map memsets 16 bytes from +0x0C to 0xFF, and
 *
 * this function scans four records of stride 4 (`CMP word ptr
 *
 * [ECX+ESI*4+0xC],DX` with DX = -1, loop bound `CMP AX,4`), writing the title

 * * index at +0x0C and the countdown at +0x0E of the record it finds.  So
 * those
 * 16 bytes are four {int16 index; int16 delay} records, not the two
 * scalars
 * cinematic_globals_t declares at 0x0C/0x0E — those two are record
 * 0.  Kept as
 * an overlay instead of re-cutting the struct, which would
 * disturb
 * cinematic_force_title's already-scored codegen.
 *
 * The macro
 * deliberately re-reads the cinematic_globals pointer at every use:
 * the
 * original loads [0x0044df00] into ECX for the scan and the +0x0C store,
 *
 * then loads it again into EDX for the +0x0E store (0x93129). */
typedef struct {
  int16_t index; ///< offset=0x00 — chapter-title tag-block index, -1 = free
  int16_t delay; ///< offset=0x02 — negated countdown, in ticks
} cinematic_title_slot_t;

#define cinematic_title_slots \
  ((cinematic_title_slot_t *)&cinematic_globals->field_0c)

/**
 * Queue a chapter title to be shown after a delay.
 *
 * Scans the
 * four-entry queue for a free record (index == -1) and stores the
 * title
 * index plus a negated tick countdown there.  If every record is taken
 * the
 * title is dropped with an error naming it.
 *
 * Notes taken from the
 * disassembly rather than the decompiler:
 *  - Ghidra renders the two cdecl
 * parameters as `in_stack_00000004` /
 *    `in_stack_00000008`; they are the
 * real stack args at [EBP+8] and
 *    [EBP+0xC].  Only the low 16 bits of
 * `index` are ever used (`MOV DX,word
 *    ptr [EBP+8]` for the store, `MOVSX
 * EAX,word ptr [EBP+8]` for the error
 *    path), hence the int16_t casts —
 * the kb declaration stays `int` because
 *    the caller in hs.c passes an
 * int.
 *  - cdecl arg mis-group at 0x930da: `PUSH 0x60; PUSH EAX; CALL
 *
 * global_scenario_get` looks like a two-argument call, but
 *
 * global_scenario_get takes none — both pushes belong to the following
 *
 * tag_block_get_element.  Likewise the single `ADD ESP,0x18` at the end is
 *
 * one coalesced cleanup for tag_block_get_element (3 dwords) and error
 *    (3
 * dwords), not a six-argument call.
 *  - The delay conversion is an inline
 * `FLD; FISTP` (round-to-nearest), not a
 *    truncating `_ftol2` call, so
 * x87_round_to_int is used instead of an (int)
 *    cast; the two scratch
 * dwords it needs are the frame's `SUB ESP,8`.  The
 *    32-bit result is
 * negated before the 16-bit store (`NEG ECX` / `MOV word
 *    ptr
 * [EDX+EAX*4+0xE],CX`).
 *  - The `slot < 4` retest after the loop is redundant
 * (the loop can only fall
 *    out with slot == 4) but is present in the
 * original as `CMP AX,4; JGE` at
 *    0x93102, so it is kept.
 */
void cinematic_set_title_delayed(int index, float value)
{
  int16_t slot;

  for (slot = 0; slot < 4; slot++) {
    if (cinematic_title_slots[slot].index == -1) {
      break;
    }
  }

  if (slot < 4) {
    cinematic_title_slots[slot].index = (int16_t)index;
    /* TICKS_PER_SECOND; spelled as a literal (as in actions.c/players.c) so
     * the multiply becomes a relocated .rdata pool load like the reference's
     * `fmuls <pool>`, not an absolute `fmuls 0x253394`. */
    cinematic_title_slots[slot].delay =
      (int16_t)(-x87_round_to_int(value * 30.0f));
    return;
  }

  error(2, "no free chapter title slots to display title '%s'",
        (char *)tag_block_get_element((char *)global_scenario_get() + 0x4FC,
                                      (int16_t)index, 0x60) +
          4);
}

/* Screen-bounds rect at 0x50657c: four int16 in the engine's standard 2D
 *
 * bounds layout {top, left, bottom, right} — the same record draw_quad above
 *
 * consumes.  Cross-checked against game_engine.c/hud.c, which offset by
 *
 * [0x50657e] on x and [0x50657c] on y. */
#define screen_bounds_top (*(int16_t *)0x50657c)
#define screen_bounds_left (*(int16_t *)0x50657e)
#define screen_bounds_bottom (*(int16_t *)0x506580)
#define screen_bounds_right (*(int16_t *)0x506582)

/* Interface globals pointer (0x46bd0c).  +0x54 is the chapter-title font tag
 *
 * index (same field game_engine.c reads), +0x2dc a fallback 2D bounds record
 *
 * used when the title tag's own rect is degenerate. */
#define interface_globals (*(char **)0x46bd0c)

/**
 * Per-frame cinematic overlay: ramp the letterbox bars in/out and draw any

 * * queued chapter titles.
 *
 * Letterbox half:
 *  - Runs only while the bars
 * are up or still retracting
 *    (`unk_8 != 0 || fraction > 0`) and the UI
 * widget stack is idle.  The
 *    `ui_widgets_active` test consumes AL (`CALL
 * 0xe3d70; TEST AL,AL; JNE`),
 *    and 0xe3d70 does `XOR AL,AL` before its
 * early out, so its kb declaration
 *    is `bool`, not `void` (doctrine
 * section 16, void-EAX).
 *  - `fraction` ramps by elapsed_ticks/30 s and is
 * clamped with a ternary
 *    select (both arms `FLD`, one shared `FSTP`), not
 * a compare-and-store.
 *  - The bar thickness is `(bottom - top) * (fraction *
 * 0.125f)`; the inner
 *    product is formed first (0x93201 `FLD [ECX]; FMUL
 * [0x268ed0]`) and only
 *    multiplied by the screen height at 0x93277, so
 * the parenthesisation is
 *    load-bearing.
 *  - Both quads reuse one rect
 * at EBP-0x28.  Store offsets taken from the
 *    disassembly, not the
 * decompiler (which folds them into CONCAT22):
 *      quad 1: [-0x28]=top,
 * [-0x26]=left, [-0x24]=top+bar,    [-0x22]=right
 *      quad 2:
 * [-0x28]=bottom-bar, [-0x26]=left, [-0x24]=bottom, [-0x22]=right
 *    i.e.
 * assignment order left, right, top, bottom in both cases.
 *  - Every
 * int->rect conversion goes through `FILD; FSTP tmp; FLD tmp; FISTP`
 *    — an
 * inline round-to-nearest, so x87_round_to_int is used rather than a
 *
 * truncating (int) cast.
 *
 * Title half: walks the four {index, delay} slots
 * at cinematic_globals+0x0C as
 * a strength-reduced byte offset plus a
 * down-counter (0x9334e `MOV
 * [EBP-0x10],4`; the decompiler's `5.60519e-45`
 * is that integer 4 reinterpreted
 * as a float).  For each live slot it
 * fetches the scenario cinematic-title
 * element (0x60 bytes at
 * scenario+0x4fc), fades the tag colour in/out over
 *
 * fade_in/up_time/fade_out, dims pure white to 0.8, draws the string and ages

 * * the slot, freeing it once the title has fully faded out.
 */
void cinematic_render(void)
{
  int now;
  int delta;
  float bar;
  float fade;
  int age;
  int offset;
  int count;
  int font_tag;
  int ustr_index;
  int elapsed;
  cinematic_title_slot_t *slot;
  char *elem;
  int16_t *bounds;
  int *strings;
  int16_t rect[4];
  float color[4];

  if ((cinematic_globals->unk_8 ||
       cinematic_globals->letterbox_fraction > 0.0f) &&
      !ui_widgets_active()) {
    now = game_time_get();
    delta = now - cinematic_globals->field_04;
    cinematic_globals->field_04 = now;
    /* TICKS_PER_SECOND spelled as a literal so the scale becomes a relocated

     * * .rdata pool load, as in cinematic_set_title_delayed above. */
    if (cinematic_globals->unk_8) {
      cinematic_globals->letterbox_fraction =
        cinematic_globals->letterbox_fraction + (float)delta * (1.0f / 30.0f);
      cinematic_globals->letterbox_fraction =
        (cinematic_globals->letterbox_fraction > 1.0f) ?
          1.0f :
          cinematic_globals->letterbox_fraction;
    } else {
      cinematic_globals->letterbox_fraction =
        cinematic_globals->letterbox_fraction - (float)delta * (1.0f / 30.0f);
      cinematic_globals->letterbox_fraction =
        (cinematic_globals->letterbox_fraction > 0.0f) ?
          cinematic_globals->letterbox_fraction :
          0.0f;
    }

    if (cinematic_globals->letterbox_fraction > 0.0f) {
      bar = (float)(screen_bounds_bottom - screen_bounds_top) *
            (cinematic_globals->letterbox_fraction * 0.125f);

      rect[1] = (int16_t)x87_round_to_int((float)screen_bounds_left);
      rect[3] = (int16_t)x87_round_to_int((float)screen_bounds_right);
      rect[0] = (int16_t)x87_round_to_int((float)screen_bounds_top);
      rect[2] = (int16_t)x87_round_to_int((float)screen_bounds_top + bar);
      draw_quad(rect, (int)0xff000000);

      rect[1] = (int16_t)x87_round_to_int((float)screen_bounds_left);
      rect[3] = (int16_t)x87_round_to_int((float)screen_bounds_right);
      rect[0] = (int16_t)x87_round_to_int((float)screen_bounds_bottom - bar);
      rect[2] = (int16_t)x87_round_to_int((float)screen_bounds_bottom);
      draw_quad(rect, (int)0xff000000);
    }
  }

  offset = 0xc;
  count = 4;
  do {
    slot = (cinematic_title_slot_t *)((char *)cinematic_globals + offset);
    if (slot->index != -1) {
      font_tag = *(int *)(interface_globals + 0x54);
      if (font_tag != -1) {
        elem = (char *)tag_block_get_element(
          (char *)global_scenario_get() + 0x4fc, slot->index, 0x60);
        ustr_index = *(int *)((char *)global_scenario_get() + 0x590);
        if (ustr_index != -1) {
          strings = (int *)tag_get(0x75737472 /* 'ustr' */, ustr_index);
          if (*(int16_t *)(elem + 0x30) >= 0 &&
              *(int16_t *)(elem + 0x30) < *strings) {
            /* Bounds rect at +0x28; a zero-width or zero-height rect falls

             * * back to the interface globals' default (0x933f6).  The second

             * * test is emitted off the bounds pointer (`CMP [EBX+4],[EBX]`),
             * so
             * it is written through `bounds` here as well. */
            bounds = (int16_t *)(elem + 0x28);
            fade = 1.0f;
            if (bounds[3] == bounds[1] || bounds[2] == bounds[0]) {
              bounds = (int16_t *)(interface_globals + 0x2dc);
            }
            if (!game_in_editor()) {
              age = slot->delay;
              /* The clamp is duplicated in both arms in the original (the

               * * "still up" path at 0x93472 pops the age and skips it

               * * entirely); MSVC tail-merges the two copies at 0x9343d. */
              if ((float)age < *(float *)(elem + 0x44)) {
                fade = (float)age / *(float *)(elem + 0x44);
                if (fade < 0.0f) {
                  fade = 0.0f;
                } else if (fade > 1.0f) {
                  fade = 1.0f;
                }
              } else if ((float)age > *(float *)(elem + 0x48)) {
                fade = 1.0f - ((float)age - *(float *)(elem + 0x48)) /
                                *(float *)(elem + 0x4c);
                if (fade < 0.0f) {
                  fade = 0.0f;
                } else if (fade > 1.0f) {
                  fade = 1.0f;
                }
              }
            }
            pixel32_to_real_argb_color(*(unsigned int *)(elem + 0x3c), color);
            color[0] = color[0] * fade;
            /* Pure white titles are dimmed to 0.8; the epsilon compare is

             * * against a qword pool entry holding (double)1e-4f, i.e. fabs()

             * * (double) rather than fabsf(). */
            if (fabs(color[1] - 1.0f) < 0.0001f &&
                fabs(color[2] - 1.0f) < 0.0001f &&
                fabs(color[3] - 1.0f) < 0.0001f) {
              if (color[1] > 0.8f) {
                color[1] = 0.8f;
              }
              if (color[2] > 0.8f) {
                color[2] = 0.8f;
              }
              if (color[3] > 0.8f) {
                color[3] = 0.8f;
              }
            }
            draw_string_set_font(
              font_tag, (unsigned short)(*(unsigned short *)(elem + 0x32) - 1),
              *(unsigned short *)(elem + 0x34), *(int *)(elem + 0x38), color);
            /* PIN(round(shadow_alpha * fade), 0, 255): the macro evaluates its

             * * argument three times, and so does the original (0x9353b,

             * * 0x9355f, 0x93581 each redo MOVZX/FILD/FMUL/FISTP). */
            rasterizer_text_set_shadow_color((
              const void
                *)((x87_round_to_int((float)*(unsigned char *)(elem + 0x43) *
                                     fade) < 0 ?
                      0 :
                      (x87_round_to_int((float)*(unsigned char *)(elem + 0x43) *
                                        fade) > 255 ?
                         255 :
                         x87_round_to_int(
                           (float)*(unsigned char *)(elem + 0x43) * fade)))
                     << 24 |
                   (*(unsigned int *)(elem + 0x40) & 0xffffff)));
            rasterizer_draw_string(
              bounds, (short *)0, (const void *)0, 0,
              (unsigned short *)FUN_0019d420(ustr_index,
                                             *(unsigned short *)(elem + 0x30)));
            rasterizer_text_set_shadow_color((const void *)0);

            elapsed = game_time_get_paused() ? 0 : game_time_get_elapsed();
            slot->delay = (int16_t)(slot->delay + elapsed);
            if (!game_in_editor() &&
                (float)slot->delay >=
                  *(float *)(elem + 0x4c) + *(float *)(elem + 0x48)) {
              slot->index = -1;
              slot->delay = -1;
            }
          }
        }
      }
    }
    offset += 4;
    count--;
  } while (count != 0);
}

/* 0x93640 - queue a cinematic title with no extra delay.
 *
 * Whole body, 19 bytes (0x93640-0x93652):
 *   PUSH EBP / MOV EBP,ESP
 *   MOV EAX,[EBP+0x8]   <- the incoming int parameter
 *   PUSH 0x0            <- pushed FIRST  => second (rightmost) argument
 *   PUSH EAX            <- pushed SECOND => first argument
 *   CALL 0x000930b0     (cinematic_set_title_delayed)
 *   ADD ESP,0x8         <- cdecl, two dwords
 *   POP EBP / RET       <- void, EAX is never set
 *
 * Ghidra decompiled this as `void FUN_00093640(void)`, which is wrong: the
 * MOV EAX,[EBP+0x8] proves one stack parameter is read and forwarded.  The
 * kb.json declaration already carries the corrected `(int value)` signature
 * and src/halo/hs/hs.c relies on it, so it is left untouched.
 *
 * The second argument is the integer immediate 0, whose bit pattern is
 * exactly 0.0f, and the callee's second parameter is `float`.  Writing the
 * literal `0.0f` reproduces the same `PUSH 0`; a runtime int-to-float
 * conversion here would be a lift bug (see lift-silent-bugs float-slot).
 *
 * No locals, so no _chkstk and the minimal `PUSH EBP; MOV EBP,ESP` frame.
 */
void FUN_00093640(int value)
{
  cinematic_set_title_delayed(value, 0.0f);
}

/* 0x93660 - dispatches a recorded-animation update based on a mode byte in the

 * * caller-supplied state block. Mode 0 does nothing; modes 1..3 route to
 *
 * FUN_00094ba0, mode 4 routes to FUN_00094290. Both handlers receive the two
 *
 * pass-through arguments plus the zero-extended byte at state+0x22.
 *
 * Confirmed from disassembly: three cdecl stack args ([ebp+8], [ebp+0xc],
 *
 * [ebp+0x10]); the state pointer itself is NOT forwarded, only its +0x22 byte.

 * * Both callees are RET stubs in this debug build (recorded_animations.obj).
 */
void FUN_00093660(void *state, int arg1, int arg2)
{
  unsigned char mode;

  mode = ((unsigned char *)state)[0x20];
  if (mode > 0) {
    if (mode > 3) {
      if (mode == 4) {
        FUN_00094290(arg1, arg2, ((unsigned char *)state)[0x22]);
      }
    } else {
      FUN_00094ba0(arg1, arg2, ((unsigned char *)state)[0x22]);
    }
  }
}

/* 0x936b0 - linear, case-insensitive name lookup in the scenario tag_block at
 * +0x36c (0x40-byte elements, name at element offset 0). Returns the short
 * index of the first matching element, or -1 when the block is empty or no
 * element matches.
 * Confirmed from disassembly: two cdecl stack args ([ebp+8] = scenario,
 * [ebp+0xc] = name); ESI holds &scenario[0x36c] for the whole loop
 * (MOV ESI,[EBP+8] / MOV ECX,[ESI+0x36c] / ADD ESI,0x36c), and the element
 * count is re-read from [ESI] on every back-edge rather than cached. The
 * counter is a short in EDI, sign-extended (MOVSX EAX,DI) before the signed
 * compare against the count. The found path returns via MOV AX,DI, the
 * exhausted path via OR EAX,-1. The single ADD ESP,0x14 after the two calls
 * is a coalesced cleanup for both (3 + 2 stack args), not a 5-arg call. */
short FUN_000936b0(void *scenario, void *entry)
{
  void *block;
  char *element;
  short index;

  block = (char *)scenario + 0x36c;
  for (index = 0; (int)index < *(int *)block; index++) {
    element = (char *)tag_block_get_element(block, index, 0x40);
    if (crt_stricmp(element, (const char *)entry) == 0) {
      return index;
    }
  }
  return -1;
}

/* 0x93710 - byte-swap a run of consecutive tag-block streams in place.
 * For
 * each of `count` list indices (clamped to at least one) the global table
 * at
 * 0x2ee950 yields a -1 terminated array of 12-byte descriptors
 * { byte-swap
 * definition, stream size, pad_08 }. Each descriptor's definition
 * is applied
 * to the bytes currently at *cursor, then *cursor is advanced by
 * that
 * descriptor's size, so the cursor walks the whole run.
 * Confirmed from
 * disassembly: two cdecl stack args ([ebp+8] = cursor,
 * [ebp+0xc] = count
 * byte); the clamp is an UNSIGNED byte compare (JA) while
 * the loop test is
 * SIGNED (JGE) on a short counter kept in a dword slot; the
 * terminator test
 * is descriptor.size == -1; descriptor+0x8 is not read here (it is
 * the destination offset consumed by FUN_00093780);
 * *cursor is
 * re-loaded after every call, never cached across it. */
void FUN_00093710(int *cursor, unsigned char count)
{
  struct byte_swap_stream_descriptor {
    void *definition;
    int size;
    int dest_offset; /* read by FUN_00093780 - not padding */
  };
  struct byte_swap_stream_descriptor *descriptor;
  short i;

  for (i = 0; i < (count > 1 ? count : 1); i++) {
    descriptor =
      (struct byte_swap_stream_descriptor *)byte_swap_definition_lists[i];
    while (descriptor->size != -1) {
      FUN_00118be0(descriptor->definition, (void *)*cursor, 1);
      *cursor = *cursor + descriptor->size;
      descriptor++;
    }
  }
}

/* 0x93780 - unpack a run of consecutive tag-block streams into a fixed
 * 0x40-byte record.  The record is cleared first and its 16-bit field at +0x8
 * seeded with -1; then, for each of `count` list indices (clamped to at least
 * one), the global table at 0x2ee950 yields a -1 terminated array of 12-byte
 * descriptors { byte-swap definition, stream size, destination offset }.  Each
 * descriptor scatters `size` bytes from the bytes currently at *cursor into
 * dest + dest_offset, and *cursor is advanced by that descriptor's size so the
 * cursor walks the whole run.
 * Confirmed from disassembly: three cdecl stack args ([ebp+8] = dest,
 * [ebp+0xc] = cursor, [ebp+0x10] = count byte); the store at dest+8 is a
 * 16-bit MOV word; the clamp is an UNSIGNED byte compare (JA) recomputed on
 * every iteration while the loop test is SIGNED (JGE) on a short counter kept
 * in a dword slot; the terminator test is descriptor.size == -1; the copy is
 * skipped when dest_offset == -1 but *cursor is advanced either way. */
void FUN_00093780(void *dest, int *cursor, unsigned char count)
{
  struct byte_swap_stream_descriptor {
    void *definition;
    int size;
    int dest_offset;
  };
  struct byte_swap_stream_descriptor *descriptor;
  short i;

  csmemset(dest, 0, 0x40);
  *(short *)((char *)dest + 8) = (short)0xffff;

  for (i = 0; i < (count > 1 ? count : 1); i++) {
    descriptor =
      (struct byte_swap_stream_descriptor *)byte_swap_definition_lists[i];
    while (descriptor->size != -1) {
      if (descriptor->dest_offset != -1) {
        csmemcpy((char *)dest + descriptor->dest_offset, (void *)*cursor,
                 descriptor->size);
      }
      *cursor = *cursor + descriptor->size;
      descriptor++;
    }
  }
}

/* 0x93810 - pack a run of consecutive tag-block streams OUT of a fixed record

 * * into a byte stream.  Exact inverse of FUN_00093780: for each of `count`
 * list
 * indices (clamped to at least one), the global table at 0x2ee950
 * yields a -1
 * terminated array of 12-byte descriptors { byte-swap
 * definition, stream size,
 * source offset }.  Each descriptor gathers `size`
 * bytes from
 * source_record + source_offset to the bytes currently at
 * *cursor, and *cursor
 * is advanced by that descriptor's size so the cursor
 * walks the whole run.
 * Confirmed from disassembly: three cdecl stack args
 * ([ebp+8] = source record
 * -> EBX, [ebp+0xc] = cursor -> EDI, [ebp+0x10] =
 * count byte); the clamp is an
 * UNSIGNED byte compare (CMP AL,1 / JA)
 * recomputed on every iteration - the
 * back edge jumps to the MOV
 * AL,[ebp+0x10] - while the loop test is SIGNED
 * (MOVSX EAX, word ptr
 * [ebp-4]) on a short counter kept in a dword slot; the
 * lone call is
 * csmemcpy(*cursor, record + source_offset, size) with ADD ESP,0xc;
 * the
 * terminator test is descriptor.size == -1; descriptor.size and *cursor are
 *
 * both re-loaded after every call, never cached across it.  Unlike
 *
 * FUN_00093780 there is no -1 guard on the offset - every descriptor copies. */
void FUN_00093810(void *source_record, int *cursor, unsigned char count)
{
  struct byte_swap_stream_descriptor {
    void *definition;
    int size;
    int source_offset;
  };
  struct byte_swap_stream_descriptor *descriptor;
  short i;

  for (i = 0; i < (count > 1 ? count : 1); i++) {
    descriptor =
      (struct byte_swap_stream_descriptor *)byte_swap_definition_lists[i];
    while (descriptor->size != -1) {
      csmemcpy((void *)*cursor,
               (char *)source_record + descriptor->source_offset,
               descriptor->size);
      *cursor = *cursor + descriptor->size;
      descriptor++;
    }
  }
}

/* 0x93880 - recorded-animation playback: consume one animation-state byte from

 * * the event stream.  Reads a single byte at *stream into *control and
 * advances
 * *stream by one.
 * Confirmed from disassembly: plain cdecl EBP
 * frame with no locals (no SUB
 * ESP); FOUR stack args, of which [ebp+8] is
 * never read - it is the
 * dispatch-table signature slot shared by the
 * per-event-type stream callbacks
 * that are invoked through vtable[1], so it
 * must stay in the prototype.
 * [ebp+0xc] = control -> EBX, [ebp+0x10] =
 * header, [ebp+0x14] = stream -> ESI,
 * with EDI = [ESI] hoisted above the
 * null checks.  All three asserts share
 * __LINE__ 0x19 and the file string at
 * 0x2690a8, and each is followed by
 * PUSH -1 / CALL system_exit.  The
 * event-type test is a MASKED byte compare
 * (MOV CL,[EAX]; AND CL,0xfc; CMP
 * CL,8) - the low two bits of the header's
 * first byte are deliberately
 * discarded, so this is not a plain equality.  The
 * payload move is MOV
 * DL,[EDI] / MOV [EBX],DL (a BYTE, not a word) and the
 * cursor bump is MOV
 * EAX,[ESI]; INC EAX; MOV [ESI],EAX - the pointer is
 * re-loaded from [ESI]
 * for the increment rather than reusing EDI. */
void FUN_00093880(void *unused, unsigned char *control, unsigned char *header,
                  unsigned char **stream)
{
  unsigned char *position;

  position = *stream;

  if (!control) {
    display_assert("control",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback.c",
                   0x19, 1);
    system_exit(-1);
  }
  if (!header) {
    display_assert("header",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback.c",
                   0x19, 1);
    system_exit(-1);
  }
  if ((*header & 0xfc) != 8) {
    display_assert("header->event_type==_playback_animation_state_set",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback.c",
                   0x19, 1);
    system_exit(-1);
  }

  *control = *position;
  *stream = *stream + 1;
}

/* 0x93910 - recorded-animation playback: consume one aiming-speed byte from
 *
 * the event stream.  Reads a single byte at *stream into control[1] and
 *
 * advances *stream by one.  Structurally identical to FUN_00093880 (the
 *
 * animation-state variant) apart from the event type, the assert line and the

 * * destination byte.
 * Confirmed from disassembly: plain cdecl EBP frame with
 * no locals (no SUB
 * ESP); FOUR stack args, of which [ebp+8] is never read -
 * it is the
 * dispatch-table signature slot shared by the per-event-type
 * stream callbacks,
 * so it must stay in the prototype or every later offset
 * shifts by 4.
 * [ebp+0xc] = control -> EBX, [ebp+0x10] = header, [ebp+0x14] =
 * stream -> ESI,
 * with EDI = [ESI] hoisted above the null checks (loaded in
 * the prologue,
 * interleaved with the PUSHes).  header is re-loaded fresh
 * into EAX for each
 * of its two uses, never cached.  All three asserts share
 * __LINE__ 0x1a and
 * the file string at 0x2690a8, and each is followed by
 * PUSH -1 / CALL
 * system_exit with no ADD ESP (the exit path is noreturn).
 * The event-type
 * test is a MASKED byte compare (MOV CL,[EAX]; AND CL,0xfc;
 * CMP CL,0xc) - the
 * low two bits are deliberately discarded, so it accepts
 * 0x0c..0x0f and is not
 * a plain equality.  The payload move is MOV DL,[EDI]
 * / MOV [EBX+1],DL (a
 * BYTE, at offset +1 rather than +0) and the cursor bump
 * is MOV EAX,[ESI]; INC
 * EAX; MOV [ESI],EAX - the pointer is re-loaded from
 * [ESI] for the increment
 * rather than reusing the hoisted EDI. */
void FUN_00093910(void *unused, unsigned char *control, unsigned char *header,
                  unsigned char **stream)
{
  unsigned char *position;

  position = *stream;

  if (!control) {
    display_assert("control",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback.c",
                   0x1a, 1);
    system_exit(-1);
  }
  if (!header) {
    display_assert("header",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback.c",
                   0x1a, 1);
    system_exit(-1);
  }
  if ((*header & 0xfc) != 0xc) {
    display_assert("header->event_type==_playback_aiming_speed_set",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback.c",
                   0x1a, 1);
    system_exit(-1);
  }

  control[1] = *position;
  *stream = *stream + 1;
}

/* 0x939a0 - recorded-animation playback: consume one 16-bit control-flags word

 * * from the event stream.  Reads a 16-bit value at *stream into the halfword
 * at
 * control+2 and advances *stream by two.  Third member of the
 * per-event-type
 * stream-callback family after FUN_00093880 (animation state,
 * byte at +0) and
 * FUN_00093910 (aiming speed, byte at +1); it differs only
 * in the accepted
 * event type, the assert line and the payload width/offset.

 * *
 * Confirmed from disassembly (0x939a0-0x93a2f, 48 instructions, no FPU,
 * no
 * loops): plain cdecl EBP frame pushing EBX/ESI/EDI with no locals (no
 * SUB
 * ESP).  FOUR stack args, of which [ebp+8] is never read - it is the
 *
 * dispatch-table signature slot shared by every stream callback, so it must
 *
 * stay in the prototype or all later offsets shift by 4.  [ebp+0xc] = control

 * * -> EBX, [ebp+0x14] = stream -> ESI, with EDI = [ESI] hoisted to 0x939ae,
 *
 * above all three asserts.  [ebp+0x10] = header is re-loaded fresh into EAX
 *
 * for each of its two uses (0x939cf, 0x939f3), never cached in a callee-saved

 * * register.  All three asserts share __LINE__ 0x1b and the file string at
 *
 * 0x2690a8, and each is followed by PUSH -1 / CALL system_exit with no ADD ESP

 * * (the exit path is noreturn).
 *
 * The event-type test is a MASKED byte
 * compare (MOV CL,[EAX]; AND CL,0xfc;
 * CMP CL,0x10) - the low two bits are
 * deliberately discarded, so it accepts
 * 0x10..0x13 and is not a plain
 * equality.  The payload move is a 16-bit pair,
 * MOV DX,[EDI] / MOV
 * [EBX+2],DX - a halfword at offset +2, leaving control+0
 * and control+1
 * (written by the two sibling handlers) untouched.  The cursor
 * bump is MOV
 * EAX,[ESI]; ADD EAX,2; MOV [ESI],EAX - the pointer is re-loaded
 * from [ESI]
 * for the increment rather than reusing the hoisted EDI. */
void FUN_000939a0(void *unused, unsigned char *control, unsigned char *header,
                  unsigned char **stream)
{
  unsigned char *position;

  position = *stream;

  if (!control) {
    display_assert("control",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback.c",
                   0x1b, 1);
    system_exit(-1);
  }
  if (!header) {
    display_assert("header",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback.c",
                   0x1b, 1);
    system_exit(-1);
  }
  if ((*header & 0xfc) != 0x10) {
    display_assert("header->event_type==_playback_control_flags_set",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback.c",
                   0x1b, 1);
    system_exit(-1);
  }

  *(unsigned short *)(control + 2) = *(unsigned short *)position;
  *stream = *stream + 2;
}

/* recorded animation playback: weapon-index-set stream event handler
 *
 * (0x00093a30, c:\halo\SOURCE\cutscene\recorded_animation_playback.c line 28).

 * *
 * Fourth member of the stream-callback family that follows FUN_00093880
 *
 * (animation state, byte at +0), FUN_00093910 (aiming speed, byte at +1)
 * and
 * FUN_000939a0 (control flags, halfword at +2).  All four share the
 *
 * dispatch-table signature, so the first stack slot [ebp+0x08] is present
 *
 * but never read - removing it would shift control/header/stream by 4.
 *
 *
 * Register shape from disassembly (0x93a30-0x93abf, 48 instructions, plain
 *
 * cdecl EBP frame, no SUB ESP, no FPU, no loops): [ebp+0x0c] control -> EBX
 *
 * at 0x93a34, [ebp+0x14] stream -> ESI at 0x93a3a, and EDI = [ESI] hoisted
 *
 * at 0x93a3e ABOVE all three asserts.  [ebp+0x10] header is re-loaded fresh
 *
 * into EAX for each of its two uses (0x93a5f, 0x93a83), never cached in a
 *
 * callee-saved register.  All three asserts share __LINE__ 0x1c and the file
 *
 * string at 0x2690a8; push order is PUSH 1 / PUSH 0x1c / PUSH filestr /
 * PUSH
 * reasonstr / CALL display_assert, each followed by PUSH -1 / CALL
 *
 * system_exit with no ADD ESP (the exit path is noreturn).
 *
 * The event-type
 * test is a MASKED byte compare (MOV CL,[EAX]; AND CL,0xfc;
 * CMP CL,0x14) -
 * the low two bits are deliberately discarded, so it accepts
 * 0x14..0x17 and
 * is not a plain equality.  The payload move is a 16-bit
 * pair, MOV DX,[EDI]
 * / MOV [EBX+4],DX - a halfword at offset +4, past the
 * fields written by the
 * three sibling handlers.  The cursor bump is
 * MOV EAX,[ESI]; ADD EAX,2; MOV
 * [ESI],EAX - the pointer is re-loaded from
 * [ESI] for the increment rather
 * than reusing the hoisted EDI. */
void FUN_00093a30(void *unused, unsigned char *control, unsigned char *header,
                  unsigned char **stream)
{
  unsigned char *position;

  position = *stream;

  if (!control) {
    display_assert("control",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback.c",
                   0x1c, 1);
    system_exit(-1);
  }
  if (!header) {
    display_assert("header",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback.c",
                   0x1c, 1);
    system_exit(-1);
  }
  if ((*header & 0xfc) != 0x14) {
    display_assert("header->event_type==_playback_weapon_index_set",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback.c",
                   0x1c, 1);
    system_exit(-1);
  }

  *(unsigned short *)(control + 4) = *(unsigned short *)position;
  *stream = *stream + 2;
}

/* recorded animation playback: throttle-set stream event handler
 *
 * (0x00093ac0, c:\halo\SOURCE\cutscene\recorded_animation_playback.c lines
 *
 * 0x21/0x23/0x24).
 *
 * Fifth member of the stream-callback family
 * (FUN_00093880 animation state,
 * FUN_00093910 aiming speed, FUN_000939a0
 * control flags, FUN_00093a30 weapon
 * index).  Same dispatch-table signature,
 * so the first stack slot [ebp+0x08]
 * is present but never read - removing it
 * would shift control/header/stream
 * by 4.
 *
 * Register shape from
 * disassembly (plain cdecl EBP frame, PUSH EBX/ESI/EDI,
 * no SUB ESP so zero
 * locals, no FPU, no loops): [ebp+0x0c] control -> ESI,
 * [ebp+0x14] stream ->
 * EDI at 0x93acb and EBX = [EDI] hoisted at 0x93ace,
 * both ABOVE all three
 * asserts.  [ebp+0x10] header is re-loaded fresh into
 * EAX for each of its
 * two uses (0x93aef, 0x93b13), never cached.
 *
 * Unlike the siblings the
 * three asserts carry DISTINCT __LINE__ values
 * (0x21 control, 0x23 header,
 * 0x24 event_type) against the shared file
 * string at 0x2690a8; push order is
 * PUSH 1 / PUSH <line> / PUSH filestr /
 * PUSH reasonstr / CALL
 * display_assert, each followed by PUSH -1 / CALL
 * system_exit with no ADD
 * ESP (the exit path is noreturn).
 *
 * The event-type test is a MASKED byte
 * compare (MOV CL,[EAX]; AND CL,0xfc;
 * CMP CL,0x18) - the low two bits are
 * deliberately discarded, so it accepts
 * 0x18..0x1b and is not a plain
 * equality.
 *
 * Payload (0x93b3d-0x93b4f) is two 32-bit moves plus a zero
 * store:
 * MOV EDX,[EBX] / MOV [ESI+0xc],EDX, MOV EAX,[EBX+4] / MOV
 * [ESI+0x10],EAX,
 * MOV [ESI+0x14],0.  The two dwords look like a float pair
 * (a throttle is
 * plausibly two floats with a cleared third slot) but the
 * original moves
 * them with integer MOV and never touches the FPU, so they
 * are transcribed
 * as uint32 to avoid emitting FLD/FSTP.  The cursor bump is
 * a single
 * in-place ADD dword ptr [EDI],0x8 - EDI still holds &stream -
 * rather than
 * the reload-add-store used by FUN_00093a30. */
void FUN_00093ac0(void *unused, unsigned char *control, unsigned char *header,
                  unsigned char **stream)
{
  unsigned char *position;

  position = *stream;

  if (!control) {
    display_assert("control",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback.c",
                   0x21, 1);
    system_exit(-1);
  }
  if (!header) {
    display_assert("header",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback.c",
                   0x23, 1);
    system_exit(-1);
  }
  if ((*header & 0xfc) != 0x18) {
    display_assert("header->event_type==_playback_throttle_set",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback.c",
                   0x24, 1);
    system_exit(-1);
  }

  *(unsigned long *)(control + 0xc) = ((unsigned long *)position)[0];
  *(unsigned long *)(control + 0x10) = ((unsigned long *)position)[1];
  *(unsigned long *)(control + 0x14) = 0;
  *stream = *stream + 8;
}

/* Named constants recovered from the assert string at 0x269238 and the float
 *
 * pool entry at 0x26919c.  Defined here at end-of-file (rather than at the
 * top)
 * so no __LINE__ in the assert_halt calls above is shifted. */
#define PLAYBACK_VECTOR_SHORT_DIFFERENCE_SET \
  0xf /* _playback_vector_short_difference_set */
#define PLAYBACK_VECTOR_CHAR_DIFFERENCE_SET \
  0x7 /* _playback_vector_char_difference_set */
#define NUMBER_OF_CONTROL_VECTORS 3
#define CONTROL_VECTOR_FLAG_LIMIT \
  (1 << NUMBER_OF_CONTROL_VECTORS) /* FLAG(NUMBER_OF_CONTROL_VECTORS) == 8 */
/* PI / 1000: the recorded angle deltas are stored in 1/1000-of-PI units.
 *
 * Encodes to 0x3b4de32e, byte-identical to the pool constant at 0x26919c. */
#define RECORDED_ANIMATION_ANGLE_SCALE 0.0031415927f
/* Wrap bound in the same units (0x3e8). */
#define RECORDED_ANIMATION_ANGLE_WRAP 1000

/* recorded animation playback: control-vector char-difference stream event
 * handler (0x00093c20,
 * c:\halo\SOURCE\cutscene\recorded_animation_playback.c lines
 * 0x64/0x66/0x67).
 *
 * Byte-delta twin of FUN_00093e20 (short deltas): identical
 * control flow, identical control-vector layout, only the stream element
 * width differs.  The cursor is a signed char pair per vector, so the
 * per-vector accumulate helper is FUN_00093b60 (MOVSX byte) rather than
 * FUN_00093ba0 (MOV word), and the cursor bump is 2 bytes rather than 4.
 *
 * [ebp+0x08] angles -> ESI (loaded at 0x93cb1, after the header asserts have
 * finished using ESI), [ebp+0x0c] control -> EDI, [ebp+0x10] header,
 * [ebp+0x14] stream; the cursor is hoisted from *stream into [ebp-0x04] at
 * 0x93c29, above all three asserts.
 *
 * Event-type validation is the two-halves form the assert string spells out:
 * the masked byte gives the lower bound (AND DL,0xfc; CMP DL,0x1c; JB) and the
 * shifted field the upper bound (MOVZX; SHR 2; SUB 7; CMP 8; JL).  Keeping the
 * first half a byte compare matters.
 *
 * The event type's low three bits (event_type - 7) form a mask over the
 * NUMBER_OF_CONTROL_VECTORS control vectors.  It is computed 16-bit wide at
 * 0x93cb8 (MOVZX BX,CL; SUB EBX,7) and the first two tests are 16-bit
 * (TEST AX,AX at 0x93cc4, TEST CX,CX at 0x93d3f); only the last, whose operand
 * is dead afterwards, narrows to TEST BL,4 - so `vector_flags` is a short.
 *
 * Vector 0 (flag 1) is the only branch where the original inlines the two
 * per-vector helpers: the accumulate-and-wrap of FUN_00093b60 at
 * 0x93ccc-0x93d02 and the short -> radians -> angles_to_vector of
 * FUN_00093be0 at 0x93d06-0x93d2f.  Vectors 1 and 2 either copy an
 * already-computed vector or call both helpers out of line; at 0x93d74 and
 * 0x93dfb the original relies on FUN_00093b60 leaving its @<eax> argument
 * intact instead of re-issuing the LEA.
 *
 * Wrap arithmetic is signed 16-bit throughout (CMP AX,0x3e8 / JLE,
 * CMP AX,0xfc18 / JGE) with a 16-bit store-back, and neither branch stores
 * when the value is already in range.
 *
 * The angle pair handed to angles_to_vector lives at [ebp-0x0c]/[ebp-0x08] and
 * is passed by the address of its first element, so it is one two-float array,
 * not two independent locals.  Push order at 0x93d1b/0x93d1f is PUSH EAX
 * (&euler) then PUSH ECX (control+0x1c), so in cdecl the out vector is the
 * FIRST argument - matching the kb decl angles_to_vector(out, angles).
 *
 * The three 12-byte control vectors live at control+0x1c, +0x28 and +0x34 and
 * are copied through two computed base pointers (LEA + three dword moves), so
 * the copies are written via local pointers rather than six absolute-offset
 * stores.  The short pairs are copied a dword at a time ([ESI] -> [ESI+4],
 * [ESI] -> [ESI+8], [ESI+4] -> [ESI+8]).
 *
 * The cursor bump is the reload-add-store form (MOV EAX,[EBP+0x14];
 * MOV ECX,[EAX]; ADD ECX,2; MOV [EAX],ECX) and advances 2 BYTES; the original
 * tail-duplicates it into all three exits. */
void FUN_00093c20(short *angles, unsigned char *control, unsigned char *header,
                  unsigned char **stream)
{
  signed char *cursor;
  short vector_flags;
  float euler[2];

  cursor = (signed char *)*stream;

  if (!control) {
    display_assert("control",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback.c",
                   0x64, 1);
    system_exit(-1);
  }
  if (!header) {
    display_assert("header",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback.c",
                   0x66, 1);
    system_exit(-1);
  }
  if ((*header & 0xfc) < (PLAYBACK_VECTOR_CHAR_DIFFERENCE_SET << 2) ||
      (int)((*header >> 2) - PLAYBACK_VECTOR_CHAR_DIFFERENCE_SET) >=
        CONTROL_VECTOR_FLAG_LIMIT) {
    display_assert("header->event_type>=_playback_vector_char_difference_set&&"
                   "header->event_type-_playback_vector_char_difference_set<"
                   "FLAG(NUMBER_OF_CONTROL_VECTORS)",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback.c",
                   0x67, 1);
    system_exit(-1);
  }

  vector_flags = (short)((*header >> 2) - PLAYBACK_VECTOR_CHAR_DIFFERENCE_SET);

  if (vector_flags & 1) {
    angles[0] = (short)(angles[0] + cursor[0]);
    if (angles[0] > RECORDED_ANIMATION_ANGLE_WRAP) {
      angles[0] = (short)(angles[0] - RECORDED_ANIMATION_ANGLE_WRAP);
    } else if (angles[0] < -RECORDED_ANIMATION_ANGLE_WRAP) {
      angles[0] = (short)(angles[0] + RECORDED_ANIMATION_ANGLE_WRAP);
    }
    angles[1] = (short)(angles[1] + cursor[1]);

    euler[0] = (float)angles[0] * RECORDED_ANIMATION_ANGLE_SCALE;
    euler[1] = (float)angles[1] * RECORDED_ANIMATION_ANGLE_SCALE;
    angles_to_vector((float *)(control + 0x1c), euler);
  }

  if (vector_flags & 2) {
    if (vector_flags & 1) {
      unsigned long *source;
      unsigned long *destination;

      *(unsigned long *)(angles + 2) = *(unsigned long *)angles;
      source = (unsigned long *)(control + 0x1c);
      destination = (unsigned long *)(control + 0x28);
      destination[0] = source[0];
      destination[1] = source[1];
      destination[2] = source[2];
    } else {
      FUN_00093b60(angles + 2, cursor);
      FUN_00093be0(angles + 2, (float *)(control + 0x28));
    }
  }

  if (vector_flags & 4) {
    if (vector_flags & 1) {
      unsigned long *source;
      unsigned long *destination;

      *(unsigned long *)(angles + 4) = *(unsigned long *)angles;
      source = (unsigned long *)(control + 0x1c);
      destination = (unsigned long *)(control + 0x34);
      destination[0] = source[0];
      destination[1] = source[1];
      destination[2] = source[2];
    } else if (vector_flags & 2) {
      unsigned long *source;
      unsigned long *destination;

      *(unsigned long *)(angles + 4) = *(unsigned long *)(angles + 2);
      source = (unsigned long *)(control + 0x28);
      destination = (unsigned long *)(control + 0x34);
      destination[0] = source[0];
      destination[1] = source[1];
      destination[2] = source[2];
    } else {
      FUN_00093b60(angles + 4, cursor);
      FUN_00093be0(angles + 4, (float *)(control + 0x34));
    }
  }

  *stream = *stream + 2;
}

/* recorded animation playback: control-vector short-difference stream event
 *
 * handler (0x00093e20, c:\halo\SOURCE\cutscene\recorded_animation_playback.c
 *
 * lines 0xa0/0xa2/0xa3).
 *
 * Sixth member of the stream-callback family after
 * FUN_00093880 / 93910 /
 * 939a0 / 93a30 / 93ac0, and the only one whose first
 * stack slot is live:
 * [ebp+0x08] is the persistent short array of
 * accumulated (yaw, pitch) pairs,
 * one pair per control vector, loaded into
 * ESI at 0x93eba (after the header
 * asserts have finished using ESI for
 * `header`).  [ebp+0x0c] control -> EDI,
 * [ebp+0x10] header, [ebp+0x14]
 * stream; the cursor is hoisted from *stream
 * into [ebp-0x04] at 0x93e26,
 * above all asserts, exactly like the siblings.
 *
 * Event-type validation
 * differs from the siblings' single equality: the
 * original tests the masked
 * byte for the lower bound (AND DL,0xfc; CMP
 * DL,0x3c; JB) and the shifted
 * field for the upper bound (MOVZX; SHR 2; SUB
 * 0xf; CMP 8; JL), i.e. exactly
 * the two halves of the assert string.  Both
 * halves are transcribed in the
 * same order so the byte compare stays a byte
 * compare.
 *
 * The event
 * type's low three bits (event_type - 15) form a mask over the
 *
 * NUMBER_OF_CONTROL_VECTORS control vectors.  Its computation at 0x93eb8 is
 *
 * MOV CL,[ESI]; SHR CL,2; MOVZX BX,CL; SUB EBX,0xf - a 16-bit-wide value, so
 *
 * `vector_flags` is a short: the two later tests are 16-bit (TEST AX,AX at
 *
 * 0x93ecd, TEST CX,CX at 0x93f46) and only the last, whose operand is dead
 *
 * afterwards, is narrowed to TEST BL,4.
 *
 * Vector 0 (flag 1) is the only
 * branch where the two per-vector helpers are
 * inlined by the original:
 * FUN_00093ba0 (accumulate + wrap) at 0x93ed5-0x93efb
 * and FUN_00093be0
 * (short -> radians -> angles_to_vector) at 0x93efe-0x93f3e.
 * Vectors 1 and 2
 * either copy an already-computed vector or call the two
 * helpers out of
 * line.  Both helpers take their short* in EAX (FUN_00093ba0
 * additionally
 * takes the cursor in EDX); at 0x93f7f the original relies on
 * FUN_00093ba0
 * leaving EAX intact rather than re-issuing the LEA.
 *
 * Wrap arithmetic is
 * signed 16-bit throughout (CMP AX,0x3e8 / JLE, CMP
 * AX,0xfc18 / JGE) with a
 * 16-bit store-back, and neither branch stores when
 * the value is already in
 * range - keep every intermediate `short`.
 *
 * The angle pair handed to
 * angles_to_vector lives at [ebp-0x0c]/[ebp-0x08] and
 * is passed by the
 * address of the first element, so it is one two-float array,
 * not two
 * independent locals.  Push order at 0x93f22/0x93f26 is PUSH EAX
 * (&euler)
 * then PUSH ECX (control+0x1c), so in cdecl the out vector is the
 * FIRST
 * argument - matching the kb decl angles_to_vector(out, angles).
 *
 * The
 * three 12-byte control vectors live at control+0x1c, +0x28 and +0x34 and
 *
 * are copied through two computed base pointers (LEA + three dword moves), so

 * * the copies are written via local pointers rather than six absolute-offset

 * * stores.  The short pairs are copied a dword at a time ([ESI] -> [ESI+4],
 *
 * [ESI] -> [ESI+8], [ESI+4] -> [ESI+8]).
 *
 * The cursor bump is the
 * reload-add-store form (MOV EAX,[EBP+0x14]; MOV
 * ECX,[EAX]; ADD ECX,4; MOV
 * [EAX],ECX) and advances 4 BYTES, not 4 shorts;
 * the original
 * tail-duplicates it into all three exits. */
void FUN_00093e20(short *angles, unsigned char *control, unsigned char *header,
                  unsigned char **stream)
{
  short *cursor;
  short vector_flags;
  float euler[2];

  cursor = (short *)*stream;

  if (!control) {
    display_assert("control",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback.c",
                   0xa0, 1);
    system_exit(-1);
  }
  if (!header) {
    display_assert("header",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback.c",
                   0xa2, 1);
    system_exit(-1);
  }
  if ((*header & 0xfc) < (PLAYBACK_VECTOR_SHORT_DIFFERENCE_SET << 2) ||
      (int)((*header >> 2) - PLAYBACK_VECTOR_SHORT_DIFFERENCE_SET) >=
        CONTROL_VECTOR_FLAG_LIMIT) {
    display_assert("header->event_type>=_playback_vector_short_difference_set&&"
                   "header->event_type-_playback_vector_short_difference_set<"
                   "FLAG(NUMBER_OF_CONTROL_VECTORS)",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback.c",
                   0xa3, 1);
    system_exit(-1);
  }

  vector_flags = (short)((*header >> 2) - PLAYBACK_VECTOR_SHORT_DIFFERENCE_SET);

  if (vector_flags & 1) {
    angles[0] = (short)(angles[0] + cursor[0]);
    if (angles[0] > RECORDED_ANIMATION_ANGLE_WRAP) {
      angles[0] = (short)(angles[0] - RECORDED_ANIMATION_ANGLE_WRAP);
    } else if (angles[0] < -RECORDED_ANIMATION_ANGLE_WRAP) {
      angles[0] = (short)(angles[0] + RECORDED_ANIMATION_ANGLE_WRAP);
    }
    angles[1] = (short)(angles[1] + cursor[1]);

    euler[0] = (float)angles[0] * RECORDED_ANIMATION_ANGLE_SCALE;
    euler[1] = (float)angles[1] * RECORDED_ANIMATION_ANGLE_SCALE;
    angles_to_vector((float *)(control + 0x1c), euler);
  }

  if (vector_flags & 2) {
    if (vector_flags & 1) {
      unsigned long *source;
      unsigned long *destination;

      *(unsigned long *)(angles + 2) = *(unsigned long *)angles;
      source = (unsigned long *)(control + 0x1c);
      destination = (unsigned long *)(control + 0x28);
      destination[0] = source[0];
      destination[1] = source[1];
      destination[2] = source[2];
    } else {
      FUN_00093ba0(angles + 2, cursor);
      FUN_00093be0(angles + 2, (float *)(control + 0x28));
    }
  }

  if (vector_flags & 4) {
    if (vector_flags & 1) {
      unsigned long *source;
      unsigned long *destination;

      *(unsigned long *)(angles + 4) = *(unsigned long *)angles;
      source = (unsigned long *)(control + 0x1c);
      destination = (unsigned long *)(control + 0x34);
      destination[0] = source[0];
      destination[1] = source[1];
      destination[2] = source[2];
    } else if (vector_flags & 2) {
      unsigned long *source;
      unsigned long *destination;

      *(unsigned long *)(angles + 4) = *(unsigned long *)(angles + 2);
      source = (unsigned long *)(control + 0x28);
      destination = (unsigned long *)(control + 0x34);
      destination[0] = source[0];
      destination[1] = source[1];
      destination[2] = source[2];
    } else {
      FUN_00093ba0(angles + 4, cursor);
      FUN_00093be0(angles + 4, (float *)(control + 0x34));
    }
  }

  *stream = *stream + 4;
}
