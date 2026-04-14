bool game_time_initialized(void)
{
  return game_time_globals && game_time_globals->initialized;
}

void game_time_initialize(void)
{
  game_time_globals =
    game_state_malloc("game time globals", NULL, sizeof(game_time_globals_t));
  csmemset(game_time_globals, 0, sizeof(game_time_globals_t));
}

void game_time_initialize_for_new_map(void)
{
  assert_halt(game_time_globals && !game_time_globals->initialized);
  csmemset(game_time_globals, 0, sizeof(game_time_globals_t));
  game_time_globals->initialized = true;
}

void game_time_dispose_from_old_map(void)
{
  if (game_time_globals) {
    game_time_globals->initialized = false;
    game_time_globals->active = false;
  }
}

void game_time_dispose(void)
{
}

void game_time_end(void)
{
  assert_halt(game_time_globals);
  game_time_globals->active = false;
}

int game_time_get(void)
{
  assert_halt(game_time_globals && game_time_globals->initialized);
  return game_time_globals->time;
}

int16_t game_time_get_elapsed(void)
{
  assert_halt(game_time_globals && game_time_globals->initialized);
  return game_time_globals->elapsed;
}

int local_time_get(void)
{
  assert_halt(game_time_globals && game_time_globals->initialized);
  return game_time_globals->time;
}

int16_t local_time_get_elapsed(void)
{
  assert_halt(game_time_globals && game_time_globals->initialized);
  return game_time_globals->elapsed;
}

bool game_predicting(void)
{
  assert_halt(game_time_globals && game_time_globals->initialized);
  return false;
}

bool game_in_progress(void)
{
  assert_halt(game_time_globals);
  if (game_time_globals->initialized) {
    if (game_time_globals->active) {
      return true;
    }
    if (game_time_globals->paused) {
      return true;
    } else {
      return false;
    }
  } else {
    return false;
  }
}

bool game_time_get_paused(void)
{
  assert_halt(game_time_globals);
  return game_time_globals->paused;
}

void game_time_set_paused(bool paused)
{
  assert_halt(game_time_globals);
  if (game_time_globals->initialized) {
    game_time_globals->active = !paused;
  }
  game_time_globals->paused = paused;
}

float game_time_get_speed(void)
{
  assert_halt(game_time_globals);
  return game_time_globals->speed;
}

void game_time_set_speed(float speed)
{
  assert_halt(game_time_globals);
  game_time_globals->speed = speed;
}

void game_time_statistics_new(void)
{
  byte_457069 = 1;
  byte_457068 = 0;
}

// 0xb5d60
// Register calling convention in original (SI=a, DI=b, BX=c); both caller
// (game_time_update) and callee replaced together so C cdecl is safe here.
void game_time_statistics_frame(int16_t a, int16_t b, int16_t c)
{
  unsigned int now;
  int16_t wall_delta;
  int16_t min_a;
  int16_t new_val;
  int16_t field4;

  if (!byte_457068) {
    *(int16_t *)0x457070 = 0;
    *(int16_t *)0x457072 = 0;
    *(int16_t *)0x457074 = 0x7fff;
    *(int16_t *)0x457076 = -0x8000;
    *(int16_t *)0x457078 = 0;
    *(int16_t *)0x45707a = 0x7fff;
    *(int16_t *)0x45707c = -0x8000;
    *(int16_t *)0x45707e = 0;
    *(int16_t *)0x457080 = 0x7fff;
    *(int16_t *)0x457082 = -0x8000;
    *(int16_t *)0x457084 = 0;
    *(int16_t *)0x457086 = 0x7fff;
    *(int16_t *)0x457088 = -0x8000;
    *(unsigned int *)0x45706c = system_milliseconds();
    byte_457068 = 1;
    return;
  }
  now = system_milliseconds();
  wall_delta = (int16_t)(now - *(unsigned int *)0x45706c);
  *(unsigned int *)0x45706c = now;
  *(int16_t *)0x457070 += 1;
  *(int16_t *)0x457072 += wall_delta;
  if (*(int16_t *)0x457076 < wall_delta)
    *(int16_t *)0x457076 = wall_delta;
  if (wall_delta < *(int16_t *)0x457074)
    *(int16_t *)0x457074 = wall_delta;
  *(int16_t *)0x457078 += a;
  if (*(int16_t *)0x45707c < a)
    *(int16_t *)0x45707c = a;
  min_a = *(int16_t *)0x45707a;
  if (a < min_a) {
    min_a = a;
    *(int16_t *)0x45707a = min_a;
  }
  *(int16_t *)0x45707e += b;
  if (*(int16_t *)0x457082 < b)
    *(int16_t *)0x457082 = b;
  if (b < *(int16_t *)0x457080)
    *(int16_t *)0x457080 = b;
  *(int16_t *)0x457084 += c;
  if (*(int16_t *)0x457088 < c)
    *(int16_t *)0x457088 = c;
  if (c < *(int16_t *)0x457086)
    *(int16_t *)0x457086 = c;
  if (*(int16_t *)0x457072 < 1000 || *(int16_t *)0x457070 <= 0)
    return;
  if (min_a == 0) {
    *(int16_t *)((char *)game_time_globals + 4) = -1;
    byte_457068 = 0;
    return;
  }
  new_val = (min_a >= 0) ? 1 : 0;
  if (new_val != *(int16_t *)((char *)game_time_globals + 4)) {
    *(int16_t *)((char *)game_time_globals + 4) = new_val;
    *(int16_t *)((char *)game_time_globals + 6) = 0;
    *(int16_t *)((char *)game_time_globals + 8) = new_val ? 0x7fff : -0x8000;
    min_a = *(int16_t *)0x45707a;
  }
  field4 = *(int16_t *)((char *)game_time_globals + 4);
  if (field4 == 0) {
    if (*(int16_t *)((char *)game_time_globals + 8) < min_a)
      *(int16_t *)((char *)game_time_globals + 8) = min_a;
  } else if (field4 == 1) {
    if (min_a < *(int16_t *)((char *)game_time_globals + 8))
      *(int16_t *)((char *)game_time_globals + 8) = min_a;
  }
  *(int16_t *)((char *)game_time_globals + 6) += 1;
  if (*(int16_t *)((char *)game_time_globals + 6) == 5)
    *(int16_t *)((char *)game_time_globals + 4) = -1;
  byte_457068 = 0;
}

void game_time_start(void)
{
  assert_halt(game_time_globals && game_time_globals->initialized);
  assert_halt(!game_time_globals->active);
  assert_halt(game_time_globals);
  game_time_globals->speed = 1.0;
  game_time_globals->leftover_dt = 0;
  game_time_globals->active = 1;
  byte_457069 = 1;
  byte_457068 = 0;
  switch (game_connection()) {
  case 0:
  case 2:
    update_server_start();
    break;
  case 1:
  case 3:
    update_client_start();
    break;
  default:
    return;
  }
}

// 0xb6020
void game_time_update(float param_1)
{
  float fVar1;
  float fVar2;
  unsigned int local_8;
  unsigned int local_8_unclamped;
  unsigned int max_ticks;
  bool bVar3;
  int conn;
  int server;
  int server_min_time;
  int cur_time;
  int update_time;
  int extra_ticks;
  unsigned int time_val;
  unsigned int *end_tick_ptr;
  unsigned int target_tick;
  unsigned int remaining;

  assert_halt(game_time_globals);
  if (!game_time_globals->initialized) {
    game_time_globals->elapsed = 0;
    return;
  }
  fVar2 = game_time_globals->speed * *(float *)0x253394;
  if (fVar2 <= *(float *)0x2533c0)
    goto LAB_end;
  conn = (int)(int16_t)game_connection();
  switch (conn) {
  default:
    max_ticks = 7;
    bVar3 = true;
    break;
  case 1:
    max_ticks = 0x1e;
    bVar3 = true;
    break;
  case 2:
    server = ((int (*)(void))0x12a1d0)();
    server_min_time = (int)((unsigned int (*)(int))0x12d5b0)(server);
    cur_time = game_time_get();
    assert_halt((unsigned int)(cur_time - server_min_time) <= 0x80);
    if (cur_time == 0) {
      max_ticks = 1;
    } else {
      max_ticks = (unsigned int)(server_min_time - cur_time) + 0x80;
      if ((int)max_ticks < 0x1e) {
        if ((int)max_ticks < 1) {
          ((void (*)(int, bool))0x12e1d0)(server, true);
          bVar3 = true;
          goto LAB_after_switch;
        }
      } else {
        max_ticks = 0x1e;
      }
      ((void (*)(int, bool))0x12e1d0)(server, false);
    }
    bVar3 = true;
    break;
  case 3:
    max_ticks = 0x1e;
    bVar3 = false;
    break;
  }
LAB_after_switch:
  fVar1 = param_1 + game_time_globals->leftover_dt;
  local_8_unclamped = (unsigned int)(double)(fVar1 * fVar2);
  local_8 = local_8_unclamped;
  if ((int)max_ticks < (int)local_8 && bVar3) {
    local_8 = max_ticks;
    game_time_globals->leftover_dt = 0.0f;
  } else {
    game_time_globals->leftover_dt =
      (float)((double)fVar1 - (double)local_8_unclamped / (double)fVar2);
  }
  if (game_time_globals->leftover_dt < *(float *)0x2533c0)
    game_time_globals->leftover_dt = 0.0f;
  assert_halt(game_time_globals->leftover_dt >= *(float *)0x2533c0 &&
              game_time_globals->leftover_dt < *(float *)0x253f00);
  conn = (int)(int16_t)game_connection();
  if (conn == 1) {
    max_ticks = (unsigned int)update_get_maximum_actions();
    if ((int)local_8 > (int)max_ticks) {
      local_8 = max_ticks > 0 ? max_ticks - 1 : 0;
    } else if ((int)(local_8 + 7) < (int)max_ticks) {
      local_8 = max_ticks > 0 ? max_ticks - 1 : 0;
    } else if ((int)(local_8 + 1) < (int)max_ticks) {
      local_8 = local_8 + 1;
    }
    assert_halt((int)local_8 <= (int)max_ticks);
    if ((int)local_8 > (int)max_ticks)
      local_8 = max_ticks;
  }
  if (0 < (int)local_8) {
    end_tick_ptr = (unsigned int *)((char *)game_time_globals + 0x14);
    time_val = game_time_globals->time;
    remaining = local_8;
    while (time_val < *end_tick_ptr && remaining > 0) {
      time_val++;
      remaining--;
    }
    game_time_globals->time = time_val;
    local_8 = remaining;
    target_tick = time_val + remaining;
    conn = (int)(int16_t)game_connection();
    if (conn == 0) {
      update_client_apply_actions((int16_t)local_8);
    } else if (conn == 2) {
      server = ((int (*)(void))0x12a1d0)();
      ((void (*)(int, int16_t))0x12cdb0)(server, (int16_t)local_8);
    }
    update_time = ((int (*)(void))0xb9610)();
    if ((int)*end_tick_ptr < update_time) {
      if (update_time <= (int)target_tick)
        target_tick = (unsigned int)update_time;
      extra_ticks = (int)target_tick - (int)*end_tick_ptr;
      if (0 < extra_ticks) {
        int n = extra_ticks;
        do {
          game_tick();
          *end_tick_ptr += 1;
          game_time_globals->time++;
          n--;
        } while (n != 0);
      }
    } else {
      extra_ticks = 0;
    }
    {
      int16_t stats_a =
        (int16_t)((uint16_t)update_time - (uint16_t)game_time_globals->time);
      int16_t stats_b = (int16_t)extra_ticks;
      game_time_statistics_frame(stats_a, stats_b, 0);
    }
    game_time_globals->elapsed = (int16_t)local_8;
  }
LAB_end:
  assert_halt(game_time_globals);
  game_frame(game_time_globals->speed * param_1);
}
