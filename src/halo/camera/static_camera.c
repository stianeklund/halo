#include "x87_math.h"

/* 0x8d410 (static_camera.obj) */
void FUN_0008d410(void *camera, int action, void *result)
{
  char *camera_bytes;
  char *result_bytes;
  char *source;
  float timer;
  float value;

  if (camera == NULL) {
    display_assert("camera", "c:\\halo\\SOURCE\\camera\\static_camera.c", 0x24,
                   true);
    system_exit(-1);
  }
  if (action == 0) {
    display_assert("action", "c:\\halo\\SOURCE\\camera\\static_camera.c", 0x25,
                   true);
    system_exit(-1);
  }
  if (result == NULL) {
    display_assert("result", "c:\\halo\\SOURCE\\camera\\static_camera.c", 0x26,
                   true);
    system_exit(-1);
  }

  camera_bytes = (char *)camera;
  result_bytes = (char *)result;
  if (*(uint8_t *)(camera_bytes + 0x34) == 0) {
    *(vector3_t *)(result_bytes + 0x04) = *(vector3_t *)(camera_bytes + 0x00);
    *(vector3_t *)(result_bytes + 0x24) = *(vector3_t *)(camera_bytes + 0x10);
    *(vector3_t *)(result_bytes + 0x30) = *(vector3_t *)(camera_bytes + 0x1c);
    *(uint32_t *)(result_bytes + 0x20) = *(uint32_t *)(camera_bytes + 0x28);

    timer = (float)*(int *)(camera_bytes + 0x2c);
    *(uint32_t *)(result_bytes + 0x44) = 0;
    *(uint32_t *)(result_bytes + 0x40) = 0;
    *(float *)(result_bytes + 0x48) = timer;
    *(uint32_t *)(result_bytes + 0x3c) = 0;
    *(uint32_t *)result_bytes = *(uint32_t *)(camera_bytes + 0x30) | 1;

    source = *(char **)0x31fc38;
    *(vector3_t *)(result_bytes + 0x10) = *(vector3_t *)source;
    *(uint8_t *)(camera_bytes + 0x34) = 1;

    if ((*(uint32_t *)result_bytes & 1) != 0) {
      if (!valid_real_normal3d_perpendicular((float *)(result_bytes + 0x24),
                                             (float *)(result_bytes + 0x30)) ||
          ((value = *(float *)(result_bytes + 0x04)),
           ((*(uint32_t *)&value & 0x7f800000) == 0x7f800000)) ||
          !(*(float *)(result_bytes + 0x04) >= -5000.0f) ||
          !(*(float *)(result_bytes + 0x04) <= 5000.0f) ||
          ((value = *(float *)(result_bytes + 0x08)),
           ((*(uint32_t *)&value & 0x7f800000) == 0x7f800000)) ||
          !(*(float *)(result_bytes + 0x08) >= -5000.0f) ||
          !(*(float *)(result_bytes + 0x08) <= 5000.0f) ||
          ((value = *(float *)(result_bytes + 0x0c)),
           ((*(uint32_t *)&value & 0x7f800000) == 0x7f800000)) ||
          !(*(float *)(result_bytes + 0x0c) >= -5000.0f) ||
          !(*(float *)(result_bytes + 0x0c) <= 5000.0f) ||
          ((value = *(float *)(result_bytes + 0x10)),
           ((*(uint32_t *)&value & 0x7f800000) == 0x7f800000)) ||
          !(*(float *)(result_bytes + 0x10) >= -5000.0f) ||
          !(*(float *)(result_bytes + 0x10) <= 5000.0f) ||
          ((value = *(float *)(result_bytes + 0x14)),
           ((*(uint32_t *)&value & 0x7f800000) == 0x7f800000)) ||
          !(*(float *)(result_bytes + 0x14) >= -5000.0f) ||
          !(*(float *)(result_bytes + 0x14) <= 5000.0f) ||
          ((value = *(float *)(result_bytes + 0x18)),
           ((*(uint32_t *)&value & 0x7f800000) == 0x7f800000)) ||
          !(*(float *)(result_bytes + 0x18) >= -5000.0f) ||
          !(*(float *)(result_bytes + 0x18) <= 5000.0f) ||
          (uint8_t)real_vector3d_valid((float *)(result_bytes + 0x3c)) == 0 ||
          ((value = *(float *)(result_bytes + 0x1c)),
           ((*(uint32_t *)&value & 0x7f800000) == 0x7f800000)) ||
          !(*(float *)(result_bytes + 0x1c) >= 0.0f) ||
          !(*(float *)(result_bytes + 0x1c) <= 5000.0f) ||
          ((value = *(float *)(result_bytes + 0x20)),
           ((*(uint32_t *)&value & 0x7f800000) == 0x7f800000)) ||
          !(*(float *)(result_bytes + 0x20) >= 0.001f) ||
          !(*(float *)(result_bytes + 0x20) <= 1.57079637f) ||
          ((value = *(float *)(result_bytes + 0x48)),
           ((*(uint32_t *)&value & 0x7f800000) == 0x7f800000)) ||
          !(*(float *)(result_bytes + 0x48) >= 0.0f) ||
          !(*(float *)(result_bytes + 0x48) <= 3600.0f)) {
        display_assert(
          csprintf((char *)0x5ab100,
                   "Invalid camera command.\n"
                   "F: (%f, %f, %f) U: (%f, %f, %f)\n"
                   "P: (%f, %f, %f) O: (%f, %f, %f)\n"
                   "D: %f V: (%f, %f, %f), FOV: %f, T: %f, FL: %ld",
                   (double)*(float *)(result_bytes + 0x24),
                   (double)*(float *)(result_bytes + 0x28),
                   (double)*(float *)(result_bytes + 0x2c),
                   (double)*(float *)(result_bytes + 0x30),
                   (double)*(float *)(result_bytes + 0x34),
                   (double)*(float *)(result_bytes + 0x38),
                   (double)*(float *)(result_bytes + 0x04),
                   (double)*(float *)(result_bytes + 0x08),
                   (double)*(float *)(result_bytes + 0x0c),
                   (double)*(float *)(result_bytes + 0x10),
                   (double)*(float *)(result_bytes + 0x14),
                   (double)*(float *)(result_bytes + 0x18),
                   (double)*(float *)(result_bytes + 0x1c),
                   (double)*(float *)(result_bytes + 0x3c),
                   (double)*(float *)(result_bytes + 0x40),
                   (double)*(float *)(result_bytes + 0x44),
                   (double)*(float *)(result_bytes + 0x20),
                   (double)*(float *)(result_bytes + 0x48),
                   (long)*(uint32_t *)result_bytes),
          "c:\\halo\\SOURCE\\camera\\static_camera.c", 0x35, true);
        system_exit(-1);
      }
    }
  }
}
