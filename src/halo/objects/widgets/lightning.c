/* Lightning widgets.
 *
 * TU: c:\halo\SOURCE\objects\widgets\lightning.c (confirmed via the __FILE__
 * assert string at 0x29acfc PUSHed by lightnings_initialize,
 * lightning_offset_marker_position and lightning_submit).  Object range
 * 0x135320-0x135f1a.  Names: T2. */

#include "../../math/real_math.h"

#define LIGHTNING_FILE "c:\\halo\\SOURCE\\objects\\widgets\\lightning.c"

#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* Largest subdivided point chain one lightning strip can hold: the points
 * buffer is cleared with csmemset(points, 0, 0x24024) = 4097 * 0x24
 * (0x135651). */
#define MAXIMUM_LIGHTNING_POINTS 4097

/* lightning_marker_definition.flags bits (T2). */
enum {
  _lightning_marker_not_connected_to_next_marker_bit = 0 /* TEST byte [ESI+0x20],1 @0x135704 */
};

/* 'elec' markers block element.  Size: element size 0xe4 at every
 * tag_block_get_element over the block (0x13557a, 0x135632, 0x135719). */
typedef struct {
  char attachment_marker[32];            ///< offset=0x00  marker name handed to object_get_marker_by_name (0x135582)
  uint16_t flags;                        ///< offset=0x20  TEST byte [ESI+0x20],1 @0x135704
  uint8_t pad_22[2];                     ///< offset=0x22
  uint16_t octaves_to_next_marker;       ///< offset=0x24  XOR ECX,ECX; MOV CX,[ESI+0x24] @0x135728 (zero-extended)
  uint8_t pad_26[0x4e];                  ///< offset=0x26
  real_vector3d random_position_bounds;  ///< offset=0x74  LEA EDI,[ESI+0x74] @0x135690 (@<edi> arg)
  real random_jitter_offset;             ///< offset=0x80  FLD/FSUB/FADD [ECX+0x80] @0x135961
  real thickness;                        ///< offset=0x84  MOV ECX,[EAX+0x84] @0x1356bc
  real_argb_color tint;                  ///< offset=0x88  4-dword copy from EAX+0x88 @0x1356cb
  uint8_t pad_98[0x4c];                  ///< offset=0x98
} lightning_marker_definition;
cs(lightning_marker_definition, 0xe4);
co(lightning_marker_definition, flags, 0x20);
co(lightning_marker_definition, octaves_to_next_marker, 0x24);
co(lightning_marker_definition, random_position_bounds, 0x74);
co(lightning_marker_definition, random_jitter_offset, 0x80);
co(lightning_marker_definition, thickness, 0x84);
co(lightning_marker_definition, tint, 0x88);

/* 'elec' tag definition.  Prefix only: nothing past the shaders block is
 * accessed, so there is no size assert. */
typedef struct {
  uint8_t pad_00[2];               ///< offset=0x00
  int16_t count;                   ///< offset=0x02  CMP word [ESI+0x2] @0x1355d6, 0x135f07
  uint8_t pad_04[0x28];            ///< offset=0x04
  int16_t jitter_scale_source;     ///< offset=0x2c  MOV AX,[ESI+0x2c] @0x135602
  int16_t thickness_scale_source;  ///< offset=0x2e  MOV AX,[EAX+0x2e] @0x135b6f
  int16_t tint_modulation_source;  ///< offset=0x30  MOV AX,[EAX+0x30] @0x135b95
  int16_t brightness_scale_source; ///< offset=0x32  MOV AX,[EAX+0x32] @0x135bb9
  tag_reference map;               ///< offset=0x34  tag_get('bitm', [ESI+0x40]) @0x13559d
  uint8_t pad_44[0x54];            ///< offset=0x44
  tag_block markers;               ///< offset=0x98  LEA EBX,[ESI+0x98] @0x13555a
  tag_block shaders;               ///< offset=0xa4  [EAX+0xa4] @0x135e4b
} lightning_definition;
co(lightning_definition, count, 0x02);
co(lightning_definition, jitter_scale_source, 0x2c);
co(lightning_definition, map, 0x34);
co(lightning_definition, markers, 0x98);
co(lightning_definition, shaders, 0xa4);

/* One subdivided point of a strip.  Stride 0x24 (LEA [EAX+EAX*8]; SHL 2). */
typedef struct {
  real_point3d position; ///< offset=0x00
  real width;            ///< offset=0x0c  node thickness copied from +0x84
  real_argb_color color; ///< offset=0x10  node tint copied from +0x88
  boolean valid;         ///< offset=0x20  asserted in the fill and emit loops
  uint8_t pad_21[3];     ///< offset=0x21
} intermediate_lightning_point;
cs(intermediate_lightning_point, 0x24);
co(intermediate_lightning_point, width, 0x0c);
co(intermediate_lightning_point, color, 0x10);
co(intermediate_lightning_point, valid, 0x20);

/* Dynamic unlit vertex (type 6).  Stride 0x18 (ADD ESI,0x18 @0x135d2d). */
typedef struct {
  real_point3d point;   ///< offset=0x00
  real_point2d texture; ///< offset=0x0c  x = u + u_offset, y = 0 / 1
  uint32_t color;       ///< offset=0x14  real_argb_color_to_pixel32
} lightning_vertex;
cs(lightning_vertex, 0x18);
co(lightning_vertex, texture, 0x0c);
co(lightning_vertex, color, 0x14);

/* Dynamic vertex type passed to rasterizer_dynamic_vertices_new (0x135ae0). */
#define _rasterizer_vertex_type_dynamic_unlit 6

#define lightning_get(lightning_index) \
  ((lightning_datum_t *)datum_get(lightning_globals.lightning_data, (lightning_index)))

/* lightnings_initialize (0x135320) */
void lightnings_initialize(void)
{
  lightning_globals.lightning_data = game_state_data_new(
      "lightnings", MAXIMUM_LIGHTNINGS, sizeof(lightning_datum_t));
  assert_halt_msg_at("lightning_globals.lightning_data", LIGHTNING_FILE, 50,
                     lightning_globals.lightning_data);
}

/* lightnings_dispose (0x135360): a bare RET. */
void lightnings_dispose(void)
{
}

/* lightnings_initialize_for_new_map (0x135370)
 *
 * The call target is the function kb.json names data_delete_all (0x119b20). */
void lightnings_initialize_for_new_map(void)
{
  if (lightning_globals.lightning_data)
    data_delete_all(lightning_globals.lightning_data);
}

/* lightnings_dispose_from_old_map (0x135390) */
void lightnings_dispose_from_old_map(void)
{
  if (lightning_globals.lightning_data)
    data_make_invalid(lightning_globals.lightning_data);
}

/* lightning_new (0x1353b0) */
int lightning_new(int definition_index)
{
  int lightning_index = data_new_at_index(lightning_globals.lightning_data);

  if (lightning_index != -1)
    lightning_get(lightning_index)->definition_index = definition_index;

  return lightning_index;
}

/* lightning_delete (0x1353f0) */
void lightning_delete(int lightning_index)
{
  if (lightning_index != -1)
    datum_delete(lightning_globals.lightning_data, lightning_index);
}

/* lightning_render (0x135410): a bare RET. */
void lightning_render(void)
{
}

/* set_real_point3d: a real_math.h inline (components passed by value, so
 * the original evaluates them z, y, x and then stores x, y, z: 0x1359f2). */
static __inline real_point3d *set_real_point3d_inline(real_point3d *point, real x,
                                               real y, real z)
{
  point->x = x;
  point->y = y;
  point->z = z;
  return point;
}

/* lightning_offset_marker_position (0x135420)
 *
 * Register-arg ABI (TEST ESI @0x135426, TEST EBX @0x135447, TEST EDI
 * @0x135468): position @esi, matrix @ebx, random_position_bounds @edi.
 * The three random draws are taken z, y, x: the last one stays in ST0 and
 * feeds offset.i. */
void lightning_offset_marker_position(real_matrix4x3 *matrix,
                                      real_point3d *position,
                                      real_vector3d *random_position_bounds)
{
  real random_x;
  real random_y;
  real random_z;
  real_vector3d offset;

  assert_halt_at(LIGHTNING_FILE, 116, position);
  assert_halt_at(LIGHTNING_FILE, 117, matrix);
  assert_halt_at(LIGHTNING_FILE, 118, random_position_bounds);

  random_z = random_math_real(random_math_get_local_seed_address());
  random_y = random_math_real(random_math_get_local_seed_address());
  random_x = random_math_real(random_math_get_local_seed_address());

  offset.i = (2.0f * random_x - 1.0f) * random_position_bounds->i;
  offset.j = (2.0f * random_y - 1.0f) * random_position_bounds->j;
  offset.k = (2.0f * random_z - 1.0f) * random_position_bounds->k;
  matrix_scale_transform_vector(&matrix->scale, &offset.i, &offset.i);

  position->x += offset.i;
  position->y += offset.j;
  position->z += offset.k;
}

/* lightning_submit (0x135510)
 *
 * Renders an object's lightning widget as camera-facing strips.  Each marker
 * chain is seeded from its first marker, subdivided toward the next marker by
 * midpoint displacement (octaves_to_next_marker levels, amplitude halved per
 * level), and emitted as one dynamic unlit triangle strip when a marker is not
 * connected to the next one or is the last. */
void lightning_submit(int object_index, int lightning_index, void *lighting,
                      render_animation *animation)
{
  (void)lighting;
  if (object_index != -1 && lightning_index != -1) {
    lightning_datum_t *lightning = lightning_get(lightning_index);
    lightning_definition *definition =
        tag_get(TAG_GROUP_ELEC, lightning->definition_index);
    object_marker marker;

    if (definition->markers.count > 0 &&
        object_get_marker_by_name(
            object_index,
            ((lightning_marker_definition *)tag_block_get_element(
                 &definition->markers, 0, sizeof(lightning_marker_definition)))
                ->attachment_marker,
            &marker, 1) > 0) {
      void *bitmap = tag_block_get_element(
          &((bitmap_group *)tag_get(TAG_GROUP_BITM, definition->map.tag_index))
               ->bitmap_data,
          0, SIZEOF_BITMAP_DATA);

      if (xbox_texture_cache_get_hardware_format(bitmap, false, true)) {
        intermediate_lightning_point points[MAXIMUM_LIGHTNING_POINTS];
        int16_t instance_index;

        for (instance_index = 0; instance_index < definition->count;
             instance_index++) {
          int16_t point_count = 0;
          real jitter_scale = 1.0f;
          boolean first_marker = true;
          int16_t marker_index;

          if (animation) {
            if (animation->values) {
              int16_t source = definition->jitter_scale_source;
              if (source >= _object_function_reference_a &&
                  source <= _object_function_reference_d)
                jitter_scale = animation->values[source - 1];
            }
          }

          for (marker_index = 0; marker_index < definition->markers.count;
               marker_index++) {
            lightning_marker_definition *marker_definition =
                tag_block_get_element(&definition->markers, marker_index,
                                      sizeof(lightning_marker_definition));

            if (first_marker) {
              point_count = 0;
              csmemset(points, 0, sizeof(points));
              object_get_marker_by_name(object_index,
                                        marker_definition->attachment_marker,
                                        &marker, 1);
              points[0].position = marker.matrix.position;
              lightning_offset_marker_position(
                  &marker.matrix, &points[0].position,
                  &marker_definition->random_position_bounds);
              points[0].width = marker_definition->thickness;
              points[0].color = marker_definition->tint;
              points[0].valid = true;
              first_marker = false;
            }

            if ((marker_definition->flags &
                 (1 << _lightning_marker_not_connected_to_next_marker_bit)) ||
                marker_index == definition->markers.count - 1) {
              rasterizer_globals.current_lock_operation =
                  _rasterizer_lock_lightning;
              if (point_count > 2) {
                int vertex_buffer_index;

                point_count++;
                vertex_buffer_index = rasterizer_dynamic_vertices_new(
                    _rasterizer_vertex_type_dynamic_unlit, 2 * point_count);
                if (vertex_buffer_index != -1) {
                  lightning_vertex *vertices = (lightning_vertex *)
                      rasterizer_dynamic_vertices_lock(vertex_buffer_index);
                  real one_over_point_count = 1.0f / point_count;
                  real u_offset =
                      random_math_real(random_math_get_local_seed_address());
                  real thickness_scale = 1.0f;
                  real_rgb_color *tint = global_real_rgb_white;
                  real brightness_scale = 1.0f;
                  real_rectangle3d bounds;
                  real_point3d centroid;
                  shader_effect_definition *shader;
                  int16_t vertex_index;

                  assert_halt_at(LIGHTNING_FILE, 239, vertices);

                  if (animation) {
                    if (animation->values) {
                      int16_t source = definition->thickness_scale_source;
                      if (source >= _object_function_reference_a &&
                          source <= _object_function_reference_d)
                        thickness_scale = animation->values[source - 1];
                    }
                    if (animation->colors) {
                      int16_t source = definition->tint_modulation_source;
                      if (source >= _object_function_reference_a &&
                          source <= _object_function_reference_d)
                        tint = &animation->colors[source - 1];
                    }
                    if (animation->values) {
                      int16_t source = definition->brightness_scale_source;
                      if (source >= _object_function_reference_a &&
                          source <= _object_function_reference_d)
                        brightness_scale = animation->values[source - 1];
                    }
                  }

                  for (vertex_index = 0; vertex_index < point_count;
                       vertex_index++) {
                    real_point3d *position = &points[vertex_index].position;
                    real_point3d *previous_position =
                        vertex_index > 0 ? &points[vertex_index - 1].position
                                         : position;
                    real_point3d *next_position =
                        vertex_index < point_count - 1
                            ? &points[vertex_index + 1].position
                            : position;
                    real width = points[vertex_index].width * thickness_scale;
                    real u = vertex_index * one_over_point_count;
                    real_vector3d up;
                    real_argb_color color;
                    uint32_t pixel;

                    assert_halt_msg_at("points[vertex_index].valid",
                                       LIGHTNING_FILE, 280,
                                       points[vertex_index].valid);

                    vector_from_points3d(previous_position, next_position, &up);
                    { /* cross_product3d(&up, &render.camera.forward, &up), inlined */
                      real k = up.i * render.camera.forward.j - up.j * render.camera.forward.i;
                      real j = up.k * render.camera.forward.i - up.i * render.camera.forward.k;
                      real i = up.j * render.camera.forward.k - up.k * render.camera.forward.j;
                      up.i = i;
                      up.j = j;
                      up.k = k;
                    }
                    FUN_0010c2e0(&up.i);

                    color.alpha =
                        points[vertex_index].color.alpha * brightness_scale;
                    color.red = points[vertex_index].color.red * tint->red;
                    color.green = points[vertex_index].color.green * tint->green;
                    color.blue = points[vertex_index].color.blue * tint->blue;
                    pixel = real_argb_color_to_pixel32(&color.alpha);

                    vertices->point.x = position->x + up.i * width;
                    vertices->point.y = position->y + up.j * width;
                    vertices->point.z = position->z + up.k * width;
                    vertices->texture.x = u + u_offset;
                    vertices->color = pixel;
                    vertices->texture.y = 0.0f;
                    vertices++;

                    width = -width;
                    vertices->point.x = position->x + up.i * width;
                    vertices->point.y = position->y + up.j * width;
                    vertices->point.z = position->z + up.k * width;
                    vertices->texture.x = u + u_offset;
                    vertices->color = pixel;
                    vertices->texture.y = 1.0f;
                    vertices++;

                    if (vertex_index == 0) {
                      bounds.x0 = bounds.x1 = position->x;
                      bounds.y0 = bounds.y1 = position->y;
                      bounds.z0 = bounds.z1 = position->z;
                    } else {
                      bounds.x0 = MIN(position->x, bounds.x0);
                      bounds.y0 = MIN(position->y, bounds.y0);
                      bounds.z0 = MIN(position->z, bounds.z0);
                      bounds.x1 = MAX(position->x, bounds.x1);
                      bounds.y1 = MAX(position->y, bounds.y1);
                      bounds.z1 = MAX(position->z, bounds.z1);
                    }
                  }

                  centroid.x = (bounds.x0 + bounds.x1) * 0.5f;
                  centroid.y = (bounds.y0 + bounds.y1) * 0.5f;
                  centroid.z = (bounds.z0 + bounds.z1) * 0.5f;

                  if (definition->shaders.count > 0)
                    shader = tag_block_get_element(
                        &definition->shaders, 0,
                        sizeof(shader_effect_definition));
                  else
                    shader = &global_shader_effect_additive;

                  rasterizer_dynamic_vertices_unlock(vertex_buffer_index);
                  FUN_0017cf60((uint32_t)shader, (uint32_t)bitmap,
                               (int)animation, -2 * point_count,
                               vertex_buffer_index, 2 * point_count - 2,
                               &centroid.x, 0);
                  rasterizer_dynamic_vertices_delete(vertex_buffer_index);
                }
                first_marker = true;
              }
              rasterizer_globals.current_lock_operation = _rasterizer_lock_none;
            } else {
              lightning_marker_definition *next_marker_definition =
                  tag_block_get_element(&definition->markers, marker_index + 1,
                                        sizeof(lightning_marker_definition));
              int16_t segment_point_count =
                  1 << marker_definition->octaves_to_next_marker;
              int16_t octaves = marker_definition->octaves_to_next_marker;
              real scale = 1.0f;
              real_vector3d up;
              int16_t octave_index;

              assert_halt_msg_at(
                  "!points[point_count+segment_point_count].valid",
                  LIGHTNING_FILE, 383,
                  !points[point_count + segment_point_count].valid);

              object_get_marker_by_name(
                  object_index, next_marker_definition->attachment_marker,
                  &marker, 1);
              points[point_count + segment_point_count].position =
                  marker.matrix.position;
              lightning_offset_marker_position(
                  &marker.matrix,
                  &points[point_count + segment_point_count].position,
                  &next_marker_definition->random_position_bounds);
              points[point_count + segment_point_count].width =
                  next_marker_definition->thickness;
              points[point_count + segment_point_count].color =
                  next_marker_definition->tint;
              points[point_count + segment_point_count].valid = true;

              vector_from_points3d(
                  &points[point_count].position,
                  &points[point_count + segment_point_count].position, &up);
              { /* cross_product3d(&up, &render.camera.forward, &up), inlined */
                real k = up.i * render.camera.forward.j - up.j * render.camera.forward.i;
                real j = up.k * render.camera.forward.i - up.i * render.camera.forward.k;
                real i = up.j * render.camera.forward.k - up.k * render.camera.forward.j;
                up.i = i;
                up.j = j;
                up.k = k;
              }
              if (normalize3d(&up.i) == 0.0f) {
                up.i = global_z_axis3d->i;
                up.j = global_z_axis3d->j;
                up.k = global_z_axis3d->k;
              }

              for (octave_index = 1; octave_index <= octaves; octave_index++) {
                int16_t segment_point_start_index = 1 << (octaves - octave_index);
                int16_t segment_point_index_increment =
                    2 * segment_point_start_index;
                int16_t segment_point_index;

                assert_halt_msg_at("segment_point_start_index>0",
                                   LIGHTNING_FILE, 408,
                                   segment_point_start_index > 0);

                for (segment_point_index = segment_point_start_index;
                     segment_point_index < segment_point_count;
                     segment_point_index += segment_point_index_increment) {
                  intermediate_lightning_point *previous_point =
                      &points[point_count + segment_point_index -
                              segment_point_start_index];
                  intermediate_lightning_point *next_point =
                      &points[point_count + segment_point_index +
                              segment_point_start_index];
                  intermediate_lightning_point *point =
                      &points[point_count + segment_point_index];
                  real segment_fraction =
                      (real)segment_point_index / segment_point_count;
                  real jitter_offset =
                      (segment_fraction *
                           (next_marker_definition->random_jitter_offset -
                            marker_definition->random_jitter_offset) +
                       marker_definition->random_jitter_offset) *
                      scale * jitter_scale;
                  real jitter;
                  real_vector3d offset;

                  assert_halt_msg_at(
                      "!points[point_count+segment_point_index].valid",
                      LIGHTNING_FILE, 426,
                      !points[point_count + segment_point_index].valid);

                  jitter = random_real_range(
                               random_math_get_local_seed_address(),
                               -1.0f, 1.0f) *
                           jitter_offset;
                  scale_vector3d(&up, jitter, &offset);
                  set_real_point3d_inline(
                      &point->position,
                      (previous_point->position.x + next_point->position.x) *
                              0.5f +
                          offset.i,
                      (previous_point->position.y + next_point->position.y) *
                              0.5f +
                          offset.j,
                      (previous_point->position.z + next_point->position.z) *
                              0.5f +
                          offset.k);
                  point->width = (previous_point->width + next_point->width) * 0.5f;
                  point->color.alpha =
                      (previous_point->color.alpha + next_point->color.alpha) *
                      0.5f;
                  point->color.red =
                      (previous_point->color.red + next_point->color.red) * 0.5f;
                  point->color.green =
                      (previous_point->color.green + next_point->color.green) *
                      0.5f;
                  point->color.blue =
                      (previous_point->color.blue + next_point->color.blue) *
                      0.5f;
                  point->valid = true;
                }

                scale *= 0.5f;
              }

              point_count += 1 << marker_definition->octaves_to_next_marker;
            }
          }
        }
      }
    }
  }
}
