/* object_lights.obj — c:\halo\SOURCE\objects\object_lights.c
 *
 * TU confirmed by the __FILE__ literal at VA 0x29b324, referenced by the
 * assert sites in lights_initialize (lines 0xc2 / 0xc3) and by the already
 * ported object_lights.c functions that currently live in objects.c
 * (FUN_00139930 / FUN_00139990 assert at lines 0x66f / 0x67f).
 *
 * Module globals (offsets/addresses binary-confirmed, names unproven):
 *   0x5a90bc  data_t *   light datum pool ("lights", 0x380 max, 0x7c stride)
 *   0x46f074  void *     pointer to the lights game globals block (4 bytes;
 *                        byte 0 is the "lights enabled" flag)
 *   0x5a90b0  cluster partition for lights ("light")
 *
 * Light datum offsets touched here (binary-confirmed, names unproven):
 *   +0x10  cluster reference head (passed to cluster_partition_remove_object
 *          and read as the seed handle by FUN_00191690)
 *   +0x14  real_rgb_color used by real_rgb_color_brightness
 */

/* lights_initialize (0x1391e0) — allocate the light datum pool and the lights
 * game globals block, enable lights, and create the light cluster partition.
 *
 * Confirmed: PUSH 0x7c; PUSH 0x380; PUSH 0x29b444 ("lights");
 *            CALL 0x1bfe10 => game_state_data_new("lights", 0x380, 0x7c).
 * Confirmed: PUSH 4; PUSH 0; PUSH 0x29b434 ("lights globals");
 *            CALL 0x1bfbf0 => game_state_malloc("lights globals", NULL, 4).
 *            The single ADD ESP,0x18 after the second CALL retires both
 *            3-argument cdecl frames.
 * Confirmed: the first result is stored to [0x5a90bc] *before* the second
 *            CALL (MOV [0x005a90bc],EAX at 0x1391fa) and re-loaded afterwards.
 * Confirmed: assert sites push 0x29b428 ("light_data") at line 0xc2 and
 *            0x29b414 ("lights_game_globals") at line 0xc3, both with
 *            halt=true, each followed by PUSH -1; CALL system_exit.
 * Confirmed: MOV byte ptr [EAX],0x1 writes the enable flag through [0x46f074].
 * Confirmed: PUSH 0x25b590 ("light"); PUSH 0x5a90b0; CALL 0x191500 =>
 *            cluster_partition_globals_new.
 * Confirmed: the JZ fall-through calls error(2, 0x29b3e8) =
 *            "couldn't allocate memory for object lights.".
 */
void lights_initialize(void)
{
  *(data_t **)0x5a90bc = game_state_data_new("lights", 0x380, 0x7c);
  *(void **)0x46f074 = game_state_malloc("lights globals", (char *)0, 4);
  assert_halt_msg_at("light_data", "c:\\halo\\SOURCE\\objects\\object_lights.c",
                     0xc2, *(data_t **)0x5a90bc != (data_t *)0);
  assert_halt_msg_at("lights_game_globals",
                     "c:\\halo\\SOURCE\\objects\\object_lights.c", 0xc3,
                     *(void **)0x46f074 != (void *)0);
  **(unsigned char **)0x46f074 = 1;
  if (*(data_t **)0x5a90bc != (data_t *)0) {
    cluster_partition_globals_new((void **)0x5a90b0, "light");
  } else {
    error(2, "couldn't allocate memory for object lights.");
  }
}

/* lights_dispose (0x1392a0) — drop the light cluster partition's references.
 *
 * Confirmed: PUSH 0x5a90b0; CALL 0x191630; POP ECX (one cdecl argument).
 */
void lights_dispose(void)
{
  cluster_partition_null_references((int *)0x5a90b0);
}

/* lights_initialize_for_new_map (0x1392b0) — clear the light pool, re-enable
 * lights, and clear the cluster partition.
 *
 * Confirmed: MOV EAX,[0x5a90bc]; PUSH EAX; CALL 0x119b20 => data_delete_all.
 * Confirmed: MOV ECX,[0x46f074]; MOV byte ptr [ECX],0x1 — the enable-flag
 *            store is scheduled between the PUSH and the CALL below.
 * Confirmed: PUSH 0x5a90b0; CALL 0x1915d0 => cluster_partition_clear.
 *            Combined ADD ESP,0x8 retires both single-argument frames.
 */
void lights_initialize_for_new_map(void)
{
  data_delete_all(*(data_t **)0x5a90bc);
  **(unsigned char **)0x46f074 = 1;
  cluster_partition_clear((void *)0x5a90b0);
}

/* lights_dispose_from_old_map (0x1392e0) — invalidate the light pool and
 * dispose the cluster partition.
 *
 * Confirmed: MOV EAX,[0x5a90bc]; PUSH EAX; CALL 0x119550 => data_make_invalid.
 * Confirmed: PUSH 0x5a90b0; CALL 0x191600 => cluster_partition_dispose.
 *            Combined ADD ESP,0x8 retires both single-argument frames.
 */
void lights_dispose_from_old_map(void)
{
  data_make_invalid(*(data_t **)0x5a90bc);
  cluster_partition_dispose((void *)0x5a90b0);
}

/* lights_enable (0x139300) — store the enable byte into the lights game
 * globals block and return it.
 *
 * Confirmed: MOV AL,byte ptr [EBP+0x8]; MOV ECX,[0x46f074];
 *            RET, so the byte return value is the value just written. The
 *            sole ported caller (FUN_000becd0 in players.c) consumes AL.
 */
unsigned char lights_enable(unsigned char value)
{
  lights_game_globals->enabled = value;
  return value;
}

/* light_delete (0x139310) — remove a light from the cluster partition and
 * free its datum.
 *
 * Confirmed: PUSH ESI(handle); PUSH EAX([0x5a90bc]); CALL 0x119320 =>
 *            datum_get(light_data, light_handle).
 * Confirmed: ADD EAX,0x10; PUSH EAX; PUSH ESI; PUSH 0x5a90b0; CALL 0x1919a0
 *            => cluster_partition_remove_object(partition, handle, light+0x10).
 * Confirmed: MOV ECX,[0x005a90bc] re-loads the pool pointer before
 *            PUSH ESI; PUSH ECX; CALL 0x1196d0 => datum_delete.
 * Confirmed: single ADD ESP,0x1c = 7 dwords = 2 + 3 + 2 cdecl arguments.
 */
void light_delete(int light_handle)
{
  void *light;

  light = datum_get(*(data_t **)0x5a90bc, light_handle);
  cluster_partition_remove_object((void *)0x5a90b0, light_handle,
                                  (char *)light + 0x10);
  datum_delete(*(data_t **)0x5a90bc, light_handle);
}

/* FUN_00139350 (0x139350) — collect up to max_count cluster indices for the
 * light's cluster chain into out_buffer, returning how many were written.
 *
 * Register-argument function: EAX = light_handle, EBX = out_buffer,
 * DI = max_count (kb.json @<eax>/@<ebx>/@<di>). EBX and EDI are never saved
 * by the prologue, which is what proves they are incoming parameters; only
 * ESI (the running count) is preserved.
 *
 * Confirmed: PUSH EAX; PUSH ECX([0x5a90bc]); CALL 0x119320 => datum_get.
 * Confirmed: MOV EDX,[EAX+0x10]; PUSH EDX; LEA EAX,[EBP-4]; PUSH EAX;
 *            PUSH 0x5a90b0; CALL 0x191690 =>
 *            FUN_00191690(partition, &state, light[0x10]).
 *            ADD ESP,0x14 = 5 dwords = 2 + 3 cdecl arguments.
 * Confirmed: XOR ESI,ESI seeds the count; TEST DI,DI / JLE skips the loop.
 * Confirmed: CMP AX,0xffff is a 16-bit compare against -1, and
 *            MOV word ptr [EBX+ECX*2],AX (with MOVSX ECX,SI) is a 16-bit
 *            store indexed by the sign-extended count.
 * Confirmed: the loop tail is PUSH EDX(&state); PUSH 0x5a90b0;
 *            CALL 0x1916d0; ADD ESP,0x8; CMP SI,DI; JL — a do/while whose
 *            -1 test breaks to the shared MOV AX,SI epilogue.
 */
int16_t FUN_00139350(int light_handle, int16_t *out_buffer, int16_t max_count)
{
  void *light;
  int state;
  int16_t count;
  int16_t cluster_index;

  light = datum_get(*(data_t **)0x5a90bc, light_handle);
  count = 0;
  cluster_index = (int16_t)FUN_00191690((void *)0x5a90b0, &state,
                                        *(int *)((char *)light + 0x10));
  if (max_count > 0) {
    do {
      if (cluster_index == -1) {
        break;
      }
      out_buffer[count] = cluster_index;
      count++;
      cluster_index = (int16_t)FUN_001916d0(0x5a90b0, &state);
    } while (count < max_count);
  }
  return count;
}

/* object_get_self_illumination (0x1393b0) — sum the brightness of every light
 * attachment on the object, then recurse into the two object links at +0xc8
 * and +0xc4.
 *
 * Object offsets (binary-confirmed; the attachment layout is already
 * documented by attachments_delete in objects.c):
 *   +0x00        object definition tag index ('obje')
 *   +0xc4/+0xc8  object handles (-1 = none) walked recursively
 *   +0xf4 + i    attachment type byte (0 = light)
 *   +0xfc + i*4  attachment handle (-1 = empty)
 * Object definition offset +0x140 is the attachment count (int).
 *
 * Confirmed: PUSH -0x1; PUSH EAX; CALL 0x13d680 =>
 *            object_get_and_verify_type(handle, -1).
 * Confirmed: PUSH ECX(obj[0]); PUSH 0x6f626a65 ('obje'); CALL 0x1ba140.
 * Confirmed: MOV dword ptr [EBP-0x4],0x0 seeds the float accumulator with an
 *            integer zero store; XOR EDI,EDI seeds the int16 loop counter.
 * Confirmed: MOV CL,[EAX+ESI+0xf4]; TEST CL,CL; JNZ — the guard is type == 0,
 *            and the attachment handle is loaded once
 *            (MOV EAX,[ESI+EAX*4+0xfc]) inside that guard.
 * Confirmed: ADD EAX,0x14; PUSH EAX; CALL 0x7a750 =>
 *            real_rgb_color_brightness(light + 0x14).
 * Confirmed: FADD float ptr [EBP-0x4] applies to the callee's ST0 result, so
 *            the accumulation is (call result) + total in that order.
 * Confirmed: [EBX+0x140] is re-read every iteration (MOV ECX,[EBX+0x140]
 *            before INC EDI; MOVSX EAX,DI; CMP EAX,ECX; JL).
 * Confirmed: the +0xc4 recursion tail leaves the sum in ST0 (no FSTP to the
 *            local) while the -1 path re-loads the local via FLD.
 */
float object_get_self_illumination(int object_handle)
{
  int *object;
  char *definition;
  int16_t i;
  int attachment_handle;
  void *light;
  float total;

  object = (int *)object_get_and_verify_type(object_handle, -1);
  definition = (char *)tag_get(0x6f626a65, object[0]);
  i = 0;
  total = 0.0f;
  if (*(int *)(definition + 0x140) > 0) {
    do {
      if (*((char *)object + 0xf4 + (int)i) == 0) {
        attachment_handle = *(int *)((char *)object + 0xfc + (int)i * 4);
        if (attachment_handle != -1) {
          light = datum_get(*(data_t **)0x5a90bc, attachment_handle);
          total =
            real_rgb_color_brightness((float *)((char *)light + 0x14)) + total;
        }
      }
      i++;
    } while ((int)i < *(int *)(definition + 0x140));
  }
  if (*(int *)((char *)object + 0xc8) != -1) {
    total =
      object_get_self_illumination(*(int *)((char *)object + 0xc8)) + total;
  }
  if (*(int *)((char *)object + 0xc4) != -1) {
    return object_get_self_illumination(*(int *)((char *)object + 0xc4)) +
           total;
  }
  return total;
}

/* FUN_00139480 (0x139480) — sample the structure lightmap (and the shader's
 * "gel" bitmap) straight down from a world position, producing a tint colour
 * and a secondary colour. Both outputs start out as the global ambient colour
 * at *(0x2ee70c) and are only overwritten when the downward trace hits an
 * environment shader whose lightmap/gel bitmaps are resident.
 *
 * Callers: particles.c (two sites) pass use_lightmap = 0.
 *
 * Confirmed: the two 12-byte copies from *(0x2ee70c) are three dword MOVs
 *            each (a real_rgb_color struct assignment), and the global
 *            pointer is re-loaded between them (MOV ECX,[0x002ee70c] at
 *            0x1394a1).
 * Confirmed: CALL 0x198cb0 with ADD ESP,0x20 = 8 cdecl arguments =>
 *            structure_test_vector(position, 0x29b204, point,
 *            &collection_index, &material_index, &surface_index, &u, &v).
 *            0x29b204 holds { 0.0f, 0.0f, -10.0f } — a straight-down probe.
 * Confirmed: CALL 0x18e3c0 => scenario_get; the result is spilled to
 *            [EBP-0x14] because ESI is reused for the lightmap bitmap.
 * Confirmed: tag_block_get_element(scenario+0x104, collection_index, 0x20)
 *            then tag_block_get_element(collection+0x14, material_index,
 *            0x100).
 * Confirmed: CMP word ptr [EAX+0x24],0x3 gates on shader type 3; the shader
 *            is then re-resolved through FUN_001906b0(shader, 3).
 * Confirmed: MOVSX EAX,word ptr [EBX+0x10]; CDQ; IDIV dword ptr [ECX+0x60] —
 *            a signed 32-bit modulo whose remainder (EDX) becomes the
 *            bitmap index for the gel bitmap.
 * Confirmed: XOR EDI,EDI clears the shared vertex-index pointer, which is
 *            fetched lazily and reused by the second block
 *            (TEST EDI,EDI; JNZ skips the refetch).
 * Confirmed: the clamp is FADD [0x25496c] (0.1f) then FCOM [0x2533c8] (1.0f)
 *            with TEST AH,0x41; JNZ keeping the sum — i.e. the JNZ is taken
 *            when the sum is less-or-equal, so this is min(x + 0.1f, 1.0f).
 * Confirmed: ADD ESP,0x24 after CALL 0x138fd0 = 9 dwords = the 3 uncleaned
 *            tag_block_get_element arguments plus 6 arguments of its own.
 */
void FUN_00139480(void *position, void *tint_color, void *out_color,
                  char use_lightmap)
{
  const int *ambient;
  char hit;
  char *scenario;
  int16_t *collection;
  char *material;
  char *shader;
  char *shader_env;
  char *bitmap_group;
  void *lightmap_bitmap;
  void *gel_bitmap;
  unsigned short *vertex_indices;
  float value;
  float point[3];
  float u;
  float v;
  int32_t surface_index;
  int16_t material_index;
  int16_t collection_index;

  ambient = *(const int **)0x2ee70c;
  ((int *)tint_color)[0] = ambient[0];
  ((int *)tint_color)[1] = ambient[1];
  ((int *)tint_color)[2] = ambient[2];
  ambient = *(const int **)0x2ee70c;
  ((int *)out_color)[0] = ambient[0];
  ((int *)out_color)[1] = ambient[1];
  ((int *)out_color)[2] = ambient[2];

  hit = structure_test_vector((float *)position, (float *)0x29b204, point,
                              &collection_index, &material_index,
                              &surface_index, &u, &v);
  if (hit == 0) {
    return;
  }

  scenario = (char *)scenario_get();
  collection = (int16_t *)tag_block_get_element(scenario + 0x104,
                                                (int)collection_index, 0x20);
  material = (char *)tag_block_get_element((char *)collection + 0x14,
                                           (int)material_index, 0x100);
  shader = (char *)tag_get(0x73686472, *(int *)(material + 0xc));
  if (*(int16_t *)(shader + 0x24) != 3) {
    return;
  }
  shader_env = (char *)FUN_001906b0(shader, 3);
  if (*(int *)(scenario + 0xc) == -1 || *(int *)(shader_env + 0x94) == -1 ||
      *collection == -1) {
    return;
  }

  lightmap_bitmap = FUN_00076ff0(*(int *)(scenario + 0xc), *collection);
  bitmap_group = (char *)tag_get(0x6269746d, *(int *)(shader_env + 0x94));
  gel_bitmap = FUN_00076ff0(
    *(int *)(shader_env + 0x94),
    (short)(*(int16_t *)(material + 0x10) % *(int *)(bitmap_group + 0x60)));
  vertex_indices = (unsigned short *)0;

  if (lightmap_bitmap != (void *)0 &&
      ((use_lightmap != 0 && FUN_00138ee0((int)lightmap_bitmap) != 0) ||
       xbox_texture_cache_get_hardware_format(lightmap_bitmap, 0, 0) !=
         (void *)0)) {
    vertex_indices = (unsigned short *)tag_block_get_element(scenario + 0xf8,
                                                             surface_index, 6);
    FUN_00138fd0((int)material, (int)lightmap_bitmap, vertex_indices, u, v,
                 (int)tint_color);
    value = ((float *)tint_color)[0] + *(float *)0x25496c;
    if (value > *(float *)0x2533c8) {
      value = *(float *)0x2533c8;
    }
    ((float *)tint_color)[0] = value;
    value = ((float *)tint_color)[1] + *(float *)0x25496c;
    if (value > *(float *)0x2533c8) {
      value = *(float *)0x2533c8;
    }
    ((float *)tint_color)[1] = value;
    value = ((float *)tint_color)[2] + *(float *)0x25496c;
    if (value > *(float *)0x2533c8) {
      value = *(float *)0x2533c8;
    }
    ((float *)tint_color)[2] = value;
  }

  if (gel_bitmap != (void *)0 &&
      ((use_lightmap != 0 && FUN_00138ee0((int)gel_bitmap) != 0) ||
       xbox_texture_cache_get_hardware_format(gel_bitmap, 0, 0) != (void *)0)) {
    if (vertex_indices == (unsigned short *)0) {
      vertex_indices = (unsigned short *)tag_block_get_element(
        scenario + 0xf8, surface_index, 6);
    }
    FUN_001390d0((int)material, (int)gel_bitmap, (uint16_t *)vertex_indices, u,
                 v, (float *)out_color);
  }
}
