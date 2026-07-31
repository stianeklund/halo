/* c:\halo\source\objects\widgets\widget_types.h
 *
 * Header path recovered from the XBE. Unlike most recovered headers, this one
 * is proven twice over: the path string at 0x29ae0c is the __FILE__ of an
 * assert that lives INSIDE this header (an inline widget-type accessor at
 * header line 0x96), and every one of its six call sites in the binary
 * (0x135fdd, 0x136064, 0x1360bd, 0x13611d, 0x1361ef, 0x136324, 0x13649e,
 * 0x13653d) inlines the same bounds check:
 *
 *     "type>=0 && type<NUMBER_OF_WIDGET_TYPES"   (string @0x29ade4)
 *
 * so NUMBER_OF_WIDGET_TYPES below is the original identifier, not our coinage.
 * See the header-recovery skill for the extraction sweep. */

#ifndef HALO_OBJECTS_WIDGETS_WIDGET_TYPES_H
#define HALO_OBJECTS_WIDGETS_WIDGET_TYPES_H

#include "../../../types.h"

/* Bound tested at every widget-type index check (CMP SI,0x5; JL), and the
 * element count of the definition table at 0x323528. Name is verbatim from the
 * assert predicate string. */
#define NUMBER_OF_WIDGET_TYPES 5

/* ---------------------------------------------------------------------------
 * widget_type_definition — one element of the static widget-type dispatch
 * table at 0x323528. Five entries, one per widget tag group; the table data
 * gives them in order: 'flag', 'ant!', 'glw!', 'mgs2', 'elec'.
 *
 * Stride 0x28 is proven twice, independently:
 *   - 0x1364ba  LEA ESI,[EAX + EAX*0x4] ; LEA ESI,[ESI*0x8 + 0x323528]
 *               (index * 5 * 8 == index * 0x28)
 *   - 0x136568  ADD EDI,0x28            (walking the table)
 *
 * The struct name is inferred from the assert string
 * "!type_definition->needs_lighting || lighting" (@0x29ae90), which names the
 * variable `type_definition`; the field name `needs_lighting` in it IS verbatim
 * and is the only field here with string evidence. The proc fields are named
 * mechanically from what the call site does with them, per naming-confidence:
 * where the role is not proven, the slot keeps a positional name.
 *
 * Every slot has an observed access, so there are no unknown ranges.
 * ------------------------------------------------------------------------- */
typedef struct widget_type_definition {
    /* +0x00: widget tag group FourCC. Linear-searched against a caller-supplied
     *        group at 0x135f36 (CMP dword ptr [ECX*0x8 + 0x323528],EDX), and
     *        asserted non-zero during init at 0x135ffb. */
    uint32_t group_tag;

    /* +0x04: byte, NOT int -- 0x1364cf reads MOV AL,byte ptr [ESI + 0x4].
     *        When set, the render path asserts a lighting pointer was supplied
     *        (widgets.c:0xf1). Name verbatim from the assert string. */
    uint8_t  needs_lighting;
    uint8_t  pad_05[3];      /* +0x05: no access observed */

    /* +0x08: called with no arguments from the widget-pool init at 0x135ffb
     *        (MOV EAX,[EDI]; CALL EAX with no stack cleanup). */
    void   (*initialize_proc)(void);

    /* +0x0c/+0x10/+0x14: three further no-argument hooks, each walked by its
     *        own loop (0x136082, 0x1360db, 0x13613b). Each is called the same
     *        way, so the disassembly does not distinguish their roles -- the
     *        surrounding pool calls hint at new-map/dispose ordering but that
     *        is not proven, so these keep positional names. */
    void   (*proc_0c)(void);
    void   (*proc_10)(void);
    void   (*proc_14)(void);

    /* +0x18: 0x13625a CALL EAX with one pushed arg and ADD ESP,0x4; the result
     *        is compared against -1 and stored as the widget's handle. */
    int    (*new_proc)(int definition_handle);

    /* +0x1c: 0x136383 CALL dword ptr [EDI + 0x1c], one arg, ADD ESP,0x4.
     *        Asserted non-NULL before the call (widgets.c:0xbe). */
    void   (*delete_proc)(int widget_handle);

    /* +0x20: 0x136562 CALL EAX, one arg, ADD ESP,0x4. The table walk for this
     *        slot starts at 0x323548 == 0x323528 + 0x20. */
    void   (*update_proc)(float delta_time);

    /* +0x24: 0x136507 CALL dword ptr [ESI + 0x24] with ADD ESP,0x10 -- four
     *        cdecl args, pushed right-to-left as
     *        (object_handle, definition_handle, lighting, parent_model_effect).
     *        Null-tested via 0x32354c == 0x323528 + 0x24 before use.
     *
     *        `lighting` is typed int here to match the kb.json decl of the
     *        caller (widgets_render_object_widgets) and every existing call
     *        site, but that width is all the binary proves: the caller only
     *        null-tests it and passes it through, never dereferencing it, so
     *        int vs pointer is indistinguishable at this call. An earlier note
     *        in widgets.c recorded it as `void *lighting`, which the name and
     *        the guarding assert ("!type_definition->needs_lighting ||
     *        lighting" -- i.e. a type needing lighting must be given some)
     *        support. Treat the pointer reading as the likely one and re-type
     *        both this field and the caller together if a callee is lifted
     *        that actually dereferences it. */
    void   (*render_proc)(int object_handle, int definition_handle, int lighting,
                          void *parent_model_effect);
} widget_type_definition;
cs(widget_type_definition, 0x28);
co(widget_type_definition, group_tag,       0x00);
co(widget_type_definition, needs_lighting,  0x04);
co(widget_type_definition, initialize_proc, 0x08);
co(widget_type_definition, proc_0c,         0x0c);
co(widget_type_definition, proc_10,         0x10);
co(widget_type_definition, proc_14,         0x14);
co(widget_type_definition, new_proc,        0x18);
co(widget_type_definition, delete_proc,     0x1c);
co(widget_type_definition, update_proc,     0x20);
co(widget_type_definition, render_proc,     0x24);

#endif /* HALO_OBJECTS_WIDGETS_WIDGET_TYPES_H */
