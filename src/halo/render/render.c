/* Test the per-group flag bit for a transparent geometry group (0x184570).
 * Returns 1 when the group's bit in the 384-bit flag array at 0x4d0cbc is
 * CLEAR (or when the group pointer does not resolve to a presorted index),
 * 0 when the bit is SET. The array is 0x30 bytes (0x180 groups, one bit per
 * group) and is cleared by rasterizer_transparent_geometry_begin.
 * Binary: MOVSX EDX,AX / SAR EDX,5 -> signed word index; NEG EAX / SBB AL,AL /
 * INC AL -> AL = (bit == 0). */
char FUN_00184570(void *group)
{
  short presorted_index;

  presorted_index = rasterizer_transparent_geometry_group_to_presorted_index(
    (unsigned int)group);
  if (presorted_index != -1) {
    return (char)(((1 << (presorted_index & 0x1f)) &
                   ((unsigned int *)0x4d0cbc)[presorted_index >> 5]) == 0);
  }
  return 1;
}

/* FUN_001845b0: set or clear this group's bit in the transparent-geometry-group
 * bit vector at 0x4d0cbc (0x30 bytes = 12 dwords = 384 bits, matching the
 * 0x180 group cap; zeroed by the csmemset above). A group pointer that does not
 * resolve to a presorted index (-1) is silently ignored.
 *
 * NOTE the branch polarity, which is the opposite of what a "set flag" reading
 * would suggest and must not be "normalized": only the LOW BYTE of clear_bit is
 * tested (MOV CL,[EBP+0xc]; TEST CL,CL), and
 *   low byte == 0  -> OR   mask (SET the bit)   [own POP EBP/RET at 0x1845e8]
 *   low byte != 0  -> ANDN mask (CLEAR the bit) [RET at 0x18460d]
 * The set path returns early (two distinct RET sites), so the early-return
 * shape is reproduced here rather than an if/else.
 *
 * The index math runs on the SIGN-EXTENDED short (MOVSX ECX,AX then SAR ECX,5),
 * so the shift must stay arithmetic on a signed int. The explicit `& 0x1f` on
 * the shift count is real, not a Ghidra artifact: the original emits
 * AND ECX,0x1f before SHL EDX,CL in both branches. The word index (SAR ECX,5)
 * is recomputed inside each branch rather than hoisted above the TEST, so the
 * expression is written out per branch here. (0x1845b0) */
void FUN_001845b0(void *group, int clear_bit)
{
  short group_presorted_index;
  int index;

  group_presorted_index =
    rasterizer_transparent_geometry_group_to_presorted_index(
      (unsigned int)group);
  if (group_presorted_index == -1) {
    return;
  }
  index = group_presorted_index;
  if ((char)clear_bit == 0) {
    *(unsigned int *)(0x4d0cbc + (index >> 5) * 4) =
      *(unsigned int *)(0x4d0cbc + (index >> 5) * 4) | (1 << (index & 0x1f));
    return;
  }
  *(unsigned int *)(0x4d0cbc + (index >> 5) * 4) =
    *(unsigned int *)(0x4d0cbc + (index >> 5) * 4) & ~(1 << (index & 0x1f));
}

/* FUN_00184610: resolve the first vertex index of a transparent geometry group
 * (0x184610). Two mutually exclusive sources on the group record:
 *   +0x58  pointer to an int16 vertex index (nullable) -- when set, the stored
 *          index is returned directly. The load is `MOV AX,word ptr [EAX]`, a
 *          WORD load, so this must stay a 16-bit read (Ghidra models the upper
 *          half of EAX as CONCAT22 garbage).
 *   +0x54  dynamic-vertex-buffer index, sentinel -1 -- when not -1 it is passed
 *          (PUSH ESI / CALL 0x17c9c0 / ADD ESP,4 -- one stack arg; Ghidra drops
 *          it and shows a 0-arg call) to 0x17c9c0, whose short result is the
 *          return value.
 * With neither source the group has no vertices: report through error() at
 * level 2 and return -1 (EDI is pre-seeded 0xffffffff at entry purely to feed
 * `MOV AX,DI` on this path, so it is not modelled as a variable here).
 * The null-group assert tail is CALL 0x8e2f0 = system_exit(-1), not
 * halt_and_catch_fire (Ghidra prints thunk_FUN_001029a0). Every return path is
 * `MOV AX,...`, hence the 16-bit return type. */
short FUN_00184610(void *group)
{
  short *vertex_index;
  int dynamic_vertex_buffer_index;

  if (group == 0) {
    display_assert(
      "group",
      "c:\\halo\\SOURCE\\rasterizer\\rasterizer_transparent_geometry.c", 0xf4,
      1);
    system_exit(-1);
  }
  vertex_index = *(short **)((char *)group + 0x58);
  if (vertex_index != 0) {
    return *vertex_index;
  }
  dynamic_vertex_buffer_index = *(int *)((char *)group + 0x54);
  if (dynamic_vertex_buffer_index != -1) {
    return rasterizer_dynamic_vertices_get_type(dynamic_vertex_buffer_index);
  }
  error(2, "### ERROR transparent geometry group has no vertices");
  return -1;
}

/* FUN_00184680 @ 0x184680 -- confirmed empty: the function body in the
 * binary is a single RET with no prologue/epilogue. Called unconditionally
 * from FUN_00158f90's per-frame "first init" block (guarded by
 * DAT_00476ab8=='\0', alongside FUN_001825d0/FUN_0015d160/FUN_00165a00/
 * FUN_00181410/FUN_0017e030/FUN_0017e010), so this is a genuine no-op in the
 * original debug build, not an unrecovered stub. */
void FUN_00184680(void)
{
}

/* rasterizer_transparent_geometry dispose counterpart to
 * rasterizer_transparent_geometry_new (0x184260): tears down the vertex
 * cache (FUN_00174cc0), frees the three group/index/vertex buffers if
 * allocated, and zeroes the buffer pointers plus the group/dynamic-vertex
 * counters. Each buffer is freed then zeroed individually (not batched) --
 * matches the disassembly's per-buffer CMP/JZ/CALL/MOV-zero sequence
 * (0x184690). */
void FUN_00184690(void)
{
  FUN_00174cc0();
  if (*(void **)0x4d0cec != 0) {
    debug_free(
      *(void **)0x4d0cec,
      "c:\\halo\\SOURCE\\rasterizer\\rasterizer_transparent_geometry.c", 0x111);
  }
  *(void **)0x4d0cec = 0;
  if (*(void **)0x4d0cfc != 0) {
    debug_free(
      *(void **)0x4d0cfc,
      "c:\\halo\\SOURCE\\rasterizer\\rasterizer_transparent_geometry.c", 0x114);
  }
  *(void **)0x4d0cfc = 0;
  if (*(void **)0x4d0cf0 != 0) {
    debug_free(
      *(void **)0x4d0cf0,
      "c:\\halo\\SOURCE\\rasterizer\\rasterizer_transparent_geometry.c", 0x118);
  }
  *(void **)0x4d0cf0 = 0;
  *(int *)0x4d0cf8 = 0;
  *(int *)0x4d0cf4 = 0;
}

/* rasterizer_transparent_geometry_stop (0x184710): resets stencil rendering
 * to mode 0 after transparent-geometry rendering. */
void rasterizer_transparent_geometry_stop(void)
{
  FUN_00158ae0(0);
}

/* group_sorted_indices_cmpfn (0x184750): qsort comparator over int16 group
 * indices into the 0xa0-byte transparent geometry group table at 0x4d0cec.
 * cdecl (two stack pointers, plain RET), int result in EAX.
 * Order: a group whose shader (+0x0c) passes shader_is_water_decal sorts
 * first (-1), then the second group's (+1); then a shader whose type word
 * (+0x24) is 7; then the group flag bit 0x80 of the first dword (a set group
 * sorts after a clear one); then the float at +0x70 descending-first (+1
 * when the first is greater, -1 when less); then the int at +0x08 (+1 when
 * greater, -1 when less).  The byte at +0x9d overrides every comparison:
 * a set byte on only one side sorts that side after the other. */
int group_sorted_indices_cmpfn(const void *a, const void *b)
{
  const int16_t *group_index1;
  const int16_t *group_index2;
  char *group1;
  char *group2;
  int result;
  int flag1;
  char last1;
  char last2;

  group_index1 = (const int16_t *)a;
  group_index2 = (const int16_t *)b;
  result = 0;
  if (group_index1 == NULL || *group_index1 < 0 ||
      *group_index1 >= *(int *)0x4d0cf4) {
    display_assert("group_index1 && (*group_index1)>=0 && "
                   "(*group_index1)<transparent_geometry_group_count",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_transparent_"
                   "geometry.c",
                   0x1aa, 1);
    system_exit(-1);
  }
  if (group_index2 == NULL || *group_index2 < 0 ||
      *group_index2 >= *(int *)0x4d0cf4) {
    display_assert("group_index2 && (*group_index2)>=0 && "
                   "(*group_index2)<transparent_geometry_group_count",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_transparent_"
                   "geometry.c",
                   0x1ab, 1);
    system_exit(-1);
  }
  group1 = *(char **)0x4d0cec + *group_index1 * 0xa0;
  group2 = *(char **)0x4d0cec + *group_index2 * 0xa0;
  if (shader_is_water_decal(*(void **)(group1 + 0xc))) {
    result = -1;
  } else if (shader_is_water_decal(*(void **)(group2 + 0xc))) {
    result = 1;
  } else if (*(char **)(group1 + 0xc) != NULL &&
             *(int16_t *)(*(char **)(group1 + 0xc) + 0x24) == 7) {
    result = -1;
  } else if (*(char **)(group2 + 0xc) != NULL &&
             *(int16_t *)(*(char **)(group2 + 0xc) + 0x24) == 7) {
    result = 1;
  } else {
    flag1 = *(int *)group1 & 0x80;
    if (flag1 != 0 && !(*(signed char *)group2 < 0)) {
      result = 1;
    } else if (flag1 == 0 && *(signed char *)group2 < 0) {
      result = -1;
    } else if (*(float *)(group1 + 0x70) > *(float *)(group2 + 0x70)) {
      result = 1;
    } else if (*(float *)(group1 + 0x70) < *(float *)(group2 + 0x70)) {
      result = -1;
    } else if (*(int *)(group1 + 0x8) > *(int *)(group2 + 0x8)) {
      result = 1;
    } else if (*(int *)(group1 + 0x8) < *(int *)(group2 + 0x8)) {
      result = -1;
    }
  }
  last1 = *(group1 + 0x9d);
  last2 = *(group2 + 0x9d);
  if (last1 != 0 && last2 == 0)
    return 1;
  if (last1 == 0 && last2 != 0)
    return -1;
  return result;
}

/* rasterizer_sort_internal (0x1848d0): builds the presorted index list for
 * the transparent geometry groups, sorts it, then writes each group's sorted
 * position back into its record.
 *   0x4d0cec  group table, 0xa0-byte records (LEA/SHL 5 on index*5)
 *   0x4d0cf4  group count (the first loop's bound and the qsort count are the
 *             single EAX load at 0x1848d0; the second loop reloads it)
 *   0x4d0cfc  int16 index array (qsort element size 2)
 * The null-group assert (line 0x192) is display_assert + system_exit(-1).
 * The comparator is group_sorted_indices_cmpfn (0x184750).
 * The second loop stores the loop counter (MOVSX EAX,DX before the store) as
 * an int at record +0x90; the record's element index comes from the sorted
 * int16 array (MOVSX ECX,word ptr [EBX+EAX*2]). */
void rasterizer_sort_internal(void)
{
  int count;
  int16_t index;
  int16_t *sorted_indices;
  char *groups;

  count = *(int *)0x4d0cf4;
  index = 0;
  if (count > 0) {
    do {
      if ((char *)(index * 0xa0 + *(int *)0x4d0cec) == 0) {
        display_assert(
          "group",
          "c:\\halo\\SOURCE\\rasterizer\\rasterizer_transparent_geometry.c",
          0x192, 1);
        system_exit(-1);
      }
      (*(int16_t **)0x4d0cfc)[index] = index;
      index++;
    } while (index < count);
  }
  qsort(*(void **)0x4d0cfc, (size_t)count, 2, group_sorted_indices_cmpfn);
  count = *(int *)0x4d0cf4;
  index = 0;
  if (count > 0) {
    groups = *(char **)0x4d0cec;
    sorted_indices = *(int16_t **)0x4d0cfc;
    do {
      *(int *)(groups + sorted_indices[index] * 0xa0 + 0x90) = index;
      index++;
    } while (index < count);
  }
}

/* render_effects (0x184b60)
 *
 * Broadcasts a single cdecl byte argument (the record's first byte, per the
 * caller render_effects_evaluate at 0xbee00) to four adjacent global enable
 * bytes: 0x32574a, 0x32574b, 0x32574c, 0x32574d. All four are set to the same
 * incoming value (000184b63 MOV AL,[EBP+8]; four MOV [addr],AL stores, no
 * branch). 0x32574c is read elsewhere as the particle-system-update gate
 * (particle_systems_render, 0xa1170) and 0x32574b as the scenario particles
 * gate; the other two bytes' readers are not evidenced in this bundle. */
/* 0x184980 — draw transparent geometry groups in presorted order.  The
 * group-table record is 0xa0 bytes: byte +0 selects the first-person frustum
 * path and pointer +0xc supplies the shader used for the water-decal filter.
 * The index list and the current index are 16-bit; preserve both signed word
 * accesses when walking the table. */
void FUN_00184980(char param_1)
{
  char first_person_flag;
  char *group;
  int group_index;
  int profile;
  void *shader;

  profile = param_1 != 0 ? 0x13 : 0x16;
  FUN_0016f910(profile);
  if (*(int *)0x4d0cf4 > 0) {
    first_person_flag = 0;
    if (param_1 != 0) {
      rasterizer_sort_internal();
      *(short *)0x4d0cb8 = 0;
      if (*(short *)0x5a5bc2 != -1) {
        *(char *)0x325740 = 1;
      }
    }
    FUN_00174ce0();
    *(char *)0x325740 = 0;
    while ((int)*(short *)0x4d0cb8 < *(int *)0x4d0cf4) {
      group_index = *(short *)(*(int *)0x4d0cfc + (int)*(short *)0x4d0cb8 * 2);
      group = (char *)(group_index * 0xa0 + *(int *)0x4d0cec);
      if (param_1 != 0) {
        shader = *(void **)(group + 0xc);
        if (shader == 0 || (*(short *)((char *)shader + 0x24) != 7 &&
                            shader_is_water_decal(shader) == 0)) {
          break;
        }
      }
      if (*(signed char *)group < 0) {
        if (param_1 != 0) {
          display_assert(
            "!water",
            "c:\\halo\\SOURCE\\rasterizer\\rasterizer_transparent_geometry.c",
            0x154, 1);
          system_exit(-1);
        }
        if (*(short *)0x5a5bc0 != 0) {
          display_assert(
            "global_window_parameters.rasterizer_target==_rasterizer_target_"
            "render_primary",
            "c:\\halo\\SOURCE\\rasterizer\\rasterizer_transparent_geometry.c",
            0x155, 1);
          system_exit(-1);
        }
        if (first_person_flag == 0) {
          FUN_00158ae0(0);
          rasterizer_set_frustum_z(*(float *)0x32569c, *(float *)0x3256a0);
          first_person_flag = 1;
        }
      } else if (first_person_flag != 0) {
        display_assert(
          "!first_person_flag",
          "c:\\halo\\SOURCE\\rasterizer\\rasterizer_transparent_geometry.c",
          0x163, 1);
        system_exit(-1);
      }
      rasterizer_transparent_geometry_group_draw(group, 0);
      *(short *)0x4d0cb8 = *(short *)0x4d0cb8 + 1;
    }
    if (param_1 == 0 && *(short *)0x5a5bc2 != -1) {
      *(char *)0x325740 = 1;
    }
    FUN_001749b0();
    *(char *)0x325740 = 0;
    if (first_person_flag != 0) {
      rasterizer_set_frustum_z(0.0f, 0.0f);
    }
  }
  FUN_0016fa40(profile);
}

void render_effects(int a)
{
  *(char *)0x32574d = (char)a;
  *(char *)0x32574c = (char)a;
  *(char *)0x32574b = (char)a;
  *(char *)0x32574a = (char)a;
}

void render_initialize(void)
{
  FUN_0018AF90();
}

void render_initialize_for_new_map(void)
{
  FUN_0018afd0();
}

/* Invalidate the cached render states data if it exists and is valid
 * (0x184ba0). Thunk through 0x18afe0. */
void j__render_dispose_from_old_map(void)
{
  FUN_0018afe0();
}

void render_dispose(void)
{
  FUN_0018B000();
}

/* Render a window in pregame mode. window_type selects the render path:
 *   0 = full pregame UI (loading screen, menus, bink playback)
 *   1 = inactive window (no player assigned, simpler scene render)
 * Called from render_frame with window_type passed via EBX register. */
void render_window_pregame(int window_type, int16_t *win)
{
  window_parameters_t window_params;

  profile_render_window_start(0);
  csmemset(&window_params, 0, sizeof(window_parameters_t));

  qmemcpy(&unknown_global_camera, (char *)win + 4, sizeof(camera_t));
  render_camera_build_frustum(&unknown_global_camera, 0, global_frustum, 1);

  qmemcpy(&window_params.camera, (char *)win + 0x58, sizeof(camera_t));
  render_camera_build_frustum(&window_params.camera, 0, window_params.frustum,
                              1);

  /* set up scene parameters */
  window_params.unk_0[0] = 0;
  window_params.unk_0[1] = -1;
  *((uint8_t *)&window_params + 5) = (window_type == 0);

  rasterizer_window_begin(&window_params);

  switch (window_type) {
  case 0:
    ((void (*)(void))0x0dff70)();
    ((void (*)(void))0x17e190)();
    break;
  case 1:
    ((void (*)(void))0x0af9a0)();
    break;
  default:
    display_assert("!\"unreachable\"", "c:\\halo\\SOURCE\\render\\render.c",
                   0x11f, 1);
    system_exit(-1);
    break;
  }

  rasterizer_window_end();
  profile_render_window_end();
}

void render_frame_pregame(pregame_render_info_t *pregame_info,
                          void *main_globals_movie)
{
  window_parameters_t window_parameters;
  float elapsed[2];
  float progress;

  ++render.frame_index;
  rasterizer_frame_begin(elapsed);
  rasterizer_windows_begin();
  profile_render_window_start(0);
  csmemset(&window_parameters, 0, 0x258u);

  qmemcpy(&unknown_global_camera, &pregame_info->cam0,
          sizeof(unknown_global_camera));
  render_camera_build_frustum(&unknown_global_camera, 0, global_frustum, 1);

  qmemcpy(&window_parameters.camera, &pregame_info->cam1,
          sizeof(window_parameters.camera));
  render_camera_build_frustum(&window_parameters.camera, 0,
                              window_parameters.frustum, 1);
  window_parameters.unk_0[0] = 0;
  rasterizer_window_begin(&window_parameters);
  render_ui_widgets(0, &pregame_info->cam1.viewport_bounds);
  bink_playback_render();
  if (game_map_loading_in_progress(&progress)) {
    progress_bar_display(progress);
  }
  rasterizer_window_end();
  profile_render_window_end();
  rasterizer_windows_end();
  rasterizer_frame_end();
}

void render_frame_present(_WORD *a1, void *a2)
{
  ((void (*)(_WORD *, void *))0x17c930)(a2, a1);
}

/* render_location_visible (0x184de0): test the location's cluster bit in the
 * visible-cluster bit vector at 0x50678c. The cluster index is the int16 at
 * location+0x4 (CMP word [ESI+4],0 / JL; MOVSX ECX,word [ESI+4]); it is
 * bounds-checked against the dword count at +0x134 of the structure returned
 * by 0x18e3c0 (kb: scenario_get; the assert text names it
 * global_structure_bsp_get()). The bit test re-reads the word (MOVSX EDX),
 * masks the shift with AND ECX,0x1f and indexes by SAR EDX,5 (signed), and
 * NEG/SBB/NEG yields (bit != 0). Assert line 0x248. */
/* File-local bit-vector test helper (no out-of-line copy in the binary).
 * The reference materializes the test as an int (NEG/SBB/NEG) before the
 * char return; an inline int-returning test reproduces that, whereas a direct
 * char-typed `!= 0` compiles to TEST/SETNE. */
static __inline int render_bit_vector_test(const unsigned int *vector,
                                           int bit)
{
  return ((1 << (bit & 0x1f)) & vector[bit >> 5]) != 0;
}

char render_location_visible(void *location)
{
  int16_t *cluster_index;

  cluster_index = (int16_t *)((char *)location + 0x4);
  if (*cluster_index < 0 ||
      *cluster_index >= *(int *)((char *)scenario_get() + 0x134)) {
    display_assert("location->cluster_index>=0 && "
                   "location->cluster_index<global_structure_bsp_get()->"
                   "clusters.count",
                   "c:\\halo\\SOURCE\\render\\render.c", 0x248, 1);
    system_exit(-1);
  }
  return (char)render_bit_vector_test((unsigned int *)0x50678c,
                                      *cluster_index);
}

/* rendered_cluster_get (0x184e50): bounds-checked accessor into the
 * rendered-cluster array. Index and count (*0x5137cc) are compared as
 * signed int16 (MOV SI,[EBP+8]; TEST SI,SI/JL; CMP SI,word[0x5137cc]/JL);
 * each element is 0x1a0 (416) bytes at base 0x5067cc (MOVSX EAX,SI;
 * IMUL EAX,EAX,0x1a0; ADD EAX,0x5067cc — the decompiler's "*0xd0" is a
 * mis-rendered immediate, disassembly is authoritative). Caller
 * FUN_00198070 (structures.c) treats the result as int16_t*, so element
 * layout is not yet a named struct. */
void *rendered_cluster_get(int rendered_cluster_index)
{
  int16_t index;

  index = (int16_t)rendered_cluster_index;
  if (index < 0 || index >= *(int16_t *)0x5137cc) {
    display_assert("rendered_cluster_index>=0 && "
                   "rendered_cluster_index<render.rendered_cluster_count",
                   "c:\\halo\\SOURCE\\render\\render.c", 0x250, 1);
    system_exit(-1);
  }
  return (char *)0x5067cc + index * 0x1a0;
}

/* render_scene (0x184ea0): render one scene pass for a window.
 * player_index arrives in EBX (@<ebx>) and is pushed on to
 * player_effect_get_screen_flash (0x184f50) and render_ui_widgets
 * (0x185242). Stack args: [EBP+8] render_cam, [EBP+0xc] render_frustum,
 * [EBP+0x10] rasterizer_cam, [EBP+0x14] rasterizer_frustum, [EBP+0x18]
 * pass_type (word), [EBP+0x1c] reflected (byte).
 *
 * Local window-parameter block is 0x258 bytes at [EBP-0x258] (csmemset size):
 *   +0x000 word  pass_type
 *   +0x002 word  copy of *(int16_t *)0x50654a (window index, set by
 *                render_frame)
 *   +0x004 byte  reflected
 *   +0x008 0x54  rasterizer camera copy (REP MOVSD ECX=0x15)
 *   +0x05c 0x18c rasterizer frustum copy (REP MOVSD ECX=0x63)
 *   +0x1e8 0x50  copy of 0x506730 (fog block written by render_window;
 *                REP MOVSD ECX=0x14 into [EBP-0x70])
 *   +0x238       screen flash, filled by player_effect_get_screen_flash
 *                ([EBP-0x20]); read back as 0x5a5bc0+0x238 = 0x5a5df8 by
 *                FUN_00171bc0 after rasterizer_window_begin copies the block.
 * The render camera/frustum go to 0x506550 (0x54) and 0x5065a4 (0x18c).
 *
 * The five rendered-cluster loops inline rendered_cluster_get (same assert
 * string, render.c line 0x250) and push the element's first word
 * zero-extended (XOR reg,reg; MOV reg16,[elem]). The whole scene body is
 * skipped while bink_playback_has_video() is true (TEST AL,AL; JNZ). */
void render_scene(int16_t player_index, void *render_cam, void *render_frustum,
                  void *rasterizer_cam, void *rasterizer_frustum,
                  int16_t pass_type, char reflected)
{
  window_parameters_t window_params;
  int16_t i;

  profile_render_window_start(1);
  *(int *)0x506544 = *(int *)0x506544 + 1;
  csmemset(&window_params, 0, sizeof(window_parameters_t));
  *(int16_t *)0x506548 = player_index;
  qmemcpy(&unknown_global_camera, render_cam, sizeof(camera_t));
  qmemcpy(global_frustum, render_frustum, 0x18c);
  qmemcpy(&window_params.camera, rasterizer_cam, sizeof(camera_t));
  qmemcpy(window_params.frustum, rasterizer_frustum, 0x18c);
  window_params.unk_0[0] = pass_type;
  *((char *)&window_params + 4) = reflected;
  window_params.unk_0[1] = *(int16_t *)0x50654a;
  qmemcpy((char *)&window_params + 0x1e8, (void *)0x506730, 0x50);
  render_structure_visibility();
  player_effect_get_screen_flash(player_index,
                                 (char *)&window_params + 0x238);
  rasterizer_window_begin(&window_params);

  if (!bink_playback_has_video()) {
    scenario_fog_region_get_fog_index();
    render_sky();
    first_person_weapon_render_update();
    lights_preprocess_scene();
    render_objects();
    FUN_001959f0();
    FUN_00195b10();
    FUN_00181a90();
    scenario_test_pas();
    lights_render_diffuse();

    FUN_0017cb20(2);
    for (i = 0; i < *(int16_t *)0x5137cc; i++) {
      FUN_0017cb30(*(uint16_t *)rendered_cluster_get(i));
    }
    FUN_0017cb40();

    FUN_0017cb20(3);
    for (i = 0; i < *(int16_t *)0x5137cc; i++) {
      FUN_0017cb30(*(uint16_t *)rendered_cluster_get(i));
    }
    FUN_0017cb40();

    FUN_00195bc0();

    FUN_0017cb20(0);
    for (i = 0; i < *(int16_t *)0x5137cc; i++) {
      FUN_0017cb30(*(uint16_t *)rendered_cluster_get(i));
    }
    FUN_0017cb40();

    FUN_0017cb20(1);
    for (i = 0; i < *(int16_t *)0x5137cc; i++) {
      FUN_0017cb30(*(uint16_t *)rendered_cluster_get(i));
    }
    FUN_0017cb40();

    lights_render_specular();
    FUN_00195c40();
    FUN_00195cb0();
    FUN_00195d00();
    FUN_00195d40();
    FUN_00195dc0();
    FUN_00195e40();
    get_postgame_hilite_colors();
    FUN_000a54b0();
    FUN_0018c5b0();
    particle_systems_render();
    render_contrails_normal();
    FUN_00184980(1);

    FUN_0017cb20(4);
    for (i = 0; i < *(int16_t *)0x5137cc; i++) {
      FUN_0017cb30(*(uint16_t *)rendered_cluster_get(i));
    }
    FUN_0017cb40();

    FUN_00193c00();
    FUN_00184980(0);
    rasterizer_transparent_geometry_stop();
    FUN_00195ec0();
    FUN_00181c20();
    interface_draw_screen();
    rasterizer_screen_flash();
    render_ui_widgets(player_index,
                      &((camera_t *)rasterizer_cam)->viewport_bounds);
  }

  bink_playback_render();
  render_camera_debug_frustum(&unknown_global_camera, global_frustum);
  FUN_0018ac50();
  FUN_000977e0();
  FUN_0017e190();
  rasterizer_window_end();
  profile_render_window_end();
}

/* Render a single game window. win is the window struct (passed via ESI in the
 * original binary). offset_or_null points to a packed (x_tile, y_tile) pair
 * for split-screen tile subdivision, or NULL for full-screen rendering.
 * Handles fog distance clamping, camera frustum setup, optional water/sky
 * reflection rendering, and the main scene render pass. */
void render_window(int16_t *win, void *offset_or_null)
{
  char *esi = (char *)win;
  char *render_cam = esi + 4;
  char *rasterizer_cam = esi + 0x58;
  float bounds[4];
  int rendered_reflection = 0;
  char reflection_info[28];
  camera_t reflection_cam;
  float render_frustum[99];
  float rasterizer_frustum[99];
  float reflection_frustum[99];

  /* initialize render pass for this camera */
  ((void (*)(void *))0x1965f0)(render_cam);

  /* update render globals from scene */
  *(int16_t *)0x506732 = 0;
  FUN_0018fbc0(*(int16_t *)esi, (int)(uint16_t) * (int16_t *)0x50678a,
               (const float *)render_cam, (char *)0x506730);
  ((void (*)(int, void *))0x198f10)((int)(uint16_t) * (int16_t *)0x506784,
                                    (void *)0x506730);

  /* track closest fog distance when BSP index is unknown */
  if (*(float *)0x506748 != *(float *)0x2533c0) {
    if (*(int16_t *)0x50678a == -1 && *(float *)0x506770 > *(float *)0x506748) {
      *(float *)0x506770 = *(float *)0x506748;
    }
  }

  /* clamp z_far to fog distance when fog type matches */
  if (*(float *)0x506740 == *(float *)0x2533c8 &&
      *(float *)0x506748 != *(float *)0x2533c0) {
    float fog = *(float *)0x506748;
    if (fog < *(float *)(esi + 0x44))
      *(float *)(esi + 0x44) = fog;
  }

  /* clamp z_far to closest fog in mode 2 */
  if (*(int16_t *)0x50674c == 2 && *(float *)0x506770 != *(float *)0x2533c0) {
    float fog = *(float *)0x506770;
    if (fog < *(float *)(esi + 0x44))
      *(float *)(esi + 0x44) = fog;
  }

  /* fog sanity: z_far must exceed z_near */
  if (*(float *)(esi + 0x44) <= *(float *)(esi + 0x40)) {
    if (!*(uint8_t *)0x4d0d02) {
      error(2, "### ERROR something is wrong with the fog in the "
               "sky tag or the fog tag");
      *(uint8_t *)0x4d0d02 = 1;
    }
    *(float *)(esi + 0x44) = *(float *)(esi + 0x40) + *(float *)0x25bb10;
  }

  /* assert viewport and window bounds match between cameras */
  if (((int (*)(void *, void *, int))0x8da40)(esi + 0x30, esi + 0x84, 8) != 0) {
    display_assert("!memcmp(&window->render_camera.viewport_bounds, "
                   "&window->rasterizer_camera.viewport_bounds, "
                   "sizeof(rectangle2d))",
                   "c:\\halo\\SOURCE\\render\\render.c", 0xbb, 1);
    system_exit(-1);
  }
  if (((int (*)(void *, void *, int))0x8da40)(esi + 0x38, esi + 0x8c, 8) != 0) {
    display_assert("!memcmp(&window->render_camera.window_bounds, "
                   "&window->rasterizer_camera.window_bounds, "
                   "sizeof(rectangle2d))",
                   "c:\\halo\\SOURCE\\render\\render.c", 0xbc, 1);
    system_exit(-1);
  }

  /* compute frustum bounds from the render camera */
  ((void (*)(void *, float *))0x185950)(render_cam, bounds);

  /* split-screen tile adjustment: narrow bounds to this tile */
  if (offset_or_null != NULL) {
    int16_t *tile = (int16_t *)offset_or_null;
    int total = (int)*(int16_t *)0x31fa98 * (int)*(int16_t *)0x46e008;
    if (total > 0) {
      float tw = (bounds[1] - bounds[0]) / (float)total;
      float th = (bounds[3] - bounds[2]) / (float)total;
      int y_idx = total - (int)tile[1] - 1;
      float x0 = (float)(int)tile[0] * tw + bounds[0];
      float y0 = (float)y_idx * th + bounds[2];
      bounds[0] = x0;
      bounds[1] = x0 + tw;
      bounds[2] = y0;
      bounds[3] = y0 + th;
    }
  }

  /* build projection frustums for both cameras */
  render_camera_build_frustum((camera_t *)render_cam, bounds, render_frustum,
                              1);
  render_camera_build_frustum((camera_t *)rasterizer_cam, bounds,
                              rasterizer_frustum, 1);

  /* reflection rendering (single player local only) */
  if (game_connection() == 1) {
    char has_refl = ((char (*)(void *, void *, void *))0x1975e0)(
      render_cam, render_frustum, reflection_info);
    if (has_refl) {
      int saved_bsp = *(int *)0x506784;

      /* reflection requires full-screen viewport */
      if (*(int16_t *)(esi + 0x32) != 0) {
        display_assert("window->render_camera.viewport_bounds.x0==0",
                       "c:\\halo\\SOURCE\\render\\render.c", 0xe1, 1);
        system_exit(-1);
      }
      if (*(int16_t *)(esi + 0x30) != 0) {
        display_assert("window->render_camera.viewport_bounds.y0==0",
                       "c:\\halo\\SOURCE\\render\\render.c", 0xe2, 1);
        system_exit(-1);
      }
      if (*(int16_t *)(esi + 0x36) != 0x280) {
        display_assert("window->render_camera.viewport_bounds.x1=="
                       "RASTERIZER_TARGET_RENDER_PRIMARY_WIDTH",
                       "c:\\halo\\SOURCE\\render\\render.c", 0xe3, 1);
        system_exit(-1);
      }
      if (*(int16_t *)(esi + 0x34) != 0x1e0) {
        display_assert("window->render_camera.viewport_bounds.y1=="
                       "RASTERIZER_TARGET_RENDER_PRIMARY_HEIGHT",
                       "c:\\halo\\SOURCE\\render\\render.c", 0xe4, 1);
        system_exit(-1);
      }

      /* build reflection camera and frustum */
      ((void (*)(void *, void *, void *))0x186ef0)(render_cam, reflection_info,
                                                   &reflection_cam);
      render_camera_build_frustum(&reflection_cam, bounds, reflection_frustum,
                                  1);

      /* render reflection to secondary target */
      ((void (*)(int))0x17c960)(0);
      *(int *)0x506784 = (int)*(int16_t *)(reflection_info + 0x18);

      render_scene(-1, &reflection_cam, reflection_frustum, &reflection_cam,
                   reflection_frustum, 1, 0);

      /* restore BSP and switch back to main render target */
      *(int *)0x506784 = saved_bsp;
      ((void (*)(int))0x17c960)(1);

      rendered_reflection = 1;
    }
  }

  render_scene(*(int16_t *)win, render_cam, render_frustum, rasterizer_cam,
               rasterizer_frustum, 0, (char)rendered_reflection);
}

void render_frame(void *a2, __int16 a3, _WORD *a4, _WORD *a5, void *a6,
                  float a7)
{
  int16_t i;
  float elapsed[2];
  int16_t *win;
  int tick;
  int16_t offset[2];

  *(int32_t *)0x506540 += 1;
  *(float *)0x50654c = a7;
  csmemset(elapsed, 0, 8);
  tick = game_time_get();
  elapsed[0] = (float)tick * *(float *)0x2546a4;
  rasterizer_frame_begin(elapsed);
  rasterizer_windows_begin();
  win = (int16_t *)a2;
  for (i = 0; i < a3; i++) {
    *(int16_t *)0x50654a = i;
    if ((char)win[1] != '\0') {
      render_window_pregame(0, win);
    } else if (win[0] == -1) {
      render_window_pregame(1, win);
    } else {
      if (a5 != NULL && a4 != NULL) {
        offset[0] =
          (int16_t)(*(int16_t *)a4 * *(int16_t *)0x31fa98 + *(int16_t *)a5);
        offset[1] = (int16_t)(((int16_t *)a4)[1] * *(int16_t *)0x31fa98 +
                              ((int16_t *)a5)[1]);
      }
      render_window(win, a5 != NULL ? (void *)offset : NULL);
    }
    win += 0x56;
  }
  ((void (*)(void))0xe28e0)();
  rasterizer_windows_end();
  rasterizer_frame_end();
}
