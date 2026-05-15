/* interface/marketing_and_strategic_business_development.c
 *
 * "Marketing and strategic business development" is the original Bungie
 * jokey-named TU for the XDemos hand-off path on the Xbox retail kiosk:
 * detect whether the on-disc "d:\XDemos\XDemos.xbe" demo image is present,
 * clean up engine-owned device/cache state, and then call
 * XAPILIB::XLaunchNewImageA to chain to the demo XBE.
 *
 * Source path string in original binary:
 *   c:\halo\SOURCE\interface\marketing_and_strategic_business_development.c
 */

/* FUN_000e02d0 — sprite renderer helper (no callers in the shipped XBE,
 * almost certainly dead/inlined utility code preserved by MSVC).
 *
 * Builds four screen-space sprite vertices around a 2D rectangle described
 * by param_1 (a small UI element struct containing two int16_t pairs at
 * offsets 0x04 and 0x10) using a rotation about a centre offset stored at
 * *param_2 (int16_t pair), with per-corner clipping selected from param_3
 * (a float[4] of clip extents), a uniform scale param_4, rotation angle
 * param_5 (radians), depth param_6, and a final 16-bit tag param_7.
 *
 * The corners are emitted into a stack-resident sprite-vertex array
 * (5 floats per vertex: x, y, z, u, v), then a 0x8c-byte rasterizer
 * descriptor is zero-initialised and a 2x2 transform set to identity
 * (the four 0x3F800000 stores) before rasterizer_sprites_render() is
 * dispatched with that descriptor and the vertex array.
 *
 * Faithful structural lift of the original control flow.  All FPU
 * operand orders match the disassembly.  param_3 == NULL substitutes a
 * stack-local zero-initialised four-float clip extent. */
void FUN_000e02d0(int param_1, short *param_2, float *param_3, float param_4,
                  float param_5, float param_6, short param_7)
{
  /* Stack frame mirrors MSVC layout (subl $0xf0).
   *  rast_desc at [ebp-0xa0]  0x8c bytes
   *  vertices  at [ebp-0xf0]  4 prefix bytes + 19 floats = 0x4c=76 bytes,
   *                           but the original allocates 0xa0-0x14=0x8c
   *                           bytes for vertices+gap.
   *  local_clip at [ebp-0x14] 4 floats */
  unsigned char rast_desc[0x8c];
  float vertices[20]; /* 0x50 bytes; original uses 19 */
  unsigned char unused_gap[0x10]; /* pad to match 0xf0 frame */
  float local_clip[4];
  float fVar1;
  float fVar2;
  short sVar3;
  short sVar4;
  short sVar5;
  float fVar6;
  float fVar7;
  float *pfVar8;
  short sVar9;
  unsigned int uVar10;
  /* fsin/fcos results live in the param_4/param_5 slots, kept as
   * aliases to keep VC71 emitting `fmuls 0x14(%ebp)` / `fmuls 0x18(%ebp)`. */
#define fsin_v param_4
#define fcos_v param_5

  (void)unused_gap;
  (void)vertices;

  local_clip[0] = 0.0f;
  local_clip[1] = (float)(int)*(short *)(param_1 + 4);
  local_clip[2] = 0.0f;
  local_clip[3] = (float)(int)*(short *)(param_1 + 6);
  /* The original emits inline FSIN/FCOS on param_5 and stores the
   * results back into the param_4 / param_5 stack slots. */
#ifdef XDK_BUILD
  { float _angle = param_5;
    /* clang-format off */
    __asm fld _angle
    __asm fsin
    __asm fstp param_4
    __asm fld _angle
    __asm fcos
    __asm fstp param_5
    /* clang-format on */
  }
#else
  { float _angle = param_5;
    __asm__ volatile("fsin" : "=t"(param_4) : "0"(_angle));
    __asm__ volatile("fcos" : "=t"(param_5) : "0"(_angle));
  }
#endif
  if (param_3 == (float *)0) {
    param_3 = local_clip;
  }
  sVar3 = *(short *)(param_1 + 0x10);
  sVar4 = *(short *)(param_1 + 0x12);
  sVar9 = 0;
  sVar5 = param_2[1];
  uVar10 = 1;
  /* origin_x lives in -0x4(%ebp) per original */
  {
    float origin_x;
    origin_x = (float)(int)*param_2;
    pfVar8 = vertices + 1; /* original walks pfVar8 with pfVar8[-1] etc. */
    do {
      if ((uVar10 & 2) == 0) {
        fVar1 = *param_3;
      } else {
        fVar1 = param_3[1];
      }
      if (sVar9 < 2) {
        fVar2 = param_3[2];
      } else {
        fVar2 = param_3[3];
      }
      sVar9 = sVar9 + 1;
      pfVar8[3] = param_6;
      uVar10 = uVar10 + 1;
      fVar6 = (fVar1 - (float)(int)sVar3) * param_4;
      fVar7 = (fVar2 - (float)(int)sVar4) * param_4;
      pfVar8[-1] = (fVar6 * fcos_v + origin_x) - fVar7 * fsin_v;
      *pfVar8 = fVar6 * fsin_v + fVar7 * fcos_v + (float)(int)sVar5;
      pfVar8[1] = fVar1;
      pfVar8[2] = fVar2;
      pfVar8 = pfVar8 + 5;
    } while (sVar9 < 4);
  }

  csmemset(rast_desc, 0, 0x8c);
  /* Four 1.0f scale factors at fixed offsets in rast_desc */
  *(unsigned int *)(rast_desc + 0x28) = 0x3f800000; /* -0x78 ebp */
  *(unsigned int *)(rast_desc + 0x2c) = 0x3f800000; /* -0x74 ebp */
  *(unsigned int *)(rast_desc + 0x40) = 0x3f800000; /* -0x60 ebp */
  *(unsigned int *)(rast_desc + 0x44) = 0x3f800000; /* -0x5c ebp */
  /* Zero a few fields and pack param_1 / param_7 / 0 into rast_desc */
  *(unsigned int *)(rast_desc + 0) = 0;
  *(unsigned char *)(rast_desc + 0x88) = 0; /* -0x18+2 = -0x16 */
  *(unsigned short *)(rast_desc + 0x86) =
    (unsigned short)param_7; /* -0x18 ebp */
  *(int *)(rast_desc + 0x0c) = param_1; /* local_98 = -0x94 ebp */
  rasterizer_sprites_render(rast_desc, local_clip);
}

/* xbox_demos_available — return cached XDemos availability flag.
 *
 * On the first call after the dirty bit at 0x30f028 is set, build a
 * file_ref for "d:\XDemos\XDemos.xbe", test for its existence, and
 * cache the result (1 if present) at 0x46beac.  Returns the cached byte.
 *
 * Original ABI returns a single byte (AL).  Return type modelled as
 * bool/unsigned char. */
unsigned char xbox_demos_available(void)
{
  file_ref_t file_ref;
  int created;
  char exists;

  if (*(volatile unsigned char *)0x30f028 == 1) {
    created = (int)file_reference_create_from_path(&file_ref,
                                                   "d:\\XDemos\\XDemos.xbe", 0);
    if (created != 0) {
      exists = (char)file_exists(&file_ref);
      if (exists != 0) {
        *(volatile unsigned char *)0x46beac = 1;
      }
    }
    *(volatile unsigned char *)0x30f028 = 0;
  }
  return *(volatile unsigned char *)0x46beac;
}

/* clean_up_for_image_launch — prepare the engine for XLaunchNewImage().
 *
 * 1. Log a diagnostic via error(2, ...).
 * 2. Call FUN_001c2af0 (saved-game files / sound shutdown helper).
 * 3. If a map precache is in progress, log and abort it.
 * 4. Call D3DDevice_PersistDisplay() (D3D8 import at 0x1e9190).  On
 *    failure assert and tail-call halt_and_catch_fire via system_exit(-1).
 *
 * Asserted file/line strings match the original __FILE__ and 0x40 line.
 * Forced out-of-line so VC71 cannot inline into FUN_000e0570 (the
 * original binary keeps it as a separate call). */
#ifdef _MSC_VER
__declspec(noinline)
#else
__attribute__((noinline))
#endif
void clean_up_for_image_launch(void)
{
  int hr;

  error(2, "preparing for XLaunchNewImage()...");
  saved_game_files_take_mutex();
  if (cache_files_precache_in_progress() != 0) {
    error(2, "stopping map file precaching...");
    cache_files_precache_map_end();
  }
  hr = D3DDevice_PersistDisplay();
  if (hr < 0) {
    display_assert(
      "IDirect3DDevice8_PersistDisplay() failed in clean_up_for_image_launch()",
      "c:\\halo\\SOURCE\\interface\\marketing_and_strategic_business_"
      "development.c",
      0x40, 1);
    system_exit(-1);
  }
}

void xbox_demos_launch(void)
{
  char launch_data[0xc00];

  if (xbox_demos_available()) {
    csmemset(launch_data, 0, sizeof(launch_data));
    clean_up_for_image_launch();
    XLaunchNewImageA("d:\\XDemos\\XDemos.xbe", launch_data);
    XLaunchNewImageA(0, launch_data);
    while (1) {
    }
  }

  error(2, "XDEMOS not found!");
}

/* FUN_000e0570 — game-over / image-launch dispatcher.
 *
 * Builds a 0xc00-byte launch parameter block (one byte + 0xbff bytes
 * zeroed via REP STOSD/STOSW/STOSB), runs the standard image-launch
 * cleanup, then dispatches XLaunchNewImageA based on the campaign
 * completion state at 0x31e054 (a signed 16-bit value):
 *
 *   level <  0x21       -> launch parent (NULL) with default block
 *   level in [0x21,0x22] -> set block[0] = 2, launch parent (NULL)
 *   level == 0x23        -> set block[0] = 0, launch parent (NULL)
 *
 * After the launch call the function spins forever (the XBE will be
 * replaced by XLaunchNewImageA, so control should never return). */
void FUN_000e0570(void)
{
  unsigned char launch_data[0xc00];
  int level;

  /* The original zero-fills the launch block with the
   * MOV byte[0],0 / XOR / MOV ECX / LEA EDI / REP STOSD / STOSW / STOSB
   * pattern.  Use inline asm so both clang (XBE build) and VC71
   * (verify) emit byte-identical code.  No MSVC-style asm clobber
   * issues here: we only touch EAX/ECX/EDI which are listed in the
   * clobber/output set. */
#ifdef XDK_BUILD
  __asm {
    mov     byte ptr launch_data[0], 0
    xor     eax, eax
    mov     ecx, 0x2ff
    lea     edi, [launch_data+1]
    rep stosd
    stosw
    stosb
  }
#else
  {
    void *_dst = launch_data;
    __asm__ volatile("movb $0, (%0)\n\t"
                     "xorl %%eax, %%eax\n\t"
                     "movl $0x2ff, %%ecx\n\t"
                     "leal 1(%0), %%edi\n\t"
                     "rep stosl\n\t"
                     "stosw\n\t"
                     "stosb"
                     :
                     : "r"(_dst)
                     : "eax", "ecx", "edi", "memory");
  }
#endif

  clean_up_for_image_launch();

  level = (int)*(volatile short *)0x31e054;
  if (level < 0x21)
    goto launch_default;
  if (level > 0x22) {
    if (level != 0x23)
      goto launch_default;
    /* level == 0x23 path: full zero block, launch, spin. */
    *(unsigned int *)launch_data = 0;
    XLaunchNewImageA(0, launch_data);
    goto spin;
  }
  *(unsigned int *)launch_data = 2;
launch_default:
  XLaunchNewImageA(0, launch_data);
spin:
  while (1) {
  }
}
