#include "camera_internal.h"

/* 0x8d410 (static_camera.obj) */
void static_camera_update(void *camera, const void *action, void *result)
{
  static_camera_t *static_camera;
  camera_command_t *command;

  if (camera == NULL) {
    display_assert("camera", "c:\\halo\\SOURCE\\camera\\static_camera.c", 0x24,
                   true);
    system_exit(-1);
  }
  if (action == NULL) {
    display_assert("action", "c:\\halo\\SOURCE\\camera\\static_camera.c", 0x25,
                   true);
    system_exit(-1);
  }
  if (result == NULL) {
    display_assert("result", "c:\\halo\\SOURCE\\camera\\static_camera.c", 0x26,
                   true);
    system_exit(-1);
  }

  static_camera = (static_camera_t *)camera;
  command = (camera_command_t *)result;
  if (!static_camera->initialized) {
    command->position = static_camera->position;
    command->forward = static_camera->forward;
    command->up = static_camera->up;
    command->field_of_view = static_camera->field_of_view;
    command->timer = (real)static_camera->timer;
    command->velocity.i = command->velocity.j = command->velocity.k = 0.0f;
    command->flags = static_camera->flags | FLAG(_observer_command_valid_bit);
    command->offset = *(real_vector3d *)global_zero_vector_ptr;
    static_camera->initialized = true;

    if (!camera_command_valid(command)) {
      display_assert(csprintf(error_string_buffer, CAMERA_COMMAND_INVALID_FORMAT,
                              CAMERA_COMMAND_INVALID_ARGUMENTS(command)),
                     "c:\\halo\\SOURCE\\camera\\static_camera.c", 0x35, true);
      system_exit(-1);
    }
  }
}
