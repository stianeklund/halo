void contrails_initialize_for_new_map(void)
{
  data_delete_all(contrail_data);
  data_delete_all(contrail_point_data);
}

void contrails_dispose_from_old_map(void)
{
  data_make_invalid(contrail_point_data);
  data_make_invalid(contrail_data);
}

void contrails_dispose(void)
{
  if (contrail_point_data)
    contrail_point_data = 0;
  if (contrail_data)
    contrail_data = 0;
}

/* Delete a contrail and all its points across all 4 instance lists. */
void contrail_delete(int datum_handle)
{
  char *contrail;
  int *list;
  int i;

  contrail = (char *)datum_get(contrail_data, datum_handle);
  list = (int *)(contrail + 0x34);

  for (i = 0; i < 4; i++) {
    int point = list[i];
    while (point != -1) {
      char *pt = (char *)datum_get(contrail_point_data, point);
      int next = *(int *)(pt + 0x34);
      datum_delete(contrail_point_data, point);
      point = next;
    }
  }
  datum_delete(contrail_data, datum_handle);
}

/* Compute how many new points to add this tick based on the contrail's
 * point interval and remaining time accumulator. Returns point count.
 * datum_handle arrives via EAX. */
int contrail_compute(int datum_handle, float delta_time, int count)
{
  char *contrail;
  char *tag;
  float interval;
  float period;
  int16_t result = 0;

  contrail = (char *)datum_get(contrail_data, datum_handle);
  tag = (char *)tag_get(0x636f6e74, *(int *)(contrail + 4));
  interval = *(float *)(tag + 4);

  /* scale interval by contrail's speed if flag set */
  if (*(uint8_t *)(tag + 2) & 1)
    interval *= *(float *)(contrail + 0x10);

  period = *(float *)0x2533c8 / interval;

  /* skip if delta_time is zero/sentinel */
  if (delta_time == *(float *)0x2533c0)
    goto done;

  /* consume time in period-sized steps */
  while (delta_time >= *(float *)(contrail + 0x20)) {
    float step = *(float *)(contrail + 0x20);
    delta_time -= step;
    *(float *)(contrail + 0x20) = period;
    result++;
    if (delta_time <= *(float *)0x2533c0)
      goto done;
  }
  *(float *)(contrail + 0x20) -= delta_time;

done:
  return (int)result;
}

/* Debug validation: check attachment consistency and point list counts.
 * datum_handle arrives via EAX. */
void contrail_validate(int datum_handle)
{
  char *contrail;
  int i;

  contrail = (char *)datum_get(contrail_data, datum_handle);
  tag_get(0x636f6e74, *(int *)(contrail + 4));

  /* validate object attachment if present */
  if (*(int *)(contrail + 8) != -1) {
    char *obj = (char *)object_get_and_verify_type(*(int *)(contrail + 8), -1);
    int16_t attach_idx = *(int16_t *)(contrail + 0xc);

    if (attach_idx < 0 ||
        (int)attach_idx >=
          *(int *)((char *)tag_get(0x6f626a65, *(int *)obj) + 0x140)) {
      char *name = ((char *(*)(int, ...))0x1ba1f0)(
        *(int *)(contrail + 4), (int)attach_idx,
        "c:\\halo\\SOURCE\\effects\\contrails.c", 0x28e, 1);
      char *msg = csprintf(
        (char *)0x5ab100,
        "contrail %s attachment index %d is outside the valid range.", name);
      display_assert(msg, "c:\\halo\\SOURCE\\effects\\contrails.c", 0x28e, 1);
      system_exit(-1);
    }

    /* verify the object's attachment slot points back to us */
    if (*(int *)(obj + (int)*(int16_t *)(contrail + 0xc) * 4 + 0xfc) !=
        datum_handle) {
      char *name = ((char *(*)(int, ...))0x1ba1f0)(*(int *)(contrail + 4));
      char *msg = csprintf(
        (char *)0x5ab100,
        "contrail %s (%ld) has an object that thinks it's attached to %ld",
        name, datum_handle,
        *(int *)(obj + (int)*(int16_t *)(contrail + 0xc) * 4 + 0xfc));
      display_assert(msg, "c:\\halo\\SOURCE\\effects\\contrails.c", 0x28f, 1);
      system_exit(-1);
    }
  }

  /* verify point counts match actual linked list lengths */
  {
    int16_t *counts = (int16_t *)(contrail + 0x2c);
    int *heads = (int *)(contrail + 0x34);
    for (i = 0; i < 4; i++) {
      int16_t actual = 0;
      int pt = heads[i];
      while (pt != -1) {
        pt = *(int *)((char *)datum_get(contrail_point_data, pt) + 0x34);
        actual++;
      }
      if (actual != counts[i]) {
        char *name = ((char *(*)(int, ...))0x1ba1f0)(
          *(int *)(contrail + 4), (int)counts[i], (int)actual,
          "c:\\halo\\SOURCE\\effects\\contrails.c", 0x2a5, 1);
        char *msg = csprintf(
          (char *)0x5ab100,
          "contrail %s thinks it has %d points but really it has %d", name);
        display_assert(msg, "c:\\halo\\SOURCE\\effects\\contrails.c", 0x2a5,
                       1);
        system_exit(-1);
      }
    }
  }
}

void contrails_initialize(void)
{
  contrail_data = game_state_data_new("contrail", 0x100, 0x44);
  contrail_point_data = game_state_data_new("contrail point", 0x400, 0x38);
  if (contrail_data) {
    if (contrail_point_data)
      return;
    contrail_data = 0;
  } else {
    if (contrail_point_data) {
      contrail_point_data = 0;
      error(0, "couldn't allocate contrail globals");
      return;
    }
  }
  error(0, "couldn't allocate contrail globals");
}

/* Advance the sprite animation for a contrail point. Picks a new random
 * bitmap sprite when the current one expires. datum_ptr arrives via ESI. */
void contrail_add_point(char *datum_ptr)
{
  char *tag;
  char *bitmaps;
  int16_t sprite_idx;

  tag = (char *)tag_get(0x636f6e74, *(int *)(datum_ptr + 4));
  bitmaps = (char *)tag_get(0x6269746d, *(int *)(tag + 0x3c));
  sprite_idx = *(int16_t *)(datum_ptr + 0x14);

  *(int16_t *)(datum_ptr + 0x16) += 1;
  *(int *)(datum_ptr + 0x24) = 0;

  if (sprite_idx >= 0) {
    if ((int)sprite_idx < *(int *)(bitmaps + 0x54) &&
        *(int16_t *)(datum_ptr + 0x16) >= 0) {
      char *seq = (char *)((int (*)(void *, int, int))0x19b210)(
        bitmaps + 0x54, (int)sprite_idx, 0x40);
      if (*(int16_t *)(datum_ptr + 0x16) < *(int16_t *)(seq + 0x22))
        return;
    }
  }

  /* pick a new random sprite */
  {
    int16_t first = *(int16_t *)(tag + 0x40);
    int16_t count = *(int16_t *)(tag + 0x42) + first;
    int rng = ((int (*)(int16_t, int16_t))0x10b120)(first, count);
    int16_t new_idx = (int16_t)((int (*)(int))0x10b2d0)(rng);
    *(int16_t *)(datum_ptr + 0x14) = new_idx;
    *(int16_t *)(datum_ptr + 0x16) = 0;
  }
}

void contrails_update(float delta_time)
{
  int datum_handle;
  char *datum;
  char *tag;
  float elapsed;
  float render_freq;
  float remaining;
  float step;
  uint8_t attached;
  int saved_point;
  int result;
  int16_t i;

  datum_handle = data_next_index(contrail_data, -1);
  while (datum_handle != -1) {
    datum = (char *)datum_get(contrail_data, datum_handle);
    tag = (char *)tag_get(0x636f6e74, *(int *)(datum + 4));
    elapsed = delta_time - *(float *)(datum + 0x28);
    contrail_validate(datum_handle);
    *(int *)(datum + 0x28) = 0;
    if (*(int *)(datum + 8) != -1) {
      attached = ((uint8_t(*)(int, int, void *))0x1403a0)(
        *(int *)(datum + 8), *(uint16_t *)(datum + 0xe),
        (void *)(datum + 0x10));
      if (attached != (*(uint8_t *)(datum + 2) & 1)) {
        saved_point = *(int *)(datum + 0x10);
        *(int *)(datum + 0x10) = 0;
        contrail_set_state(datum_handle, 1, 1);
        *(int *)(datum + 0x10) = saved_point;
      }
      if (attached == 0) {
        *(uint8_t *)(datum + 2) &= 0xfe;
      } else {
        *(uint8_t *)(datum + 2) |= 1;
        result = contrail_compute(datum_handle, delta_time, 1);
        contrail_set_state(datum_handle, result, 1);
      }
    }
    render_freq = *(float *)(tag + 0x2c);
    if ((*(uint8_t *)(tag + 2) & 0x20) != 0)
      render_freq *= *(float *)(datum + 0x10);
    render_freq = *(float *)0x2533c8 / render_freq;
    remaining = elapsed;
    if (remaining > *(float *)0x2533c0) {
      do {
        step = render_freq - *(float *)(datum + 0x24);
        if (step <= remaining) {
          *(float *)(datum + 0x24) = remaining + *(float *)(datum + 0x24);
          break;
        }
        contrail_add_point(datum);
        remaining -= step;
      } while (remaining > *(float *)0x2533c0);
    }
    {
      float fade_rate = *(float *)(tag + 0x24);
      if ((*(uint8_t *)(tag + 3) & 1) != 0)
        fade_rate *= *(float *)(datum + 0x10);
      *(float *)(datum + 0x18) -= fade_rate * elapsed;
    }
    {
      float grow_rate = *(float *)(tag + 0x28);
      if ((*(uint8_t *)(tag + 3) & 2) != 0)
        grow_rate *= *(float *)(datum + 0x10);
      *(float *)(datum + 0x1c) += grow_rate * elapsed;
    }
    contrail_render_update(datum_handle, delta_time);
    i = 0;
    do {
      if (((0 < *(int16_t *)(datum + 0x2c + (int)i * 2)) !=
           (*(int *)(datum + 0x34 + (int)i * 4) != -1)) == true) {
        display_assert(
          "contrail->contrail_point_counts[instance_index]>0=="
          "contrail->first_contrail_point_indices[instance_index]!=NONE",
          "c:\\halo\\SOURCE\\effects\\contrails.c", 0x10a, 1);
        system_exit(-1);
      }
    } while (*(int *)(datum + 0x34 + (int)i * 4) == -1 && (i++, i < 4));
    contrail_validate(datum_handle);
    if (i == 4 && *(int *)(datum + 8) == -1)
      contrail_delete(datum_handle);
    datum_handle = data_next_index(contrail_data, datum_handle);
  }
}
