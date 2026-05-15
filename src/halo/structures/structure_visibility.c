void debug_pvs(uint8_t param_1)
{
  *(uint8_t *)0x505702 = param_1;
  *(uint8_t *)0x505701 = param_1;
}

void structure_visibility_find_camera(void *param_1)
{
  char *scenario;
  uint32_t leaf;
  char *cluster_elem;
  char *fog_elem;

  scenario = (char *)scenario_get();
  leaf = bsp3d_find_leaf(tag_block_get_element(scenario + 0xb0, 0, 0x60), 0,
                         param_1);

  if (leaf == 0xffffffff) {
    leaf = *(uint32_t *)0x506780;
    if ((int)leaf < *(int *)(scenario + 0xe0))
      goto do_cluster;
    leaf = 0xffffffff;
  }

  *(uint32_t *)0x506780 = leaf;
  *(int *)0x506784 = -1;
  *(short *)0x50678a = -1;
  *(uint8_t *)0x506789 = 0;
  if (leaf == 0xffffffff)
    return;

do_cluster:
  cluster_elem = (char *)tag_block_get_element(scenario + 0xe0,
                                               (int)(leaf & 0x7fffffff), 0x10);
  *(int *)0x506784 = (int)*(short *)(cluster_elem + 8);
  fog_elem =
    (char *)tag_block_get_element(scenario + 0x134, *(int *)0x506784, 0x68);
  *(short *)0x50678a = *(short *)fog_elem;
  fog_elem = (char *)FUN_0018e7d0((int)*(short *)0x50678a);
  if (fog_elem != 0 && *(int *)(fog_elem + 0xc) != -1)
    *(uint8_t *)0x506789 = 1;
}

/* FUN_001966b0: scenario visibility cluster sweep.
 *   For each rendered cluster, walk its frustum-visible portals
 *   and mark referenced bitfield entries until a cap (0x4000) is hit.
 *   param_1 = scenario pointer (tag block base at +0x134 = clusters table). */
void FUN_001966b0(int param_1)
{
  short *local_8;
  int local_10;
  int sVar6;
  char *iVar3;
  int iVar4;
  int *piVar1;
  int *piVar5;
  int sVar2;
  int iVar_bit;
  unsigned int uVar7;

  if (*(char *)0x449ef1 != '\0' && *(char *)0x32c368 != '\0') {
    profile_enter_private((void *)0x32c360);
  }
  local_10 = 0;
  if (0 < *(short *)0x5137cc) {
    do {
      if (*(short *)0x5937d0 >= 0x4000) break;
      local_8 = (short *)rendered_cluster_get(local_10);
      iVar3 = (char *)tag_block_get_element((char *)param_1 + 0x134,
                                            (int)*local_8, 0x68);
      if (*(unsigned char *)0x505701 == 0 && *(int *)0x506784 != -1) {
        local_8 = local_8 + 10;
      } else {
        local_8 = (short *)0x5065a4;
      }
      iVar3 = iVar3 + 0x34;
      sVar6 = 0;
      if (0 < *(int *)iVar3) {
        do {
          if (*(short *)0x5937d0 >= 0x4000) break;
          iVar4 = (int)tag_block_get_element(iVar3, sVar6, 0x24);
          if (render_frustum_cube_visible(local_8, iVar4, 0) != 0) {
            piVar1 = (int *)(iVar4 + 0x18);
            piVar5 = (int *)tag_block_get_element(piVar1, 0, 4);
            sVar2 = 0;
            if (0 < *piVar1) {
              do {
                iVar_bit = *piVar5 >> 5;
                uVar7 = 1u << (*piVar5 & 0x1f);
                if ((uVar7 & *(unsigned int *)((char *)0x5137d0 + iVar_bit * 4)) == 0) {
                  if (*(short *)0x5937d0 >= 0x4000) break;
                  *(unsigned int *)((char *)0x5137d0 + iVar_bit * 4) |= uVar7;
                  *(short *)0x5937d0 = *(short *)0x5937d0 + 1;
                }
                piVar5 = piVar5 + 1;
                sVar2 = sVar2 + 1;
              } while ((short)sVar2 < *piVar1);
            }
          }
          sVar6 = sVar6 + 1;
        } while ((short)sVar6 < *(int *)iVar3);
      }
      local_10 = local_10 + 1;
    } while ((short)local_10 < *(short *)0x5137cc);
  }
  if (*(char *)0x449ef1 != '\0' && *(char *)0x32c368 != '\0') {
    profile_exit_private((void *)0x32c360);
  }
}
