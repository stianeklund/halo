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
  game_time_globals_t *globals;

  globals = game_time_globals;
  if (globals) {
    globals->initialized = false;
    globals->active = false;
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
  char *globals;

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
  *(int16_t *)0x457070 += 1;
  *(int16_t *)0x457072 += wall_delta;
  if (wall_delta > *(int16_t *)0x457076)
    *(int16_t *)0x457076 = wall_delta;
  *(unsigned int *)0x45706c = now;
  if (wall_delta < *(int16_t *)0x457074)
    *(int16_t *)0x457074 = wall_delta;

  *(int16_t *)0x457078 += a;
  if (a > *(int16_t *)0x45707c)
    *(int16_t *)0x45707c = a;
  min_a = *(int16_t *)0x45707a;
  if (a < min_a) {
    min_a = a;
    *(int16_t *)0x45707a = min_a;
  }

  *(int16_t *)0x45707e += b;
  if (b > *(int16_t *)0x457082)
    *(int16_t *)0x457082 = b;
  if (b < *(int16_t *)0x457080)
    *(int16_t *)0x457080 = b;

  *(int16_t *)0x457084 += c;
  if (c > *(int16_t *)0x457088)
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

  globals = (char *)game_time_globals;
  new_val = (min_a >= 0) ? 1 : 0;
  if (new_val != *(int16_t *)(globals + 4)) {
    *(int16_t *)(globals + 4) = new_val;
    *(int16_t *)(globals + 6) = 0;
    *(int16_t *)(globals + 8) = new_val ? 0x7fff : -0x8000;
    min_a = *(int16_t *)0x45707a;
  }

  switch (*(int16_t *)(globals + 4)) {
  case 0:
    if (min_a > *(int16_t *)(globals + 8))
      *(int16_t *)(globals + 8) = min_a;
    break;
  case 1:
    if (min_a < *(int16_t *)(globals + 8))
      *(int16_t *)(globals + 8) = min_a;
    break;
  default:
    break;
  }

  *(int16_t *)(globals + 6) += 1;
  if (*(int16_t *)(globals + 6) == 5) {
    *(int16_t *)(globals + 4) = -1;
  }
  byte_457068 = 0;
}

void game_time_start(void)
{
  game_time_globals_t *globals;

  assert_halt(game_time_globals && game_time_globals->initialized);
  assert_halt(!game_time_globals->active);
  assert_halt(game_time_globals);
  globals = game_time_globals;
  globals->speed = 1.0;
  globals->leftover_dt = 0;
  globals->active = 1;
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

extern double floor(double);

/* 0xb6020
 *
 * Compare operands are read from .rdata so they match the original's
 * `fcom DWORD PTR ds:...` forms:
 *   0x2533c0 = 0.0f, 0x253f00 = 100.0f, 0x254cb8 = 1000.0f,
 *   TICKS_PER_SECOND = *(float *)0x253394 = 30.0f.
 * game_time_globals + 0x14 is the "target/end tick" counter driven by
 * update_client_get_maximum_possible_server_time(); it has no name in game_time_globals_t yet. */
void game_time_update(float param_1)
{
  float fVar1;
  float fVar2;
  double ticks_f;
  double leftover_base;
  int ticks_elapsed;
  int maximum_ticks;
  bool clamp_leftover;
  int conn;
  void *server;
  unsigned int server_min_time;
  unsigned int cur_time;
  int update_time;
  int extra_ticks;
  int delta;
  int target_tick;
  int n;
  int cur_tick;
  game_time_globals_t *globals;

  assert_halt(game_time_globals);
  if (!game_time_globals->active) {
    game_time_globals->elapsed = 0;
    return;
  }
  fVar2 = game_time_globals->speed * TICKS_PER_SECOND;
  if (!(fVar2 > *(float *)0x2533c0))
    goto LAB_end;
  conn = (int)(int16_t)game_connection();
  switch (conn) {
  case 0: /* jump-table entry 0 shares the default arm's body (0xb612e) */
    maximum_ticks = 7;
    clamp_leftover = true;
    break;
  default:
    maximum_ticks = 7;
    clamp_leftover = true;
    break;
  case 1:
    maximum_ticks = 0x1e;
    clamp_leftover = true;
    break;
  case 2:
    server = global_network_game_server_get();
    server_min_time =
      network_game_server_get_oldest_client_update_received((int)server);
    cur_time = (unsigned int)game_time_get();
    if (cur_time - server_min_time > 0x80) {
      assert_halt_msg(0, "update server is too far ahead of a client for the "
                         "client to ever catch up!");
    }
    if (cur_time > 0) {
      delta = (int)(server_min_time - cur_time) + 0x80;
      if (delta < 0x1e) {
        maximum_ticks = delta;
        if (delta <= 0) {
          network_game_server_stalled_on_client(server, true);
          clamp_leftover = true;
          break;
        }
      } else {
        maximum_ticks = 0x1e;
      }
      network_game_server_stalled_on_client(server, false);
    } else {
      maximum_ticks = 1;
    }
    clamp_leftover = true;
    break;
  case 3:
    maximum_ticks = 0x1e;
    clamp_leftover = false;
    break;
  }
  fVar1 = param_1 + game_time_globals->leftover_dt;
  /* The clamped value feeds the tick count; the UNCLAMPED floor result is what
   * stays on the x87 stack and drives leftover_dt (0xb616c pops only the
   * duplicate/constant pushed at 0xb6162/0xb6166). */
  ticks_f = floor((double)(fVar1 * fVar2));
  ticks_elapsed =
    (int)(ticks_f <= *(float *)0x254cb8 ? ticks_f : (double)*(float *)0x254cb8);
  if (ticks_elapsed > maximum_ticks) {
    /* 0xb617c/0xb617f clamp before testing clamp_leftover: the clamp is
     * unconditional, only the leftover_dt reset is gated. */
    ticks_elapsed = maximum_ticks;
    if (clamp_leftover)
      leftover_base = ticks_f / fVar2;
    else
      leftover_base = fVar1;
  } else {
    leftover_base = fVar1;
  }
  game_time_globals->leftover_dt = (float)(leftover_base - ticks_f / fVar2);
  if (game_time_globals->leftover_dt < *(float *)0x2533c0)
    game_time_globals->leftover_dt = 0.0f;
  assert_halt_msg(game_time_globals->leftover_dt >= *(float *)0x2533c0 &&
                    game_time_globals->leftover_dt < *(float *)0x253f00,
                  "game_time_globals->leftover_dt>=0.f && "
                  "game_time_globals->leftover_dt<100.f");
  if ((int)(int16_t)game_connection() == 1) {
    maximum_ticks = update_client_get_maximum_actions();
    if (ticks_elapsed > maximum_ticks) {
      ticks_elapsed = maximum_ticks - 1 < 0 ? 0 : maximum_ticks - 1;
    } else if (ticks_elapsed + 7 < maximum_ticks) {
      ticks_elapsed = maximum_ticks - 1 < 0 ? 0 : maximum_ticks - 1;
    } else if (ticks_elapsed + 1 < maximum_ticks) {
      ticks_elapsed = ticks_elapsed + 1;
    }
    assert_halt_msg(ticks_elapsed <= maximum_ticks,
                    "ticks_elapsed <= maximum_actions");
    if (ticks_elapsed > maximum_ticks)
      ticks_elapsed = maximum_ticks;
  }
  if (ticks_elapsed > 0) {
    globals = game_time_globals;
    while (1) {
      cur_tick = (int)globals->time;
      if (cur_tick >= *(int *)((char *)globals + 0x14))
        break;
      globals->time = (unsigned int)(cur_tick + 1);
      if (--ticks_elapsed <= 0)
        break;
    }
    target_tick = (int)globals->time + ticks_elapsed;
    switch ((int)(int16_t)game_connection()) {
    case 0:
      update_client_local_ticks((int16_t)ticks_elapsed);
      break;
    case 2:
      network_game_server_update_ticks((int)global_network_game_server_get(),
                                       (unsigned short)ticks_elapsed);
      break;
    default:
      break;
    }
    update_time = update_client_get_maximum_possible_server_time();
    if (update_time > *(int *)((char *)game_time_globals + 0x14)) {
      if (update_time <= target_tick)
        target_tick = update_time;
      extra_ticks = target_tick - *(int *)((char *)game_time_globals + 0x14);
      if (extra_ticks > 0) {
        n = extra_ticks;
        do {
          game_tick();
          *(int *)((char *)game_time_globals + 0x14) += 1;
          game_time_globals->time++;
          n--;
        } while (n != 0);
      }
    } else {
      extra_ticks = 0;
    }
    game_time_statistics_frame(
      (int16_t)((uint16_t)update_time - (uint16_t)game_time_globals->time),
      (int16_t)extra_ticks, 0);
    game_time_globals->elapsed = (uint16_t)ticks_elapsed;
  }
LAB_end:
  assert_halt(game_time_globals);
  game_frame(game_time_globals->speed * param_1);
}
