#include "camera_internal.h"

/* First-person camera: the camera mode that looks out of a unit's eyes
 * (or a vehicle seat's marker).  Address range 0x88c40..0x89330. */

void first_person_camera_new(void *camera)
{
  first_person_camera_t *first_person_camera;

  assert_halt_at("c:\\halo\\SOURCE\\camera\\first_person_camera.c", 0x18,
                 camera);

  first_person_camera = (first_person_camera_t *)camera;
  first_person_camera->field_of_view = 0.0f;
}

/* 0x88c80: the eye position and forward vector of a unit; a seat flagged
 * _unit_seat_first_person_camera_bit takes both from the vehicle's
 * "primary trigger" marker. */
void FUN_00088c80(int unit_handle, float *out_position, float *out_forward)
{
  unit_data_t *unit;
  object_data_t *vehicle;
  unit_definition_t *definition;
  unit_seat_t *seat;
  object_marker marker;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle,
                                                   _object_mask_unit);
  unit_get_camera_position(unit_handle, out_position);
  *(real_vector3d *)out_forward = unit->aiming_vector;

  if (unit->object.parent_object_index.value != NONE) {
    vehicle = (object_data_t *)object_try_and_get_and_verify_type(
      unit->object.parent_object_index.value, _object_mask_vehicle);
    if (vehicle != NULL) {
      definition =
        (unit_definition_t *)tag_get(TAG_GROUP_VEHI, vehicle->tag_index);
      seat = (unit_seat_t *)tag_block_get_element(
        &definition->seats, (short)unit->parent_seat_index, sizeof(unit_seat_t));

      if (seat->flags & FLAG(_unit_seat_first_person_camera_bit)) {
        if (object_get_marker_by_name(unit->object.parent_object_index.value,
                                      "primary trigger", &marker, 1)) {
          *(real_point3d *)out_position = marker.matrix.position;
          *(real_vector3d *)out_forward =
            *(real_vector3d *)&marker.matrix.forward;
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
          &definition->seats, (short)unit->parent_seat_index,
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

/* 0x89240 */
void first_person_camera_fake(int unit_index, void *result)
{
  unit_data_t *unit;

  unit = (unit_data_t *)object_get_and_verify_type(unit_index,
                                                   _object_mask_unit);
  first_person_camera_for_unit_and_vector((float *)&unit->aiming_vector,
                                          unit_index, result);
}

/* 0x89270.  A field-of-view change blends in over 0.18 seconds through the
 * command's field-of-view transition slot. */
void first_person_camera_update(void *camera, void *action, void *result)
{
  real_vector3d facing_direction;
  int32_t unit_index;
  real field_of_view;
  camera_command_t *command = (camera_command_t *)result;

  unit_index = player_control_get_unit_index(((first_person_camera_action_t *)action)->local_player_index);
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

  player_control_get_facing_direction(((first_person_camera_action_t *)action)->local_player_index,
                                      (float *)&facing_direction);
  first_person_camera_for_unit_and_vector((float *)&facing_direction,
                                          unit_index, result);
  field_of_view =
    player_control_get_field_of_view(((first_person_camera_action_t *)action)->local_player_index);

  command->field_of_view = field_of_view;
  if (field_of_view != ((first_person_camera_t *)camera)->field_of_view) {
    command->field_54[3] = 0.18f;
    command->field_4c[3] = true;
    ((first_person_camera_t *)camera)->field_of_view = field_of_view;
  }
}
