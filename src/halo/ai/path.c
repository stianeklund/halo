/* path.c — AI path planning state builders.
 *
 * Corresponds to path.obj (XBE address range ~0x5dfc0–0x5ff70+).
 * __FILE__ = c:\halo\SOURCE\ai\path.c (confirmed via display_assert strings
 * in path_state_build_path at 0x5eae0).
 *
 * Ported: path_state_init (0x5dfc0), path_state_set_focus (0x5e000),
 *         path_state_set_sphere (0x5e030), path_state_set_min_speed (0x5e070),
 *         path_state_commit (0x5e090), path_state_set_obstacle (0x5e0d0),
 *         path_get_node (path node accessor with bounds assert),
 *         path_node_from_hash_table (path hash table lookup by key),
 *         path_3d_available (path ray-cast clearance check),
 *         FUN_0005ff70 (path traverse + debug snapshot),
 *         FUN_00060070 (obstacle-disc bounds-checked accessor, path.h inline).
 * Deferred: path_state_build_path (0x5eae0) — complex path evaluation,
 * deferred.
 */

#include "../../common.h"

/* All callees (csmemset, csmemcpy, scenario_get,
 * global_structure_bsp_index_get) declared via decl.h / generated header */

/* 0x005dfc0 — path_state_init
 * Zero-fills a 0x48-byte path_state record, then writes the initial fields.
 *
 * Disassembly-confirmed field layout (ESI = param_1):
 *   [ESI+0x00] = param_2  (uint32_t flags)
 *   [ESI+0x04] = param_3  (uint8_t — byte at +4 in the uint32_t slot, MSVC
 * packs) [ESI+0x08] = param_4  (int unit_handle) [ESI+0x0c] = 0xffffffff
 *
 * Note: Ghidra showed param_1+1 (dword slot) for param_3 storage, but the
 * disassembly has `MOV byte ptr [ESI+4], CL` — param_3 is stored at byte
 * offset +4, not at dword-slot +1 (+4). The decompiler rendered this as
 * `*(undefined1*)(param_1+1)` due to how it assigns dword-indexed fields.
 * The raw byte offset is +4.
 */
void path_input_new(void *param_1, uint32_t param_2, uint8_t param_3,
                    int param_4)
{
  csmemset(param_1, 0, 0x48);
  *(uint32_t *)param_1 = param_2;
  *(uint8_t *)((char *)param_1 + 4) = param_3;
  *(int *)((char *)param_1 + 8) = param_4;
  *(int *)((char *)param_1 + 0xc) = -1;
  return;
}

/* 0x005dff0 — path_state_set_ignore_object
 * Sets the ignore-object handle in a path_state record.
 *
 * Disassembly: MOV EAX,[EBP+0xc]; MOV ECX,[EBP+0x8]; MOV [ECX+0xc],EAX; RET
 */
void path_input_set_target_object(void *param_1, int param_2)
{
  *(int *)((char *)param_1 + 0xc) = param_2;
}

/* 0x005e000 — path_state_set_focus
 * Sets the focus-position fields in a path_state record.
 *
 * Disassembly-confirmed stores (EAX = param_1):
 *   [EAX+0x10] = 1  (focus_valid flag, uint8_t)
 *   [EAX+0x14] = param_2[0]  (focus_pos.x)
 *   [EAX+0x18] = param_2[1]  (focus_pos.y)
 *   [EAX+0x1c] = param_2[2]  (focus_pos.z)
 *   [EAX+0x20] = param_3  (bone index)
 */
void path_input_set_start(void *param_1, float *param_2, int param_3)
{
  *(uint8_t *)((char *)param_1 + 0x10) = 1;
  *(vector3_t *)((char *)param_1 + 0x14) = *(vector3_t *)param_2;
  *(int *)((char *)param_1 + 0x20) = param_3;
}

/* 0x005e030 — path_state_set_sphere
 * Sets the sphere-obstacle fields in a path_state record.
 *
 * Disassembly-confirmed stores (EAX = param_1, ECX/EDX = params 3/4/5):
 *   [EAX+0x24] = 1       (sphere_valid flag, uint8_t)
 *   [EAX+0x28] = param_2[0]  (sphere_pos.x)
 *   [EAX+0x2c] = param_2[1]  (sphere_pos.y)
 *   [EAX+0x30] = param_2[2]  (sphere_pos.z)
 *   [EAX+0x34] = param_4  (flags — note: [EBP+0x14] stored at +0x34, NOT +0x38)
 *   [EAX+0x38] = param_3  (inner_r — note: [EBP+0x10] stored at +0x38, NOT
 * +0x34) [EAX+0x3c] = param_5  (outer_r)
 *
 * Store rotation confirmed: MSVC emitted param_3 → +0x38, param_4 → +0x34
 * (pipeline-scheduled out-of-order). See disassembly:
 *   MOV [EAX+0x38], ECX   ; ECX = [EBP+0x10] = param_3
 *   MOV [EAX+0x34], EDX   ; EDX = [EBP+0x14] = param_4
 */
void path_input_set_attractor(void *param_1, float *param_2, float param_3,
                              uint32_t param_4, float param_5)
{
  *(uint8_t *)((char *)param_1 + 0x24) = 1;
  *(float *)((char *)param_1 + 0x28) = param_2[0];
  *(float *)((char *)param_1 + 0x2c) = param_2[1];
  *(float *)((char *)param_1 + 0x30) = param_2[2];
  *(float *)((char *)param_1 + 0x38) = param_3;
  *(uint32_t *)((char *)param_1 + 0x34) = param_4;
  *(float *)((char *)param_1 + 0x3c) = param_5;
  return;
}

/* 0x005e070 — path_state_set_min_speed
 * Sets the minimum-speed constraint fields in a path_state record.
 *
 * Disassembly-confirmed stores (EAX = param_1):
 *   [EAX+0x40] = 1       (min_speed_valid flag, uint8_t)
 *   [EAX+0x44] = param_2 (min_speed, int)
 */
void path_input_set_search_bounds(void *param_1, int param_2)
{
  *(uint8_t *)((char *)param_1 + 0x40) = 1;
  *(int *)((char *)param_1 + 0x44) = param_2;
  return;
}

/* 0x005e090 — path_state_commit
 * Zero-fills a 0x1408c-byte result buffer, then copies 0x48 bytes (0x12 dwords)
 * from the path_state (param_1) into it, stores the current scenario handle at
 * result+0x64, and writes the cam_ref handle at result+0x48.
 *
 * Disassembly-confirmed (EBX = param_2 = result buffer):
 *   csmemset(param_2, 0, 0x1408c)
 *   param_2[0x19] = scenario_get()   ; at byte offset 0x64 (0x19 * 4)
 *   MOVSD.REP ECX=0x12: copy param_1[0..0x47] → param_2[0..0x47]
 *   param_2[0x12] = param_3          ; at byte offset 0x48 (0x12 * 4)
 *
 * Note: scenario handle stored at +0x64, not +0x19 (raw dword index is 0x19).
 * The copy overwrites param_2[0..0x47], then param_3 is stored at
 * param_2[0x48].
 */
void path_state_new(void *param_1, void *param_2, void *param_3)
{
  csmemset(param_2, 0, 0x1408c);
  *(void **)((char *)param_2 + 0x64) = scenario_get();
  /* Copy 0x48 bytes from param_1 into param_2 at offset 0 (MOVSD.REP ECX=0x12)
   */
  qmemcpy(param_2, param_1, 0x48);
  *(void **)((char *)param_2 + 0x48) = param_3;
  return;
}

/* 0x005e0d0 — path_state_set_obstacle
 * Sets an obstacle hit record in a path_state.
 *
 * Disassembly-confirmed stores (EAX = param_1):
 *   [EAX+0x4c] = 1       (obstacle_valid flag, uint8_t)
 *   [EAX+0x50] = param_2[0]  (hit_pos.x)
 *   [EAX+0x54] = param_2[1]  (hit_pos.y)
 *   [EAX+0x58] = param_2[2]  (hit_pos.z)
 *   [EAX+0x5c] = param_3  (hit_flags)
 *   [EAX+0x60] = param_4  (mask)
 */
void FUN_0005e0d0(void *param_1, float *param_2, int param_3, int param_4)
{
  *(uint8_t *)((char *)param_1 + 0x4c) = 1;
  *(vector3_t *)((char *)param_1 + 0x50) = *(vector3_t *)param_2;
  *(int *)((char *)param_1 + 0x5c) = param_3;
  *(int *)((char *)param_1 + 0x60) = param_4;
}

/* 0x005e560 — path_heap_pop_cheapest_node
 * Pops the cheapest node (heap[1]) off the binary min-heap embedded in a path
 * state and returns its node index, or NONE (-1) if the heap is empty.
 *
 * Register-arg: state passed in EAX (no stack push at either call site in
 * path_state_traverse: `mov eax, edi; call 0x5e560`).
 *
 * `state->heap_count` (word at +0x11084) is a "next free slot" index into the
 * 1-indexed heap array at +0x11086 (dword entries: low word = node_index,
 * high word = quantized_cost_estimate — confirmed by the word-sized read/
 * compare at +0x1108a/+0x1108c, which is exactly heap[1]). heap_count==1
 * means the heap holds no real elements.
 *
 * Node records are 0x44 bytes starting at state+0x84 (path_get_node,
 * 0x5e760); this function computes the same `node_index * 0x44` scaled
 * pointer but folds the +0x84 node-array base directly into the two field
 * displacements it uses (+0xb0 = quantized_cost_estimate, +0xb4 =
 * heap_location), so no separate node-array-base add appears in the
 * disassembly.
 *
 * Disassembly-confirmed asserts (display_assert + system_exit(-1), all at
 * __FILE__ "c:\halo\SOURCE\ai\path.c", strings read from the pristine XBE
 * .rdata):
 *   0x572 (1394): state->heap_count >= 1
 *   0x577 (1399): (node_index >= 0) && (node_index < PATH_NODE_LIST_SIZE)
 *   0x579 (1401): state->node_list[node_index].heap_location == 1
 *   0x57a (1402): state->node_list[node_index].quantized_cost_estimate ==
 *                 state->heap[1].quantized_cost_estimate
 *
 * Pop sequence once heap_count > 1:
 *   node_index = heap[1].node_index (saved as return value)
 *   node_list[node_index].heap_location = NONE (0xffff)
 *   heap_count--
 *   if heap_count > 1: heap[1] = heap[heap_count] (move last element to
 *     root), then path_heap_bubble_down(state, 1) to restore heap order.
 * (0x5e330, push 1; call 0x5e330; add esp,4 for the heap_index stack arg.
 * BUG FIX 2026-08-30: 0x5e330's disassembly never loads EBX itself — it reads
 * [EBX+0x11084] etc. directly, relying on the caller's EBX still holding
 * `state` from this function's own "MOV EBX,EAX" at entry (0x5e563), never
 * clobbered before the call. That is an implicit register argument, missed
 * when this function was first lifted (kb.json decl only carried the stack
 * arg, tagged "not a register arg"). A plain C call from clang-compiled code
 * does not guarantee EBX holds `state`, so path_heap_bubble_down read garbage
 * node_list/heap state and tripped its heap_location invariant assert
 * ("state->node_list[parent_node_index].heap_location == parent_location",
 * path.c #1280 in the original source line numbering). Fix: kb.json decl now
 * carries the implicit state pointer explicitly as an ebx register arg, and
 * this call site passes it.)
 */
#define PATH_NODE_LIST_SIZE 0x400

short path_heap_pop_cheapest_node(void *state)
{
  short node_index;
  short heap_count;
  char *node;

  node_index = -1;

  if (!(*(short *)((char *)state + 0x11084) >= 1)) {
    display_assert("state->heap_count >= 1", "c:\\halo\\SOURCE\\ai\\path.c",
                   0x572, 1);
    system_exit(-1);
  }

  if (*(short *)((char *)state + 0x11084) > 1) {
    node_index = *(short *)((char *)state + 0x1108a);

    if (!(node_index >= 0 && node_index < PATH_NODE_LIST_SIZE)) {
      display_assert("(node_index >= 0) && (node_index < PATH_NODE_LIST_SIZE)",
                     "c:\\halo\\SOURCE\\ai\\path.c", 0x577, 1);
      system_exit(-1);
    }

    node = (char *)state + (int)node_index * 0x44;

    if (*(short *)(node + 0xb4) != 1) {
      display_assert("state->node_list[node_index].heap_location == 1",
                     "c:\\halo\\SOURCE\\ai\\path.c", 0x579, 1);
      system_exit(-1);
    }

    if (*(short *)(node + 0xb0) != *(short *)((char *)state + 0x1108c)) {
      display_assert("state->node_list[node_index].quantized_cost_estimate == "
                     "state->heap[1].quantized_cost_estimate",
                     "c:\\halo\\SOURCE\\ai\\path.c", 0x57a, 1);
      system_exit(-1);
    }

    *(short *)(node + 0xb4) = -1;

    heap_count = *(short *)((char *)state + 0x11084) - 1;
    *(short *)((char *)state + 0x11084) = heap_count;

    if (heap_count > 1) {
      unsigned int last_entry;

      last_entry =
        *(unsigned int *)((char *)state + (int)heap_count * 4 + 0x11086);
      *(unsigned int *)((char *)state + 0x1108a) = last_entry;
      path_heap_bubble_down(state, 1);
    }
  }

  return node_index;
}

/* 0x005e680 — path_heap_insert
 * Inserts a new {node_index, quantized_cost_estimate} entry into the path
 * state's binary min-heap at heap[heap_count] (the next free 1-indexed
 * slot), then restores heap order via path_heap_bubble_up.
 *
 * Register-arg: state passed in EAX (moved to EDI at entry — same
 * convention as the sibling heap functions above).
 *
 * Disassembly-confirmed assert (display_assert + system_exit(-1), at
 * __FILE__ "c:\halo\SOURCE\ai\path.c"):
 *   0x594 (1428): state->heap_count >= 1
 *
 * If state->heap_count has reached PATH_NODE_LIST_SIZE (0x400), the insert
 * is dropped after logging via error(2, "path_heap_insert: overflowed
 * static size heap") — a separate fall-through return path with no state
 * mutation (no assert/halt), confirmed by the disassembly's second exit
 * (`ADD ESP,8; POP EDI; POP EBP; RET`).
 *
 * Otherwise: stores heap_count+1 into state->heap_count, writes node_index
 * and quantized_cost_estimate into heap[old heap_count], then calls
 * path_heap_bubble_up(state, old_heap_count). path_heap_bubble_up takes
 * `state` implicitly via EDI (relies on this function's EDI, never reloaded
 * before the call — the same implicit-register-argument pattern documented
 * for path_heap_bubble_down above) and `heap_index` via EAX (this
 * function's EAX still holds the pre-increment heap_count at the call
 * site: the only instruction that wrote EAX's low 16 bits since entry is
 * `MOV AX,[EDI+0x11084]`, and nothing between that load and the call
 * touches EAX again — confirmed by disassembly at 0x5e6b0..0x5e6e4).
 */
void path_heap_insert(void *state, short node_index,
                      short quantized_cost_estimate)
{
  short heap_count;

  if (!(*(short *)((char *)state + 0x11084) >= 1)) {
    display_assert("state->heap_count >= 1", "c:\\halo\\SOURCE\\ai\\path.c",
                   0x594, 1);
    system_exit(-1);
  }

  heap_count = *(short *)((char *)state + 0x11084);

  if (heap_count < PATH_NODE_LIST_SIZE) {
    *(short *)((char *)state + 0x11084) = heap_count + 1;
    *(short *)((char *)state + (int)heap_count * 4 + 0x11086) = node_index;
    *(short *)((char *)state + (int)heap_count * 4 + 0x11088) =
      quantized_cost_estimate;

    path_heap_bubble_up(state, heap_count);
  } else {
    error(2, "path_heap_insert: overflowed static size heap");
  }
  return;
}

/* 0x005e700 — FUN_0005e700
 * Tests whether a structure-bsp collision surface is breakable and still
 * intact (not yet broken).
 *
 * Register-arg: structure_bsp passed in EAX (used directly at entry with
 * no stack load — same register-arg convention as the sibling path-heap
 * functions above; surface_index is the only stack argument, at [EBP+8]).
 *
 * Same tag_block chain as structure_test_ray2d (path_smoothing.c, 0x63710)
 * and path_3d_available (0x5e830 below):
 *   tag_block_get_element(structure_bsp + 0xb0, 0, 0x60) -> bsp element
 *   tag_block_get_element(bsp + 0x3c, surface_index, 0xc) -> collision_surface
 * The collision_surface flags byte at +8, bit 3, is
 * _collision_surface_breakable_bit (named via the display_assert string in
 * structure_test_ray2d). If clear, returns 0 immediately (disassembly: TEST
 * CL,8; JE to the plain epilogue — no assert on this path, unlike
 * structure_test_ray2d's fatal invariant check on the same bit). If set, the
 * byte at +9 indexes breakable_surfaces_get_bsp_surface_data()'s dword bitmap
 * the same way structure_test_ray2d does; returns 1 if that bit is clear
 * (surface intact) and 0 if it is set (already broken).
 *
 * Disassembly-confirmed 0x5e700-0x5e75a (bounds-table exact): the trailing
 * NEG EAX / SBB AL,AL / INC AL sequence is the compiler's 0/1 bool
 * materialization for `(bitmap_word & mask) == 0`.
 */
char FUN_0005e700(void *structure_bsp, int surface_index)
{
  void *bsp_surfaces;
  char *collision_surface;
  unsigned int *breakable_bitmap;
  unsigned int word;

  bsp_surfaces = tag_block_get_element((char *)structure_bsp + 0xb0, 0, 0x60);
  collision_surface = (char *)tag_block_get_element((char *)bsp_surfaces + 0x3c,
                                                    surface_index, 0xc);

  if ((collision_surface[8] & 8) == 0) {
    return 0;
  }

  breakable_bitmap = (unsigned int *)breakable_surfaces_get_bsp_surface_data();
  word = (unsigned char)collision_surface[9];

  return (breakable_bitmap[word >> 5] & (1u << (word & 0x1f))) == 0;
}

/* 0x005e760 — path_get_node
 * Returns a pointer to a node within the path state buffer, given a node index.
 *
 * Asserts node_index != NONE (-1) and 0 <= node_index < state->node_count
 * (short at state+0x80). Each node is 0x44 bytes, and the node array starts
 * at state+0x84.
 *
 * Disassembly-confirmed:
 *   param_1 (EDI) = path state pointer
 *   param_2 (SI)  = node_index (short, loaded as word ptr [EBP+0xc])
 *   return: MOVSX EAX,SI; IMUL EAX,EAX,0x44; LEA EAX,[EAX+EDI+0x84]
 */
char *path_get_node(char *param_1, short param_2)
{
  if (param_2 == -1) {
    display_assert("node_index != NONE", "c:\\halo\\SOURCE\\ai\\path.c", 0x611,
                   1);
    system_exit(-1);
  } else if (param_2 >= 0 && param_2 < *(short *)(param_1 + 0x80)) {
    goto done;
  }
  display_assert("(node_index >= 0) && (node_index < state->node_count)",
                 "c:\\halo\\SOURCE\\ai\\path.c", 0x612, 1);
  system_exit(-1);
done:
  return param_1 + (int)param_2 * 0x44 + 0x84;
}

/* 0x005e7e0 — path_hash_lookup
 * Looks up a node in the path state hash table by key.
 *
 * Computes a starting hash slot from (param_2 & 0x1ff) << 3, then probes the
 * hash table at state+0x1208a (array of shorts, 0x1000 entries). For each
 * non-NONE slot, checks if the node's key (at node_base + 0x8 = state +
 * node_index * 0x44 + 0x8c) matches param_2. Returns the matching node index
 * (short in AX), or -1 if not found.
 *
 * Disassembly-confirmed:
 *   ECX = hash slot index (12-bit, masked with 0xfff)
 *   AX  = hash table entry (short, node index or -1)
 *   EDI = sign-extended AX for node key comparison
 *   Loop: MOVSX EAX,CX; MOV AX,[EDX+EAX*2+0x1208a]; INC ECX; AND ECX,0xfff
 */
short path_node_from_hash_table(char *param_1, unsigned int param_2)
{
  unsigned int slot;
  short sVar1;

  slot = (param_2 & 0x1ff) << 3;
  do {
    sVar1 = *(short *)(param_1 + (short)slot * 2 + 0x1208a);
    slot = (slot + 1) & 0xfff;
  } while (sVar1 != -1 &&
           *(unsigned int *)(param_1 + (int)sVar1 * 0x44 + 0x8c) != param_2);
  return sVar1;
}

/* 0x005e830 — path ray-cast clearance check
 * Casts a ray from param_2 toward param_4 using the BSP collision tree at
 * param_1+0xb0.  Returns 1 (clear) if the ray-cast fails, the hit fraction
 * is >= 1.0, or the remaining distance after the hit is below a threshold.
 * Returns 0 otherwise (path is blocked).
 *
 * Disassembly-confirmed:
 *   ESI = param_4[0], EDI = param_4[1], [EBP-0x14] = param_4[2]  (saved dest)
 *   [EBP-0x10] = param_4[0] - param_2[0]  (delta.x)
 *   [EBP-0x0c] = param_4[1] - param_2[1]  (delta.y)
 *   [EBP-0x08] = param_4[2] - param_2[2]  (delta.z)
 *   tag_block_get_element(param_1+0xb0, 0, 0x60) -> bsp element
 *   collision_bsp_test_vector(1, bsp, 0, 0, param_2, &delta, FLT_MAX,
 * result_buf) -> ray cast Condition: (1.0 - t)^2 * dist_sq < 0.1  => clear
 * (return 1) param_5 receives the result byte; param_6 receives param_4 copy
 * (dest pos)
 */
char path_3d_available(int param_1, int *param_2, int param_3, int *param_4,
                       unsigned char *param_5, float *param_6)
{
  char cVar3;
  unsigned char uVar5;
  float local_438[264];
  float local_18;
  float delta[3];
  unsigned char local_5;

  local_18 = *((float *)param_4 + 2);
  delta[0] = *(float *)param_4 - *(float *)param_2;
  uVar5 = 0;
  local_5 = 0;
  delta[1] = *((float *)param_4 + 1) - *((float *)param_2 + 1);
  delta[2] = *((float *)param_4 + 2) - *((float *)param_2 + 2);
  cVar3 = ((char (*)(int, void *, short, int, float *, float *, float,
                     float *))0x149480)(
    1, tag_block_get_element((char *)param_1 + 0xb0, 0, 0x60), 0, 0,
    (float *)param_2, delta, 3.4028235e+38f, local_438);
  if (cVar3 == '\0' || local_438[0] >= *(float *)0x2533c8 ||
      (*(float *)0x2533c8 - local_438[0]) *
          (*(float *)0x2533c8 - local_438[0]) *
          (delta[1] * delta[1] + delta[0] * delta[0] + delta[2] * delta[2]) <
        *(float *)0x25496c) {
    uVar5 = 1;
    local_5 = 1;
  }
  if (param_5 != (unsigned char *)0) {
    *param_5 = local_5;
  }
  if (param_6 != (float *)0) {
    *param_6 = *(float *)param_4;
    param_6[1] = *((float *)param_4 + 1);
    param_6[2] = local_18;
  }
  return (char)uVar5;
}

/* 0x005e920 — path_find_initial
 * Builds an initial navigation state record from a source position.
 *
 * Zeroes a 0x5c-byte output struct, then calls path_3d_available to perform a
 * pathfinding query. If path_3d_available succeeds, the output struct is
 * populated with the destination position (from param_4), a result vector from
 * the query, and various flags/sentinel values. Returns 1 on success, 0 on
 * failure.
 *
 * Output struct layout (ESI = param_5):
 *   [+0x00] = 1              (valid flag, byte)
 *   [+0x04] = param_4[0]     (destination position x)
 *   [+0x08] = param_4[1]     (destination position y)
 *   [+0x0c] = param_4[2]     (destination position z)
 *   [+0x10] = 0xFFFFFFFF     (sentinel)
 *   [+0x14] = 0x00000000     (cleared)
 *   [+0x18] = local_byte     (byte from path_3d_available output)
 *   [+0x19] = 1              (byte flag)
 *   [+0x1a] = 0              (byte flag)
 *   [+0x1c] = 0xFFFFFFFF     (sentinel)
 *   [+0x20] = local_vec[0]   (result vector x)
 *   [+0x24] = local_vec[1]   (result vector y)
 *   [+0x28] = local_vec[2]   (result vector z)
 */
char path_3d_build_path(int param_1, int *param_2, int param_3, int *param_4,
                        char *param_5)
{
  char result;
  float local_vec[3];
  uint8_t local_byte;

  csmemset(param_5, 0, 0x5c);
  result = path_3d_available(param_1, param_2, param_3, param_4, &local_byte,
                             local_vec);
  if (result != 0) {
    *(float *)(param_5 + 0x20) = local_vec[0];
    *(float *)(param_5 + 0x24) = local_vec[1];
    *(float *)(param_5 + 0x28) = local_vec[2];
    *(uint8_t *)(param_5 + 0x19) = 1;
    *(int *)(param_5 + 0x1c) = -1;
    *(uint8_t *)(param_5 + 0x1a) = 0;
    *(uint8_t *)(param_5 + 0x18) = local_byte;
    *(int *)(param_5 + 0x04) = param_4[0];
    *(int *)(param_5 + 0x08) = param_4[1];
    *(int *)(param_5 + 0x0c) = param_4[2];
    *(int *)(param_5 + 0x10) = -1;
    *(int *)(param_5 + 0x14) = 0;
    *(uint8_t *)param_5 = 1;
  }
  return *param_5;
}

/* 0x005eae0 — path_build_steps
 * Builds the step list for a path from the traversal node graph.
 *
 * Walks backward through the node chain (via parent links at node+0x02)
 * collecting raw steps (datum_ref + entry_point) indexed by depth.
 * Then applies smoothing (FUN_000633b0) and obstacle avoidance (FUN_00061750)
 * to produce the final step list stored in nav_state_out.
 *
 * nav_state_out layout (0x5c bytes):
 *   [+0x00] = valid (byte)
 *   [+0x04] = destination position (3 floats)
 *   [+0x10] = datum ref
 *   [+0x14] = distance
 *   [+0x18] = all_nodes_encountered flag (byte)
 *   [+0x19] = step_count (byte)
 *   [+0x1a] = zero (byte)
 *   [+0x1c] = step array (step_count * 16 bytes)
 *
 * Each step is 16 bytes: datum_ref(4) + position(12).
 *
 * Returns: nav_state_out[0] (valid flag byte).
 */
char path_state_build_path(unsigned int path_buf, unsigned int *nav_state_out)
{
  unsigned int *puVar9;
  unsigned int *puVar10;
  int iVar6;
  int iVar8;
  int node_ptr;
  short sVar4;
  short sVar5;
  char cVar3;
  char all_nodes_flag;
  unsigned int raw_steps[256]; /* 64 entries * 4 dwords = 0x400 bytes */
  unsigned int final_steps[16]; /* 4 entries * 4 dwords = 0x40 bytes */
  unsigned int smooth_steps[16]; /* 4 entries * 4 dwords = 0x40 bytes */
  unsigned int prev_node_index;
  int prev_node_ptr;
  unsigned int cur_index;
  int final_step_count;
  int raw_step_count;
  int smooth_step_count;

  puVar10 = nav_state_out;
  if (*(int *)(path_buf + 0x48) != 0) {
    *(unsigned short *)(*(int *)(path_buf + 0x48) + 0x12) = 0;
  }
  *(unsigned char *)nav_state_out = 0;

  if (*(char *)(path_buf + 0x4c) == '\0') {
    if (*(int *)(path_buf + 0x48) != 0) {
      *(unsigned short *)(*(int *)(path_buf + 0x48) + 0x12) = 1;
    }
    goto LAB_0005ef13;
  }

  cur_index = path_node_from_hash_table((char *)path_buf,
                                        *(unsigned int *)(path_buf + 0x5c));
  if ((short)cur_index == -1) {
    if (*(float *)(path_buf + 0x6c) < *(float *)(path_buf + 0x60)) {
      cur_index = (unsigned int)*(unsigned short *)(path_buf + 0x68);
      iVar6 = (int)path_get_node((char *)path_buf, cur_index);
      puVar10[1] = *(unsigned int *)(path_buf + 0x74);
      puVar10[2] = *(unsigned int *)(path_buf + 0x78);
      puVar10[3] = *(unsigned int *)(path_buf + 0x7c);
      puVar10[4] = *(unsigned int *)(iVar6 + 8);
      puVar10[5] = *(unsigned int *)(path_buf + 0x6c);
      goto LAB_0005eb88;
    }
  } else {
    iVar6 = (int)path_get_node((char *)path_buf, cur_index);
    /* memcpy 5 dwords from path_buf+0x50 to nav_state_out+0x04 */
    puVar9 = (unsigned int *)(path_buf + 0x50);
    puVar10 = nav_state_out + 1;
    for (iVar8 = 5; iVar8 != 0; iVar8--) {
      *puVar10 = *puVar9;
      puVar10++;
      puVar9++;
    }
    nav_state_out[5] = 0;
    puVar10 = nav_state_out;
  LAB_0005eb88:
    if ((short)cur_index != -1) {
      int depth_plus_one = *(short *)(iVar6 + 0x2e) + 1;
      smooth_step_count = 0;
      final_step_count = 0;
      all_nodes_flag = 1;
      prev_node_index = 0xffffffff;
      prev_node_ptr = 0;
      raw_step_count = 0x40;
      if (depth_plus_one < 0x41) {
        raw_step_count = depth_plus_one;
      }

      do {
        unsigned int next_index;
        int depth;

        node_ptr = (int)path_get_node((char *)path_buf, cur_index);
        sVar4 = *(short *)(node_ptr + 0x2e);

        if (sVar4 < 0x40) {
          if (sVar4 < 0 || sVar4 >= (short)raw_step_count) {
            display_assert(
              "(node->depth >= 0) && (node->depth < raw_step_count)",
              "c:\\halo\\SOURCE\\ai\\path.c", 0x1e8, 1);
            system_exit(-1);
          }

          raw_steps[*(short *)(node_ptr + 0x2e) * 4] =
            *(unsigned int *)(node_ptr + 8);
          depth = (int)*(short *)(node_ptr + 0x2e);

          if ((short)prev_node_index == -1) {
            /* First node: copy destination from nav_state_out */
            raw_steps[depth * 4 + 1] = puVar10[1];
            raw_steps[depth * 4 + 3] = puVar10[3];
            raw_steps[depth * 4 + 2] = puVar10[2];
          } else {
            if (depth != *(short *)(prev_node_ptr + 0x2e) - 1) {
              display_assert("node->depth == child_node->depth - 1",
                             "c:\\halo\\SOURCE\\ai\\path.c", 0x1f0, 1);
              system_exit(-1);
            }
            depth = (int)*(short *)(node_ptr + 0x2e);
            raw_steps[depth * 4 + 1] = *(unsigned int *)(prev_node_ptr + 0xc);
            raw_steps[depth * 4 + 2] = *(unsigned int *)(prev_node_ptr + 0x10);
            raw_steps[depth * 4 + 3] = *(unsigned int *)(prev_node_ptr + 0x14);
          }
        } else {
          all_nodes_flag = 0;
        }

        prev_node_index = cur_index;
        next_index = (unsigned int)*(unsigned short *)(node_ptr + 2);
        prev_node_ptr = node_ptr;
        cur_index = next_index;
      } while (*(unsigned short *)(node_ptr + 2) != 0xffff);

      sVar4 = (short)prev_node_index;
      cur_index = (unsigned int)*(unsigned short *)(node_ptr + 2);

      if (sVar4 == -1) {
        display_assert("child_node_index != NONE",
                       "c:\\halo\\SOURCE\\ai\\path.c", 0x1fb, 1);
        system_exit(-1);
      }
      if (*(short *)(node_ptr + 0x2e) != 0) {
        display_assert("child_node->depth == 0", "c:\\halo\\SOURCE\\ai\\path.c",
                       0x1fc, 1);
        system_exit(-1);
      }

      sVar4 = game_connection();
      iVar6 = raw_step_count;
      if (sVar4 == 0 && *(char *)0x5ac9d0 != '\0') {
        smooth_step_count = 4;
        if ((short)raw_step_count < 5) {
          smooth_step_count = raw_step_count;
        }
        csmemcpy(smooth_steps, raw_steps, (int)(short)smooth_step_count << 4);
      } else {
        FUN_000633b0(path_buf, raw_step_count, raw_steps, &smooth_step_count,
                     smooth_steps, &all_nodes_flag);
        iVar6 = raw_step_count;
      }

      sVar4 = (short)iVar6;
      sVar5 = game_connection();
      if (sVar5 == 0 && *(char *)0x5ac9cf != '\0') {
        final_step_count = smooth_step_count;
        if (4 < (short)smooth_step_count) {
          final_step_count = 4;
        }
        csmemcpy(final_steps, smooth_steps, (int)(short)final_step_count << 4);
      LAB_0005ede3:
        *(char *)((char *)puVar10 + 0x19) = (char)final_step_count;
        *(char *)(puVar10 + 6) = all_nodes_flag;
        *(unsigned char *)puVar10 = 1;
        *(char *)((char *)puVar10 + 0x1a) = 0;
        csmemcpy(puVar10 + 7, final_steps, (int)(short)final_step_count << 4);

        puVar9 = nav_state_out;
        if (*(char *)(puVar10 + 6) != '\0') {
          cVar3 = *(char *)((char *)puVar10 + 0x19);
          puVar10[1] = puVar10[(int)cVar3 * 4 + 4];
          puVar10[2] = puVar10[(int)cVar3 * 4 + 5];
          puVar10[3] = puVar10[(int)cVar3 * 4 + 6];
          nav_state_out[4] = puVar10[(int)cVar3 * 4 + 3];
          sVar4 = (short)raw_step_count;
          *(float *)(puVar9 + 5) =
            FUN_0001ad60((float *)(puVar10 + 1), (float *)(path_buf + 0x50));
          puVar10 = puVar9;
        }

        if (*(int *)(path_buf + 0x48) != 0) {
          *(unsigned short *)(*(int *)(path_buf + 0x48) + 0x12) = 5;
        }
      } else {
        cVar3 = FUN_00061750(path_buf, smooth_step_count, smooth_steps,
                             &final_step_count, final_steps, &all_nodes_flag);
        if (*(int *)(path_buf + 0x48) == 0) {
          if (cVar3 != '\0')
            goto LAB_0005ede3;
        } else {
          if (cVar3 != '\0')
            goto LAB_0005ede3;
          *(unsigned short *)(*(int *)(path_buf + 0x48) + 0x12) = 4;
        }
      }

      /* Debug: store raw, smooth, and final steps */
      if (*(int *)(path_buf + 0x48) != 0) {
        *(short *)(*(int *)(path_buf + 0x48) + 0x140fc) = sVar4;
        csmemcpy((void *)(*(int *)(path_buf + 0x48) + 0x14100), raw_steps,
                 (int)sVar4 << 4);
        *(short *)(*(int *)(path_buf + 0x48) + 0x14500) =
          (short)smooth_step_count;
        csmemcpy((void *)(*(int *)(path_buf + 0x48) + 0x14504), smooth_steps,
                 (int)(short)smooth_step_count << 4);
        *(short *)(*(int *)(path_buf + 0x48) + 0x14544) =
          (short)final_step_count;
        csmemcpy((void *)(*(int *)(path_buf + 0x48) + 0x14548), final_steps,
                 (int)(short)final_step_count << 4);
      }
      goto LAB_0005ef13;
    }
  }

  /* Neither branch produced a valid path */
  if (*(int *)(path_buf + 0x48) != 0) {
    *(unsigned short *)(*(int *)(path_buf + 0x48) + 0x12) =
      (unsigned short)(*(short *)(path_buf + 0x68) != -1) + 2;
  }

LAB_0005ef13:
  if (*(int *)(path_buf + 0x48) == 0) {
    return *(char *)puVar10;
  }

  /* Copy nav_state_out (0x5c bytes = 0x17 dwords) into debug buffer */
  puVar9 = (unsigned int *)(*(int *)(path_buf + 0x48) + 0x140a0);
  for (iVar6 = 0x17; iVar6 != 0; iVar6--) {
    *puVar9 = *puVar10;
    puVar10++;
    puVar9++;
  }

  if (*(short *)(*(int *)(path_buf + 0x48) + 0x12) != 5) {
    *(char *)(*(int *)(path_buf + 0x48) + 0xd) = 1;
  }
  if (*(short *)(*(int *)(path_buf + 0x48) + 0x12) == 0) {
    display_assert("state->debug->path_build_result != _path_build_result_none",
                   "c:\\halo\\SOURCE\\ai\\path.c", 0x265, 1);
    system_exit(-1);
    return *(char *)nav_state_out;
  }
  return *(char *)nav_state_out;
}

/* 0x005f240 — build_path_edges_for_surface
 * Walks the edge ring of one collision-BSP surface and fills an output array
 * of up to 0x40 edge records (0x20 bytes each). Returns the number written.
 *
 * Register-arg: structure_bsp in EAX (used at entry with no stack load:
 * `MOV ECX,[EAX+0x1e8]` / `ADD EAX,0xb0` at 0x5f246-0x5f251). surface_index
 * is [EBP+8], edges_out is [EBP+0xc].
 *
 * Same tag_block chain as FUN_0005e700 above:
 *   tag_block_get_element(structure_bsp + 0xb0, 0, 0x60)     -> bsp
 *   tag_block_get_element(bsp + 0x3c, surface_index, 0xc)    -> surface
 *   tag_block_get_element(bsp + 0x48, edge_index, 0x18)      -> edge
 *   tag_block_get_element(bsp + 0x54, vertex_index, 0x10)    -> vertex
 * [structure_bsp+0x1e8] is a per-surface byte array; its element indexed by
 * the adjacent surface index is stored at out+0x4. Meaning unproven.
 *
 * Edge layout (0x18, offsets disassembly-derived only):
 *   +0x00 int vertex_a, +0x04 int vertex_b,
 *   +0x08/+0x0c int next_edge (selected by which surface we came from),
 *   +0x10/+0x14 int surface_a / surface_b.
 * `is_right` = (surface_index == edge[5]); the adjacent surface is the OTHER
 * of the two (0x5f2dc SETZ AL / 0x5f2ec SETZ DL selects edge[4 + !is_right]),
 * while the ring walk continues through edge[2 + is_right] (0x5f39c).
 *
 * Output record (0x20):
 *   +0x00 int adjacent_surface_index
 *   +0x04 byte  structure_bsp[0x1e8][adjacent_surface_index]
 *   +0x08 float[3] vertex_a position (copied as three dwords, 0x5f35a-0x5f36a)
 *   +0x14 float[3] vertex_b - vertex_a (FLD [v1]; FSUB [v0]; FSTP,
 *                  0x5f370-0x5f390 — v1 minus v0, not the reverse)
 *
 * Return is 16-bit: the loop counter lives in BX (MOVSX ESI,BX at 0x5f2df)
 * and the fall-through exit reloads only AX (`MOV AX,word ptr [EBP-8]` at
 * 0x5f3a9), so the upper half of EAX is not part of the result.
 */
short build_path_edges_for_surface(void *structure_bsp, int surface_index,
                                   char *edges_out)
{
  unsigned char *surface_flags;
  char *bsp;
  int *surface;
  int *edge;
  float *v0;
  float *v1;
  char *out;
  int edge_index;
  int adjacent;
  int is_right;
  short edge_count;

  surface_flags = *(unsigned char **)((char *)structure_bsp + 0x1e8);
  bsp = (char *)tag_block_get_element((char *)structure_bsp + 0xb0, 0, 0x60);
  edge_count = 0;

  if (surface_index < 0 || surface_index >= *(int *)(bsp + 0x3c)) {
    display_assert(
      "(surface_index >= 0) && (surface_index < bsp->surfaces.count)",
      "c:\\halo\\SOURCE\\ai\\path.c", 0x5d8, 1);
    system_exit(-1);
  }

  surface = (int *)tag_block_get_element(bsp + 0x3c, surface_index, 0xc);
  edge_index = surface[1];

  for (;;) {
    edge = (int *)tag_block_get_element(bsp + 0x48, edge_index, 0x18);
    is_right = (surface_index == edge[5]);
    out = edges_out + edge_count * 0x20;
    edge_count++;

    adjacent = edge[4 + (is_right == 0)];
    *(int *)out = adjacent;
    if (adjacent != -1 && (adjacent < 0 || adjacent >= *(int *)(bsp + 0x3c))) {
      display_assert("(edge->adjacent_surface_index >= 0) && "
                     "(edge->adjacent_surface_index < bsp->surfaces.count)",
                     "c:\\halo\\SOURCE\\ai\\path.c", 0x5ee, 1);
      system_exit(-1);
    }

    out[4] = (char)surface_flags[*(int *)out];

    v0 = (float *)tag_block_get_element(bsp + 0x54, edge[0], 0x10);
    v1 = (float *)tag_block_get_element(bsp + 0x54, edge[1], 0x10);

    *(int *)(out + 0x8) = ((int *)v0)[0];
    *(int *)(out + 0xc) = ((int *)v0)[1];
    *(int *)(out + 0x10) = ((int *)v0)[2];
    *(float *)(out + 0x14) = v1[0] - v0[0];
    *(float *)(out + 0x18) = v1[1] - v0[1];
    *(float *)(out + 0x1c) = v1[2] - v0[2];

    if (edge_count == 0x40) {
      break;
    }
    edge_index = edge[2 + is_right];
    if (edge_index == surface[1]) {
      break;
    }
  }

  return edge_count;
}

/* 0x005ff70 — path traverse and debug snapshot
 * Initializes a path traverse operation on a path buffer, then optionally
 * copies the resulting state into a debug record.
 *
 * Increments one of two global 16-bit counters depending on a flag at +0x4c
 * (obstacle_valid). Clears the node list, resets distance fields, calls
 * FUN_0005ef80 (@edi) to set up the initial path node. If that succeeds,
 * calls path_state_traverse to perform the full traverse. If a debug record
 * exists at +0x48, copies the entire path buffer into it, stores the BSP index,
 * and asserts the traverse result is non-zero (not _path_traverse_result_none).
 * If the result is not 5, marks the debug record as needing attention.
 *
 * Returns: char (0 = failed/skipped, nonzero = traverse result from
 * path_state_traverse)
 */
char FUN_0005ff70(unsigned int *param_1)
{
  char cVar1;
  short uVar2;
  char local_5;

  local_5 = 0;
  if (*(char *)((char *)param_1 + 0x4c) != '\0') {
    (*(short *)0x5ac7f4)++;
  } else {
    (*(short *)0x5ac76c)++;
  }
  *(short *)((char *)param_1 + 0x80) = 0;
  *(short *)((char *)param_1 + 0x11084) = 1;
  csmemset((char *)param_1 + 0x1208a, -1, 0x2000);
  *(unsigned int *)((char *)param_1 + 0x6c) = 0x7f7fffff;
  *(unsigned int *)((char *)param_1 + 0x70) = 0x7f7fffff;
  *(short *)((char *)param_1 + 0x68) = (short)0xffff;
  if (*(unsigned int *)((char *)param_1 + 0x48) != 0) {
    *(short *)(*(unsigned int *)((char *)param_1 + 0x48) + 0x10) = 0;
  }
  cVar1 = FUN_0005ef80(param_1);
  if (cVar1 != '\0') {
    local_5 = path_state_traverse(param_1);
  } else {
    if (*(unsigned int *)((char *)param_1 + 0x48) != 0) {
      *(short *)(*(unsigned int *)((char *)param_1 + 0x48) + 0x10) = 1;
    }
  }
  if (*(unsigned int *)((char *)param_1 + 0x48) != 0) {
    qmemcpy((char *)(*(unsigned int *)((char *)param_1 + 0x48) + 0x14), param_1,
            0x1408c);
    uVar2 = global_structure_bsp_index_get();
    *(short *)(*(unsigned int *)((char *)param_1 + 0x48) + 0xe) = uVar2;
    if (*(short *)(*(unsigned int *)((char *)param_1 + 0x48) + 0x10) == 0) {
      display_assert(
        "state->debug->path_traverse_result != _path_traverse_result_none",
        "c:\\halo\\SOURCE\\ai\\path.c", 0x32d, 1);
      system_exit(-1);
    }
    if (*(short *)(*(unsigned int *)((char *)param_1 + 0x48) + 0x10) != 5) {
      *(char *)(*(unsigned int *)((char *)param_1 + 0x48) + 0xd) = 1;
      return local_5;
    }
  }
  return local_5;
}

/* 0x00060070 — obstacle-disc bounds-checked accessor (path.h inline function,
 * instantiated standalone in path.obj; TU = c:\halo\source\ai\path.h).
 *
 * This is the SAME inline bounds check that obstacles_test_circle/obstacles_add_disc
 * (src/halo/structures/structures.c) each duplicate at their own call sites
 * — identical display_assert text, file string, and line number confirm it:
 *   "disc_index>=0 && disc_index<obstacles->disc_count &&
 *    obstacles->disc_count<=MAXIMUM_DISC_COUNT"
 *   "c:\halo\source\ai\path.h", 0x18c
 * Header record layout (same as those two): obstacles+0x2 = int16_t
 * disc_count (valid range 0..MAXIMUM_DISC_COUNT==0x80); discs begin at
 * obstacles+0x8 with a 24-byte (0x18) stride, so disc N is at
 * obstacles+8+N*0x18.
 *
 * Disassembly-confirmed: cdecl, 2 stack args only (MOV EDI,[EBP+8];
 * MOV SI,[EBP+0xc]; plain RET — no register args, no ADD ESP,N cleanup).
 * Bounds violated -> display_assert + system_exit(-1), matching every other
 * lifted call site of this same inline check.
 *
 * Returns &obstacles->disc[disc_index] (record pointer; record field types
 * are not established at this call site — see obstacles_test_circle/obstacles_add_disc for
 * confirmed individual field offsets within the 24-byte record).
 */
void *FUN_00060070(void *obstacles, int16_t disc_index)
{
  char *base;
  short disc_count;

  base = (char *)obstacles;
  disc_count = *(short *)(base + 2);
  if (disc_index < 0 || disc_count <= disc_index || disc_count > 0x80) {
    display_assert("disc_index>=0 && disc_index<obstacles->disc_count && "
                   "obstacles->disc_count<=MAXIMUM_DISC_COUNT",
                   "c:\\halo\\source\\ai\\path.h", 0x18c, 1);
    system_exit(-1);
  }
  return base + 8 + disc_index * 0x18;
}

/* 0x000600c0 - obstacle-disc link accessor.
 *
 * The fingerprinted Ghidra artifact for this attempt held only
 * {"error":"Ghidra is not reachable at http://localhost:8089"} in every
 * field (decompile, disassembly, callees, call_site_audit, struct_offsets),
 * so the evidence below is read directly from the pristine XBE
 * (halo-patched/cachebeta.xbe) with capstone, bounded 0x600c0-0x600e4 per
 * the committed tools/verify/function_bounds.json entry:
 *
 *   PUSH EBP; MOV EBP,ESP
 *   MOV EAX,[EBP+0xc]        ; disc_index
 *   CMP AX,0xffff            ; == NONE (-1)?
 *   JE  0x600df
 *   PUSH EAX                 ; arg2 = disc_index
 *   MOV EAX,[EBP+8]          ; obstacles
 *   PUSH EAX                 ; arg1 = obstacles
 *   CALL 0x60070             ; FUN_00060070 (bounds-checked disc accessor)
 *   MOVSX EAX,word [EAX+2]   ; sign-extended int16_t at disc+0x2
 *   ADD ESP,8                ; cdecl cleanup, 2 stack args
 *   POP EBP; RET
 *  0x600df:
 *   OR EAX,0xffffffff        ; return -1
 *   POP EBP; RET
 *
 * cdecl, two stack args only, no register args. Sole callee is
 * FUN_00060070 above (already ported, same TU), whose bounds check is the
 * only side effect on the taken path.
 *
 * Field meaning: +0x2 within the 24-byte (0x18) disc record is an int16_t,
 * sign-extended to a 32-bit int return (the reference MOVSX on the found
 * path and OR EAX,-1 on the sentinel path together prove the return is a
 * full dword, not a word). The -1 sentinel pass-through is consistent with
 * a disc-link field, but no string or assert evidence names it at this
 * call site, so it stays field_02 (offset accessed, meaning unproven).
 */
int FUN_000600c0(void *obstacles, int16_t disc_index)
{
  if (disc_index != -1) {
    return *(short *)((char *)FUN_00060070(obstacles, disc_index) + 2);
  }
  return -1;
}

/* 0x000600f0 — obstacle-avoidance step bounds-checked accessor (inline
 * function; instantiated standalone in path.obj, same pattern as
 * FUN_00060070 above). __FILE__ in the assert string is
 * "c:\halo\SOURCE\ai\path_obstacle_avoidance.c", line 0x28 (40) —
 * confirmed by reading the two .rdata strings out of the pristine XBE at
 * the reference disassembly's push operands (0x25e9b0 / 0x25ea14):
 *   "step_index>=0 && step_index<path->step_count && "
 *   "path->step_count<=MAXIMUM_OBSTACLE_AVOIDANCE_STEPS"
 * This names the fields directly: path+0x2c = int16_t step_count (valid
 * range 0..MAXIMUM_OBSTACLE_AVOIDANCE_STEPS==0x80), step records begin at
 * path+0x30 with a 40-byte (0x28) stride, so step N is at
 * path+0x30+N*0x28.
 *
 * Disassembly-confirmed (synthesized per-function reference,
 * 000600f0-0006013b.obj): cdecl, 2 stack args only (MOV EDI,[EBP+8];
 * MOV SI,[EBP+0xc]; plain RET — no register args, no ADD ESP,N cleanup).
 * Bounds violated -> display_assert + system_exit(-1), matching every
 * other lifted call site of this same inline check.
 *
 * Sole caller in this TU is FUN_00060910 (below), which calls this purely
 * for its bounds-check side effect and discards the returned pointer.
 *
 * Returns &path->step[step_index] (record pointer; individual field
 * offsets within the 40-byte record are not established at this call
 * site).
 */
void *FUN_000600f0(void *path, int16_t step_index)
{
  char *base;
  short step_count;

  base = (char *)path;
  step_count = *(short *)(base + 0x2c);
  if (step_index < 0 || step_count <= step_index || step_count > 0x80) {
    display_assert("step_index>=0 && step_index<path->step_count && "
                   "path->step_count<=MAXIMUM_OBSTACLE_AVOIDANCE_STEPS",
                   "c:\\halo\\SOURCE\\ai\\path_obstacle_avoidance.c", 0x28, 1);
    system_exit(-1);
  }
  return base + 0x30 + step_index * 0x28;
}

/* 0x00060140 — path-obstacle-avoidance step-index heap accessor.
 *
 * Ghidra was unreachable for this attempt (cached artifact held only
 * "Ghidra is not reachable at http://localhost:8089" for every field, and
 * the live MCP connection failed this session too). Evidence below is read
 * directly from the pristine XBE (halo-patched/cachebeta.xbe) with capstone,
 * bounded 0x60140-0x6019d per the committed function_bounds.json entry.
 *
 * Disassembly-confirmed:
 *   PUSH EBP; MOV EBP,ESP; PUSH ESI
 *   MOV SI,[EBP+0xc]              ; heap_index (int16_t stack arg)
 *   TEST SI,SI
 *   PUSH EDI; MOV EDI,[EBP+8]     ; path pointer
 *   JL 0x60163                    ; heap_index<0 -> assert
 *   MOV AX,[EDI+0x1430]           ; path->heap_count
 *   CMP SI,AX; JGE 0x60163        ; heap_index>=heap_count -> assert
 *   CMP AX,0x80; JLE 0x6018f      ; heap_count<=0x80 -> ok
 *   ; fail (fallthrough when heap_count>0x80, or from either JL/JGE above):
 *   PUSH 1; PUSH 0x31; PUSH 0x25ea14; PUSH 0x25ea40; CALL display_assert
 *   PUSH -1; CALL system_exit; ADD ESP,0x14
 *   ; ok (0x6018f) and fail-fallthrough both compute the same read:
 *   MOVSX EAX,SI; MOV AX,[EDI+EAX*2+0x1432]   ; path->heap[heap_index]
 *   POP EDI; POP ESI; POP EBP; RET
 *
 * The two .rdata strings read directly out of the XBE at the pushed operands
 * are "heap_index>=0 && heap_index<path->heap_count && path->heap_count<=
 * MAXIMUM_OBSTACLE_AVOIDANCE_STEPS" (0x25ea40) and
 * "c:\halo\SOURCE\ai\path_obstacle_avoidance.c" (0x25ea14), line 0x31 (49).
 *
 * path->heap_count at +0x1430 sits immediately after FUN_000600f0's
 * MAXIMUM_OBSTACLE_AVOIDANCE_STEPS(0x80)-entry, 0x28-byte step array based at
 * +0x30 (0x30+0x80*0x28==0x1430), and +0x1432 is confirmed by FUN_00060910
 * (push) / FUN_00060970 (pop-front) elsewhere in this file as a int16_t
 * count(+0x1430)/array(+0x1432, 0x80 entries) pair on the same struct — this
 * function is the bounds-checked read accessor for that same heap array,
 * each entry a step index into the step array above.
 *
 * Return width: int16_t (only AX is read/written on both the ok and fail
 * paths; the sole caller below, FUN_00060200, only inspects SI of the
 * result).
 */
int16_t FUN_00060140(void *path, int16_t heap_index)
{
  char *base;
  short heap_count;

  base = (char *)path;
  heap_count = *(short *)(base + 0x1430);
  if (heap_index < 0 || heap_count <= heap_index || heap_count > 0x80) {
    display_assert("heap_index>=0 && heap_index<path->heap_count && "
                   "path->heap_count<=MAXIMUM_OBSTACLE_AVOIDANCE_STEPS",
                   "c:\\halo\\SOURCE\\ai\\path_obstacle_avoidance.c", 0x31, 1);
    system_exit(-1);
  }
  return *(short *)(base + 0x1432 + (int)heap_index * 2);
}

/* 0x000601a0 — 0-based binary-heap parent-index helper.
 *
 * Ghidra was unreachable for this attempt (cached artifact held only
 * "Ghidra is not reachable at http://localhost:8089" for every field, and
 * the live MCP connection failed this session too). Evidence below is
 * read directly from the pristine XBE (halo-patched/cachebeta.xbe) with
 * the same capstone-disassembly method vc71_verify.py uses to build its
 * reference (tools/verify/vc71_verify.py:_xbe_read / _true_end_offset),
 * bounded 0x601a0-0x601d3 per the committed function_bounds.json entry.
 *
 * Disassembly (51 bytes, single basic block plus one assert-tail branch):
 *   PUSH EBP; MOV EBP,ESP; PUSH ESI
 *   MOV SI,[EBP+8]            ; heap_index (int16_t stack arg)
 *   TEST SI,SI; JG 0x601ca    ; heap_index > 0 -> skip assert
 *   PUSH 1; PUSH 0x39; PUSH 0x25ea14; PUSH 0x25eaa4
 *   CALL 0x8d9f0              ; display_assert(reason, file, line, halt)
 *   PUSH -1; CALL 0x8e2f0     ; system_exit(-1)
 *   ADD ESP,0x14
 * 0x601ca: MOVSX EAX,SI; DEC EAX; SAR EAX,1   ; (heap_index-1) >> 1
 *   POP ESI; POP EBP; RET
 *
 * The two .rdata strings at 0x25eaa4/0x25ea14 (read from the XBE) are
 * "heap_index>0" and "c:\halo\SOURCE\ai\path_obstacle_avoidance.c", line
 * 0x39 (57) — a DIFFERENT original source file than this TU's own heap
 * routines (path_heap_pop_cheapest_node / path_heap_insert above assert
 * against "c:\halo\SOURCE\ai\path.c"), so this is a distinct, standalone
 * 0-based heap used by path_obstacle_avoidance.c that happened to link
 * into path.obj. display_assert/system_exit are both already in kb.json
 * with plain stack-arg decls (no @<reg> callees here).
 *
 * A full .text scan for E8 CALLs targeting 0x601a0 across every code
 * section of the pristine XBE found zero direct callers — this helper is
 * either called only through a function-pointer/vtable slot not visible
 * to a static relative-call scan, or is otherwise not reached in this
 * build; no caller evidence constrains param/return width beyond the
 * function's own body.
 *
 * The tail computes (heap_index-1)>>1, the standard 0-based binary-heap
 * parent-index formula (valid only for heap_index>0, hence the assert —
 * index 0 is the root and has no parent). Written as `>> 1`, NOT `/ 2`:
 * the disassembly is DEC EAX; SAR EAX,1 with no CDQ/sign-adjustment
 * sequence, which is what a plain signed `/2` would require to round
 * toward zero for a possibly-negative dividend. A raw SAR with no
 * adjustment is what MSVC emits for a literal `>> 1` in source, so the
 * original expression must have used shift, not division (they only
 * coincide here because the assert already guarantees a non-negative
 * operand — irrelevant to which one the compiler actually emitted).
 *
 * Return width: kept int16_t to match this file's established convention
 * for heap/step indices (FUN_00060970 above, FUN_000600f0's step_index).
 * Not disassembly-provable either way — EAX is left with a full 32-bit
 * result and never truncated before RET, and no caller was found to
 * pin the declared width. The value itself is unaffected: heap_index>0
 * is asserted, so (heap_index-1)>>1 always fits int16_t.
 */
int16_t FUN_000601a0(int16_t heap_index)
{
  if (heap_index <= 0) {
    display_assert("heap_index>0",
                   "c:\\halo\\SOURCE\\ai\\path_obstacle_avoidance.c", 0x39, 1);
    system_exit(-1);
  }
  return (int16_t)((heap_index - 1) >> 1);
}

/* 0x000601e0 — 0-based binary-heap LEFT-child-index helper.
 *
 * The fingerprinted Ghidra artifact for this attempt was invalid: every
 * field (decompile_c, disassembly, callees, call_site_audit) held only
 * {"error":"Ghidra is not reachable at http://localhost:8089"}, and the
 * live MCP bridge was down this session too. Evidence below is read
 * directly out of the pristine XBE (halo-patched/cachebeta.xbe) with
 * tools/verify/xbe_reference.py emit --addr 0x601e0 — the same bytes
 * vc71_verify.py's own reference derives from — bounded 0x601e0-0x601eb
 * per the committed function_bounds.json entry (end 0x601ec).
 *
 * Full 12 bytes: 55 8b ec 8b 45 08 8d 44 00 01 5d c3
 *   PUSH EBP; MOV EBP,ESP
 *   MOV EAX,[EBP+8]           ; heap_index
 *   LEA EAX,[EAX+EAX*1+0x1]   ; eax = 2*heap_index + 1
 *   POP EBP; RET
 *
 * Single basic block, no calls, no asserts (a child-index formula needs
 * no index>0 guard, unlike the parent helper FUN_000601a0 above). This
 * completes the standard 0-based binary-heap index triple in this file:
 * FUN_000601a0 parent (heap_index-1)>>1, this left child 2i+1, and
 * FUN_000601f0 below right child 2i+2 — byte-identical to this function
 * except for the LEA displacement (0x2 instead of 0x1).
 *
 * No source-side callers exist (grep over src/); the binary caller set
 * was NOT enumerated this attempt, because the fingerprinted artifact's
 * caller/xref lists were invalid (Ghidra unreachable) — so no caller is
 * known to pin the parameter/return width. int16_t matches
 * the established heap-index convention of both siblings, and the
 * identically-shaped FUN_000601f0 scores 100% VC71 with that typing, so
 * the narrowing is codegen-free here. EAX carries the full 32-bit result
 * and is never truncated before RET, so the width is a convention choice,
 * not disassembly-provable.
 */
int16_t FUN_000601e0(int16_t heap_index)
{
  return (int16_t)(heap_index * 2 + 1);
}

/* 0x000601f0 — 0-based binary-heap right-child-index helper.
 *
 * Ghidra was unreachable for this attempt (cached artifact held only
 * "Ghidra is not reachable at http://localhost:8089" for every field, and
 * the live MCP connection failed this session too). Evidence below is
 * read directly from the pristine XBE (halo-patched/cachebeta.xbe) via
 * tools/verify/xbe_reference.py (the same bytes vc71_verify.py's own
 * reference derives from), bounded 0x601f0-0x601fb per the committed
 * function_bounds.json entry (end 0x601fc).
 *
 * Full disassembly (12 bytes, single basic block, no calls):
 *   PUSH EBP; MOV EBP,ESP
 *   MOV EAX,[EBP+8]           ; heap_index
 *   LEA EAX,[EAX+EAX*1+0x2]   ; eax = 2*heap_index + 2
 *   POP EBP; RET
 *
 * The sibling function FUN_000601a0 immediately above computes the
 * 0-based binary-heap PARENT index (heap_index-1)>>1. The immediately
 * preceding address FUN_000601e0 (now ported above) is byte-identical
 * except for its LEA displacement (0x1 instead of 0x2):
 * [EAX+EAX*1+0x1] = 2*heap_index+1. Together this is the standard 0-based
 * binary-heap index triple (parent, left child, right child):
 * FUN_000601e0 is left-child, FUN_000601f0 (this function) is right-child.
 * Unlike the parent helper, no display_assert here — a child-index formula
 * needs no index>0 guard.
 *
 * A full .text scan for E8 CALLs targeting 0x601f0 across every code
 * section of the pristine XBE found zero direct callers, so no caller
 * pins param/return width. Declared int16_t param/return to match this
 * file's established convention for heap indices (FUN_000601a0 above) —
 * EAX is left with the full 32-bit result and never truncated before RET,
 * so this is a style choice consistent with the sibling, not
 * disassembly-provable either way.
 */
int16_t FUN_000601f0(int16_t heap_index)
{
  return (int16_t)(heap_index * 2 + 2);
}

/* 0x00060200 — path-obstacle-avoidance step float-field accessor via heap
 * indirection.
 *
 * Ghidra was unreachable for this attempt (cached artifact held only
 * "Ghidra is not reachable at http://localhost:8089" for every field, and
 * the live MCP connection failed this session too). Evidence below is read
 * directly from the pristine XBE (halo-patched/cachebeta.xbe) with capstone,
 * bounded 0x60200-0x60257 per the committed function_bounds.json entry
 * (end 0x60258; the trailing bytes 0x60258-0x6025f are 8 NOP pad bytes
 * before the next function at 0x60260, confirming the true end).
 *
 * Disassembly-confirmed:
 *   PUSH EBP; MOV EBP,ESP
 *   MOV EAX,[EBP+0xc]              ; param_2, full dword read
 *   PUSH ESI; PUSH EDI
 *   MOV EDI,[EBP+8]                ; path pointer
 *   PUSH EAX; PUSH EDI; CALL FUN_00060140   ; step_index = path->heap[param_2]
 *   ADD ESP,8
 *   MOV ESI,EAX
 *   TEST SI,SI; JL 0x6022b                  ; step_index<0 -> assert
 *   MOV AX,[EDI+0x2c]                       ; path->step_count
 *   CMP SI,AX; JGE 0x6022b                  ; step_index>=step_count -> assert
 *   CMP AX,0x80; JLE 0x60248                ; step_count<=0x80 -> ok
 *   ; fail (fallthrough when step_count>0x80, or from either JL/JGE above):
 *   PUSH 1; PUSH 0x28; PUSH 0x25ea14; PUSH 0x25e9b0; CALL display_assert
 *   PUSH -1; CALL system_exit; ADD ESP,0x14
 *   ; ok (0x60248) and fail-fallthrough both compute the same FLD:
 *   MOVSX EAX,SI; ADD EAX,2; LEA ECX,[EAX+EAX*4]; FLD dword ptr [EDI+ECX*8]
 *   POP EDI; POP ESI; POP EBP; RET
 *
 * The assert reason string (0x25e9b0) read directly out of the XBE is
 * byte-identical to FUN_000600f0's own assert text —
 * "step_index>=0 && step_index<path->step_count && path->step_count<=
 * MAXIMUM_OBSTACLE_AVOIDANCE_STEPS" — same file (0x25ea14) and same line
 * 0x28 (40), confirming path->step_count lives at the same +0x2c offset on
 * the same "path" struct FUN_000600f0 already established.
 *
 * EDI+(step_index+2)*40 == EDI+0x30+step_index*0x28+0x20 (0x30/0x28 are
 * FUN_000600f0's confirmed step-array base/stride; 0x50==2*40 accounts for
 * the "+2"): this reads a float at offset 0x20 within path->step[step_index]
 * — the same step array/stride FUN_000600f0 indexes. The field itself is
 * not independently named; no other lifted call site in this TU establishes
 * what lives at step+0x20 beyond "a float".
 *
 * param_2 declared int16_t to match this file's established convention for
 * heap/step indices (it is forwarded unmodified to FUN_00060140, whose own
 * param is int16_t; the MOV EAX,[EBP+0xc] full-dword read is just the
 * ordinary int16_t->int default-argument-promotion push, identical in
 * shape whether the source type is short or int).
 */
float FUN_00060200(void *path, int16_t param_2)
{
  char *base;
  short step_index;
  short step_count;

  base = (char *)path;
  step_index = FUN_00060140(path, param_2);
  step_count = *(short *)(base + 0x2c);
  if (step_index < 0 || step_count <= step_index || step_count > 0x80) {
    display_assert("step_index>=0 && step_index<path->step_count && "
                   "path->step_count<=MAXIMUM_OBSTACLE_AVOIDANCE_STEPS",
                   "c:\\halo\\SOURCE\\ai\\path_obstacle_avoidance.c", 0x28, 1);
    system_exit(-1);
  }
  return *(float *)(base + ((int)step_index + 2) * 40);
}

/* 0x00060260 — error_heap: dump the obstacle-avoidance step heap to the
 * error log, one line per heap entry.
 *
 * Evidence: fingerprinted Ghidra bundle
 * b5b3bf171119dc3ac77aeab5d0640ad97949fa404a2242658cfbdd4b54a63da2
 * (decompile + disassembly + call-site audit), bounded 0x60260-0x60328 per
 * the committed function_bounds.json entry (end 0x60329).
 *
 * ABI (disassembly-confirmed): the function never loads EBX — it reads
 * [EBX+0x1430], [EBX+0x2c] and [EBX+EAX*8] directly at entry, so the `path`
 * pointer is an implicit EBX register argument (kb.json decl carries
 * @<ebx>). One ordinary cdecl stack argument at [EBP+8] (MOV EDX,[EBP+0x8])
 * is forwarded unchanged as the first argument of error(), whose kb.json
 * decl types it `unsigned __int16` — the error severity/type code.
 *
 * Disassembly-confirmed body:
 *   MOV AX,[EBX+0x1430]        ; path->heap_count
 *   XOR ESI,ESI                ; heap_index = 0
 *   TEST AX,AX; JLE end        ; hoisted first test of the for-condition
 * loop:
 *   TEST SI,SI; JL fail1
 *   CMP SI,AX; JGE fail1
 *   CMP AX,0x80; JLE ok1       ; inlined heap accessor bounds assert
 * fail1: PUSH 1; PUSH 0x31; PUSH 0x25ea14; PUSH 0x25ea40
 *   CALL display_assert; PUSH -1; CALL system_exit
 * ok1:
 *   MOVSX EAX,SI; MOV DI,[EBX+EAX*2+0x1432]   ; step_index = path->heap[i]
 *   TEST DI,DI; JL fail2
 *   MOV AX,[EBX+0x2c]; CMP DI,AX; JGE fail2
 *   CMP AX,0x80; JLE ok2       ; inlined step accessor bounds assert
 * fail2: PUSH 1; PUSH 0x28; PUSH 0x25ea14; PUSH 0x25e9b0
 *   CALL display_assert; PUSH -1; CALL system_exit
 * ok2:
 *   MOV EDX,[EBP+0x8]
 *   MOVSX EAX,DI; ADD EAX,2; LEA EAX,[EAX+EAX*4]
 *   FLD dword [EBX+EAX*8]      ; == path + (step_index+2)*40
 *   MOVSX EAX,SI
 *   FSTP dword [EBP-0x4]       ; narrow to a float local
 *   MOV ECX,[EBP-0x4]          ; the SAME float's raw bits
 *   FLD dword [EBP-0x4]
 *   PUSH ECX                   ; last vararg -> "%x"
 *   SUB ESP,8; FSTP qword [ESP] ; float promoted to double -> "%.12g"
 *   PUSH EAX                   ; heap_index -> "%3d"
 *   PUSH 0x25eab4              ; "%3d. %.12g (%x)"
 *   PUSH EDX                   ; error type
 *   CALL error; MOV AX,[EBX+0x1430]; ADD ESP,0x18
 *   INC ESI; CMP SI,AX; JL loop
 *
 * Both inlined accessors are the same two bounds checks already lifted
 * standalone in this TU (FUN_00060140 at line 0x31 and FUN_000600f0 /
 * FUN_00060200 at line 0x28, same __FILE__ 0x25ea14 and same reason strings
 * 0x25ea40 / 0x25e9b0). They are written inline here, NOT as calls to those
 * helpers, because the reference contains no CALL to either — inlining that
 * the original compiler performed must be preserved, not re-outlined.
 *
 * The float read is path + (step_index+2)*40 == path+0x30+step_index*0x28
 * +0x20, i.e. the same float at +0x20 of the 40-byte step record that
 * FUN_00060200 reads; the field has no independently established name.
 *
 * The third vararg is the dword of that float, not a second float: ECX is
 * loaded from the narrowed [EBP-0x4] slot with an integer MOV and pushed as
 * a plain dword (the "%x" conversion), while the separate FLD/FSTP qword of
 * the same slot supplies the "%.12g" double. This is the standard Bungie
 * float-debug-print idiom (value plus its hex bit pattern).
 */
void error_heap(void *path, unsigned short type)
{
  char *base;
  short heap_index;
  short heap_count;
  short step_index;
  short step_count;
  float value;

  base = (char *)path;
  for (heap_index = 0; heap_index < *(short *)(base + 0x1430); heap_index++) {
    heap_count = *(short *)(base + 0x1430);
    if (heap_index < 0 || heap_count <= heap_index || heap_count > 0x80) {
      display_assert("heap_index>=0 && heap_index<path->heap_count && "
                     "path->heap_count<=MAXIMUM_OBSTACLE_AVOIDANCE_STEPS",
                     "c:\\halo\\SOURCE\\ai\\path_obstacle_avoidance.c", 0x31,
                     1);
      system_exit(-1);
    }

    step_index = *(short *)(base + 0x1432 + (int)heap_index * 2);
    step_count = *(short *)(base + 0x2c);
    if (step_index < 0 || step_count <= step_index || step_count > 0x80) {
      display_assert("step_index>=0 && step_index<path->step_count && "
                     "path->step_count<=MAXIMUM_OBSTACLE_AVOIDANCE_STEPS",
                     "c:\\halo\\SOURCE\\ai\\path_obstacle_avoidance.c", 0x28,
                     1);
      system_exit(-1);
    }

    value = *(float *)(base + ((int)step_index + 2) * 40);
    error(type, "%3d. %.12g (%x)", (int)heap_index, value, *(uint32_t *)&value);
  }
}

/* 0x00060910 — bounded push onto a fixed-size 16-bit value list
 * The owning structure's type is not established by this call site (it is
 * NOT the giant path-state struct used elsewhere in this file — offset
 * 0x1430 would fall in the middle of node_list[74] of that struct, which
 * makes no sense for a standalone counter/array pair). Only the two touched
 * offsets are confirmed by disassembly:
 *   +0x1430: short count, valid range 0..0x80
 *   +0x1432: array of up to 0x80 shorts, indexed by count
 *
 * Register-arg ABI (no stack params, confirmed by disassembly — MOV ESI,EAX
 * at entry and the unpaired unaff_BX use both indicate this function's own
 * incoming args arrive in registers, not on the stack):
 *   param_1 (EAX) = pointer to the owning structure
 *   value   (BX)  = 16-bit value to append
 *
 * Bracketed by FUN_00060330(path, tag) calls with two different literal .rdata
 * addresses (0x25eb18 on entry, 0x25eb04 only on the success exit — NOT
 * called on the full-list failure path; the JGE at 0x60930 branches straight
 * to the return-0 tail without a second bracket call). FUN_000600f0(param_1,
 * value) runs after the count increment is stored but before the value is
 * written into the array; FUN_000604e0(old_count) runs right after the
 * array write, passed the PRE-increment index (old_count), matching the
 * cumulative-cleanup call chain at 0x6093d-0x6095a (ADD ESP,0x10 at 0x6095f
 * cleans all 4 pushes from this chain in one shot; the call-site audit's
 * per-call cleanup attribution is misleading here — cross-checked against
 * disassembly).
 *
 * Returns 1 if the value was appended, 0 if the list was already full
 * (count >= 0x80). Ghidra's decompile mis-typed this `void` — AL carries the
 * real return value on both paths (MOV AL,0x1 / XOR AL,AL before RET).
 */
char FUN_00060910(void *param_1, int16_t value)
{
  short count;

  FUN_00060330(param_1, (const char *)0x25eb18);
  count = *(short *)((char *)param_1 + 0x1430);
  if (count < 0x80) {
    *(short *)((char *)param_1 + 0x1430) = count + 1;
    FUN_000600f0(param_1, value);
    *(short *)((char *)param_1 + 0x1432 + count * 2) = value;
    FUN_000604e0(param_1, count);
    FUN_00060330(param_1, (const char *)0x25eb04);
    return 1;
  }
  return 0;
}

/* 0x00060970 — pop-front (swap-with-last) from the same fixed-size 16-bit
 * value list FUN_00060910 pushes onto (see that function's comment for the
 * +0x1430 count / +0x1432 array[0x80] layout; owning struct type still not
 * established).
 *
 * Register-arg ABI (no stack params, confirmed by disassembly — MOV ESI,EAX
 * at entry, no other incoming register use):
 *   param_1 (EAX) = pointer to the owning structure
 *
 * Bracketed by FUN_00060330(path, tag) calls with two different literal .rdata
 * addresses, same pattern as FUN_00060910: 0x25eb40 fires unconditionally on
 * entry; 0x25eb2c only fires on the non-empty exit (the JLE at 0x6098c goes
 * straight to the empty-return tail without it). FUN_00060670(0) runs right
 * after array[0] is overwritten with the swapped-in last element, before the
 * second bracket call — ADD ESP,0x8 at 0x609c3 cleans both this call's PUSH 0
 * (0x609a9) and the FUN_00060330 call's PUSH 0x25eb2c (0x609b7) in one shot;
 * FUN_00060670's own kb.json decl was `void FUN_00060670(void)` (0 args) —
 * wrong, corrected to `void FUN_00060670(int param_1)` to match this pushed
 * constant (has_reg_args confirmed false for this callee by the call-site
 * audit, so EAX's live value at the call is not consumed).
 *
 * Removes the front element (array[0]) by copying the current last element
 * into its slot and decrementing count (order not preserved), returning the
 * ORIGINAL front element. Returns -1 (0xffff) if the list was already empty
 * — Ghidra mis-typed this `void`; MOV AX,DI / OR AX,0xffff before RET carry
 * the real int16_t return value on both paths.
 */
int16_t FUN_00060970(void *param_1)
{
  short count;
  short front;
  short last;

  FUN_00060330(param_1, (const char *)0x25eb40);
  count = *(short *)((char *)param_1 + 0x1430);
  if (0 < count) {
    count = count - 1;
    *(short *)((char *)param_1 + 0x1430) = count;
    last = *(short *)((char *)param_1 + 0x1432 + count * 2);
    front = *(short *)((char *)param_1 + 0x1432);
    *(short *)((char *)param_1 + 0x1432) = last;
    FUN_00060670(param_1, 0);
    FUN_00060330(param_1, (const char *)0x25eb2c);
    return front;
  }
  return (int16_t)-1;
}
