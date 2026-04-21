/* Structure BSP rendering subsystem init/dispose. */

int cluster_partition_iter_next(void *partition, int *state)
{
  if (*state != -1) {
    char *cluster_reference =
      datum_get(*(void **)((char *)partition + 4), *state);
    *state = *(int *)(cluster_reference + 8);
    return *(int *)(cluster_reference + 4);
  }

  return -1;
}

int cluster_partition_iter_first(void *partition, int *state,
                                 int16_t cluster_idx)
{
  if (cluster_idx < 0 ||
      cluster_idx >= *(int *)((char *)scenario_get() + 0x134)) {
    display_assert("cluster_index>=0 && "
                   "cluster_index<global_structure_bsp_get()->clusters.count",
                   "c:\\halo\\SOURCE\\structures\\cluster_partitions.c", 0xd5,
                   true);
    system_exit(-1);
  }

  *state = *(int *)(*(int *)partition + cluster_idx * 4);
  if (*state != -1) {
    char *cluster_reference =
      datum_get(*(void **)((char *)partition + 4), *state);
    *state = *(int *)(cluster_reference + 8);
    return *(int *)(cluster_reference + 4);
  }

  return -1;
}

void structures_initialize(void)
{
  ((void (*)(void))0x193970)();
  ((void (*)(void))0x196290)();
}

void structures_initialize_for_new_map(void)
{
  ((void (*)(void))0x193bb0)();
  ((void (*)(void))0x1962d0)();
}

void structures_dispose_from_old_map(void)
{
  ((void (*)(void))0x1963a0)();
}

void structures_dispose(void)
{
  ((void (*)(void))0x1963b0)();
}

int16_t structure_find_in_cluster(uint16_t cluster_count, float *position,
                                  float radius, int max_count,
                                  int16_t *intersected_indices)
{
  if (position == NULL) {
    display_assert("position", "c:\\halo\\SOURCE\\structures\\structures.c",
                   0x86, true);
    system_exit(-1);
  }

  if (radius < 0.f) {
    display_assert("radius>=0.f", "c:\\halo\\SOURCE\\structures\\structures.c",
                   0x87, true);
    system_exit(-1);
  }

  if ((int16_t)max_count <= 0) {
    display_assert("maximum_count>0",
                   "c:\\halo\\SOURCE\\structures\\structures.c", 0x88, true);
    system_exit(-1);
  }

  if (intersected_indices == NULL) {
    display_assert("intersected_indices",
                   "c:\\halo\\SOURCE\\structures\\structures.c", 0x89, true);
    system_exit(-1);
  }

  if ((int16_t)cluster_count != -1) {
    if (radius > 0.f) {
      int16_t cluster_count_out;

      ((void (*)(void))0x198400)();
      cluster_count_out =
        ((int16_t(*)(uint16_t, float *, float, int, int16_t *))0x1989b0)(
          cluster_count, position, radius, max_count, intersected_indices);

      if (*(uint8_t *)0x4d92e1 == 0) {
        display_assert("structure_globals.cluster_marker_initialized",
                       "c:\\halo\\SOURCE\\structures\\structures.c", 0x130,
                       true);
        system_exit(-1);
      }

      *(uint8_t *)0x4d92e1 = 0;
      return cluster_count_out;
    }

    if ((int16_t)max_count > 0) {
      *intersected_indices = (int16_t)cluster_count;
      return 1;
    }
  }

  return 0;
}
