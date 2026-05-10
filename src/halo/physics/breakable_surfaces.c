/* Local forward declarations for functions not yet in decl.h */
/* FUN_0018e3f0 = global_collision_bsp_get, declared in decl.h */
/* FUN_00106200: declared in generated decl.h via kb.json */

/* floor/ceil: the original calls MSVC CRT floor/ceil (0x1dbc26/0x1d9c2b).
 * We provide simple implementations since we don't link the CRT math lib. */
static double breakable_floor(double x)
{
  int i = (int)x;
  return (double)((x < (double)i) ? (i - 1) : i);
}

static double breakable_ceil(double x)
{
  int i = (int)x;
  return (double)((x > (double)i) ? (i + 1) : i);
}

/* 0x1457f0 — Returns a pointer to the health float for a breakable surface,
 * indexed by global_structure_bsp_index and surface_index within the globals
 * buffer starting at offset 0x204. */
float *breakable_surface_get(short surface_index)
{
  assert_halt(breakable_surface_globals);
  assert_halt(global_structure_bsp_index >= 0 &&
              global_structure_bsp_index < 16);
  assert_halt(surface_index >= 0 && surface_index < 256);
  return (
    float *)(breakable_surface_globals + 0x204 +
             ((int)global_structure_bsp_index * 256 + (int)surface_index) * 4);
}

void breakable_surfaces_initialize(void)
{
  assert_halt(!breakable_surface_globals);
  breakable_surface_globals =
    (char *)game_state_malloc("breakable surface globals", 0, 0x4204);
}

void breakable_surfaces_dispose(void)
{
}

void breakable_surfaces_initialize_for_new_map(void)
{
  int i;
  int j;
  uint32_t *floats;

  assert_halt(breakable_surface_globals);

  *breakable_surface_globals = 1;
  for (i = 0; i < 16; i++) {
    csmemset(breakable_surface_globals + 1 + i * 0x20, 0xFF, 0x20);
    floats = (uint32_t *)(breakable_surface_globals + 0x204 + i * 0x400);
    for (j = 0; j < 0x100; j++)
      floats[j] = 0x3f800000;
  }
}

void breakable_surfaces_dispose_from_old_map(void)
{
}

/* 0x1459e0 — breakable_surfaces_get_bsp_surface_data
 *
 * Returns a pointer to the 32-byte bitfield array for the current structure
 * BSP within the breakable_surface_globals buffer. The layout of the buffer
 * is: byte[0] = flags, then 16 × 32-byte entries (one per BSP) starting at
 * offset 1.
 *
 * Asserts that breakable_surface_globals is initialised and that
 * global_structure_bsp_index is in [0, MAXIMUM_STRUCTURE_BSPS_PER_SCENARIO).
 *
 * Confirmed: MOV EAX,[0x46f08c] TEST/JNZ assert pattern at 0x1459e0.
 * Confirmed: MOV AX,[0x326a0c] — int16_t global_structure_bsp_index.
 * Confirmed: assert "global_structure_bsp_index>=0 &&
 *   global_structure_bsp_index<MAXIMUM_STRUCTURE_BSPS_PER_SCENARIO" at line
 *   0x8b (139), __FILE__ "c:\halo\SOURCE\physics\breakable_surfaces.c".
 * Confirmed: LEA EAX,[EAX + ECX*1 + 0x1] — ptr = globals + bsp_index*32 + 1.
 */
char *breakable_surfaces_get_bsp_surface_data(void)
{
  assert_halt(breakable_surface_globals);
  assert_halt(global_structure_bsp_index >= 0 &&
              global_structure_bsp_index < 16);
  return breakable_surface_globals + 1 + (int)global_structure_bsp_index * 32;
}

/* 0x145ad0 — Breakable surface destruction effect spawner.
 *
 * When a breakable surface is destroyed, this function spawns debris particles
 * across the surface area. It performs a BFS traversal of connected breakable
 * surfaces sharing the same material, collecting vertices and computing a 2D
 * bounding box in the plane's local coordinate system. For each material
 * effect entry, it subdivides the surface into a grid and spawns particles
 * at each grid cell that passes a point-in-polygon test. Particle velocity
 * is computed from the damage source direction, surface normal impulse, and
 * random perturbation. Sound effects are also triggered at the surface center.
 *
 * Confirmed: _chkstk with 0x1240 bytes stack frame.
 * Confirmed: scenario_get at 0x145adf, FUN_0018e3f0 (global_collision_bsp) at 0x145ae6.
 * Confirmed: assert param_2!=NULL via display_assert at 0x145b05.
 * Confirmed: DAT_00324c32 early-out flag check at 0x145b14.
 * Confirmed: BFS queue in local_1244[1024], traversal count local_b4.
 * Confirmed: tag_block_get_element calls for planes, edges, vertices, effects.
 * Confirmed: FUN_00061df0 projects 3D to 2D, FUN_000992d0 unprojects 2D to 3D.
 * Confirmed: FUN_00106200 point-in-polygon test with 0 radius.
 * Confirmed: particle_new spawns a particle.
 * Confirmed: unattached_impulse_sound_new triggers sound at end if material has sound tag. */
void FUN_00145ad0(unsigned short param_1, void *damage_params, int param_3)
{
  uint32_t *param_2;
  int scenario;
  int collision_bsp;
  int surface_element;
  char *material_data;
  int material_effects_base;
  char *effect_entry;
  char *jpt_tag;
  int jpt_offset;

  float plane[4];
  float edge_dir[3];
  float cross_dir[3];
  float vertices[24]; /* 8 vertices x 3 floats */
  char projected_verts[64]; /* 8 vertices x 8 bytes (2D) */
  char projected_point[8]; /* 2D point for polygon test */

  float origin[3];
  float adjusted_pos[3];
  float delta[3];
  float velocity[3];

  float dot_edge, dot_cross;
  float min_u, max_u, min_v, max_v;
  float bbox_min[3], bbox_max[3];

  int projection;
  uint8_t sign;
  uint32_t surface_data_0, surface_data_1;
  uint32_t *surface_ptr;
  float *vert_ptr;
  float *vert_other;
  int next_surface;
  int current_edge;
  uint8_t is_reversed;
  short edge_count;
  short search_i;
  static int bfs_queue[1024]; /* static to avoid _chkstk */
  int bfs_count;
  int bfs_head;
  int current_surface;

  float dist, inv_dist;
  float t, scale;
  float effect_size;

  short grid_x0, grid_y0, grid_x1, grid_y1;
  int grid_row;
  uint16_t grid_width, grid_height;

  float *zero_vec;

  char spawn_params[0x60];
  float *sp_velocity_dir;

  unsigned int *seed;
  float rand_val;

  bool have_bbox;

  param_2 = (uint32_t *)damage_params;

  /* _chkstk for 0x1240 bytes */
  scenario = (int)scenario_get();
  collision_bsp = (int)global_collision_bsp_get();

  if (param_2 == NULL) {
    display_assert("param_2", "c:\\halo\\SOURCE\\physics\\breakable_surfaces.c",
                   0xfb, 1);
    system_exit(-1);
  }

  if (*(char *)0x324c32 == 0)
    return;

  /* Get the breakable surface entry from the collision BSP */
  surface_element =
    (int)tag_block_get_element((void *)(collision_bsp + 0x3c), param_3, 0xc);

  /* Get material data for the surface's material type */
  material_data = (char *)tag_block_get_element(
    (void *)(scenario + 0xa4),
    (int)*(short *)(surface_element + 10), 0x14);

  /* Get the game globals structure for this material */
  {
    int gg = (int)game_globals_get();
    material_effects_base =
      (int)tag_block_get_element((void *)(gg + 0x194),
                                 (int)*(short *)(material_data + 0x12), 0x374) +
      0x2d4;
  }

  /* Assert surface belongs to our BSP */
  have_bbox = false;
  bfs_head = 0;
  if (*(uint8_t *)(surface_element + 9) != (uint8_t)param_1) {
    display_assert(
      "surface->breakable_surface==breakable_surface_index",
      "c:\\halo\\SOURCE\\physics\\breakable_surfaces.c", 0x10a, 1);
    system_exit(-1);
  }

  /* Initialize BFS with the starting surface */
  bfs_queue[0] = param_3;
  bfs_count = 1;

  /* BFS loop over connected breakable surfaces */
  do {
    /* Dequeue next surface */
    current_surface = bfs_queue[(short)bfs_head];
    bfs_head = bfs_head + 1;

    /* Get surface data (two uint32s: packed plane ref + first edge) */
    surface_ptr =
      (uint32_t *)tag_block_get_element((void *)(collision_bsp + 0x3c),
                                        current_surface, 0xc);
    surface_data_0 = surface_ptr[0];
    surface_data_1 = surface_ptr[1];

    /* Get plane equation (strip sign bit for plane index) */
    vert_ptr = (float *)tag_block_get_element(
      (void *)(collision_bsp + 0xc),
      (int)(surface_data_0 & 0x7fffffff), 0x10);

    /* Copy plane, negating if surface references back side */
    if ((int)surface_data_0 < 0) {
      plane[0] = -vert_ptr[0];
      plane[1] = -vert_ptr[1];
      plane[2] = -vert_ptr[2];
      plane[3] = -vert_ptr[3];
    } else {
      plane[0] = vert_ptr[0];
      plane[1] = vert_ptr[1];
      plane[2] = vert_ptr[2];
      plane[3] = vert_ptr[3];
    }

    /* Determine best projection axis (largest absolute component of normal) */
    {
      float ax = xbox_fabsf(plane[0]);
      float ay = xbox_fabsf(plane[1]);
      float az = xbox_fabsf(plane[2]);

      if (az < ay || az < ax) {
        if (ay < ax)
          projection = 0;
        else
          projection = 1;
      } else {
        projection = 2;
      }
    }

    /* Determine sign for projection axis remapping */
    sign = 1;
    if (plane[(short)projection] <= 0.0f)
      sign = 0;

    /* Walk edges of this surface, collecting vertices */
    edge_count = 0;
    current_edge = surface_data_1;

    do {
      char *edge_data;

      /* Get edge element */
      edge_data = (char *)tag_block_get_element(
        (void *)(collision_bsp + 0x48), current_edge, 0x18);

      /* Check which side of the edge this surface is on */
      is_reversed = (*(int *)(edge_data + 0x14) == current_surface) ? 1 : 0;

      /* Get vertex for this edge (left or right vertex depending on side) */
      vert_ptr = (float *)tag_block_get_element(
        (void *)(collision_bsp + 0x54),
        *(int *)(edge_data + (uint32_t)(!is_reversed) * 4), 0x10);

      /* Get the next surface index for neighbor detection */
      next_surface = *(int *)(edge_data + 0x10 + (uint32_t)(!is_reversed) * 4);

      if (edge_count == 0) {
        /* First edge — set up coordinate frame */
        if (current_surface == param_3) {
          /* For the initial surface, project the damage origin to find
           * our local reference point */
          FUN_00061df0(param_2 + 10, (uint32_t)projection, sign,
                       projected_point);
          FUN_000992d0((float *)projected_point, plane, (short)projection,
                       sign, origin);
        } else {
          origin[0] = vert_ptr[0];
          origin[1] = vert_ptr[1];
          origin[2] = vert_ptr[2];
        }

        /* Get the other vertex of this edge to compute edge direction */
        vert_other = (float *)tag_block_get_element(
          (void *)(collision_bsp + 0x54),
          *(int *)(edge_data + (uint32_t)is_reversed * 4), 0x10);

        /* Compute edge direction vector */
        edge_dir[0] = vert_other[0] - vert_ptr[0];
        edge_dir[1] = vert_other[1] - vert_ptr[1];
        edge_dir[2] = vert_other[2] - vert_ptr[2];

        /* Normalize edge direction */
        dist = xbox_sqrtf(edge_dir[0] * edge_dir[0] +
                          edge_dir[1] * edge_dir[1] +
                          edge_dir[2] * edge_dir[2]);
        if (xbox_fabsf(dist) >= (float)*(double *)0x2533d0) {
          inv_dist = 1.0f / dist;
          edge_dir[0] = edge_dir[0] * inv_dist;
          edge_dir[1] = edge_dir[1] * inv_dist;
          edge_dir[2] = edge_dir[2] * inv_dist;
        }

        /* Compute cross product: cross_dir = edge_dir x plane_normal */
        cross_dir[0] =
          edge_dir[1] * plane[2] - edge_dir[2] * plane[1];
        cross_dir[1] =
          edge_dir[2] * plane[0] - plane[2] * edge_dir[0];
        cross_dir[2] =
          plane[1] * edge_dir[0] - edge_dir[1] * plane[0];

        /* Compute reference dot products */
        dot_edge = origin[0] * edge_dir[0] +
                   origin[1] * edge_dir[1] +
                   origin[2] * edge_dir[2];
        dot_cross = origin[0] * cross_dir[0] +
                    origin[1] * cross_dir[1] +
                    origin[2] * cross_dir[2];

        /* Initialize bounding box in local 2D */
        min_u = (edge_dir[0] * vert_ptr[0] +
                 edge_dir[1] * vert_ptr[1] +
                 edge_dir[2] * vert_ptr[2]) -
                dot_edge;
        min_v = (cross_dir[0] * vert_ptr[0] +
                 cross_dir[1] * vert_ptr[1] +
                 cross_dir[2] * vert_ptr[2]) -
                dot_cross;

        max_u = min_u;
        max_v = min_v;
      } else {
        /* Subsequent edges — extend bounding box */
        float u_val, v_val;

        u_val = (edge_dir[0] * vert_ptr[0] +
                 edge_dir[1] * vert_ptr[1] +
                 edge_dir[2] * vert_ptr[2]) -
                dot_edge;
        v_val = (cross_dir[0] * vert_ptr[0] +
                 cross_dir[1] * vert_ptr[1] +
                 cross_dir[2] * vert_ptr[2]) -
                dot_cross;

        if (u_val <= min_u)
          min_u = u_val;
        if (v_val <= min_v)
          min_v = v_val;
        if (u_val > max_u)
          max_u = u_val;
        if (v_val > max_v)
          max_v = v_val;

        if (edge_count > 7) {
          display_assert("edge_count<MAXIMUM_VERTICES_PER_SURFACE",
                         "c:\\halo\\SOURCE\\physics\\breakable_surfaces.c",
                         0x15c, 1);
          system_exit(-1);
        }
      }

      /* Store this vertex in the vertex array */
      {
        int vi = (int)edge_count;
        vertices[vi * 3] = vert_ptr[0];
        vertices[vi * 3 + 1] = vert_ptr[1];
        vertices[vi * 3 + 2] = vert_ptr[2];

        /* Project vertex to 2D */
        FUN_00061df0(vert_ptr, (uint32_t)projection, sign,
                     projected_verts + vi * 8);
      }

      /* Update 3D bounding box */
      if (have_bbox) {
        if (vert_ptr[0] <= bbox_min[0])
          bbox_min[0] = vert_ptr[0];
        if (vert_ptr[1] <= bbox_min[1])
          bbox_min[1] = vert_ptr[1];
        if (vert_ptr[2] <= bbox_min[2])
          bbox_min[2] = vert_ptr[2];
        if (vert_ptr[0] > bbox_max[0])
          bbox_max[0] = vert_ptr[0];
        if (vert_ptr[1] > bbox_max[1])
          bbox_max[1] = vert_ptr[1];
        if (vert_ptr[2] > bbox_max[2])
          bbox_max[2] = vert_ptr[2];
      } else {
        bbox_min[0] = vert_ptr[0];
        bbox_max[0] = vert_ptr[0];
        bbox_max[1] = vert_ptr[1];
        bbox_min[1] = vert_ptr[1];
        bbox_max[2] = vert_ptr[2];
        bbox_min[2] = vert_ptr[2];
        have_bbox = true;
      }

      /* Check if the neighbor surface should be added to the BFS queue */
      search_i = 0;
      while (next_surface != -1) {
        short bfs_count_s = (short)bfs_count;

        if (search_i >= bfs_count_s) {
          /* Not already in queue — check if it's the same breakable surface */
          if (next_surface != -1) {
            char *neighbor;
            neighbor = (char *)tag_block_get_element(
              (void *)(collision_bsp + 0x3c), next_surface, 0xc);
            if (*(uint8_t *)(neighbor + 9) == (uint8_t)param_1 &&
                *(short *)(neighbor + 0xa) ==
                    *(short *)(surface_element + 0xa)) {
              if (bfs_count_s >= 0x400) {
                display_assert("surface_count<MAXIMUM_BREAKABLE_SURFACE_SURFACES",
                               "c:\\halo\\SOURCE\\physics\\breakable_surfaces.c",
                               0x184, 1);
                system_exit(-1);
              }
              bfs_queue[bfs_count_s] = next_surface;
              bfs_count = bfs_count + 1;
            }
          }
          break;
        }

        /* Check if this surface is already in the queue */
        if (bfs_queue[search_i] == next_surface)
          next_surface = -1;
        search_i = search_i + 1;
      }

      /* Advance to next edge */
      current_edge =
        *(int *)(edge_data + 8 + (uint32_t)is_reversed * 4);
      edge_count = edge_count + 1;
    } while ((uint32_t)current_edge != surface_ptr[1]);

    /* Iterate over material effects for this surface */
    {
      int effect_idx_s;
      int effect_count;

      effect_idx_s = 0;
      effect_count = *(int *)(material_effects_base + 0x48);

      if (effect_count > 0) {
        int effect_idx = 0;

        do {
          effect_entry = (char *)tag_block_get_element(
            (void *)(material_effects_base + 0x48), effect_idx, 0x80);

          if (*(int *)(effect_entry + 0xc) != -1) {
            effect_size = *(float *)(effect_entry + 0x14);

            if (effect_size == 0.0f) {
              /* Zero-size effect: spawn single particle at center only for
               * the initial surface */
              if (current_surface != param_3)
                goto next_effect;

              grid_x0 = 0;
              grid_y0 = 0;
              grid_x1 = 0;
              grid_y1 = 0;
              grid_row = 0;
              goto spawn_loop;
            }

            /* Compute grid extents from bounding box and effect size */
            {
              float val;

              /* grid_y0 = floor(min_u / effect_size) */
              val = min_u / effect_size;
              if (val < -1000.0f)
                val = -1000.0f;
              else if (val > 1000.0f)
                val = 1000.0f;
              grid_y0 = (short)(int)breakable_floor((double)val);

              /* grid_x0 = floor(min_v / effect_size) */
              val = min_v / effect_size;
              if (val < -1000.0f)
                val = -1000.0f;
              else if (val > 1000.0f)
                val = 1000.0f;
              grid_x0 = (short)(int)breakable_floor((double)val);

              /* grid_y1 = ceil(max_u / effect_size) */
              val = max_u / effect_size;
              if (val < -1000.0f)
                val = -1000.0f;
              else if (val > 1000.0f)
                val = 1000.0f;
              grid_y1 = (short)(int)breakable_ceil((double)val);

              /* grid_x1 = ceil(max_v / effect_size) */
              val = max_v / effect_size;
              if (val < -1000.0f)
                val = -1000.0f;
              else if (val > 1000.0f)
                val = 1000.0f;
              grid_x1 = (short)(int)breakable_ceil((double)val);

              if (grid_x0 > grid_x1)
                goto next_effect;
            }

            grid_row = (int)grid_x0;

spawn_loop:
            /* Outer loop: rows (u-axis) */
            grid_width =
              (uint16_t)((int)grid_x1 - (int)grid_x0 + 1);

            do {
              if (grid_y0 <= grid_y1) {
                int col_iter;
                float row_f;

                row_f = (float)grid_row;
                col_iter = (int)grid_y0;
                grid_height =
                  (uint16_t)((int)grid_y1 - (int)grid_y0 + 1);

                do {
                  float jitter_u, jitter_v;
                  float u_coord, v_coord;

                  /* Get RNG seed and generate jitter */
                  seed = random_math_get_local_seed_address();
                  jitter_u = random_real_range((int *)seed, -0.75f, 0.75f);

                  seed = random_math_get_local_seed_address();
                  jitter_v = random_real_range((int *)seed, -0.75f, 0.75f);

                  /* Compute world-space position from grid coordinates */
                  u_coord =
                    ((float)col_iter + jitter_u) * effect_size;
                  v_coord = (row_f + jitter_v) * effect_size;

                  adjusted_pos[0] = origin[0];
                  adjusted_pos[1] = origin[1];
                  adjusted_pos[2] = origin[2];

                  /* adjusted_pos += edge_dir * u_coord */
                  adjusted_pos[0] =
                    edge_dir[0] * u_coord + adjusted_pos[0];
                  adjusted_pos[1] =
                    edge_dir[1] * u_coord + adjusted_pos[1];
                  adjusted_pos[2] =
                    edge_dir[2] * u_coord + adjusted_pos[2];

                  /* adjusted_pos += cross_dir * v_coord */
                  adjusted_pos[0] =
                    cross_dir[0] * v_coord + adjusted_pos[0];
                  adjusted_pos[1] =
                    cross_dir[1] * v_coord + adjusted_pos[1];
                  adjusted_pos[2] =
                    cross_dir[2] * v_coord + adjusted_pos[2];

                  /* Project adjusted position to 2D and test if inside polygon */
                  FUN_00061df0(adjusted_pos, (uint32_t)projection,
                               sign, projected_point);

                  if (!FUN_00106200(edge_count, projected_verts,
                                   (float *)projected_point, 0.0f))
                    goto next_cell;

                  /* === Spawn particle === */
                  {
                    /* Initialize velocity from global zero vector */
                    zero_vec = *(float **)0x31fc38;
                    velocity[0] = zero_vec[0];
                    velocity[1] = zero_vec[1];
                    velocity[2] = zero_vec[2];

                    /* Get the damage effect tag (jpt!) */
                    jpt_tag = (char *)tag_get(0x6a707421, param_2[0]);
                    jpt_offset = (int)jpt_tag + 0x194;

                    /* Compute delta from damage origin to this point */
                    delta[0] = adjusted_pos[0] - *(float *)(param_2 + 10);
                    delta[1] = adjusted_pos[1] - *(float *)(param_2 + 11);
                    delta[2] = adjusted_pos[2] - *(float *)(param_2 + 12);

                    /* Compute distance */
                    dist = xbox_sqrtf(delta[0] * delta[0] +
                                      delta[1] * delta[1] +
                                      delta[2] * delta[2]);

                    /* Normalize delta */
                    if (xbox_fabsf(dist) < (float)*(double *)0x2533d0) {
                      dist = 0.0f;
                    } else {
                      inv_dist = 1.0f / dist;
                      delta[0] = delta[0] * inv_dist;
                      delta[1] = delta[1] * inv_dist;
                      delta[2] = delta[2] * inv_dist;
                    }

                    /* Apply impulse from damage effect (radial component) */
                    if (0.0f < *(float *)(jpt_offset + 0x1c)) {
                      /* t = clamp(1 - dist/range, 0, 1) */
                      t = 1.0f - dist / *(float *)(jpt_offset + 0x1c);
                      if (t < 0.0f)
                        t = 0.0f;
                      else if (t > 1.0f)
                        t = 1.0f;

                      /* Apply power curve if exponent != 0 */
                      if (*(float *)(jpt_offset + 0x20) != 0.0f)
                        t = (float)xbox_pow((double)t,
                                            (double)*(float *)(jpt_offset + 0x20));

                      /* Scale by magnitude */
                      scale = t * *(float *)(jpt_offset + 0x18);

                      /* velocity += delta * scale */
                      velocity[0] =
                        delta[0] * scale + velocity[0];
                      velocity[1] =
                        delta[1] * scale + velocity[1];
                      velocity[2] =
                        delta[2] * scale + velocity[2];
                    }

                    /* Apply impulse from damage effect (directional component) */
                    if (0.0f < *(float *)(jpt_offset + 0x4)) {
                      /* t = clamp(1 - dist/range, 0, 1) */
                      t = 1.0f - dist / *(float *)(jpt_offset + 0x4);
                      if (t < 0.0f)
                        t = 0.0f;
                      else if (t > 1.0f)
                        t = 1.0f;

                      /* Apply power curve if exponent != 0 */
                      if (*(float *)(jpt_offset + 0x8) != 0.0f)
                        t = (float)xbox_pow((double)t,
                                            (double)*(float *)(jpt_offset + 0x8));

                      /* Scale by magnitude */
                      scale = t * *(float *)(jpt_offset + 0x0);

                      /* velocity += damage_direction * scale */
                      velocity[0] =
                        scale * *(float *)(param_2 + 13) + velocity[0];
                      velocity[1] =
                        scale * *(float *)(param_2 + 14) + velocity[1];
                      velocity[2] =
                        scale * *(float *)(param_2 + 15) + velocity[2];
                    }

                    /* Apply effect-specific velocity scale */
                    if (0.0f < *(float *)(effect_entry + 0x1c)) {
                      float lo, hi;
                      lo = *(float *)(effect_entry + 0x18);
                      hi = *(float *)(effect_entry + 0x1c);
                      seed = random_math_get_local_seed_address();
                      rand_val =
                        random_real_range((int *)seed, lo, hi);
                      velocity[0] = velocity[0] * rand_val;
                      velocity[1] = velocity[1] * rand_val;
                      velocity[2] = velocity[2] * rand_val;
                    }

                    /* Build spawn_params struct */
                    *(int *)(spawn_params + 0x00) =
                      *(int *)(effect_entry + 0xc); /* particle tag */
                    *(int *)(spawn_params + 0x04) = -1; /* no parent */
                    *(short *)(spawn_params + 0x08) = -1;
                    *(short *)(spawn_params + 0x0a) = -1;
                    *(spawn_params + 0x0c) = 0;
                    *(spawn_params + 0x0d) = 0;
                    *(spawn_params + 0x0e) = 0;

                    /* position */
                    *(float *)(spawn_params + 0x10) = adjusted_pos[0];
                    *(float *)(spawn_params + 0x14) = adjusted_pos[1];
                    *(float *)(spawn_params + 0x18) = adjusted_pos[2];

                    /* velocity direction (normalized later) */
                    sp_velocity_dir = (float *)(spawn_params + 0x1c);
                    sp_velocity_dir[0] = velocity[0];
                    sp_velocity_dir[1] = velocity[1];
                    sp_velocity_dir[2] = velocity[2];

                    /* velocity (same as direction initially) */
                    *(float *)(spawn_params + 0x28) = velocity[0];
                    *(float *)(spawn_params + 0x2c) = velocity[1];
                    *(float *)(spawn_params + 0x30) = velocity[2];

                    /* origin (zero vector) */
                    {
                      float *zv = *(float **)0x31fc38;
                      *(float *)(spawn_params + 0x34) = zv[0];
                      *(float *)(spawn_params + 0x38) = zv[1];
                      *(float *)(spawn_params + 0x3c) = zv[2];
                    }

                    /* Random angle in [0, pi] */
                    seed = random_math_get_local_seed_address();
                    *(float *)(spawn_params + 0x40) =
                      random_real_range((int *)seed, 0.0f, 3.14159274f);

                    /* Random scale from effect bounds (offset 0x24..0x28) */
                    {
                      float lo2 = *(float *)(effect_entry + 0x24);
                      float hi2 = *(float *)(effect_entry + 0x28);
                      seed = random_math_get_local_seed_address();
                      *(float *)(spawn_params + 0x44) =
                        random_real_range((int *)seed, lo2, hi2);
                    }

                    /* Random scale from effect bounds (offset 0x34..0x38) */
                    {
                      float lo3 = *(float *)(effect_entry + 0x34);
                      float hi3 = *(float *)(effect_entry + 0x38);
                      seed = random_math_get_local_seed_address();
                      *(float *)(spawn_params + 0x48) =
                        random_real_range((int *)seed, lo3, hi3);
                    }

                    /* Color alpha — computed from effect distance curve */
                    {
                      seed = random_math_get_local_seed_address();
                      rand_val = random_math_real(seed);
                      *(float *)(spawn_params + 0x4c) = rand_val;

                      seed = random_math_get_local_seed_address();
                      rand_val = random_math_real(seed);

                      if ((*(float *)(effect_entry + 0x54) -
                           *(float *)(effect_entry + 0x44)) *
                              rand_val +
                          *(float *)(effect_entry + 0x44) <
                          0.0f) {
                        *(float *)(spawn_params + 0x4c) = 0.0f;
                      } else {
                        seed = random_math_get_local_seed_address();
                        rand_val = random_math_real(seed);
                        if ((*(float *)(effect_entry + 0x54) -
                             *(float *)(effect_entry + 0x44)) *
                                rand_val +
                            *(float *)(effect_entry + 0x44) <=
                            1.0f) {
                          seed = random_math_get_local_seed_address();
                          rand_val = random_math_real(seed);
                          *(float *)(spawn_params + 0x4c) =
                            (*(float *)(effect_entry + 0x54) -
                             *(float *)(effect_entry + 0x44)) *
                                rand_val +
                            *(float *)(effect_entry + 0x44);
                        } else {
                          *(float *)(spawn_params + 0x4c) = 1.0f;
                        }
                      }
                    }

                    /* Color RGB from effect color bounds */
                    FUN_0007c270((float *)(spawn_params + 0x50),
                                 *(uint32_t *)(effect_entry + 0x10) & 3,
                                 (float *)(effect_entry + 0x48),
                                 (float *)(effect_entry + 0x58),
                                 *(float *)(spawn_params + 0x4c));

                    /* Normalize velocity direction */
                    {
                      float vel_len;
                      vel_len = xbox_sqrtf(
                        sp_velocity_dir[0] * sp_velocity_dir[0] +
                        sp_velocity_dir[1] * sp_velocity_dir[1] +
                        sp_velocity_dir[2] * sp_velocity_dir[2]);
                      if (xbox_fabsf(vel_len) <
                          (float)*(double *)0x2533d0) {
                        /* Zero velocity — assign random direction */
                        random_seed_get_direction3d(
                          random_math_get_local_seed_address(),
                          sp_velocity_dir);
                      } else {
                        inv_dist = 1.0f / vel_len;
                        sp_velocity_dir[0] *= inv_dist;
                        sp_velocity_dir[1] *= inv_dist;
                        sp_velocity_dir[2] *= inv_dist;
                        if (vel_len == 0.0f) {
                          /* Edge case: exact zero after normalize */
                          random_seed_get_direction3d(
                            random_math_get_local_seed_address(),
                            sp_velocity_dir);
                        }
                      }
                    }

                    /* Spawn the particle */
                    particle_new(spawn_params);
                  }

next_cell:
                  grid_height = grid_height - 1;
                  col_iter = col_iter + 1;
                } while (grid_height != 0);
              }

              grid_width = grid_width - 1;
              grid_row = grid_row + 1;
            } while (grid_width != 0);
          }

next_effect:
          effect_idx_s = effect_idx_s + 1;
          effect_idx = effect_idx_s;
        } while ((short)effect_idx_s < effect_count);
      }
    }
  } while ((short)bfs_head < (short)bfs_count);

  /* Play destruction sound if material has a sound tag */
  if (*(int *)(material_effects_base + 0x2c) != -1 && have_bbox) {
    char sound_location[0x2c];
    float *fwd_vec;

    /* position = center of bounding box */
    *(float *)(sound_location + 0x00) =
      (bbox_max[0] + bbox_min[0]) * 0.5f;
    *(float *)(sound_location + 0x04) =
      (bbox_max[1] + bbox_min[1]) * 0.5f;
    *(float *)(sound_location + 0x08) =
      (bbox_max[2] + bbox_min[2]) * 0.5f;

    /* forward vector */
    fwd_vec = *(float **)0x31fc3c;
    *(float *)(sound_location + 0x0c) = fwd_vec[0];
    *(float *)(sound_location + 0x10) = fwd_vec[1];
    *(float *)(sound_location + 0x14) = fwd_vec[2];

    /* up vector (zero) */
    zero_vec = *(float **)0x31fc38;
    *(float *)(sound_location + 0x18) = zero_vec[0];
    *(float *)(sound_location + 0x1c) = zero_vec[1];
    *(float *)(sound_location + 0x20) = zero_vec[2];

    /* cluster/leaf from damage params */
    *(uint32_t *)(sound_location + 0x24) = param_2[5];
    *(uint32_t *)(sound_location + 0x28) = param_2[6];

    unattached_impulse_sound_new(*(int *)(material_effects_base + 0x2c),
                 sound_location, 1.0f);
  }
}

/* 0x146a90 — Apply breakable-surface damage: reduces surface health based on
 * the damage effect tag's range/modifier and material vitality. Clears the
 * surface bit and fires the destruction callback if health reaches zero. */
void FUN_00146a90(int surface_id, void *damage_params, int unknown)
{
  float *health_ptr;
  char *material_data;
  char *jpt_tag;
  float damage;
  float new_health;
  char *bsp_data;
  int surface_idx;
  uint32_t *word_ptr;
  int material_type;

  assert_halt(breakable_surface_globals);
  if (*breakable_surface_globals == 0)
    return;
  if ((short)surface_id == -1)
    return;
  if (*(int *)damage_params == -1)
    return;
  if (*(short *)((char *)damage_params + 0x4c) == -1)
    return;

  health_ptr = breakable_surface_get((short)surface_id);
  if (*health_ptr <= 0.0f)
    return;

  material_type = (int)*(short *)((char *)damage_params + 0x4c);
  material_data = (char *)FUN_0018e500((short)material_type);
  if (material_data == NULL)
    return;
  if (*(float *)(material_data + 0x2d4) <= 0.0f)
    return;

  jpt_tag = (char *)tag_get(0x6a707421, *(int *)damage_params);

  damage =
    FUN_000121e0(*(float *)(jpt_tag + 0x1d4), *(float *)(jpt_tag + 0x1d8));
  damage = (damage - *(float *)(jpt_tag + 0x1d0)) *
             *(float *)((char *)damage_params + 0x40) +
           *(float *)(jpt_tag + 0x1d0);
  damage = damage * *(float *)(jpt_tag + 0x200 + material_type * 4) /
           *(float *)(material_data + 0x2d4);
  new_health = *health_ptr - damage;
  *health_ptr = new_health;

  if (new_health <= 0.0f) {
    bsp_data = breakable_surfaces_get_bsp_surface_data();
    surface_idx = (int)(short)surface_id;
    word_ptr = (uint32_t *)(bsp_data + (surface_idx >> 5) * 4);
    *word_ptr = *word_ptr & ~(1 << (surface_idx & 0x1f));
    FUN_00145ad0((unsigned short)surface_id, damage_params, unknown);
  }
}
