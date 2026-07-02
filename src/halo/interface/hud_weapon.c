/* hud_weapon.c — weapon HUD interface (0xd8af0-0xd91f0)
 *
 * Lifted from Halo CE Xbox (cachebeta.xbe).  Owns the weapon-HUD globals
 * buffer (0x1e4 bytes, pointer stored at 0x46bd24) and renders the per-player
 * weapon / crosshair overlays.  Original source: hud_weapon.c.
 */

#include "x87_math.h"

/* Pointer to the weapon-HUD globals buffer (0x1e4 bytes), allocated by
 * FUN_000d8af0 and stored at the fixed global 0x46bd24. */
#define weapon_hud_globals (*(void **)0x46bd24)

/* FUN_000d8af0 (0xd8af0) — allocate the weapon-HUD globals buffer from the
 * game-state heap and stash it at 0x46bd24.  Asserts on allocation failure. */
void FUN_000d8af0(void)
{
  weapon_hud_globals = game_state_malloc("hud weapon interface", 0, 0x1e4);
  if (weapon_hud_globals == 0) {
    display_assert("weapon_hud_globals",
                   "c:\\halo\\SOURCE\\interface\\hud_weapon.c", 0x6b, 1);
    system_exit(-1);
  }
}

/* FUN_000d8b30 (0xd8b30) — reset the weapon-HUD globals buffer to all-0xff
 * (NONE handles) on new-map initialisation. */
void FUN_000d8b30(void)
{
  if (weapon_hud_globals == 0) {
    display_assert("weapon_hud_globals",
                   "c:\\halo\\SOURCE\\interface\\hud_weapon.c", 0x73, 1);
    system_exit(-1);
  }
  csmemset(weapon_hud_globals, -1, 0x1e4);
}

/* FUN_000d8b70 (0xd8b70) — old-map teardown hook for the weapon HUD.  The
 * original is an empty body (single RET). */
void FUN_000d8b70(void)
{
}

/* FUN_000d8b80 (0xd8b80) — dispose hook for the weapon HUD.  The original is
 * an empty body (single RET). */
void FUN_000d8b80(void)
{
}

/* FUN_000d8b90 (0xd8b90) — set (nonzero) or clear (zero) bit 0 of the weapon
 * HUD globals flags word at +0x1e0. */
void FUN_000d8b90(char show)
{
  char *g;
  unsigned int value;

  g = (char *)weapon_hud_globals;
  value = *(unsigned int *)(g + 0x1e0);
  if (show != 0) {
    *(unsigned int *)(g + 0x1e0) = value | 1;
    return;
  }
  *(unsigned int *)(g + 0x1e0) = value & ~1u;
}

/* FUN_000d8bc0 (0xd8bc0) — per-local-player weapon-HUD state accessor.
 * Returns &globals[local_player_index] at stride 0x28.  Index in ESI. */
void *FUN_000d8bc0(int16_t local_player_index /* @<esi> */)
{
  if (local_player_index >= 0 && local_player_index < 4) {
    if (weapon_hud_globals == 0) {
      display_assert("weapon_hud_globals",
                     "c:\\halo\\SOURCE\\interface\\hud_weapon.c", 0x1af, 1);
      system_exit(-1);
    }
    return (char *)weapon_hud_globals + local_player_index * 0x28;
  }
  display_assert(
      "local_player_index>=0 && local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
      "c:\\halo\\SOURCE\\interface\\hud_weapon.c", 0x1ae, 1);
  system_exit(-1);
}

/* FUN_000d8c30 (0xd8c30) — per-local-player accessor into a second globals
 * region: &globals[local_player_index+2] at stride 0x50.  Index in ESI. */
void *FUN_000d8c30(int16_t local_player_index /* @<esi> */)
{
  if (local_player_index >= 0 && local_player_index < 4) {
    if (weapon_hud_globals == 0) {
      display_assert("weapon_hud_globals",
                     "c:\\halo\\SOURCE\\interface\\hud_weapon.c", 0x1b8, 1);
      system_exit(-1);
    }
    return (char *)weapon_hud_globals + (local_player_index + 2) * 0x50;
  }
  display_assert(
      "local_player_index>=0 && local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
      "c:\\halo\\SOURCE\\interface\\hud_weapon.c", 0x1b7, 1);
  system_exit(-1);
}

/* FUN_000d8ca0 (0xd8ca0) — refresh the tracked weapon object for a local
 * player's HUD.  EAX = current object handle (gate: skip when NONE),
 * ESI = local player index.  Looks up the player record and re-verifies the
 * unit object it references (type mask 3). */
void FUN_000d8ca0(int object_handle /* @<eax> */,
                  int16_t local_player_index /* @<esi> */)
{
  int player_index;
  void *player_datum;

  if (object_handle != -1) {
    player_index = local_player_get_player_index(local_player_index);
    if (player_index == -1) {
      object_try_and_get_and_verify_type(player_index, 3);
      return;
    }
    player_index = local_player_get_player_index(local_player_index);
    player_datum = datum_get(*(data_t **)0x5aa6d4, player_index);
    object_try_and_get_and_verify_type(*(int *)((char *)player_datum + 0x34), 3);
  }
}

/* FUN_000d8fd0 (0xd8fd0) — return the filename portion of a path (text after
 * the last '\\'), or the whole string when there is no separator. */
char *FUN_000d8fd0(char *path)
{
  char *sep;

  sep = strrchr(path, '\\');
  if (sep != 0) {
    return sep + 1;
  }
  return path;
}

/* FUN_000d8cf0 (0xd8cf0) — draw a weapon's crosshair / grenade / heat HUD for
 * one object (param_2 = unit object handle) into the interface pass param_1.
 * Skips drawing when the weapon prevents grenade throwing, when the unit is a
 * seat's parent, or when there is no current grenade.  Uses the return-address
 * canary + guard-buffer stack-corruption idiom. */
void FUN_000d8cf0(int param_1, int param_2)
{
  int canary;
  int guard[128];
  short sVar;
  int obj;
  int weapon;
  int *unit;
  int seat_parent;
  short grenade_type;
  void *gg;
  int element;
  int whud_index;
  void *state;
  int grhi;
  char cVar;
  int flags7;
  int flags8;
  short count;

  canary = FUN_000d1540();
  csmemset(guard, 0x62, 0x200);

  obj = (int)object_get_and_verify_type(param_2, 3);
  weapon = unit_get_weapon(param_2, *(short *)(obj + 0x2a2));
  unit = (int *)object_get_and_verify_type(param_2, 3);
  tag_get(0x756e6974 /* 'unit' */, *unit);

  if ((weapon_prevents_grenade_throwing(weapon) == 0) &&
      (*(char *)((char *)unit + 0x2cc) != -1) &&
      ((seat_parent = (int)object_try_and_get_and_verify_type(unit[0x33], 3),
        seat_parent == 0 ||
        ((*(int *)(seat_parent + 0x2d4) != param_2) &&
         (*(int *)(seat_parent + 0x2d8) != param_2)))) &&
      (grenade_type = unit_get_current_grenade_type(param_2), grenade_type != -1)) {

    gg = game_globals_get();
    element = (int)tag_block_get_element((char *)gg + 0x128, (int)grenade_type, 0x44);
    whud_index = *(int *)(element + 0x20);
    state = FUN_000d8bc0((short)param_1);

    if (whud_index != -1) {
      grhi = (int)tag_get(0x67726869 /* 'grhi' */, whud_index);

      cVar = *(char *)((char *)unit + *(signed char *)((char *)unit + 0x2cc) + 0x2ce);
      flags7 = ((short)cVar <= *(short *)(grhi + 0x148));
      if (cVar == 0) {
        flags7 = flags7 | 2;
      } else {
        flags7 = flags7 & ~2;
      }
      sVar = local_player_count();
      if (1 < sVar) {
        flags7 = flags7 | 4;
      } else {
        flags7 = flags7 & ~4;
      }

      if ((flags7 & 1) == 0) {
        *(int *)((char *)state + 0x24) = -1;
      } else if (*(int *)((char *)state + 0x24) == -1) {
        *(int *)((char *)state + 0x24) = game_time_get();
      }

      if (*(int *)(grhi + 0x54) != -1) {
        FUN_000d3fe0(param_1, (short *)grhi, grhi + 0x24, flags7,
                     *(int *)((char *)state + 0x24));
      }
      if (*(int *)(grhi + 0xbc) != -1) {
        FUN_000d3fe0(param_1, (short *)grhi, grhi + 0x8c, flags7,
                     *(int *)((char *)state + 0x24));
      }
      if (*(char *)(grhi + 0x138) != 0) {
        grenade_type = unit_get_current_grenade_type(param_2);
        count = unit_get_grenade_count(param_2, grenade_type);
        FUN_000d3860((short)param_1, (void *)grhi, (void *)(grhi + 0xf4), count,
                     -1, flags7, *(int *)((char *)state + 0x24), 0.0f);
      }
      if (*(int *)(grhi + 0x158) != -1) {
        cVar = *(char *)((char *)unit + *(signed char *)((char *)unit + 0x2cc) + 0x2ce);
        flags8 = ((short)cVar <= *(short *)(grhi + 0x148));
        if (cVar == 0) {
          flags8 = flags8 | 2;
        } else {
          flags8 = flags8 & ~2;
        }
        if ((short)flags8 == 0) {
          flags8 = 4;
        } else {
          flags8 = flags8 & ~4;
        }
        sVar = local_player_count();
        FUN_000d4260(param_1, grhi, grhi + 0x14c, (short)flags8 | 8,
                     *(int *)((char *)state + 0x24), flags7, (1 < sVar));
      }
    }
  }

  sVar = 0x7f;
  do {
    if (guard[(int)sVar] != 0x62626262) {
      goto LAB_corrupt;
    }
    sVar = sVar - 1;
  } while (-1 < sVar);
  sVar = -1;
LAB_corrupt:
  if (canary != FUN_000d1540()) {
    display_assert("corrupt return address!",
                   "c:\\halo\\SOURCE\\interface\\hud_weapon.c", 0x3a2, 1);
    system_exit(-1);
  }
  if (sVar != -1) {
    display_assert(csprintf((char *)0x5ab100, "corrupt stack at %d!", (int)sVar),
                   "c:\\halo\\SOURCE\\interface\\hud_weapon.c", 0x3a2, 1);
    system_exit(-1);
  }
}

/* FUN_000d8ff0 (0xd8ff0) — draw the full weapon-HUD crosshair hierarchy for one
 * weapon.  @<eax> = weapon-HUD-hierarchy tag index (wphi), @<ecx> = player datum
 * pointer, stack = referencing weapon object handle and a per-weapon state buffer
 * (buf).  Walks the wphi child chain (up to 16 levels); for each crosshair block
 * and each overlay element it evaluates the state selector, resolves the bitmap
 * frame, and draws it.  Guarded by the return-address canary + 0x200-byte
 * stack-corruption sentinel idiom. */
void FUN_000d8ff0(int whud_index /* @<eax> */, int *player /* @<ecx> */,
                  int weapon_handle, int buf)
{
  int canary;
  int guard[128];
  int player_obj;
  int *state;                /* per-player HUD block (FUN_000d8c30) */
  int *wphi;                 /* wphi tag base, then reused as current-level hud */
  unsigned int render_flags;
  int unit_obj;
  int *weap;
  int *level_hud[16];        /* [0] = wphi base */
  int level_idx[16];         /* [0] = whud_index */
  int level;
  int level_count;
  int child;
  unsigned int selector_mask;
  short rect[18];            /* 0x24-byte crosshair screen rect; [0] = corner count */
  int stereo_flag;
  int xhair;
  int xhair_i;
  short *xhair_elem;
  int xhair_type;
  int *state_slot;
  int overlay;
  int overlay_i;
  int overlay_elem;
  unsigned int flags;
  float scale;
  int anim_block;
  int frame_index;
  short frame;
  char cur_valid;
  int bv;
  int hold;
  char *pc;
  int k;
  short *bitm;
  int bitmap_elem;
  short frame_sel;
  float *uvs;
  int seq_elem;
  int is32bpp;
  float rect2d[4];           /* contiguous rect passed by addr: [0]=rx0 [1]=rx1 [2]=ry0 [3]=ry1 */
  float sx, sy, dx, dy;
  int i;
  char *tag_name;
  char *slash;

  canary = FUN_000d1540();
  csmemset(guard, 0x62, 0x200);
  if (((*(unsigned char *)((char *)weapon_hud_globals + 0x1e0) & 1) != 0) &&
      (whud_index != -1)) {
    player_obj = *(int *)((char *)player + 0x34);
    state = (int *)FUN_000d8c30(*(short *)((char *)player + 2));
    wphi = (int *)tag_get(0x77706869 /* 'wphi' */, whud_index);
    render_flags = (unsigned int)(*(short *)((char *)global_scenario_get() + 0x3c) != 2);
    if (local_player_count() == 1) {
      render_flags = render_flags | 2;
    } else {
      render_flags = render_flags & ~2u;
    }
    if (1 < local_player_count()) {
      render_flags = render_flags | 4;
    } else {
      render_flags = render_flags & ~4u;
    }
    if (player_obj != -1) {
      unit_obj = (int)object_get_and_verify_type(player_obj, 3);
      if (weapon_handle != -1) {
        weap = (int *)object_get_and_verify_type(weapon_handle, 4);
        tag_get(0x77656170 /* 'weap' */, *weap);
      }

      /* seed the hierarchy walk: level_hud[0] = wphi, level_idx[0] = whud_index */
      level_hud[0] = wphi;
      for (i = 1; i < 16; i = i + 1) {
        level_hud[i] = (int *)0;
      }
      level_idx[0] = whud_index;
      for (i = 1; i < 16; i = i + 1) {
        level_idx[i] = 0;
      }
      selector_mask = (unsigned int)state[0x13];
      level = 1;
      do {
        child = *(int *)((char *)level_hud[(short)level - 1] + 0xc);
        if (child == -1) break;
        level_idx[(short)level] = child;
        level_hud[(short)level] = (int *)tag_get(0x77706869, child);
        level = level + 1;
      } while ((short)level < 0x10);
      level_count = level;
      if ((short)level == 0x10) {
        error(2, "too many levels in current weapon hud hierarchy");
      }

      level = 0;
      if (0 < (short)level_count) {
        do {
          wphi = level_hud[(short)level];

          /* zero the 0x24-byte screen rect: count=4 header, 8 dwords, trailing short */
          rect[0] = 4;
          for (i = 0; i < 8; i = i + 1) {
            *(int *)((char *)rect + 2 + i * 4) = 0;
          }
          *(short *)((char *)rect + 2 + 0x20) = 0;

          stereo_flag = (1 < local_player_count());
          xhair = 0;
          if (0 < wphi[0x21]) {
            xhair_i = 0;
            do {
              xhair_elem = (short *)tag_block_get_element(wphi + 0x21, xhair_i, 0x68);
              xhair_type = *xhair_elem;
              if (((selector_mask & 1 << ((unsigned char)xhair_type & 0x1f)) != 0) &&
                  (((int)(short)render_flags &
                    1 << (*(unsigned char *)(xhair_elem + 2) & 0x1f)) != 0)) {
                state_slot = state + xhair_type;
                overlay = 0;
                if (0 < *(int *)(xhair_elem + 0x1a)) {
                  overlay_i = 0;
                  do {
                    overlay_elem = (int)tag_block_get_element(xhair_elem + 0x1a, overlay_i, 0x6c);
                    flags = *(unsigned int *)(overlay_elem + 0x48);
                    if (((-1 < (char)flags) && (((flags & 4) == 0) || (0 < state[1]))) &&
                        (((flags & 0x40) == 0) || (state[1] == 0))) {

                      if ((local_player_count() < 2) ||
                          ((*(unsigned char *)(overlay_elem + 0xc) & 2) != 0)) {
                        scale = 1.0f;
                      } else {
                        scale = 0.5f;
                      }

                      /* bitmap tag reference is on the crosshair element (xhair+0x24) */
                      if ((*(unsigned char *)(overlay_elem + 0x48) & 2) == 0) {
                        anim_block = (int)tag_block_get_element(
                            (void *)((int)tag_get(0x6269746d /* 'bitm' */,
                                verify_tag_reference((int *)((char *)xhair_elem + 0x24))) + 0x54),
                            (int)*(short *)(overlay_elem + 0x46), 0x40);
                      } else {
                        anim_block = 0;
                      }

                      frame = 0;
                      switch (xhair_type) {
                      case 0:
                        if ((*(unsigned char *)(overlay_elem + 0x48) & 1) == 0) {
                          frame = (short)*state_slot;
                          frame_index = *(int *)(overlay_elem + 0x24);
                          goto LAB_check_frame;
                        }
                        frame = 0;
                        if (*state_slot < 1) {
                          frame_index = *(int *)(overlay_elem + 0x24);
                        } else {
                          frame_index = FUN_000d2320((int *)(overlay_elem + 0x24), 0);
                        }
                        goto LAB_have_frame;
                      case 1:
                        flags = *(unsigned int *)(overlay_elem + 0x48);
                        if ((flags & 0x20) == 0) {
                          frame = (short)*state_slot -
                                  (short)((unsigned short)(flags >> 2) & 1);
                        } else {
                          if (*state_slot == 0) goto LAB_next_overlay;
                          frame = 0;
                        }
                        if (((flags & 1) == 0) || (*state < 1)) {
                          frame_index = *(int *)(overlay_elem + 0x24);
                          goto LAB_check_frame;
                        }
                        frame_index = FUN_000d2320((int *)(overlay_elem + 0x24), 0);
                        goto LAB_check_frame;
                      case 8:
                      case 9:
                      case 14:
                      case 18:
                        if (xhair_type == 18) {
                          if (*(float *)(buf + 4) != 0.0f) {
                            cur_valid = 0;
                            goto LAB_time_check;
                          }
                          bv = ((*(unsigned int *)(unit_obj + 0x1b8) & 0x800) == 0);
                          goto LAB_check_bv;
                        }
                        if (xhair_type == 8) {
                          if ((*(short *)(buf + 0xe) == 0) &&
                              (*(short *)(buf + 0x12) == 0)) {
                            bv = ((*(unsigned int *)(unit_obj + 0x1b8) & 0x800) == 0);
                            goto LAB_check_bv;
                          }
                          cur_valid = 0;
                          goto LAB_time_check;
                        }
                        if (xhair_type == 9) {
                          bv = 1;
                          pc = (char *)(unit_obj + 0x2ce);
                          k = 2;
                          do {
                            if ((bv) && (*pc == 0)) {
                              bv = 1;
                            } else {
                              bv = 0;
                            }
                            pc = pc + 1;
                            k = k - 1;
                          } while (k != 0);
                          if ((bv) && (*(char *)(unit_obj + 0x23d) == 0)) {
                            bv = ((*(unsigned int *)(unit_obj + 0x1b8) & 0x2000) == 0);
                            goto LAB_check_bv;
                          }
                          cur_valid = 0;
                          goto LAB_time_check;
                        }
                        /* case 14: reuse cur_valid carried from a prior element */
                        if (cur_valid != 0) goto LAB_state_check;
LAB_time_check:
                        hold = FUN_000d2300(overlay_elem + 0x24);
                        if (game_time_get() - *state_slot < hold) goto LAB_state_check;
                        goto LAB_kill_slot;
LAB_check_bv:
                        if (bv) {
                          cur_valid = 0;
                          goto LAB_time_check;
                        }
                        cur_valid = 1;
LAB_state_check:
                        if (*state_slot != -1) goto LAB_animated;
LAB_kill_slot:
                        *state_slot = -1;
                        goto LAB_next_overlay;
                      case 2:
                      case 3:
                      case 4:
                      case 5:
                      case 6:
                      case 7:
                      case 10:
                      case 11:
                      case 12:
                      case 13:
                      case 15:
                      case 16:
                      case 17:
LAB_animated:
                        if (*(short *)(overlay_elem + 0x44) < 1) {
                          frame = 0;
                        } else {
                          frame = (short)((((game_time_get() - *state_slot) /
                                            (int)*(short *)(overlay_elem + 0x44)) / 0x1e) %
                                          *(int *)(anim_block + 0x34));
                        }
                        if (((*(unsigned char *)(overlay_elem + 0x48) & 1) == 0) ||
                            (*state_slot == -1)) {
                          frame_index = *(int *)(overlay_elem + 0x24);
                          goto LAB_check_frame;
                        }
                        frame_index = FUN_000d2320((int *)(overlay_elem + 0x24), *state_slot);
                        goto LAB_check_frame;
                      default:
                        /* invalid crosshair state type — halt on bad data */
                        display_assert(0, "c:\\halo\\SOURCE\\interface\\hud_weapon.c",
                                       0x4a2, 1);
                        system_exit(-1);
                      }

LAB_check_frame:
                      if (frame == -1) {
                        tag_name = (char *)tag_get_name(level_idx[(short)level]);
                        slash = strrchr(tag_name, '\\');
                        if (slash != 0) {
                          tag_name = slash + 1;
                        }
                        display_assert(csprintf((char *)0x5ab100,
                            "frame index NONE when drawing crosshair %d, element %d, for weapon interface '%s'",
                            xhair_i, overlay_i, tag_name),
                            "c:\\halo\\SOURCE\\interface\\hud_weapon.c", 0x4a5, 1);
                        system_exit(-1);
                      }

LAB_have_frame:
                      bitm = (short *)tag_get(0x6269746d /* 'bitm' */,
                                verify_tag_reference((int *)((char *)xhair_elem + 0x24)));
                      if (anim_block == 0) {
                        frame_sel = *(short *)(overlay_elem + 0x46);
                      } else {
                        frame_sel = *(short *)tag_block_get_element((void *)(anim_block + 0x34),
                                                                    (int)frame, 0x20);
                      }
                      bitmap_elem = (int)tag_block_get_element((void *)(bitm + 0x30),
                                                               (int)frame_sel, 0x30);
                      if (xbox_texture_cache_get_hardware_format((void *)bitmap_elem, 0, 1) != 0) {
                        if ((*(unsigned char *)(overlay_elem + 0x48) & 0x10) == 0) {
                          if (anim_block == 0) {
                            uvs = (float *)0;
                          } else {
                            uvs = (float *)((int)tag_block_get_element((void *)(anim_block + 0x34),
                                                                       (int)frame, 0x20) + 8);
                          }
                          flags = (unsigned int)(*bitm == 4);
                          FUN_000d3fa0(bitmap_elem, rect, overlay_elem, (int)uvs,
                                       *(int *)&scale, 0, frame_index, stereo_flag, flags, 1);
                        } else {
                          /* stretched draw: build a real_rectangle2d and inset by the
                           * screen-bounds delta scaled into normalized HUD space */
                          is32bpp = (*bitm == 4);
                          sx = 1.0f;
                          sy = 1.0f;
                          if (anim_block == 0) {
                            rect2d[0] = 0.0f;
                            rect2d[1] = is32bpp ? (float)(int)*(short *)(bitmap_elem + 4) : (float)1;
                            rect2d[2] = 0.0f;
                            rect2d[3] = is32bpp ? (float)(int)*(short *)(bitmap_elem + 6) : (float)1;
                            if (is32bpp) {
                              sx = (float)*(double *)0x2573d8;
                              sy = (float)*(double *)0x2573d8;
                            } else {
                              sx = (*(float *)0x2533c8 / (float)(int)*(short *)(bitmap_elem + 4)) *
                                   (float)*(double *)0x281f30;
                              sy = (*(float *)0x2533c8 / (float)(int)*(short *)(bitmap_elem + 6)) *
                                   (float)*(double *)0x281f30;
                            }
                          } else {
                            seq_elem = (int)tag_block_get_element((void *)(anim_block + 0x34), (int)frame, 0x20);
                            rect2d[0] = *(float *)(seq_elem + 8);
                            rect2d[1] = *(float *)(seq_elem + 0xc);
                            rect2d[2] = *(float *)(seq_elem + 0x10);
                            rect2d[3] = *(float *)(seq_elem + 0x14);
                            sy = 1.0f;
                          }
                          dx = ((float)(int)*(short *)(bitmap_elem + 4) -
                                (float)((int)*(short *)0x506582 - (int)*(short *)0x50657e) *
                                (*(float *)0x2533c8 / scale)) * sx * *(float *)0x255964;
                          dy = ((float)(int)*(short *)(bitmap_elem + 6) -
                                (float)((int)*(short *)0x506580 - (int)*(short *)0x50657c) *
                                (*(float *)0x2533c8 / scale)) * sy * *(float *)0x255964;
                          rect2d[0] = rect2d[0] - dx;
                          rect2d[1] = dx + rect2d[1];
                          rect2d[2] = rect2d[2] - dy;
                          rect2d[3] = dy + rect2d[3];
                          FUN_000d3fa0(bitmap_elem, rect, overlay_elem, (int)rect2d,
                                       *(int *)&scale, 0, frame_index, stereo_flag, is32bpp, 1);
                        }
                      }
                    }
LAB_next_overlay:
                    overlay = overlay + 1;
                    overlay_i = (int)(short)overlay;
                  } while (overlay_i < *(int *)(xhair_elem + 0x1a));
                }
              }
              xhair = xhair + 1;
              xhair_i = (int)(short)xhair;
            } while (xhair_i < wphi[0x21]);
          }
          level = level + 1;
        } while ((short)level < (short)level_count);
      }
    }
  }

  frame = 0x7f;
  do {
    if (guard[(int)frame] != 0x62626262) goto LAB_corrupt;
    frame = frame - 1;
  } while (-1 < frame);
  frame = -1;
LAB_corrupt:
  if (canary != FUN_000d1540()) {
    display_assert("corrupt return address!",
                   "c:\\halo\\SOURCE\\interface\\hud_weapon.c", 0x4e2, 1);
    system_exit(-1);
  }
  if (frame != -1) {
    display_assert(csprintf((char *)0x5ab100, "corrupt stack at %d!", (int)frame),
                   "c:\\halo\\SOURCE\\interface\\hud_weapon.c", 0x4e2, 1);
    system_exit(-1);
  }
}
