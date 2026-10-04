#include "camera_internal.h"

/* First-person camera: the camera mode that looks out of a unit's eyes
 * (or a vehicle seat's marker).  Address range 0x88c40..0x89330. */

/* first_person_camera_new (0x88c40) — reset the first-person camera data
 * block.  Ghidra types this void(void) and reports the stack parameter as
 * in_stack_00000004; the disassembly loads it at [EBP+8] into ESI
 * (MOV ESI,[EBP+8] at 0x88c44), so it is a single cdecl pointer parameter.
 *
 * The null check at 0x88c47 (TEST ESI,ESI / JNZ 0x88c68) is an assert: the
 * pushed arguments at 0x88c4b-0x88c54 are display_assert("camera",
 * "c:\halo\SOURCE\camera\first_person_camera.c", 0x18, true) followed by
 * system_exit(-1) at 0x88c60.  The .rdata reason string is "camera", so the
 * original condition was written on a parameter of that name; the assert is
 * stamped with the first_person_camera.c TU, not director.c, so the file and
 * line are pinned with assert_halt_at.
 *
 * The body is a single dword store of 0 (MOV [ESI],0x0 at 0x88c68); the width
 * is a dword, and nothing else in the block is touched here. */
void first_person_camera_new(void *camera)
{
  assert_halt_at("c:\\halo\\SOURCE\\camera\\first_person_camera.c", 0x18,
                 camera);

  *(uint32_t *)camera = 0;
}

/* FUN_00088c80 (0x88c80) — produce the first-person camera's eye position and
 * forward vector for a unit.
 *
 * Ghidra types this void(void) and reports the three cdecl arguments as
 * in_stack_00000004/8/c; the disassembly loads them at [EBP+8] (EDI, then
 * ESI after the first call), [EBP+0xc] (EBX) and [EBP+0x10] (EDI) at
 * 0x88c89/0x88c94/0x88ca0, so it is a three-parameter cdecl function.
 *
 * Baseline: unit_get_camera_position fills the caller's position vector, and
 * the unit's own aiming vector at +0x1ec..+0x1f4 is copied out as the forward
 * vector.  Both copies are plain dword moves in the reference
 * (MOV EDX,[EAX] / MOV [ECX],EDX at 0x88ca9..0x88cb8), so they are spelled as
 * dword copies here rather than float assignments.
 *
 * Override: if the unit is riding something (+0xcc is a valid object handle,
 * type mask 2), the vehicle's seat definition is fetched from the 'vehi'
 * definition's seat block at +0x2e4, indexed by the unit's seat index at
 * +0x2a0 with element size 0x11c.  The reference reads only the low byte of
 * the seat definition and branches on its sign (MOV CL,[EAX] / TEST CL,CL /
 * JNS at 0x88cfd..0x88d04) — a seat flag whose bit 7 selects the marker-driven
 * camera.  In that case the "primary trigger" marker on the vehicle supplies
 * both vectors: the marker record's forward vector at +0x3c and its position
 * at +0x60, matching the marker layout used by player_control (0x6c-byte
 * record, one marker requested).
 *
 * Note the two halves are written to opposite parameters: the marker position
 * (+0x60, read at [EBP-0xc]) goes to the EBX parameter that
 * unit_get_camera_position filled, and the marker forward (+0x3c, read at
 * [EBP-0x30]) goes to the EDI parameter that received the unit's aiming vector.
 */
void FUN_00088c80(int unit_handle, float *out_position, float *out_forward)
{
  char *unit;
  char *vehicle;
  char *seat;
  char marker_buf[0x6c]; /* object_get_marker_by_name output */

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  unit_get_camera_position(unit_handle, out_position);

  ((uint32_t *)out_forward)[0] = *(uint32_t *)(unit + 0x1ec);
  ((uint32_t *)out_forward)[1] = *(uint32_t *)(unit + 0x1f0);
  ((uint32_t *)out_forward)[2] = *(uint32_t *)(unit + 0x1f4);

  if (*(int *)(unit + 0xcc) != NONE) {
    vehicle =
      (char *)object_try_and_get_and_verify_type(*(int *)(unit + 0xcc), 2);
    if (vehicle != NULL) {
      /* one nested expression: the original cleans both calls with a single
       * ADD ESP,0x14 at 0x88cff */
      seat = (char *)tag_block_get_element(
        (char *)tag_get(0x76656869 /* 'vehi' */, *(int *)vehicle) + 0x2e4,
        *(int16_t *)(unit + 0x2a0), 0x11c);

      if (*seat < 0) {
        if (object_get_marker_by_name(*(int *)(unit + 0xcc),
                                      (void *)"primary trigger", marker_buf,
                                      1) != 0) {
          ((uint32_t *)out_position)[0] = *(uint32_t *)(marker_buf + 0x60);
          ((uint32_t *)out_position)[1] = *(uint32_t *)(marker_buf + 0x64);
          ((uint32_t *)out_position)[2] = *(uint32_t *)(marker_buf + 0x68);
          ((uint32_t *)out_forward)[0] = *(uint32_t *)(marker_buf + 0x3c);
          ((uint32_t *)out_forward)[1] = *(uint32_t *)(marker_buf + 0x40);
          ((uint32_t *)out_forward)[2] = *(uint32_t *)(marker_buf + 0x44);
        }
      }
    }
  }
}

/* Build a first-person camera command for a unit looking along a vector
 * (0x88d50). Register ABI: vector @<eax>, unit_index @<ecx>, result @<esi>
 * (callers 0x8925d first_person_camera_fake, 0x892f0
 * first_person_camera_update).
 * [TU: c:\halo\SOURCE\camera\first_person_camera.c -- asserts at 0x52/0x85]
 * The command starts invalid at a 70-degree field of view looking along
 * vector, offset from the global zero vector.  With a unit, the position
 * comes from unit_get_camera_position; when the unit sits in a vehicle the
 * seat either snaps to the vehicle's "primary trigger" marker
 * (_unit_seat_first_person_camera_bit) or rotates forward/up into the
 * vehicle's frame.  The finished command is checked with
 * camera_command_valid. */
void first_person_camera_for_unit_and_vector(float *vector, int32_t unit_index,
                                             void *result)
{
  camera_command_t *command = (camera_command_t *)result;
  unit_data_t *unit;
  object_data_t *vehicle;
  unit_definition_t *definition;
  unit_seat_t *seat;
  real_matrix4x3 matrix;
  object_marker marker;

  command->timer = 0.0f;
  command->flags = 0;
  command->offset = *(real_vector3d *)global_zero_vector_ptr;
  command->depth = 0.0f;
  command->forward = *(real_vector3d *)vector;
  command->field_of_view = 1.22173047f; /* 70 degrees */
  observer_up_from_forward((float *)&command->forward, (float *)&command->up);
  if (!valid_real_normal3d_perpendicular((float *)&command->forward,
                                         (float *)&command->up)) {
    display_assert("valid_real_vector3d_axes2(&result->forward, &result->up)",
                   "c:\\halo\\SOURCE\\camera\\first_person_camera.c", 0x52, 1);
    system_exit(-1);
  }

  if (unit_index != -1) {
    unit =
      (unit_data_t *)object_get_and_verify_type(unit_index, _object_mask_unit);
    unit_get_camera_position(unit_index, (float *)&command->position);
    object_get_root_location(unit_index, (float *)&command->velocity, NULL);
    if (unit->object.parent_object_index.value != -1) {
      vehicle = (object_data_t *)object_try_and_get_and_verify_type(
        unit->object.parent_object_index.value, _object_mask_vehicle);
      if (vehicle != NULL) {
        definition =
          (unit_definition_t *)tag_get(TAG_GROUP_VEHI, vehicle->tag_index);
        seat = (unit_seat_t *)tag_block_get_element(
          &definition->seats, (short)unit->unk_672 /* parent seat index */,
          sizeof(unit_seat_t));
        if (seat->flags & FLAG(_unit_seat_first_person_camera_bit)) {
          if (object_get_marker_by_name(unit->object.parent_object_index.value,
                                        "primary trigger", &marker, 1)) {
            command->position = marker.matrix.position;
            command->forward = *(real_vector3d *)&marker.matrix.forward;
            command->up = *(real_vector3d *)&marker.matrix.up;
          }
        } else {
          /* in-place transforms: 0x88edc/0x88ef4 PUSH EDI twice, 0x88f02
           * PUSH EBX twice (same buffer as input and output) */
          matrix4x3_from_forward_up_position(
            &matrix, (float *)&vehicle->position, (float *)&vehicle->forward,
            (float *)&vehicle->up);
          real_matrix4x3_transform_point(
            &matrix, &command->forward,
            &command->forward); /* dup-args-ok: in-place */
          observer_up_from_forward((float *)&command->forward,
                                   (float *)&command->up);
          matrix_transform_vector(
            (float *)&matrix, (float *)&command->forward,
            (float *)&command->forward); /* dup-args-ok: in-place */
          matrix_transform_vector(
            (float *)&matrix, (float *)&command->up,
            (float *)&command->up); /* dup-args-ok: in-place */
        }
      }
    }
    command->flags = FLAG(_observer_command_valid_bit);
  }

  if (!camera_command_valid(command)) {
    display_assert(csprintf(error_string_buffer, CAMERA_COMMAND_INVALID_FORMAT,
                            CAMERA_COMMAND_INVALID_ARGUMENTS(command)),
                   "c:\\halo\\SOURCE\\camera\\first_person_camera.c", 0x85, 1);
    system_exit(-1);
  }
}

/* Camera for a unit looking along the unit's own vector at +0x1ec (0x89240). */
void first_person_camera_fake(int unit_index, void *result)
{
  char *unit;

  unit = (char *)object_get_and_verify_type(unit_index, _object_mask_unit);
  first_person_camera_for_unit_and_vector((float *)(unit + 0x1ec), unit_index,
                                          result);
}


/* Per-tick update for the first-person camera mode (0x89270).
 * [TU: c:\halo\SOURCE\camera\first_person_camera.c — __FILE__ assert xref]
 * param_2 points at a block whose leading int16_t is the local player index
 * (the only field this function touches); the rest of its meaning is unproven.
 * The camera block's leading float is the previously-applied field of view: on
 * any change the result block is told to blend (0x3e3851ec == 0.18f at +0x60,
 * flag byte at +0x4f) and the new value is written back. */
void first_person_camera_update(void *camera, void *param_2, void *result)
{
  float forward[3];
  int32_t unit_index;
  float field_of_view;

  unit_index = player_control_get_unit_index(*(int16_t *)param_2);
  if (camera == NULL) {
    display_assert("camera", "c:\\halo\\SOURCE\\camera\\first_person_camera.c",
                   0x9d, 1);
    system_exit(-1);
  }
  if (result == NULL) {
    display_assert("result", "c:\\halo\\SOURCE\\camera\\first_person_camera.c",
                   0x9e, 1);
    system_exit(-1);
  }

  player_control_get_facing_direction(*(int16_t *)param_2, forward);
  first_person_camera_for_unit_and_vector(forward, unit_index, result);
  field_of_view = player_control_get_field_of_view(*(int16_t *)param_2);

  *(float *)((char *)result + 0x20) = field_of_view;
  if (field_of_view != *(float *)camera) {
    *(int *)((char *)result + 0x60) = 0x3e3851ec;
    *(uint8_t *)((char *)result + 0x4f) = 1;
    *(float *)camera = field_of_view;
  }
}
