#include "x87_math.h"

/* MSVC 7.1 memset intrinsic: the original zeroes the per-set count table with
 * inline rep stos; a csmemset CALL would not match. Same pattern as the fabs
 * intrinsic in geometry.c/real_math.c. */
extern void *__cdecl memset(void *, int, unsigned int);
extern int __cdecl abs(int);
#if defined(_MSC_VER) && !defined(__clang__)
#pragma intrinsic(memset)
#pragma intrinsic(abs)
#else
#define memset __builtin_memset
#define abs __builtin_abs
#endif

float FUN_00193910(float param_1, float param_2)
{
  const float k_inv_255 = *(const float *)0x261518;
  const float k_scale = *(const float *)0x253f78;
  return (param_2 * k_inv_255 + param_1) * k_scale;
}

void FUN_001939f0(float param_1)
{
  *(float *)0x4d8eac = param_1;
  *(uint8_t *)0x4d8ea4 = 1;
  *(float *)0x4d8ea8 = param_1 - *(float *)0x4d8ea8;
}

int FUN_00193a80(int param_1, short *param_2, int in_EAX)
{
  int count;
  int mid;
  int elem;
  short elem_val;
  short cmp0;

  count = (in_EAX - param_1) >> 5;
  if (count > 0) {
    cmp0 = *param_2;
    do {
      mid = count / 2;
      elem = mid * 0x20 + param_1;
      elem_val = *(short *)elem;
      if (elem_val < cmp0)
        goto do_lower;
      if (elem_val != cmp0)
        goto do_upper;
      if (*(short *)(elem + 2) < param_2[1])
        goto do_lower;
      if (*(short *)(elem + 2) != param_2[1])
        goto do_upper;
      if (*(short *)(elem + 4) < param_2[2]) {
      do_lower:
        param_1 = elem + 0x20;
        count = count - 1 - mid;
      } else {
      do_upper:
        count = mid;
      }
    } while (count > 0);
  }
  return param_1;
}

int FUN_00193b00(int param_1, short *param_2, int in_EAX)
{
  int count;
  int mid;
  int elem;
  short elem_val;

  count = (in_EAX - param_1) >> 5;
  if (count > 0) {
    do {
      mid = count / 2;
      elem = mid * 0x20 + param_1;
      elem_val = *(short *)elem;
      if ((elem_val <= *param_2) &&
          ((elem_val != *param_2) ||
           ((*(short *)(elem + 2) <= param_2[1]) &&
            ((*(short *)(elem + 2) != param_2[1]) ||
             (*(short *)(elem + 4) <= param_2[2]))))) {
        param_1 = elem + 0x20;
        mid = count - 1 - mid;
      }
      count = mid;
    } while (count > 0);
  }
  return param_1;
}

float FUN_00193b80(float *param_1, float *param_2)
{
  return param_1[0] * param_2[0] + param_1[1] * param_2[1] +
         param_1[2] * param_2[2] + param_1[3] * param_2[3];
}

/*
 * structure_detail_objects.c per-frame cell-grid rebuild (0x193c00).
 * Rebuilds the visible detail-object cell cache for the 3x3x3 cell
 * neighbourhood centred on the current view cell, when either the view
 * cell moved, the cache was invalidated, or the definition's dirty bit
 * (flags & 2) is set.  Asserts cite structure_detail_objects.c:198 (0xc6).
 *
 * Verified against disassembly 0x193c00-0x193e0f:
 *   - two scenario_get() calls (count check + element fetch), NO args.
 *   - cell coords: FLD src; FMUL [0x268ed0]; FSUB [0x253398];
 *     FSTP tmp; FLD tmp; FISTP int  (float32 round-trip, round-to-nearest).
 *   - lower/upper bound calls: p1@ecx = begin (first elem),
 *     in_EAX@eax = end (last elem + 0x20); stack = &key.
 *   - record float at +0xc stored via FSTP (float, NOT (int)).
 */
void FUN_00193c00(void)
{
  short player_count;
  char *scn;
  int *local_14; /* detail-object palette element (0x40 bytes) */
  int *piVar10; /* tag-block ptr, then output record ptr */
  int base; /* detail-objects data base (*0x4d8ea0) */
  int base_l; /* working copy of base (iVar3) */
  short coord[4]; /* view cell {x, y, z, 0} */
  float ftmp;
  int itmp;
  short key[4]; /* search key {x, y, z, 0} passed to bounds */
  int local_54;
  int local_50;
  int local_3c;
  short *local_10; /* packed (y<<16|x)+1 in pass1; count cursor pass2 */
  int local_8i; /* loop counter (float bit-pattern of int in orig) */
  int local_c_ctr;
  short cell_counts[0x20]; /* per-set running counts (local_94 + local_92[]) */
  unsigned int local_2c; /* accumulated per-set type bitmask */
  short *begin;
  short *last;
  short *ub_last; /* last cell in upper-bound range (psVar5 - one element) */
  int end;
  short *psVar4;
  short *psVar5;
  short *psVar11;
  short *local_44; /* cursor into cell_counts */
  int local_30;
  int local_38;
  int local_48;
  int local_34;
  int local_4c;
  int iVar7;
  int idx38;
  int diff;
  float fr;
  unsigned short *puVar6;
  int *local_28p; /* base+0x5200 header block */
  short comp_idx; /* compacted-record index (sVar2) */
  short set_idx; /* set index 0..0x1f (sVar9) */
  short cnt; /* per-set count (sVar1) */

  player_count = local_player_count();
  if ((player_count != 1) || (*(short *)0x506548 == -1)) {
    return;
  }

  scn = (char *)scenario_get();
  if (*(int *)(scn + 0x24c) == 0) {
    local_14 = 0;
  } else {
    local_14 =
      (int *)tag_block_get_element((char *)scenario_get() + 0x24c, 0, 0x40);
  }
  piVar10 = local_14;
  base = *(int *)0x4d8ea0;

  /* view cell coords: round-to-nearest( src * scale - bias ) */
  ftmp = *(float *)0x506550 * *(float *)0x268ed0 - *(float *)0x253398;
  itmp = x87_round_to_int(ftmp);
  coord[0] = (short)itmp;
  ftmp = *(float *)0x506554 * *(float *)0x268ed0 - *(float *)0x253398;
  itmp = x87_round_to_int(ftmp);
  coord[1] = (short)itmp;
  ftmp = *(float *)0x506558 * *(float *)0x268ed0 - *(float *)0x253398;
  itmp = x87_round_to_int(ftmp);
  coord[2] = (short)itmp;
  coord[3] = 0;

  if (*(char *)((char *)local_14 + 0x30) == 0) {
    return;
  }

  FUN_0017cb50(); /* enter/lock */

  if ((coord[0] != *(short *)(base + 0x5208)) ||
      (coord[1] != *(short *)(base + 0x520a)) ||
      (coord[2] != *(short *)(base + 0x520c)) ||
      (*(char *)(base + 0x520e) == 0) ||
      ((*(unsigned char *)((char *)piVar10 + 0x30) & 2) != 0)) {
    /* --- zero the 0x20-entry per-set count table (word store + inline
     * rep-stos memset over the remaining 0x3e bytes, matching the original
     * movw / rep stosl / stosw sequence) --- */
    cell_counts[0] = 0;
    local_2c = 0;
    memset(&cell_counts[1], 0, 0x3e);

    *(unsigned char *)((char *)piVar10 + 0x30) = 1; /* set built flag */

    /* cache the view cell coords (two dword stores over {x,y} and {z,pad}) */
    *(int *)(base + 0x520c) = *(int *)&coord[2];
    local_54 = *(int *)&coord[1] + 1; /* (z<<16|y)+1 */
    *(int *)(base + 0x5208) = *(int *)&coord[0];
    local_10 = (short *)(*(int *)&coord[0] + 1); /* (y<<16|x)+1 */
    *(char *)(base + 0x520e) = 1; /* mark cache valid */

    local_8i = 3;
    key[2] = coord[2]; /* z */

    do {
      local_3c = local_54;
      local_c_ctr = 3;
      do {
        begin = (short *)tag_block_get_element(piVar10, 0, 0x20);
        last = (short *)tag_block_get_element(piVar10, *piVar10 - 1, 0x20);
        end = (int)((char *)last + 0x20);

        key[2] = (short)(key[2] - 1); /* z - 1 (lower bound) */
        key[0] = (short)(int)local_10;
        key[1] = (short)local_3c;
        key[3] = 0;
        psVar4 = (short *)FUN_00193a80((int)begin, key, end);
        key[2] = (short)(key[2] + 3); /* z + 2 (upper bound) */
        psVar5 = (short *)FUN_00193b00((int)begin, key, end);
        key[2] = coord[2]; /* reset z */

        ub_last = psVar5 - 0x10;
        if (((*psVar4 == key[0]) && (psVar4[1] == key[1])) &&
            ((ub_last[0] == key[0]) && (ub_last[1] == key[1]))) {
          if (psVar4 > ub_last) {
            display_assert(
              "lower_bound_cell<=upper_bound_cell",
              "c:\\halo\\SOURCE\\structures\\structure_detail_objects.c", 0xc6,
              1);
            system_exit(-1);
          }

          if (psVar4 < psVar5) {
            psVar11 = psVar4 + 2; /* &cell.z */
            local_50 =
              (int)(((unsigned int)((int)psVar5 + (-1 - (int)psVar4))) >> 5) +
              1;
            do {
              diff = abs((int)coord[2] - (int)*psVar11);
              if (diff <= 1) { /* abs(z - cell.z) <= 1; intrinsic abs =
                                  original's cdq/xor/sub sequence */
                local_44 = &cell_counts[0];
                local_30 = 0;
                local_38 = 0;
                local_2c = local_2c | *(unsigned int *)(psVar11 + 2);
                local_48 = 0;
                local_34 = 0;
                local_4c = 0x20;
                do {
                  base_l = base;
                  if ((*(unsigned int *)(psVar11 + 2) &
                       (1u << local_48)) != 0) {
                    iVar7 = *local_44 + local_34;
                    *local_44 = (short)(*local_44 + 1);
                    piVar10 = (int *)(base_l + iVar7 * 0x18);
                    *(short *)((char *)piVar10 + 8) = psVar11[-2]; /* cell.x */
                    *(short *)((char *)piVar10 + 10) = psVar11[-1]; /* cell.y */
                    idx38 = (int)(short)local_38;
                    fr = (float)(int)psVar11[1] * *(float *)0x261518 +
                         (float)(int)*psVar11;
                    *(float *)((char *)piVar10 + 0xc) = fr;
                    *piVar10 = *(int *)(psVar11 + 4) + local_30;
                    puVar6 = (unsigned short *)tag_block_get_element(
                      local_14 + 6, *(int *)(psVar11 + 6) + idx38, 2);
                    piVar10[1] = *puVar6;
                    if (local_14[9] != 0) {
                      base_l = (int)tag_block_get_element(
                        local_14 + 9, *(int *)(psVar11 + 6) + idx38, 0x10);
                    } else {
                      base_l = base + 0xa420;
                    }
                    piVar10[5] = base_l;
                    local_30 = local_30 + piVar10[1];
                    local_38 = local_38 + 1;
                  }
                  local_44 = local_44 + 1;
                  local_4c = local_4c - 1;
                  local_48 = local_48 + 1;
                  local_34 = local_34 + 0x1b;
                } while (local_4c != 0);
              }
              psVar11 = psVar11 + 0x10;
              local_50 = local_50 - 1;
            } while (local_50 != 0);
          }
        }

        local_3c = local_3c - 1;
        local_c_ctr = local_c_ctr - 1;
        piVar10 = local_14;
      } while (local_c_ctr != 0);

      local_10 = (short *)((int)local_10 - 1);
      local_8i = local_8i - 1;
    } while (local_8i != 0);

    /* --- second pass: compact active per-set counts into +0x5100 records ---
     */
    base = *(int *)0x4d8ea0; /* original re-reads the global for pass 2 */
    local_28p = (int *)(base + 0x5200);
    set_idx = 0;
    *local_28p = base + 0x5100;
    comp_idx = 0;
    local_10 = &cell_counts[0];
    *(short *)(base + 0x5204) = 0;
    local_8i = 0;
    base_l = base;
    do {
      if (((local_2c & (1u << local_8i)) != 0) &&
          ((cnt = *local_10) != 0)) {
        piVar10 = (int *)(base + 0x5100 + comp_idx * 8);
        comp_idx = comp_idx + 1;
        *piVar10 = base_l;
        *(short *)((char *)piVar10 + 4) = cnt;
        *(short *)((char *)piVar10 + 6) = set_idx;
        *(short *)(base + 0x5204) = (short)(*(short *)(base + 0x5204) + 1);
      }
      set_idx = set_idx + 1;
      local_8i = local_8i + 1;
      local_10 = local_10 + 1;
      base_l = base_l + 0x288;
    } while (set_idx < 0x20);

    FUN_0017cb60(local_28p);
  }

  FUN_0017cb70((void *)(base + 0x5200));
  FUN_0017cb80(); /* leave/unlock */
}
