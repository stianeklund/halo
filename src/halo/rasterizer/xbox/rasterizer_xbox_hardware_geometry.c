/*
 * rasterizer_vertex_buffer_new @ 0x168cd0 — creates a static D3D vertex buffer
 * for a rasterizer_vertex_buffer record and, when source vertices are supplied,
 * uploads them and fills the record in.
 *
 * Frame (cdecl, 5 stack args, bool in AL):
 *   [EBP+0x08] vertex_buffer   record to fill (0x14 bytes)
 *   [EBP+0x0c] vertex_type     passed to rasterizer_geometry_get_vertex_size,
 *                              stored back as a word at +0x00
 *   [EBP+0x10] vertex_count
 *   [EBP+0x14] vertices        source data, may be NULL (buffer left empty)
 *   [EBP+0x18] buffer_size
 * Locals: [EBP-0x04] d3d_vertex_buffer, [EBP-0x08] locked vertex data.
 *
 * Record layout proven by the stores at 0x168df1-0x168e01 and the 0x14-byte
 * csmemset at 0x168e15:
 *   +0x00 word  vertex type      +0x02 word  (never accessed)
 *   +0x04 dword vertex count     +0x08 dword written 0 here
 *   +0x0c dword source vertices  +0x10 dword IDirect3DVertexBuffer8 *
 *
 * 0x476ab0 is global_d3d_device; 0x325652 is the word render-phase marker,
 * raised to 2 across the Lock and cleared afterwards (0x168db4/0x168dc9).
 */
/* 0x168cd0 */
bool rasterizer_vertex_buffer_new(void *vertex_buffer, int vertex_type,
                                  int vertex_count, void *vertices,
                                  int buffer_size)
{
  void *d3d_vertex_buffer;
  void *locked_vertices;
  short vertex_size;
  bool success;
  int hr;

  success = true;
  vertex_size = (short)rasterizer_geometry_get_vertex_size((short)vertex_type);

  if (vertex_buffer == 0) {
    display_assert("vertex_buffer",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                   "hardware_geometry.c",
                   0x18, 1);
    system_exit(-1);
  }
  if (vertex_size * vertex_count != buffer_size && vertices != 0) {
    display_assert("vertex_size*count==buffer_size || !vertices",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                   "hardware_geometry.c",
                   0x19, 1);
    system_exit(-1);
  }

  if (vertex_count == 0) {
    success = false;
  }
  if (*(void **)0x476ab0 == 0) {
    success = false;
  } else if (success) {
    hr = D3DDevice_CreateVertexBuffer((uint32_t)buffer_size, 8, 0, 1,
                                      &d3d_vertex_buffer);
    if (hr >= 0) {
      success = true;
    } else {
      success = false;
      rasterizer_error(
        hr,
        "IDirect3DDevice8_CreateVertexBuffer(global_d3d_device, buffer_size,"
        " RASTERIZER_STATIC_BUFFER_USAGE, 0, RASTERIZER_STATIC_BUFFER_POOL,"
        " &d3d_vertex_buffer)");
    }
    if (d3d_vertex_buffer == 0) {
      success = false;
    }
    if (!success) {
      d3d_vertex_buffer = 0;
    }
  }

  if (vertices != 0) {
    if (success) {
      void *lock_buffer;
      uint32_t lock_size;

      lock_buffer = d3d_vertex_buffer;
      lock_size = (uint32_t)buffer_size;
      *(short *)0x325652 = 2;
      D3DVertexBuffer_Lock(lock_buffer, 0, lock_size,
                          &locked_vertices, 0);
      *(short *)0x325652 = 0;
      if (locked_vertices == 0) {
        success = false;
        locked_vertices = 0; /* dead store present in the original (0x168dd4) */
      } else {
        csmemcpy(locked_vertices, vertices, (size_t)buffer_size);
        *(int *)((char *)vertex_buffer + 0x4) = vertex_count;
        *(int *)((char *)vertex_buffer + 0x8) = 0;
        *(void **)((char *)vertex_buffer + 0xc) = vertices;
        *(short *)vertex_buffer = (short)vertex_type;
        *(void **)((char *)vertex_buffer + 0x10) = d3d_vertex_buffer;
        return true;
      }
    }
  } else if (success) {
    return success;
  }

  csmemset(vertex_buffer, 0, 0x14);
  error(2, "### ERROR failed to create vertex buffer hardware format");
  return success;
}

/*
 * rasterizer_vertex_buffer_delete @ 0x168e40 — releases the record's
 * IDirect3DVertexBuffer8 at +0x10 and clears the slot. Proven against the
 * pristine XBE bytes for 0x168e40-0x168e62 (mov eax,[esi+0x10]; push eax;
 * call D3DResource_Release; mov [esi+0x10],0).
 */
/* 0x168e40 */
void rasterizer_vertex_buffer_delete(void *vertex_buffer)
{
  if (vertex_buffer != 0 && *(void **)((char *)vertex_buffer + 0x10) != 0) {
    D3DResource_Release(*(void **)((char *)vertex_buffer + 0x10));
    *(void **)((char *)vertex_buffer + 0x10) = 0;
  }
}

/*
 * rasterizer_triangle_buffer_new @ 0x168e70 — creates a static D3D index
 * buffer for a rasterizer_triangle_buffer record, uploads the caller's
 * indices and fills the record in.
 *
 * Frame (cdecl, 4 stack args, bool in AL):
 *   [EBP+0x08] triangle_buffer  record to fill (0x10 bytes)
 *   [EBP+0x0c] type             word; 0 = triangle list, 1 = triangle strip
 *   [EBP+0x10] count            asserted > 0
 *   [EBP+0x14] triangles        source indices, asserted non-NULL
 * The original reuses the [EBP+0x08] slot as the CreateIndexBuffer out
 * pointer (0x168f48 LEA EAX,[EBP+8]) after copying the record pointer into
 * ESI at 0x168e75; a plain local is behaviourally identical.
 *
 * Buffer size (0x168ee4-0x168f1b): type 0 -> count*6 bytes (3 16-bit indices
 * per triangle), type 1 -> count*2+4 bytes (strip), anything else asserts.
 *
 * Record layout proven by the stores at 0x168fab-0x168fb8 and the 0x10-byte
 * csmemset at 0x168f28:
 *   +0x00 word  triangle type    +0x02 word  (never accessed)
 *   +0x04 dword triangle count   +0x08 dword source triangles
 *   +0x0c dword IDirect3DIndexBuffer8 *
 *
 * 0x476ab0 is global_d3d_device. The locked index data is read from the
 * index buffer at +0x04 (0x168f8f), not through a Lock call.
 */
/* 0x168e70 */
bool rasterizer_triangle_buffer_new(void *triangle_buffer, short type,
                                    int count, void *triangles)
{
  void *d3d_index_buffer;
  void *index_data;
  int buffer_size;
  bool success;
  int hr;

  buffer_size = 0;

  if (triangle_buffer == 0) {
    display_assert("triangle_buffer",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                   "hardware_geometry.c",
                   0x73, 1);
    system_exit(-1);
  }
  if (triangles == 0) {
    display_assert("triangles",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                   "hardware_geometry.c",
                   0x74, 1);
    system_exit(-1);
  }
  if (count <= 0) {
    display_assert("count>0",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                   "hardware_geometry.c",
                   0x75, 1);
    system_exit(-1);
  }

  switch (type) {
  case 0:
    buffer_size = count * 6;
    break;
  case 1:
    buffer_size = count * 2 + 4;
    break;
  default:
    display_assert("### ERROR unsupported triangle buffer type",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                   "hardware_geometry.c",
                   0x80, 1);
    system_exit(-1);
    break;
  }

  if (*(void **)0x476ab0 != 0) {
    hr = D3DDevice_CreateIndexBuffer((uint32_t)buffer_size, 8, 0x65, 1,
                                     &d3d_index_buffer);
    if (hr >= 0) {
      success = true;
    } else {
      success = false;
      rasterizer_error(
        hr, "IDirect3DDevice8_CreateIndexBuffer(global_d3d_device, buffer_size,"
            " RASTERIZER_STATIC_BUFFER_USAGE, D3DFMT_INDEX16,"
            " RASTERIZER_STATIC_BUFFER_POOL, &d3d_index_buffer)");
    }
    if (d3d_index_buffer == 0) {
      success = false;
    }
    if (!success) {
      d3d_index_buffer = 0; /* store present in the original (0x168f79/0x168f86) */
    } else {
      index_data = *(void **)((char *)d3d_index_buffer + 0x4);
      if (index_data == 0) {
        success = false;
      } else {
        csmemcpy(index_data, triangles, (size_t)buffer_size);
        *(short *)triangle_buffer = type;
        *(void **)((char *)triangle_buffer + 0x8) = triangles;
        *(int *)((char *)triangle_buffer + 0x4) = count;
        *(void **)((char *)triangle_buffer + 0xc) = d3d_index_buffer;
        return true;
      }
    }
  } else {
    success = false;
  }

  csmemset(triangle_buffer, 0, 0x10);
  error(2, "### ERROR failed to create triangle buffer hardware format");
  return success;
}

/*
 * rasterizer_triangle_buffer_delete @ 0x168fd0 — releases the index buffer at +0xc of
 * the caller's record and clears the slot. Confirmed against the pristine
 * XBE bytes for 0x168fd0-0x168ff2 (mov eax,[esi+0xc]; push eax; call
 * D3DResource_Release; mov [esi+0xc],0) -- the same release+null idiom as
 * rasterizer_vertex_buffer_delete (0x168e40) above, but on offset +0xc
 * instead of +0x10, which matches the IDirect3DIndexBuffer8 * slot at +0xc
 * of the rasterizer_triangle_buffer record proven in
 * rasterizer_triangle_buffer_new's header comment.
 *
 * Frame (cdecl, 1 stack arg, void return):
 *   [EBP+0x08] buffer   record whose +0xc resource pointer is released
 *
 * Naming: kb.json previously carried the placeholder decl
 * `void _rasterizer_dynamic_unlit_geometry_draw(void);` with no
 * name-confidence metadata (unlike the three T1-confirmed names above in
 * this file), and the body cannot execute any drawing, so that name is
 * contradicted by the evidence. The recovered name rests on three facts:
 * the body is instruction-for-instruction the same shape as
 * rasterizer_vertex_buffer_delete, both call the same target
 * (D3DResource_Release at 0x1ED930), and the released slot +0xc is the
 * IDirect3DIndexBuffer8 * of the rasterizer_triangle_buffer record. It is
 * the one missing member of the new/delete pair set in this object. No
 * callers exist anywhere in the image (Ghidra call graph and a full-image
 * byte-pattern search both came back empty), so this is behavioural
 * evidence rather than a symbol-dump confirmation.
 */
/* 0x168fd0 */
void rasterizer_triangle_buffer_delete(void *triangle_buffer)
{
  if (triangle_buffer != 0 && *(void **)((char *)triangle_buffer + 0xc) != 0) {
    D3DResource_Release(*(void **)((char *)triangle_buffer + 0xc));
    *(void **)((char *)triangle_buffer + 0xc) = 0;
  }
}

/*
 * rasterizer_project_billboard @ 0x169200 — project a world-space point plus a radius into
 * screen space, returning whether it landed in front of the eye.
 *
 * This function is byte-identical to the already-ported rasterizer_widget_project_billboard
 * (src/halo/rasterizer/rasterizer.c) except for the CALL rel32 displacement
 * to matrix_transform_point, which necessarily differs because the two
 * functions live at different addresses but call the same absolute target
 * (0x109590) -- confirmed with a direct byte diff of the two 444-byte
 * bodies against the pristine XBE (tools/verify/xbe_reference.py): the only
 * differing bytes are the 3 low bytes of that one rel32 operand. This is
 * the same "MSVC emitted the inline/shared body once per translation unit"
 * duplication already documented for IDirect3DDevice8_SetVertexData2f in rasterizer.c. The body
 * below is copied from rasterizer_widget_project_billboard's proven lift; see that function's
 * header comment for the full evidence trail (viewport rect field order,
 * projection matrix row/col layout, FCOM/TEST AH,0x41 guard semantics, and
 * the row-2-first x87 evaluation order).
 *
 * out_screen arrives in EBX (first store to it is FSTP float ptr [EBX], no
 * prior write from any parameter slot), so it stays annotated @<ebx> in
 * kb.json exactly as it already was before this lift; that annotation is
 * unchanged here. Ghidra's `void rasterizer_project_billboard(void)` misses that register
 * argument and the AL return (MOV AL,0x1 vs XOR AL,AL vs MOV AL,CL).
 *
 * Globals confirmed by direct memory read against the pristine XBE:
 *   0x2533c0 = 0.0f, 0x2533c8 = 1.0f, 0x253398 = 0.5f
 * 0x5a5bf4/0x5a5bf8/0x5a5bfa/0x5a5bf6 (viewport rect) and
 * 0x5a5d60..0x5a5d9c (projection matrix rows) and 0x5a5c2c
 * (world-to-view matrix) are the same globals rasterizer_widget_project_billboard uses, proven by
 * the identical operand addresses in both functions' disassembly.
 */
/* 0x169200 */
bool rasterizer_project_billboard(float *point, float radius, float *out_extent,
                  float *out_screen)
{
  float view_point[3]; /* [EBP-0x1c..-0x14] matrix_transform_point output */
  float radius_y;      /* [EBP-0x10] */
  float radius_x;      /* [EBP-0xc]  */
  float proj_z;         /* [EBP-8]  FST (not FSTP) -- stays live in ST0 */
  float proj_y;         /* [EBP-4]  */
  float inv_w;
  float depth;
  short viewport_width;
  int viewport_extent; /* packed (bottom,right)-(top,left); low word = height */
  bool visible;

  visible = 0;
  if (radius > 0.0f) {
    viewport_width = *(short *)0x5a5bfa - *(short *)0x5a5bf6;
    viewport_extent = *(int *)0x5a5bf8 - *(int *)0x5a5bf4;

    matrix_transform_point((float *)0x5a5c2c, point, view_point);

    proj_y = *(float *)0x5a5d64 * view_point[0] +
             *(float *)0x5a5d74 * view_point[1] +
             *(float *)0x5a5d84 * view_point[2] + *(float *)0x5a5d94;
    proj_z = *(float *)0x5a5d68 * view_point[0] +
             *(float *)0x5a5d78 * view_point[1] +
             *(float *)0x5a5d88 * view_point[2] + *(float *)0x5a5d98;
    radius_x = *(float *)0x5a5d60 * radius;
    radius_y = *(float *)0x5a5d74 * radius;

    if (proj_z > 0.0f) {
      inv_w = 1.0f / (*(float *)0x5a5d6c * view_point[0] +
                      *(float *)0x5a5d7c * view_point[1] +
                      *(float *)0x5a5d8c * view_point[2] +
                      *(float *)0x5a5d9c);

      out_screen[0] =
        (((*(float *)0x5a5d60 * view_point[0] +
           *(float *)0x5a5d70 * view_point[1] +
           *(float *)0x5a5d80 * view_point[2] + *(float *)0x5a5d90) *
            inv_w +
          1.0f) *
           (float)viewport_width -
         1.0f) *
        0.5f;
      out_screen[1] =
        ((1.0f - inv_w * proj_y) * (float)(short)viewport_extent - 1.0f) * 0.5f;

      /* FLD 1.0f ; FCOMP -- the constant is the left operand, so the clamp is
       * written 1.0f <= depth rather than depth >= 1.0f. */
      depth = inv_w * proj_z;
      if (!(1.0f > depth)) {
        depth = 1.0f;
      }
      out_screen[2] = depth;

      out_extent[0] = (float)viewport_width * inv_w * radius_x * 0.5f;
      out_extent[1] = (float)(short)viewport_extent * inv_w * radius_y * 0.5f;
      visible = 1;
    }
  }
  return visible;
}
