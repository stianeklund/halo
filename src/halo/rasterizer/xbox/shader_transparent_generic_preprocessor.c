/* Original TU:
 * c:\halo\SOURCE\rasterizer\xbox\shader_transparent_generic_preprocessor.c
 * (proven by the __FILE__ string at 0x2aed00 pushed by the asserts below). */

/* Bounds proven by the two CMP/JL pairs at 0x17be69 (CMP SI,0x19) and
 * 0x17be84 (CMP DI,0x8) — both JL, so the upper bound is exclusive. Names
 * come verbatim from the assert message strings. */
#define NUMBER_OF_SHADER_TRANSPARENT_GENERIC_STAGE_INPUTS 25
#define NUMBER_OF_SHADER_TRANSPARENT_GENERIC_STAGE_INPUT_MAPPINGS 8

/* Three read-only tables, referenced by absolute address (contents not yet
 * proven read-only, so they are not re-declared as static const arrays):
 *   0x2aea20  int[25]     per-register base value; -1 selects the 2D table
 *   0x2aeaec  int[8]      per-mapping OR-mask
 *   0x2aeb30  int[25][8]  row-major (stride 8) register x mapping value */
#define SHADER_TRANSPARENT_GENERIC_REGISTER_TABLE ((int *)0x2aea20)
#define SHADER_TRANSPARENT_GENERIC_MAPPING_TABLE ((int *)0x2aeaec)
#define SHADER_TRANSPARENT_GENERIC_REGISTER_MAPPING_TABLE ((int *)0x2aeb30)

/* 0x17be50 — resolve one shader_transparent_generic stage input (register
 * index) plus its input mapping into the hardware combiner input dword.
 *
 * No prologue at all in the original: both indices arrive in registers
 * (SI/DI, sign-extended with MOVSX before use) and the result is returned in
 * EAX on both exit paths (0x17bebb / 0x17bec6). Asserts are at source lines
 * 0xd4 and 0xd5 and tail into system_exit(-1). */
int FUN_0017be50(short register_index, short mapping_index)
{
  int base;

  if (register_index < 0 ||
      register_index >= NUMBER_OF_SHADER_TRANSPARENT_GENERIC_STAGE_INPUTS) {
    display_assert("register_index>=0 && "
                   "register_index<NUMBER_OF_SHADER_TRANSPARENT_GENERIC_STAGE_"
                   "INPUTS",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\shader_transparent_"
                   "generic_preprocessor.c",
                   0xd4, 1);
    system_exit(-1);
  }
  if (mapping_index < 0 ||
      mapping_index >=
        NUMBER_OF_SHADER_TRANSPARENT_GENERIC_STAGE_INPUT_MAPPINGS) {
    display_assert("mapping_index>=0 && "
                   "mapping_index<NUMBER_OF_SHADER_TRANSPARENT_GENERIC_STAGE_"
                   "INPUT_MAPPINGS",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\shader_transparent_"
                   "generic_preprocessor.c",
                   0xd5, 1);
    system_exit(-1);
  }

  base = SHADER_TRANSPARENT_GENERIC_REGISTER_TABLE[register_index];
  if (base == -1) {
    return SHADER_TRANSPARENT_GENERIC_REGISTER_MAPPING_TABLE
      [mapping_index +
       register_index *
         NUMBER_OF_SHADER_TRANSPARENT_GENERIC_STAGE_INPUT_MAPPINGS];
  }
  return SHADER_TRANSPARENT_GENERIC_MAPPING_TABLE[mapping_index] | base;
}

/* Bounds proven by CMP AX,6 at 0x17bf4d and the two CMP AX,2 at 0x17bf7c /
 * 0x17bfab — all JL, so the upper bounds are exclusive. Names come verbatim
 * from the assert message strings. */
#define NUMBER_OF_SHADER_TRANSPARENT_GENERIC_STAGE_OUTPUT_MAPPINGS 6
#define NUMBER_OF_SHADER_TRANSPARENT_GENERIC_STAGE_OUTPUT_FUNCTIONS 2

/* Per-output-mapping combiner bits: 6 dwords at 0x2aec18, read at 0x17bfda as
 * [eax*4 + 0x2aec18]. Contents { 0x00, 0x30, 0x10, 0x20, 0x08, 0x18 }.
 * Referenced by absolute address (as the two tables above) so the load keeps
 * the original's absolute-displacement form. */
#define SHADER_TRANSPARENT_GENERIC_OUTPUT_MAPPING_TABLE ((int *)0x2aec18)

/* The only four stage fields this function touches; the rest of the structure
 * is still unknown, so no struct is declared for it yet. Widths are proven by
 * the accessing instruction listed against each. */
#define STAGE_FLAGS(stage) (*(unsigned char *)(stage))
/* TEST byte ptr [ESI],1 @0x17bff0 */
#define STAGE_COLOR_OUTPUT_AB_FUNCTION(stage)                                  \
  (*(short *)((char *)(stage) + 0x4e)) /* MOV AX,word [ESI+0x4e] @0x17bf73 */
#define STAGE_COLOR_OUTPUT_CD_FUNCTION(stage)                                  \
  (*(short *)((char *)(stage) + 0x52)) /* MOV AX,word [ESI+0x52] @0x17bfa2 */
#define STAGE_COLOR_OUTPUT_MAPPING(stage)                                      \
  (*(short *)((char *)(stage) + 0x56)) /* MOVSX EAX,word [ESI+0x56] @0x17bfd6 */

/* 0x17bf20 — pack one shader_transparent_generic stage's colour-output
 * configuration into the hardware combiner's output-mapping/flag dword.
 *
 * No prologue at all in the original: the stage pointer arrives in ESI and the
 * result is returned in EAX by the bare RET at 0x17bff8. Asserts are at source
 * lines 0xf6..0xf9 and tail into system_exit(-1). */
int FUN_0017bf20(void *stage)
{
  int output_flags;

  if (stage == 0) {
    display_assert("stage",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\shader_transparent_"
                   "generic_preprocessor.c",
                   0xf6, 1);
    system_exit(-1);
  }
  if (STAGE_COLOR_OUTPUT_MAPPING(stage) < 0 ||
      STAGE_COLOR_OUTPUT_MAPPING(stage) >=
        NUMBER_OF_SHADER_TRANSPARENT_GENERIC_STAGE_OUTPUT_MAPPINGS) {
    display_assert("stage->color_output_mapping>=0 && "
                   "stage->color_output_mapping<NUMBER_OF_SHADER_TRANSPARENT_"
                   "GENERIC_STAGE_OUTPUT_MAPPINGS",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\shader_transparent_"
                   "generic_preprocessor.c",
                   0xf7, 1);
    system_exit(-1);
  }
  if (STAGE_COLOR_OUTPUT_AB_FUNCTION(stage) < 0 ||
      STAGE_COLOR_OUTPUT_AB_FUNCTION(stage) >=
        NUMBER_OF_SHADER_TRANSPARENT_GENERIC_STAGE_OUTPUT_FUNCTIONS) {
    display_assert("stage->color_output_AB_function>=0 && "
                   "stage->color_output_AB_function<NUMBER_OF_SHADER_"
                   "TRANSPARENT_GENERIC_STAGE_OUTPUT_FUNCTIONS",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\shader_transparent_"
                   "generic_preprocessor.c",
                   0xf8, 1);
    system_exit(-1);
  }
  if (STAGE_COLOR_OUTPUT_CD_FUNCTION(stage) < 0 ||
      STAGE_COLOR_OUTPUT_CD_FUNCTION(stage) >=
        NUMBER_OF_SHADER_TRANSPARENT_GENERIC_STAGE_OUTPUT_FUNCTIONS) {
    display_assert("stage->color_output_CD_function>=0 && "
                   "stage->color_output_CD_function<NUMBER_OF_SHADER_"
                   "TRANSPARENT_GENERIC_STAGE_OUTPUT_FUNCTIONS",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\shader_transparent_"
                   "generic_preprocessor.c",
                   0xf9, 1);
    system_exit(-1);
  }

  output_flags = SHADER_TRANSPARENT_GENERIC_OUTPUT_MAPPING_TABLE
    [STAGE_COLOR_OUTPUT_MAPPING(stage)];
  if (STAGE_COLOR_OUTPUT_AB_FUNCTION(stage) == 1) {
    output_flags |= 0x2;
  }
  if (STAGE_COLOR_OUTPUT_CD_FUNCTION(stage) == 1) {
    output_flags |= 0x1;
  }
  if ((STAGE_FLAGS(stage) & 0x1) != 0) {
    output_flags |= 0x4;
  }
  return output_flags;
}
