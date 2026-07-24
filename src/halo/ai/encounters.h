/* c:\halo\SOURCE\ai\encounters.h
 *
 * Header path recovered from the XBE: asserts inside this header's inline
 * functions stamp "..\ai\encounters.h" into __FILE__, which proves both that
 * the header existed and the directory it lived in. See the header-recovery
 * skill for the extraction sweep.
 *
 * Only types whose use is confined to src/halo/ai/ belong here. */

#ifndef HALO_AI_ENCOUNTERS_H
#define HALO_AI_ENCOUNTERS_H

#include "../../types.h"

/* ---------------------------------------------------------------------------
 * encounter_definition — one element of the scenario "encounters" tag-block
 * (element stride 0xb0, block at scenario+0x42c). This is READ-ONLY tag data.
 * The mutable per-map runtime encounter record (data_t pool *0x5ab270) is a
 * DIFFERENT struct with its own offsets — see encounters.c FUN_0005a120, which
 * copies encounter_def->field_24 into runtime encounter+0x2. Do not conflate
 * the two: identical numeric offsets on each are unrelated fields.
 *
 * Only offsets with an observed access are named; the wide gaps (0x26..0x7e,
 * 0xa4..0xb0) are genuine unobserved tag data, left as pad_XX. Struct name is
 * verbatim from the assert string
 *   "encounter_definition->squads.count <= MAXIMUM_SQUADS_PER_ENCOUNTER"
 * (encounters.c:0x5a4).
 * ------------------------------------------------------------------------- */
typedef struct encounter_definition {
    char       name[0x20];       /* +0x00: <=32-byte name; strnicmp(elem,name,0x20) in FUN_00053e20 @0x53e20 */
    uint32_t   flags;            /* +0x20: bit1/2/3 -> runtime 0x3c/0x40/0x41 (encounters.c:2688-2692); bit4 tested (actors.c:8205, actor_looking.c:4407) */
    int16_t    field_24;         /* +0x24: copied to runtime encounter+0x2 (encounters.c:2684) */
    uint8_t    pad_26[0x58];     /* +0x26: no access observed */
    int16_t    bsp_index;        /* +0x7e: compared to structure BSP index (ai.c:919, ai_reconnect_to_structure_bsp) */
    tag_block  squads;           /* +0x80: squads.count asserted @encounters.c:0x5a4; element stride 0xe8, max 0x40 */
    tag_block  platoons;         /* +0x8c: platoons (encounters.c:2654 "platoon count from encounter_def+0x8c"); element stride 0xac, max 0x20 (inferred) */
    tag_block  firing_positions; /* +0x98: firing positions (actor_looking.c:6442 "iterate over encounter's firing positions"); element stride 0x18 (inferred) */
    uint8_t    pad_a4[0xc];      /* +0xa4: no access observed */
} encounter_definition;
cs(encounter_definition, 0xb0);
co(encounter_definition, name,             0x00);
co(encounter_definition, flags,            0x20);
co(encounter_definition, field_24,         0x24);
co(encounter_definition, bsp_index,        0x7e);
co(encounter_definition, squads,           0x80);
co(encounter_definition, platoons,         0x8c);
co(encounter_definition, firing_positions, 0x98);

#endif /* HALO_AI_ENCOUNTERS_H */
