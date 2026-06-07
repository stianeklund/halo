/* inflate_blocks_free: reset blocks state, free window, workaround, and state.
 * 0x114630 / circular_queue.obj (inflate.c) */
int FUN_00114630(int s, int z)
{
  FUN_00113930(s, z, 0);
  ((void (*)(void *, void *))(*(void **)(z + 0x24)))(*(void **)(z + 0x28),
                                                     *(void **)(s + 0x28));
  ((void (*)(void *, void *))(*(void **)(z + 0x24)))(*(void **)(z + 0x28),
                                                     *(void **)(s + 0x24));
  ((void (*)(void *, void *))(*(void **)(z + 0x24)))(*(void **)(z + 0x28),
                                                     (void *)s);
  if (*(int *)0x320e30 > 0)
    crt_fprintf(*(void **)0x331070, "inflate:   blocks freed\n");
  return 0;
}

/* inflate_blocks_set_dictionary: copy dictionary into sliding window.
 * 0x114690 / circular_queue.obj (inflate.c) */
void FUN_00114690(int s, int d, int n)
{
  int sum;
  csmemcpy((void *)*(int *)(s + 0x28), (void *)d, n);
  sum = n + *(int *)(s + 0x28);
  *(int *)(s + 0x34) = sum;
  *(int *)(s + 0x30) = sum;
}

/* inflate_blocks_sync_point: return 1 if blocks state == 1.
 * 0x1146c0 / circular_queue.obj (inflate.c) */
__declspec(noinline) int FUN_001146c0(int *param_1)
{
  return *param_1 == 1;
}

/* inflate_codes_new: allocate and initialize a codes state struct.
 * 0x1146e0 / circular_queue.obj (inflate.c) */
void *FUN_001146e0(int bl, int bd, int tl, int td, int z)
{
  int *c;
  c = (int *)(*(void *(*)(void *, unsigned int, unsigned int))(
    *(void **)(z + 0x20)))(*(void **)(z + 0x28), 1, 0x1c);
  if (c != (int *)0) {
    *((char *)c + 0x10) = (char)bl;
    c[0] = 0;
    *((char *)c + 0x11) = (char)bd;
    c[5] = tl;
    c[6] = td;
    if (*(int *)0x320e30 > 0)
      crt_fprintf(*(void **)0x331070, "inflate:       codes new\n");
  }
  return (void *)c;
}

/* inflate_codes_free: free a codes state struct.
 * 0x114f60 / circular_queue.obj (inflate.c) */
void FUN_00114f60(int c, int z)
{
  ((void (*)(void *, void *))(*(void **)(z + 0x24)))(*(void **)(z + 0x28),
                                                     (void *)c);
  if (*(int *)0x320e30 > 0)
    crt_fprintf(*(void **)0x331070, "inflate:       codes free\n");
}

/* inflateReset: reset inflate stream state to initial values.
 * 0x1153c0 / circular_queue.obj (inflate.c) */
int FUN_001153c0(int z)
{
  unsigned int *s;
  if (z != 0 && (s = *(unsigned int **)(z + 0x1c)) != (unsigned int *)0) {
    *(int *)(z + 0x14) = 0;
    *(int *)(z + 0x08) = 0;
    *(int *)(z + 0x18) = 0;
    *s = s[3] ? 7u : 0u;
    FUN_00113930(*(int *)(*(int *)(z + 0x1c) + 0x14), z, 0);
    if (*(int *)0x320e30 > 0)
      crt_fprintf(*(void **)0x331070, "inflate: reset\n");
    return 0;
  }
  return (int)0xfffffffe;
}

/* inflateEnd: tear down inflate stream and free internal state.
 * 0x115430 / circular_queue.obj (inflate.c) */
int FUN_00115430(int z)
{
  int blocks;
  if (z != 0 && *(int *)(z + 0x1c) != 0 && *(int *)(z + 0x24) != 0) {
    blocks = *(int *)(*(int *)(z + 0x1c) + 0x14);
    if (blocks != 0)
      FUN_00114630(blocks, z);
    ((void (*)(void *, void *))(*(void **)(z + 0x24)))(*(void **)(z + 0x28),
                                                       *(void **)(z + 0x1c));
    *(int *)(z + 0x1c) = 0;
    if (*(int *)0x320e30 > 0)
      crt_fprintf(*(void **)0x331070, "inflate: end\n");
    return 0;
  }
  return (int)0xfffffffe;
}

/* inflateInit2_: initialize inflate stream with explicit window bits and
 * version check. 0x1154a0 / circular_queue.obj (inflate.c) */
int FUN_001154a0(int z, int w, char *version, int stream_size)
{
  int iVar1;
  int nowrap_flag;
  int adler_fn;
  int wbits;
  if (version == (char *)0 || *version != '1' || stream_size != 0x38)
    return (int)0xfffffffa;
  if (z == 0)
    return (int)0xfffffffe;
  *(int *)(z + 0x18) = 0;
  if (*(int *)(z + 0x20) == 0) {
    *(void **)(z + 0x20) = (void *)FUN_00117ad0;
    *(int *)(z + 0x28) = 0;
  }
  if (*(int *)(z + 0x24) == 0) {
    *(void **)(z + 0x24) = (void *)FUN_00117b00;
  }
  iVar1 = (int)(*(void *(*)(void *, unsigned int, unsigned int))(
    *(void **)(z + 0x20)))(*(void **)(z + 0x28), 1, 0x18);
  *(int *)(z + 0x1c) = iVar1;
  if (iVar1 != 0) {
    adler_fn = 0x0c;
    *(int *)(iVar1 + 0x14) = 0;
    *(int *)(*(int *)(z + 0x1c) + adler_fn) = 0;
    if (w < 0) {
      w = -w;
      *(int *)(*(int *)(z + 0x1c) + adler_fn) = 1;
    }
    wbits = 1 << (unsigned char)w;
    if (w < 8 || w > 15) {
      FUN_00115430(z);
      return (int)0xfffffffe;
    }
    *(int *)(*(int *)(z + 0x1c) + 0x10) = w;
    nowrap_flag = *(int *)(*(int *)(z + 0x1c) + adler_fn);
    adler_fn = ((nowrap_flag != 0) - 1) & 0x110a10;
    *(void **)(*(int *)(z + 0x1c) + 0x14) = FUN_001139d0(z, adler_fn, wbits);
    if (*(int *)(*(int *)(z + 0x1c) + 0x14) == 0) {
      FUN_00115430(z);
    } else {
      if (*(int *)0x320e30 > 0)
        crt_fprintf(*(void **)0x331070, "inflate: allocated\n");
      FUN_001153c0(z);
      return 0;
    }
  }
  return (int)0xfffffffc;
}

/* inflate_fast: fast inner loop for inflate_codes when enough input/output
 * available. Processes literal/length/distance codes without per-byte checks.
 * 0x114fa0 / circular_queue.obj (inffast.c)
 * param_1=bl (literal bits), param_2=bd (distance bits),
 * param_3=tl (literal table), param_4=td (distance table),
 * param_5=s (block state), param_6=z (z_stream) */
int FUN_00114fa0(int param_1, int param_2, int param_3, int param_4,
                 int param_5, int *param_6)
{
  unsigned char *pbVar7;
  int bVar2;
  unsigned char bVar3;
  unsigned int uVar4;
  unsigned int uVar5;
  int iVar6;
  unsigned int uVar8;
  char *fmt;
  unsigned int uVar9;
  unsigned int uVar10;
  unsigned int uVar11;
  unsigned int uVar12;
  unsigned char *local_14;
  unsigned char *local_10;
  unsigned char *local_c;
  unsigned int local_8;

  local_c = (unsigned char *)*param_6;
  local_8 = (unsigned int)param_6[1];
  local_10 = *(unsigned char **)(param_5 + 0x34);
  uVar11 = *(unsigned int *)(param_5 + 0x1c);
  uVar12 = *(unsigned int *)(param_5 + 0x20);
  if (local_10 < *(unsigned char **)(param_5 + 0x30)) {
    local_14 = (unsigned char *)(*(int *)(param_5 + 0x30) + (-1 - (int)local_10));
  } else {
    local_14 = (unsigned char *)(*(int *)(param_5 + 0x2c) - (int)local_10);
  }
  uVar8 = *(unsigned int *)(0x320d88 + param_1 * 4);
  uVar4 = *(unsigned int *)(0x320d88 + param_2 * 4);
  do {
    for (; uVar11 < 0x14; uVar11 = uVar11 + 8) {
      local_8 = local_8 - 1;
      uVar12 = uVar12 | (unsigned int)*local_c << (unsigned char)uVar11;
      local_c = local_c + 1;
    }
    pbVar7 = (unsigned char *)(param_3 + (uVar8 & uVar12) * 8);
    bVar2 = pbVar7[1];
    bVar3 = *pbVar7;
    uVar5 = (unsigned int)bVar3;
    uVar12 = uVar12 >> bVar2;
    if (uVar5 == 0) {
      goto LAB_001151e9;
    }
    uVar11 = uVar11 - pbVar7[1];
    while ((bVar3 & 0x10) == 0) {
      if ((uVar5 & 0x40) != 0) {
        if ((uVar5 & 0x20) != 0) {
          if (*(int *)0x320e30 > 1) {
            crt_fprintf(*(void **)0x331070,
                        "inflate:         * end of block\n");
          }
          uVar8 = (unsigned int)param_6[1] - local_8;
          if (uVar11 >> 3 < (unsigned int)param_6[1] - local_8) {
            uVar8 = uVar11 >> 3;
          }
          *(unsigned int *)(param_5 + 0x20) = uVar12;
          *(unsigned int *)(param_5 + 0x1c) = uVar11 + uVar8 * (unsigned int)-8;
          iVar6 = *param_6;
          param_6[1] = (int)(uVar8 + local_8);
          *param_6 = (int)local_c - (int)uVar8;
          param_6[2] = param_6[2] + (((int)local_c - (int)uVar8) - iVar6);
          *(unsigned char **)(param_5 + 0x34) = local_10;
          return 1;
        }
        param_6[6] = (int)"invalid literal/length code";
        uVar8 = (unsigned int)param_6[1] - local_8;
        if (uVar11 >> 3 < (unsigned int)param_6[1] - local_8) {
          uVar8 = uVar11 >> 3;
        }
        *(unsigned int *)(param_5 + 0x20) = uVar12;
        *(unsigned int *)(param_5 + 0x1c) = uVar11 + uVar8 * (unsigned int)-8;
        iVar6 = *param_6;
        param_6[1] = (int)(uVar8 + local_8);
        *param_6 = (int)local_c - (int)uVar8;
        param_6[2] = param_6[2] + (((int)local_c - (int)uVar8) - iVar6);
        *(unsigned char **)(param_5 + 0x34) = local_10;
        return (int)0xfffffffd;
      }
      iVar6 = (*(unsigned int *)(0x320d88 + uVar5 * 4) & uVar12) +
              *(int *)(pbVar7 + 4);
      bVar2 = pbVar7[iVar6 * 8 + 1];
      pbVar7 = pbVar7 + iVar6 * 8;
      bVar3 = *pbVar7;
      uVar5 = (unsigned int)bVar3;
      uVar12 = uVar12 >> bVar2;
      if (uVar5 == 0) goto LAB_001151e9;
      uVar11 = uVar11 - pbVar7[1];
    }
    uVar5 = uVar5 & 0xf;
    uVar9 = (*(unsigned int *)(0x320d88 + uVar5 * 4) & uVar12) +
            *(int *)(pbVar7 + 4);
    uVar11 = uVar11 - uVar5;
    uVar12 = uVar12 >> (unsigned char)uVar5;
    if (*(int *)0x320e30 > 1) {
      crt_fprintf(*(void **)0x331070,
                  "inflate:         * length %u\n", uVar9);
    }
    for (; uVar11 < 0xf; uVar11 = uVar11 + 8) {
      local_8 = local_8 - 1;
      uVar12 = uVar12 | (unsigned int)*local_c << (unsigned char)uVar11;
      local_c = local_c + 1;
    }
    pbVar7 = (unsigned char *)(param_4 + (uVar4 & uVar12) * 8);
    uVar12 = uVar12 >> pbVar7[1];
    uVar11 = uVar11 - pbVar7[1];
    bVar3 = *pbVar7;
    while ((bVar3 & 0x10) == 0) {
      if ((bVar3 & 0x40) != 0) {
        param_6[6] = (int)"invalid distance code";
        uVar8 = (unsigned int)param_6[1] - local_8;
        if (uVar11 >> 3 < (unsigned int)param_6[1] - local_8) {
          uVar8 = uVar11 >> 3;
        }
        *(unsigned int *)(param_5 + 0x20) = uVar12;
        *(unsigned int *)(param_5 + 0x1c) = uVar11 + uVar8 * (unsigned int)-8;
        param_6[1] = (int)(uVar8 + local_8);
        param_6[2] = param_6[2] + (((int)local_c - (int)uVar8) - *param_6);
        *param_6 = (int)local_c - (int)uVar8;
        *(unsigned char **)(param_5 + 0x34) = local_10;
        return (int)0xfffffffd;
      }
      iVar6 = (*(unsigned int *)(0x320d88 + (unsigned int)bVar3 * 4) & uVar12) +
              *(int *)(pbVar7 + 4);
      {
        unsigned char *pbVar1;
        pbVar1 = pbVar7 + iVar6 * 8 + 1;
        pbVar7 = pbVar7 + iVar6 * 8;
        uVar12 = uVar12 >> *pbVar1;
        uVar11 = uVar11 - *pbVar1;
        bVar3 = *pbVar7;
      }
    }
    uVar5 = bVar3 & 0xf;
    for (; uVar11 < uVar5; uVar11 = uVar11 + 8) {
      local_8 = local_8 - 1;
      uVar12 = uVar12 | (unsigned int)*local_c << (unsigned char)uVar11;
      local_c = local_c + 1;
    }
    uVar10 = (*(unsigned int *)(0x320d88 + uVar5 * 4) & uVar12) +
             *(int *)(pbVar7 + 4);
    uVar11 = uVar11 - uVar5;
    uVar12 = uVar12 >> (unsigned char)uVar5;
    if (*(int *)0x320e30 > 1) {
      crt_fprintf(*(void **)0x331070,
                  "inflate:         * distance %u\n", uVar10);
    }
    local_14 = local_14 - uVar9;
    if ((unsigned int)((int)local_10 - *(int *)(param_5 + 0x28)) < uVar10) {
      uVar10 = (unsigned int)(*(int *)(param_5 + 0x28) - (int)local_10) + uVar10;
      pbVar7 = (unsigned char *)(*(int *)(param_5 + 0x2c) - uVar10);
      if (uVar10 < uVar9) {
        uVar9 = uVar9 - uVar10;
        do {
          *local_10 = *pbVar7;
          local_10 = local_10 + 1;
          pbVar7 = pbVar7 + 1;
          uVar10 = uVar10 - 1;
        } while (uVar10 != 0);
        pbVar7 = *(unsigned char **)(param_5 + 0x28);
      }
    } else {
      pbVar7 = local_10 + -(int)uVar10;
      *local_10 = *pbVar7;
      local_10[1] = pbVar7[1];
      local_10 = local_10 + 2;
      pbVar7 = pbVar7 + 2;
      uVar9 = uVar9 - 2;
    }
    do {
      *local_10 = *pbVar7;
      local_10 = local_10 + 1;
      pbVar7 = pbVar7 + 1;
      uVar9 = uVar9 - 1;
    } while (uVar9 != 0);
    goto LAB_0011522d;
LAB_001151e9:
    uVar11 = uVar11 - bVar2;
    if (*(int *)0x320e30 > 1) {
      uVar5 = *(unsigned int *)(pbVar7 + 4);
      if (uVar5 < 0x20 || (fmt = "inflate:         * literal \'%c\'\n",
                            0x7e < uVar5)) {
        fmt = "inflate:         * literal 0x%02x\n";
      }
      crt_fprintf(*(void **)0x331070, fmt, uVar5);
    }
    *local_10 = pbVar7[4];
    local_10 = local_10 + 1;
    local_14 = local_14 + -1;
LAB_0011522d:
    if (local_14 < (unsigned char *)0x102 || local_8 < 10) {
      uVar8 = (unsigned int)param_6[1] - local_8;
      if (uVar11 >> 3 < (unsigned int)param_6[1] - local_8) {
        uVar8 = uVar11 >> 3;
      }
      *(unsigned int *)(param_5 + 0x20) = uVar12;
      *(unsigned int *)(param_5 + 0x1c) = uVar11 + uVar8 * (unsigned int)-8;
      iVar6 = *param_6;
      param_6[1] = (int)(uVar8 + local_8);
      *param_6 = (int)local_c - (int)uVar8;
      param_6[2] = param_6[2] + (((int)local_c - (int)uVar8) - iVar6);
      *(unsigned char **)(param_5 + 0x34) = local_10;
      return 0;
    }
  } while (1);
}

/* inflate_codes: process Huffman-coded data in a compressed block.
 * 10-case state machine that decodes literal/length/distance codes.
 * Falls through to inflate_fast when enough input/output is available.
 * 0x114740 / circular_queue.obj (infcodes.c)
 * Note: declared void but tail-calls FUN_00116280 whose return (in EAX)
 * is implicitly passed to the eventual caller (FUN_00113a90). */
void FUN_00114740(unsigned int param_1, int *param_2, int param_3)
{
  unsigned char *pbVar1;
  unsigned char bVar2;
  unsigned int *c; /* codes state struct */
  unsigned char *puVar4;
  int s;          /* block state pointer (saved param_1) */
  int *z;         /* z_stream pointer (saved param_2) */
  unsigned int uVar7;
  char *fmt;
  int iVar8;
  unsigned char *f;  /* source pointer for copy */
  int uVar10;
  unsigned char *q;  /* output pointer (working) */
  unsigned char *p;  /* write pointer (current) */
  unsigned char *m_ptr; /* available output space */
  unsigned int n;  /* available input bytes */
  unsigned char *local_c; /* input pointer */
  unsigned int b;  /* bit buffer (reuses param_1 slot [EBP+0x8]) */
  unsigned int k;  /* bits in bit buffer (reuses param_2 slot [EBP+0xc]) */
  int r;           /* result code (param_3 slot [EBP+0x10]) */

  z = param_2;
  s = (int)param_1;
  p = *(unsigned char **)(param_1 + 0x34);
  c = *(unsigned int **)(param_1 + 4);
  local_c = (unsigned char *)*param_2;
  n = (unsigned int)param_2[1];
  k = (unsigned int)*(int **)(param_1 + 0x1c);
  if (p < *(unsigned char **)(param_1 + 0x30)) {
    m_ptr = (unsigned char *)(*(int *)(param_1 + 0x30) + (-1 - (int)p));
  } else {
    m_ptr = (unsigned char *)(*(int *)(param_1 + 0x2c) - (int)p);
  }
  b = *(unsigned int *)(param_1 + 0x20);
  r = param_3;

  uVar7 = *c;
  while (uVar7 < 10) {
    switch (uVar7) {
    case 0:
      if ((unsigned char *)0x101 < m_ptr && 9 < n) {
        *(unsigned int *)(s + 0x20) = b;
        *(int **)(s + 0x1c) = (int *)r;
        z[1] = (int)n;
        iVar8 = *z;
        *z = (int)local_c;
        z[2] = (int)(local_c + (z[2] - iVar8));
        *(unsigned char **)(s + 0x34) = p;
        r = FUN_00114fa0((int)(unsigned char)*((unsigned char *)c + 0x10),
                               (int)(unsigned char)*((unsigned char *)c + 0x11),
                               (int)c[5], (int)c[6], s, z);
        local_c = (unsigned char *)*z;
        p = *(unsigned char **)(s + 0x34);
        n = (unsigned int)z[1];
        b = *(unsigned int *)(s + 0x20);
        k = *(unsigned int *)(s + 0x1c);
        if (p < *(unsigned char **)(s + 0x30)) {
          m_ptr = (unsigned char *)(*(int *)(s + 0x30) +
                                       (-1 - (int)p));
        } else {
          m_ptr = (unsigned char *)(*(int *)(s + 0x2c) - (int)p);
        }
        if (r != 0) {
          *c = (unsigned int)(r != 1) * 2 + 7;
          break;
        }
      }
      c[3] = (unsigned int)(unsigned char)*((unsigned char *)c + 0x10);
      c[2] = c[5];
      *c = 1;
      /* fall through */
    case 1:
      for (; (int *)r < (int *)c[3];
           r = r + 8) {
        if (n == 0) {
          goto LAB_00114dca;
        }
        n = n - 1;
        (void)r; /* original: r = r */
        b = b | (unsigned int)*local_c << (unsigned char)r;
        local_c = local_c + 1;
      }
      pbVar1 = (unsigned char *)(c[2] +
                (*(unsigned int *)(0x320d88 + c[3] * 4) & b) * 8);
      b = b >> pbVar1[1];
      r = r - (unsigned int)pbVar1[1];
      bVar2 = *pbVar1;
      uVar7 = (unsigned int)bVar2;
      if (uVar7 == 0) {
        c[2] = *(unsigned int *)(pbVar1 + 4);
        if (*(int *)0x320e30 > 1) {
          uVar7 = *(unsigned int *)(pbVar1 + 4);
          if (uVar7 < 0x20 ||
              (fmt = "inflate:         literal \'%c\'\n", 0x7e < uVar7)) {
            fmt = "inflate:         literal 0x%02x\n";
          }
          crt_fprintf(*(void **)0x331070, fmt, uVar7);
        }
        *c = 6;
      } else if ((bVar2 & 0x10) == 0) {
        if ((bVar2 & 0x40) == 0) {
          c[3] = uVar7;
          c[2] = (unsigned int)(pbVar1 + *(int *)(pbVar1 + 4) * 8);
        } else {
          if ((bVar2 & 0x20) == 0) {
            *c = 9;
            z[6] = (int)"invalid literal/length code";
            goto switchD_caseD_9;
          }
          if (*(int *)0x320e30 > 1) {
            crt_fprintf(*(void **)0x331070,
                        "inflate:         end of block\n");
          }
          *c = 7;
        }
      } else {
        c[2] = uVar7 & 0xf;
        c[1] = *(unsigned int *)(pbVar1 + 4);
        *c = 2;
      }
      break;
    case 2:
      uVar7 = c[2];
      for (; (unsigned int)r < uVar7;
           r = r + 8) {
        if (n == 0) goto LAB_00114dca;
        n = n - 1;
        (void)r; /* original: r = r */
        b = b | (unsigned int)*local_c << (unsigned char)r;
        local_c = local_c + 1;
      }
      c[1] = c[1] + (*(unsigned int *)(0x320d88 + uVar7 * 4) & b);
      b = b >> (unsigned char)uVar7;
      r = r - (int)uVar7;
      c[3] = (unsigned int)*((unsigned char *)c + 0x11);
      c[2] = c[6];
      if (*(int *)0x320e30 > 1) {
        crt_fprintf(*(void **)0x331070,
                    "inflate:         length %u\n", c[1]);
      }
      *c = 3;
      goto LAB_00114a43;
    case 3:
LAB_00114a43:
      for (; (unsigned int)r < c[3];
           r = r + 8) {
        if (n == 0) {
          *(unsigned int *)(s + 0x20) = b;
          *(int **)(s + 0x1c) = (int *)r;
          goto LAB_00114e4b;
        }
        n = n - 1;
        (void)r; /* original: r = r */
        b = b | (unsigned int)*local_c << (unsigned char)r;
        local_c = local_c + 1;
      }
      pbVar1 = (unsigned char *)(c[2] +
                (*(unsigned int *)(0x320d88 + c[3] * 4) & b) * 8);
      b = b >> pbVar1[1];
      r = r - (unsigned int)pbVar1[1];
      bVar2 = *pbVar1;
      if ((bVar2 & 0x10) != 0) {
        c[2] = bVar2 & 0xf;
        c[3] = *(unsigned int *)(pbVar1 + 4);
        *c = 4;
        break;
      }
      if ((bVar2 & 0x40) != 0) {
        *c = 9;
        z[6] = (int)"invalid distance code";
        *(unsigned int *)(s + 0x20) = b;
        *(int **)(s + 0x1c) = (int *)r;
        z[1] = (int)n;
        r = -3;
        goto LAB_00114d76;
      }
      c[3] = (unsigned int)bVar2;
      c[2] = (unsigned int)(pbVar1 + *(int *)(pbVar1 + 4) * 8);
      break;
    case 4:
      uVar7 = c[2];
      for (; (unsigned int)r < uVar7;
           r = r + 8) {
        if (n == 0) {
          *(unsigned int *)(s + 0x20) = b;
          *(int **)(s + 0x1c) = (int *)r;
          goto LAB_00114e4b;
        }
        n = n - 1;
        (void)r; /* original: r = r */
        b = b | (unsigned int)*local_c << (unsigned char)r;
        local_c = local_c + 1;
      }
      c[3] = c[3] + (*(unsigned int *)(0x320d88 + uVar7 * 4) & b);
      r = r - (int)uVar7;
      b = b >> (unsigned char)uVar7;
      if (*(int *)0x320e30 > 1) {
        crt_fprintf(*(void **)0x331070,
                    "inflate:         distance %u\n", c[3]);
      }
      *c = 5;
      /* fall through */
    case 5:
      uVar7 = c[3];
      if ((unsigned int)((int)p - *(int *)(s + 0x28)) < uVar7) {
        iVar8 = (*(int *)(s + 0x2c) - *(int *)(s + 0x28)) -
                (int)uVar7;
      } else {
        iVar8 = -(int)uVar7;
      }
      f = p + iVar8;
      uVar7 = c[1];
      while (uVar7 != 0) {
        q = p;
        if (m_ptr == (unsigned char *)0x0) {
          if (p == *(unsigned char **)(s + 0x2c)) {
            m_ptr = *(unsigned char **)(s + 0x30);
            q = *(unsigned char **)(s + 0x28);
            if (m_ptr != q) {
              if (q < m_ptr) {
                m_ptr = (unsigned char *)((int)m_ptr + (-1 - (int)q));
              } else {
                m_ptr = (unsigned char *)(*(int *)(s + 0x2c) - (int)q);
              }
              p = q;
              if (m_ptr != (unsigned char *)0x0) goto LAB_00114c69;
            }
          }
          *(unsigned char **)(s + 0x34) = p;
          r = FUN_00116280(s, (int)z, r);
          q = *(unsigned char **)(s + 0x34);
          p = *(unsigned char **)(s + 0x30);
          if (q < p) {
            m_ptr = (unsigned char *)((int)p + (-1 - (int)q));
          } else {
            m_ptr = (unsigned char *)(*(int *)(s + 0x2c) - (int)q);
          }
          if (q == *(unsigned char **)(s + 0x2c) &&
              (puVar4 = *(unsigned char **)(s + 0x28),
               p != puVar4)) {
            q = puVar4;
            if (puVar4 < p) {
              m_ptr = (unsigned char *)((int)p + (-1 - (int)puVar4));
            } else {
              m_ptr = (unsigned char *)(*(int *)(s + 0x2c) - (int)puVar4);
            }
          }
          if (m_ptr == (unsigned char *)0x0) goto LAB_00114e67;
        }
LAB_00114c69:
        *q = *f;
        p = q + 1;
        f = f + 1;
        m_ptr = m_ptr + -1;
        r = 0;
        if (f == *(unsigned char **)(s + 0x2c)) {
          f = *(unsigned char **)(s + 0x28);
        }
        c[1] = c[1] - 1;
        uVar7 = c[1];
      }
      *c = 0;
      break;
    case 6:
      q = p;
      if (m_ptr == (unsigned char *)0x0) {
        if (p == *(unsigned char **)(s + 0x2c)) {
          m_ptr = *(unsigned char **)(s + 0x30);
          q = *(unsigned char **)(s + 0x28);
          if (m_ptr != q) {
            if (q < m_ptr) {
              m_ptr = (unsigned char *)((int)m_ptr + (-1 - (int)q));
            } else {
              m_ptr = (unsigned char *)(*(int *)(s + 0x2c) - (int)q);
            }
            p = q;
            if (m_ptr != (unsigned char *)0x0) goto LAB_00114d37;
          }
        }
        *(unsigned char **)(s + 0x34) = p;
        r = FUN_00116280(s, (int)z, r);
        q = *(unsigned char **)(s + 0x34);
        p = *(unsigned char **)(s + 0x30);
        if (q < p) {
          m_ptr = (unsigned char *)((int)p + (-1 - (int)q));
        } else {
          m_ptr = (unsigned char *)(*(int *)(s + 0x2c) - (int)q);
        }
        if (q == *(unsigned char **)(s + 0x2c) &&
            (f = *(unsigned char **)(s + 0x28),
             p != f)) {
          q = f;
          if (f < p) {
            m_ptr = (unsigned char *)((int)p + (-1 - (int)f));
          } else {
            m_ptr = (unsigned char *)(*(int *)(s + 0x2c) - (int)f);
          }
        }
        if (m_ptr == (unsigned char *)0x0) {
          goto LAB_00114e67;
        }
      }
LAB_00114d37:
      *q = (char)c[2];
      p = q + 1;
      m_ptr = m_ptr + -1;
      r = 0;
      *c = 0;
      break;
    case 7:
      if ((unsigned int)r > 7) {
        if ((unsigned int)r > 0xf) {
          FUN_00117a80("inflate_codes grabbed too many bytes");
        }
        r = r - 8;
        n = n + 1;
        local_c = local_c + -1;
      }
      *(unsigned char **)(s + 0x34) = p;
      uVar10 = FUN_00116280(s, (int)z, r);
      p = *(unsigned char **)(s + 0x34);
      if (*(unsigned char **)(s + 0x30) != p) {
        *(unsigned int *)(s + 0x20) = b;
        *(int **)(s + 0x1c) = (int *)r;
        z[1] = (int)n;
        z[2] = (int)(local_c + (z[2] - *z));
        *z = (int)local_c;
        *(unsigned char **)(s + 0x34) = p;
        FUN_00116280(s, (int)z, uVar10);
        return;
      }
      *c = 8;
      /* fall through */
    case 8:
      *(unsigned int *)(s + 0x20) = b;
      *(int **)(s + 0x1c) = (int *)r;
      z[1] = (int)n;
      r = 1;
      goto LAB_00114d76;
    case 9:
      goto switchD_caseD_9;
    }
    uVar7 = *c;
  }
  /* default exit: bad state */
  *(unsigned int *)(s + 0x20) = b;
  *(unsigned int *)(s + 0x1c) = k;
  z[1] = (int)n;
  r = -2;
LAB_00114d76:
  z[2] = (int)(local_c + (z[2] - *z));
LAB_00114d82:
  *z = (int)local_c;
  *(unsigned char **)(s + 0x34) = p;
  FUN_00116280(s, (int)z, r);
  return;

switchD_caseD_9:
  *(unsigned int *)(s + 0x20) = b;
  *(unsigned int *)(s + 0x1c) = k;
  z[1] = (int)n;
  z[2] = (int)(local_c + (z[2] - *z));
  r = -3;
  goto LAB_00114d82;

LAB_00114dca:
  *(unsigned int *)(s + 0x20) = b;
  *(unsigned int *)(s + 0x1c) = k;
  iVar8 = *z;
  *z = (int)local_c;
  z[1] = 0;
  z[2] = (int)(local_c + (z[2] - iVar8));
  *(unsigned char **)(s + 0x34) = p;
  FUN_00116280(s, (int)z, r);
  return;

LAB_00114e4b:
  z[2] = (int)(local_c + (z[2] - *z));
  z[1] = 0;
  goto LAB_00114d82;

LAB_00114e67:
  *(unsigned int *)(s + 0x20) = b;
  *(unsigned int *)(s + 0x1c) = k;
  z[1] = (int)n;
  z[2] = (int)(local_c + (z[2] - *z));
  p = q;
  goto LAB_00114d82;
}

/* inflateInit: initialize inflate stream with default window bits
 * (MAX_WBITS=15). 0x1155c0 / circular_queue.obj (inflate.c) */
int FUN_001155c0(int z, char *version, int stream_size)
{
  return FUN_001154a0(z, 0xf, version, stream_size);
}

/* huft_build: build Huffman decode tables from code length array.
 * Generates multi-level lookup tables for fast Huffman decoding.
 * 0x115ba0 / circular_queue.obj (inftrees.c)
 * bb is passed @eax (register arg); returns 0 on success, -3/-4/-5 on error. */
int FUN_00115ba0(unsigned int *bb, int *param_1, unsigned int param_2,
                 unsigned int param_3, int param_4, int param_5,
                 int *param_6, int param_7, unsigned int *param_8,
                 unsigned int *param_9)
{
  int iVar1;
  unsigned int *puVar2;
  unsigned int uVar5;
  unsigned int uVar7;
  int iVar6;
  int iVar8;
  unsigned int uVar9;
  unsigned int *puVar10;
  unsigned int uVar11;
  int iVar12;
  unsigned char bVar13;
  int iVar14;
  unsigned int uVar15;
  int local_104[14]; /* u: table stack */
  unsigned int auStack_cc[17]; /* x: bit offsets */
  unsigned int local_88[17]; /* c: bit counts */
  int local_44;
  int *local_40;
  int local_3c;
  unsigned int local_38;
  int local_34;
  unsigned int local_30; /* r: table entry (4 bytes: byte0=Exop, byte1=Bits, dword+4=Base) */
  unsigned int local_2c = 0;
  unsigned int *local_28;
  int local_24;
  unsigned int local_20;
  unsigned int local_1c;
  unsigned int *local_18;
  unsigned int local_14;
  int local_10;
  unsigned int local_c;
  unsigned int local_8;
  int *piVar4;

  /* Zero the bit count array c[0..15] */
  local_88[0] = 0; local_88[1] = 0; local_88[2] = 0; local_88[3] = 0;
  local_88[4] = 0; local_88[5] = 0; local_88[6] = 0; local_88[7] = 0;
  local_88[8] = 0; local_88[9] = 0; local_88[10] = 0; local_88[11] = 0;
  local_88[12] = 0; local_88[13] = 0; local_88[14] = 0; local_88[15] = 0;

  /* Count bit lengths */
  piVar4 = param_1;
  uVar11 = param_2;
  do {
    local_88[*piVar4] = local_88[*piVar4] + 1;
    piVar4 = piVar4 + 1;
    uVar11 = uVar11 - 1;
  } while (uVar11 != 0);

  /* If all codes are zero length, nothing to do */
  if (local_88[0] == param_2) {
    *param_6 = 0;
    *bb = 0;
    return 0;
  }

  /* Find minimum and maximum code lengths */
  local_8 = *bb;
  uVar11 = 1;
  do {
    if (local_88[uVar11] != 0) break;
    if (local_88[uVar11 + 1] != 0) { uVar11 = uVar11 + 1; break; }
    if (local_88[uVar11 + 2] != 0) { uVar11 = uVar11 + 2; break; }
    if (local_88[uVar11 + 3] != 0) { uVar11 = uVar11 + 3; break; }
    if (local_88[uVar11 + 4] != 0) { uVar11 = uVar11 + 4; break; }
    uVar11 = uVar11 + 5;
  } while (uVar11 < 0x10);
  local_c = uVar11;
  if (local_8 < uVar11) {
    local_8 = uVar11;
  }
  local_20 = 0xf;
  do {
    if (local_88[local_20] != 0) break;
    local_20 = local_20 - 1;
  } while (local_20 != 0);
  if (local_20 < local_8) {
    local_8 = local_20;
  }
  uVar9 = local_8;
  iVar12 = 1 << (unsigned char)uVar11;
  *bb = local_8;

  /* Check for over-subscribed or incomplete set */
  for (; uVar11 < local_20; uVar11 = uVar11 + 1) {
    if ((int)(iVar12 - (int)local_88[uVar11]) < 0) {
      return (int)0xfffffffd;
    }
    iVar12 = (iVar12 - (int)local_88[uVar11]) * 2;
  }
  local_24 = local_20 * 4;
  iVar12 = iVar12 - (int)local_88[local_20];
  local_3c = iVar12;
  if (iVar12 < 0) {
    return (int)0xfffffffd;
  }
  local_88[local_20] = local_88[local_20] + (unsigned int)iVar12;

  /* Generate offsets into symbol table for each code length */
  iVar8 = 0;
  iVar14 = (int)local_20 - 1;
  auStack_cc[2] = 0;
  if (iVar14 != 0) {
    iVar6 = 0;
    do {
      iVar8 = iVar8 + *(int *)((int)local_88 + iVar6 + 4);
      iVar14 = iVar14 + -1;
      *(int *)((int)auStack_cc + iVar6 + 0xc) = iVar8;
      iVar6 = iVar6 + 4;
    } while (iVar14 != 0);
  }

  /* Fill the symbol table with sorted values */
  uVar11 = 0;
  do {
    iVar8 = *param_1;
    local_18 = (unsigned int *)(param_1 + 1);
    if (iVar8 != 0) {
      uVar15 = auStack_cc[iVar8 + 1];
      param_9[uVar15] = uVar11;
      auStack_cc[iVar8 + 1] = uVar15 + 1;
    }
    uVar11 = uVar11 + 1;
    param_1 = (int *)local_18;
  } while (uVar11 < param_2);

  /* Generate the Huffman tables */
  iVar8 = *(int *)((int)auStack_cc + local_24 + 4);
  local_18 = param_9;
  auStack_cc[1] = 0;
  local_104[0] = 0;
  local_10 = 0;
  local_38 = 0;
  uVar11 = 0;
  iVar14 = -(int)uVar9;
  local_1c = 0;
  param_1 = (int *)0xffffffff;
  if ((int)local_c <= (int)local_20) {
    local_34 = (int)local_c - 1;
    local_28 = local_88 + local_c;
    uVar15 = local_2c;
    do {
      local_88[0x10] = *local_28;
      iVar12 = local_3c;
      while (local_3c = iVar12, local_88[0x10] != 0) {
        local_14 = local_88[0x10] - 1;
        local_44 = iVar14 + (int)uVar9;
        if (local_44 < (int)local_c) {
          local_24 = iVar14 - (int)uVar9;
          do {
            iVar14 = iVar14 + (int)uVar9;
            local_24 = local_24 + (int)uVar9;
            iVar12 = (int)param_1 + 1;
            local_44 = local_44 + (int)uVar9;
            uVar11 = local_20 - (unsigned int)iVar14;
            if (uVar9 < local_20 - (unsigned int)iVar14) {
              uVar11 = uVar9;
            }
            uVar9 = local_c - (unsigned int)iVar14;
            uVar5 = 1 << (unsigned char)uVar9;
            if (local_88[0x10] < uVar5 &&
                (iVar6 = (int)uVar5 + (-1 - (int)local_14),
                 puVar2 = local_28, uVar9 < uVar11)) {
              while (uVar9 = uVar9 + 1, uVar9 < uVar11) {
                uVar5 = puVar2[1];
                uVar7 = (unsigned int)iVar6 * 2;
                if (uVar7 < uVar5 || uVar7 - uVar5 == 0) break;
                iVar6 = (int)(uVar7 - uVar5);
                puVar2 = puVar2 + 1;
              }
            }
            local_38 = 1 << (unsigned char)uVar9;
            uVar5 = local_38 + *param_8;
            if (0x5a0 < uVar5) {
              return (int)0xfffffffc;
            }
            iVar6 = param_7 + (int)*param_8 * 8;
            local_40 = local_104 + iVar12;
            local_104[iVar12] = iVar6;
            uVar11 = local_1c;
            *param_8 = uVar5;
            local_10 = iVar6;
            if (iVar12 == 0) {
              *param_6 = iVar6;
            } else {
              local_30 = (local_30 & 0xffffff00) | (unsigned char)uVar9;
              uVar9 = local_1c >> (unsigned char)local_24;
              iVar1 = local_40[-1];
              auStack_cc[(int)param_1 + 2] = local_1c;
              local_30 = (local_30 & 0xffff00ff) |
                         ((unsigned int)(unsigned char)local_8 << 8);
              uVar15 = (unsigned int)(((iVar6 - iVar1) >> 3) - (int)uVar9);
              *(unsigned int *)(iVar1 + uVar9 * 8) = local_30;
              *(unsigned int *)(iVar1 + 4 + uVar9 * 8) = uVar15;
            }
            uVar9 = local_8;
            param_1 = (int *)iVar12;
          } while (local_44 < (int)local_c);
        }
        bVar13 = (unsigned char)iVar14;
        if (local_18 < param_9 + iVar8) {
          uVar15 = *local_18;
          if (uVar15 < param_3) {
            local_30 = (local_30 & 0xffffff00) |
                       (unsigned int)((uVar15 < 0x100) - 1u & 0x60);
          } else {
            iVar12 = (int)(uVar15 - param_3) * 4;
            local_30 = (local_30 & 0xffffff00) |
                       (unsigned int)((unsigned char)(*(char *)(iVar12 + param_5) + 0x50));
            uVar15 = *(unsigned int *)(iVar12 + param_4);
          }
          local_18 = local_18 + 1;
        } else {
          local_30 = (local_30 & 0xffffff00) | 0xc0;
        }
        local_30 = (local_30 & 0xffff0000) |
                   ((unsigned int)(unsigned char)((char)local_c - bVar13) << 8) |
                   (local_30 & 0xff);
        iVar12 = 1 << ((char)local_c - bVar13 & 0x1f);
        uVar9 = uVar11 >> bVar13;
        if (uVar9 < local_38) {
          puVar10 = (unsigned int *)(local_10 + uVar9 * 8);
          do {
            *puVar10 = local_30;
            puVar10[1] = uVar15;
            puVar10 = puVar10 + iVar12 * 2;
            uVar9 = uVar9 + (unsigned int)iVar12;
            uVar11 = local_1c;
          } while (uVar9 < local_38);
        }
        /* Increment bit-reversal counter */
        uVar5 = 1 << (unsigned char)local_34;
        uVar9 = uVar11 & uVar5;
        while (uVar9 != 0) {
          uVar11 = uVar11 ^ uVar5;
          uVar5 = uVar5 >> 1;
          uVar9 = uVar11 & uVar5;
        }
        uVar11 = uVar11 ^ uVar5;
        local_1c = uVar11;
        local_88[0x10] = local_14;
        uVar9 = local_8;
        iVar12 = local_3c;
        /* Back up through table levels if needed */
        if (((1 << bVar13) - 1u & uVar11) !=
            auStack_cc[(int)param_1 + 1]) {
          do {
            iVar14 = iVar14 - (int)local_8;
            puVar2 = auStack_cc + (int)param_1;
            param_1 = (int *)((int)param_1 + -1);
          } while (((1 << (unsigned char)iVar14) - 1u & uVar11) !=
                   *puVar2);
        }
      }
      local_28 = local_28 + 1;
      local_c = local_c + 1;
      local_34 = local_34 + 1;
    } while ((int)local_c <= (int)local_20);
  }
  /* Check for incomplete code set */
  if (iVar12 != 0 && local_20 != 1) {
    return (int)0xfffffffb;
  }
  return 0;
}

/* inflate: main decompression state machine (zlib).
 * 0x1155e0 / circular_queue.obj (inflate.c) */
int FUN_001155e0(int z, int flush)
{
  unsigned char bVar1;
  int uVar2;
  int *puVar3;
  unsigned int uVar4;
  unsigned int uVar5;
  int *param_1 = (int *)z;
  int param_2 = flush;

  if (param_1 == (int *)0 || (int *)param_1[7] == (int *)0 || *param_1 == 0) {
    return 0xfffffffe;
  }
  uVar2 = *(int *)param_1[7];
  uVar5 = 0xfffffffb;
  uVar4 = (unsigned int)(param_2 != 4) - 1 & 0xfffffffb;
  do {
    switch (uVar2) {
    case 0:
      if (param_1[1] == 0) return uVar5;
      param_1[1] = param_1[1] - 1;
      param_1[2] = param_1[2] + 1;
      *(unsigned int *)(param_1[7] + 4) = (unsigned int)*(unsigned char *)*param_1;
      puVar3 = (int *)param_1[7];
      uVar2 = puVar3[1];
      *param_1 = *param_1 + 1;
      if (((unsigned char)uVar2 & 0xf) == 8) {
        if (((unsigned int)puVar3[1] >> 4) + 8 <= (unsigned int)puVar3[4]) {
          *puVar3 = 1;
          goto case_1;
        }
        *puVar3 = 0xd;
        param_1[6] = (int)"invalid window size";
      } else {
        *puVar3 = 0xd;
        param_1[6] = (int)"unknown compression method";
      }
      goto set_mark;
    case 1:
    case_1:
      if (param_1[1] == 0) return uVar4;
      param_1[1] = param_1[1] - 1;
      param_1[2] = param_1[2] + 1;
      bVar1 = *(unsigned char *)*param_1;
      *param_1 = *param_1 + 1;
      if ((((int *)param_1[7])[1] * 0x100 + (unsigned int)bVar1) % 0x1f == 0) {
        if (0 < *(int *)0x320e30) {
          crt_fprintf(*(void **)0x331070, "inflate: zlib header ok\n");
        }
        if ((bVar1 & 0x20) != 0) {
          *(int *)param_1[7] = 2;
          uVar5 = uVar4;
          goto case_2;
        }
        *(int *)param_1[7] = 7;
        uVar5 = uVar4;
      } else {
        *(int *)param_1[7] = 0xd;
        param_1[6] = (int)"incorrect header check";
        *(int *)(param_1[7] + 4) = 5;
        uVar5 = uVar4;
      }
      break;
    case 2:
    case_2:
      if (param_1[1] == 0) return uVar5;
      param_1[2] = param_1[2] + 1;
      param_1[1] = param_1[1] - 1;
      *(unsigned int *)(param_1[7] + 8) =
          (unsigned int)*(unsigned char *)*param_1 << 0x18;
      *param_1 = *param_1 + 1;
      *(int *)param_1[7] = 3;
      uVar5 = uVar4;
    case 3:
      if (param_1[1] == 0) return uVar5;
      param_1[1] = param_1[1] - 1;
      param_1[2] = param_1[2] + 1;
      *(unsigned int *)(param_1[7] + 8) =
          *(int *)(param_1[7] + 8) +
          (unsigned int)*(unsigned char *)*param_1 * 0x10000;
      *param_1 = *param_1 + 1;
      *(int *)param_1[7] = 4;
      uVar5 = uVar4;
    case 4:
      if (param_1[1] == 0) return uVar5;
      param_1[1] = param_1[1] - 1;
      param_1[2] = param_1[2] + 1;
      *(unsigned int *)(param_1[7] + 8) =
          *(int *)(param_1[7] + 8) +
          (unsigned int)*(unsigned char *)*param_1 * 0x100;
      *param_1 = *param_1 + 1;
      *(int *)param_1[7] = 5;
      uVar5 = uVar4;
    case 5:
      if (param_1[1] == 0) return uVar5;
      param_1[1] = param_1[1] - 1;
      param_1[2] = param_1[2] + 1;
      *(int *)(param_1[7] + 8) =
          *(int *)(param_1[7] + 8) + (unsigned int)*(unsigned char *)*param_1;
      *param_1 = *param_1 + 1;
      param_1[0xc] = ((int *)param_1[7])[2];
      *(int *)param_1[7] = 6;
      return 2;
    case 6:
      *(int *)param_1[7] = 0xd;
      param_1[6] = (int)"need dictionary";
      *(int *)(param_1[7] + 4) = 0;
      return 0xfffffffe;
    case 7:
      uVar5 = FUN_00113a90(*(int *)(param_1[7] + 0x14), param_1, uVar5);
      if (uVar5 == 0xfffffffd) {
        *(int *)param_1[7] = 0xd;
        *(int *)(param_1[7] + 4) = 0;
        uVar5 = 0xfffffffd;
      } else {
        if (uVar5 == 0) {
          uVar5 = uVar4;
        }
        if (uVar5 != 1) return uVar5;
        FUN_00113930(*(int *)(param_1[7] + 0x14), (int)param_1,
                     param_1[7] + 4);
        puVar3 = (int *)param_1[7];
        if (puVar3[3] == 0) {
          *puVar3 = 8;
          uVar5 = uVar4;
          goto case_8;
        }
        *puVar3 = 0xc;
        uVar5 = uVar4;
      }
      break;
    case 8:
    case_8:
      if (param_1[1] == 0) return uVar5;
      param_1[1] = param_1[1] - 1;
      param_1[2] = param_1[2] + 1;
      *(unsigned int *)(param_1[7] + 8) =
          (unsigned int)*(unsigned char *)*param_1 << 0x18;
      *param_1 = *param_1 + 1;
      *(int *)param_1[7] = 9;
      uVar5 = uVar4;
    case 9:
      if (param_1[1] == 0) return uVar5;
      param_1[1] = param_1[1] - 1;
      param_1[2] = param_1[2] + 1;
      *(unsigned int *)(param_1[7] + 8) =
          *(int *)(param_1[7] + 8) +
          (unsigned int)*(unsigned char *)*param_1 * 0x10000;
      *param_1 = *param_1 + 1;
      *(int *)param_1[7] = 10;
      uVar5 = uVar4;
    case 10:
      if (param_1[1] == 0) return uVar5;
      param_1[1] = param_1[1] - 1;
      param_1[2] = param_1[2] + 1;
      *(unsigned int *)(param_1[7] + 8) =
          *(int *)(param_1[7] + 8) +
          (unsigned int)*(unsigned char *)*param_1 * 0x100;
      *param_1 = *param_1 + 1;
      *(int *)param_1[7] = 0xb;
      uVar5 = uVar4;
    case 0xb:
      if (param_1[1] == 0) return uVar5;
      param_1[1] = param_1[1] - 1;
      param_1[2] = param_1[2] + 1;
      *(int *)(param_1[7] + 8) =
          *(int *)(param_1[7] + 8) + (unsigned int)*(unsigned char *)*param_1;
      *param_1 = *param_1 + 1;
      puVar3 = (int *)param_1[7];
      if (puVar3[1] == puVar3[2]) {
        if (0 < *(int *)0x320e30) {
          crt_fprintf(*(void **)0x331070, "inflate: zlib check ok\n");
        }
        *(int *)param_1[7] = 0xc;
        return 1;
      }
      *puVar3 = 0xd;
      param_1[6] = (int)"incorrect data check";
    set_mark:
      *(int *)(param_1[7] + 4) = 5;
      uVar5 = uVar4;
      break;
    case 0xc:
      return 1;
    case 0xd:
      return 0xfffffffd;
    default:
      return 0xfffffffe;
    }
    uVar2 = *(int *)param_1[7];
  } while (1);
}

/* inflateSetDictionary: set the decompression dictionary after DICT check.
 * 0x115a00 / circular_queue.obj (inflate.c) */
int FUN_00115a00(int z, int dictionary, unsigned int dictLength)
{
  int adler_check;
  unsigned int wsize;
  int *new_var;
  unsigned int n;
  new_var = (int *)0;
  n = dictLength;
  if (z != 0 && *(int **)(z + 0x1c) != new_var && **(int **)(z + 0x1c) == 6) {
    adler_check = FUN_00110a10(1, dictionary, (int)dictLength);
    if (adler_check != *(int *)(z + 0x30))
      return (int)0xfffffffd;
    *(int *)(z + 0x30) = 1;
    wsize = 1 << *(int *)(*(int *)(z + 0x1c) + 0x10);
    if (dictLength >= wsize) {
      n = wsize - 1;
      dictionary = dictionary + (int)(dictLength - n);
    }
    FUN_00114690(*(int *)(*(int *)(z + 0x1c) + 0x14), dictionary, (int)n);
    **(int **)(z + 0x1c) = 7;
    return 0;
  }
  return (int)0xfffffffe;
}

/* inflateSync: scan for a zlib sync point (0x00 0x00 0xff 0xff) in next_in.
 * 0x115a90 / circular_queue.obj (inflate.c) */
int FUN_00115a90(int *z)
{
  int *state;
  char *p;
  int saved_total_in;
  int saved_total_out;
  unsigned int n;
  char *q;
  unsigned int avail_in;
  unsigned char qval;

  if (z == (int *)0 || z[7] == 0) {
    return (int)0xfffffffe;
  }
  state = (int *)z[7];
  if (*state != 0xd) {
    *state = 0xd;
    *(unsigned int *)(z[7] + 4) = 0;
  }
  avail_in = (unsigned int)z[1];
  if (avail_in == 0) {
    return (int)0xfffffffb;
  }
  p = (char *)z[0];
  n = *(unsigned int *)(z[7] + 4);
  q = p;
  do {
    if (n >= 4)
      break;
    qval = *(unsigned char *)q;
    if (qval == ((unsigned char *)0x28d850)[n]) {
      n = n + 1;
    } else if (qval == '\0') {
      n = 4 - n;
    } else {
      n = 0;
    }
    q = q + 1;
    avail_in--;
  } while (avail_in != 0);
  z[2] = z[2] + (int)((int)q - (int)p);
  z[0] = (int)q;
  z[1] = (int)avail_in;
  *(unsigned int *)(z[7] + 4) = n;
  if (n != 4) {
    return (int)0xfffffffd;
  }
  saved_total_in = z[2];
  saved_total_out = z[5];
  FUN_001153c0((int)z);
  z[2] = saved_total_in;
  z[5] = saved_total_out;
  *(int *)z[7] = 7;
  return 0;
}

/* inflateSyncPoint: return 1 if inflate blocks are at a sync point.
 * 0x115b70 / circular_queue.obj (inflate.c) */
int FUN_00115b70(int z)
{
  int iVar1;
  int uVar2;

  if (z != 0 && *(int *)(z + 0x1c) != 0) {
    iVar1 = *(int *)(*(int *)(z + 0x1c) + 0x14);
    if (iVar1 != 0) {
      uVar2 = FUN_001146c0((int *)iVar1);
      return uVar2;
    }
  }
  return (int)0xfffffffe;
}


/* inflate_trees_bits: build decode table for bit-length codes.
 * 0x116010 / circular_queue.obj (inflate.c) */
int FUN_00116010(int *c, int *bb, int tl, int td, int z)
{
  int iVar1;
  int iVar2;
  unsigned int local_8;

  local_8 = 0;
  iVar1 = (*(int (**)(int, int, int))(z + 0x20))(*(int *)(z + 0x28), 0x13, 4);
  if (iVar1 == 0)
    return -4;
  iVar2 = FUN_00115ba0((unsigned int *)bb, c, 0x13, 0x13, 0, 0, (int *)tl, td,
                       &local_8, (unsigned int *)iVar1);
  if (iVar2 == -3) {
    *(const char **)(z + 0x18) = "oversubscribed dynamic bit lengths tree";
    (*(void (**)(int, int))(z + 0x24))(*(int *)(z + 0x28), iVar1);
    return -3;
  }
  if (iVar2 == -5 || *bb == 0) {
    *(const char **)(z + 0x18) = "incomplete dynamic bit lengths tree";
    iVar2 = -3;
  }
  (*(void (**)(int, int))(z + 0x24))(*(int *)(z + 0x28), iVar1);
  return iVar2;
}

/* inflate_trees_dynamic: build decode tables for dynamic Huffman block.
 * 0x1160c0 / circular_queue.obj (inflate.c) */
int FUN_001160c0(unsigned int param_1, int param_2, int param_3, int *param_4,
                 int *param_5, int param_6, int param_7, int param_8,
                 int param_9)
{
  int iVar1;
  int iVar2;
  unsigned int local_8;

  local_8 = 0;
  iVar1 = (*(int (**)(int, int, int))(param_9 + 0x20))(*(int *)(param_9 + 0x28),
                                                       0x120, 4);
  if (iVar1 == 0)
    return -4;
  iVar2 = FUN_00115ba0((unsigned int *)param_4, (int *)param_3, param_1, 0x101,
                       (int)0x28d960, (int)0x28d9e0, (int *)param_6, param_8,
                       &local_8, (unsigned int *)iVar1);
  if (iVar2 == 0) {
    if (*param_4 != 0) {
      iVar2 = FUN_00115ba0(
        (unsigned int *)param_5, (int *)(param_3 + (int)param_1 * 4),
        (unsigned int)param_2, 0, (int)0x28da60, (int)0x28dad8, (int *)param_7,
        param_8, &local_8, (unsigned int *)iVar1);
      if (iVar2 == 0) {
        if (*param_5 != 0 || param_1 < 0x102) {
          (*(void (**)(int, int))(param_9 + 0x24))(*(int *)(param_9 + 0x28),
                                                   iVar1);
          return 0;
        }
      } else if (iVar2 == -3) {
        *(const char **)(param_9 + 0x18) = "oversubscribed distance tree";
        (*(void (**)(int, int))(param_9 + 0x24))(*(int *)(param_9 + 0x28),
                                                 iVar1);
        return iVar2;
      } else if (iVar2 == -5) {
        *(const char **)(param_9 + 0x18) = "incomplete distance tree";
        iVar2 = -3;
        (*(void (**)(int, int))(param_9 + 0x24))(*(int *)(param_9 + 0x28),
                                                 iVar1);
        return iVar2;
      } else if (iVar2 == -4) {
        goto free_and_return_inner;
      }
      *(const char **)(param_9 + 0x18) = "empty distance tree with lengths";
      iVar2 = -3;
    free_and_return_inner:
      (*(void (**)(int, int))(param_9 + 0x24))(*(int *)(param_9 + 0x28), iVar1);
      return iVar2;
    }
  } else if (iVar2 == -3) {
    *(const char **)(param_9 + 0x18) = "oversubscribed literal/length tree";
    (*(void (**)(int, int))(param_9 + 0x24))(*(int *)(param_9 + 0x28), iVar1);
    return iVar2;
  } else if (iVar2 == -4) {
    goto free_and_return_outer;
  }
  *(const char **)(param_9 + 0x18) = "incomplete literal/length tree";
  iVar2 = -3;
free_and_return_outer:
  (*(void (**)(int, int))(param_9 + 0x24))(*(int *)(param_9 + 0x28), iVar1);
  return iVar2;
}

/* inflate_trees_fixed: set pointers to fixed Huffman decode tables.
 * 0x116250 / circular_queue.obj (inflate.c) */
int FUN_00116250(int *param_1, int *param_2, int **param_3, int **param_4)
{
  *param_1 = *(int *)0x31fc80;
  *param_2 = *(int *)0x31fc84;
  *param_3 = (int *)0x31fc88;
  *param_4 = (int *)0x320c88;
  return 0;
}

/* inflate_flush: copy data from circular window to stream output buffer.
 * 0x116280 / circular_queue.obj (inflate.c) */
int FUN_00116280(int param_1, int param_2, int param_3)
{
  unsigned int uVar1;
  unsigned int uVar6;
  unsigned int uVar2;
  unsigned int uVar5;
  unsigned int uVar3;
  int local_8;
  int iVar4;
  unsigned int (*callback)(unsigned int, unsigned int, unsigned int);

  uVar1 = *(unsigned int *)(param_1 + 0x30);
  uVar6 = *(unsigned int *)(param_1 + 0x34);
  local_8 = *(int *)(param_2 + 0xc);
  if (uVar6 < uVar1) {
    uVar6 = *(unsigned int *)(param_1 + 0x2c);
  }
  uVar2 = *(unsigned int *)(param_2 + 0x10);
  uVar5 = uVar6 - uVar1;
  if (uVar5 > uVar2) {
    uVar5 = uVar2;
  }
  if ((uVar5 != 0) && (param_3 == -5)) {
    param_3 = 0;
  }
  *(unsigned int *)(param_2 + 0x10) = uVar2 - uVar5;
  *(int *)(param_2 + 0x14) = *(int *)(param_2 + 0x14) + (int)uVar5;
  callback = *(unsigned int (**)(unsigned int, unsigned int, unsigned int))(
    param_1 + 0x38);
  if (callback !=
      (unsigned int (*)(unsigned int, unsigned int, unsigned int))0) {
    uVar3 = callback(*(unsigned int *)(param_1 + 0x3c), uVar1, uVar5);
    *(unsigned int *)(param_1 + 0x3c) = uVar3;
    *(unsigned int *)(param_2 + 0x30) = uVar3;
  }
  csmemcpy((void *)local_8, (void *)uVar1, uVar5);
  local_8 = local_8 + (int)uVar5;
  iVar4 = (int)(uVar1 + uVar5);
  if (iVar4 == *(int *)(param_1 + 0x2c)) {
    iVar4 = *(int *)(param_1 + 0x28);
    if (*(int *)(param_1 + 0x34) == *(int *)(param_1 + 0x2c)) {
      *(int *)(param_1 + 0x34) = iVar4;
    }
    uVar2 = *(unsigned int *)(param_2 + 0x10);
    uVar5 = (unsigned int)(*(int *)(param_1 + 0x34) - iVar4);
    if (uVar5 > uVar2) {
      uVar5 = uVar2;
    }
    if ((uVar5 != 0) && (param_3 == -5)) {
      param_3 = 0;
    }
    *(unsigned int *)(param_2 + 0x10) = uVar2 - uVar5;
    *(int *)(param_2 + 0x14) = *(int *)(param_2 + 0x14) + (int)uVar5;
    callback = *(unsigned int (**)(unsigned int, unsigned int, unsigned int))(
      param_1 + 0x38);
    if (callback !=
        (unsigned int (*)(unsigned int, unsigned int, unsigned int))0) {
      uVar3 =
        callback(*(unsigned int *)(param_1 + 0x3c), (unsigned int)iVar4, uVar5);
      *(unsigned int *)(param_1 + 0x3c) = uVar3;
      *(unsigned int *)(param_2 + 0x30) = uVar3;
    }
    csmemcpy((void *)local_8, (void *)iVar4, uVar5);
    local_8 = local_8 + (int)uVar5;
    iVar4 = iVar4 + (int)uVar5;
  }
  *(int *)(param_2 + 0xc) = local_8;
  *(int *)(param_1 + 0x30) = iVar4;
  return param_3;
}

/* send_bits: output val (length bits) to deflate bit buffer.
 * 0x116390 / circular_queue.obj (deflate.c)
 * ABI: @eax=value, @ebx=length, @esi=deflate_state */
void FUN_00116390(int value, int length, int state)
{
  int iVar1;

  if (*(int *)0x320e30 > 1) {
    crt_fprintf(*(void **)0x331070, " l %2d v %4x ", length, value);
  }
  if (length < 1 || length > 0xf) {
    FUN_00117a80("invalid length");
  }
  iVar1 = *(int *)(state + 0x16bc);
  *(int *)(state + 0x16b4) = *(int *)(state + 0x16b4) + length;
  if (0x10 - length < iVar1) {
    *(unsigned short *)(state + 0x16b8) =
      *(unsigned short *)(state + 0x16b8) |
      (unsigned short)(value << iVar1);
    *(unsigned char *)(*(int *)(state + 8) + *(int *)(state + 0x14)) =
      *(unsigned char *)(state + 0x16b8);
    iVar1 = *(int *)(state + 0x14) + 1;
    *(int *)(state + 0x14) = iVar1;
    *(unsigned char *)(iVar1 + *(int *)(state + 8)) =
      *(unsigned char *)(state + 0x16b9);
    *(int *)(state + 0x14) = *(int *)(state + 0x14) + 1;
    iVar1 = *(int *)(state + 0x16bc);
    *(int *)(state + 0x16bc) = iVar1 + -0x10 + length;
    *(unsigned short *)(state + 0x16b8) =
      (unsigned short)value >> ((unsigned int)(0x10 - (char)iVar1) & 0x1f);
    return;
  }
  *(int *)(state + 0x16bc) = iVar1 + length;
  *(unsigned short *)(state + 0x16b8) =
    *(unsigned short *)(state + 0x16b8) |
    (unsigned short)(value << iVar1);
}

/* init_block: zero per-block frequency counts and set EOB count to 1.
 * 0x116460 / circular_queue.obj (deflate.c)
 * ABI: @edx=state */
void FUN_00116460(int state)
{
  unsigned short *puVar1;
  int iVar2;

  puVar1 = (unsigned short *)(state + 0x8c);
  iVar2 = 0x11e;
  do {
    *puVar1 = 0;
    puVar1 += 2;
    iVar2--;
  } while (iVar2 != 0);
  puVar1 = (unsigned short *)(state + 0x980);
  iVar2 = 0x1e;
  do {
    *puVar1 = 0;
    puVar1 += 2;
    iVar2--;
  } while (iVar2 != 0);
  puVar1 = (unsigned short *)(state + 0xa74);
  iVar2 = 0x13;
  do {
    *puVar1 = 0;
    puVar1 += 2;
    iVar2--;
  } while (iVar2 != 0);
  *(unsigned int *)(state + 0x16a4) = 0;
  *(unsigned int *)(state + 0x16a0) = 0;
  *(unsigned int *)(state + 0x16a8) = 0;
  *(unsigned int *)(state + 0x1698) = 0;
  *(unsigned short *)(state + 0x48c) = 1;
}

/* pqdownheap: restore heap ordering by sifting element down.
 * 0x1164d0 / circular_queue.obj (deflate.c)
 * ABI: @eax=deflate_state, @edi=freq_table(ct_data*), cdecl param_1=heap_index
 */
void FUN_001164d0(int param_1, int state, int tree)
{
  unsigned short uVar1;
  unsigned short uVar2;
  int iVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  int iVar7;
  int heap_len;

  heap_len = *(int *)(state + 0x1448);
  iVar3 = *(int *)(state + 0xb54 + param_1 * 4);
  iVar7 = param_1 * 2;
  iVar5 = iVar7 - heap_len;
  if (iVar7 > heap_len) {
    *(int *)(state + 0xb54 + param_1 * 4) = iVar3;
    return;
  }
  while (1) {
    iVar6 = iVar7;
    if (iVar7 < heap_len) {
      iVar5 = *(int *)(state + 0xb58 + iVar7 * 4);
      uVar1 = *(unsigned short *)(tree + iVar5 * 4);
      uVar2 =
        *(unsigned short *)(tree + *(int *)(state + 0xb54 + iVar7 * 4) * 4);
      if (uVar1 < uVar2 ||
          (uVar1 == uVar2 &&
           *(unsigned char *)(iVar5 + 0x1450 + state) <=
             *(unsigned char *)(*(int *)(state + 0xb54 + iVar7 * 4) + 0x1450 +
                                state))) {
        iVar6 = iVar7 + 1;
      }
    }
    iVar5 = *(int *)(state + 0xb54 + iVar6 * 4);
    uVar1 = *(unsigned short *)(tree + iVar3 * 4);
    uVar2 = *(unsigned short *)(tree + iVar5 * 4);
    if (uVar1 < uVar2 ||
        (uVar1 == uVar2 && *(unsigned char *)(iVar3 + 0x1450 + state) <=
                             *(unsigned char *)(iVar5 + 0x1450 + state)))
      break;
    *(int *)(state + 0xb54 + param_1 * 4) = iVar5;
    iVar4 = *(int *)(state + 0x1448);
    iVar7 = iVar6 * 2;
    iVar5 = iVar7 - iVar4;
    param_1 = iVar6;
    if (iVar5 != 0 && iVar4 <= iVar7) {
      *(int *)(state + 0xb54 + iVar6 * 4) = iVar3;
      return;
    }
  }
  *(int *)(state + 0xb54 + param_1 * 4) = iVar3;
}

/* gen_bitlen: compute optimal bit lengths for a tree (0x1165b0).
 * Adjusts the bit length distribution to satisfy maximum depth constraint,
 * then recomputes opt_len and static_len.
 * ABI: @eax=tree_desc*, @esi=deflate_state (preserved by caller) */
void FUN_001165b0(int *desc, int state)
{
  int max_code;
  int *tree;
  int *stree_ptr;
  int stree;
  int *extra;
  int extra_base;
  unsigned int max_length;
  int overflow;
  int h;
  int *heap_ptr;
  unsigned int heap_count;
  int n;
  unsigned int bits;
  unsigned int freq;
  int xbits;
  short *bl;
  int k;

  max_code = ((int *)desc)[1];
  tree = (int *)((int *)desc)[0];
  stree_ptr = (int *)((int *)desc)[2];
  stree = stree_ptr[0];
  extra = (int *)stree_ptr[1];
  extra_base = stree_ptr[2];
  max_length = (unsigned int)stree_ptr[4];

  csmemset((void *)(state + 0xb34), 0, 0x20);

  *(short *)((char *)tree + *(int *)(state + 0xb54 + *(int *)(state + 0x144c) * 4) * 4 + 2) = 0;

  h = *(int *)(state + 0x144c) + 1;
  overflow = 0;
  if (h < 0x23d) {
    heap_ptr = (int *)(state + 0xb54 + h * 4);
    heap_count = 0x23d - h;
    h = h + heap_count;
    do {
      n = *heap_ptr;
      bits = (unsigned int)*(unsigned short *)((char *)tree + (unsigned int)*(unsigned short *)((char *)tree + n * 4 + 2) * 4 + 2) + 1;
      if ((int)max_length < (int)bits) {
        overflow = overflow + 1;
        bits = max_length;
      }
      *(short *)((char *)tree + n * 4 + 2) = (short)bits;
      if (n <= max_code) {
        *(short *)(state + 0xb34 + bits * 2) = *(short *)(state + 0xb34 + bits * 2) + 1;
        xbits = 0;
        if (n >= extra_base) {
          xbits = extra[(n - extra_base)];
        }
        freq = (unsigned int)*(unsigned short *)((char *)tree + n * 4);
        *(int *)(state + 0x16a0) = *(int *)(state + 0x16a0) + (int)(bits + xbits) * (int)freq;
        if (stree != 0) {
          *(int *)(state + 0x16a4) = *(int *)(state + 0x16a4) + (int)((unsigned int)*(unsigned short *)(stree + n * 4 + 2) + xbits) * (int)freq;
        }
      }
      heap_ptr = heap_ptr + 1;
      heap_count = heap_count - 1;
    } while (heap_count != 0);

    if (overflow != 0) {
      if (z_verbose >= 0) {
        crt_fprintf(&z_stderr, "\nbit length overflow\n");
      }
      bl = (short *)(state + 0xb34 + max_length * 2);
      do {
        k = max_length - 1;
        while (*(short *)(state + 0xb34 + k * 2) == 0) {
          k = k - 1;
        }
        *(short *)(state + 0xb34 + k * 2) = *(short *)(state + 0xb34 + k * 2) - 1;
        *(short *)(state + 0xb36 + k * 2) = *(short *)(state + 0xb36 + k * 2) + 2;
        *bl = *bl - 1;
        overflow = overflow - 2;
      } while (overflow > 0);

      for (; (int)max_length > 0; max_length = max_length - 1) {
        bits = (unsigned int)*bl;
        if (bits != 0) {
          int addr = state + 0xb54 + h * 4;
          heap_count = bits;
          do {
            n = *(int *)(addr - 4);
            h = h - 1;
            addr = addr - 4;
            if (n <= max_code) {
              freq = (unsigned int)*(unsigned short *)((char *)tree + n * 4 + 2);
              if (freq != max_length) {
                if (z_verbose >= 0) {
                  crt_fprintf(&z_stderr, "code %d bits %d->%d\n", n, freq, max_length);
                }
                *(int *)(state + 0x16a0) = *(int *)(state + 0x16a0) + (int)(max_length - *(unsigned short *)((char *)tree + n * 4 + 2)) * (int)(unsigned int)*(unsigned short *)((char *)tree + n * 4);
                *(short *)((char *)tree + n * 4 + 2) = (short)max_length;
                bits = heap_count;
              }
              bits = bits - 1;
              heap_count = bits;
            }
          } while (bits != 0);
        }
        bl = bl - 1;
      }
    }
  }
}

/* scan_tree: scan a Huffman tree to determine code lengths and run statistics.
 * 0x1167f0 / circular_queue.obj (deflate.c)
 * ABI: @eax=tree(ct_data*), cdecl param_1=max_code, param_2=deflate_state */
void FUN_001167f0(int param_1, int param_2, int tree)
{
  short *psVar1;
  unsigned short uVar2;
  int iVar3;
  int iVar4;
  int iVar5;
  unsigned int uVar6;
  unsigned int uVar7;
  int local_10;
  unsigned int local_c;
  unsigned short *local_8;

  uVar2 = *(unsigned short *)(tree + 2);
  iVar5 = 0;
  local_c = 0xffffffff;
  iVar3 = 7;
  iVar4 = 4;
  iVar3 = 0;
  if (uVar2 == iVar3) {
    iVar3 = 0x8a;
    iVar4 = 3;
  }
  *(unsigned short *)(tree + 6 + param_1 * 4) = 0xffff;
  if (param_1 >= 0) {
    local_8 = (unsigned short *)(tree + 6);
    local_10 = param_1 + 1;
    uVar6 = (unsigned int)uVar2;
    do {
      uVar7 = (unsigned int)*local_8;
      iVar5++;
      if (iVar3 <= iVar5 || uVar6 != uVar7) {
        if (iVar5 < iVar4) {
          psVar1 = (short *)(param_2 + 0xa74 + uVar6 * 4);
          *psVar1 += (short)iVar5;
        } else if (uVar6 != 0) {
          if (uVar6 != local_c) {
            *(short *)(param_2 + 0xa74 + uVar6 * 4) += 1;
          }
          *(short *)(param_2 + 0xab4) += 1;
        } else {
          if (iVar5 < 0xb)
            *(short *)(param_2 + 0xab8) += 1;
          else
            *(short *)(param_2 + 0xabc) += 1;
        }
        iVar5 = 0;
        local_c = uVar6;
        if (uVar7 == 0) {
          iVar3 = 0x8a;
          iVar4 = 3;
        } else if (uVar6 == uVar7) {
          iVar3 = 6;
          iVar4 = 3;
        } else {
          iVar3 = 7;
          iVar4 = 4;
        }
      }
      local_8 += 2;
      local_10--;
      uVar6 = uVar7;
    } while (local_10 != 0);
  }
}

/* send_tree: send a literal or distance tree with run-length encoding (0x1168e0).
 * Emits REP(16), REPZ_3_10(17), REPZ_11_138(18) codes for runs.
 * ABI: @eax=state, cdecl param_1=tree, param_2=max_code */
void FUN_001168e0(int state, int param_1, int param_2)
{
  unsigned int curlen;
  unsigned int nextlen;
  int count;
  int max_count;
  int min_count;
  unsigned int prevlen;

  prevlen = 0xffffffff;
  max_count = 7;
  min_count = 4;
  curlen = (unsigned int)*(unsigned short *)(param_1 + 2);
  if (curlen == 0) {
    max_count = 0x8a;
    min_count = 3;
  }
  if (param_2 >= 0) {
    int ptr = param_1 + 6;
    int loop_count = param_2 + 1;
    count = 0;
    do {
      nextlen = (unsigned int)*(unsigned short *)ptr;
      count = count + 1;
      if (count >= max_count || curlen != nextlen) {
        if (count < min_count) {
          do {
            if (z_verbose > 2) {
              crt_fprintf(&z_stderr, "\ncd %3d ", curlen);
            }
            FUN_00116390(*(unsigned short *)(state + 0xa74 + curlen * 4),
                         *(unsigned short *)(state + 0xa76 + curlen * 4),
                         state);
            count = count - 1;
          } while (count != 0);
        } else {
          if (curlen == 0) {
            if (count < 0xb) {
              if (z_verbose > 2) {
                crt_fprintf(&z_stderr, "\ncd %3d ", 0x11);
              }
              FUN_00116390(*(unsigned short *)(state + 0xab8),
                           *(unsigned short *)(state + 0xaba),
                           state);
              FUN_00116390(count - 3, 3, state);
            } else {
              if (z_verbose > 2) {
                crt_fprintf(&z_stderr, "\ncd %3d ", 0x12);
              }
              FUN_00116390(*(unsigned short *)(state + 0xabc),
                           *(unsigned short *)(state + 0xabe),
                           state);
              FUN_00116390(count - 0xb, 7, state);
            }
          } else {
            if (curlen != prevlen) {
              if (z_verbose > 2) {
                crt_fprintf(&z_stderr, "\ncd %3d ", curlen);
              }
              FUN_00116390(*(unsigned short *)(state + 0xa74 + curlen * 4),
                           *(unsigned short *)(state + 0xa76 + curlen * 4),
                           state);
              count = count - 1;
            }
            if (count < 3 || count > 6) {
              FUN_00117a80(" 3_6?");
            }
            if (z_verbose > 2) {
              crt_fprintf(&z_stderr, "\ncd %3d ", 0x10);
            }
            FUN_00116390(*(unsigned short *)(state + 0xab4),
                         *(unsigned short *)(state + 0xab6),
                         state);
            FUN_00116390(count - 3, 2, state);
          }
        }
        count = 0;
        prevlen = curlen;
        if (nextlen == 0) {
          max_count = 0x8a;
          min_count = 3;
        } else if (curlen == nextlen) {
          max_count = 6;
          min_count = 3;
        } else {
          max_count = 7;
          min_count = 4;
        }
      }
      ptr = ptr + 4;
      loop_count = loop_count - 1;
      curlen = nextlen;
    } while (loop_count != 0);
  }
}

/* send_all_trees: send literal, distance, and bit-length tree headers (0x116b00).
 * ABI: @eax=state, cdecl param_1=lcodes, param_2=dcodes, param_3=blcodes */
void FUN_00116b00(int state, int param_1, int param_2, int param_3)
{
  unsigned short uVar1;
  int i;
  int iVar2;

  if (param_1 < 0x101 || param_2 < 1 || param_3 < 4) {
    FUN_00117a80("not enough codes");
  }
  if (param_1 > 0x11e || param_2 > 0x1e || param_3 > 0x13) {
    FUN_00117a80("too many codes");
  }
  if (z_verbose > 0) {
    crt_fprintf(&z_stderr, "\nbl counts: ");
  }
  FUN_00116390(param_1 - 0x101, 5, state);
  FUN_00116390(param_2 - 1, 5, state);
  FUN_00116390(param_3 - 4, 4, state);
  i = 0;
  if (param_3 > 0) {
    do {
      if (z_verbose > 0) {
        crt_fprintf(&z_stderr, "\nbl code %2d ", (unsigned int)zlib_bl_order[i]);
      }
      uVar1 = *(unsigned short *)(state + 0xa76 + (unsigned int)zlib_bl_order[i] * 4);
      if (z_verbose > 1) {
        crt_fprintf(&z_stderr, " l %2d v %4x ", 3, (unsigned int)uVar1);
      }
      *(int *)(state + 0x16b4) = *(int *)(state + 0x16b4) + 3;
      iVar2 = *(int *)(state + 0x16bc);
      if (iVar2 > 0xd) {
        *(unsigned short *)(state + 0x16b8) = *(unsigned short *)(state + 0x16b8) | (unsigned short)(uVar1 << iVar2);
        *(unsigned char *)(*(int *)(state + 8) + *(int *)(state + 0x14)) = *(unsigned char *)(state + 0x16b8);
        iVar2 = *(int *)(state + 0x14) + 1;
        *(int *)(state + 0x14) = iVar2;
        *(unsigned char *)(iVar2 + *(int *)(state + 8)) = *(unsigned char *)(state + 0x16b9);
        *(int *)(state + 0x14) = *(int *)(state + 0x14) + 1;
        iVar2 = *(int *)(state + 0x16bc);
        *(int *)(state + 0x16bc) = iVar2 - 0xd;
        *(unsigned short *)(state + 0x16b8) = (unsigned short)(uVar1 >> (0x10 - iVar2));
      } else {
        *(unsigned short *)(state + 0x16b8) = *(unsigned short *)(state + 0x16b8) | (unsigned short)(uVar1 << iVar2);
        *(int *)(state + 0x16bc) = iVar2 + 3;
      }
      i = i + 1;
    } while (i < param_3);
  }
  if (z_verbose > 0) {
    crt_fprintf(&z_stderr, "\nbl tree: sent %ld", *(int *)(state + 0x16b4));
  }
  FUN_001168e0(state, state + 0x8c, param_1 - 1);
  if (z_verbose > 0) {
    crt_fprintf(&z_stderr, "\nlit tree: sent %ld", *(int *)(state + 0x16b4));
  }
  FUN_001168e0(state, state + 0x980, param_2 - 1);
  if (z_verbose > 0) {
    crt_fprintf(&z_stderr, "\ndist tree: sent %ld", *(int *)(state + 0x16b4));
  }
}

/* _tr_tally: record a literal or a match (distance/length) in deflate buffers.
 * 0x116d10 / circular_queue.obj (deflate.c) */
int FUN_00116d10(int param_1, int param_2, int param_3)
{
  short *psVar1;
  unsigned int bVar2;

  *(short *)(*(int *)(param_1 + 0x169c) + *(int *)(param_1 + 0x1698) * 2) =
    (short)param_2;
  *(char *)(*(int *)(param_1 + 0x1690) + *(int *)(param_1 + 0x1698)) =
    (char)param_3;
  *(int *)(param_1 + 0x1698) = *(int *)(param_1 + 0x1698) + 1;
  if (param_2 == 0) {
    psVar1 = (short *)(param_1 + 0x8c + param_3 * 4);
    *psVar1 += 1;
  } else {
    *(int *)(param_1 + 0x16a8) += 1;
    param_2--;
    if ((unsigned short)param_2 >=
        (unsigned short)(*(short *)(param_1 + 0x24) - 0x106))
      goto bad_match;
    if ((unsigned short)param_3 > 0xff)
      goto bad_match;
    if ((unsigned int)param_2 < 0x100) {
      bVar2 = *(unsigned char *)(0x28e288 + (unsigned int)param_2);
    } else {
      bVar2 = *(unsigned char *)(0x28e388 + ((unsigned int)param_2 >> 7));
    }
    if ((unsigned short)bVar2 < 0x1e)
      goto after_assert;
  bad_match:
    FUN_00117a80("_tr_tally: bad match");
  after_assert:
    psVar1 = (short *)(param_1 + 0x490 +
                       (unsigned int)(unsigned char)(*(
                         unsigned char *)(0x28e488 + (unsigned int)param_3)) *
                         4);
    *psVar1 += 1;
    if ((unsigned int)param_2 < 0x100) {
      bVar2 = *(unsigned char *)(0x28e288 + (unsigned int)param_2);
    } else {
      bVar2 = *(unsigned char *)(0x28e388 + ((unsigned int)param_2 >> 7));
    }
    *(short *)(param_1 + 0x980 + bVar2 * 4) += 1;
  }
  return *(int *)(param_1 + 0x1698) == *(int *)(param_1 + 0x1694) - 1;
}

/* compress_block: send the block data compressed using given Huffman trees (0x116e00).
 * ABI: @eax=state, cdecl param_1=ltree, param_2=dtree */
void FUN_00116e00(int state, int param_1, int param_2)
{
  unsigned int dist;
  unsigned int lc;
  unsigned int code;
  unsigned int extra;
  unsigned int idx;

  idx = 0;
  if (*(unsigned int *)(state + 0x1698) != 0) {
    do {
      dist = (unsigned int)*(unsigned short *)(*(int *)(state + 0x169c) + idx * 2);
      lc = (unsigned int)*(unsigned char *)(idx + *(int *)(state + 0x1690));
      idx = idx + 1;
      if (dist == 0) {
        if (z_verbose > 2) {
          crt_fprintf(&z_stderr, "\ncd %3d ", lc);
        }
        FUN_00116390(*(unsigned short *)(param_1 + lc * 4),
                     *(unsigned short *)(param_1 + lc * 4 + 2),
                     state);
        if (z_verbose > 1 && crt_isgraph(lc) != 0) {
          crt_fprintf(&z_stderr, " \'%c\' ", lc);
        }
      } else {
        code = (unsigned int)zlib_length_code[lc];
        if (z_verbose > 2) {
          crt_fprintf(&z_stderr, "\ncd %3d ", code + 0x101);
        }
        FUN_00116390(*(unsigned short *)(param_1 + code * 4 + 0x404),
                     *(unsigned short *)(param_1 + code * 4 + 0x406),
                     state);
        extra = (unsigned int)zlib_extra_lbits[code];
        if (extra != 0) {
          FUN_00116390(lc - zlib_base_length[code], extra, state);
        }
        dist = dist - 1;
        if (dist < 0x100) {
          code = (unsigned int)zlib_dist_code_lo[dist];
        } else {
          code = (unsigned int)zlib_dist_code_hi[dist >> 7];
        }
        if (code >= 0x1e) {
          FUN_00117a80("bad d_code");
        }
        if (z_verbose > 2) {
          crt_fprintf(&z_stderr, "\ncd %3d ", code);
        }
        FUN_00116390(*(unsigned short *)(param_2 + code * 4),
                     *(unsigned short *)(param_2 + code * 4 + 2),
                     state);
        extra = (unsigned int)zlib_extra_dbits[code];
        if (extra != 0) {
          FUN_00116390(dist - zlib_base_dist[code], extra, state);
        }
      }
      if (*(unsigned int *)(state + 0x14) >= *(unsigned int *)(state + 0x1694) + idx * 2) {
        FUN_00117a80("pendingBuf overflow");
      }
    } while (idx < *(unsigned int *)(state + 0x1698));
  }
  if (z_verbose > 2) {
    crt_fprintf(&z_stderr, "\ncd %3d ", 0x100);
  }
  FUN_00116390(*(unsigned short *)(param_1 + 0x400),
               *(unsigned short *)(param_1 + 0x402),
               state);
  *(int *)(state + 0x16ac) = (int)(unsigned int)*(unsigned short *)(param_1 + 0x402);
}

/* set_data_type: set data_type field based on literal frequency counts.
 * 0x117000 / circular_queue.obj (deflate.c)
 * ABI: @ecx=deflate_state */
void FUN_00117000(int state)
{
  unsigned int uVar1;
  unsigned int uVar3;
  unsigned short *puVar2;
  int iVar4;

  uVar3 = 0;
  uVar1 = (unsigned int)*(unsigned short *)(state + 0xa4) +
          (unsigned int)*(unsigned short *)(state + 0xa0) +
          (unsigned int)*(unsigned short *)(state + 0x9c) +
          (unsigned int)*(unsigned short *)(state + 0x98) +
          (unsigned int)*(unsigned short *)(state + 0x94) +
          (unsigned int)*(unsigned short *)(state + 0x90) +
          (unsigned int)*(unsigned short *)(state + 0x8c);
  puVar2 = (unsigned short *)(state + 0xa8);
  iVar4 = 0x79;
  do {
    uVar3 += *puVar2;
    puVar2 += 2;
    iVar4--;
  } while (iVar4 != 0);
  puVar2 = (unsigned short *)(state + 0x28c);
  iVar4 = 0x80;
  do {
    uVar1 += *puVar2;
    puVar2 += 2;
    iVar4--;
  } while (iVar4 != 0);
  *(char *)(state + 0x1c) = (char)(uVar3 >> 2 >= uVar1);
}

/* bi_flush: flush the bit buffer if at least 8 bits are pending.
 * 0x1170b0 / circular_queue.obj (deflate.c)
 * ABI: @eax=deflate_state */
void FUN_001170b0(int state)
{
  if (*(int *)(state + 0x16bc) == 0x10) {
    *(unsigned char *)(*(int *)(state + 8) + *(int *)(state + 0x14)) =
      *(unsigned char *)(state + 0x16b8);
    *(int *)(state + 0x14) += 1;
    *(unsigned char *)(*(int *)(state + 8) + *(int *)(state + 0x14)) =
      *(unsigned char *)(state + 0x16b9);
    *(int *)(state + 0x14) += 1;
    *(unsigned short *)(state + 0x16b8) = 0;
    *(unsigned int *)(state + 0x16bc) = 0;
    return;
  }
  if (*(int *)(state + 0x16bc) >= 8) {
    *(unsigned char *)(*(int *)(state + 8) + *(int *)(state + 0x14)) =
      *(unsigned char *)(state + 0x16b8);
    *(unsigned short *)(state + 0x16b8) =
      (unsigned short)*(unsigned char *)(state + 0x16b9);
    *(int *)(state + 0x14) += 1;
    *(int *)(state + 0x16bc) -= 8;
  }
}

/* bi_windup: flush any remaining bits, byte-align the bit buffer.
 * 0x117130 / circular_queue.obj (deflate.c)
 * ABI: @eax=deflate_state; returns state (EAX unchanged) */
int FUN_00117130(int state)
{
  int bi_valid;
  unsigned int new_bi_count;

  bi_valid = *(int *)(state + 0x16bc);
  if (bi_valid > 8) {
    *(unsigned char *)(*(int *)(state + 8) + *(int *)(state + 0x14)) =
      *(unsigned char *)(state + 0x16b8);
    *(int *)(state + 0x14) += 1;
    *(unsigned char *)(*(int *)(state + 0x14) + *(int *)(state + 8)) =
      *(unsigned char *)(state + 0x16b9);
    *(int *)(state + 0x14) += 1;
  } else if (bi_valid > 0) {
    *(unsigned char *)(*(int *)(state + 8) + *(int *)(state + 0x14)) =
      *(unsigned char *)(state + 0x16b8);
    *(int *)(state + 0x14) += 1;
  }
  new_bi_count = (*(unsigned int *)(state + 0x16b4) + 7u) & 0xfffffff8u;
  *(unsigned short *)(state + 0x16b8) = 0;
  *(unsigned int *)(state + 0x16bc) = 0;
  *(unsigned int *)(state + 0x16b4) = new_bi_count;
  return state;
}

/* deflate_stored block copy: copy stored block to output with optional header.
 * 0x1171a0 / circular_queue.obj (deflate.c)
 * ABI: @ecx=len, @edx=buf, @eax=state (threaded through FUN_00117130), cdecl
 * param_3=header */
void FUN_001171a0(unsigned int len, unsigned char *buf, int state, int header)
{
  int iVar1;
  int iVar2;
  unsigned char bVar3;

  iVar1 = FUN_00117130(state);
  *(unsigned int *)(iVar1 + 0x16ac) = 8;
  if (header != 0) {
    *(unsigned char *)(*(int *)(iVar1 + 0x14) + *(int *)(iVar1 + 8)) =
      (unsigned char)len;
    iVar2 = *(int *)(iVar1 + 0x14) + 1;
    *(int *)(iVar1 + 0x14) = iVar2;
    bVar3 = (unsigned char)((unsigned int)len >> 8);
    *(unsigned char *)(iVar2 + *(int *)(iVar1 + 8)) = bVar3;
    iVar2 = *(int *)(iVar1 + 0x14) + 1;
    *(int *)(iVar1 + 0x14) = iVar2;
    *(unsigned char *)(iVar2 + *(int *)(iVar1 + 8)) = ~(unsigned char)len;
    iVar2 = *(int *)(iVar1 + 0x14) + 1;
    *(int *)(iVar1 + 0x14) = iVar2;
    *(unsigned char *)(iVar2 + *(int *)(iVar1 + 8)) = ~bVar3;
    *(int *)(iVar1 + 0x14) = *(int *)(iVar1 + 0x14) + 1;
    *(int *)(iVar1 + 0x16b4) += 0x20;
  }
  *(int *)(iVar1 + 0x16b4) += len * 8;
  while (len > 0) {
    *(unsigned char *)(*(int *)(iVar1 + 0x14) + *(int *)(iVar1 + 8)) = *buf++;
    *(int *)(iVar1 + 0x14) += 1;
    len--;
  }
}

/* deflate state init: initialize tree, block, and bit-buffer fields.
 * 0x117250 / circular_queue.obj (deflate.c) */
void FUN_00117250(int param_1)
{
  *(int *)(param_1 + 0xb10) = param_1 + 0x8c;
  *(int *)(param_1 + 0xb28) = param_1 + 0xa74;
  *(int **)(param_1 + 0xb18) = (int *)0x320dcc;
  *(int *)(param_1 + 0xb1c) = param_1 + 0x980;
  *(int **)(param_1 + 0xb24) = (int *)0x320de0;
  *(int **)(param_1 + 0xb30) = (int *)0x320df4;
  *(unsigned short *)(param_1 + 0x16b8) = 0;
  *(unsigned int *)(param_1 + 0x16bc) = 0;
  *(unsigned int *)(param_1 + 0x16ac) = 8;
  *(unsigned int *)(param_1 + 0x16b0) = 0;
  *(unsigned int *)(param_1 + 0x16b4) = 0;
  FUN_00116460(param_1);
}

/* gen_codes: generate Huffman codes from code-length counts and tree lengths.
 * 0x1172d0 / circular_queue.obj (deflate.c)
 * ABI: @eax=bl_count(short[16]), cdecl param_1=tree(ct_data*), param_2=max_code
 */
void FUN_001172d0(int *param_1, int param_2, short *bl_count)
{
  unsigned int uVar1;
  int iVar2;
  int iVar4;
  int iVar5;
  unsigned int uVar3;
  unsigned int uVar7;
  unsigned int uVar8;
  unsigned int uVar9;
  unsigned int uVar10;
  unsigned short auStack_28[16];
  unsigned int local_8;
  unsigned short uVar6;

  uVar6 = 0;
  iVar2 = 1;
  do {
    uVar6 =
      (unsigned short)((*(short *)((int)bl_count + iVar2 * 2 - 2) + uVar6) * 2);
    auStack_28[iVar2] = uVar6;
    iVar2++;
  } while (iVar2 < 0x10);
  if (((unsigned int)(*(unsigned short *)((int)bl_count + 0x1e) - 1) +
       (unsigned int)uVar6) != 0x7fff) {
    FUN_00117a80("inconsistent bit counts");
  }
  if (*(int *)0x320e30 > 0) {
    crt_fprintf(*(void **)0x331070, "\ngen_codes: max_code %d ", param_2);
  }
  iVar2 = 0;
  if (param_2 >= 0) {
    do {
      uVar10 = (unsigned int)*(unsigned short *)((int)param_1 + iVar2 * 4 + 2);
      if (uVar10 != 0) {
        uVar7 = (unsigned int)auStack_28[uVar10];
        local_8 = (unsigned int)auStack_28[uVar10] + 1;
        auStack_28[uVar10] = (unsigned short)local_8;
        uVar1 = 0;
        uVar8 = uVar10;
        do {
          uVar3 = uVar1;
          uVar9 = uVar7 & 1;
          uVar7 >>= 1;
          uVar8--;
          uVar1 = (uVar3 | uVar9) << 1;
        } while ((int)uVar8 > 0);
        *(unsigned short *)((int)param_1 + iVar2 * 4) =
          (unsigned short)uVar3 | (unsigned short)uVar9;
        if (*(int *)0x320e30 > 1 && param_1 != (int *)0x28dd90) {
          iVar4 = uisgraph(iVar2);
          iVar5 = iVar2;
          if (iVar4 == 0)
            iVar5 = 0x20;
          crt_fprintf(
            *(void **)0x331070, "\nn %3d %c l %2d c %4x (%x) ", iVar2, iVar5,
            uVar10, (unsigned int)*(unsigned short *)((int)param_1 + iVar2 * 4),
            (local_8 & 0xffff) - 1);
        }
      }
      iVar2++;
    } while (iVar2 <= param_2);
  }
}

/* build_tree: build a Huffman tree from frequency counts (0x1173f0).
 * Constructs heap, builds tree using pqdownheap, generates bit lengths
 * and codes.
 * ABI: @eax=state, cdecl param_1=tree_desc* */
void FUN_001173f0(int state, int *param_1)
{
  int *tree;
  int *stree_info;
  int stree;
  int max_elems;
  int n;
  int max_code;
  int node;
  int m;
  unsigned char d1, d2;

  tree = (int *)param_1[0];
  stree_info = (int *)param_1[2];
  max_elems = stree_info[3];
  stree = stree_info[0];

  n = 0;
  max_code = -1;
  *(int *)(state + 0x1448) = 0;
  *(int *)(state + 0x144c) = 0x23d;

  if (max_elems > 0) {
    do {
      if (*(short *)((char *)tree + n * 4) != 0) {
        int heap_size = *(int *)(state + 0x1448) + 1;
        *(int *)(state + 0x1448) = heap_size;
        *(int *)(state + 0xb54 + heap_size * 4) = n;
        max_code = n;
        *(unsigned char *)(state + 0x1450 + n) = 0;
      } else {
        *(short *)((char *)tree + n * 4 + 2) = 0;
      }
      n = n + 1;
    } while (n < max_elems);
  }

  while (*(int *)(state + 0x1448) < 2) {
    if (max_code < 2) {
      max_code = max_code + 1;
      n = max_code;
    } else {
      n = 0;
    }
    {
      int heap_size = *(int *)(state + 0x1448) + 1;
      *(int *)(state + 0x1448) = heap_size;
      *(int *)(state + 0xb54 + heap_size * 4) = n;
    }
    *(short *)((char *)tree + n * 4) = 1;
    *(unsigned char *)(state + 0x1450 + n) = 0;
    *(int *)(state + 0x16a0) = *(int *)(state + 0x16a0) - 1;
    if (stree != 0) {
      *(int *)(state + 0x16a4) = *(int *)(state + 0x16a4) - (int)(unsigned int)*(unsigned short *)(stree + n * 4 + 2);
    }
  }
  param_1[1] = max_code;

  {
    int half = *(int *)(state + 0x1448) / 2;
    for (; half >= 1; half = half - 1) {
      FUN_001164d0(half, state, (int)tree);
    }
  }

  node = max_elems;
  do {
    m = *(int *)(state + 0xb58);
    {
      int last = *(int *)(state + 0x1448);
      *(int *)(state + 0x1448) = last - 1;
      *(int *)(state + 0xb58) = *(int *)(state + 0xb54 + last * 4);
    }
    FUN_001164d0(1, state, (int)tree);

    n = *(int *)(state + 0xb58);

    {
      int mh = *(int *)(state + 0x144c) - 1;
      *(int *)(state + 0x144c) = mh;
      *(int *)(state + 0xb54 + mh * 4) = m;
    }
    {
      int mh = *(int *)(state + 0x144c) - 1;
      *(int *)(state + 0x144c) = mh;
      *(int *)(state + 0xb54 + mh * 4) = n;
    }

    *(short *)((char *)tree + node * 4) = *(short *)((char *)tree + n * 4) + *(short *)((char *)tree + m * 4);

    d1 = *(unsigned char *)(state + 0x1450 + n);
    d2 = *(unsigned char *)(state + 0x1450 + m);
    if (d2 < d1) {
      d2 = d1;
    }
    *(unsigned char *)(state + 0x1450 + node) = (unsigned char)(d2 + 1);
    *(short *)((char *)tree + n * 4 + 2) = (short)node;
    *(short *)((char *)tree + m * 4 + 2) = (short)node;

    *(int *)(state + 0xb58) = node;
    node = node + 1;
    FUN_001164d0(1, state, (int)tree);
  } while (*(int *)(state + 0x1448) >= 2);

  {
    int mh = *(int *)(state + 0x144c) - 1;
    *(int *)(state + 0x144c) = mh;
    *(int *)(state + 0xb54 + mh * 4) = *(int *)(state + 0xb58);
  }

  FUN_001165b0(param_1, state);
  FUN_001172d0(tree, max_code, (short *)(state + 0xb34));
}

/* build_bl_tree: build the bit-length tree and return max bl_order index (0x117600).
 * ABI: @esi=state (preserved by caller). Returns max index in EDI→EAX. */
int FUN_00117600(int state)
{
  int max_blindex;
  int opt_len;

  FUN_001167f0(*(int *)(state + 0xb14), state, state + 0x8c);
  FUN_001167f0(*(int *)(state + 0xb20), state, state + 0x980);
  FUN_001173f0(state, (int *)(state + 0xb28));

  max_blindex = 0x12;
  do {
    if (*(short *)(state + 0xa76 + (unsigned int)zlib_bl_order[max_blindex] * 4) != 0)
      break;
    max_blindex = max_blindex - 1;
  } while (max_blindex > 2);

  opt_len = *(int *)(state + 0x16a0) + max_blindex * 3 + 0x11;
  *(int *)(state + 0x16a0) = opt_len;
  if (z_verbose > 0) {
    crt_fprintf(&z_stderr, "\ndyn trees: dyn %ld, stat %ld", opt_len,
                *(int *)(state + 0x16a4));
  }
  return max_blindex;
}

/* Align the output stream and emit STATIC_TREES end-of-block (0x1176f0).
 * If the last match distance is too small, repeat alignment. */
void FUN_001176f0(int param_1)
{
  FUN_00116390(2, 3, param_1);
  if (z_verbose > 2) {
    crt_fprintf(&z_stderr, "\ncd %3d ", 0x100);
  }
  FUN_00116390(0, 7, param_1);
  *(int *)(param_1 + 0x16b0) = *(int *)(param_1 + 0x16b0) + 10;
  FUN_001170b0(param_1);
  if (*(int *)(param_1 + 0x16ac) - *(int *)(param_1 + 0x16bc) + 0xb < 9) {
    FUN_00116390(2, 3, param_1);
    if (z_verbose > 2) {
      crt_fprintf(&z_stderr, "\ncd %3d ", 0x100);
    }
    FUN_00116390(0, 7, param_1);
    *(int *)(param_1 + 0x16b0) = *(int *)(param_1 + 0x16b0) + 10;
    FUN_001170b0(param_1);
    *(int *)(param_1 + 0x16ac) = 7;
    return;
  }
  *(int *)(param_1 + 0x16ac) = 7;
}

/* Send a stored block: emit 3-bit block type header, then raw copy (0x1176a0).
 * Updates bits_sent accounting. */
void FUN_001176a0(int param_1, unsigned char *param_2, int param_3, int param_4)
{
  FUN_00116390(param_4, 3, param_1);
  *(int *)(param_1 + 0x16b0) =
    ((*(int *)(param_1 + 0x16b0) + 10) & 0xfffffff8) + 0x20 + param_3 * 8;
  FUN_001171a0(param_3, param_2, param_1, 1);
}

/* _tr_flush_block: decide how to flush the current block and emit it (0x1177c0).
 * Chooses between stored, static Huffman, or dynamic Huffman based on sizes. */
void FUN_001177c0(int param_1, int param_2, int param_3, int param_4)
{
  unsigned int opt_len;
  unsigned int static_len;
  int max_blindex;
  int bits_sent;

  max_blindex = 0;
  if (*(int *)(param_1 + 0x7c) < 1) {
    if (param_2 == 0) {
      FUN_00117a80("lost buf");
    }
    static_len = param_3 + 5;
  } else {
    if (*(char *)(param_1 + 0x1c) == 2) {
      FUN_00117000(param_1);
    }
    FUN_001173f0(param_1, (int *)(param_1 + 0xb10));
    if (z_verbose > 0) {
      crt_fprintf(&z_stderr, "\nlit data: dyn %ld, stat %ld",
                  *(int *)(param_1 + 0x16a0), *(int *)(param_1 + 0x16a4));
    }
    FUN_001173f0(param_1, (int *)(param_1 + 0xb1c));
    if (z_verbose > 0) {
      crt_fprintf(&z_stderr, "\ndist data: dyn %ld, stat %ld",
                  *(int *)(param_1 + 0x16a0), *(int *)(param_1 + 0x16a4));
    }
    max_blindex = FUN_00117600(param_1);
    opt_len = (*(unsigned int *)(param_1 + 0x16a0) + 10) >> 3;
    static_len = (*(unsigned int *)(param_1 + 0x16a4) + 10) >> 3;
    if (z_verbose > 0) {
      crt_fprintf(&z_stderr,
                  "\nopt %lu(%lu) stat %lu(%lu) stored %lu lit %u ",
                  opt_len, *(int *)(param_1 + 0x16a0),
                  static_len, *(int *)(param_1 + 0x16a4),
                  param_3, *(int *)(param_1 + 0x1698));
    }
    if (static_len > opt_len)
      goto use_opt;
  }
  opt_len = static_len;
use_opt:
  if (opt_len < (unsigned int)(param_3 + 4) || param_2 == 0) {
    if (static_len == opt_len) {
      FUN_00116390(param_4 + 2, 3, param_1);
      FUN_00116e00(param_1, (int)&zlib_static_ltree, (int)&zlib_static_dtree);
      bits_sent = *(int *)(param_1 + 0x16a4);
    } else {
      FUN_00116390(param_4 + 4, 3, param_1);
      FUN_00116b00(param_1, *(int *)(param_1 + 0xb14) + 1,
                   *(int *)(param_1 + 0xb20) + 1, max_blindex + 1);
      FUN_00116e00(param_1, param_1 + 0x8c, param_1 + 0x980);
      bits_sent = *(int *)(param_1 + 0x16a0);
    }
    *(int *)(param_1 + 0x16b0) = *(int *)(param_1 + 0x16b0) + bits_sent + 3;
  } else {
    FUN_001176a0(param_1, (unsigned char *)param_2, param_3, param_4);
  }
  if (*(int *)(param_1 + 0x16b0) != *(int *)(param_1 + 0x16b4)) {
    FUN_00117a80("bad compressed size");
  }
  FUN_00116460(param_1);
  if (param_4 != 0) {
    FUN_00117130(param_1);
    *(int *)(param_1 + 0x16b0) = *(int *)(param_1 + 0x16b0) + 7;
  }
  if (z_verbose > 0) {
    crt_fprintf(&z_stderr, "\ncomprlen %lu(%lu) ",
                *(unsigned int *)(param_1 + 0x16b0) >> 3,
                *(unsigned int *)(param_1 + 0x16b0) + param_4 * -7);
  }
}

/* uncompress: inflate a zlib-deflated block into dest.
 * Returns 0 (Z_OK) on success with *p2 set to decompressed byte count,
 * -5 (Z_BUF_ERROR) if inflate returned Z_OK without Z_STREAM_END,
 * or the raw zlib error code on any other failure.
 * 0x1179e0 / circular_queue.obj (uncompress.c) */
int FUN_001179e0(int p1, unsigned int *p2, unsigned int *p3, unsigned int p4)
{
  int err;
  int z[14]; /* z_stream, 0x38 bytes */

  z[1] = (int)p4;
  z[0] = (int)p3;
  z[4] = (int)*p2;
  z[3] = p1;
  z[8] = 0;
  z[9] = 0;
  err = FUN_001155c0((int)z, "1.1.3", 0x38);
  if (err == 0) {
    int inflate_ret;
    inflate_ret = FUN_001155e0((int)z, 4);
    if (inflate_ret != 1) {
      FUN_00115430((int)z);
      err = -5;
      if (inflate_ret != 0)
        return inflate_ret;
    } else {
      *p2 = (unsigned int)z[5];
      err = FUN_00115430((int)z);
    }
  }
  return err;
}

/* z_error: print assertion message and exit.
 * 0x117a80 / circular_queue.obj (zutil.c) */
__declspec(noinline) void FUN_00117a80(const char *msg)
{
  display_assert(msg, "c:\\halo\\SOURCE\\memory\\zlib\\zutil.c", 0x30, 1);
  system_exit(-1);
}

/* zError: return the error message string for a zlib error code.
 * 0x117ab0 / circular_queue.obj (zutil.c) */
const char *FUN_00117ab0(int errcode)
{
  return ((const char **)0x320e10)[-errcode];
}

/* zcalloc: zlib allocator — wraps debug_malloc.
 * 0x117ad0 / circular_queue.obj (zutil.c) */
void *FUN_00117ad0(void *opaque, unsigned int items, unsigned int size)
{
  return (void *)debug_malloc(items * size, 1,
                              "c:\\halo\\SOURCE\\memory\\zlib\\zutil.c", 0xdd);
}

/* zcfree: zlib free — wraps debug_free (ignores opaque).
 * 0x117b00 / circular_queue.obj (zutil.c) */
void FUN_00117b00(void *opaque, void *ptr)
{
  debug_free(ptr, "c:\\halo\\SOURCE\\memory\\zlib\\zutil.c", 0xe4);
}

/* Initialize an array header struct: store element_size and zero count/head.
 * Asserts that the table pointer is non-null and element_size > 0.
 * 0x117b20 / circular_queue.obj (array.c line 16-17) */
void array_new(int *table, int element_size)
{
  if (table == (int *)0x0) {
    display_assert("array", "c:\\halo\\SOURCE\\memory\\array.c", 0x10, 1);
    system_exit(-1);
  }
  if (element_size <= 0) {
    display_assert("element_size>0", "c:\\halo\\SOURCE\\memory\\array.c", 0x11,
                   1);
    system_exit(-1);
  }
  table[0] = element_size;
  table[1] = 0;
  table[2] = 0;
}

/* Resize a dynamic array to new_count elements. Reallocates the element
 * buffer via debug_realloc and zero-initializes any newly allocated entries.
 * Returns 1 on success, 0 if new_count < 0, new_count > INT_MAX, or
 * if the realloc result is inconsistent with new_count (allocation failure).
 * Wrapped in profiling guards (0x449ef1 / 0x320e40 / 0x320e38).
 * 0x117b90 / circular_queue.obj (array.c line 33-44) */
int array_resize(int *array, int new_count)
{
  int success;
  int old_count;
  int element_size;
  int new_elements;
  int new_nonzero;
  int old_nonzero;

  success = 0;
  if (array == (int *)0x0) {
    display_assert("array", "c:\\halo\\SOURCE\\memory\\array.c", 0x21, 1);
    system_exit(-1);
  }
  if (array[0] < 1) {
    display_assert("array->element_size>0", "c:\\halo\\SOURCE\\memory\\array.c",
                   0x22, 1);
    system_exit(-1);
  }
  if (array[1] < 0) {
    display_assert("array->count>=0", "c:\\halo\\SOURCE\\memory\\array.c", 0x23,
                   1);
    system_exit(-1);
  }
  old_nonzero = (array[1] != 0);
  if (old_nonzero != (array[2] != 0)) {
    display_assert("(array->count!=0)==(array->elements!=NULL)",
                   "c:\\halo\\SOURCE\\memory\\array.c", 0x24, 1);
    system_exit(-1);
  }
  if (*(char *)0x449ef1 != 0 && *(char *)0x320e40 != 0) {
    profile_enter_private((void *)0x320e38);
  }
  if (new_count >= 0) {
    if (new_count != array[1]) {
      new_elements =
        (int)debug_realloc((void *)array[2], array[0] * new_count,
                           "c:\\halo\\SOURCE\\memory\\array.c", 0x2c);
      new_nonzero = (new_elements != 0);
      if ((new_count != 0) != (new_nonzero != 0)) {
        goto done;
      }
      old_count = array[1];
      if (old_count < new_count) {
        element_size = array[0];
        csmemset((void *)(new_elements + old_count * element_size), 0,
                 (new_count - old_count) * element_size);
      }
      array[1] = new_count;
      array[2] = new_elements;
    }
    success = 1;
  }
done:
  if (*(char *)0x449ef1 != 0 && *(char *)0x320e40 != 0) {
    profile_exit_private((void *)0x320e38);
  }
  return success;
}

/* Dispose of a dynamic array: free its element buffer and reset fields.
 * Asserts non-null array, non-negative count, and consistency between
 * count and element pointer. Frees via debug_realloc(ptr, 0) and stores
 * the (NULL) return back into the elements field.
 * 0x117cf0 / circular_queue.obj (array.c line 73) */
void FUN_00117cf0(int *param_1)
{
  if (param_1 == (int *)0x0) {
    display_assert("array", "c:\\halo\\SOURCE\\memory\\array.c", 0x49, 1);
    system_exit(-1);
  }
  if ((int)param_1[1] < 0) {
    display_assert("array->count>=0", "c:\\halo\\SOURCE\\memory\\array.c", 0x4a,
                   1);
    system_exit(-1);
  }
  if ((param_1[1] != 0) != (param_1[2] != 0)) {
    display_assert("(array->count!=0)==(array->elements!=NULL)",
                   "c:\\halo\\SOURCE\\memory\\array.c", 0x4b, 1);
    system_exit(-1);
  }
  param_1[0] = (int)0xffffffff;
  param_1[1] = (int)0xffffffff;
  if (param_1[2] != 0) {
    param_1[2] = (int)debug_realloc((void *)param_1[2], 0,
                                    "c:\\halo\\SOURCE\\memory\\array.c", 0x50);
  }
}

/* Append one element to a dynamic array, growing via realloc.
 * Validates array integrity, then reallocs to count+1 elements, zeroes the
 * new slot, and returns the old count (= index of the new element).
 * Returns -1 if count is already INT_MAX or if realloc fails.
 * 0x117da0 / circular_queue.obj (array.c line 93) */
int FUN_00117da0(int *array)
{
  int new_count;
  void *new_elements;
  int old_index;

  if (array == (int *)0x0) {
    display_assert("array", "c:\\halo\\SOURCE\\memory\\array.c", 0x5d, 1);
    system_exit(-1);
  }
  if (array[0] < 1) {
    display_assert("array->element_size>0", "c:\\halo\\SOURCE\\memory\\array.c",
                   0x5e, 1);
    system_exit(-1);
  }
  if (array[1] < 0) {
    display_assert("array->count>=0", "c:\\halo\\SOURCE\\memory\\array.c", 0x5f,
                   1);
    system_exit(-1);
  }
  if ((array[1] != 0) != (array[2] != 0)) {
    display_assert("(array->count!=0)==(array->elements!=NULL)",
                   "c:\\halo\\SOURCE\\memory\\array.c", 0x60, 1);
    system_exit(-1);
  }

  if (*(char *)0x449ef1 != 0 && *(char *)0x321438 != 0) {
    profile_enter_private((void *)0x321430);
  }

  old_index = -1;
  if (array[1] < 0x7fffffff) {
    new_count = array[1] + 1;
    new_elements = debug_realloc((void *)array[2], array[0] * new_count,
                                 "c:\\halo\\SOURCE\\memory\\array.c", 0x67);
    if (new_elements != (void *)0x0) {
      old_index = array[1];
      csmemset((void *)(array[0] * old_index + (int)new_elements), 0, array[0]);
      array[1] = new_count;
      array[2] = (int)new_elements;
    }
  }

  if (*(char *)0x449ef1 != 0 && *(char *)0x321438 != 0) {
    profile_exit_private((void *)0x321430);
  }

  return old_index;
}

/* Return the address of an element at the given index in a dynamic array.
 * Validates array pointer, element_size > 0, element_size == param_3,
 * non-negative count, pointer/count consistency, and index in [0, count).
 * Returns element_size * index + elements (raw address).
 * 0x117ee0 / circular_queue.obj (array.c line 125) */
int FUN_00117ee0(int *array, int index, int element_size)
{
  if (array == (int *)0x0) {
    display_assert("array", "c:\\halo\\SOURCE\\memory\\array.c", 0x7d, 1);
    system_exit(-1);
  }
  if (array[0] < 1) {
    display_assert("array->element_size>0", "c:\\halo\\SOURCE\\memory\\array.c",
                   0x7e, 1);
    system_exit(-1);
  }
  if (array[0] != element_size) {
    display_assert("array->element_size==element_size",
                   "c:\\halo\\SOURCE\\memory\\array.c", 0x7f, 1);
    system_exit(-1);
  }
  if (array[1] < 0) {
    display_assert("array->count>=0", "c:\\halo\\SOURCE\\memory\\array.c", 0x80,
                   1);
    system_exit(-1);
  }
  if ((array[1] != 0) != (array[2] != 0)) {
    display_assert("(array->count!=0)==(array->elements!=NULL)",
                   "c:\\halo\\SOURCE\\memory\\array.c", 0x81, 1);
    system_exit(-1);
  }
  if ((index < 0) || (array[1] <= index)) {
    display_assert("index>=0 && index<array->count",
                   "c:\\halo\\SOURCE\\memory\\array.c", 0x82, 1);
    system_exit(-1);
  }
  return array[0] * index + array[2];
}

/* Remove the element at index from a dynamic array, shifting subsequent
 * elements down via csmemmove, then shrink the allocation via debug_realloc.
 * Validates array integrity and index bounds before and after the operation.
 * 0x117ff0 / circular_queue.obj (array.c line 0x8b) */
void FUN_00117ff0(int *array, int index)
{
  int element_size;
  int new_count;
  int dest;
  void *new_ptr;

  if (array == (int *)0x0) {
    display_assert("array", "c:\\halo\\SOURCE\\memory\\array.c", 0x8b, 1);
    system_exit(-1);
  }
  if (array[0] < 1) {
    display_assert("array->element_size>0", "c:\\halo\\SOURCE\\memory\\array.c",
                   0x8c, 1);
    system_exit(-1);
  }
  if (array[1] < 0) {
    display_assert("array->count>=0", "c:\\halo\\SOURCE\\memory\\array.c", 0x8d,
                   1);
    system_exit(-1);
  }
  if ((array[1] != 0) != (array[2] != 0)) {
    display_assert("(array->count!=0)==(array->elements!=NULL)",
                   "c:\\halo\\SOURCE\\memory\\array.c", 0x8e, 1);
    system_exit(-1);
  }
  if ((index < 0) || (array[1] <= index)) {
    display_assert("index>=0 && index<array->count",
                   "c:\\halo\\SOURCE\\memory\\array.c", 0x8f, 1);
    system_exit(-1);
  }
  if (*(char *)0x449ef1 != 0 && *(char *)0x321a30 != 0) {
    profile_enter_private((void *)0x321a28);
  }
  element_size = array[0];
  new_count = array[1] - 1;
  array[1] = new_count;
  if (index < new_count) {
    dest = element_size * index + array[2];
    csmemmove((void *)dest, (const void *)(element_size + dest),
              (unsigned int)((new_count - index) * element_size));
  }
  new_ptr = debug_realloc((void *)array[2], element_size * array[1],
                          "c:\\halo\\SOURCE\\memory\\array.c", 0x9c);
  array[2] = (int)new_ptr;
  if ((array[1] != 0) != (array[2] != 0)) {
    display_assert("(array->count!=0)==(array->elements!=NULL)",
                   "c:\\halo\\SOURCE\\memory\\array.c", 0x9d, 1);
    system_exit(-1);
  }
  if (*(char *)0x449ef1 != 0 && *(char *)0x321a30 != 0) {
    profile_exit_private((void *)0x321a28);
  }
}

/* Initialize a small fixed-size array: zero the count byte and fill
 * all element slots with 0xff (sentinel/invalid). Validates all inputs.
 * 0x118190 / circular_queue.obj (array.c line 171) */
void FUN_00118190(unsigned char *count, int elements, short element_size,
                  short maximum_count)
{
  if (count == (unsigned char *)0x0) {
    display_assert("count", "c:\\halo\\SOURCE\\memory\\array.c", 0xab, 1);
    system_exit(-1);
  }
  if (elements == 0) {
    display_assert("elements", "c:\\halo\\SOURCE\\memory\\array.c", 0xac, 1);
    system_exit(-1);
  }
  if (element_size <= 0) {
    display_assert("element_size>0", "c:\\halo\\SOURCE\\memory\\array.c", 0xad,
                   1);
    system_exit(-1);
  }
  if (0xff < (int)maximum_count) {
    display_assert("maximum_count<=UNSIGNED_CHAR_MAX",
                   "c:\\halo\\SOURCE\\memory\\array.c", 0xae, 1);
    system_exit(-1);
  }
  *count = 0;
  csmemset((void *)elements, 0xffffffff,
           (int)element_size * (int)maximum_count);
}

/* Resize a small fixed-size array to new_count elements. Zeroes newly
 * allocated slots or fills freed slots with 0xff. Returns 1 on success,
 * 0 if new_count is out of range [0, maximum_count).
 * 0x118260 / circular_queue.obj (array.c line 191) */
int FUN_00118260(unsigned char *count, int elements, short element_size,
                 short maximum_count, short new_count)
{
  unsigned int uVar1;
  unsigned int uVar2;

  if (count == (unsigned char *)0x0) {
    display_assert("count && *count>=0", "c:\\halo\\SOURCE\\memory\\array.c",
                   0xbf, 1);
    system_exit(-1);
  }
  if (elements == 0) {
    display_assert("elements", "c:\\halo\\SOURCE\\memory\\array.c", 0xc0, 1);
    system_exit(-1);
  }
  if (element_size <= 0) {
    display_assert("element_size>0", "c:\\halo\\SOURCE\\memory\\array.c", 0xc1,
                   1);
    system_exit(-1);
  }
  if (0xff < (int)maximum_count) {
    display_assert("maximum_count<=UNSIGNED_CHAR_MAX",
                   "c:\\halo\\SOURCE\\memory\\array.c", 0xc2, 1);
    system_exit(-1);
  }
  if ((-1 < (int)new_count) && ((int)new_count < (int)maximum_count)) {
    if ((unsigned int)*count != (unsigned int)(int)new_count) {
      uVar1 = (int)element_size * (unsigned int)*count + elements;
      uVar2 = (int)element_size * (int)new_count + elements;
      if (uVar1 < uVar2) {
        csmemset((void *)uVar1, 0, uVar2 - uVar1);
        *count = (unsigned char)new_count;
        return 1;
      }
      csmemset((void *)uVar2, 0xffffffff, uVar1 - uVar2);
      *count = (unsigned char)new_count;
    }
    return 1;
  }
  return 0;
}

/* Append a new zeroed element to a small fixed-size array.
 * Returns the index of the new element, or 0xffff if the array is full.
 * 0x118370 / circular_queue.obj (array.c line 230) */
unsigned short FUN_00118370(unsigned char *count, int elements,
                            short element_size, short maximum_count)
{
  volatile short new_var;
  unsigned char bVar1;

  if (count == (unsigned char *)0x0) {
    display_assert("count && *count>=0", "c:\\halo\\SOURCE\\memory\\array.c",
                   0xe6, 1);
    system_exit(-1);
  }
  if (elements == 0) {
    display_assert("elements", "c:\\halo\\SOURCE\\memory\\array.c", 0xe7, 1);
    system_exit(-1);
  }
  if (element_size <= 0) {
    display_assert("element_size>0", "c:\\halo\\SOURCE\\memory\\array.c", 0xe8,
                   1);
    system_exit(-1);
  }
  if (0xff < (int)maximum_count) {
    display_assert("maximum_count<=UNSIGNED_CHAR_MAX",
                   "c:\\halo\\SOURCE\\memory\\array.c", 0xe9, 1);
    system_exit(-1);
  }
  bVar1 = *count;
  if ((short)(unsigned short)bVar1 < maximum_count) {
    new_var = (int)((short)(unsigned short)bVar1);
    *count = bVar1 + 1;
    csmemset((void *)(new_var * (int)element_size + elements), 0,
             (int)element_size);
    return (unsigned short)bVar1;
  }
  return (unsigned short)(-1);
}

/* Return the address of a specific element by index in a small fixed-size
 * array. Validates all inputs including index < *count.
 * 0x118460 / circular_queue.obj (array.c line 251) */
int FUN_00118460(unsigned char count, int elements, short element_size,
                 short index)
{
  if (count == 0) {
    display_assert("count>0", "c:\\halo\\SOURCE\\memory\\array.c", 0xfb, 1);
    system_exit(-1);
  }
  if (elements == 0) {
    display_assert("elements", "c:\\halo\\SOURCE\\memory\\array.c", 0xfc, 1);
    system_exit(-1);
  }
  if (element_size <= 0) {
    display_assert("element_size>0", "c:\\halo\\SOURCE\\memory\\array.c", 0xfd,
                   1);
    system_exit(-1);
  }
  if ((index < 0) || ((short)(unsigned short)count <= index)) {
    display_assert("index>=0 && index<count",
                   "c:\\halo\\SOURCE\\memory\\array.c", 0xfe, 1);
    system_exit(-1);
  }
  return (int)element_size * (int)index + elements;
}

/* Remove an element from a small fixed-size array by index, shifting
 * subsequent elements down and filling the vacated slot with 0xff.
 * 0x118520 / circular_queue.obj (array.c line 265) */
void FUN_00118520(unsigned char *count, int elements, short element_size,
                  short index)
{
  int iVar1;
  int iVar3;
  int iVar4;
  unsigned char bVar2;

  if ((count == (unsigned char *)0x0) || (*count == 0)) {
    display_assert("count && *count>0", "c:\\halo\\SOURCE\\memory\\array.c",
                   0x109, 1);
    system_exit(-1);
  }
  if (elements == 0) {
    display_assert("elements", "c:\\halo\\SOURCE\\memory\\array.c", 0x10a, 1);
    system_exit(-1);
  }
  if (element_size <= 0) {
    display_assert("element_size>0", "c:\\halo\\SOURCE\\memory\\array.c", 0x10b,
                   1);
    system_exit(-1);
  }
  if ((index < 0) || ((short)(unsigned short)*count <= index)) {
    display_assert("index>=0 && index<*count",
                   "c:\\halo\\SOURCE\\memory\\array.c", 0x10c, 1);
    system_exit(-1);
  }
  bVar2 = *count - 1;
  *count = bVar2;
  iVar4 = (int)index;
  if (iVar4 < (int)(unsigned int)bVar2) {
    iVar3 = (int)element_size;
    iVar1 = iVar3 * iVar4 + elements;
    csmemmove((void *)iVar1, (const void *)(iVar3 + iVar1),
              ((unsigned int)bVar2 - iVar4) * iVar3);
  }
  csmemset((void *)((unsigned int)*count * (int)element_size + elements),
           0xffffffff, (int)element_size);
}

/* FUN_00118620 (0x118620) — Byte-swap an array of elements in place.
 * element_size encodes the swap width: -2 = 2-byte, -4 = 4-byte, -8 = 8-byte.
 * 0x118620 / circular_queue.obj (byte_swapping_codes.c line 113) */
void FUN_00118620(void *data, int count, int element_size)
{
  unsigned short *p2;
  unsigned int *p4;
  unsigned long long *p8;
  int new_var;
  unsigned int v;
  unsigned long long v8;

  if (data == 0) {
    display_assert("memory",
                   "c:\\halo\\SOURCE\\memory\\byte_swapping_codes.c", 0x71, 1);
    system_exit(-1);
  }
  if (count < 0) {
    display_assert("count>=0",
                   "c:\\halo\\SOURCE\\memory\\byte_swapping_codes.c", 0x72, 1);
    system_exit(-1);
  }
  if (element_size != -2 && element_size != -4 &&
      (new_var = element_size) != -8) {
    display_assert(
        "code==_2byte || code==_4byte || code==_8byte",
        "c:\\halo\\SOURCE\\memory\\byte_swapping_codes.c", 0x73, 1);
    system_exit(-1);
  }

  switch (element_size) {
  case -8:
    p8 = (unsigned long long *)data;
    if (count > 0) {
      do {
        v8 = *p8;
        *p8 = ((*p8 >> 56) |
              ((*p8 >> 40) & 0xff00ULL) |
              ((*p8 >> 24) & 0xff0000ULL) |
              ((*p8 >> 8)  & 0xff000000ULL) |
              ((v8 << 8)   & 0xff00000000ULL) |
              ((*p8 << 24) & 0xff0000000000ULL) |
              ((*p8 << 40) & 0xff000000000000ULL) |
              (*p8 << 56));
        p8++;
        count--;
      } while (count != 0);
    }
    break;
  case -4:
    p4 = (unsigned int *)data;
    if (count > 0) {
      do {
        v = *p4;
        *p4 = (v >> 24) | ((v >> 8) & 0xff00) |
              ((v << 8) & 0xff0000) | (v << 24);
        p4++;
        count--;
      } while (count != 0);
    }
    break;
  case -2:
    p2 = (unsigned short *)data;
    if (count > 0) {
      do {
        *p2 = (unsigned short)((*p2 >> 8) | (*p2 << 8));
        p2++;
        count--;
      } while (count != 0);
    }
    break;
  }
}

/* Byte-swap interpreter: walk a byte-swap code array and swap fields (0x1187f0).
 * Handles 2/4/8-byte swaps, nested struct references, and array repeats.
 * out_size receives the total data offset processed, out_step receives
 * the number of code words consumed. */
void FUN_001187f0(void *bs_definition, int data_ptr, int *codes,
                  int *out_size, int *out_step)
{
  int *def = (int *)bs_definition;
  char *msg;
  int code;
  int offset;
  int step;
  int array_count;
  int local_size;
  int local_step;
  unsigned int v4;
  unsigned int v8_lo, v8_hi;

  if (def[3] != 0x62797377) {
    msg = csprintf((char *)0x5ab100,
                   "got bs data with bad signature (assuming name is wrong)",
                   "c:\\halo\\SOURCE\\memory\\byte_swapping.c", 0xb0, 0);
    display_assert(msg, 0, 0, 0);
    if (def[3] != 0x62797377) {
      msg = csprintf((char *)0x5ab100, "%s bs data has bad signature", *(char **)def,
                     "c:\\halo\\SOURCE\\memory\\byte_swapping.c", 0xb2, 1);
      display_assert(msg, 0, 0, 0);
      system_exit(-1);
    }
  }

  if (codes[0] != -100) {
    msg = csprintf((char *)0x5ab100,
                   "%s bs data @%p.#0 has bad start #%d",
                   *(char **)def, codes, *(int *)def[2],
                   "c:\\halo\\SOURCE\\memory\\byte_swapping.c", 0xb7, 1);
    display_assert(msg, 0, 0, 0);
    system_exit(-1);
  }

  array_count = codes[1];
  if (array_count < 0) {
    msg = csprintf((char *)0x5ab100,
                   "%s bs data @%p.#1 has invalid array size #%d",
                   *(char **)def, codes, array_count,
                   "c:\\halo\\SOURCE\\memory\\byte_swapping.c", 0xbd, 1);
    display_assert(msg, 0, 0, 0);
    system_exit(-1);
  }

  offset = 0;
  if (array_count < 1)
    goto done;

  do {
    step = 2;
    for (;;) {
      code = codes[step];
      switch (code) {
      case -2:
        if (data_ptr != 0) {
          unsigned short w = *(unsigned short *)(offset + data_ptr);
          *(unsigned short *)(offset + data_ptr) =
              (unsigned short)((w >> 8) | (w << 8));
        }
        step = step + 1;
        offset = offset + 2;
        break;

      case -4:
        if (data_ptr != 0) {
          v4 = *(unsigned int *)(offset + data_ptr);
          *(unsigned int *)(offset + data_ptr) =
              (((v4 & 0xff0000) | (v4 >> 16)) >> 8) |
              (((v4 << 16) | (v4 & 0xff00)) << 8);
        }
        step = step + 1;
        offset = offset + 4;
        break;

      case -8:
        if (data_ptr != 0) {
          v8_lo = *(unsigned int *)(offset + data_ptr);
          v8_hi = *(unsigned int *)(offset + data_ptr + 4);
          *(unsigned int *)(offset + data_ptr) =
              (((v8_hi >> 16) | (((v8_hi & 0xff0000) >> 16) | (v8_hi & 0xff00)) << 16) >> 8) |
              (v8_hi << 24);
          *(unsigned int *)(offset + data_ptr + 4) =
              (((v8_lo << 16) | (((v8_lo & 0xff00) << 16) | (v8_lo & 0xff0000)) >> 16) << 8) |
              (v8_lo >> 24);
        }
        step = step + 1;
        offset = offset + 8;
        break;

      case -100: {
        int sub_data;
        if (data_ptr == 0)
          sub_data = 0;
        else
          sub_data = offset + data_ptr;
        FUN_001187f0(bs_definition, sub_data, codes + step, &local_size,
                     &local_step);
        step = step + local_step;
        offset = offset + local_size;
        break;
      }

      case -102: {
        int ref_def = codes[step + 1];
        int sub_data;
        if (data_ptr == 0)
          sub_data = 0;
        else
          sub_data = offset + data_ptr;
        FUN_001187f0((void *)ref_def, sub_data, *(int **)(ref_def + 8),
                     &local_size, 0);
        step = step + 2;
        offset = offset + local_size;
        break;
      }

      case -101:
        goto next_iteration;

      default:
        if (code < 1) {
          msg = csprintf((char *)0x5ab100,
                         "%s bs @%p.#%d has invalid code #%d",
                         *(char **)def, codes, step, code,
                         "c:\\halo\\SOURCE\\memory\\byte_swapping.c", 0x129, 1);
          display_assert(msg, 0, 0, 0);
          system_exit(-1);
        } else {
          step = step + 1;
          offset = offset + code;
        }
        break;
      }
    }
  next_iteration:
    step = step + 1;
    array_count = array_count - 1;
  } while (array_count != 0);

done:
  if (out_size != 0)
    *out_size = offset;
  if (out_step != 0)
    *out_step = step;
}

/* Compute the byte size described by a byte-swap definition by walking
 * the code array with null data. Returns the computed size.
 * 0x118ba0 / circular_queue.obj (byte_swapping.c) */
int FUN_00118ba0(const char *name, int *codes)
{
  int definition[4];
  int out_size;
  int out_step;

  definition[0] = (int)name;
  definition[1] = 0;
  definition[2] = (int)codes;
  definition[3] = 0x62797377;
  FUN_001187f0(definition, 0, codes, &out_size, &out_step);
  return out_size;
}

/* Byte-swap all count instances of data according to the given definition.
 * Validates the definition on first use (computes and caches size). If data
 * is non-null and count > 0, walks each element through the byte-swap codes.
 * 0x118be0 / circular_queue.obj (byte_swapping.c line 77) */
void FUN_00118be0(void *definition, void *data, int count)
{
  int *def;
  int computed_size;
  int out_step;
  int i;

  if (definition == (void *)0x0) {
    display_assert("definition", "c:\\halo\\SOURCE\\memory\\byte_swapping.c",
                   0x4d, 1);
    system_exit(-1);
  }
  def = (int *)definition;
  if ((*(char *)((int *)def + 4) == '\0') && (def[1] >= 0)) {
    FUN_001187f0(def, 0, (int *)def[2], &computed_size, &out_step);
    if (computed_size != def[1]) {
      display_assert(csprintf((char *)0x5ab100,
                              "%s bs data @%p is #%d but should be #%d bytes",
                              (const char *)def[0], def, computed_size, def[1]),
                     "c:\\halo\\SOURCE\\memory\\byte_swapping.c", 0x58, 1);
      system_exit(-1);
    }
    *(char *)((int *)def + 4) = '\x01';
  }
  if ((data != (void *)0x0) && (count > 0)) {
    for (i = 0; i < count; i++) {
      FUN_001187f0(def, def[1] * i + (int)data, (int *)def[2], (int *)0x0,
                   (int *)0x0);
    }
  }
}

/* Build a byte-swap definition on the stack and invoke FUN_00118be0 to
 * byte-swap data_count instances of data. Validates codes, data_count,
 * and size before constructing the definition struct.
 * 0x118cb0 / circular_queue.obj (byte_swapping.c line 40) */
void FUN_00118cb0(const char *name, int size, int *codes, int data_count,
                  void *data)
{
  int definition[5];

  if (codes == (int *)0x0) {
    display_assert("codes", "c:\\halo\\SOURCE\\memory\\byte_swapping.c", 0x28,
                   1);
    system_exit(-1);
  }
  if (data_count < 0) {
    display_assert("data_count>=0", "c:\\halo\\SOURCE\\memory\\byte_swapping.c",
                   0x29, 1);
    system_exit(-1);
  }
  if (size < 0) {
    display_assert("size>=0", "c:\\halo\\SOURCE\\memory\\byte_swapping.c", 0x2a,
                   1);
    system_exit(-1);
  }
  definition[0] = (int)name;
  definition[1] = size;
  definition[2] = (int)codes;
  definition[3] = 0x62797377;
  *(char *)&definition[4] = (data != (void *)0x0);
  FUN_00118be0(definition, data, data_count);
}

/* Reset a circular queue by zeroing the write and read offsets.
 * Sets write_offset (queue+0xc) and read_offset (queue+0x8) to zero.
 * 0x118d60 / circular_queue.obj */
void circular_queue_reset(int queue)
{
  *(int *)(queue + 0xc) = 0;
  *(int *)(queue + 0x8) = 0;
}

/* Validate that a circular queue structure is not corrupt (0x118d70).
 * Checks: non-null pointer, signature == "circ" (0x63697263), non-null buffer,
 * positive size, and read/write offsets within [0, size). If any check fails,
 * reports the corruption via display_assert and halts with system_exit(-1). */
void FUN_00118d70(int queue)
{
  int size;

  if (queue != 0 && *(int *)(queue + 0x04) == 0x63697263 &&
      *(int *)(queue + 0x14) != 0 &&
      (size = *(int *)(queue + 0x10), size > 0) &&
      *(int *)(queue + 0x08) >= 0 && *(int *)(queue + 0x08) < size &&
      *(int *)(queue + 0x0c) >= 0 && *(int *)(queue + 0x0c) < size) {
    return;
  }

  display_assert(csprintf((char *)0x5ab100,
                          "the circular queue @%p appears to be corrupt.",
                          (void *)queue),
                 "c:\\halo\\SOURCE\\memory\\circular_queue.c", 0xcc, 1);
  system_exit(-1);
}

/* Allocate and initialize a circular queue with a buffer of param_2 bytes.
 * Returns the queue pointer or NULL if allocation fails.
 * 0x118de0 / circular_queue.obj
 */
int *circular_queue_new(int param_1, int param_2)
{
  int *puVar1;

  puVar1 = (int *)debug_malloc(
    param_2 + 0x19, 0, "c:\\halo\\SOURCE\\memory\\circular_queue.c", 0x34);
  if (puVar1 != (int *)0) {
    csmemset(puVar1, 0, 0x18);
    *puVar1 = param_1;
    puVar1[1] = 0x63697263;
    puVar1[4] = param_2 + 1;
    puVar1[5] = (int)(puVar1 + 6);
    FUN_00118d70((int)puVar1);
  }
  return puVar1;
}

/* Free a circular queue and its memory. 0x118e40 / circular_queue.obj */
void circular_queue_delete(int queue)
{
  FUN_00118d70(queue);
  debug_free((void *)queue, "c:\\halo\\SOURCE\\memory\\circular_queue.c", 0x48);
}

/* Return the number of bytes currently used (queued) in a circular queue.
 * Validates the queue, then computes write_offset - read_offset, wrapping
 * via buffer_size when the result is negative.
 * 0x118e70 / circular_queue.obj */
int circular_queue_size(int queue)
{
  int used;

  FUN_00118d70(queue);
  used = *(int *)(queue + 0xc) - *(int *)(queue + 0x8);
  if (used < 0)
    used = used + *(int *)(queue + 0x10);
  return used;
}

/* Return the number of free bytes available in a circular queue (0x118e90).
 * Computes: buffer_size - used - 1, where used = (write_offset - read_offset),
 * wrapping around via buffer_size when write_offset < read_offset. The -1
 * accounts for the sentinel gap that distinguishes full from empty. */
unsigned int circular_queue_free_space(int queue)
{
  int used;

  FUN_00118d70(queue);

  used = *(int *)(queue + 0x0c) - *(int *)(queue + 0x08);
  if (used < 0) {
    used = used + *(int *)(queue + 0x10);
  }
  return *(int *)(queue + 0x10) - used - 1;
}

/* Enqueue data into a circular queue, wrapping around the buffer boundary
 * if necessary (0x118ec0). Returns true if the data was enqueued, or false
 * if the queue does not have enough space. Handles the wrap-around case by
 * splitting the copy into two parts: one to the end of the buffer, and one
 * from the beginning. Asserts validity of the queue before and after. */
bool FUN_00118ec0(int queue, void *data, int data_size)
{
  int write_offset;
  int used;
  int remaining;

  FUN_00118d70(queue);
  assert_halt(data && data_size > 0 && data_size < *(int *)(queue + 0x10));

  FUN_00118d70(queue);

  write_offset = *(int *)(queue + 0x0c);
  used = write_offset - *(int *)(queue + 0x08);
  if (used < 0) {
    used = used + *(int *)(queue + 0x10);
  }

  if (used + data_size < *(int *)(queue + 0x10)) {
    remaining = *(int *)(queue + 0x10) - write_offset;
    if (data_size >= remaining) {
      csmemcpy((void *)(*(int *)(queue + 0x14) + write_offset), data,
               remaining);
      data = (char *)data + remaining;
      *(int *)(queue + 0x0c) = 0;
      data_size = data_size - remaining;
    }

    if (data_size > 0) {
      csmemcpy((void *)(*(int *)(queue + 0x14) + *(int *)(queue + 0x0c)), data,
               data_size);
      *(int *)(queue + 0x0c) = *(int *)(queue + 0x0c) + data_size;
    }

    assert_halt(*(int *)(queue + 0x0c) >= 0 &&
                *(int *)(queue + 0x0c) < *(int *)(queue + 0x10));
    return 1;
  }
  return 0;
}

/* Try to read data_size bytes from a circular queue without removing them
 * unless advance is set. Returns true if enough data was available, false
 * otherwise. Handles the wrap-around case the same way as the enqueue path:
 * split copy at the buffer boundary (0x118fb0). */
bool circular_queue_try_read(int queue, void *data, int data_size, char advance)
{
  int read_offset;
  int available;
  int remaining;

  FUN_00118d70(queue);
  assert_halt(data && data_size > 0 && data_size < *(int *)(queue + 0x10));

  FUN_00118d70(queue);

  read_offset = *(int *)(queue + 0x8);
  available = *(int *)(queue + 0xc) - read_offset;
  if (available < 0) {
    available = available + *(int *)(queue + 0x10);
  }

  if (data_size > available) {
    return 0;
  }

  remaining = *(int *)(queue + 0x10) - read_offset;
  if (data_size >= remaining) {
    csmemcpy(data, (void *)(*(int *)(queue + 0x14) + read_offset), remaining);
    data = (char *)data + remaining;
    read_offset = 0;
    data_size = data_size - remaining;
  }

  if (data_size > 0) {
    csmemcpy(data, (void *)(*(int *)(queue + 0x14) + read_offset), data_size);
    read_offset = read_offset + data_size;
  }

  assert_halt(read_offset >= 0 && read_offset < *(int *)(queue + 0x10));

  if (advance != 0) {
    *(int *)(queue + 0x8) = read_offset;
  }

  return 1;
}
