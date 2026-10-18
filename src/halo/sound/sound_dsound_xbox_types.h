/* DirectSound channel and stream layouts used by sound_dsound_xbox.c.
 *
 * Repo-local header: the original header path is not binary-proven.  Kept out
 * of types.h because struct definitions there move unrelated VC71 code. */

#ifndef HALO_SOUND_SOUND_DSOUND_XBOX_TYPES_H
#define HALO_SOUND_SOUND_DSOUND_XBOX_TYPES_H

#include "../../types.h"

/* XDK constants, guarded because this TU does not include dsound.h.  Values
 * match third_party/xbox/dsoundtypes.h; WAVE_FORMAT_PCM is the standard tag. */
#ifndef WAVE_FORMAT_PCM
#define WAVE_FORMAT_PCM 0x0001
#endif
#ifndef WAVE_FORMAT_XBOADPCM
#define WAVE_FORMAT_XBOADPCM 0x0069
#endif
#ifndef XDSMIXBIN_FRONT_LEFT
#define XDSMIXBIN_FRONT_LEFT 0
#define XDSMIXBIN_FRONT_RIGHT 1
#define XDSMIXBIN_FRONT_CENTER 2
#define XDSMIXBIN_BACK_LEFT 4
#define XDSMIXBIN_BACK_RIGHT 5
#endif

/* Format block passed to IDirectSound_CreateSoundStream through
 * DSSTREAMDESC.lpwfxFormat: a WAVEFORMATEX followed by the Xbox ADPCM
 * samples-per-block word (the XBOXADPCMWAVEFORMAT layout).  PCM streams leave
 * the last two fields unwritten. */
typedef struct dsound_stream_wave_format {
  uint16_t format_tag;               ///< offset=0x00
  uint16_t channel_count;            ///< offset=0x02
  uint32_t samples_per_second;       ///< offset=0x04
  uint32_t average_bytes_per_second; ///< offset=0x08
  uint16_t block_align;              ///< offset=0x0c
  uint16_t bits_per_sample;          ///< offset=0x0e
  uint16_t extra_size;               ///< offset=0x10
  uint16_t samples_per_block;        ///< offset=0x12
} dsound_stream_wave_format;
cs(dsound_stream_wave_format, 0x14);
co(dsound_stream_wave_format, samples_per_second, 0x04);
co(dsound_stream_wave_format, block_align, 0x0c);
co(dsound_stream_wave_format, extra_size, 0x10);
co(dsound_stream_wave_format, samples_per_block, 0x12);

typedef void(__stdcall *dsound_stream_callback)(void *stream_context,
                                                void *packet_context,
                                                unsigned int status);

/* Same layout and field order as the XDK DSSTREAMDESC (not included here: this
 * TU only sees kb-generated prototypes). */
typedef struct dsound_stream_desc {
  uint32_t flags;                       ///< offset=0x00
  uint32_t max_attached_packets;        ///< offset=0x04
  dsound_stream_wave_format *wave_format; ///< offset=0x08
  dsound_stream_callback callback;      ///< offset=0x0c
  void *context;                        ///< offset=0x10
  void *mix_bins_output;                ///< offset=0x14
} dsound_stream_desc;
cs(dsound_stream_desc, 0x18);
co(dsound_stream_desc, wave_format, 0x08);
co(dsound_stream_desc, callback, 0x0c);
co(dsound_stream_desc, context, 0x10);

/* 0x2c-byte block handed to dsound_channel_set_location.  Only the vector
 * dsound_initialize_channel fills is named; the callee reads further fields. */
typedef struct dsound_channel_location {
  uint8_t pad_00[0x0c];
  real_vector3d field_0c; ///< offset=0x0c
  uint8_t pad_18[0x14];
} dsound_channel_location;
cs(dsound_channel_location, 0x2c);
co(dsound_channel_location, field_0c, 0x0c);

/* One element of the actual-channel array returned by
 * sound_dsound_channel_get (element size 0x74).  Only the fields
 * dsound_initialize_channel touches are named. */
typedef struct dsound_channel {
  uint8_t pad_00[2];
  int16_t field_02;    ///< offset=0x02, reset to -1
  uint8_t pad_04[2];
  uint8_t field_06;    ///< offset=0x06, reset to 0
  uint8_t pad_07[0x31];
  int16_t type_flags;  ///< offset=0x38, the type_flags argument verbatim
  uint8_t pad_3a[0x2e];
  int32_t field_68;    ///< offset=0x68, reset to 0
  int32_t field_6c;    ///< offset=0x6c, reset to 0
  void *stream;        ///< offset=0x70, receives the created sound stream
} dsound_channel;
cs(dsound_channel, 0x74);
co(dsound_channel, field_02, 0x02);
co(dsound_channel, field_06, 0x06);
co(dsound_channel, type_flags, 0x38);
co(dsound_channel, field_68, 0x68);
co(dsound_channel, field_6c, 0x6c);
co(dsound_channel, stream, 0x70);

#endif
