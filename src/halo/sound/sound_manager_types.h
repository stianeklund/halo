/* Listener and sound-source layouts used by sound_manager.c.
 *
 * Repo-local header: the original header path is not binary-proven.  Kept out
 * of types.h because struct definitions there move unrelated VC71 code.
 * Evidence tables: recovery/evidence/sound_listener.json,
 * recovery/evidence/sound_source.json. */

#ifndef HALO_SOUND_SOUND_MANAGER_TYPES_H
#define HALO_SOUND_SOUND_MANAGER_TYPES_H

#include "../../types.h"

/* sound_source.spatialization_mode.  Names are the identifiers in the
 * binary's assert strings; mode 2 (location already listener-relative) has no
 * string name. */
enum {
  _sound_spatialization_mode_none = 0,
  _sound_spatialization_mode_absolute = 1
};

/* One slot of the 4-entry local-player listener table at 0x4eaf58. */
typedef struct sound_listener {
  int8_t valid;          ///< offset=0x00
  uint8_t field_01;      ///< offset=0x01
  uint8_t pad_02[2];     ///< offset=0x02
  real_matrix4x3 matrix; ///< offset=0x04
  real_vector3d field_38; ///< offset=0x38
} sound_listener;
cs(sound_listener, 0x44);
co(sound_listener, valid, 0x00);
co(sound_listener, field_01, 0x01);
co(sound_listener, matrix, 0x04);
co(sound_listener, field_38, 0x38);

typedef struct sound_source {
  int16_t spatialization_mode; ///< offset=0x00
  uint8_t pad_02[2];           ///< offset=0x02
  real field_04;               ///< offset=0x04
  real field_08;               ///< offset=0x08
  struct {
    real_point3d position;     ///< offset=0x0c
    real_vector3d forward;     ///< offset=0x18
  } location;
  uint8_t pad_24[16];          ///< offset=0x24
  uint16_t field_34;           ///< offset=0x34
  uint8_t pad_36[2];           ///< offset=0x36
  real field_38;               ///< offset=0x38
  real field_3c;               ///< offset=0x3c
} sound_source;
cs(sound_source, 0x40);
co(sound_source, spatialization_mode, 0x00);
co(sound_source, field_04, 0x04);
co(sound_source, field_08, 0x08);
co(sound_source, location, 0x0c);
co(sound_source, location.forward, 0x18);
co(sound_source, field_34, 0x34);
co(sound_source, field_38, 0x38);
co(sound_source, field_3c, 0x3c);

#endif
