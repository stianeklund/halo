#include "x87_math.h"

/* 0xa3e60 */
void *FUN_000a3e60(int16_t local_player_index /* @<esi> */)
{
  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\effects\\weather_particle_systems.c",
                     0x5b,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  return (char *)0x4557f4 + (int)local_player_index * 0x9c;
}

/* 0xa3ea0 */
void *FUN_000a3ea0(void *weather_particle_system /* @<edi> */,
                   int16_t type_index /* @<esi> */)
{
  void *definition;

  definition = tag_get(0x7261696e, *(int *)weather_particle_system);
  assert_halt_msg_at(
    "type_index>=0 && type_index<definition->particle_types.count",
    "c:\\halo\\SOURCE\\effects\\weather_particle_systems.c", 0x66,
    type_index >= 0 && (int)type_index < *(int *)((char *)definition + 0x24));

  return (char *)weather_particle_system + (int)type_index * 0x10 + 0x1c;
}

void weather_particle_systems_initialize(void)
{
  weather_particle_system_data = data_new("weather particles", 0x200, 0x54);
  if (weather_particle_system_data == 0)
    error(0, "couldn't allocate weather particle system globals.");
}

void weather_particle_systems_initialize_for_new_map(void)
{
  int16_t i;
  int *entry;

  data_t *arr;

  i = 0;
  entry = (int *)0x4557f4;
  do {
    assert_halt(i >= 0 && i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
    *entry = NONE;
    i++;
    entry = (int *)((char *)entry + 0x9c);
  } while (i < 4);
  arr = weather_particle_system_data;
  *(int16_t *)0x4557f0 = 0;
  data_delete_all(arr);
}

void weather_particle_systems_dispose_from_old_map(void)
{
  if (weather_particle_system_data->valid)
    data_make_invalid(weather_particle_system_data);
}

void weather_particle_systems_dispose(void)
{
  if (weather_particle_system_data) {
    data_dispose(weather_particle_system_data);
    weather_particle_system_data = 0;
  }
}

/* 0xa4000 */
/* Wraps a 3-component vector into [0, extent) per component: out[i] =
 * fmod(in[i], extent), biased by +extent when in[i] < 0.0f (positive-modulo
 * wrap, e.g. tiling a weather particle system's spawn extent around the
 * camera). FUN_001daf7e == _CIfmod / C fmod (x87 FPREM w/ C2-reduction
 * loop) -- see x87_fmod / lift-learnings for why this is NOT the FPREM1
 * xbox_fmod macro. */
void FUN_000a4000(float *vector_out /* @<edi> */, float *vector_in /* @<esi> */,
                  float extent)
{
  float wrap_bias;
#if defined(_MSC_VER) && !defined(__clang__)
  double __cdecl fmod(double, double);
#endif

  wrap_bias = (vector_in[0] < *(const float *)0x2533c0) ? extent : 0.0f;
#if defined(_MSC_VER) && !defined(__clang__)
  vector_out[0] = (float)fmod((double)vector_in[0], (double)extent) + wrap_bias;
#else
  vector_out[0] = x87_fmod(vector_in[0], (double)extent) + wrap_bias;
#endif

  wrap_bias = (vector_in[1] < *(const float *)0x2533c0) ? extent : 0.0f;
#if defined(_MSC_VER) && !defined(__clang__)
  vector_out[1] = (float)fmod((double)vector_in[1], (double)extent) + wrap_bias;
#else
  vector_out[1] = x87_fmod(vector_in[1], (double)extent) + wrap_bias;
#endif

  wrap_bias = (vector_in[2] < *(const float *)0x2533c0) ? extent : 0.0f;
#if defined(_MSC_VER) && !defined(__clang__)
  vector_out[2] = (float)fmod((double)vector_in[2], (double)extent) + wrap_bias;
#else
  vector_out[2] = x87_fmod(vector_in[2], (double)extent) + wrap_bias;
#endif
}

/* 0xa40a0 */
void FUN_000a40a0(int16_t local_player_index, int particle_system_tag_index,
                  float scale)
{
  char *weather_particle_system;
  void *definition;
  void *type_definition;
  void *type_element;
  char *particle_type;
  int16_t type_index;
  int type_index_int;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\effects\\weather_particle_systems.c",
                     0x5b,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  weather_particle_system = (char *)0x4557f4 + (int)local_player_index * 0x9c;
  definition = tag_get(0x7261696e, particle_system_tag_index);

  assert_halt_msg_at("system->definition_index==NONE",
                     "c:\\halo\\SOURCE\\effects\\weather_particle_systems.c",
                     0xb1, *(int *)weather_particle_system == -1);

  *(float *)(weather_particle_system + 0xc) = scale;
  *(int *)weather_particle_system = particle_system_tag_index;
  *(int *)(weather_particle_system + 4) = 0;
  *(int *)(weather_particle_system + 8) = 0;
  weather_particle_system_count++;

  type_index = 0;
  type_index_int = 0;

  if (*(int *)((char *)definition + 0x24) > 0) {
    do {
      type_definition = tag_get(0x7261696e, *(int *)weather_particle_system);
      assert_halt_msg_at(
        "type_index>=0 && type_index<definition->particle_types.count",
        "c:\\halo\\SOURCE\\effects\\weather_particle_systems.c", 0x66,
        type_index >= 0 &&
          type_index_int < *(int *)((char *)type_definition + 0x24));

      particle_type = weather_particle_system + type_index_int * 0x10 + 0x1c;
      type_element = tag_block_get_element((void *)((char *)definition + 0x24),
                                           type_index_int, 0x25c);

      *(int *)(particle_type + 0xc) = -1;
      *(int16_t *)(particle_type + 8) = 0;

      *(float *)particle_type =
        random_real_range((int *)random_math_get_local_seed_address(),
                          *(float *)((char *)type_element + 0xa4),
                          *(float *)((char *)type_element + 0xa8));

      type_index = type_index + 1;
      type_index_int = (int)type_index;
      *(float *)(particle_type + 4) = *(float *)((char *)type_element + 0x30);
    } while (type_index_int < *(int *)((char *)definition + 0x24));
  }
}

void FUN_000a4200(int16_t local_player_index)
{
  char *weather_particle_system;
  void *definition;
  void *type_definition;
  char *particle_type;
  void *particle;
  int16_t type_index;
  int type_index_int;
  int next_particle_handle;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\effects\\weather_particle_systems.c",
                     0x5b,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  weather_particle_system = (char *)0x4557f4 + (int)local_player_index * 0x9c;
  definition = tag_get(0x7261696e, *(int *)weather_particle_system);
  type_index = 0;
  type_index_int = 0;

  if (*(int *)((char *)definition + 0x24) > 0) {
    do {
      type_definition = tag_get(0x7261696e, *(int *)weather_particle_system);
      assert_halt_msg_at(
        "type_index>=0 && type_index<definition->particle_types.count",
        "c:\\halo\\SOURCE\\effects\\weather_particle_systems.c", 0x66,
        type_index >= 0 &&
          type_index_int < *(int *)((char *)type_definition + 0x24));

      particle_type = weather_particle_system + type_index_int * 0x10 + 0x1c;
      while (*(int *)(particle_type + 0xc) != -1) {
        particle = datum_get(weather_particle_system_data,
                             *(int *)(particle_type + 0xc));
        next_particle_handle = *(int *)((char *)particle + 0x50);
        datum_delete(weather_particle_system_data,
                     *(int *)(particle_type + 0xc));
        --*(int16_t *)(particle_type + 0x8);
        *(int *)(particle_type + 0xc) = next_particle_handle;
      }

      type_index = type_index + 1;
      type_index_int = (int)type_index;
    } while (type_index_int < *(int *)((char *)definition + 0x24));
  }

  --weather_particle_system_count;
  *(int *)weather_particle_system = -1;
}

/* 0xa4310 */
/* Spawns one weather particle of `type_index` for `local_player_index`:
 * allocates a datum, randomizes position/direction/scale/color/rotation from
 * the type definition's ranges, then push-fronts it on the per-type particle
 * list. Returns the new particle handle, or NONE when the pool is full. */
int FUN_000a4310(int16_t type_index /* @<eax> */,
                 int16_t local_player_index /* @<ecx> */)
{
  char *weather_particle_system;
  char *particle_type;
  char *particle;
  char *bitmap;
  void *definition;
  void *type_element;
  void *sequence_element;
  float *direction;
  float scale;
  int particle_handle;
  int16_t sequence_index;

  particle_handle = data_new_at_index(weather_particle_system_data);
  if (particle_handle != NONE) {
    assert_halt_msg_at("local_player_index>=0 && "
                       "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                       "c:\\halo\\SOURCE\\effects\\weather_particle_systems.c",
                       0x5b,
                       local_player_index >= 0 &&
                         local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

    weather_particle_system = (char *)0x4557f4 + (int)local_player_index * 0x9c;
    definition = tag_get(0x7261696e, *(int *)weather_particle_system);
    particle_type = (char *)FUN_000a3ea0(weather_particle_system, type_index);
    type_element = tag_block_get_element((void *)((char *)definition + 0x24),
                                         (int)type_index, 0x25c);
    particle = (char *)datum_get(weather_particle_system_data, particle_handle);
    bitmap =
      (char *)tag_get(0x6269746d, *(int *)((char *)type_element + 0x1a0));

    *(float *)(particle + 4) =
      random_real_range((int *)random_math_get_local_seed_address(), 0.0f,
                        *(float *)(particle_type + 4));

    *(float *)(particle + 8) =
      random_real_range((int *)random_math_get_local_seed_address(), 0.0f,
                        *(float *)(particle_type + 4));

    *(float *)(particle + 0xc) =
      random_real_range((int *)random_math_get_local_seed_address(), 0.0f,
                        *(float *)(particle_type + 4));

    *(int *)(particle + 0x18) = 0;
    *(int *)(particle + 0x14) = 0;
    *(int *)(particle + 0x10) = 0;

    direction = (float *)(particle + 0x1c);
    random_seed_get_direction3d(random_math_get_local_seed_address(), direction);

    scale = random_real_range((int *)random_math_get_local_seed_address(),
                              *(float *)((char *)type_element + 0xcc),
                              *(float *)((char *)type_element + 0xd0));
    direction[0] *= scale;
    direction[1] *= scale;
    direction[2] *= scale;

    *(float *)(particle + 0x44) =
      random_real_range((int *)random_math_get_local_seed_address(),
                        *(float *)((char *)type_element + 0xfc),
                        *(float *)((char *)type_element + 0x100));

    *(float *)(particle + 0x4c) =
      random_real_range((int *)random_math_get_local_seed_address(),
                        *(float *)((char *)type_element + 0x104),
                        *(float *)((char *)type_element + 0x108));

    *(float *)(particle + 0x48) =
      random_real_range((int *)random_math_get_local_seed_address(),
                        *(float *)((char *)type_element + 0x10c),
                        *(float *)((char *)type_element + 0x110));

    if (*(char *)((char *)type_element + 0x20) & 4) {
      *(float *)(particle + 0x30) =
        random_real_range((int *)random_math_get_local_seed_address(), 0.0f,
                          6.2831855f);
    } else {
      *(int *)(particle + 0x30) = 0;
    }

    sequence_index =
      random_range(random_math_get_local_seed_address(), 0,
                   (int16_t)*(uint16_t *)(bitmap + 0x54));
    *(int16_t *)(particle + 0x28) = sequence_index;
    sequence_element =
      tag_block_get_element((void *)(bitmap + 0x54), (int)sequence_index, 0x40);
    *(float *)(particle + 0x2c) =
      random_real_range((int *)random_math_get_local_seed_address(), 0.0f,
                        (float)*(int *)((char *)sequence_element + 0x34));

    FUN_0007c270((float *)(particle + 0x38),
                 *(uint32_t *)((char *)type_element + 0x20),
                 (float *)((char *)type_element + 0x138),
                 (float *)((char *)type_element + 0x148),
                 random_math_real(random_math_get_local_seed_address()));

    *(float *)(particle + 0x34) =
      random_real_range((int *)random_math_get_local_seed_address(),
                        *(float *)((char *)type_element + 0x134),
                        *(float *)((char *)type_element + 0x144));

    *(int *)(particle + 0x50) = *(int *)(particle_type + 0xc);
    ++*(int16_t *)(particle_type + 8);
    *(int *)(particle_type + 0xc) = particle_handle;
  }

  return particle_handle;
}

/* 0xa45d0 */
/* Wraps `vector_in` into [0, extent) via FUN_000a4000, then rewrites
 * `vector_out` as the offset from the wrapped point back to the input. */
void FUN_000a45d0(float *vector_out /* @<eax> */, float *vector_in /* @<ecx> */,
                  float extent)
{
  FUN_000a4000(vector_out, vector_in, extent);
  vector_out[0] = vector_in[0] - vector_out[0];
  vector_out[1] = vector_in[1] - vector_out[1];
  vector_out[2] = vector_in[2] - vector_out[2];
}

/* 0xa4610 */
/* Advances one weather particle for a frame: blends its direction toward a
 * fresh random direction by the type's turbulence weight, clamps the speed
 * into the type's [min,max] band, integrates velocity, runs the particle
 * through physics, applies a small per-particle positional jitter, and wraps
 * the result back into the system's extent.
 *
 * VC71 88.2%; the residual [FPU-WARN] is base-register-only (ref (%esi) vs
 * (%edi), 0x14(%edi) vs 0x14(%ebx)) with identical mnemonics and offsets --
 * not an operand-order or FSUB-direction bug. The ref also reads
 * local_direction back out of EAX after random_seed_get_direction3d, which
 * returns void; we address the array directly instead (lift-learnings 16). */
void FUN_000a4610(int16_t type_index /* @<eax> */,
                  int16_t local_player_index /* @<ecx> */,
                  int16_t particle_index)
{
  char *weather_particle_system;
  char *particle_type;
  char *particle;
  void *definition;
  void *type_definition;
  void *type_element;
  float *direction;
  float *position;
  unsigned int *seed;
  unsigned int particle_seed;
  float local_direction[3];
  float speed;
  float blend;
  float delta_time;
  float turbulence_min;
  float turbulence_max;
  int flags;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\effects\\weather_particle_systems.c",
                     0x5b,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  weather_particle_system = (char *)0x4557f4 + (int)local_player_index * 0x9c;
  definition = tag_get(0x7261696e, *(int *)weather_particle_system);
  type_definition = tag_get(0x7261696e, *(int *)weather_particle_system);

  assert_halt_msg_at(
    "type_index>=0 && type_index<definition->particle_types.count",
    "c:\\halo\\SOURCE\\effects\\weather_particle_systems.c", 0x66,
    type_index >= 0 &&
      (int)type_index < *(int *)((char *)type_definition + 0x24));

  particle_type = weather_particle_system + ((int)type_index << 4) + 0x1c;
  type_element = tag_block_get_element((void *)((char *)definition + 0x24),
                                       (int)type_index, 0x25c);
  particle =
    (char *)datum_get(weather_particle_system_data, (int)particle_index);

  if (*(float *)((char *)type_element + 0xcc) != 0.0f ||
      *(float *)((char *)type_element + 0xd0) != 0.0f) {
    direction = (float *)(particle + 0x1c);
    speed = normalize3d(direction);

    blend = 1.0f - *(float *)((char *)type_element + 0xd4);
    turbulence_max = *(float *)((char *)type_element + 0xd8);
    turbulence_min = -*(float *)((char *)type_element + 0xd8);
    seed = random_math_get_local_seed_address();
    speed =
      speed + random_real_range((int *)seed, turbulence_min, turbulence_max);

    if (speed < *(float *)((char *)type_element + 0xcc))
      speed = *(float *)((char *)type_element + 0xcc);
    else if (speed > *(float *)((char *)type_element + 0xd0))
      speed = *(float *)((char *)type_element + 0xd0);

    seed = random_math_get_local_seed_address();
    random_seed_get_direction3d(seed, local_direction);

    direction[0] =
      local_direction[0] * *(float *)((char *)type_element + 0xd4) +
      blend * direction[0];
    direction[1] =
      local_direction[1] * *(float *)((char *)type_element + 0xd4) +
      blend * direction[1];
    direction[2] =
      local_direction[2] * *(float *)((char *)type_element + 0xd4) +
      blend * direction[2];

    direction[0] = speed * direction[0];
    direction[1] = speed * direction[1];
    direction[2] = speed * direction[2];

    delta_time = *(float *)(weather_particle_system + 8);
    *(float *)(particle + 0x10) =
      delta_time * direction[0] + *(float *)(particle + 0x10);
    *(float *)(particle + 0x14) =
      delta_time * direction[1] + *(float *)(particle + 0x14);
    *(float *)(particle + 0x18) =
      delta_time * direction[2] + *(float *)(particle + 0x18);
  }

  flags = (*(char *)(weather_particle_system + 0x1a) != 0) ? 7 : 5;
  particle_seed = (unsigned int)(int)particle_index;
  position = (float *)(particle + 4);

  FUN_00154a50(flags,
               (int)tag_get(0x70706879, *(int *)((char *)type_element + 0xb8)),
               (int *)(weather_particle_system + 0x10),
               (int)*(uint16_t *)(weather_particle_system + 0x18), position,
               (float *)(particle + 0x10), 0, 0, 0, *(float *)(particle + 0x44),
               *(float *)(weather_particle_system + 8));

  random_seed_get_direction3d(&particle_seed, local_direction);

  position[0] = local_direction[0] * *(float *)0x255ef8 + position[0];
  position[1] = local_direction[1] * *(float *)0x255ef8 + position[1];
  position[2] = local_direction[2] * *(float *)0x255ef8 + position[2];

  FUN_000a4000(position, position, *(float *)(particle_type + 4));
}

/* 0xa48c0 */
/* Snapshots the render camera into `planes`: copies the 4x4 clip-plane block
 * from the global camera state, stores the forward vector at +0x40, and at
 * +0x4c the plane distance dot(camera_position + forward*clip_distance,
 * forward). */
void FUN_000a48c0(void *planes /* @<ecx> */, float clip_distance)
{
  float *plane_block;
  float *forward;
  float point_x;
  float point_y;
  float point_z;

  forward = (float *)((char *)planes + 0x40);

  point_x = *(float *)0x50655c * clip_distance + *(float *)0x506550;
  point_y = *(float *)0x506560 * clip_distance + *(float *)0x506554;
  point_z = *(float *)0x506564 * clip_distance + *(float *)0x506558;

  forward[0] = *(float *)0x50655c;
  forward[1] = *(float *)0x506560;
  forward[2] = *(float *)0x506564;

  forward[3] =
    point_z * forward[2] + point_y * forward[1] + point_x * forward[0];

  plane_block = (float *)planes;
  plane_block[0] = *(float *)0x50661c;
  plane_block[1] = *(float *)0x506620;
  plane_block[2] = *(float *)0x506624;
  plane_block[3] = *(float *)0x506628;

  plane_block = (float *)((char *)planes + 0x10);
  plane_block[0] = *(float *)0x50662c;
  plane_block[1] = *(float *)0x506630;
  plane_block[2] = *(float *)0x506634;
  plane_block[3] = *(float *)0x506638;

  plane_block = (float *)((char *)planes + 0x20);
  plane_block[0] = *(float *)0x50663c;
  plane_block[1] = *(float *)0x506640;
  plane_block[2] = *(float *)0x506644;
  plane_block[3] = *(float *)0x506648;

  plane_block = (float *)((char *)planes + 0x30);
  plane_block[0] = *(float *)0x50664c;
  plane_block[1] = *(float *)0x506650;
  plane_block[2] = *(float *)0x506654;
  plane_block[3] = *(float *)0x506658;
}

/* 0xa4a00 */
int16_t FUN_000a4a00(int *visible_indices, float range)
{
  char *scenario;
  int *weather_polyhedra;
  float *polyhedron;
  float dx;
  float dy;
  float dz;
  float radius;
  float distance_squared;
  int16_t visible_count;
  int16_t polyhedron_index;

  scenario = (char *)scenario_get();
  visible_count = 0;
  polyhedron_index = 0;
  weather_polyhedra = (int *)(scenario + 0x1c0);
  if (*(int *)(scenario + 0x1c0) > 0) {
    do {
      polyhedron = (float *)tag_block_get_element((void *)weather_polyhedra,
                                                  polyhedron_index, 0x20);
      dx = *polyhedron - *(float *)0x506550;
      dy = polyhedron[1] - *(float *)0x506554;
      dz = polyhedron[2] - *(float *)0x506558;
      radius = range + polyhedron[3];
      distance_squared = dx * dx;
      distance_squared = distance_squared + dz * dz;
      distance_squared = distance_squared + dy * dy;
      if (distance_squared < radius * radius) {
        if (visible_count < 8) {
          *(int16_t *)((char *)visible_indices + (int16_t)visible_count * 2) =
            (int16_t)polyhedron_index;
          visible_count++;
        } else {
          error(2, "too many weather polyhedra visible.");
        }
      }
      polyhedron_index++;
    } while ((int)polyhedron_index < *weather_polyhedra);
  }
  return visible_count;
}

/* 0xa4ab0 */
void FUN_000a4ab0(int16_t local_player_index, int16_t type_index /* @<eax> */,
                  float scale)
{
  char *weather_particle_system;
  void *type_definition;
  char *particle_type;
  void *particle;
  float type_field_04;
  float target_f;
  int target_count;
  int16_t current_count;
  int16_t *count_field;
  int particle_handle;
  int next_particle_handle;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\effects\\weather_particle_systems.c",
                     0x5b,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  weather_particle_system = (char *)0x4557f4 + (int)local_player_index * 0x9c;
  tag_get(0x7261696e, *(int *)weather_particle_system);
  type_definition = tag_get(0x7261696e, *(int *)weather_particle_system);

  assert_halt_msg_at(
    "type_index>=0 && type_index<definition->particle_types.count",
    "c:\\halo\\SOURCE\\effects\\weather_particle_systems.c", 0x66,
    type_index >= 0 &&
      (int)type_index < *(int *)((char *)type_definition + 0x24));

  particle_type = weather_particle_system + (int)type_index * 0x10 + 0x1c;

  /* target = particle_type[0] * particle_type[4]^3 * scale /
   *          weather_particle_system_count, clamped to >= 0. */
  type_field_04 = *(float *)(particle_type + 4);
  target_f = type_field_04;
  target_f = target_f * *(float *)particle_type;
  target_f = target_f * type_field_04;
  target_f = target_f * scale;
  target_f = target_f * type_field_04;
  target_count = (int)(target_f / (int)weather_particle_system_count);
  if (target_count < 0)
    target_count = 0;

  current_count = *(int16_t *)(particle_type + 8);
  while (current_count < target_count) {
    particle_handle = FUN_000a4310(type_index, local_player_index);
    if (particle_handle == -1)
      break;
    current_count = *(int16_t *)(particle_type + 8);
  }

  count_field = (int16_t *)(particle_type + 8);
  current_count = *count_field;
  while (current_count > target_count) {
    particle =
      datum_get(weather_particle_system_data, *(int *)(particle_type + 0xc));
    next_particle_handle = *(int *)((char *)particle + 0x50);
    datum_delete(weather_particle_system_data, *(int *)(particle_type + 0xc));
    --*count_field;
    *(int *)(particle_type + 0xc) = next_particle_handle;
    current_count = *count_field;
  }
}

/* 0xa4be0 */
/* Per-frame update for one local player's weather system: advances the system
 * clock, then for every particle type spawns new particles at a rate faded by
 * the camera's height band, and steps each live particle's bitmap sequence,
 * rotation, and physics. */
void FUN_000a4be0(int16_t local_player_index)
{
  char *weather_particle_system;
  char *particle_type;
  char *particle;
  char *bitmap;
  void *definition;
  void *type_definition;
  void *type_element;
  void *sequence_element;
  void *particle_types;
  float camera_height;
  float near_fade;
  float far_fade;
  float sequence_position;
  int16_t type_index;
  int type_index_int;
  int particle_handle;
  int spin;
#if defined(_MSC_VER) && !defined(__clang__)
  double __cdecl fmod(double, double);
#endif

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\effects\\weather_particle_systems.c",
                     0x5b,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  weather_particle_system = (char *)0x4557f4 + (int)local_player_index * 0x9c;
  definition = tag_get(0x7261696e, *(int *)weather_particle_system);

  *(int *)(weather_particle_system + 8) = *(int *)0x50654c;
  *(float *)(weather_particle_system + 4) =
    *(float *)(weather_particle_system + 8) +
    *(float *)(weather_particle_system + 4);

  type_index = 0;
  type_index_int = 0;
  particle_types = (void *)((char *)definition + 0x24);

  if (*(int *)particle_types > 0) {
    do {
      type_definition = tag_get(0x7261696e, *(int *)weather_particle_system);
      assert_halt_msg_at(
        "type_index>=0 && type_index<definition->particle_types.count",
        "c:\\halo\\SOURCE\\effects\\weather_particle_systems.c", 0x66,
        type_index >= 0 &&
          type_index_int < *(int *)((char *)type_definition + 0x24));

      particle_type = weather_particle_system + (type_index_int << 4) + 0x1c;
      type_element =
        tag_block_get_element(particle_types, type_index_int, 0x25c);
      bitmap =
        (char *)tag_get(0x6269746d, *(int *)((char *)type_element + 0x1a0));

      camera_height = *(float *)0x506558;

      near_fade = (camera_height - *(float *)((char *)type_element + 0x34)) /
                  (*(float *)((char *)type_element + 0x38) -
                   *(float *)((char *)type_element + 0x34));
      if (near_fade < 0.0f)
        near_fade = 0.0f;
      else if (near_fade > 1.0f)
        near_fade = 1.0f;

      far_fade = (camera_height - *(float *)((char *)type_element + 0x3c)) /
                 (*(float *)((char *)type_element + 0x40) -
                  *(float *)((char *)type_element + 0x3c));
      if (far_fade < 0.0f)
        far_fade = 0.0f;
      else if (far_fade > 1.0f)
        far_fade = 1.0f;

      FUN_000a4ab0(local_player_index, type_index,
                   (1.0f - far_fade) *
                     (*(float *)(weather_particle_system + 0xc) * near_fade));

      particle_handle = *(int *)(particle_type + 0xc);
      if (particle_handle != NONE) {
        bitmap = bitmap + 0x54;
        do {
          particle =
            (char *)datum_get(weather_particle_system_data, particle_handle);

          sequence_position = *(float *)(particle + 0x4c) *
                                *(float *)(weather_particle_system + 8) +
                              *(float *)(particle + 0x2c);
          *(float *)(particle + 0x2c) = sequence_position;

          sequence_element = tag_block_get_element(
            (void *)bitmap, (int)*(int16_t *)(particle + 0x28), 0x40);
#if defined(_MSC_VER) && !defined(__clang__)
          *(float *)(particle + 0x2c) =
            (float)fmod((double)sequence_position,
                        (double)*(int *)((char *)sequence_element + 0x34));
#else
          *(float *)(particle + 0x2c) =
            x87_fmod(sequence_position,
                     (double)*(int *)((char *)sequence_element + 0x34));
#endif

          spin = (particle_handle & 1) ? -1 : 1;
          *(float *)(particle + 0x30) =
            (float)spin * *(float *)(particle + 0x48) *
              *(float *)(weather_particle_system + 8) +
            *(float *)(particle + 0x30);

          FUN_000a4610(type_index, local_player_index,
                       (int16_t)particle_handle);

          particle_handle = *(int *)(particle + 0x50);
        } while (particle_handle != NONE);
      }

      type_index = type_index + 1;
      type_index_int = (int)type_index;
    } while (type_index_int < *(int *)particle_types);
  }
}

/* 0xa4e20 */
/* Renders one local player's weather particles.
 *
 * The particle field is a single extent-sized cube of particles that is tiled
 * 3x3x3 around the camera.  Box 0 is the untiled cell (camera position snapped
 * down to the extent grid); the 26 surrounding cells are frustum-culled up
 * front by render_frustum_cube_visible, and each surviving cell caches the
 * five camera-plane distances of its origin.  A particle is then drawn in the
 * first cell whose five cached distances, offset by the particle's own plane
 * distances, are all negative -- i.e. the first cell that puts it on screen.
 */
void weather_particle_system_render(int16_t local_player_index /* @<eax> */)
{
  char *weather_particle_system;
  char *definition;
  char *scenario;
  char *particle_type;
  char *type_element;
  char *particle;
  char *polyhedron;
  int *particle_types;
  int *polyhedron_planes;
  float *plane;
  float *box;
  float *direction;
  float camera_planes[5][4];
  float box_position[26][3];
  float box_plane_distance[26][5];
  float particle_plane_distance[5];
  float world_position[3];
  float cube[6];
  float tile_offset[3];
  float box_min_x, box_min_y, box_min_z;
  float box_max_x, box_max_y, box_max_z;
  float fade_extent;
  float depth;
  float ratio;
  float near_fade;
  float far_fade;
  int16_t visible_polyhedra[8];
  char sprite_data[0xa4];
  int16_t visible_polyhedron_count;
  int16_t box_count;
  int16_t type_index;
  int16_t box_index;
  int16_t polyhedron_index;
  int16_t plane_index;
  int16_t render_mode;
  int16_t i, j, k, n;
  int particle_handle;
  int inside;
  int occluded;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\effects\\weather_particle_systems.c",
                     0x5b,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  weather_particle_system = (char *)0x4557f4 + (int)local_player_index * 0x9c;
  definition = (char *)tag_get(0x7261696e, *(int *)weather_particle_system);
  scenario = (char *)scenario_get();
  FUN_000a4be0(local_player_index);

  particle_types = (int *)(definition + 0x24);
  for (type_index = 0; (int)type_index < *particle_types; ++type_index) {
    particle_type = weather_particle_system + (int)type_index * 0x10 + 0x1c;
    type_element =
      (char *)tag_block_get_element(particle_types, (int)type_index, 0x25c);
    if (*(int16_t *)(particle_type + 8) == 0)
      continue;

    visible_polyhedron_count =
      FUN_000a4a00((int *)visible_polyhedra, *(float *)(particle_type + 4));
    FUN_000a48c0(camera_planes, *(float *)(particle_type + 4));
    FUN_000a4000(box_position[0], (float *)0x506550,
                 *(float *)(particle_type + 4));

    /* Snap the camera down to the extent grid: box 0's origin. */
    box_position[0][0] = *(float *)0x506550 - box_position[0][0];
    box_position[0][1] = *(float *)0x506554 - box_position[0][1];
    box_position[0][2] = *(float *)0x506558 - box_position[0][2];

    for (n = 0; n < 5; ++n)
      box_plane_distance[0][n] = box_position[0][1] * camera_planes[n][1] +
                                 box_position[0][0] * camera_planes[n][0] +
                                 box_position[0][2] * camera_planes[n][2];

    box_min_x = box_position[0][0];
    box_max_x = box_position[0][0] + *(float *)(particle_type + 4);
    box_min_y = box_position[0][1];
    box_max_y = box_position[0][1] + *(float *)(particle_type + 4);
    box_min_z = box_position[0][2];
    box_max_z = box_position[0][2] + *(float *)(particle_type + 4);

    tile_offset[0] = -*(float *)(particle_type + 4);
    tile_offset[1] = 0.0f;
    tile_offset[2] = *(float *)(particle_type + 4);

    box_count = 1;
    for (i = 0; i < 3; ++i) {
      for (j = 0; j < 3; ++j) {
        for (k = 0; k < 3; ++k) {
          if (i == 1 && j == 1 && k == 1)
            continue;

          cube[0] = tile_offset[i] + box_min_x;
          cube[2] = box_min_y + tile_offset[j];
          cube[4] = tile_offset[k] + box_min_z;
          cube[1] = tile_offset[i] + box_max_x;
          cube[3] = tile_offset[j] + box_max_y;
          cube[5] = tile_offset[k] + box_max_z;

          if (render_frustum_cube_visible((void *)0x5065a4, (int)(size_t)cube,
                                          1) != 0) {
            assert_halt_msg_at(
              "box_count<MAXIMUM_NUMBER_OF_VISIBLE_WEATHER_PARTICLE_BOXES",
              "c:\\halo\\SOURCE\\effects\\weather_particle_systems.c", 0x2a1,
              box_count < 26);

            box = box_position[box_count];
            box[0] = cube[0];
            box[1] = cube[2];
            box[2] = cube[4];

            for (n = 0; n < 5; ++n)
              box_plane_distance[box_count][n] = box[1] * camera_planes[n][1] +
                                                 camera_planes[n][0] * box[0] +
                                                 camera_planes[n][2] * box[2];

            ++box_count;
          }
        }
      }
    }

    FUN_0018d2c0((uint32_t *)sprite_data, *(int16_t *)(particle_type + 8),
                 (uint32_t) * (int *)(type_element + 0x1a0),
                 (int)(size_t)(type_element + 0x1a8), 0);

    particle_handle = *(int *)(particle_type + 0xc);
    while (particle_handle != NONE) {
      particle =
        (char *)datum_get(weather_particle_system_data, particle_handle);

      for (n = 0; n < 5; ++n)
        particle_plane_distance[n] =
          (camera_planes[n][1] * *(float *)(particle + 8) +
           camera_planes[n][2] * *(float *)(particle + 0xc) +
           camera_planes[n][0] * *(float *)(particle + 4)) - camera_planes[n][3];

      box = (float *)0;
      for (box_index = 0; box_index < box_count; ++box_index) {
        inside = 1;
        for (n = 0; n < 5; ++n) {
          if (!inside)
            break;
          inside =
            box_plane_distance[box_index][n] + particle_plane_distance[n] <
            0.0f;
        }
        if (inside) {
          box = box_position[box_index];
          break;
        }
      }

      if (box != (float *)0) {
        fade_extent =
          (*(float *)(type_element + 0x30) > *(float *)(particle_type + 4)) ?
            *(float *)(particle_type + 4) :
            *(float *)(type_element + 0x30);

        world_position[0] = box[0] + *(float *)(particle + 4);
        world_position[1] = box[1] + *(float *)(particle + 8);
        world_position[2] = box[2] + *(float *)(particle + 0xc);

        depth = (world_position[0] - *(float *)0x506550) * *(float *)0x50655c +
                (world_position[1] - *(float *)0x506554) * *(float *)0x506560 +
                (world_position[2] - *(float *)0x506558) * *(float *)0x506564;

        if (depth > *(float *)(type_element + 0x24) && depth < fade_extent) {
          ratio =
            (depth - *(float *)(type_element + 0x24)) /
            (*(float *)(type_element + 0x28) - *(float *)(type_element + 0x24));
          if (ratio < 0.0f)
            near_fade = 0.0f;
          else if (ratio > 1.0f)
            near_fade = 1.0f;
          else
            near_fade = ratio;

          ratio = (depth - *(float *)(type_element + 0x2c)) /
                  (fade_extent - *(float *)(type_element + 0x2c));
          if (ratio < 0.0f)
            ratio = 0.0f;
          else if (ratio > 1.0f)
            ratio = 1.0f;
          far_fade = 1.0f - ratio;

          /* Cull particles that fall inside any visible occluder polyhedron. */
          occluded = 0;
          for (polyhedron_index = 0;
               polyhedron_index < visible_polyhedron_count;
               ++polyhedron_index) {
            polyhedron = (char *)tag_block_get_element(
              (void *)(scenario + 0x1c0), visible_polyhedra[polyhedron_index],
              0x20);
            polyhedron_planes = (int *)(polyhedron + 0x14);

            plane_index = 0;
            if (*polyhedron_planes > 0) {
              do {
                plane = (float *)tag_block_get_element(
                  (void *)polyhedron_planes, (int)plane_index, 0x10);
                if (world_position[0] * plane[0] +
                      world_position[1] * plane[1] +
                      world_position[2] * plane[2] - plane[3] <
                    0.0f)
                  break;
                ++plane_index;
              } while ((int)plane_index < *polyhedron_planes);
            }

            if ((int)plane_index == *polyhedron_planes) {
              occluded = 1;
              break;
            }
          }

          if (!occluded) {
            direction = (*(int16_t *)(type_element + 0x1a6) == 1) ?
                          (float *)(particle + 0x1c) :
                          (float *)(particle + 0x10);

            render_mode = *(int16_t *)(type_element + 0x1a4);
            if (render_mode != 0) {
              if (direction[0] * direction[0] + direction[1] * direction[1] +
                    direction[2] * direction[2] ==
                  0.0f)
                direction = *(float **)0x31fc44;
            }

            FUN_0018d6e0(
              sprite_data, render_mode, *(uint16_t *)(particle + 0x28),
              (int16_t) * (float *)(particle + 0x2c), world_position, direction,
              *(float *)(particle + 0x30),
              (*(float *)(particle + 0x44) + *(float *)(particle + 0x44)) *
                *(float *)(type_element + 0x154),
              (float *)(particle + 0x34), far_fade * near_fade, 0);
          }
        }
      }

      particle_handle = *(int *)(particle + 0x50);
    }

    FUN_0018d360(sprite_data);
  }
}
