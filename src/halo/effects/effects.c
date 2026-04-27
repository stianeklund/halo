void effects_initialize(void)
{
  effect_data = game_state_data_new("effect", 0x100, 0xfc);
  effect_location_data = game_state_data_new("effect location", 0x200, 0x3c);
  if (!effect_data || !effect_location_data)
    error(0, "couldn't allocate effect globals");
}

void effects_initialize_for_new_map(void)
{
  data_delete_all(effect_data);
  data_delete_all(effect_location_data);
}

void effects_dispose_from_old_map(void)
{
  data_make_invalid(effect_data);
  data_make_invalid(effect_location_data);
}

void effects_dispose(void)
{
  if (effect_data)
    effect_data = 0;
  if (effect_location_data)
    effect_location_data = 0;
}

/* Delete an effect and all its per-event datum chains (0x9c750).
 * For each event in the effect tag, walks the linked list of event datums
 * starting at effect+0x5c+i*4 and deletes each from the event datum pool
 * (0x5aa8ac). Then deletes the effect datum from the effects pool (0x5aa8b0). */
void FUN_0009c750(int effect_handle)
{
  char *effect = (char *)(int)datum_absolute_index_to_index(
      *(data_t **)0x5aa8b0, effect_handle);
  if (!effect)
    return;

  char *tag_data = (char *)tag_get(0x65666665, *(int *)(effect + 4));
  int count = *(int *)(tag_data + 0x28);

  int16_t i = 0;
  while ((int)i < count) {
    int cursor = *(int *)(effect + 0x5c + (int)i * 4);
    while (cursor != -1) {
      char *datum = (char *)datum_get(*(data_t **)0x5aa8ac, cursor);
      datum_delete(*(data_t **)0x5aa8ac, cursor);
      cursor = *(int *)(datum + 4);
    }
    i++;
  }

  datum_delete(*(data_t **)0x5aa8b0, effect_handle);
}

/* Check if any active effect with a nonzero danger radius is close enough
 * to any player to be considered dangerous. Iterates all effects, skips
 * those with flag bit 3 set or zero danger radius, then for each player
 * walks the effect's location list and computes squared distance to the
 * player's bounding sphere. Returns true as soon as one location falls
 * within (object_radius + danger_radius). */
bool dangerous_effects_near_player(void)
{
  int effect_index;
  void *effect;
  void *effect_tag;
  data_iter_t iter;
  void *player;
  void *player_object;
  int location_index;
  void *location;
  float position[3];

  for (effect_index = data_next_index(effect_data, NONE); effect_index != NONE;
       effect_index = data_next_index(effect_data, effect_index)) {
    effect = datum_get(effect_data, effect_index);
    effect_tag = tag_get(0x65666665, *(int *)((char *)effect + 4));

    /* skip effects with "completed" flag or zero danger radius */
    if ((*(uint8_t *)((char *)effect + 2) & 8) != 0)
      continue;
    if (*(float *)((char *)effect_tag + 8) == 0.0f)
      continue;

    data_iterator_new(&iter, player_data);
    for (player = data_iterator_next(&iter); player != NULL;
         player = data_iterator_next(&iter)) {
      if (*(int *)((char *)player + 0x34) == NONE)
        continue;

      player_object =
        object_get_and_verify_type(*(int *)((char *)player + 0x34), NONE);

      /* walk the effect's location linked list */
      location_index = *(int *)((char *)effect + 0x5c);
      while (location_index != NONE) {
        uint16_t node_index;
        float radius_sum, dx, dy, dz, dist_sq;

        location = datum_get(effect_location_data, location_index);
        location_index = *(int *)((char *)location + 4);

        /* negative (but not -1) node index → resolve via helper */
        if (*(int16_t *)((char *)location + 2) != -1 &&
            *(int16_t *)((char *)location + 2) < 0) {
          location = ((void *(*)(void *, int *, int))0x9cca0)(
            effect, &location_index, 0);
        }
        if (location == NULL)
          break;

        /* resolve world-space position from node or direct coords */
        node_index = *(uint16_t *)((char *)location + 2);
        if (node_index == 0xffff) {
          /* no node attachment — use position directly */
          position[0] = *(float *)((char *)location + 0x30);
          position[1] = *(float *)((char *)location + 0x34);
          position[2] = *(float *)((char *)location + 0x38);
        } else {
          void *matrix;
          if ((int16_t)node_index < 0) {
            matrix = ((void *(*)(int, int))0xdd410)(
              *(uint16_t *)((char *)effect + 0x4c), node_index & 0x7fff);
          } else {
            matrix = ((void *(*)(int, int))0x140eb0)(
              *(int *)((char *)effect + 0x3c), node_index & 0x7fff);
          }
          /* transform local position to world space via node matrix */
          ((void (*)(void *, void *, float *))0x109590)(
            matrix, (char *)location + 0x30, position);
        }

        /* squared-distance check against player bounding sphere */
        radius_sum = *(float *)((char *)player_object + 0x5c) +
                     *(float *)((char *)effect_tag + 8);
        dx = position[0] - *(float *)((char *)player_object + 0x50);
        dy = position[1] - *(float *)((char *)player_object + 0x54);
        dz = position[2] - *(float *)((char *)player_object + 0x58);
        dist_sq = dy * dy + dx * dx + dz * dz;
        if (radius_sum * radius_sum >= dist_sq)
          return true;
      }
    }
  }

  return false;
}

/* Per-frame update for a single effect instance. Handles: attached object
 * tracking (marker resolution, color inheritance), trigger/kill volume
 * testing, and the event timing state machine. Each effect steps through
 * its tag's event list; when an event completes, the next event is chosen
 * (with a random skip-fraction check). New events spawn particles and
 * sub-effects via the parts array. Up to 8 event transitions can occur
 * per frame to handle very short events. */
void effect_update(int effect_index, float elapsed)
{
  void *effect;
  void *effect_tag;
  void *object;
  short loop_count;
  bool transitioned;

  effect = datum_get(effect_data, effect_index);
  effect_tag = tag_get(0x65666665, *(int *)((char *)effect + 4));

  /* ---- attached object tracking ---- */
  if (*(int *)((char *)effect + 0x3c) != NONE) {
    void *parent_obj;

    /* if object was deleted, delete the effect too */
    object =
      ((void *(*)(int, int))0x13d640)(*(int *)((char *)effect + 0x3c), NONE);
    if (object == NULL)
      goto delete_effect;

    /* copy position/orientation from the root object's node matrices */
    {
      int parent_handle =
        ((int (*)(int))0x13d7f0)(*(int *)((char *)effect + 0x3c));
      parent_obj = object_get_and_verify_type(parent_handle, NONE);
    }
    if ((*(uint32_t *)((char *)parent_obj + 4) & 0x800) == 0) {
      *(int16_t *)((char *)effect + 0x14) = NONE;
    } else {
      *(int *)((char *)effect + 0x10) = *(int *)((char *)parent_obj + 0x48);
      *(int *)((char *)effect + 0x14) = *(int *)((char *)parent_obj + 0x4c);
      *(int *)((char *)effect + 0x24) = *(int *)((char *)parent_obj + 0x18);
      *(int *)((char *)effect + 0x28) = *(int *)((char *)parent_obj + 0x1c);
      *(int *)((char *)effect + 0x2c) = *(int *)((char *)parent_obj + 0x20);
    }

    if ((*(uint8_t *)((char *)effect + 2) & 2) == 0)
      goto check_location;

    /* resolve marker A on the attached object */
    {
      char marker_found = ((char (*)(int, int, void *))0x1403a0)(
        *(int *)((char *)effect + 0x3c),
        (int)*(uint16_t *)((char *)effect + 0x8), (char *)effect + 0x44);

      if (!marker_found) {
        if ((*(uint8_t *)effect_tag & 1) != 0) {
          /* tag says "deleted when attachment deactivates": clear slot */
          void *obj_tag = tag_get(0x6f626a65, *(int *)object);
          int count = *(int *)((char *)obj_tag + 0x140);
          short i;
          for (i = 0; (int)i < count; i++) {
            if (*(int *)((char *)object + (int)i * 4 + 0xfc) == effect_index) {
              *(int *)((char *)object + (int)i * 4 + 0xfc) = NONE;
              goto delete_effect;
            }
          }
          goto delete_effect;
        }
        if ((*(uint8_t *)((char *)effect + 2) & 0xc) == 0)
          ((void (*)(int, int))0x9ceb0)(effect_index, 0);
      } else {
        /* marker found — check if effect was waiting to restart */
        uint16_t ef = *(uint16_t *)((char *)effect + 2);
        if ((ef & 8) != 0) {
          if ((ef & 0x20) != 0) {
            ((void (*)(int))0x9c750)(effect_index);
          } else {
            /* clear completed flag and restart from event 0 */
            *(uint16_t *)((char *)effect + 2) = ef & 0xfff7;
            FUN_0009cb90(effect_index, 0);
          }
        }
      }

      /* resolve marker B */
      ((void (*)(int, int, void *))0x1403a0)(
        *(int *)((char *)effect + 0x3c),
        (int)*(uint16_t *)((char *)effect + 0xa), (char *)effect + 0x48);

      /* inherit color from object node */
      if (*(int16_t *)((char *)effect + 0xc) != NONE) {
        int node_idx = (int)*(int16_t *)((char *)effect + 0xc);
        float *src = (float *)((char *)object + (node_idx + 0x1e) * 12);
        float *color = (float *)((char *)effect + 0x18);
        color[0] = src[0];
        color[1] = src[1];
        color[2] = src[2];
        if (!((bool (*)(float *))0x7b020)(color)) {
          error(2, "%s: assert_valid_real_rgb_color(%f, %f, %f)",
                "&effect->color", (double)color[0], (double)color[1],
                (double)color[2], "c:\\halo\\SOURCE\\effects\\effects.c", 0x4cb,
                1);
          system_exit(NONE);
        }
      }
    }
  }

check_location:
  /* ---- trigger/kill volume test ---- */
  if (*(int16_t *)((char *)effect + 0x14) == NONE)
    goto check_flags;
  {
    bool in_vol;
    if ((*(uint8_t *)effect_tag & 2) != 0)
      in_vol = ((bool (*)(void *))0x18e9b0)((char *)effect + 0x10);
    else
      in_vol = ((bool (*)(void *))0x18e910)((char *)effect + 0x10);
    if (!in_vol)
      goto check_flags;
    {
      uint16_t f = *(uint16_t *)((char *)effect + 2);
      if ((f & 0x10) == 0)
        goto event_loop;
      *(uint16_t *)((char *)effect + 2) = f & 0xffef;
      goto event_loop;
    }
  }

check_flags: {
  uint16_t f = *(uint16_t *)((char *)effect + 2);
  if (f & 0x10)
    goto event_loop;
  if (!(f & 2))
    goto delete_effect;
  *(uint16_t *)((char *)effect + 2) = f | 0x10;
}

event_loop:
  /* ---- main event timing loop (up to 8 transitions per frame) ---- */
  loop_count = 0;
  if (elapsed < 0.0f)
    return;

  for (;;) {
    uint16_t ef_flags;
    float time_remaining;

    ef_flags = *(uint16_t *)((char *)effect + 2);
    if (ef_flags & 8)
      return;
    if (loop_count >= 8)
      return;

    time_remaining =
      *(float *)((char *)effect + 0x54) - *(float *)((char *)effect + 0x50);
    if (time_remaining > elapsed) {
      /* event continues — consume elapsed and stop */
      transitioned = false;
      *(float *)((char *)effect + 0x50) += elapsed;
      elapsed = -1.0f;
    } else {
      /* event completed — advance and keep remaining time */
      transitioned = true;
      elapsed = elapsed - time_remaining;
      *(float *)((char *)effect + 0x50) = *(float *)((char *)effect + 0x54);
    }

    if (ef_flags & 1) {
      /* ---- have active event: update it ---- */
      if ((ef_flags & 0x10) == 0) {
        FUN_0009d590(effect);
      }

      if (transitioned) {
        /* determine next event index */
        short next_event;
        if ((*(uint8_t *)((char *)effect + 2) & 2) != 0 &&
            *(int16_t *)((char *)effect + 0x4e) ==
              *(int16_t *)((char *)effect_tag + 6) &&
            (next_event = *(int16_t *)((char *)effect_tag + 4),
             next_event != NONE)) {
          /* looping effect: jump back to loop start event */
        } else {
          next_event = *(int16_t *)((char *)effect + 0x4e) + 1;
        }

        /* skip events whose random roll exceeds skip_fraction */
        while ((int)(int16_t)next_event < *(int *)((char *)effect_tag + 0x34)) {
          int *seed;
          float rnd;
          void *evt;
          void *tag2 = tag_get(0x65666665, *(int *)((char *)effect + 4));
          if (*(uint8_t *)tag2 & 2)
            seed = get_global_random_seed_address();
          else
            seed = ((int *(*)(void))0x10b120)();
          rnd = ((float (*)(int *))0x10b240)(seed);
          evt = tag_block_get_element((char *)effect_tag + 0x34,
                                      (int)(int16_t)next_event, 0x44);
          if (rnd >= *(float *)((char *)evt + 4))
            break;
          next_event++;
        }

        if ((int)(int16_t)next_event >= *(int *)((char *)effect_tag + 0x34)) {
          /* past all events */
          if (*(uint16_t *)((char *)effect + 2) & 2) {
            *(uint16_t *)((char *)effect + 2) |= 8;
            return;
          }
          ((void (*)(int))0x9c750)(effect_index);
          return;
        }

        /* start the chosen event */
        {
          FUN_0009cb90(effect_index, (int)(int16_t)next_event);
        }
      }
    } else {
      /* ---- no active event yet: initialize it ---- */
      if (transitioned) {
        void *event_elem;
        int *seed;
        void *tag2;
        int parts_count;
        int *parts_block;
        short part_i;
        int part_idx;

        event_elem =
          tag_block_get_element((char *)effect_tag + 0x34,
                                (int)*(int16_t *)((char *)effect + 0x4e), 0x44);
        *(uint8_t *)((char *)effect + 2) |= 1;
        *(int *)((char *)effect + 0x50) = 0;
        *(int *)((char *)effect + 0x58) = 0xbf800000; /* -1.0f */

        tag2 = tag_get(0x65666665, *(int *)((char *)effect + 4));
        if (*(uint8_t *)tag2 & 2)
          seed = get_global_random_seed_address();
        else
          seed = ((int *(*)(void))0x10b120)();

        /* set event duration from random range */
        *(float *)((char *)effect + 0x54) =
          ((float (*)(int *, float, float))0x10b270)(
            seed, *(float *)((char *)event_elem + 0x10),
            *(float *)((char *)event_elem + 0x14));

        /* create particles/effects for each part in the event */
        parts_count = *(int *)((char *)event_elem + 0x38);
        parts_block = (int *)((char *)event_elem + 0x38);
        for (part_i = 0, part_idx = 0; part_idx < parts_count;
             part_i++, part_idx = (int)(int16_t)part_i) {
          void *part;
          float lo, hi, base, range, scale_a, scale_b;
          int flags_a, flags_b, mask;
          int *seed2;
          float result;
          uint8_t byte_val;

          part = tag_block_get_element(parts_block, part_idx, 0xe8);
          lo = (float)(int)*(int16_t *)((char *)part + 0x6c);
          hi = (float)(int)*(int16_t *)((char *)part + 0x6e);
          seed2 = ((int *(*)(void))0x10b120)();
          flags_a = *(int *)((char *)part + 0xe0);
          flags_b = *(int *)((char *)part + 0xe4);
          scale_a = *(float *)((char *)effect + 0x44);
          scale_b = *(float *)((char *)effect + 0x48);

          /* inline effect_compute_scale: scale lo/hi by effect
           * A/B scale factors based on per-part flag bits */
          base = lo;
          mask = 1 << 5;
          if (flags_a & mask)
            base *= scale_a;
          if (flags_b & mask)
            base *= scale_b;
          range = hi - lo;
          mask = 1 << 6;
          if (flags_a & mask)
            range *= scale_a;
          if (flags_b & mask)
            range *= scale_b;
          result =
            ((float (*)(int *, float, float))0x10b270)(seed2, 0.0f, range) +
            base;

          /* __ftol2: convert float to byte count */
          byte_val = (uint8_t)(int)result;
          *((uint8_t *)effect + 0xdc + part_idx) = byte_val;

          /* remap values > 6 based on local player count */
          if (byte_val > 6) {
            float temp = (float)(int)byte_val - *(float *)0x254640;
            int16_t count = local_player_count();
            temp = temp / (float)(int)count + *(float *)0x254640;
            *((uint8_t *)effect + 0xdc + part_idx) = (uint8_t)(int)temp;
          }
        }

        /* create effect locations for the new event */
        if ((*(uint8_t *)((char *)effect + 2) & 0x10) == 0) {
          FUN_0009e310(effect);
        }
      }
    }

    /* advance loop counter and check if elapsed time remains */
    loop_count++;
    if (elapsed < 0.0f)
      return;
  }

delete_effect:
  ((void (*)(int))0x9c750)(effect_index);
}

bool effects_update(float elapsed)
{
  int effect_index;

  if (profile_global_enable && *(bool *)0x2eebf0)
    profile_enter_private((void *)0x2eebe8);

  for (effect_index = data_next_index(effect_data, NONE); effect_index != NONE;
       effect_index = data_next_index(effect_data, effect_index)) {
    effect_update(effect_index, elapsed);
  }

  if (profile_global_enable && *(bool *)0x2eebf0)
    profile_exit_private((void *)0x2eebe8);

  return false;
}
