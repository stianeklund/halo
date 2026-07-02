/* render_debug.c -- debug primitive rendering
 * (c:\halo\SOURCE\render\render_debug.c)
 *
 * The debug renderer keeps a per-frame cache of debug primitives (points,
 * lines, boxes, text, ...) submitted through the cache writer at 0x188ec0.
 * Each cache record is 0x38 (56) bytes; the leading short is the primitive
 * type. render_debug (0x18ac50) flushes the cache once per frame: it runs the
 * fixed set of debug sub-renderers, walks the cache dispatching each record to
 * its draw routine, then clears the cache when the game frame advances.
 */

/* Per-frame debug primitive cache record (0x38 bytes, array based at 0x4d1220).
 * The payload is a tagged union keyed by `type`; individual offsets are reused
 * per primitive kind, so the fields carry raw-offset names. */
typedef struct debug_primitive {
  short type; /* +0x00 primitive type (0..9)          */
  short pad02; /* +0x02                                */
  float f04; /* +0x04                                */
  float f08; /* +0x08                                */
  float f0c; /* +0x0c                                */
  float f10; /* +0x10                                */
  unsigned short s14; /* +0x14                                */
  unsigned char b16; /* +0x16                                */
  unsigned char pad17; /* +0x17                                */
  float f18; /* +0x18                                */
  float f1c; /* +0x1c                                */
  float f20; /* +0x20                                */
  float f24; /* +0x24                                */
  float f28; /* +0x28                                */
  float f2c; /* +0x2c                                */
  float f30; /* +0x30                                */
  float f34; /* +0x34                                */
} debug_primitive; /* sizeof == 0x38 */

typedef char
  debug_primitive_size_check[sizeof(debug_primitive) == 0x38 ? 1 : -1];

#define debug_primitives ((debug_primitive *)0x4d1220)
#define debug_primitive_count (*(short *)0x4d8224)
#define debug_primitive_frame (*(short *)0x4d8220)

/* Per-frame debug string arena: char[0x400] at 0x4d0e20, ending at 0x4d121f
 * (immediately before the primitive cache at 0x4d1220). Text primitives intern
 * their string here; the cursor at 0x4d8228 counts bytes used (max 0x3ff). */
#define debug_string_pool ((char *)0x4d0e20)
#define debug_string_pool_count (*(short *)0x4d8228)
#define debug_string_overflow_warned (*(char *)0x4d822b)

/* Intern a string into the per-frame debug string arena (0x188b20). Scans the
 * arena for an identical string already present and returns its address;
 * otherwise appends a null-terminated copy and advances the cursor. Returns
 * NULL (with a one-shot overflow warning) when the arena is full. The source
 * string is passed in EDI. */
char *FUN_00188b20(const char *str)
{
  char *result;
  short i;
  char *dst;
  short added;

  result = 0;
  i = 0;
  if (debug_string_pool_count > 0) {
    do {
      if (csstrcmp(str, &debug_string_pool[i]) == 0) {
        result = &debug_string_pool[i];
        if (result != 0) {
          goto done;
        }
        break;
      }
      i++;
    } while (i < debug_string_pool_count);
  }

  if (debug_string_pool_count < 0x3ff) {
    dst = &debug_string_pool[debug_string_pool_count];
    csstrncpy(dst, str, 0x3ff - debug_string_pool_count);
    debug_string_pool[0x3ff] = 0;
    added = (short)csstrlen(str);
    debug_string_pool_count = debug_string_pool_count + added + 1;
    if (debug_string_pool_count > 0x3ff) {
      debug_string_pool_count = 0x3ff;
    }
    return dst;
  }

  if (debug_string_overflow_warned == 0) {
    error(2, "render debug cache string overflow");
    debug_string_overflow_warned = 1;
  }
done:
  return result;
}

/* Render a debug bounding box (0x18ab30). With wireframe set, expand the six
 * min/max bounds {x0,x1,y0,y1,z0,z1} into the eight box corners and draw the
 * two z-faces as line loops plus the four vertical edges; otherwise submit a
 * solid box primitive (type 7) to the cache. */
void FUN_0018ab30(char wireframe, float *bounds, void *color)
{
  float v[24];
  float *p;
  int i;

  if (bounds == 0) {
    display_assert("bounds", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x32c,
                   1);
    system_exit(-1);
  }
  if (color == 0) {
    display_assert("color", "c:\\halo\\SOURCE\\render\\render_debug.c", 0x32d,
                   1);
    system_exit(-1);
  }

  if (wireframe != 0) {
    /* Eight corners, three floats each, grouped as two z-faces of four. */
    v[0] = bounds[0];
    v[1] = bounds[2];
    v[2] = bounds[4]; /* x0 y0 z0 */
    v[3] = bounds[1];
    v[4] = bounds[2];
    v[5] = bounds[4]; /* x1 y0 z0 */
    v[6] = bounds[1];
    v[7] = bounds[3];
    v[8] = bounds[4]; /* x1 y1 z0 */
    v[9] = bounds[0];
    v[10] = bounds[3];
    v[11] = bounds[4]; /* x0 y1 z0 */
    v[12] = bounds[0];
    v[13] = bounds[2];
    v[14] = bounds[5]; /* x0 y0 z1 */
    v[15] = bounds[1];
    v[16] = bounds[2];
    v[17] = bounds[5]; /* x1 y0 z1 */
    v[18] = bounds[1];
    v[19] = bounds[3];
    v[20] = bounds[5]; /* x1 y1 z1 */
    v[21] = bounds[0];
    v[22] = bounds[3];
    v[23] = bounds[5]; /* x0 y1 z1 */

    FUN_00189ba0(&v[0], 4, color); /* bottom z-face line loop */
    FUN_00189ba0(&v[12], 4, color); /* top z-face line loop    */

    p = &v[0];
    i = 4;
    do {
      FUN_00189270(1, p, p + 12, color); /* vertical edge */
      p += 3;
      i--;
    } while (i != 0);
    return;
  }

  FUN_00188ec0(7, bounds, color);
}

/* Flush the per-frame debug primitive cache (0x18ac50). Run the fixed debug
 * sub-renderers, dispatch every cached primitive to its draw routine, then
 * reset the cache once the game frame advances past the cached frame. */
void FUN_0018ac50(void)
{
  short i;
  debug_primitive *rec;

  FUN_000534d0();
  FUN_00053da0();
  render_debug_object_damage();
  render_debug_scripting();
  render_debug_trigger_volumes();
  texture_cache_debug_render();
  FUN_001be7b0();
  render_debug_recording();
  FUN_00194070();
  FUN_00149ce0();
  collision_log_render();
  FUN_00061ca0();
  render_debug_fog_planes();
  FUN_00099070();
  FUN_00189de0();
  FUN_0018a000();
  FUN_0018a110();
  FUN_0018a190();
  FUN_0018a370();
  FUN_0018a3e0();
  players_debug_render();

  for (i = 0; i < debug_primitive_count; i++) {
    rec = &debug_primitives[i];
    switch (rec->type) {
    case 0:
      FUN_0018a860(1, &rec->f04, rec->s14, rec->b16, &rec->f18,
                   *(int *)&rec->f20, &rec->f24, *(int *)&rec->f34);
      break;
    case 1:
      FUN_00189150(1, &rec->f04, rec->f10, &rec->s14);
      break;
    case 2:
      FUN_00189270(1, &rec->f04, &rec->f10, &rec->f1c);
      break;
    case 3:
      FUN_00189540(1, &rec->f04, rec->f10, &rec->s14);
      break;
    case 4:
      FUN_001896d0(1, &rec->f04, &rec->f10, *(int *)&rec->f1c, &rec->f20);
      break;
    case 5:
      FUN_00189860(1, &rec->f04, &rec->f10, rec->f1c, &rec->f20);
      break;
    case 6:
      FUN_00189a20(1, &rec->f04, &rec->f1c);
      break;
    case 7:
      FUN_0018ab30(1, &rec->f04, &rec->f1c);
      break;
    case 8:
      FUN_00189c40(1, *(const char **)&rec->f04);
      break;
    case 9:
      FUN_00189cb0(1, &rec->f08, *(void **)&rec->f04, (int)&rec->s14);
      break;
    default:
      display_assert(0, "c:\\halo\\SOURCE\\render\\render_debug.c", 0x4f5, 1);
      system_exit(-1);
    }
  }

  if ((int)debug_primitive_frame != (int)(short)game_time_get() - 1) {
    debug_primitive_frame = (short)game_time_get();
    debug_primitive_count = 0;
    *(short *)0x4d8228 = 0;
    *(char *)0x4d0e20 = 0;
  }
}
