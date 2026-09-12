# Halo CE Dynamic Shadow & Frustum Culling Technical Report

## 1. Overview & Architecture: How Dynamic Shadows Work

Dynamic shadows in Halo CE (Xbox debug build 2276) are generated per-frame using a multi-stage hybrid decal and stencil volume projection pipeline:

```
[Camera Frustum Sweep]
  ├── render_camera_build_frustum (0x187250)
  ├── FUN_00196850 (0x196850) ──> writes visible surfaces to bitset 0x5137d0
  └── FUN_0018b080 (0x18b080) ──> gathers camera-visible objects into 0x4d82d4 via FUN_00196c90
                                        │
[Shadow Pass Entry]                     │
  scenario_test_pas (0x18c460)          │
    └── FUN_0018c370 (0x18c370) ────────┘ (iterates ONLY over 0x4d82d4)
          └── FUN_0018c100 (0x18c100) [Per-Object Gating]
                ├── FUN_0018b130 / render_frustum_sphere_diameter_in_pixels (0x185a70)
                │     └── Screen diameter > 0x253394 cutoff
                ├── Ambient darkness & active camouflage fade calculation
                ├── FUN_0018b830 (0x18b830) ──> 4x3 shadow basis matrix & FUN_0017ccb0
                └── FUN_0018b990 (0x18b990) [Builds 6-Plane Oriented Shadow Box]
                      └── FUN_00196190 (0x196190, render_structure_shadows)
                            └── FUN_00197e90 (0x197e90)
                                  └── FUN_00196fd0 (0x196fd0)
                                        ├── FUN_00196a60 (0x196a60) AABB cull
                                        ├── FUN_00196b10 (0x196b10) 6-plane clip
                                        └── Surface MUST be in 0x5137d0 bitset!
                                              └── Draw Decal via FUN_0017ccf0
```

### Key Stages
1. **Camera Visibility Sweep**:
   - The engine builds the primary camera frustum.
   - It performs a BSP surface sweep ([`FUN_00196850`](file:///data/data/com.termux/files/home/halo/src/halo/structures/structure_visibility.c#L141)), marking surfaces visible to the camera in a bitvector at `0x5137d0`.
   - It gathers all objects visible to the camera into the visible-object table `0x4d82d4` ([`FUN_0018b080`](file:///data/data/com.termux/files/home/halo/src/halo/scenario/scenario.c#L108) $\rightarrow$ `FUN_00196c90`).
2. **Shadow Pass Dispatch**:
   - `scenario_test_pas` (`0x18c460`) drives the pass by looping over `0x4d82d4` with `FUN_0018c370`.
   - Each caster is evaluated in `FUN_0018c100`. If its screen-space diameter drops below threshold or if ambient lighting is too bright, the shadow is discarded.
3. **Shadow Volume Extrusion**:
   - `FUN_0018b830` derives a 4x3 basis matrix using a perpendicular vector to the light direction.
   - `FUN_0018b990` builds a 6-plane oriented bounding box around the caster along the light direction (extending $0.5r$ in front and $4.0r$ behind).
4. **Surface Gathering & Filtering**:
   - `render_structure_shadows` (`0x196190`) and `FUN_00197e90` flood adjacent clusters and filter candidate structure surfaces using `FUN_00196fd0`.
   - Candidate geometry is tested against the 6 shadow planes (`FUN_00196b10`) and AABB bounds (`FUN_00196a60`).
   - Crucially, surfaces must also be flagged in `0x5137d0` (the camera visible surface set).
5. **GPU Rasterization**:
   - Vertex shader constants receive the shadow projection transform at `-0x44` (`FUN_00172a30`).
   - The GPU draws projected decal batches (`FUN_00172de0`) and blurs the accumulation buffer via a 4-tap full-screen quad (`FUN_00172730`).

---

## 2. Root Cause Analysis: View-Angle (Right Thumbstick) Shadow Popping

When adjusting view angle with the right thumbstick, shadows abruptly appear or disappear due to four hard frustum dependencies:

1. **Caster Object Frustum Culling (`0x18b080` & `0x18c370`)**:
   `FUN_0018c370` iterates strictly over `0x4d82d4` (the table of objects passing the camera frustum check). If turning the camera puts the caster (e.g. Master Chief or a vehicle) off-screen, it is culled from `0x4d82d4`. The shadow immediately disappears even if the shadow itself would have fallen in front of the player.
2. **Receiver Surface PVS Bitvector Culling (`0x196850` & `0x196fd0`)**:
   `FUN_00196fd0` rejects any structure surface whose bit in `0x5137d0` is not set. If turning the camera causes `FUN_00196850` to clip or drop the receiving ground polygons from the camera PVS, no shadow can be projected onto them.
3. **Screen-Space Diameter Cutoff (`0x18c100` $\rightarrow$ `0x185a70`)**:
   `FUN_0018c100` computes the caster's pixel diameter using `render_frustum_sphere_diameter_in_pixels`. Looking at glancing angles can push the object center close to the camera plane or drop the projected diameter below `0x253394`, causing an instant fade to zero.
4. **Shadow Volume Clipping Planes (`0x18b990` $\rightarrow$ `0x196b10`)**:
   The 6 clip planes generated along the light direction clip against candidate world clusters. If the cluster sub-element's 8 corners fail any plane test in `FUN_00196b10`, the entire sub-element is discarded.

---

## 3. Master Function Inventory (Named & Unnamed)

| Address | Symbol Name / Identifier | Object File | Port Status | Role in Shadow / Frustum Pipeline |
|---|---|---|---|---|
| `0x187250` | `render_camera_build_frustum` | `render_cameras.obj` | Ported | Constructs camera view/proj matrices and frustum planes from yaw/pitch. |
| `0x185a70` | `render_frustum_sphere_diameter_in_pixels` | `render_cameras.obj` | Unported | Calculates screen-space pixel diameter of an object bounding sphere. |
| `0x186ac0` | `render_frustum_sphere_visible` | `render_cameras.obj` | Unported | Tests bounding sphere against camera frustum AABB and 6 planes. |
| `0x186790` | `render_frustum_triangle_visible` | `render_cameras.obj` | Unported | Tests triangle vertices against cluster camera frustum planes. |
| `0x196850` | `FUN_00196850` | `structure_visibility.obj` | Ported | Camera surface sweep; sets visible surface bits in `0x5137d0`. |
| `0x196c90` | `FUN_00196c90` | `structure_visibility.obj` | Unported | Loops over rendered clusters; fills `0x4d82d4` via `render_frustum_sphere_visible`. |
| `0x18b080` | `FUN_0018b080` | `scenario.obj` | Ported | Driver that invokes `FUN_00196c90` to rebuild `0x4d82d4`. |
| `0x18c460` | `scenario_test_pas` | `scenario.obj` | Ported | Master shadow pass entry point. |
| `0x18c370` | `FUN_0018c370` | `scenario.obj` | Ported | Iterates over `0x4d82d4` visible objects and invokes `FUN_0018c100`. |
| `0x18c100` | `FUN_0018c100` | `scenario.obj` | Ported | Per-object shadow processor; gates on diameter (`0x185a70`) and darkness. |
| `0x18b130` | `FUN_0018b130` | `scenario.obj` | Ported | Fetches object sphere and calls `render_frustum_sphere_diameter_in_pixels`. |
| `0x18b830` | `FUN_0018b830` | `scenario.obj` | Ported | Builds 4x3 shadow basis matrix and calls `FUN_0017ccb0`. |
| `0x18b990` | `FUN_0018b990` | `scenario.obj` | Ported | Builds 6-plane oriented-box shadow volume and AABB scalars. |
| `0x196190` | `FUN_00196190` (`render_structure_shadows`) | `structures.obj` | Ported | Allocates 16 KB scratch buffer and drives structure shadow queries. |
| `0x197e90` | `FUN_00197e90` | `structures.obj` | Ported | Cluster query dispatcher; delegates to `FUN_00196fd0` or BSP walk. |
| `0x196fd0` | `FUN_00196fd0` | `structures.obj` | Ported | Collects cluster surfaces; checks against `0x5137d0`, `0x196a60`, and `0x196b10`. |
| `0x196a60` | `FUN_00196a60` | `structure_visibility.obj` | Unported | AABB vs AABB overlap test for shadow volume bounding box. |
| `0x196b10` | `FUN_00196b10` | `structure_visibility.obj` | Unported | 8-corner AABB vs 6 shadow planes intersection/cull test. |
| `0x172a30` | `FUN_00172a30` | `rasterizer.obj` | Ported | Computes shadow projection matrix and uploads to VS constant `-0x44`. |
| `0x17ccb0` | `FUN_0017ccb0` | `decals.obj` | Ported | Thunk forwarding directly to `FUN_00172a30`. |
| `0x17ccf0` | `FUN_0017ccf0` | `decals.obj` | Ported | Surface-draw callback passed into `FUN_00195790` by `FUN_00196190`. |
| `0x17cd00` | `FUN_0017cd00` | `decals.obj` | Unported | Shadow profile and render state flush thunk. |
| `0x172590` | `FUN_00172590` | `rasterizer_xbox_shadows.obj` | Ported | Sets shadow model skinning parameters. |
| `0x172730` | `FUN_00172730` | `rasterizer_xbox_shadows.obj` | Ported | Fullscreen quad 4-tap blur pass for shadow accumulation buffer. |
| `0x172de0` | `FUN_00172de0` | `rasterizer_xbox_shadows.obj` | Ported | Emits shadow decal projection draw call (`D3DCULL_CCW` / `NONE`). |
| `0x173090` | `FUN_00173090` | `rasterizer_xbox_shadows.obj` | Ported | Stencil shadow batch draw. |

---

## 4. Previously Named / Labeled Functions Analyzed

The following functions already carried authoritative symbols in `kb.json` and engine headers:

1. **`0x187250` — `render_camera_build_frustum`**: Computes view matrix, projection matrix, and frustum clipping planes from camera orientation.
2. **`0x185a70` — `render_frustum_sphere_diameter_in_pixels`**: Derives screen-space pixel diameter: $2r \times \frac{\text{focal\_length}}{\text{depth}}$.
3. **`0x186ac0` — `render_frustum_sphere_visible`**: Tests an object sphere against the frustum AABB and 6 clipping planes.
4. **`0x186790` — `render_frustum_triangle_visible`**: Bitmask frustum test verifying whether a 3-vertex triangle is visible.
5. **`0x18c460` — `scenario_test_pas`**: Top-level function orchestrating the potentially audible / affected set shadow rendering pass.
6. **`0x18c3a0` — `scenario_test_pvs`**: Top-level function orchestrating the potentially visible set object render pass.
7. **`0x109e10` — `matrix_from_forward_and_up`**: Constructs a 4x3 basis matrix with cross-product generation for the left vector.
8. **`0x10a110` — `matrix4x3_from_forward_up_position`**: Combines `matrix_from_forward_and_up` with translation position.

---

## 5. Decompiled Functions (Generated with Kuna)

The following functions were decompiled directly from `Halo (2276, Oct 12 2001)/cachebeta.xbe` using `kuna`:

### 5.1 `0x196a60` — `FUN_00196a60` (Shadow AABB Overlap Cull)
```c
/* ecx: a0 (shadow AABB: [xmin, xmax, ymin, ymax, zmin, zmax])
 * edx: a1 (cluster sub-element AABB) */
unsigned int sub_196a60(float *a0, float *a1)
{
  if (*a1 <= a0[1]) {
    if ((*a0 <= a1[1]) && (a1[2] <= a0[3])) {
      if ((a0[2] <= a1[3]) && (a1[4] <= a0[5])) {
        if (a0[4] <= a1[5]) {
          if ((((*a0 <= *a1) && (a1[1] <= a0[1])) && (a0[2] <= a1[2])) &&
              (((a1[3] <= a0[3] && (a0[4] <= a1[4])) && (a1[5] <= a0[5]))))
            return 2; /* Completely enclosed */
          return 1;   /* Intersects */
        }
      }
    }
  }
  return 0; /* Culled / no overlap */
}
```

### 5.2 `0x196b10` — `FUN_00196b10` (Shadow 6-Plane Volume Frustum Test)
```c
/* eax: bounds (6 floats: [xmin, xmax, ymin, ymax, zmin, zmax])
 * a0: plane_count (e.g. 6)
 * a1: planes (array of 4-float planes {nx, ny, nz, d}) */
unsigned int sub_196b10(short plane_count, int planes_ptr)
{
  float local_bounds[6];
  float *plane;
  float d;
  float z0, z1, y0, y1, x0, x1;
  unsigned char plane_mask;
  unsigned char any_outside = 0;
  short i;

  for (i = 0; i < 6; i++) {
    local_bounds[i] = ((float *)bounds)[i];
  }

  if (plane_count <= 0)
    return 2;

  for (i = 0; i < plane_count; i++) {
    plane = (float *)(i * 0x10 + planes_ptr);
    d = plane[3];
    plane_mask = 0;

    x0 = local_bounds[0] * plane[0];
    x1 = local_bounds[1] * plane[0];
    y0 = local_bounds[2] * plane[1];
    y1 = local_bounds[3] * plane[1];
    z0 = local_bounds[4] * plane[2];
    z1 = local_bounds[5] * plane[2];

    if ((x0 + y0 + z0) - d < 0.0f) plane_mask |= 0x01;
    if ((x1 + y0 + z0) - d < 0.0f) plane_mask |= 0x02;
    if ((x0 + y1 + z0) - d < 0.0f) plane_mask |= 0x04;
    if ((x1 + y1 + z0) - d < 0.0f) plane_mask |= 0x08;
    if ((x0 + y0 + z1) - d < 0.0f) plane_mask |= 0x10;
    if ((x1 + y0 + z1) - d < 0.0f) plane_mask |= 0x20;
    if ((x0 + y1 + z1) - d < 0.0f) plane_mask |= 0x40;
    if ((x1 + y1 + z1) - d < 0.0f) plane_mask |= 0x80;

    if (plane_mask == 0xff)
      return 0; /* All 8 corners outside this plane -> culled */

    any_outside |= plane_mask;
  }

  return (any_outside == 0) ? 2 : 1;
}
```

### 5.3 `0x196c90` — `FUN_00196c90` (Camera Frustum Object Culling Loop)
```c
short FUN_00196c90(int out_handles, short max_count,
                   void *iter_first, void *iter_next,
                   void *get_bounds, void *needs_update, void *mark)
{
  int obj_handle;
  short count = 0;
  int cluster_i = 0;
  float center[3];
  float radius;
  unsigned short *cluster;

  sound_rendered_clusters_init();

  if (1 <= rendered_cluster_count) {
    do {
      cluster = (unsigned short *)rendered_cluster_get(cluster_i);
      obj_handle = ((int (*)(char *, int))iter_first)(scratch, *cluster);

      while (obj_handle != -1) {
        if (((char (*)(int))needs_update)(obj_handle)) {
          ((void (*)(int, float *, float *))get_bounds)(obj_handle, center, &radius);
          if (count < max_count) {
            if (visible_cluster_cache == -1 ||
                render_frustum_sphere_visible((void *)(cluster + 10), center, radius)) {
              *(int *)(out_handles + count * 4) = obj_handle;
              count++;
              ((void (*)(int))mark)(obj_handle);
            }
          }
        }
        obj_handle = ((int (*)(char *))iter_next)(scratch);
      }
      cluster_i++;
    } while (cluster_i < rendered_cluster_count);
  }

  return count;
}
```

### 5.4 `0x185a70` — `render_frustum_sphere_diameter_in_pixels`
```c
float render_frustum_sphere_diameter_in_pixels(void *frustum, float *center, float radius)
{
  float depth;

  depth = *(float *)((char *)frustum + 0x1c) * center[0] +
          *(float *)((char *)frustum + 0x28) * center[1] +
          *(float *)((char *)frustum + 0x34) * center[2] +
          *(float *)((char *)frustum + 0x40);

  if (depth < 0.0f)
    depth = -depth;
  if (depth <= 0.001f)
    depth = 0.001f;

  return ((*(float *)((char *)frustum + 0x188) / depth) * radius) * 2.0f;
}
```

### 5.5 `0x186ac0` — `render_frustum_sphere_visible`
```c
int render_frustum_sphere_visible(void *frustum, float *center, float radius)
{
  float dist;
  int i;

  /* AABB bounding box check */
  if (center[0] - radius > *(float *)((char *)frustum + 0x12c) ||
      center[1] - radius > *(float *)((char *)frustum + 0x134) ||
      center[2] - radius > *(float *)((char *)frustum + 0x13c) ||
      *(float *)((char *)frustum + 0x128) > center[0] + radius ||
      *(float *)((char *)frustum + 0x130) > center[1] + radius ||
      *(float *)((char *)frustum + 0x138) > center[2] + radius) {
    return 0;
  }

  /* 6 plane tests */
  for (i = 0; i < 6; i++) {
    dist = plane_distance_to_point((float *)((char *)frustum + 0x78 + i * 0x10), center);
    if (dist > radius)
      return 0; /* Outside this plane */
  }

  return 1;
}
```

### 5.6 `0x186790` — `render_frustum_triangle_visible`
```c
bool render_frustum_triangle_visible(void *plane_ctx, void *v0, void *v1, void *v2)
{
  unsigned short f0, f1, f2;

  f0 = render_frustum_build_point_flags(plane_ctx, v0);
  if (f0 == 0)
    return true;

  f1 = render_frustum_build_point_flags(plane_ctx, v1);
  if (f1 != 0) {
    f0 = (f0 & 0x3f) & f1;
    f2 = render_frustum_build_point_flags(plane_ctx, v2);
    if (f2 != 0) {
      if ((f0 & f2) != 0)
        return false; /* All 3 vertices outside the same plane */
    }
  }

  return true;
}
```

### 5.7 `0x17cd00` — `FUN_0017cd00` (Decals/Shadow State Flush)
```c
void FUN_0017cd00(void)
{
  if (*(void **)0x476ab0 == NULL) {
    display_assert("global_d3d_device", "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0x233, 1);
    system_exit(-1);
  }
  if (*(short *)0x5a5bc0 == 0 && *(char *)0x3256ca != 0) {
    if (*(char *)0x47e4b5 == 0)
      rasterizer_warning(2, "shadow parameters not active");
    if (*(char *)0x3251fc == 0) {
      D3DDevice_SetRenderState_Simple(*(int *)0x5a5bc0, 0, 0, 0, 1);
      *(char *)0x3251fc = 1;
    }
  }
}
```
