/* inflate_blocks_free: reset blocks state, free window, workaround, and state.
 * 0x114630 / circular_queue.obj (inflate.c) */
int inflate_blocks_free(int s, int z)
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
void inflate_set_dictionary(int s, int d, int n)
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
void *inflate_codes_new(int bl, int bd, int tl, int td, int z)
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

/* zlib infutil.h macro set, as the original was compiled with.  Each use site
 * expands the same text, which is what produces the reference's repeated
 * update/reload blocks and lets the compiler tail-merge them itself. */
#define z_verbose (*(int *)0x320e30)
#define z_stderr (*(void **)0x331070)
#define inflate_mask ((const unsigned int *)0x320d88)
#define Tracevv(x)                                                             \
  {                                                                            \
    if (z_verbose > 1)                                                         \
      crt_fprintf x;                                                           \
  }
#define Assert(cond, msg)                                                      \
  {                                                                            \
    if (!(cond))                                                               \
      FUN_00117a80(msg);                                                       \
  }

/* update pointers and return */
#define UPDBITS                                                                \
  {                                                                            \
    *(unsigned int *)(s + 0x20) = b;                                           \
    *(unsigned int *)(s + 0x1c) = k;                                           \
  }
#define UPDIN                                                                  \
  {                                                                            \
    z[1] = (int)n;                                                             \
    z[2] = (int)(p + (z[2] - *z));                                             \
    *z = (int)p;                                                               \
  }
#define UPDOUT { *(unsigned char **)(s + 0x34) = q; }
#define UPDATE { UPDBITS UPDIN UPDOUT }
#define LEAVE { UPDATE return FUN_00116280((int)s, (int)z, r); }

/* get bytes and bits */
#define LOADIN                                                                 \
  {                                                                            \
    p = (unsigned char *)*z;                                                   \
    n = (unsigned int)z[1];                                                    \
    b = *(unsigned int *)(s + 0x20);                                           \
    k = *(unsigned int *)(s + 0x1c);                                           \
  }
#define NEEDBYTE                                                               \
  {                                                                            \
    if (n != 0)                                                                \
      r = 0;                                                                   \
    else                                                                       \
      LEAVE                                                                    \
  }
#define NEXTBYTE (n--, *p++)
#define NEEDBITS(j)                                                            \
  {                                                                            \
    while (k < (j)) {                                                          \
      NEEDBYTE                                                                 \
      b |= (unsigned int)NEXTBYTE << k;                                        \
      k += 8;                                                                  \
    }                                                                          \
  }
#define DUMPBITS(j)                                                            \
  {                                                                            \
    b >>= (j);                                                                 \
    k -= (j);                                                                  \
  }

/* output bytes */
#define WAVAIL                                                                 \
  ((unsigned int)(q < *(unsigned char **)(s + 0x30)                            \
                      ? *(int *)(s + 0x30) + (-1 - (int)q)                     \
                      : *(int *)(s + 0x2c) - (int)q))
#define LOADOUT                                                                \
  {                                                                            \
    q = *(unsigned char **)(s + 0x34);                                         \
    m = WAVAIL;                                                                \
  }
#define WRAP                                                                   \
  {                                                                            \
    if (q == *(unsigned char **)(s + 0x2c) &&                                  \
        *(unsigned char **)(s + 0x30) != *(unsigned char **)(s + 0x28)) {      \
      q = *(unsigned char **)(s + 0x28);                                       \
      m = WAVAIL;                                                              \
    }                                                                          \
  }
#define FLUSH                                                                  \
  {                                                                            \
    UPDOUT                                                                     \
    r = FUN_00116280((int)s, (int)z, r);                                       \
    LOADOUT                                                                    \
  }
#define NEEDOUT                                                                \
  {                                                                            \
    if (m == 0) {                                                              \
      WRAP                                                                     \
      if (m == 0) {                                                            \
        FLUSH                                                                  \
        WRAP                                                                   \
        if (m == 0)                                                            \
          LEAVE                                                                \
      }                                                                        \
    }                                                                          \
    r = 0;                                                                     \
  }
#define OUTBYTE(a)                                                             \
  {                                                                            \
    *q++ = (unsigned char)(a);                                                 \
    m--;                                                                       \
  }

/* load local pointers */
#define LOAD { LOADIN LOADOUT }

/* inflate_codes: decode literal/length and distance codes until an
 * end-of-block code or until enough input/output is available to hand off to
 * inflate_fast.  0x114740 / circular_queue.obj (infcodes.c)
 *
 * s = inflate_blocks_state, z = z_stream, r = incoming return code.  Returns
 * the value produced by inflate_flush at whichever LEAVE is taken.
 *
 * codes state (c) layout: [0] mode, [1] len, [2] sub.code.tree /
 * sub.copy.get / sub.lit, [3] sub.code.need / sub.copy.dist, +0x10 lbits,
 * +0x11 dbits, [5] ltree, [6] dtree. */
int inflate_codes(unsigned int s, int *z, int r)
{
  unsigned int j;   /* temporary storage */
  unsigned char *t; /* temporary pointer */
  unsigned int e;   /* extra bits or operation */
  unsigned int b;   /* bit buffer */
  unsigned int k;   /* bits in bit buffer */
  unsigned char *p; /* input data pointer */
  unsigned int n;   /* bytes available there */
  unsigned char *q; /* output window write pointer */
  unsigned int m;   /* bytes to end of window or read pointer */
  unsigned char *f; /* pointer to copy strings from */
  unsigned int *c;  /* codes state */

  c = *(unsigned int **)(s + 4);

  /* copy input/output information to locals (UPDATE macro restores) */
  LOAD

  /* process input and output based on current state */
  while (1)
    switch (*c) {
    /* waiting for "i:"=input, "o:"=output, "x:"=nothing */
    case 0: /* START: x: set up for LEN */
      if (m >= 258 && n >= 10) {
        UPDATE
        r = FUN_00114fa0((int)*((unsigned char *)c + 0x10),
                         (int)*((unsigned char *)c + 0x11), (int)c[5],
                         (int)c[6], (int)s, (int)z);
        LOAD
        if (r != 0) {
          *c = r == 1 ? 7 : 9;
          break;
        }
      }
      c[3] = (unsigned int)*((unsigned char *)c + 0x10);
      c[2] = c[5];
      *c = 1;
      /* fall through */
    case 1: /* LEN: i: get length/literal/eob next */
      j = c[3];
      NEEDBITS(j)
      t = (unsigned char *)(c[2] + (inflate_mask[j] & b) * 8);
      DUMPBITS(t[1])
      e = (unsigned int)*t;
      if (e == 0) { /* literal */
        c[2] = *(unsigned int *)(t + 4);
        Tracevv((z_stderr,
                 c[2] >= 0x20 && c[2] < 0x7f
                     ? "inflate:         literal '%c'\n"
                     : "inflate:         literal 0x%02x\n",
                 c[2]));
        *c = 6;
        break;
      }
      if ((e & 0x10) != 0) { /* length */
        c[2] = e & 0xf;
        c[1] = *(unsigned int *)(t + 4);
        *c = 2;
        break;
      }
      if ((e & 0x40) == 0) { /* next table */
        c[3] = e;
        c[2] = (unsigned int)(t + *(int *)(t + 4) * 8);
        break;
      }
      if ((e & 0x20) != 0) { /* end of block */
        Tracevv((z_stderr, "inflate:         end of block\n"));
        *c = 7;
        break;
      }
      *c = 9; /* invalid code */
      z[6] = (int)"invalid literal/length code";
      r = -3;
      LEAVE
    case 2: /* LENEXT: i: getting length extra (have base) */
      j = c[2];
      NEEDBITS(j)
      c[1] += inflate_mask[j] & b;
      DUMPBITS(j)
      c[3] = (unsigned int)*((unsigned char *)c + 0x11);
      c[2] = c[6];
      Tracevv((z_stderr, "inflate:         length %u\n", c[1]));
      *c = 3;
      /* fall through */
    case 3: /* DIST: i: get distance next */
      j = c[3];
      NEEDBITS(j)
      t = (unsigned char *)(c[2] + (inflate_mask[j] & b) * 8);
      DUMPBITS(t[1])
      e = (unsigned int)*t;
      if ((e & 0x10) != 0) { /* distance */
        c[2] = e & 0xf;
        c[3] = *(unsigned int *)(t + 4);
        *c = 4;
        break;
      }
      if ((e & 0x40) == 0) { /* next table */
        c[3] = e;
        c[2] = (unsigned int)(t + *(int *)(t + 4) * 8);
        break;
      }
      *c = 9; /* invalid code */
      z[6] = (int)"invalid distance code";
      r = -3;
      LEAVE
    case 4: /* DISTEXT: i: getting distance extra */
      j = c[2];
      NEEDBITS(j)
      c[3] += inflate_mask[j] & b;
      DUMPBITS(j)
      Tracevv((z_stderr, "inflate:         distance %u\n", c[3]));
      *c = 5;
      /* fall through */
    case 5: /* COPY: o: copying bytes in window, waiting for space */
      f = q - c[3];
      /* modulo window size -- handles invalid distances */
      if ((unsigned int)((int)q - *(int *)(s + 0x28)) < c[3])
        f += *(int *)(s + 0x2c) - *(int *)(s + 0x28);
      while (c[1] != 0) {
        NEEDOUT
        OUTBYTE(*f++)
        if (f == *(unsigned char **)(s + 0x2c))
          f = *(unsigned char **)(s + 0x28);
        c[1]--;
      }
      *c = 0;
      break;
    case 6: /* LIT: o: got literal, waiting for output space */
      NEEDOUT
      OUTBYTE(c[2])
      *c = 0;
      break;
    case 7: /* WASH: o: got eob, possibly more output */
      if (k > 7) { /* return unused byte, if any */
        Assert(k < 16, "inflate_codes grabbed too many bytes")
        k -= 8;
        n++;
        p--; /* can always return one */
      }
      FLUSH
      if (*(unsigned char **)(s + 0x30) != *(unsigned char **)(s + 0x34))
        LEAVE
      *c = 8;
      /* fall through */
    case 8: /* END */
      r = 1;
      LEAVE
    case 9: /* BADCODE: x: got error */
      r = -3;
      LEAVE
    default:
      r = -2;
      LEAVE
    }
}

#undef z_verbose
#undef z_stderr
#undef inflate_mask
#undef Tracevv
#undef Assert
#undef UPDBITS
#undef UPDIN
#undef UPDOUT
#undef UPDATE
#undef LEAVE
#undef LOADIN
#undef NEEDBYTE
#undef NEXTBYTE
#undef NEEDBITS
#undef DUMPBITS
#undef WAVAIL
#undef LOADOUT
#undef WRAP
#undef FLUSH
#undef NEEDOUT
#undef OUTBYTE
#undef LOAD

/* inflate_codes_free: free a codes state struct.
 * 0x114f60 / circular_queue.obj (inflate.c) */
void inflate_codes_free(int c, int z)
{
  ((void (*)(void *, void *))(*(void **)(z + 0x24)))(*(void **)(z + 0x28),
                                                     (void *)c);
  if (*(int *)0x320e30 > 0)
    crt_fprintf(*(void **)0x331070, "inflate:       codes free\n");
}

/* inflate_fast: fast inner loop for inflate_codes when enough input/output
 * available. Processes literal/length/distance codes without per-byte checks.
 * 0x114fa0 / circular_queue.obj (inffast.c)
 * param_1=bl (literal bits), param_2=bd (distance bits),
 * param_3=tl (literal table), param_4=td (distance table),
 * param_5=s (block state), param_6=z (z_stream) */
__declspec(noinline) int FUN_00114fa0(int param_1, int param_2,
                                      int param_3, int param_4,
                                      int param_5, int *param_6)
{
  unsigned char *t;
  int bits;
  unsigned char exop;
  unsigned int md;
  unsigned int tmp_e;
  int tmp_i;
  unsigned int tmp_c;
  char *fmt;
  unsigned int copy_len;
  unsigned int copy_dist;
  unsigned int k;
  unsigned int b;
  unsigned char *m_ptr;
  unsigned char *out_ptr;
  unsigned char *in_ptr;
  unsigned int n;

  in_ptr = (unsigned char *)*param_6;
  n = (unsigned int)param_6[1];
  out_ptr = *(unsigned char **)(param_5 + 0x34);
  k = *(unsigned int *)(param_5 + 0x1c);
  b = *(unsigned int *)(param_5 + 0x20);
  if (out_ptr < *(unsigned char **)(param_5 + 0x30)) {
    m_ptr =
      (unsigned char *)(*(int *)(param_5 + 0x30) + (-1 - (int)out_ptr));
  } else {
    m_ptr = (unsigned char *)(*(int *)(param_5 + 0x2c) - (int)out_ptr);
  }
  tmp_c = *(unsigned int *)(0x320d88 + param_1 * 4);
  md = *(unsigned int *)(0x320d88 + param_2 * 4);
  do {
    for (; k < 0x14; k = k + 8) {
      n = n - 1;
      b = b | (unsigned int)*in_ptr << (unsigned char)k;
      in_ptr = in_ptr + 1;
    }
    t = (unsigned char *)(param_3 + (tmp_c & b) * 8);
    bits = t[1];
    exop = *t;
    tmp_e = (unsigned int)exop;
    b = b >> bits;
    if (tmp_e == 0) {
      goto LAB_001151e9;
    }
    k = k - t[1];
    while ((exop & 0x10) == 0) {
      if ((tmp_e & 0x40) != 0) {
        if ((tmp_e & 0x20) != 0) {
          if (*(int *)0x320e30 > 1) {
            crt_fprintf(*(void **)0x331070,
                        "inflate:         * end of block\n");
          }
          tmp_c = (unsigned int)param_6[1] - n;
          if (k >> 3 < (unsigned int)param_6[1] - n) {
            tmp_c = k >> 3;
          }
          *(unsigned int *)(param_5 + 0x20) = b;
          *(unsigned int *)(param_5 + 0x1c) = k + tmp_c * (unsigned int)-8;
          tmp_i = *param_6;
          param_6[1] = (int)(tmp_c + n);
          *param_6 = (int)in_ptr - (int)tmp_c;
          param_6[2] = param_6[2] + (((int)in_ptr - (int)tmp_c) - tmp_i);
          *(unsigned char **)(param_5 + 0x34) = out_ptr;
          return 1;
        }
        param_6[6] = (int)"invalid literal/length code";
        tmp_c = (unsigned int)param_6[1] - n;
        if (k >> 3 < (unsigned int)param_6[1] - n) {
          tmp_c = k >> 3;
        }
        *(unsigned int *)(param_5 + 0x20) = b;
        *(unsigned int *)(param_5 + 0x1c) = k + tmp_c * (unsigned int)-8;
        tmp_i = *param_6;
        param_6[1] = (int)(tmp_c + n);
        *param_6 = (int)in_ptr - (int)tmp_c;
        param_6[2] = param_6[2] + (((int)in_ptr - (int)tmp_c) - tmp_i);
        *(unsigned char **)(param_5 + 0x34) = out_ptr;
        return (int)0xfffffffd;
      }
      tmp_i = (*(unsigned int *)(0x320d88 + tmp_e * 4) & b) +
              *(int *)(t + 4);
      bits = t[tmp_i * 8 + 1];
      t = t + tmp_i * 8;
      exop = *t;
      tmp_e = (unsigned int)exop;
      b = b >> bits;
      if (tmp_e == 0)
        goto LAB_001151e9;
      k = k - t[1];
    }
    tmp_e = tmp_e & 0xf;
    copy_len =
      (*(unsigned int *)(0x320d88 + tmp_e * 4) & b) + *(int *)(t + 4);
    k = k - tmp_e;
    b = b >> (unsigned char)tmp_e;
    if (*(int *)0x320e30 > 1) {
      crt_fprintf(*(void **)0x331070, "inflate:         * length %u\n", copy_len);
    }
    for (; k < 0xf; k = k + 8) {
      n = n - 1;
      b = b | (unsigned int)*in_ptr << (unsigned char)k;
      in_ptr = in_ptr + 1;
    }
    t = (unsigned char *)(param_4 + (md & b) * 8);
    b = b >> t[1];
    k = k - t[1];
    exop = *t;
    while ((exop & 0x10) == 0) {
      if ((exop & 0x40) != 0) {
        param_6[6] = (int)"invalid distance code";
        tmp_c = (unsigned int)param_6[1] - n;
        if (k >> 3 < (unsigned int)param_6[1] - n) {
          tmp_c = k >> 3;
        }
        *(unsigned int *)(param_5 + 0x20) = b;
        *(unsigned int *)(param_5 + 0x1c) = k + tmp_c * (unsigned int)-8;
        param_6[1] = (int)(tmp_c + n);
        param_6[2] = param_6[2] + (((int)in_ptr - (int)tmp_c) - *param_6);
        *param_6 = (int)in_ptr - (int)tmp_c;
        *(unsigned char **)(param_5 + 0x34) = out_ptr;
        return (int)0xfffffffd;
      }
      tmp_i = (*(unsigned int *)(0x320d88 + (unsigned int)exop * 4) & b) +
              *(int *)(t + 4);
      {
        unsigned char *bits_ptr;
        bits_ptr = t + tmp_i * 8 + 1;
        t = t + tmp_i * 8;
        b = b >> *bits_ptr;
        k = k - *bits_ptr;
        exop = *t;
      }
    }
    tmp_e = exop & 0xf;
    for (; k < tmp_e; k = k + 8) {
      n = n - 1;
      b = b | (unsigned int)*in_ptr << (unsigned char)k;
      in_ptr = in_ptr + 1;
    }
    copy_dist =
      (*(unsigned int *)(0x320d88 + tmp_e * 4) & b) + *(int *)(t + 4);
    k = k - tmp_e;
    b = b >> (unsigned char)tmp_e;
    if (*(int *)0x320e30 > 1) {
      crt_fprintf(*(void **)0x331070, "inflate:         * distance %u\n",
                  copy_dist);
    }
    m_ptr = m_ptr - copy_len;
    if ((unsigned int)((int)out_ptr - *(int *)(param_5 + 0x28)) < copy_dist) {
      copy_dist =
        (unsigned int)(*(int *)(param_5 + 0x28) - (int)out_ptr) + copy_dist;
      t = (unsigned char *)(*(int *)(param_5 + 0x2c) - copy_dist);
      if (copy_dist < copy_len) {
        copy_len = copy_len - copy_dist;
        do {
          *out_ptr = *t;
          out_ptr = out_ptr + 1;
          t = t + 1;
          copy_dist = copy_dist - 1;
        } while (copy_dist != 0);
        t = *(unsigned char **)(param_5 + 0x28);
      }
    } else {
      t = out_ptr + -(int)copy_dist;
      *out_ptr = *t;
      out_ptr[1] = t[1];
      out_ptr = out_ptr + 2;
      t = t + 2;
      copy_len = copy_len - 2;
    }
    do {
      *out_ptr = *t;
      out_ptr = out_ptr + 1;
      t = t + 1;
      copy_len = copy_len - 1;
    } while (copy_len != 0);
    goto LAB_0011522d;
  LAB_001151e9:
    k = k - bits;
    if (*(int *)0x320e30 > 1) {
      tmp_e = *(unsigned int *)(t + 4);
      if (tmp_e < 0x20 ||
          (fmt = "inflate:         * literal \'%c\'\n", 0x7e < tmp_e)) {
        fmt = "inflate:         * literal 0x%02x\n";
      }
      crt_fprintf(*(void **)0x331070, fmt, tmp_e);
    }
    *out_ptr = t[4];
    out_ptr = out_ptr + 1;
    m_ptr = m_ptr + -1;
  LAB_0011522d:
    if (m_ptr < (unsigned char *)0x102 || n < 10) {
      tmp_c = (unsigned int)param_6[1] - n;
      if (k >> 3 < (unsigned int)param_6[1] - n) {
        tmp_c = k >> 3;
      }
      *(unsigned int *)(param_5 + 0x20) = b;
      *(unsigned int *)(param_5 + 0x1c) = k + tmp_c * (unsigned int)-8;
      tmp_i = *param_6;
      param_6[1] = (int)(tmp_c + n);
      *param_6 = (int)in_ptr - (int)tmp_c;
      param_6[2] = param_6[2] + (((int)in_ptr - (int)tmp_c) - tmp_i);
      *(unsigned char **)(param_5 + 0x34) = out_ptr;
      return 0;
    }
  } while (1);
}

/* inflateReset: reset inflate stream state to initial values.
 * 0x1153c0 / circular_queue.obj (inflate.c) */
int inflateReset(int z)
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
      inflate_blocks_free(blocks, z);
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
  int state;
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
  state = (int)(*(void *(*)(void *, unsigned int, unsigned int))(
    *(void **)(z + 0x20)))(*(void **)(z + 0x28), 1, 0x18);
  *(int *)(z + 0x1c) = state;
  if (state != 0) {
    adler_fn = 0x0c;
    *(int *)(state + 0x14) = 0;
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
      inflateReset(z);
      return 0;
    }
  }
  return (int)0xfffffffc;
}

/* inflateInit: initialize inflate stream with default window bits
 * (MAX_WBITS=15). 0x1155c0 / circular_queue.obj (inflate.c) */
int inflateInit_(int z, char *version, int stream_size)
{
  return FUN_001154a0(z, 0xf, version, stream_size);
}

/* inflate: main decompression state machine (zlib).
 * 0x1155e0 / circular_queue.obj (inflate.c) */
int FUN_001155e0(int z, int flush)
{
  unsigned char flags_byte;
  int mode;
  int *state;
  unsigned int default_ret;
  unsigned int result;
  int *param_1 = (int *)z;
  int param_2 = flush;

  if (param_1 == (int *)0 || (int *)param_1[7] == (int *)0 || *param_1 == 0) {
    return 0xfffffffe;
  }
  mode = *(int *)param_1[7];
  result = 0xfffffffb;
  default_ret = (unsigned int)(param_2 != 4) - 1 & 0xfffffffb;
  do {
    switch (mode) {
    case 0:
      if (param_1[1] == 0)
        return result;
      param_1[1] = param_1[1] - 1;
      param_1[2] = param_1[2] + 1;
      *(unsigned int *)(param_1[7] + 4) =
        (unsigned int)*(unsigned char *)*param_1;
      state = (int *)param_1[7];
      mode = state[1];
      *param_1 = *param_1 + 1;
      if (((unsigned char)mode & 0xf) == 8) {
        if (((unsigned int)state[1] >> 4) + 8 <= (unsigned int)state[4]) {
          *state = 1;
          result =
            default_ret; /* orig MOV EDI,EBX @0x11565a: carries default_ret */
          goto case_1;
        }
        *state = 0xd;
        param_1[6] = (int)"invalid window size";
      } else {
        *state = 0xd;
        param_1[6] = (int)"unknown compression method";
      }
      goto set_mark;
    case 1:
    case_1:
      /* direct re-entry returns result (0xfffffffb=Z_BUF_ERROR); fall-through
       * from case 0 sets result=default_ret above, so it returns
       * default_ret (matches orig EDI). */
      if (param_1[1] == 0)
        return result;
      param_1[1] = param_1[1] - 1;
      param_1[2] = param_1[2] + 1;
      flags_byte = *(unsigned char *)*param_1;
      *param_1 = *param_1 + 1;
      if ((((int *)param_1[7])[1] * 0x100 + (unsigned int)flags_byte) % 0x1f == 0) {
        if (0 < *(int *)0x320e30) {
          crt_fprintf(*(void **)0x331070, "inflate: zlib header ok\n");
        }
        if ((flags_byte & 0x20) != 0) {
          *(int *)param_1[7] = 2;
          result = default_ret;
          goto case_2;
        }
        *(int *)param_1[7] = 7;
        result = default_ret;
      } else {
        *(int *)param_1[7] = 0xd;
        param_1[6] = (int)"incorrect header check";
        *(int *)(param_1[7] + 4) = 5;
        result = default_ret;
      }
      break;
    case 2:
    case_2:
      if (param_1[1] == 0)
        return result;
      param_1[2] = param_1[2] + 1;
      param_1[1] = param_1[1] - 1;
      *(unsigned int *)(param_1[7] + 8) =
        (unsigned int)*(unsigned char *)*param_1 << 0x18;
      *param_1 = *param_1 + 1;
      *(int *)param_1[7] = 3;
      result = default_ret;
    case 3:
      if (param_1[1] == 0)
        return result;
      param_1[1] = param_1[1] - 1;
      param_1[2] = param_1[2] + 1;
      *(unsigned int *)(param_1[7] + 8) =
        *(int *)(param_1[7] + 8) +
        (unsigned int)*(unsigned char *)*param_1 * 0x10000;
      *param_1 = *param_1 + 1;
      *(int *)param_1[7] = 4;
      result = default_ret;
    case 4:
      if (param_1[1] == 0)
        return result;
      param_1[1] = param_1[1] - 1;
      param_1[2] = param_1[2] + 1;
      *(unsigned int *)(param_1[7] + 8) =
        *(int *)(param_1[7] + 8) +
        (unsigned int)*(unsigned char *)*param_1 * 0x100;
      *param_1 = *param_1 + 1;
      *(int *)param_1[7] = 5;
      result = default_ret;
    case 5:
      if (param_1[1] == 0)
        return result;
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
      result = FUN_00113a90(*(int *)(param_1[7] + 0x14), param_1, result);
      if (result == 0xfffffffd) {
        *(int *)param_1[7] = 0xd;
        *(int *)(param_1[7] + 4) = 0;
        result = 0xfffffffd;
      } else {
        if (result == 0) {
          result = default_ret;
        }
        if (result != 1)
          return result;
        FUN_00113930(*(int *)(param_1[7] + 0x14), (int)param_1, param_1[7] + 4);
        state = (int *)param_1[7];
        if (state[3] == 0) {
          *state = 8;
          result = default_ret;
          goto case_8;
        }
        *state = 0xc;
        result = default_ret;
      }
      break;
    case 8:
    case_8:
      if (param_1[1] == 0)
        return result;
      param_1[1] = param_1[1] - 1;
      param_1[2] = param_1[2] + 1;
      *(unsigned int *)(param_1[7] + 8) =
        (unsigned int)*(unsigned char *)*param_1 << 0x18;
      *param_1 = *param_1 + 1;
      *(int *)param_1[7] = 9;
      result = default_ret;
    case 9:
      if (param_1[1] == 0)
        return result;
      param_1[1] = param_1[1] - 1;
      param_1[2] = param_1[2] + 1;
      *(unsigned int *)(param_1[7] + 8) =
        *(int *)(param_1[7] + 8) +
        (unsigned int)*(unsigned char *)*param_1 * 0x10000;
      *param_1 = *param_1 + 1;
      *(int *)param_1[7] = 10;
      result = default_ret;
    case 10:
      if (param_1[1] == 0)
        return result;
      param_1[1] = param_1[1] - 1;
      param_1[2] = param_1[2] + 1;
      *(unsigned int *)(param_1[7] + 8) =
        *(int *)(param_1[7] + 8) +
        (unsigned int)*(unsigned char *)*param_1 * 0x100;
      *param_1 = *param_1 + 1;
      *(int *)param_1[7] = 0xb;
      result = default_ret;
    case 0xb:
      if (param_1[1] == 0)
        return result;
      param_1[1] = param_1[1] - 1;
      param_1[2] = param_1[2] + 1;
      *(int *)(param_1[7] + 8) =
        *(int *)(param_1[7] + 8) + (unsigned int)*(unsigned char *)*param_1;
      *param_1 = *param_1 + 1;
      state = (int *)param_1[7];
      if (state[1] == state[2]) {
        if (0 < *(int *)0x320e30) {
          crt_fprintf(*(void **)0x331070, "inflate: zlib check ok\n");
        }
        *(int *)param_1[7] = 0xc;
        return 1;
      }
      *state = 0xd;
      param_1[6] = (int)"incorrect data check";
    set_mark:
      *(int *)(param_1[7] + 4) = 5;
      result = default_ret;
      break;
    case 0xc:
      return 1;
    case 0xd:
      return 0xfffffffd;
    default:
      return 0xfffffffe;
    }
    mode = *(int *)param_1[7];
  } while (1);
}

/* inflateSetDictionary: set the decompression dictionary after DICT check.
 * 0x115a00 / circular_queue.obj (inflate.c) */
int inflateSetDictionary(int z, int dictionary, unsigned int dictLength)
{
  int adler_check;
  unsigned int wsize;
  int *new_var;
  unsigned int n;
  new_var = (int *)0;
  n = dictLength;
  if (z != 0 && *(int **)(z + 0x1c) != new_var && **(int **)(z + 0x1c) == 6) {
    adler_check = FUN_00110a10(1, (unsigned char *)dictionary, dictLength);
    if (adler_check != *(int *)(z + 0x30))
      return (int)0xfffffffd;
    *(int *)(z + 0x30) = 1;
    wsize = 1 << *(int *)(*(int *)(z + 0x1c) + 0x10);
    if (dictLength >= wsize) {
      n = wsize - 1;
      dictionary = dictionary + (int)(dictLength - n);
    }
    inflate_set_dictionary(*(int *)(*(int *)(z + 0x1c) + 0x14), dictionary, (int)n);
    **(int **)(z + 0x1c) = 7;
    return 0;
  }
  return (int)0xfffffffe;
}

/* inflateSync: scan for a zlib sync point (0x00 0x00 0xff 0xff) in next_in.
 * 0x115a90 / circular_queue.obj (inflate.c) */
int inflateSync(int *z)
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
    } else if (qval != 0) {
      n = 0;
    } else {
      n = 4 - n;
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
  inflateReset((int)z);
  z[2] = saved_total_in;
  z[5] = saved_total_out;
  *(int *)z[7] = 7;
  return 0;
}

/* inflateSyncPoint: return 1 if inflate blocks are at a sync point.
 * 0x115b70 / circular_queue.obj (inflate.c) */
int inflateSyncPoint(int z)
{
  int blocks;
  int result;

  if (z != 0 && *(int *)(z + 0x1c) != 0) {
    blocks = *(int *)(*(int *)(z + 0x1c) + 0x14);
    if (blocks != 0) {
      result = FUN_001146c0((int *)blocks);
      return result;
    }
  }
  return (int)0xfffffffe;
}

/* zlib inflate_huft: one 8-byte Huffman decode table entry.  The original
 * writes the two byte members with `movb` into [EBP-0x2c] / [EBP-0x2b] and
 * copies whole entries with a two-dword structure assignment. */
typedef struct inflate_huft_s {
  unsigned char exop; /* number of extra bits or operation */
  unsigned char bits; /* number of bits in this code or subcode */
  unsigned short pad;
  unsigned int base; /* literal, length base, distance base, or table offset */
} inflate_huft;

#define BMAX 15    /* maximum bit length of any code */
#define MANY 0x5a0 /* number of huft entries in the caller's table space */

/* huft_build: build a Huffman decoding table.
 * 0x115ba0 / circular_queue.obj (inftrees.c)
 *
 *   b  = code lengths in bits (all assumed <= BMAX)
 *   n  = number of codes (assumed <= 288)
 *   s  = number of simple-valued codes (0..s-1)
 *   d  = list of base values for codes >= s
 *   e  = list of extra bits for codes >= s
 *   t  = result: starting table
 *   m  = maximum lookup bits, returns actual
 *   hp = space for the tables
 *   hn = hufts used in space
 *   v  = working area for huft_build
 *
 * `m` is the one argument MSVC passes in EAX (private register convention for
 * a file-static helper), hence the @<eax> annotation on the 7th parameter in
 * kb.json even though the stack arguments keep their original order.
 *
 * Returns 0 (Z_OK), -3 (Z_DATA_ERROR: over-subscribed set), -4 (Z_MEM_ERROR:
 * out of huft space) or -5 (Z_BUF_ERROR: incomplete set). */
int FUN_00115ba0(int *b, unsigned int n, unsigned int s, int *d, int *e,
                 int *t, unsigned int *m, int hp, unsigned int *hn,
                 unsigned int *v)
{
  unsigned int a;           /* counter for codes of length k */
  unsigned int c[BMAX + 1]; /* bit length count table */
  unsigned int f;           /* i repeats in table every f entries */
  int g;                    /* maximum code length */
  int h;                    /* table level */
  unsigned int i;           /* counter, current code */
  unsigned int j;           /* counter */
  int k;                    /* number of bits in current code */
  int l;                    /* bits per table (returned in m) */
  unsigned int mask;        /* (1 << w) - 1 */
  unsigned int *p;          /* pointer into c[], b[], or v[] */
  inflate_huft *q;          /* points to current table */
  inflate_huft r;           /* table entry for structure assignment */
  inflate_huft *u[BMAX];    /* table stack */
  int w;                    /* bits before this table == (l * h) */
  unsigned int x[BMAX + 1]; /* bit offsets, then code stack */
  unsigned int *xp;         /* pointer into x */
  int y;                    /* number of dummy codes added */
  unsigned int z;           /* number of entries in current table */

  /* Generate counts for each bit length */
  c[0] = 0;
  c[1] = 0;
  c[2] = 0;
  c[3] = 0;
  c[4] = 0;
  c[5] = 0;
  c[6] = 0;
  c[7] = 0;
  c[8] = 0;
  c[9] = 0;
  c[10] = 0;
  c[11] = 0;
  c[12] = 0;
  c[13] = 0;
  c[14] = 0;
  c[15] = 0;
  p = (unsigned int *)b;
  i = n;
  do {
    c[*p++]++; /* assume all entries <= BMAX */
  } while (--i != 0);
  if (c[0] == n) { /* null input--all zero length codes */
    *t = 0;
    *m = 0;
    return 0;
  }

  /* Find minimum and maximum length, bound *m by those */
  l = (int)*m;
  for (j = 1; j <= BMAX; j++)
    if (c[j] != 0)
      break;
  k = (int)j; /* minimum code length */
  if ((unsigned int)l < j)
    l = (int)j;
  for (i = BMAX; i != 0; i--)
    if (c[i] != 0)
      break;
  g = (int)i; /* maximum code length */
  if ((unsigned int)l > i)
    l = (int)i;
  *m = (unsigned int)l;

  /* Adjust last length count to fill out codes, if needed */
  for (y = 1 << j; j < i; j++, y <<= 1)
    if ((y -= (int)c[j]) < 0)
      return -3;
  if ((y -= (int)c[i]) < 0)
    return -3;
  c[i] += (unsigned int)y;

  /* Generate starting offsets into the value table for each length */
  x[1] = j = 0;
  p = c + 1;
  xp = x + 2;
  while (--i != 0) { /* note that i == g from above */
    *xp++ = (j += *p++);
  }

  /* Make a table of values in order of bit lengths */
  p = (unsigned int *)b;
  i = 0;
  do {
    j = *p++;
    if (j != 0)
      v[x[j]++] = i;
  } while (++i < n);
  n = x[g]; /* set n to length of v */

  /* Generate the Huffman codes and for each, make the table entries */
  x[0] = 0; /* first Huffman code is zero */
  i = 0;
  p = v;                  /* grab values in bit order */
  h = -1;                 /* no tables yet--level -1 */
  w = -l;                 /* bits decoded == (l * h) */
  u[0] = (inflate_huft *)0; /* just to keep compilers happy */
  q = (inflate_huft *)0;    /* ditto */
  z = 0;                    /* ditto */

  /* go through the bit lengths (k already is bits in shortest code) */
  for (; k <= g; k++) {
    a = c[k];
    while (a-- != 0) {
      /* here i is the Huffman code of length k bits for value *p */
      /* make tables up to required level */
      while (k > w + l) {
        h++;
        w += l; /* previous table always l bits */

        /* compute minimum size table less than or equal to l bits */
        z = (unsigned int)(g - w);
        z = z > (unsigned int)l ? (unsigned int)l : z; /* table size upper limit */
        j = (unsigned int)(k - w);
        f = 1u << j;
        if (f > a + 1) { /* try a k-w bit table */
          /* too few codes for k-w bit table */
          f -= a + 1; /* deduct codes from patterns left */
          xp = c + k;
          if (j < z)
            while (++j < z) { /* try smaller tables up to z bits */
              f <<= 1;
              if (f <= *++xp)
                break;  /* enough codes to use up j bits */
              f -= *xp; /* else deduct codes from patterns */
            }
        }
        z = 1u << j; /* table entries for j-bit table */

        /* allocate new table */
        if (*hn + z > MANY)
          return -4; /* overflow of MANY */
        q = (inflate_huft *)(hp + (int)*hn * 8);
        u[h] = q;
        *hn += z;

        /* connect to last table, if there is one */
        if (h != 0) {
          x[h] = i;                  /* save pattern for backing up */
          r.bits = (unsigned char)l; /* bits to dump before this table */
          r.exop = (unsigned char)j; /* bits in this table */
          j = i >> (w - l);
          r.base = (unsigned int)(q - u[h - 1] - j); /* offset to this table */
          u[h - 1][j] = r;                           /* connect to last table */
        } else {
          *t = (int)q; /* first table is returned result */
        }
      }

      /* set up table entry in r */
      r.bits = (unsigned char)(k - w);
      if (p >= v + n) {
        r.exop = 128 + 64; /* out of values--invalid code */
      } else if (*p < s) {
        /* 256 is end-of-block */
        r.exop = (unsigned char)(*p < 256 ? 0 : 32 + 64);
        r.base = *p++; /* simple code is just the value */
      } else {
        /* non-simple--look up in lists */
        r.exop = (unsigned char)(e[*p - s] + 16 + 64);
        r.base = (unsigned int)d[*p++ - s];
      }

      /* fill code-like entries with r */
      f = 1u << (k - w);
      for (j = i >> w; j < z; j += f)
        q[j] = r;

      /* backwards increment the k-bit code i */
      for (j = 1u << (k - 1); (i & j) != 0; j >>= 1)
        i ^= j;
      i ^= j;

      /* backup over finished tables */
      mask = (1u << w) - 1; /* needed on HP, cc -O bug */
      while ((i & mask) != x[h]) {
        h--; /* don't need to update q */
        w -= l;
        mask = (1u << w) - 1;
      }
    }
  }

  /* Return Z_BUF_ERROR if we were given an incomplete table */
  return (y != 0 && g != 1) ? -5 : 0;
}

#undef BMAX
#undef MANY

/* inflate_trees_bits: build decode table for bit-length codes.
 * 0x116010 / circular_queue.obj (inflate.c) */
int inflate_trees_bits(int *c, int *bb, int tl, int td, int z)
{
  int work;
  int result;
  unsigned int hn;

  hn = 0;
  work = (*(int (**)(int, int, int))(z + 0x20))(*(int *)(z + 0x28), 0x13, 4);
  if (work == 0)
    return -4;
  result = FUN_00115ba0(c, 0x13, 0x13, 0, 0, (int *)tl, (unsigned int *)bb, td,
                        &hn, (unsigned int *)work);
  if (result == -3) {
    *(const char **)(z + 0x18) = "oversubscribed dynamic bit lengths tree";
    (*(void (**)(int, int))(z + 0x24))(*(int *)(z + 0x28), work);
    return -3;
  }
  if (result == -5 || *bb == 0) {
    *(const char **)(z + 0x18) = "incomplete dynamic bit lengths tree";
    result = -3;
  }
  (*(void (**)(int, int))(z + 0x24))(*(int *)(z + 0x28), work);
  return result;
}

/* inflate_trees_dynamic: build decode tables for dynamic Huffman block.
 * 0x1160c0 / circular_queue.obj (inflate.c) */
int inflate_trees_dynamic(unsigned int param_1, int param_2, int param_3, int *param_4,
                 int *param_5, int param_6, int param_7, int param_8,
                 int param_9)
{
  int work;
  int result;
  unsigned int hn;

  hn = 0;
  work = (*(int (**)(int, int, int))(param_9 + 0x20))(*(int *)(param_9 + 0x28),
                                                       0x120, 4);
  if (work == 0)
    return -4;
  result = FUN_00115ba0((int *)param_3, param_1, 0x101, (int *)0x28d960,
                        (int *)0x28d9e0, (int *)param_6,
                        (unsigned int *)param_4, param_8, &hn,
                        (unsigned int *)work);
  if (result == 0) {
    if (*param_4 != 0) {
      result = FUN_00115ba0(
        (int *)(param_3 + (int)param_1 * 4), (unsigned int)param_2, 0,
        (int *)0x28da60, (int *)0x28dad8, (int *)param_7,
        (unsigned int *)param_5, param_8, &hn, (unsigned int *)work);
      if (result == 0) {
        if (*param_5 != 0 || param_1 < 0x102) {
          (*(void (**)(int, int))(param_9 + 0x24))(*(int *)(param_9 + 0x28),
                                                   work);
          return 0;
        }
      } else if (result == -3) {
        *(const char **)(param_9 + 0x18) = "oversubscribed distance tree";
        (*(void (**)(int, int))(param_9 + 0x24))(*(int *)(param_9 + 0x28),
                                                 work);
        return result;
      } else if (result == -5) {
        *(const char **)(param_9 + 0x18) = "incomplete distance tree";
        result = -3;
        (*(void (**)(int, int))(param_9 + 0x24))(*(int *)(param_9 + 0x28),
                                                 work);
        return result;
      } else if (result == -4) {
        goto free_and_return_inner;
      }
      *(const char **)(param_9 + 0x18) = "empty distance tree with lengths";
      result = -3;
    free_and_return_inner:
      (*(void (**)(int, int))(param_9 + 0x24))(*(int *)(param_9 + 0x28), work);
      return result;
    }
  } else if (result == -3) {
    *(const char **)(param_9 + 0x18) = "oversubscribed literal/length tree";
    (*(void (**)(int, int))(param_9 + 0x24))(*(int *)(param_9 + 0x28), work);
    return result;
  } else if (result == -4) {
    goto free_and_return_outer;
  }
  *(const char **)(param_9 + 0x18) = "incomplete literal/length tree";
  result = -3;
free_and_return_outer:
  (*(void (**)(int, int))(param_9 + 0x24))(*(int *)(param_9 + 0x28), work);
  return result;
}

/* inflate_trees_fixed: set pointers to fixed Huffman decode tables.
 * 0x116250 / circular_queue.obj (inflate.c) */
int inflate_trees_fixed(int *param_1, int *param_2, int **param_3, int **param_4)
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
  unsigned int read_ptr;
  unsigned int limit;
  unsigned int avail_out;
  unsigned int copy_n;
  unsigned int check;
  int out_ptr;
  int read_cur;
  unsigned int (*callback)(unsigned int, unsigned int, unsigned int);

  read_ptr = *(unsigned int *)(param_1 + 0x30);
  limit = *(unsigned int *)(param_1 + 0x34);
  out_ptr = *(int *)(param_2 + 0xc);
  if (limit < read_ptr) {
    limit = *(unsigned int *)(param_1 + 0x2c);
  }
  avail_out = *(unsigned int *)(param_2 + 0x10);
  copy_n = limit - read_ptr;
  if (copy_n > avail_out) {
    copy_n = avail_out;
  }
  if ((copy_n != 0) && (param_3 == -5)) {
    param_3 = 0;
  }
  *(unsigned int *)(param_2 + 0x10) = avail_out - copy_n;
  *(int *)(param_2 + 0x14) = *(int *)(param_2 + 0x14) + (int)copy_n;
  callback = *(unsigned int (**)(unsigned int, unsigned int, unsigned int))(
    param_1 + 0x38);
  if (callback !=
      (unsigned int (*)(unsigned int, unsigned int, unsigned int))0) {
    check = callback(*(unsigned int *)(param_1 + 0x3c), read_ptr, copy_n);
    *(unsigned int *)(param_1 + 0x3c) = check;
    *(unsigned int *)(param_2 + 0x30) = check;
  }
  csmemcpy((void *)out_ptr, (void *)read_ptr, copy_n);
  out_ptr = out_ptr + (int)copy_n;
  read_cur = (int)(read_ptr + copy_n);
  if (read_cur == *(int *)(param_1 + 0x2c)) {
    read_cur = *(int *)(param_1 + 0x28);
    if (*(int *)(param_1 + 0x34) == *(int *)(param_1 + 0x2c)) {
      *(int *)(param_1 + 0x34) = read_cur;
    }
    avail_out = *(unsigned int *)(param_2 + 0x10);
    copy_n = (unsigned int)(*(int *)(param_1 + 0x34) - read_cur);
    if (copy_n > avail_out) {
      copy_n = avail_out;
    }
    if ((copy_n != 0) && (param_3 == -5)) {
      param_3 = 0;
    }
    *(unsigned int *)(param_2 + 0x10) = avail_out - copy_n;
    *(int *)(param_2 + 0x14) = *(int *)(param_2 + 0x14) + (int)copy_n;
    callback = *(unsigned int (**)(unsigned int, unsigned int, unsigned int))(
      param_1 + 0x38);
    if (callback !=
        (unsigned int (*)(unsigned int, unsigned int, unsigned int))0) {
      check =
        callback(*(unsigned int *)(param_1 + 0x3c), (unsigned int)read_cur, copy_n);
      *(unsigned int *)(param_1 + 0x3c) = check;
      *(unsigned int *)(param_2 + 0x30) = check;
    }
    csmemcpy((void *)out_ptr, (void *)read_cur, copy_n);
    out_ptr = out_ptr + (int)copy_n;
    read_cur = read_cur + (int)copy_n;
  }
  *(int *)(param_2 + 0xc) = out_ptr;
  *(int *)(param_1 + 0x30) = read_cur;
  return param_3;
}

/* send_bits: output val (length bits) to deflate bit buffer.
 * 0x116390 / circular_queue.obj (deflate.c)
 * ABI: @eax=value, @ebx=length, @esi=deflate_state */
void FUN_00116390(int value, int length, int state)
{
  int bi_valid; /* slot also reused as the pending_buf index at state+0x14 */

  if (*(int *)0x320e30 > 1) {
    crt_fprintf(*(void **)0x331070, " l %2d v %4x ", length, value);
  }
  if (length < 1 || length > 0xf) {
    FUN_00117a80("invalid length");
  }
  bi_valid = *(int *)(state + 0x16bc);
  *(int *)(state + 0x16b4) = *(int *)(state + 0x16b4) + length;
  if (0x10 - length < bi_valid) {
    *(unsigned short *)(state + 0x16b8) =
      *(unsigned short *)(state + 0x16b8) | (unsigned short)(value << bi_valid);
    *(unsigned char *)(*(int *)(state + 8) + *(int *)(state + 0x14)) =
      *(unsigned char *)(state + 0x16b8);
    bi_valid = (*(int *)(state + 0x14) = *(int *)(state + 0x14) + 1);
    *(unsigned char *)(bi_valid + *(int *)(state + 8)) =
      *(unsigned char *)(state + 0x16b9);
    *(int *)(state + 0x14) = *(int *)(state + 0x14) + 1;
    bi_valid = *(int *)(state + 0x16bc);
    *(int *)(state + 0x16bc) = (bi_valid + -0x10) + length;
    *(unsigned short *)(state + 0x16b8) =
      (unsigned short)value >> ((unsigned int)(0x10 - (char)bi_valid) & 0x1f);
    return;
  }
  *(int *)(state + 0x16bc) = bi_valid + length;
  *(unsigned short *)(state + 0x16b8) =
    *(unsigned short *)(state + 0x16b8) | (unsigned short)(value << bi_valid);
}

/* init_block: zero per-block frequency counts and set EOB count to 1.
 * 0x116460 / circular_queue.obj (deflate.c)
 * ABI: @edx=state */
void FUN_00116460(int state)
{
  unsigned short *freq_ptr;
  int count;

  freq_ptr = (unsigned short *)(state + 0x8c);
  count = 0x11e;
  do {
    *freq_ptr = 0;
    freq_ptr += 2;
    count--;
  } while (count != 0);
  freq_ptr = (unsigned short *)(state + 0x980);
  count = 0x1e;
  do {
    *freq_ptr = 0;
    freq_ptr += 2;
    count--;
  } while (count != 0);
  freq_ptr = (unsigned short *)(state + 0xa74);
  count = 0x13;
  do {
    *freq_ptr = 0;
    freq_ptr += 2;
    count--;
  } while (count != 0);
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
  unsigned short freq_a;
  unsigned short freq_b;
  int node;
  int heap_len_cur;
  int tmp_i;
  int child;
  int j;
  int heap_len;

  heap_len = *(int *)(state + 0x1448);
  node = *(int *)(state + 0xb54 + param_1 * 4);
  j = param_1 * 2;
  tmp_i = j - heap_len;
  if (j > heap_len) {
    *(int *)(state + 0xb54 + param_1 * 4) = node;
    return;
  }
  while (1) {
    child = j;
    if (j < heap_len) {
      tmp_i = *(int *)(state + 0xb58 + j * 4);
      freq_a = *(unsigned short *)(tree + tmp_i * 4);
      freq_b =
        *(unsigned short *)(tree + *(int *)(state + 0xb54 + j * 4) * 4);
      if (freq_a < freq_b ||
          (freq_a == freq_b &&
           *(unsigned char *)(tmp_i + 0x1450 + state) <=
             *(unsigned char *)(*(int *)(state + 0xb54 + j * 4) + 0x1450 +
                                state))) {
        child = j + 1;
      }
    }
    tmp_i = *(int *)(state + 0xb54 + child * 4);
    freq_a = *(unsigned short *)(tree + node * 4);
    freq_b = *(unsigned short *)(tree + tmp_i * 4);
    if (freq_a < freq_b ||
        (freq_a == freq_b && *(unsigned char *)(node + 0x1450 + state) <=
                             *(unsigned char *)(tmp_i + 0x1450 + state)))
      break;
    *(int *)(state + 0xb54 + param_1 * 4) = tmp_i;
    heap_len_cur = *(int *)(state + 0x1448);
    j = child * 2;
    tmp_i = j - heap_len_cur;
    param_1 = child;
    if (tmp_i != 0 && heap_len_cur <= j) {
      *(int *)(state + 0xb54 + child * 4) = node;
      return;
    }
  }
  *(int *)(state + 0xb54 + param_1 * 4) = node;
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
  volatile unsigned int new_var;

  max_code = ((int *)desc)[1];
  tree = (int *)((int *)desc)[0];
  stree_ptr = (int *)((int *)desc)[2];
  stree = stree_ptr[0];
  extra = (int *)stree_ptr[1];
  extra_base = stree_ptr[2];
  max_length = (unsigned int)stree_ptr[4];

  csmemset((void *)(state + 0xb34), 0, 0x20);

  *(short *)((char *)tree +
             *(int *)(state + 0xb54 + *(int *)(state + 0x144c) * 4) * 4 + 2) =
    0;

  h = *(int *)(state + 0x144c) + 1;
  overflow = 0;
  if (h < 0x23d) {
    heap_ptr = (int *)(state + 0xb54 + h * 4);
    heap_count = 0x23d - h;
    h = h + heap_count;
    do {
      n = *heap_ptr;
      bits = (unsigned int)*(
               unsigned short *)((char *)tree +
                                 (unsigned int)*(
                                   unsigned short *)((char *)tree + n * 4 + 2) *
                                   4 +
                                 2) +
             1;
      if ((int)max_length < (int)bits) {
        overflow = overflow + 1;
        bits = max_length;
      }
      *(short *)((char *)tree + n * 4 + 2) = (short)bits;
      if (n <= max_code) {
        *(short *)(state + 0xb34 + bits * 2) =
          *(short *)(state + 0xb34 + bits * 2) + 1;
        xbits = 0;
        if (n >= extra_base) {
          xbits = extra[(n - extra_base)];
        }
        freq = (unsigned int)*(unsigned short *)((char *)tree + n * 4);
        *(int *)(state + 0x16a0) =
          *(int *)(state + 0x16a0) + (int)(bits + xbits) * (int)freq;
        if (stree != 0) {
          *(int *)(state + 0x16a4) =
            *(int *)(state + 0x16a4) +
            (int)((unsigned int)*(unsigned short *)(stree + n * 4 + 2) +
                  xbits) *
              (int)freq;
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
        *(short *)(state + 0xb34 + k * 2) =
          *(short *)(state + 0xb34 + k * 2) - 1;
        *(short *)(state + 0xb36 + k * 2) =
          *(short *)(state + 0xb36 + k * 2) + 2;
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
            new_var = max_length;
            if (n <= max_code) {
              freq =
                (unsigned int)*(unsigned short *)((char *)tree + n * 4 + 2);
              if (freq != max_length) {
                if (z_verbose >= 0) {
                  crt_fprintf(&z_stderr, "code %d bits %d->%d\n", n, freq,
                              max_length);
                }
                *(int *)(state + 0x16a0) =
                  *(int *)(state + 0x16a0) +
                  (int)(new_var -
                        *(unsigned short *)((char *)tree + n * 4 + 2)) *
                    (int)(unsigned int)*(unsigned short *)((char *)tree +
                                                           n * 4);
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
  short *freq_ptr;
  int first_len;
  int max_run;
  int min_run;
  int run_count;
  unsigned int curlen;
  unsigned int nextlen;
  int n_left;
  unsigned int prevlen;
  unsigned short *len_ptr;

  first_len = *(unsigned short *)(tree + 2);
  run_count = 0;
  prevlen = 0xffffffff;
  max_run = 7;
  min_run = 4;
  /* original keeps max_run (zlib max_count) = 7 here; the lift's `max_run = 0`
   * clobbered it, making the run-flush condition (run_count <= max_run) always
   * true and breaking run-length detection -> corrupted deflate output. Test
   * first code length directly (orig: MOV ECX,7 kept; sets 0x8a only when the
   * first length == 0). */
  if (first_len == 0) {
    max_run = 0x8a;
    min_run = 3;
  }
  *(unsigned short *)(tree + 6 + param_1 * 4) = 0xffff;
  if (param_1 >= 0) {
    len_ptr = (unsigned short *)(tree + 6);
    n_left = param_1 + 1;
    curlen = (unsigned int)first_len;
    do {
      nextlen = (unsigned int)*len_ptr;
      run_count++;
      if (max_run <= run_count || curlen != nextlen) {
        if (run_count < min_run) {
          freq_ptr = (short *)(param_2 + 0xa74 + curlen * 4);
          *freq_ptr += (short)run_count;
        } else if (curlen != 0) {
          if (curlen != prevlen) {
            *(short *)(param_2 + 0xa74 + curlen * 4) += 1;
          }
          *(short *)(param_2 + 0xab4) += 1;
        } else {
          if (run_count < 0xb)
            *(short *)(param_2 + 0xab8) += 1;
          else
            *(short *)(param_2 + 0xabc) += 1;
        }
        run_count = 0;
        prevlen = curlen;
        if (nextlen == 0) {
          max_run = 0x8a;
          min_run = 3;
        } else if (curlen == nextlen) {
          max_run = 6;
          min_run = 3;
        } else {
          max_run = 7;
          min_run = 4;
        }
      }
      len_ptr += 2;
      n_left--;
      curlen = nextlen;
    } while (n_left != 0);
  }
}

/* send_tree: send a literal or distance tree with run-length encoding
 * (0x1168e0). Emits REP(16), REPZ_3_10(17), REPZ_11_138(18) codes for runs.
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
                           *(unsigned short *)(state + 0xaba), state);
              FUN_00116390(count - 3, 3, state);
            } else {
              if (z_verbose > 2) {
                crt_fprintf(&z_stderr, "\ncd %3d ", 0x12);
              }
              FUN_00116390(*(unsigned short *)(state + 0xabc),
                           *(unsigned short *)(state + 0xabe), state);
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
                         *(unsigned short *)(state + 0xab6), state);
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

/* send_all_trees: send literal, distance, and bit-length tree headers
 * (0x116b00). ABI: @eax=state, cdecl param_1=lcodes, param_2=dcodes,
 * param_3=blcodes */
void FUN_00116b00(int state, int param_1, int param_2, int param_3)
{
  unsigned int bl_len;
  int i;
  int bi_valid; /* slot also reused as the pending_buf index at state+0x14 */

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
        crt_fprintf(&z_stderr, "\nbl code %2d ",
                    (unsigned int)zlib_bl_order[i]);
      }
      bl_len =
        *(unsigned short *)(state + 0xa76 + (unsigned int)zlib_bl_order[i] * 4);
      if (z_verbose > 1) {
        crt_fprintf(&z_stderr, " l %2d v %4x ", 3, (unsigned int)bl_len);
      }
      *(int *)(state + 0x16b4) = *(int *)(state + 0x16b4) + 3;
      bi_valid = *(int *)(state + 0x16bc);
      if (bi_valid > 0xd) {
        *(unsigned short *)(state + 0x16b8) =
          *(unsigned short *)(state + 0x16b8) |
          (unsigned short)(bl_len << bi_valid);
        *(unsigned char *)(*(int *)(state + 8) + *(int *)(state + 0x14)) =
          *(unsigned char *)(state + 0x16b8);
        bi_valid = *(int *)(state + 0x14) + 1;
        *(int *)(state + 0x14) = bi_valid;
        *(unsigned char *)(bi_valid + *(int *)(state + 8)) =
          *(unsigned char *)(state + 0x16b9);
        *(int *)(state + 0x14) = *(int *)(state + 0x14) + 1;
        bi_valid = *(int *)(state + 0x16bc);
        *(int *)(state + 0x16bc) = bi_valid - 0xd;
        *(unsigned short *)(state + 0x16b8) =
          (unsigned short)(bl_len >> (0x10 - bi_valid));
      } else {
        *(unsigned short *)(state + 0x16b8) =
          *(unsigned short *)(state + 0x16b8) |
          (unsigned short)(bl_len << bi_valid);
        *(int *)(state + 0x16bc) = bi_valid + 3;
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
int _tr_tally(int param_1, int param_2, int param_3)
{
  short *freq_ptr;
  unsigned int code;

  *(short *)(*(int *)(param_1 + 0x169c) + *(int *)(param_1 + 0x1698) * 2) =
    (short)param_2;
  *(char *)(*(int *)(param_1 + 0x1690) + *(int *)(param_1 + 0x1698)) =
    (char)param_3;
  *(int *)(param_1 + 0x1698) = *(int *)(param_1 + 0x1698) + 1;
  if (param_2 == 0) {
    freq_ptr = (short *)(param_1 + 0x8c + param_3 * 4);
    *freq_ptr += 1;
  } else {
    *(int *)(param_1 + 0x16a8) += 1;
    param_2--;
    if ((unsigned short)param_2 >=
        (unsigned short)(*(short *)(param_1 + 0x24) - 0x106))
      goto bad_match;
    if ((unsigned short)param_3 > 0xff)
      goto bad_match;
    if ((unsigned int)param_2 < 0x100) {
      code = *(unsigned char *)(0x28e288 + (unsigned int)param_2);
    } else {
      code = *(unsigned char *)(0x28e388 + ((unsigned int)param_2 >> 7));
    }
    if ((unsigned short)code < 0x1e)
      goto after_assert;
  bad_match:
    FUN_00117a80("_tr_tally: bad match");
  after_assert:
    freq_ptr = (short *)(param_1 + 0x490 +
                       (unsigned int)(unsigned char)(*(
                         unsigned char *)(0x28e488 + (unsigned int)param_3)) *
                         4);
    *freq_ptr += 1;
    if ((unsigned int)param_2 < 0x100) {
      code = *(unsigned char *)(0x28e288 + (unsigned int)param_2);
    } else {
      code = *(unsigned char *)(0x28e388 + ((unsigned int)param_2 >> 7));
    }
    *(short *)(param_1 + 0x980 + code * 4) += 1;
  }
  return *(int *)(param_1 + 0x1698) == *(int *)(param_1 + 0x1694) - 1;
}

/* compress_block: send the block data compressed using given Huffman trees
 * (0x116e00). ABI: @eax=state, cdecl param_1=ltree, param_2=dtree */
void FUN_00116e00(int state, int param_1, int param_2)
{
  unsigned int dist;
  unsigned int lc;
  unsigned int code;
  volatile long extra;
  unsigned int idx;

  idx = 0;
  if (*(unsigned int *)(state + 0x1698) != 0) {
    do {
      dist =
        (unsigned int)*(unsigned short *)(*(int *)(state + 0x169c) + idx * 2);
      lc = (unsigned int)*(unsigned char *)(idx + *(int *)(state + 0x1690));
      idx = idx + 1;
      if (dist == 0) {
        if (z_verbose > 2) {
          crt_fprintf(&z_stderr, "\ncd %3d ", lc);
        }
        FUN_00116390(*(unsigned short *)(param_1 + lc * 4),
                     *(unsigned short *)(param_1 + lc * 4 + 2), state);
        if (z_verbose > 1 && crt_isgraph(lc) != 0) {
          crt_fprintf(&z_stderr, " \'%c\' ", lc);
        }
      } else {
        code = (unsigned int)zlib_length_code[lc];
        if (z_verbose > 2) {
          crt_fprintf(&z_stderr, "\ncd %3d ", code + 0x101);
        }
        FUN_00116390(*(unsigned short *)(param_1 + code * 4 + 0x404),
                     *(unsigned short *)(param_1 + code * 4 + 0x406), state);
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
                     *(unsigned short *)(param_2 + code * 4 + 2), state);
        extra = (unsigned int)zlib_extra_dbits[code];
        if (extra != 0) {
          FUN_00116390(dist - zlib_base_dist[code], extra, state);
        }
      }
      if (*(unsigned int *)(state + 0x14) >=
          *(unsigned int *)(state + 0x1694) + idx * 2) {
        FUN_00117a80("pendingBuf overflow");
      }
    } while (idx < *(unsigned int *)(state + 0x1698));
  }
  if (z_verbose > 2) {
    crt_fprintf(&z_stderr, "\ncd %3d ", 0x100);
  }
  FUN_00116390(*(unsigned short *)(param_1 + 0x400),
               *(unsigned short *)(param_1 + 0x402), state);
  *(int *)(state + 0x16ac) =
    (int)(unsigned int)*(unsigned short *)(param_1 + 0x402);
}

/* set_data_type: set data_type field based on literal frequency counts.
 * 0x117000 / circular_queue.obj (deflate.c)
 * ABI: @ecx=deflate_state */
void FUN_00117000(int state)
{
  unsigned int bin_freq;
  unsigned int ascii_freq;
  unsigned short *freq_ptr;
  int count;

  ascii_freq = 0;
  bin_freq = (unsigned int)*(unsigned short *)(state + 0xa4) +
          (unsigned int)*(unsigned short *)(state + 0xa0) +
          (unsigned int)*(unsigned short *)(state + 0x9c) +
          (unsigned int)*(unsigned short *)(state + 0x98) +
          (unsigned int)*(unsigned short *)(state + 0x94) +
          (unsigned int)*(unsigned short *)(state + 0x90) +
          (unsigned int)*(unsigned short *)(state + 0x8c);
  freq_ptr = (unsigned short *)(state + 0xa8);
  count = 0x79;
  do {
    ascii_freq += *freq_ptr;
    freq_ptr += 2;
    count--;
  } while (count != 0);
  freq_ptr = (unsigned short *)(state + 0x28c);
  count = 0x80;
  do {
    bin_freq += *freq_ptr;
    freq_ptr += 2;
    count--;
  } while (count != 0);
  *(char *)(state + 0x1c) = (char)(ascii_freq >> 2 >= bin_freq);
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
  int s_ptr;
  int pending;
  unsigned char len_hi;

  s_ptr = FUN_00117130(state);
  *(unsigned int *)(s_ptr + 0x16ac) = 8;
  if (header != 0) {
    *(unsigned char *)(*(int *)(s_ptr + 0x14) + *(int *)(s_ptr + 8)) =
      (unsigned char)len;
    pending = *(int *)(s_ptr + 0x14) + 1;
    *(int *)(s_ptr + 0x14) = pending;
    len_hi = (unsigned char)((unsigned int)len >> 8);
    *(unsigned char *)(pending + *(int *)(s_ptr + 8)) = len_hi;
    pending = *(int *)(s_ptr + 0x14) + 1;
    *(int *)(s_ptr + 0x14) = pending;
    *(unsigned char *)(pending + *(int *)(s_ptr + 8)) = ~(unsigned char)len;
    pending = *(int *)(s_ptr + 0x14) + 1;
    *(int *)(s_ptr + 0x14) = pending;
    *(unsigned char *)(pending + *(int *)(s_ptr + 8)) = ~len_hi;
    *(int *)(s_ptr + 0x14) = *(int *)(s_ptr + 0x14) + 1;
    *(int *)(s_ptr + 0x16b4) += 0x20;
  }
  *(int *)(s_ptr + 0x16b4) += len * 8;
  while (len > 0) {
    *(unsigned char *)(*(int *)(s_ptr + 0x14) + *(int *)(s_ptr + 8)) = *buf++;
    *(int *)(s_ptr + 0x14) += 1;
    len--;
  }
}

/* deflate state init: initialize tree, block, and bit-buffer fields.
 * 0x117250 / circular_queue.obj (deflate.c) */
void _tr_init(int param_1)
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
  unsigned int res_next;
  int i;
  int is_graph;
  int ch;
  unsigned int res;
  unsigned int code_val;
  unsigned int bits_left;
  unsigned int bit;
  unsigned int len;
  unsigned short auStack_28[16];
  unsigned short code;

  code = 0;
  i = 1;
  do {
    code =
      (unsigned short)((*(short *)((int)bl_count + i * 2 - 2) + code) * 2);
    auStack_28[i] = code;
    i++;
  } while (i < 0x10);
  if (((unsigned int)(*(unsigned short *)((int)bl_count + 0x1e) - 1) +
       (unsigned int)code) != 0x7fff) {
    FUN_00117a80("inconsistent bit counts");
  }
  if (*(int *)0x320e30 > 0) {
    crt_fprintf(*(void **)0x331070, "\ngen_codes: max_code %d ", param_2);
  }
  i = 0;
  if (param_2 >= 0) {
    do {
      len = (unsigned int)*(unsigned short *)((int)param_1 + i * 4 + 2);
      if (len != 0) {
        code_val = (unsigned int)auStack_28[len];
        auStack_28[len] =
          (unsigned short)((unsigned int)auStack_28[len] + 1);
        res_next = 0;
        bits_left = len;
        do {
          res = res_next;
          bit = code_val & 1;
          code_val >>= 1;
          bits_left--;
          res_next = (res | bit) << 1;
        } while ((int)bits_left > 0);
        *(unsigned short *)((int)param_1 + i * 4) =
          (unsigned short)res | (unsigned short)bit;
        if (*(int *)0x320e30 > 1 && param_1 != (int *)0x28dd90) {
          is_graph = uisgraph(i);
          ch = i;
          if (is_graph == 0)
            ch = 0x20;
          crt_fprintf(
            *(void **)0x331070, "\nn %3d %c l %2d c %4x (%x) ", i, ch,
            len, (unsigned int)*(unsigned short *)((int)param_1 + i * 4),
            ((unsigned int)auStack_28[len] & 0xffff) - 1);
        }
      }
      i++;
    } while (i <= param_2);
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
      *(int *)(state + 0x16a4) =
        *(int *)(state + 0x16a4) -
        (int)(unsigned int)*(unsigned short *)(stree + n * 4 + 2);
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

    *(short *)((char *)tree + node * 4) =
      *(short *)((char *)tree + n * 4) + *(short *)((char *)tree + m * 4);

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

/* build_bl_tree: build the bit-length tree and return max bl_order index
 * (0x117600). ABI: @esi=state (preserved by caller). Returns max index in
 * EDI→EAX. */
int FUN_00117600(int state)
{
  int max_blindex;
  int opt_len;

  FUN_001167f0(*(int *)(state + 0xb14), state, state + 0x8c);
  FUN_001167f0(*(int *)(state + 0xb20), state, state + 0x980);
  FUN_001173f0(state, (int *)(state + 0xb28));

  max_blindex = 0x12;
  do {
    if (*(short *)(state + 0xa76 +
                   (unsigned int)zlib_bl_order[max_blindex] * 4) != 0)
      break;
    max_blindex = max_blindex - 1;
  } while (max_blindex >= 3);

  opt_len = *(int *)(state + 0x16a0) + (max_blindex + 1) * 3 + 14;
  *(int *)(state + 0x16a0) = opt_len;
  if (z_verbose > 0) {
    crt_fprintf(&z_stderr, "\ndyn trees: dyn %ld, stat %ld", opt_len,
                *(int *)(state + 0x16a4));
  }
  return max_blindex;
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

/* Align the output stream and emit STATIC_TREES end-of-block (0x1176f0).
 * If the last match distance is too small, repeat alignment. */
void _tr_align(int param_1)
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

/* _tr_flush_block: decide how to flush the current block and emit it
 * (0x1177c0). Chooses between stored, static Huffman, or dynamic Huffman based
 * on sizes. */
void _tr_flush_block(int param_1, int param_2, int param_3, int param_4)
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
      crt_fprintf(&z_stderr, "\nopt %lu(%lu) stat %lu(%lu) stored %lu lit %u ",
                  opt_len, *(int *)(param_1 + 0x16a0), static_len,
                  *(int *)(param_1 + 0x16a4), param_3,
                  *(int *)(param_1 + 0x1698));
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
int uncompress(int p1, unsigned int *p2, unsigned int *p3, unsigned int p4)
{
  int err;
  int z[14]; /* z_stream, 0x38 bytes */

  z[1] = (int)p4;
  z[0] = (int)p3;
  z[4] = (int)*p2;
  z[3] = p1;
  z[8] = 0;
  z[9] = 0;
  err = inflateInit_((int)z, "1.1.3", 0x38);
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
const char *zError(int errcode)
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
  unsigned int old_end;
  unsigned int new_end;

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
      old_end = (int)element_size * (unsigned int)*count + elements;
      new_end = (int)element_size * (int)new_count + elements;
      if (old_end < new_end) {
        csmemset((void *)old_end, 0, new_end - old_end);
        *count = (unsigned char)new_count;
        return 1;
      }
      csmemset((void *)new_end, 0xffffffff, old_end - new_end);
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
  unsigned char index;

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
  index = *count;
  if ((short)(unsigned short)index < maximum_count) {
    new_var = (int)((short)(unsigned short)index);
    *count = index + 1;
    csmemset((void *)(new_var * (int)element_size + elements), 0,
             (int)element_size);
    return (unsigned short)index;
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
  int elem_addr;
  int elem_size;
  int idx;
  unsigned char new_count;

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
  new_count = *count - 1;
  *count = new_count;
  idx = (int)index;
  if (idx < (int)(unsigned int)new_count) {
    elem_size = (int)element_size;
    elem_addr = elem_size * idx + elements;
    csmemmove((void *)elem_addr, (const void *)(elem_size + elem_addr),
              ((unsigned int)new_count - idx) * elem_size);
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
    display_assert("memory", "c:\\halo\\SOURCE\\memory\\byte_swapping_codes.c",
                   0x71, 1);
    system_exit(-1);
  }
  if (count < 0) {
    display_assert("count>=0",
                   "c:\\halo\\SOURCE\\memory\\byte_swapping_codes.c", 0x72, 1);
    system_exit(-1);
  }
  if (element_size != -2 && element_size != -4 &&
      (new_var = element_size) != -8) {
    display_assert("code==_2byte || code==_4byte || code==_8byte",
                   "c:\\halo\\SOURCE\\memory\\byte_swapping_codes.c", 0x73, 1);
    system_exit(-1);
  }

  switch (element_size) {
  case -8:
    p8 = (unsigned long long *)data;
    if (count > 0) {
      do {
        v8 = *p8;
        *p8 =
          ((*p8 >> 56) | ((*p8 >> 40) & 0xff00ULL) |
           ((*p8 >> 24) & 0xff0000ULL) | ((*p8 >> 8) & 0xff000000ULL) |
           ((v8 << 8) & 0xff00000000ULL) | ((*p8 << 24) & 0xff0000000000ULL) |
           ((*p8 << 40) & 0xff000000000000ULL) | (*p8 << 56));
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
        *p4 =
          (v >> 24) | ((v >> 8) & 0xff00) | ((v << 8) & 0xff0000) | (v << 24);
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

/* Byte-swap interpreter: walk a byte-swap code array and swap fields
 * (0x1187f0). Handles 2/4/8-byte swaps, nested struct references, and array
 * repeats. out_size receives the total data offset processed, out_step receives
 * the number of code words consumed. */
void FUN_001187f0(void *bs_definition, int data_ptr, int *codes, int *out_size,
                  int *out_step)
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
  unsigned long long *p8;
  unsigned long long v8;
  char *new_var;

  if (def[3] != 0x62797377) {
    msg = csprintf((char *)0x5ab100,
                   "got bs data with bad signature (assuming name is wrong)",
                   "c:\\halo\\SOURCE\\memory\\byte_swapping.c", 0xb0, 0);
    display_assert(msg, 0, 0, 0);
    if (def[3] != 0x62797377) {
      msg = csprintf((char *)0x5ab100, "%s bs data has bad signature",
                     *(char **)def, "c:\\halo\\SOURCE\\memory\\byte_swapping.c",
                     0xb2, 1);
      display_assert(msg, 0, 0, 0);
      system_exit(-1);
    }
  }

  if (codes[0] != -100) {
    msg = csprintf((char *)0x5ab100, "%s bs data @%p.#0 has bad start #%d",
                   *(char **)def, codes, *(int *)def[2],
                   "c:\\halo\\SOURCE\\memory\\byte_swapping.c", 0xb7, 1);
    display_assert(msg, 0, 0, 0);
    system_exit(-1);
  }

  array_count = codes[1];
  if (array_count < 0) {
    msg =
      csprintf((char *)0x5ab100, "%s bs data @%p.#1 has invalid array size #%d",
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
          p8 = (unsigned long long *)(offset + data_ptr);
          v8 = *p8;
          *p8 =
            ((*p8 >> 56) | ((*p8 >> 40) & 0xff00ULL) |
             ((*p8 >> 24) & 0xff0000ULL) | ((*p8 >> 8) & 0xff000000ULL) |
             ((v8 << 8) & 0xff00000000ULL) | ((*p8 << 24) & 0xff0000000000ULL) |
             ((*p8 << 40) & 0xff000000000000ULL) | (*p8 << 56));
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
          new_var = (char *)0x5ab100;
          msg = csprintf(new_var, "%s bs @%p.#%d has invalid code #%d",
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
  int *queue;

  queue = (int *)debug_malloc(
    param_2 + 0x19, 0, "c:\\halo\\SOURCE\\memory\\circular_queue.c", 0x34);
  if (queue != (int *)0) {
    csmemset(queue, 0, 0x18);
    *queue = param_1;
    queue[1] = 0x63697263;
    queue[4] = param_2 + 1;
    queue[5] = (int)(queue + 6);
    FUN_00118d70((int)queue);
  }
  return queue;
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
