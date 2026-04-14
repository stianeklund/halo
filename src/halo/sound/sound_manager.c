/* Sound manager — low-level sound system lifecycle and rendering. */

/* Empty on Xbox — no per-map sound initialization needed. */
void sound_initialize_for_new_map(void)
{
}

/* sound_dispose_from_old_map and sound_render removed from kb.json —
 * both are complex and require careful implementation. An empty stub
 * for dispose_from_old_map caused xbox_sound_cache.c assert on map load
 * because sound cleanup was skipped. */
