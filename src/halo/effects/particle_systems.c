/* Spawn a foot effect where a step lands (0x9f570).
 *
 * Looks up the 'foot' effect tag by handle and range-checks the low 16 bits of
 * param_2 against the tag's first block count, then traces a short ray from
 * position (raised 0.15 in z) along the global vector at [0x31fc50] scaled by
 * 0.3, using collision flags 0xc2a0.  On a hit, the surface index is taken
 * from the collision result (+0x34) unless the hit resolves through 0x18f3e0,
 * in which case 0x1c is used; the collision result's location (+0x18), normal
 * (+0x24) and point (+0x0c) are forwarded to the foot-effect spawner at
 * 0x9f430 together with param_4.
 *
 * On a miss, and only when the collision debug flag [0x4557e9] is set, a 0.05
 * debug sphere is drawn at the original position.
 *
 * The tag_block_get_element result is intentionally discarded: the original
 * makes the call (for its bounds assert / side effect) and never reads EAX. */
void FUN_0009f570(int effect_tag_index, int param_2, void *position,
                  float param_4)
{
  float origin[3];
  float direction[3];
  char collision_result[0x50];
  int *tag_data;
  float *down;
  int surface_index;

  tag_data = (int *)tag_get(0x666f6f74, effect_tag_index);
  if ((int)(short)param_2 >= *tag_data) {
    return;
  }
  tag_block_get_element(tag_data, (int)(short)param_2, 0x1c);

  /* Copy the point verbatim (dword moves in the original), then bias z. */
  ((int *)origin)[0] = ((int *)position)[0];
  ((int *)origin)[1] = ((int *)position)[1];
  ((int *)origin)[2] = ((int *)position)[2];
  origin[2] = origin[2] + 0.15f;

  down = *(float **)0x31fc50;
  direction[0] = down[0] * 0.3f;
  direction[1] = down[1] * 0.3f;
  direction[2] = down[2] * 0.3f;

  if (FUN_0014df70(0xc2a0, origin, direction, -1,
                   (int16_t *)collision_result)) {
    surface_index =
      FUN_0018f3e0(collision_result + 0x0c, collision_result + 0x18, NULL) ?
        0x1c :
        *(int *)(collision_result + 0x34);
    FUN_0009f430(effect_tag_index, param_2, surface_index,
                 collision_result + 0x18, collision_result + 0x24,
                 collision_result + 0x0c, param_4);
    return;
  }
  if (*(char *)0x4557e9 != '\0') {
    FUN_00189540(0, position, 0.05f, *(void **)0x2ee6d0);
  }
}

void particle_systems_initialize(void)
{
  particle_system_header_data =
    game_state_data_new("particle systems", 0x40, 0x158);
  particle_system_data =
    game_state_data_new("particle system particles", 0x200, 0x80);
}

void particle_systems_initialize_for_new_map(void)
{
  data_delete_all(particle_system_header_data);
  data_delete_all(particle_system_data);
}

void particle_system_delete(int particle_system_handle)
{
  char *entry, *tag, *particle_entry;
  int particle_index, next_particle;
  short i;

  entry =
    (char *)datum_get(particle_system_header_data, particle_system_handle);
  tag = (char *)tag_get(0x7063746c, *(int *)(entry + 8));
  for (i = 0; i < *(int *)(tag + 0x5c); i++) {
    particle_index = *(int *)(entry + 0x94 + i * 0x40);
    while (particle_index != NONE) {
      particle_entry = (char *)datum_get(particle_system_data, particle_index);
      next_particle = *(int *)(particle_entry + 4);
      datum_delete(particle_system_data, particle_index);
      particle_index = next_particle;
    }
  }
  datum_delete(particle_system_header_data, particle_system_handle);
}

void particle_systems_dispose(void)
{
}

/* Re-resolve every live particle system (and each of its particles) against
 * the newly-active structure BSP (0x9f7e0).
 *
 * For each particle system datum:
 * - If it is attached to an object (+0xC != NONE), refresh its location from
 *   the object.
 * - Otherwise recompute the location from its own point (+0x20); if the point
 *   no longer falls inside any BSP leaf (location bsp index at +0x1C == NONE)
 *   the whole particle system is deleted and its particles are skipped.
 * Then walk each of the type's particle lists (heads at +0x94, stride 0x40,
 * count from the particle system definition tag at +0x5C) and re-resolve every
 * particle's location from its point; particles that fall outside the BSP are
 * unlinked from the list and deleted. */
void particle_systems_reconnect_to_structure_bsp(void)
{
  char *entry, *tag, *particle_entry;
  int *cursor;
  int index;
  short i;

  for (index = data_next_index(particle_system_header_data, NONE);
       index != NONE;
       index = data_next_index(particle_system_header_data, index)) {
    entry = (char *)datum_get(particle_system_header_data, index);
    tag = (char *)tag_get(0x7063746c, *(int *)(entry + 8));
    if (*(int *)(entry + 0xc) != NONE) {
      object_get_location(*(int *)(entry + 0xc), entry + 0x18);
    } else {
      scenario_location_from_point(entry + 0x18, entry + 0x20);
      if (*(short *)(entry + 0x1c) == NONE) {
        particle_system_delete(index);
        continue;
      }
    }
    for (i = 0; i < *(int *)(tag + 0x5c); i++) {
      cursor = (int *)(entry + 0x94 + i * 0x40);
      while (*cursor != NONE) {
        particle_entry = (char *)datum_get(particle_system_data, *cursor);
        scenario_location_from_point(particle_entry + 0x14,
                                     particle_entry + 0x1c);
        if (*(short *)(particle_entry + 0x18) == NONE) {
          datum_delete(particle_system_data, *cursor);
          *cursor = *(int *)(particle_entry + 4);
        } else {
          cursor = (int *)(particle_entry + 4);
        }
      }
    }
  }
}

/* Advance particle type state to next state index (0x9f920).
 * Computes next_state = current_state + delta, where delta is +1 or -1
 * based on the direction flag at type_state+0x38. If next_state is valid
 * (0 <= next_state < particle_states.count), just stores it. Otherwise:
 * - If type can loop (flag bit 0) and has an object and states exist:
 *   - If ping-pong mode (flag bit 1): bounce off ends, flip direction
 *   - Else: wrap to state 0
 * - Otherwise: terminate by setting both current_state and next_state to -1 */
void FUN_0009f920(void *type_state_arg, void *type_def_arg, void *ps_datum)
{
  char *type_state = (char *)type_state_arg;
  char *type_def = (char *)type_def_arg;
  char direction;
  short delta;
  short current_state;
  short next_state;
  int state_count;
  unsigned int flags;

  direction = *(char *)(type_state + 0x38);
  delta = (direction != 0) ? 1 : -1;
  current_state = *(short *)type_state;
  next_state = current_state + delta;
  *(short *)(type_state + 0x2) = next_state;

  if (next_state >= 0 && (int)next_state < *(int *)(type_def + 0x68)) {
    /* Valid next state */
    return;
  }

  /* Out of bounds - check if we can loop */
  flags = *(unsigned int *)(type_def + 0x20);
  if ((flags & 1) != 0 && *(int *)((char *)ps_datum + 0xc) != -1 &&
      (state_count = *(int *)(type_def + 0x68)) > 0) {
    /* Can loop */
    if ((flags & 2) != 0) {
      /* Ping-pong mode: bounce off ends and flip direction */
      int bounced = (int)current_state - (int)delta;
      if (bounced < 0) {
        bounced = 0;
      } else if (bounced > state_count - 1) {
        bounced = state_count - 1;
      }
      *(short *)(type_state + 0x2) = (short)bounced;
      *(char *)(type_state + 0x38) = (direction == 0) ? 1 : 0;
      return;
    }
    /* Wrap mode: restart at state 0 */
    *(short *)(type_state + 0x2) = 0;
  } else {
    /* Terminate */
    *(short *)type_state = -1;
    *(short *)(type_state + 0x2) = -1;
  }
}

/* Advance particle state to next state index (0x9f9d0).
 * Similar to FUN_0009f920 but operates on individual particle state rather
 * than type state. Computes next_state = current_state + delta, where delta
 * is +1 or -1 based on the direction flag at particle+0x2. If next_state is
 * valid (0 <= next_state < particle_states.count at sys_def+0x74), stores it.
 * Otherwise:
 * - If type can loop (flag bit 2) and states exist:
 *   - If ping-pong mode (flag bit 3): bounce off ends, flip direction
 *   - Else: wrap to state 0
 * - Otherwise: terminate by setting both current/next_state to -1 */
void FUN_0009f9d0(void *particle_arg, void *sys_def_arg)
{
  char *particle = (char *)particle_arg;
  char *sys_def = (char *)sys_def_arg;
  char direction;
  short delta;
  short current_state;
  short next_state;
  int state_count;
  unsigned int flags;

  direction = *(char *)(particle + 0x2);
  delta = (direction != 0) ? 1 : -1;
  current_state = *(short *)(particle + 0x8);
  next_state = current_state + delta;
  *(short *)(particle + 0xa) = next_state;

  if (next_state >= 0 && (int)next_state < *(int *)(sys_def + 0x74)) {
    /* Valid next state */
    return;
  }

  /* Out of bounds - check if we can loop */
  flags = *(unsigned int *)(sys_def + 0x20);
  if ((flags & 4) != 0) {
    state_count = *(int *)(sys_def + 0x74);
    if (state_count > 0) {
      /* Can loop */
      if ((flags & 8) != 0) {
        /* Ping-pong mode: bounce off ends and flip direction */
        int bounced = (int)current_state - (int)delta;
        if (bounced < 0) {
          bounced = 0;
        } else if (bounced > state_count - 1) {
          bounced = state_count - 1;
        }
        *(short *)(particle + 0xa) = (short)bounced;
        *(char *)(particle + 0x2) = (direction == 0) ? 1 : 0;
        return;
      }

      /* Wrap mode: restart at state 0 */
      *(short *)(particle + 0xa) = 0;
      return;
    }
  }

  /* Terminate: no valid next state */
  *(short *)(particle + 0x8) = -1;
  *(short *)(particle + 0xa) = -1;
}

/* Run the collision/physics step for one particle (0x9fa60).
 * Looks up the owning particle system's 'ptcl' tag by the index at
 * particle+0x8. If the particle has no active collision/attachment record
 * (particle+0xc == NONE) and the tag names a physics tag (tagdata+0x44 !=
 * NONE), fetches that 'pphy' tag and runs the collision step over the
 * particle's position (+0x20) and velocity (+0x2c) for dt, recording the
 * result into the collision-location record at +0x18. The step's return
 * value is discarded by the original (ADD ESP,0x2c then RET, EAX unused). */
void FUN_0009fa60(void *particle_arg, float dt)
{
  char *particle = (char *)particle_arg;
  char *tag;
  int physics_tag_index;

  tag = (char *)tag_get(0x7063746c, *(int *)(particle + 8));
  if (*(int *)(particle + 0xc) == NONE) {
    physics_tag_index = *(int *)(tag + 0x44);
    if (physics_tag_index != NONE) {
      FUN_00154a50(0, (int)tag_get(0x70706879, physics_tag_index),
                   (int *)(particle + 0x18), NONE, (float *)(particle + 0x20),
                   (float *)(particle + 0x2c), (float *)0, (float *)0,
                   (int16_t *)0, 1.0f, dt);
    }
  }
}

/* Copy two 3-float vectors into an output parameter block (0x9fad0).
 * out+0x1c receives the 12 bytes at src+0x60, and out+0x28 receives the 12
 * bytes at param_1+0x2c. Both copies are three dword MOV pairs in the
 * original; no FPU instruction appears in the body, so the values travel
 * through GPRs and a plain aggregate assignment reproduces the shape.
 * The second cdecl slot ([EBP+0xC]) is never read by the original; it is
 * kept in the signature so caller push order and stack shape are preserved.
 * Frame is `push ebp; mov ebp,esp; push esi` with no `sub esp` — do not add
 * locals here. */
void FUN_0009fad0(void *param_1, void *param_2, void *out, void *src)
{
  *(vector3_t *)((char *)out + 0x1c) = *(vector3_t *)((char *)src + 0x60);
  *(vector3_t *)((char *)out + 0x28) = *(vector3_t *)((char *)param_1 + 0x2c);
}

/* Advance one particle of a particle system through its collision/physics step
 * (0x9fb10).
 * Resolves the system's 'pctl' definition tag (index at system+0x8), takes the
 * particle type element `type_index` from the definition's block at +0x5c
 * (0x80 stride), and the matching per-type runtime record inside the system at
 * +0x58 (0x40 stride).
 * A particle carries a current state index at +0x8 and, when it is mid-way
 * between two states, a second index at +0xa. Both index the definition's state
 * block at +0x74 (0x178 stride).
 *   - particle+0xa == NONE: single state. Radius scale is the state's +0x80
 *     scaled by the runtime record's +0x28 and the type's +0x2c; physics comes
 *     from the state's 'pphy' tag index at +0x90.
 *   - otherwise: t = particle+0xc / particle+0x10, clamped to [0,1] (the low
 *     clamp is `FCOMP 0.0; TEST AH,5; JP` = strict `t < 0.0f`; the high clamp
 * is `FCOMP 1.0; TEST AH,0x41; JNZ`-to-skip = strict `t > 1.0f`), the scale is
 *     the lerp of the two states' +0x80, and the physics definition is the
 *     interpolation of both states' 'pphy' tags into a 0x40-byte stack buffer.
 * The FPU sequence at 0x9fbe6 is `FLD 1.0; FSUB t; FLD t; FMUL [EBX+0x80];
 * FXCH; FMUL [ECX+0x80]; FADDP` with EBX = the state at particle+0x8 and
 * ECX = the state at particle+0xa, i.e. `(1-t)*state_b + t*state_a`.
 * FUN_00154a50's collision result is tested byte-wise: bit 0 against the type's
 * flag 0x20, bit 1 against 0x10, bit 2 against 0x40; any hit clears the
 * particle's live byte at +0x3.
 * Frame is `push ebp; mov ebp,esp; sub esp,0x40` with a single 0x40-byte local
 * at EBP-0x40 and an `MOV ESP,EBP` epilogue - keep the buffer as one array.
 * MSVC recycles the dead incoming slots [EBP+0x8] and [EBP+0xc] to hold the
 * scale and `t` locals; that packing is the compiler's, not a source feature.
 */
void FUN_0009fb10(void *particle_system, int16_t type_index, float delta_time,
                  void *particle)
{
  char physics_buffer[0x40];
  char *definition;
  char *type;
  char *runtime;
  char *state_a;
  char *state_b;
  char *particle_bytes;
  void *physics;
  float scale;
  float t;
  int result;

  definition =
    (char *)tag_get(0x7063746c, *(int *)((char *)particle_system + 8));
  type = (char *)tag_block_get_element(definition + 0x5c, type_index, 0x80);
  runtime = (char *)particle_system + 0x58 + type_index * 0x40;
  particle_bytes = (char *)particle;
  state_a = (char *)tag_block_get_element(
    type + 0x74, *(int16_t *)(particle_bytes + 8), 0x178);
  if (*(int16_t *)(particle_bytes + 0xa) == NONE) {
    scale = *(float *)(state_a + 0x80) * *(float *)(runtime + 0x28) *
            *(float *)(type + 0x2c);
    physics = tag_get(0x70706879, *(int *)(state_a + 0x90));
  } else {
    state_b = (char *)tag_block_get_element(
      type + 0x74, *(int16_t *)(particle_bytes + 0xa), 0x178);
    t = *(float *)(particle_bytes + 0xc) / *(float *)(particle_bytes + 0x10);
    if (t < 0.0f) {
      t = 0.0f;
    } else if (t > 1.0f) {
      t = 1.0f;
    }
    scale = ((1.0f - t) * *(float *)(state_b + 0x80) +
             t * *(float *)(state_a + 0x80)) *
            *(float *)(runtime + 0x28) * *(float *)(type + 0x2c);
    physics = point_physics_definition_interpolate(
      tag_get(0x70706879, *(int *)(state_a + 0x90)),
      tag_get(0x70706879, *(int *)(state_b + 0x90)), t, physics_buffer);
  }
  result = FUN_00154a50(0, (int)physics, (int *)(particle_bytes + 0x14), NONE,
                        (float *)(particle_bytes + 0x1c),
                        (float *)(particle_bytes + 0x28), (float *)0,
                        (float *)0, (int16_t *)0, scale, delta_time);
  if (((result & 1) && (*(unsigned char *)(type + 0x20) & 0x20)) ||
      ((result & 2) && (*(unsigned char *)(type + 0x20) & 0x10)) ||
      ((result & 4) && (*(unsigned char *)(type + 0x20) & 0x40))) {
    *(unsigned char *)(particle_bytes + 3) = 0;
  }
}

/* Particle physics update wrapper (0x9fca0).
 * Fetches the particle's 'ltcp' definition tag from the tag index stored at
 * particle+0x8 and discards the result (the original ignores EAX; the call is
 * kept because a tag_get on a bad index asserts inside the tag system), then
 * forwards the particle and dt verbatim to the collision/physics step at
 * 0x9fa60.
 * dt travels as an untouched 4-byte copy (MOV ECX,[EBP+0xC]; PUSH ECX) with no
 * FPU instruction anywhere in the body, so it must stay typed `float` end to
 * end — an `int` passthrough would insert FILD/FSTP.
 * Frame is `push ebp; mov ebp,esp; push esi` with no `sub esp`; ESI holds the
 * particle pointer across both calls, and a single `ADD ESP,0x10` cleans up
 * both cdecl call sites. Do not introduce locals here. */
void FUN_0009fca0(void *particle_arg, float dt)
{
  tag_get(0x7063746c, *(int *)((char *)particle_arg + 8));
  FUN_0009fa60(particle_arg, dt);
}

void particle_systems_dispose_from_old_map(void)
{
  int particle_system_index;

  if (particle_system_header_data && particle_system_header_data->valid) {
    for (particle_system_index =
           data_next_index(particle_system_header_data, NONE);
         particle_system_index != NONE;
         particle_system_index = data_next_index(particle_system_header_data,
                                                 particle_system_index)) {
      particle_system_delete(particle_system_index);
    }
    data_make_invalid(particle_system_header_data);
    data_make_invalid(particle_system_data);
  }
}

/* Emit particles for a particle type (0x9fd30).
 * Calculates how many particles to emit based on dt and the type's emission
 * rate, then allocates and initializes each particle. Uses either time-based
 * accumulation or fixed/random count depending on the location-resolved flag.
 * Each particle has its creation physics applied via an indirect call. If the
 * particle fails to resolve a valid location, it's deleted; otherwise it's
 * linked into the type's particle list. */
/* type_index is a SIGNED 16-bit index: the original loads it once with
   `MOVSWL 0x8(%ebp),%ECX` at 0x9fd30+0x19 and feeds that one sign-extended
   value to both the `* 0x40` stride (`SHL $0x6`) and the tag-block index.
   Declaring it `int` dropped the sign-extension for values >= 0x8000. */
void FUN_0009fd30(void *ps_arg, int16_t type_index, float dt)
{
  char *ps = (char *)ps_arg;
  char *tag_def;
  char *type_def;
  char *type_state;
  char *state_def;
  char *particle;
  char marker_buf[8 * 0x6c]; /* 8 entries at 0x6c bytes each; original SUB
                                ESP,0x380 */
  int particle_handle;
  short loop_count;
  unsigned short target_count;
  short creation_func_idx;
  int emit_count_int;
  float emit_frac;
  char is_location_resolved;
  short location_valid;
  typedef void (*creation_physics_fn)(char *ps, short type_idx, char *particle,
                                      char *marker_buf);

  tag_def = (char *)tag_get(0x7063746c, *(int *)(ps + 8));
  type_state = ps + 0x58 + type_index * 0x40;
  type_def =
    (char *)tag_block_get_element((void *)(tag_def + 0x5c), type_index, 0x80);
  is_location_resolved = (*(unsigned int *)(ps + 4) >> 1) & 1;

  if (is_location_resolved != 0) {
    /* Fixed or random emission count */
    state_def = (char *)0;
    if ((*(unsigned int *)(type_def + 0x20) & 0x400) != 0) {
      emit_count_int = (int)*(short *)(type_def + 0x24);
      emit_frac = (float)emit_count_int * *(float *)(ps + 0x14) + 0.5f;
      target_count = (unsigned int)(int)emit_frac;
    } else {
      target_count = (unsigned int)(unsigned short)*(short *)(type_def + 0x24);
    }
  } else {
    /* Time-based emission with fractional accumulator */
    state_def = (char *)tag_block_get_element((void *)(type_def + 0x68),
                                              (int)*(short *)type_state, 0xc0);
    emit_frac = dt * *(float *)(type_state + 0x30);
    /* 0x9fdc8-0x9fdd0: `call _ftol2; movsx edx,ax; mov [ebp-0xc],edx` --
       the ftol result is truncated to a SIGNED 16-bit value before being
       stored, and it is that truncated value which 0x9fdd9 `fisub
       dword [ebp-0xc]` converts back to float. */
    emit_count_int = (short)(int)emit_frac;
    target_count =
      (unsigned int)(unsigned short)(*(short *)(type_state + 0x3a) +
                                     (short)emit_count_int);
    emit_frac =
      emit_frac - (float)emit_count_int + *(float *)(type_state + 0x34);
    *(float *)(type_state + 0x34) = emit_frac;
    if (emit_frac > 1.0f) {
      target_count = target_count + 1;
      *(float *)(type_state + 0x34) = emit_frac - 1.0f;
    }
  }

  if (*(short *)(type_state + 0x3a) >= (short)target_count) {
    goto check_emission_multiplier;
  }

  /* Set up position and orientation for new particles */
  if (*(int *)(ps + 0xc) != -1) {
    /* Get marker from attached object */
    char *obj = (char *)object_get_and_verify_type(*(int *)(ps + 0xc), -1);
    /* The 'obje' particle_systems block at +0x140 has 0x48-byte elements --
       original is `PUSH 0x48` at 0x9fd30+0x110, and five other sites
       (contrails.c x2, particles.c, objects.c x2) already use 0x48 for the
       same block.  This was 0x6c, which is the marker_buf entry stride below
       and unrelated: with the wrong stride any attachment index != 0 lands
       mid-element, so marker_elem+0x10 is a bogus string_id and the marker
       lookup fails or matches the wrong marker. */
    location_valid = object_get_markers_by_string_id(
      *(int *)(ps + 0xc),
      (void *)((char *)tag_block_get_element(
                 (void *)((char *)tag_get(0x6f626a65, *(int *)obj) + 0x140),
                 (int)*(short *)(ps + 0x10), 0x48) +
               0x10),
      marker_buf, 8);
    object_get_location(*(int *)(ps + 0xc), ps + 0x18);
  } else {
    /* No object attachment: use system position and gravity.
       Original MSVC stack layout places the position triple at
       marker_buf+0x60; the creation physics function (original binary) reads
       position from there.  Store into the marker buffer directly -- the
       separate local array is what pushed our frame to 0x38c vs the
       original's 0x380. */
    int *pos_src = (int *)(ps + 0x20);
    int *pos_dst = (int *)(marker_buf + 0x60);
    /* 0x9fe92-0x9febf: `mov edx,[0x31fc38]` then three dword copies to
       [ebp-0x344], [ebp-0x340], [ebp-0x33c].  marker_buf is at EBP-0x380
       (proved by 0x9fe39 `lea ecx,[ebp-0x380]` and 0x9ffe3
       `lea ecx,[ebp+eax-0x380]`), so 0x380-0x344 = marker_buf+0x3c.  This
       previously went to a dead local array, which clang eliminated --
       leaving the creation-physics callee reading uninitialized stack. */
    int *up_src = (int *)*(int *)0x31fc38;
    int *up_dst = (int *)(marker_buf + 0x3c);
    pos_dst[0] = pos_src[0];
    pos_dst[1] = pos_src[1];
    pos_dst[2] = pos_src[2];
    up_dst[0] = up_src[0];
    up_dst[1] = up_src[1];
    up_dst[2] = up_src[2];
    location_valid = 1;
  }

  if (*(short *)(ps + 0x1c) == -1) {
    goto check_emission_multiplier;
  }

  loop_count = 0;
  while (*(short *)(type_state + 0x3a) < (short)target_count) {
    if (location_valid == 0)
      break;
    if (loop_count >= 0x80)
      break;

    particle_handle = data_new_at_index(particle_system_data);
    if (particle_handle == -1)
      break;

    particle = (char *)datum_get(particle_system_data, particle_handle);
    if (is_location_resolved != 0) {
      creation_func_idx = *(short *)(type_def + 0x54);
    } else {
      creation_func_idx = *(short *)(state_def + 0xb0);
    }

    if (particle == (char *)0) {
      display_assert("particle",
                     "c:\\halo\\SOURCE\\effects\\particle_systems.c", 0x1dc, 1);
      system_exit(-1);
    }

    /* Initialize particle */
    *(char *)(particle + 3) = 1;
    *(short *)(particle + 8) = -1;
    *(short *)(particle + 0xa) = -1;
    *(char *)(particle + 2) = 1;
    *(float *)(particle + 0x44) = -1.0f;
    *(float *)(particle + 0x40) = random_real_range(
      (int *)random_math_get_local_seed_address(), 0.0f, 3.14159265f * 2.0f);

    if (creation_func_idx < 0 || creation_func_idx >= 3) {
      display_assert("creation_function_index>=0 && "
                     "creation_function_index<NUMBER_OF_PARTICLE_SYSTEM_TYPE_"
                     "CREATION_PHYSICS",
                     "c:\\halo\\SOURCE\\effects\\particle_systems.c", 0x1e8, 1);
      system_exit(-1);
    }

    /* Call creation physics via function table.
       0x9ffd8 calls 0x10b2d0 = random_range (int16_t result in AX), NOT
       random_real_range (0x10b270, used above for the rotation).  Its result
       selects which marker to use: 0x9ffdd-0x9ffe3
       `movsx eax,ax; imul eax,eax,0x6c; lea ecx,[ebp+eax-0x380]`, i.e.
       marker_buf + index*0x6c.  We previously discarded the result and always
       passed element 0. */
    {
      int marker_index = (int)random_range(random_math_get_local_seed_address(),
                                           0, location_valid);
      ((creation_physics_fn *)(0x26ab10))[creation_func_idx](
        ps, (short)type_index, particle, marker_buf + marker_index * 0x6c);
    }

    /* Resolve particle location from its position */
    scenario_location_from_point(particle + 0x14, particle + 0x1c);

    if (*(short *)(particle + 0x18) != -1) {
      /* Link particle into type's list */
      *(short *)(type_state + 0x3a) = *(short *)(type_state + 0x3a) + 1;
      *(int *)(particle + 4) = *(int *)(type_state + 0x3c);
      *(int *)(type_state + 0x3c) = particle_handle;
    } else {
      /* Invalid location: delete particle */
      datum_delete(particle_system_data, particle_handle);
    }

    loop_count = loop_count + 1;
  }

check_emission_multiplier:
  /* If particle count < threshold, scale down emission timer */
  if ((float)(int)*(short *)(type_state + 0x3a) <
      *(float *)(type_state + 0x2c)) {
    *(float *)(type_state + 0x4) = *(float *)(type_state + 0x4) * 0.5f;
  }
}

/* Populate particle output from state definition (0xa0080).
 * Reads particle state definition properties and fills in 7 floats in the
 * output array. First generates a random interpolation factor t, then:
 * - output[0] = random_range(state_def+0x48, state_def+0x4c)
 * - output[1] = random_range(state_def+0x50, state_def+0x54)
 * - output[2] = random_range(state_def+0x58, state_def+0x5c)
 * - output[3] = random_range(state_def+0x60, state_def+0x70)
 * - output[4] = lerp(state_def+0x64, state_def+0x74, t)
 * - output[5] = lerp(state_def+0x68, state_def+0x78, t)
 * - output[6] = lerp(state_def+0x6c, state_def+0x7c, t) */
void FUN_000a0080(void *sys_def_arg, short state_index, void *output_arg)
{
  char *sys_def = (char *)sys_def_arg;
  float *output = (float *)output_arg;
  char *state_def;
  float t;

  state_def = (char *)tag_block_get_element((void *)(sys_def + 0x74),
                                            (int)state_index, 0x178);

  /* Generate random interpolation factor */
  t =
    random_real_range((int *)random_math_get_local_seed_address(), 0.0f, 1.0f);

  /* Fill output with random ranges */
  output[1] = random_real_range((int *)random_math_get_local_seed_address(),
                                *(float *)(state_def + 0x50),
                                *(float *)(state_def + 0x54));
  output[2] = random_real_range((int *)random_math_get_local_seed_address(),
                                *(float *)(state_def + 0x58),
                                *(float *)(state_def + 0x5c));
  output[0] = random_real_range((int *)random_math_get_local_seed_address(),
                                *(float *)(state_def + 0x48),
                                *(float *)(state_def + 0x4c));
  output[3] = random_real_range((int *)random_math_get_local_seed_address(),
                                *(float *)(state_def + 0x60),
                                *(float *)(state_def + 0x70));

  /* Fill output with linear interpolations */
  output[4] =
    (*(float *)(state_def + 0x74) - *(float *)(state_def + 0x64)) * t +
    *(float *)(state_def + 0x64);
  output[5] =
    (*(float *)(state_def + 0x78) - *(float *)(state_def + 0x68)) * t +
    *(float *)(state_def + 0x68);
  output[6] =
    (*(float *)(state_def + 0x7c) - *(float *)(state_def + 0x6c)) * t +
    *(float *)(state_def + 0x6c);
}

/* Main per-particle-system update tick (0xa0180).
 * Updates object attachment, runs system physics, then iterates all
 * particle types and their linked particles — handling state transitions,
 * lifetime, interpolation, flag-based multipliers, and particle physics.
 * Dead particles are unlinked and deleted. If no active types remain and
 * the system has no object attachment, deletes the system via
 * particle_system_delete. */
void FUN_000a0180(float dt, int particle_system_handle)
{
  char *ps_datum;
  char *tag_def;
  char *type_def;
  char *type_state;
  char *volatile states_block;
  char *state_elem;
  char *particle;
  char *prev_particle;
  char *type_state_def;
  char *tag_block_ptr;
  int particle_handle;
  short next_state;
  short particle_next_state;
  short i;
  short active_types;
  float t, t_inv;
  float duration;
  float rr_lo, rr_hi;
  float rr_lo_e, rr_hi_e;
  float rr_lo_a, rr_hi_a, rr_lo_b, rr_hi_b;
  float *src;

  ps_datum =
    (char *)datum_get(particle_system_header_data, particle_system_handle);
  tag_def = (char *)tag_get(0x7063746c, *(int *)(ps_datum + 8));
  active_types = 0;

  /* Object attachment logic */
  if (*(int *)(ps_datum + 0xc) != -1) {
    char *obj =
      (char *)object_get_and_verify_type(*(int *)(ps_datum + 0xc), -1);
    if ((*(unsigned int *)(obj + 4) & 0x800) != 0 &&
        object_get_function_value(*(int *)(ps_datum + 0xc),
                                  *(short *)(ps_datum + 0x12),
                                  (void *)(ps_datum + 0x14))) {
      *(unsigned int *)(ps_datum + 4) = *(unsigned int *)(ps_datum + 4) | 1;
    } else {
      *(unsigned int *)(ps_datum + 4) = *(unsigned int *)(ps_datum + 4) & ~1u;
    }
    object_get_world_position(*(int *)(ps_datum + 0xc),
                              (vector3_t *)(ps_datum + 0x20));
    object_get_root_location(*(int *)(ps_datum + 0xc),
                             (float *)(ps_datum + 0x2c), (float *)0);
    *(float *)(ps_datum + 0x2c) =
      *(float *)(ps_datum + 0x2c) * TICKS_PER_SECOND;
    *(float *)(ps_datum + 0x30) =
      *(float *)(ps_datum + 0x30) * TICKS_PER_SECOND;
    *(float *)(ps_datum + 0x34) =
      *(float *)(ps_datum + 0x34) * TICKS_PER_SECOND;
  }

  if (*(short *)(tag_def + 0x48) < 0 || *(short *)(tag_def + 0x48) >= 2) {
    display_assert("system_definition->system_update_physics>=0 && "
                   "system_definition->system_update_physics<"
                   "NUMBER_OF_PARTICLE_SYSTEM_UPDATE_PHYSICS",
                   "c:\\halo\\SOURCE\\effects\\particle_systems.c", 0x2e1, 1);
    system_exit(-1);
  }

  /* Indirect call: system physics update */
  {
    typedef void (*system_physics_fn)(char *, float);
    ((system_physics_fn *)(0x26ab08))[*(short *)(tag_def + 0x48)](ps_datum, dt);
  }

  tag_block_ptr = tag_def + 0x5c;
  i = 0;
  if (*(int *)tag_block_ptr <= 0)
    goto done;

  /* Outer loop: iterate particle types */
  for (;;) {
    type_def =
      (char *)tag_block_get_element((void *)tag_block_ptr, (int)(short)i, 0x80);
    type_state = ps_datum + 0x58 + (int)(short)i * 0x40;

    /* Skip disabled types (flag 0x100) */
    if ((*(unsigned int *)(type_def + 0x20) & 0x100) != 0)
      goto next_type;

    /* Decrement type timer */
    *(float *)(type_state + 4) = *(float *)(type_state + 4) - dt;

    /* If current state is NONE, skip */
    if (*(short *)type_state == -1)
      goto next_type;

    states_block = type_def + 0x68;

  state_transition_loop:
    state_elem = (char *)tag_block_get_element((void *)states_block,
                                               (int)*(short *)type_state, 0xc0);
    next_state = *(short *)(type_state + 2);

    /* If timer >= 0.0, do interpolation */
    if (!(*(float *)(type_state + 4) < 0.0f))
      goto do_interpolation;

    /* Timer expired: state transition */
    if (next_state == -1) {
      /* No next state: advance via FUN_0009f920 */
      FUN_0009f920(type_state, type_def, ps_datum);
      rr_lo_a = *(float *)(state_elem + 0x28);
      rr_hi_a = *(float *)(state_elem + 0x2c);
      duration = random_real_range((int *)random_math_get_local_seed_address(),
                                   rr_lo_a, rr_hi_a);
    } else {
      *(short *)type_state = next_state;
      *(short *)(type_state + 2) = -1;
      state_elem = (char *)tag_block_get_element((void *)states_block,
                                                 (int)next_state, 0xc0);
      rr_lo_b = *(float *)(state_elem + 0x20);
      rr_hi_b = *(float *)(state_elem + 0x24);
      duration = random_real_range((int *)random_math_get_local_seed_address(),
                                   rr_lo_b, rr_hi_b);
    }
    *(float *)(type_state + 8) = duration;
    *(float *)(type_state + 4) = duration + *(float *)(type_state + 4);
    if (*(short *)type_state != -1)
      goto state_transition_loop;
    goto after_interpolation;

  do_interpolation:
    src = (float *)(state_elem + 0x34);
    if (next_state == -1) {
      /* Single state: copy properties directly */
      csmemcpy(type_state + 0xc, src, 0x28);
    } else {
      char *state_elem2 = (char *)tag_block_get_element((void *)states_block,
                                                        (int)next_state, 0xc0);
      float *src2 = (float *)(state_elem2 + 0x34);
      float *dst = (float *)(type_state + 0xc);
      int k;
      t = *(float *)(type_state + 4) / *(float *)(type_state + 8);
      if (t < 0.0f)
        t = 0.0f;
      else if (t > 1.0f)
        t = 1.0f;
      t_inv = 1.0f - t;
      k = 10;
      do {
        *dst = t * *src + t_inv * *src2;
        dst++;
        src++;
        src2++;
      } while (--k != 0);
    }

    /* Flag-based multipliers */
    if ((*(unsigned int *)(type_def + 0x20) & 0x200) != 0) {
      *(float *)(type_state + 0x18) =
        *(float *)(ps_datum + 0x38) * *(float *)(type_state + 0x18);
      *(float *)(type_state + 0x1c) =
        *(float *)(ps_datum + 0x3c) * *(float *)(type_state + 0x1c);
      *(float *)(type_state + 0x20) =
        *(float *)(ps_datum + 0x40) * *(float *)(type_state + 0x20);
      *(float *)(type_state + 0x24) =
        *(float *)(ps_datum + 0x44) * *(float *)(type_state + 0x24);
    }
    if ((*(unsigned int *)(type_def + 0x20) & 0x800) != 0) {
      *(float *)(type_state + 0x2c) =
        *(float *)(ps_datum + 0x14) * *(float *)(type_state + 0x2c);
    }
    if ((*(unsigned int *)(type_def + 0x20) & 0x1000) != 0) {
      *(float *)(type_state + 0x30) =
        *(float *)(ps_datum + 0x14) * *(float *)(type_state + 0x30);
    }
    if ((*(unsigned int *)(type_def + 0x20) & 0x2000) != 0) {
      *(float *)(type_state + 0xc) =
        *(float *)(ps_datum + 0x14) * *(float *)(type_state + 0xc);
    }
    if ((*(unsigned int *)(type_def + 0x20) & 0x4000) != 0) {
      *(float *)(type_state + 0x10) =
        *(float *)(ps_datum + 0x14) * *(float *)(type_state + 0x10);
    }
    if ((*(unsigned int *)(type_def + 0x20) & 0x8000) != 0) {
      *(float *)(type_state + 0x14) =
        *(float *)(ps_datum + 0x14) * *(float *)(type_state + 0x14);
    }

  after_interpolation:
    /* If current state is NONE, no particles to process */
    if (*(short *)type_state == -1)
      goto type_done;

    /* Emit particles if attached (flag bit 0) */
    prev_particle = (char *)0;
    if ((*(unsigned char *)(ps_datum + 4) & 1) != 0) {
      FUN_0009fd30(ps_datum, (int)(short)i, dt);
    }

    /* Inner particle loop: walk linked list at type_state + 0x3c */
    {
      short bx = *(short *)(type_state + 0x3c);
      for (;;) {
        if (bx == -1)
          goto particles_done;
        particle_handle = (int)bx;
        particle = (char *)datum_get(particle_system_data, particle_handle);

        /* Decrement particle lifetime */
        *(float *)(particle + 0xc) = *(float *)(particle + 0xc) - dt;

        /* Particle state init: if state == NONE and type has states */
        if (*(short *)(particle + 8) == -1 && *(int *)(type_def + 0x74) > 0) {
          char *pstate_elem;
          *(short *)(particle + 8) = 0;
          pstate_elem =
            (char *)tag_block_get_element((void *)(type_def + 0x74), 0, 0x178);
          rr_lo_e = *(float *)(pstate_elem + 0x20);
          rr_hi_e = *(float *)(pstate_elem + 0x24);
          duration = random_real_range(
            (int *)random_math_get_local_seed_address(), rr_lo_e, rr_hi_e);
          *(float *)(particle + 0xc) = duration;
          *(float *)(particle + 0x10) = duration;
          FUN_000a0080(type_def, *(short *)(particle + 8), particle + 0x48);
        }

        /* If particle direction byte is 0, kill state */
        if (*(char *)(particle + 3) == 0) {
          *(short *)(particle + 8) = -1;
        }

        if (*(short *)(particle + 8) != -1) {
          char *volatile pstates_block = type_def + 0x74;

          /* Particle state transition loop */
          for (;;) {
            char *pstate_elem = (char *)tag_block_get_element(
              (void *)pstates_block, (int)*(short *)(particle + 8), 0x178);

            /* If lifetime >= 0, stop transitioning */
            if (!(*(float *)(particle + 0xc) < 0.0f))
              break;

            particle_next_state = *(short *)(particle + 0xa);
            if (particle_next_state == -1) {
              /* End of states: advance particle state */
              FUN_0009f9d0(particle, type_def);
              rr_lo = *(float *)(pstate_elem + 0x28);
              rr_hi = *(float *)(pstate_elem + 0x2c);
            } else {
              /* Advance to next particle state */
              *(short *)(particle + 8) = particle_next_state;
              *(short *)(particle + 0xa) = -1;
              pstate_elem = (char *)tag_block_get_element(
                (void *)pstates_block, (int)particle_next_state, 0x178);
              rr_lo = *(float *)(pstate_elem + 0x20);
              rr_hi = *(float *)(pstate_elem + 0x24);
            }
            duration = random_real_range(
              (int *)random_math_get_local_seed_address(), rr_lo, rr_hi);
            *(float *)(particle + 0x10) = duration;
            *(float *)(particle + 0xc) = duration + *(float *)(particle + 0xc);

            if (*(short *)(particle + 0xa) != -1) {
              /* Regenerate particle output via FUN_000a0080 using next_state */
              FUN_000a0080(type_def, *(short *)(particle + 0xa),
                           particle + 0x64);
            } else {
              memcpy(particle + 0x48, particle + 0x64, 7 * 4);
            }


            if (*(short *)(particle + 8) != -1)
              continue;
            break;
          } /* end particle state transition loop */
        }

        if (*(short *)(particle + 8) == -1) {
          /* Particle is dead: unlink and delete */
          if (prev_particle != (char *)0) {
            *(int *)(prev_particle + 4) = *(int *)(particle + 4);
          } else {
            *(int *)(type_state + 0x3c) = *(int *)(particle + 4);
          }
          datum_delete(particle_system_data, particle_handle);
          bx = *(short *)(particle + 4);
          *(short *)(type_state + 0x3a) = *(short *)(type_state + 0x3a) - 1;
          continue;
        }

          /* Particle is alive: apply physics */
          type_state_def = (char *)tag_block_get_element(
            (void *)states_block, (int)*(short *)type_state, 0xc0);

          if (*(short *)(particle + 0xa) == -1) {
            /* Single state: direct scale */
            *(float *)(particle + 0x40) = *(float *)(particle + 0x50) *
                                            *(float *)(type_state + 0x14) *
                                            dt +
                                          *(float *)(particle + 0x40);
            t = *(float *)(particle + 0x4c);
          } else {
            /* Interpolated state */
            tag_block_get_element((void *)(type_def + 0x74),
                                  (int)*(short *)(particle + 8), 0x178);
            t = *(float *)(particle + 0xc) / *(float *)(particle + 0x10);
            if (t < 0.0f)
              t = 0.0f;
            else if (t > 1.0f)
              t = 1.0f;
            t_inv = 1.0f - t;
            *(float *)(particle + 0x40) =
              (t * *(float *)(particle + 0x50) +
               t_inv * *(float *)(particle + 0x6c)) *
                *(float *)(type_state + 0x14) * dt +
              *(float *)(particle + 0x40);
            t = t * *(float *)(particle + 0x4c) +
                t_inv * *(float *)(particle + 0x68);
          }

          *(float *)(particle + 0x44) =
            t * *(float *)(type_state + 0x10) * dt +
            *(float *)(particle + 0x44);

          if (*(short *)(type_state_def + 0xb2) < 0 ||
              *(short *)(type_state_def + 0xb2) >= 1) {
            display_assert(
              "type_state_definition->particle_update_physics>=0 && "
              "type_state_definition->particle_update_physics<"
              "NUMBER_OF_PARTICLE_SYSTEM_TYPE_UPDATE_PHYSICS",
              "c:\\halo\\SOURCE\\effects\\particle_systems.c", 0x3af, 1);
            system_exit(-1);
          }

          /* Indirect call: particle physics update */
          {
            typedef void (*particle_physics_fn)(char *, int, float, char *);
            ((particle_physics_fn *)(0x26ab1c))[*(
              short *)(type_state_def + 0xb2)](ps_datum, (int)(short)i, dt,
                                               particle);
          }

          prev_particle = particle;
          bx = *(short *)(particle + 4);
          continue;
      }
    }

  particles_done:
    active_types = active_types + 1;
  type_done:
  next_type:
    i = i + 1;
    if ((int)(short)i >= *(int *)tag_block_ptr)
      break;
  }

done:
  /* Clear bit 1 of flags */
  *(unsigned int *)(ps_datum + 4) = *(unsigned int *)(ps_datum + 4) & ~2u;
  /* If no active types and no object, delete system */
  if (active_types == 0 && *(int *)(ps_datum + 0xc) == -1) {
    particle_system_delete(particle_system_handle);
  }
}

/* Submit every visible particle of one particle system to the sprite renderer
 * (0xa0800).  Called once per visible system from particle_system_update
 * (0xa11eb MOV EAX,ESI / CALL 0xa0800 -- the datum index arrives in EAX and
 * there is no stack cleanup, hence the @<eax> declaration in kb.json).
 *
 * Walks the 'ptcl' tag's particle-type block (tag+0x5c, element size 0x80) and
 * for each type walks the per-system particle list whose head short lives at
 * type_state+0x3c (type_state = datum+0x58+index*0x40).  A type is skipped when
 * its state's leading short is NONE (0xa085d) or when bit 0x100 of the type
 * definition's flags dword is set (0xa086d MOV ECX,[EDI+0x20] / TEST CH,1).
 *
 * Per particle (skipped unless the byte at +3 is set and render_location_visible
 * accepts the location at +0x14, 0xa08a2/0xa08b4):
 *   - the position (+0x1c) and direction (+0x34) are pushed through the view
 *     matrix at 0x5065b4 (matrix_transform_point/_vector, 0xa08ed/0xa08ff);
 *   - the particle's current state block (type_def+0x74, element size 0x178) is
 *     fetched with the index at particle+8, and the next state with the index at
 *     particle+0xa.  When the next index is NONE (0xa090b) the radius (+0x48)
 *     and the four tint channels (+0x54..+0x60) are taken straight from the
 *     current state; otherwise they are lerped against the second block of five
 *     floats (+0x64, +0x70..+0x7c) using age/lifetime (particle+0xc / +0x10)
 *     clamped to [0,1] (0xa0976 FCOMP 0.0 with TEST AH,5/JP = strict `<`;
 *     0xa098f FCOMP 1.0 with TEST AH,0x41/JNE = strict `>`).  Both are then
 *     scaled by the type state's five multipliers (+0x0c, +0x18..+0x24).
 *   - when both states resolve to the same shader (equal shorts at +0xe2/+0xe6
 *     of state+0xb8 and equal sequence index at state+0x40, 0xa0a10..0xa0a3d)
 *     the blend collapses back to the single-state case.
 *   - the sprite index comes from the 'bitm' sequence's sprite count
 *     (sequence+0x34): a fresh particle (animation frame still the raw bit
 *     pattern of -1.0f, 0xa0a85 CMP EAX,0xBF800000) picks a random sprite and
 *     caches it back into particle+0x44, otherwise the cached frame is reduced
 *     modulo the sprite count and biased positive.
 *   - each non-negligible blend weight (> 0.01, 0xa0adf/0xa0bf6) emits one
 *     sprite batch: FUN_0018d2c0 opens the record, FUN_0018dcf0 (sprite path,
 *     type_def+0x28 == 1) or FUN_0018d6e0 adds the sprite, the batch's shader
 *     pointer (record+8) receives the state's dword at +0x80, and FUN_0018d360
 *     closes it.  The second pass nudges the view-space z of the origin by
 *     0.001 (0xa0c5f) so the two blended sprites do not z-fight.
 *
 * Confirmed: record[0xa4] is the render_sprite build record -- the frame is
 * 0x118 bytes and `record+8` (EBP-0x110, read at 0xa0bd8/0xa0cfd) is the
 * pointer FUN_0018d2c0 stores at its param_1[2] (0x18d323 MOV [EAX+8],ESI);
 * scenario.c's sprite batcher declares the same `char record[0xa4]`.
 * Uncertain: the second pass copies state_a's dword at +0x80 into state_b's
 * shader record (0xa0cf7 MOV ECX,[EBX+0x80] with EBX still the *first* state);
 * this asymmetry is reproduced verbatim. */
void FUN_000a0800(int particle_system_handle)
{
  float intensity_a;      /* EBP-0x04 */
  float intensity_b;      /* EBP-0x08 */
  int sprite_index;       /* EBP-0x0c */
  float radius;           /* EBP-0x10 */
  char *type_def;         /* EBP-0x14 */
  float tint_b;           /* EBP-0x18 */
  float tint_g;           /* EBP-0x1c */
  float tint_r;           /* EBP-0x20 */
  float tint_a;           /* EBP-0x24 */
  char *ps_datum;         /* EBP-0x28 */
  char *particle_state_b; /* EBP-0x2c */
  short i;                /* EBP-0x30 */
  char *shader_b;         /* EBP-0x34 */
  char *type_state;       /* EBP-0x38 */
  float color_b[4];       /* EBP-0x48 */
  float color_a[4];       /* EBP-0x58 */
  float origin[3];        /* EBP-0x64 */
  int *type_block;        /* EBP-0x68 */
  float direction[3];     /* EBP-0x74 */
  char record[0xa4];      /* EBP-0x118 render_sprite build record */
  int type_index;
  short particle_index;
  short sequence_index;
  char *particle;
  char *particle_state_a;
  char *shader_a;
  char *bitmap_tag;
  char *bitmap_sequence;
  unsigned int sprite_flags;

  ps_datum =
    (char *)datum_get(particle_system_header_data, particle_system_handle);
  type_block =
    (int *)((char *)tag_get(0x7063746c, *(int *)(ps_datum + 8)) + 0x5c);
  i = 0;
  type_index = 0;
  if (*type_block > 0) {
    do {
      type_def = (char *)tag_block_get_element(type_block, type_index, 0x80);
      type_state = ps_datum + 0x58 + type_index * 0x40;
      if (*(short *)type_state != NONE &&
          (*(unsigned int *)(type_def + 0x20) & 0x100) == 0) {
        particle_index = *(short *)(type_state + 0x3c);
        while (particle_index != NONE) {
          particle =
            (char *)datum_get(particle_system_data, (int)particle_index);
          if (*(char *)(particle + 3) != 0 &&
              render_location_visible(particle + 0x14) != 0) {
            particle_state_a = (char *)tag_block_get_element(
              type_def + 0x74, (int)*(short *)(particle + 8), 0x178);
            shader_b = NULL;
            matrix_transform_point((float *)0x5065b4,
                                   (float *)(particle + 0x1c), origin);
            matrix_transform_vector((float *)0x5065b4,
                                    (float *)(particle + 0x34), direction);
            if (*(short *)(particle + 0xa) == NONE) {
              particle_state_b = NULL;
              radius = *(float *)(particle + 0x48) *
                       *(float *)(type_state + 0x0c);
              tint_a = *(float *)(particle + 0x54) *
                       *(float *)(type_state + 0x18);
              tint_r = *(float *)(particle + 0x58) *
                       *(float *)(type_state + 0x1c);
              tint_g = *(float *)(particle + 0x5c) *
                       *(float *)(type_state + 0x20);
              tint_b = *(float *)(particle + 0x60) *
                       *(float *)(type_state + 0x24);
            } else {
              particle_state_b = (char *)tag_block_get_element(
                type_def + 0x74, (int)*(short *)(particle + 0xa), 0x178);
              shader_b = particle_state_b + 0xb8;
              intensity_a =
                *(float *)(particle + 0x0c) / *(float *)(particle + 0x10);
              if (intensity_a < 0.0f) {
                intensity_a = 0.0f;
              } else if (intensity_a > 1.0f) {
                intensity_a = 1.0f;
              }
              intensity_b = 1.0f - intensity_a;
              radius = (intensity_b * *(float *)(particle + 0x64) +
                        intensity_a * *(float *)(particle + 0x48)) *
                       *(float *)(type_state + 0x0c);
              tint_a = (intensity_b * *(float *)(particle + 0x70) +
                        intensity_a * *(float *)(particle + 0x54)) *
                       *(float *)(type_state + 0x18);
              tint_r = (intensity_b * *(float *)(particle + 0x74) +
                        intensity_a * *(float *)(particle + 0x58)) *
                       *(float *)(type_state + 0x1c);
              tint_g = (intensity_b * *(float *)(particle + 0x78) +
                        intensity_a * *(float *)(particle + 0x5c)) *
                       *(float *)(type_state + 0x20);
              tint_b = (intensity_b * *(float *)(particle + 0x7c) +
                        intensity_a * *(float *)(particle + 0x60)) *
                       *(float *)(type_state + 0x24);
              /* The original materializes state_a's shader base into a
               * register here (0xa0a10 LEA EAX,[EBX+0xb8]) and indexes it at
               * +0x2a / +0x2e, but folds the same fields as +0xe2 elsewhere. */
              shader_a = particle_state_a + 0xb8;
              if (!(shader_a != NULL && shader_b != NULL &&
                    *(short *)(shader_a + 0x2a) ==
                      *(short *)(shader_b + 0x2a) &&
                    *(short *)(shader_a + 0x2e) ==
                      *(short *)(shader_b + 0x2e) &&
                    *(short *)(particle_state_a + 0x40) ==
                      *(short *)(particle_state_b + 0x40))) {
                goto have_blend;
              }
            }
            intensity_a = 1.0f;
            intensity_b = 0.0f;
          have_blend:
            bitmap_tag = (char *)tag_get(
              0x6269746d, *(int *)(particle_state_a + 0x3c));
            sequence_index = *(short *)(particle_state_a + 0x40);
            if (*(short *)(type_def + 0x28) == 1) {
              sequence_index = sequence_index + 1;
            }
            bitmap_sequence = (char *)tag_block_get_element(
              bitmap_tag + 0x54, (int)sequence_index, 0x40);

            if (*(int *)(particle + 0x44) == (int)0xbf800000) {
              sprite_index =
                random_range(random_math_get_local_seed_address(), 0,
                             *(short *)(bitmap_sequence + 0x34));
              *(float *)(particle + 0x44) = (float)sprite_index;
              sprite_index = (int)*(float *)(particle + 0x44);
            } else {
              sprite_index = (short)*(float *)(particle + 0x44) %
                             *(int *)(bitmap_sequence + 0x34);
              /* The original writes only the low half of the slot here
               * (0xa0ad1 ADD AX,[EDI+0x34] / 0xa0ad5 MOV [EBP-0xc],AX), so the
               * upper 16 bits keep the raw remainder from the IDIV.  Both
               * consumers take the value as a 16-bit sprite index, so only the
               * low half is observable. */
              if ((short)sprite_index < 0) {
                *(short *)&sprite_index =
                  (short)((short)sprite_index +
                          *(short *)(bitmap_sequence + 0x34));
              }
            }

            if (intensity_a > 0.01f) {
              color_a[0] = tint_a;
              color_a[1] = tint_r;
              color_a[2] = tint_g;
              color_a[3] = tint_b;
              if (*(short *)(particle_state_a + 0xe2) == 0) {
                color_a[1] = color_a[1] * *(float *)(ps_datum + 0x48);
                color_a[2] = color_a[2] * *(float *)(ps_datum + 0x4c);
                color_a[3] = color_a[3] * *(float *)(ps_datum + 0x50);
              }
              FUN_0018d2c0((uint32_t *)record, 2,
                           *(unsigned int *)(particle_state_a + 0x3c),
                           (int)(particle_state_a + 0xb8), 0);
              sprite_flags = 1;
              if (*(short *)(type_def + 0x28) == 1) {
                if ((*(unsigned char *)(type_def + 0x20) & 0x80) != 0) {
                  sprite_flags = 3;
                }
                FUN_0018dcf0(record, sprite_flags,
                             (int)*(unsigned short *)(particle_state_a + 0x40),
                             sprite_index, origin, direction,
                             *(float *)(particle + 0x40), radius, color_a,
                             intensity_a);
              } else {
                FUN_0018d6e0(record, *(short *)(type_def + 0x2a),
                             *(unsigned short *)(particle_state_a + 0x40),
                             (short)sprite_index, origin, direction,
                             *(float *)(particle + 0x40), radius, color_a,
                             intensity_a, 1);
              }
              *(int *)(*(char **)(record + 8) + 0x98) =
                *(int *)(particle_state_a + 0x80);
              FUN_0018d360(record);
            }

            if (intensity_b > 0.01f) {
              color_b[0] = tint_a;
              color_b[1] = tint_r;
              color_b[2] = tint_g;
              color_b[3] = tint_b;
              if (*(short *)(particle_state_a + 0xe2) == 0) {
                color_b[1] = color_b[1] * *(float *)(ps_datum + 0x48);
                color_b[2] = color_b[2] * *(float *)(ps_datum + 0x4c);
                color_b[3] = color_b[3] * *(float *)(ps_datum + 0x50);
              }
              FUN_0018d2c0((uint32_t *)record, 2,
                           *(unsigned int *)(particle_state_b + 0x3c),
                           (int)shader_b, 0);
              origin[2] = origin[2] + 0.001f;
              sprite_flags = 1;
              if (*(short *)(type_def + 0x28) == 1) {
                if ((*(unsigned char *)(type_def + 0x20) & 0x80) != 0) {
                  sprite_flags = 3;
                }
                FUN_0018dcf0(record, sprite_flags,
                             (int)*(unsigned short *)(particle_state_b + 0x40),
                             sprite_index, origin, direction,
                             *(float *)(particle + 0x40), radius, color_b,
                             intensity_b);
              } else {
                FUN_0018d6e0(record, *(short *)(type_def + 0x2a),
                             *(unsigned short *)(particle_state_b + 0x40),
                             (short)sprite_index, origin, direction,
                             *(float *)(particle + 0x40), radius, color_b,
                             intensity_b, 1);
              }
              *(int *)(*(char **)(record + 8) + 0x98) =
                *(int *)(particle_state_a + 0x80);
              FUN_0018d360(record);
            }
          }
          particle_index = *(short *)(particle + 4);
        }
      }
      i = i + 1;
      type_index = (int)i;
    } while (type_index < *type_block);
  }
}

/* Initialize a newly created particle's direction, position and velocity
 * (0xa0d50).
 *
 * Reads three scalars from the particle type's block at type_def+0x5c
 * (element size 4, indices 0/1/2), generates a random unit direction into
 * state+0x28, then scales it:
 *   state+0x28 = scale_a * dir.i
 *   state+0x2c = scale_a * dir.j
 *   state+0x30 = scale_b * dir.k   (forced positive when def+0x54 is set)
 * The scaled direction is offset by the origin point (origin+0x60..0x68)
 * into state+0x1c..0x24, copied into state+0x34..0x38, and finally scaled
 * by scale_c and biased by def+0x2c..0x34 back into state+0x28..0x30.
 * state+0x3c is cleared.  The state+0x34 vector is then rotated with
 * sin=1.0/cos=0.0 about the global axis at *(float **)0x31fc44.
 *
 * Confirmed: tag_get('pctl', def+8) at 0xa0d63; tag_block_get_element(
 * tag+0x5c, block_index, 0x80) at 0xa0d76; three tag_block_get_element(
 * type_def+0x5c, {0,1,2}, 4) calls at 0xa0d83/0xa0d92/0xa0da1;
 * random_seed_get_direction3d(seed, state+0x28) at 0xa0dbb (out pointer
 * pushed at 0xa0db1 before the seed); FABS guarded by the byte at def+0x54
 * at 0xa0ddc (the value is popped on both paths); rotate_vector3d_by_sincos(
 * state+0x34, *(float **)0x31fc44, 1.0f, 0.0f) at 0xa0e47.
 * Uncertain: field meanings of state+0x1c/0x24/0x34/0x3c and origin+0x60. */
void FUN_000a0d50(void *definition, short block_index, void *state,
                  void *origin)
{
  char *def = (char *)definition;
  char *st = (char *)state;
  char *org = (char *)origin;
  char *type_def;
  void *scale_block;
  float scale_a;
  float scale_b;
  float scale_c;
  float x;
  float y;
  float z;

  type_def = (char *)tag_block_get_element(
    (char *)tag_get(0x7063746c, *(int *)(def + 8)) + 0x5c, (int)block_index,
    0x80);
  scale_block = type_def + 0x5c;
  scale_a = *(float *)tag_block_get_element(scale_block, 0, 4);
  scale_b = *(float *)tag_block_get_element(scale_block, 1, 4);
  scale_c = *(float *)tag_block_get_element(scale_block, 2, 4);

  random_seed_get_direction3d(random_math_get_local_seed_address(),
                              (float *)(st + 0x28));

  x = scale_a * *(float *)(st + 0x28);
  *(float *)(st + 0x28) = x;
  y = scale_a * *(float *)(st + 0x2c);
  *(float *)(st + 0x2c) = y;
  z = scale_b * *(float *)(st + 0x30);
  *(float *)(st + 0x30) = z;
  if (*(char *)(def + 0x54) != 0) {
    *(float *)(st + 0x30) = (float)fabs(z);
  }

  *(float *)(st + 0x1c) = x + *(float *)(org + 0x60);
  *(float *)(st + 0x20) = y + *(float *)(org + 0x64);
  *(int *)(st + 0x3c) = 0;
  *(float *)(st + 0x24) = *(float *)(org + 0x68) + *(float *)(st + 0x30);
  *(float *)(st + 0x34) = x;
  *(float *)(st + 0x38) = y;
  *(float *)(st + 0x28) = x * scale_c + *(float *)(def + 0x2c);
  *(float *)(st + 0x2c) = y * scale_c + *(float *)(def + 0x30);
  *(float *)(st + 0x30) =
    scale_c * *(float *)(st + 0x30) + *(float *)(def + 0x34);

  rotate_vector3d_by_sincos((float *)(st + 0x34), *(float **)0x31fc44, 1.0f,
                            0.0f);
}

/* Seed a particle's launch velocity, origin and axis frame for the second
 * emitter shape (0xa0e60).  Sibling of FUN_000a0d50: both sit in the emitter
 * dispatch table at 0x26ab14/0x26ab18 and share the same 4-argument shape.
 *
 * Reads three scalars from the particle type's nested tag block (type+0x5c):
 *   scale_a (index 0), scale_b (index 1), scale_c (index 2).
 * With step = (1/30) * scale_a, the launch velocity written to
 * state+0x28..0x30 is
 *     (1 - scale_b)*step * origin+0x3c..0x44        (emitter axis term)
 *   + random_unit_direction  * scale_b*step         (random term)
 *   + definition+0x2c..0x34                         (constant bias)
 * The emitter point (origin+0x60..0x68) is copied verbatim into
 * state+0x1c..0x24.  state+0x34..0x3c then receives a cross product that
 * frames the particle: cross(velocity, *global_up_vector_ptr) when
 * scale_c != 0, otherwise cross(origin+0x3c..0x44, velocity).
 *
 * Confirmed: signature recovered from the frame - [ebp+8] is captured into
 * ESI at 0xa0e67 and later supplies def+0x2c/0x30/0x34 (0xa0f09/0xa0f1d/
 * 0xa0f31); [ebp+0xc] is widened by MOVSX at 0xa0e79 so it is 16-bit;
 * [ebp+0x10] is the written-to state (0xa0ef8); [ebp+0x14] is the read-only
 * origin (0xa0ef2).  Both [ebp+8] and [ebp+0xc] are dead after their first
 * use and MSVC repacks those slots as float locals (0xa0ea0/0xa0eaf, then
 * 0xa0ed1/0xa0edf) - they are ordinary locals here, not extra parameters.
 * Both exits are a bare RET with no deliberate EAX value, so the return is
 * void; the function is only reached through the dispatch table (no CALL
 * imm32 site exists in the image).
 * Confirmed calls: tag_get('pctl', def+8) at 0xa0e74; tag_block_get_element(
 * tag+0x5c, block_index, 0x80) at 0xa0e87; three tag_block_get_element(
 * type_def+0x5c, {0,1,2}, 4) at 0xa0e94/0xa0ea3/0xa0eb2 - all five cdecl,
 * with one merged ADD ESP,0x38 at 0xa0ec8 (14 dwords).
 * The shared factor step lives in ST(1) and is consumed by FMUL ST,ST(1) at
 * 0xa0ecb and 0xa0edd, then popped at 0xa0ee2; 1/30 is the pool constant at
 * 0x26ab24 (0x3d088889) and the 1.0f at 0x2533c8.
 * random_seed_get_direction3d(seed, dir) at 0xa0eea: the out pointer is
 * pushed at 0xa0ecd BEFORE the seed getter is called at 0xa0ee4 (cdecl
 * right-to-left), and ADD ESP,8 at 0xa0f04 proves two arguments - not a
 * one-argument call as the push order suggests.
 * origin+0x60..0x68 is copied with integer MOVs at 0xa0f3a-0xa0f47.
 * The branch at 0xa0f4e is FCOMP against the 0.0f pool constant at 0x2533c0
 * with TEST AH,0x44 / JNP: JNP is taken only on equality, so the
 * fall-through arm (0xa0f5c) is the scale_c != 0 case.
 * Uncertain: field meanings of state+0x1c/0x24/0x34/0x3c, origin+0x3c and
 * origin+0x60; whether scale_c has a scalar meaning elsewhere or is only
 * ever tested as a flag. */
void FUN_000a0e60(void *definition, short block_index, void *state,
                  void *origin)
{
  char *def = (char *)definition;
  char *st = (char *)state;
  char *org = (char *)origin;
  char *type_def;
  void *scale_block;
  float scale_a;
  float scale_b;
  float scale_c;
  float step;
  float weight_random;
  float weight_axis;
  float *up;
  float cross_i;
  float cross_j;
  float cross_k;
  float dir[3];

  type_def = (char *)tag_block_get_element(
    (char *)tag_get(0x7063746c, *(int *)(def + 8)) + 0x5c, (int)block_index,
    0x80);
  scale_block = type_def + 0x5c;
  scale_a = *(float *)tag_block_get_element(scale_block, 0, 4);
  scale_b = *(float *)tag_block_get_element(scale_block, 1, 4);
  scale_c = *(float *)tag_block_get_element(scale_block, 2, 4);

  step = 0.0333333351f * scale_a;
  weight_random = scale_b * step;
  weight_axis = (1.0f - scale_b) * step;

  random_seed_get_direction3d(random_math_get_local_seed_address(), dir);

  *(float *)(st + 0x28) = weight_axis * *(float *)(org + 0x3c) +
                          dir[0] * weight_random + *(float *)(def + 0x2c);
  *(float *)(st + 0x2c) = weight_axis * *(float *)(org + 0x40) +
                          dir[1] * weight_random + *(float *)(def + 0x30);
  *(float *)(st + 0x30) = weight_axis * *(float *)(org + 0x44) +
                          dir[2] * weight_random + *(float *)(def + 0x34);

  *(float *)(st + 0x1c) = *(float *)(org + 0x60);
  *(float *)(st + 0x20) = *(float *)(org + 0x64);
  *(float *)(st + 0x24) = *(float *)(org + 0x68);

  if (scale_c != 0.0f) {
    up = *(float **)0x31fc44;
    cross_k = *(float *)(st + 0x28) * up[1] - *(float *)(st + 0x2c) * up[0];
    cross_j = *(float *)(st + 0x30) * up[0] - up[2] * *(float *)(st + 0x28);
    cross_i = up[2] * *(float *)(st + 0x2c) - *(float *)(st + 0x30) * up[1];
    *(float *)(st + 0x34) = cross_i;
    *(float *)(st + 0x38) = cross_j;
    *(float *)(st + 0x3c) = cross_k;
  } else {
    cross_k = *(float *)(st + 0x2c) * *(float *)(org + 0x3c) -
              *(float *)(st + 0x28) * *(float *)(org + 0x40);
    cross_j = *(float *)(org + 0x44) * *(float *)(st + 0x28) -
              *(float *)(st + 0x30) * *(float *)(org + 0x3c);
    cross_i = *(float *)(st + 0x30) * *(float *)(org + 0x40) -
              *(float *)(org + 0x44) * *(float *)(st + 0x2c);
    *(float *)(st + 0x34) = cross_i;
    *(float *)(st + 0x38) = cross_j;
    *(float *)(st + 0x3c) = cross_k;
  }
}

/* Initialize particle system type instances from the pctl tag (0xa0fd0).
 * For each particle type in the tag definition:
 *   - If the type has no particle states, returns false (setup failed).
 *   - Otherwise, initializes the instance state: sets the current state
 *     index to 0, next state to NONE, marks as initialized, clears
 *     particle count and first-particle handle, and picks a random
 *     duration from the first particle state's bounds.
 * Resolves the BSP location from the system's position, sets the
 * "location resolved" flag, then runs an initial 0.001s update tick
 * via FUN_000a0180 if all types were valid. */
char FUN_000a0fd0(int particle_handle)
{
  char *entry;
  char *tag;
  int *tag_block_ptr;
  char *type_def;
  char *state_elem;
  char *instance;
  short i;
  int idx;
  char result;
  float duration;

  entry = (char *)datum_get(particle_system_header_data, particle_handle);
  tag = (char *)tag_get(0x7063746c, *(int *)(entry + 8));
  result = 1;
  scenario_location_from_point(entry + 0x18, entry + 0x20);
  tag_block_ptr = (int *)(tag + 0x5c);
  *(unsigned int *)(entry + 4) |= 2;
  idx = 0;
  i = 0;
  if (*tag_block_ptr < 1) {
    result = 1;
  } else {
    do {
      instance = entry + 0x58 + idx * 0x40;
      type_def = (char *)tag_block_get_element(tag_block_ptr, idx, 0x80);
      if (*(int *)(type_def + 0x68) == 0) {
        result = 0;
      } else {
        *(short *)(instance + 0x00) = 0;
        *(short *)(instance + 0x02) = (short)NONE;
        *(char *)(instance + 0x38) = 1;
        *(short *)(instance + 0x3a) = 0;
        *(int *)(instance + 0x3c) = NONE;
        if (*(int *)(type_def + 0x68) > 0) {
          state_elem =
            (char *)tag_block_get_element((int *)(type_def + 0x68), 0, 0xc0);
          duration = random_real_range(
            (int *)random_math_get_local_seed_address(),
            *(float *)(state_elem + 0x20), *(float *)(state_elem + 0x24));
          *(float *)(instance + 0x04) = duration;
          *(float *)(instance + 0x08) = duration;
        }
      }
      i = i + 1;
      idx = (int)i;
    } while (idx < *tag_block_ptr);
    if (result == 0) {
      return 0;
    }
  }
  FUN_000a0180(0.001f, particle_handle);
  return result;
}

void particle_systems_update(float dt)
{
  int particle_system_index;

  assert_halt(particle_system_header_data &&
              particle_system_header_data->valid);
  for (particle_system_index =
         data_next_index(particle_system_header_data, NONE);
       particle_system_index != NONE;
       particle_system_index =
         data_next_index(particle_system_header_data, particle_system_index)) {
    FUN_000a0180(dt, particle_system_index);
  }
}

/* Per-frame visible-particle-system pass (0xa1170).
 *
 * Gated on the byte flag at [0x32574c]: one of four adjacent enable bytes
 * (0x32574a..0x32574d) that 0x184b60 writes together from a single argument;
 * 0xa54b0 reads the same byte.  When it is clear the entire pass is skipped
 * and the function returns immediately (000a1170 MOV AL,[0x0032574c] /
 * TEST AL,AL / JZ 0x000a120a).
 *
 * Otherwise it asserts the particle-system datum array is live and walks every
 * allocated datum.  A system is processed only when
 *   - its location has resolved to a BSP leaf: the 16-bit bsp reference at
 *     +0x1C is not NONE (000a11d4 CMP word ptr [EAX+0x1c],-0x1), and
 *   - the location block at +0x18 is potentially visible from the local
 *     player's cluster (000a11df CALL 0x0018e910, result tested in AL).
 * The global at 0x5aa8a8 is re-read on every iteration in the original
 * (000a11c5 and 000a11f2), so it is not hoisted here.
 *
 * ABI: FUN_000a0800 takes the particle-system datum index in EAX, not on the
 * stack (000a11eb MOV EAX,ESI / 000a11ed CALL 0x000a0800, no stack cleanup
 * after the call).  Confirmed at the callee: 0xa0800 immediately does
 * datum_get(g_particle_systems_data, in_EAX).  kb.json declares it
 * `int particle_system_handle@<eax>` so the forward thunk supplies EAX. */
void particle_system_update(void)
{
  int particle_system_index;
  char *entry;

  if (*(char *)0x32574c != 0) {
    assert_halt(particle_system_header_data &&
                particle_system_header_data->valid);
    for (particle_system_index =
           data_next_index(particle_system_header_data, NONE);
         particle_system_index != NONE;
         particle_system_index =
           data_next_index(particle_system_header_data,
                           particle_system_index)) {
      entry =
        (char *)datum_get(particle_system_header_data, particle_system_index);
      if (*(short *)(entry + 0x1c) != NONE &&
          scenario_location_potentially_visible_local(entry + 0x18)) {
        FUN_000a0800(particle_system_index);
      }
    }
  }
}
