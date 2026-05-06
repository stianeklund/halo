/* 0x14ad40 — Zero-initialize a 6-byte collision features header */
void collision_features_init(void *features)
{
  csmemset(features, 0, 6);
}
