void FUN_001965d0(uint8_t param_1)
{
  *(uint8_t *)0x505702 = param_1;
  *(uint8_t *)0x505701 = param_1;
}

void FUN_001965f0(void *param_1)
{
  char *scenario;
  uint32_t leaf;
  char *cluster_elem;
  char *fog_elem;

  scenario = (char *)scenario_get();
  leaf = bsp3d_find_leaf(tag_block_get_element(scenario + 0xb0, 0, 0x60), 0, param_1);

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
  fog_elem = (char *)tag_block_get_element(scenario + 0x134,
    *(int *)0x506784, 0x68);
  *(short *)0x50678a = *(short *)fog_elem;
  fog_elem = (char *)FUN_0018e7d0((int)*(short *)0x50678a);
  if (fog_elem != 0 && *(int *)(fog_elem + 0xc) != -1)
    *(uint8_t *)0x506789 = 1;
}
