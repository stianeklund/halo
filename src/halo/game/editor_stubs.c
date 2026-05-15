// 0x977e0
void FUN_000977e0(void)
{
}

// 0x977f0
bool game_in_editor(void)
{
  return false;
}

// 0x97800
bool FUN_00097800(void)
{
  return 1;
}

// 0x97810
bool editor_should_exit(void)
{
  return false;
}

// 0x97820
void editor_initialize(void)
{
}

// 0x97830
void editor_dispose(void)
{
}

// 0x97840
void editor_update(void)
{
}

// 0x97850
void editor_initialize_for_new_map(void)
{
}

// 0x97860
void editor_dispose_from_old_map(void)
{
}

// 0x97870
float FUN_00097870(float param_1, float param_2, uint32_t param_3,
                   uint8_t param_4)
{
  if (param_3 & (1u << param_4)) {
    return param_1 * param_2;
  }
  return param_2;
}
