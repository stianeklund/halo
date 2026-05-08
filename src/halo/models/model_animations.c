/* model_animation_choose_random (0x120f20) — Choose a weighted random
 * animation.
 *
 * Gets the animation graph tag ('antr'), generates a random float [0,1) using
 * either the global or local random seed (based on update_kind), then walks the
 * animation chain starting at animation_index. For each animation element:
 * compares the random value against the weight threshold at element+0x44.
 * If random <= threshold, returns the current animation index. Otherwise
 * advances to the next animation via element+0x38.
 *
 * Confirmed: tag_get('antr', animation_graph_tag_index) at 0x120f2e.
 * Confirmed: update_kind==1 → get_global_random_seed_address() at 0x120f40.
 * Confirmed: update_kind==0 → random_math_get_local_seed_address() at 0x120f53.
 * Confirmed: random_math_real(seed) at 0x120f46/0x120f59.
 * Confirmed: tag_block_get_element(antr_tag+0x74, index, 0xb4) at 0x120f9e.
 * Confirmed: FCOMP [ECX+0x44] + JNP loop exit at 0x120fab-0x120fb3.
 * Confirmed: next animation at element+0x38 (int16_t) at 0x120fb5.
 */
int model_animation_choose_random(int update_kind,
                                  int animation_graph_tag_index,
                                  int16_t animation_index)
{
  char *antr_tag;
  float random_value;
  char *element;

  antr_tag = (char *)tag_get(0x616e7472, animation_graph_tag_index);
  if (update_kind == 1) {
    random_value =
      random_math_real((unsigned int *)get_global_random_seed_address());
  } else {
    random_value = random_math_real(random_math_get_local_seed_address());
    if (update_kind != 0) {
      display_assert("ASSERTION_SERIES(update_kind, 2)",
                     "c:\\halo\\SOURCE\\models\\model_animations.c", 0x3f0, 1);
      system_exit(-1);
    }
  }
  while (animation_index != -1) {
    element = (char *)tag_block_get_element(antr_tag + 0x74,
                                            (int)animation_index, 0xb4);
    if (random_value <= *(float *)(element + 0x44))
      break;
    animation_index = *(int16_t *)(element + 0x38);
  }
  return (int)animation_index;
}
