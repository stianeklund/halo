
#ifdef MSVC
#pragma code_seg(".thunks")
#define asm __asm
#define THUNK(x) x ## __thunk
#else
#define THUNK(x) __attribute__((weak, section(".thunks"))) x
#endif

#ifdef MSVC
#pragma comment(linker, "/alternatename:_director_apply_replay_mode_for_player=_director_apply_replay_mode_for_player__thunk")
#endif

void (*director_apply_replay_mode_for_player__xbe)(/* char reset_flag@<al>, */ int16_t local_player_index, char mode_flags) = (void*)0x87050;

void THUNK(director_apply_replay_mode_for_player)(char reset_flag, int16_t local_player_index, char mode_flags)
{
  asm mov al, reset_flag;
  director_apply_replay_mode_for_player__xbe(local_player_index, mode_flags);
}
#ifdef MSVC
#pragma comment(linker, "/alternatename:_director_compute_camera_input=_director_compute_camera_input__thunk")
#endif

bool (*director_compute_camera_input__xbe)(/* short * out_buf@<eax>, */ int local_player_index) = (void*)0x87110;

bool THUNK(director_compute_camera_input)(short *out_buf, int local_player_index)
{
  asm mov eax, out_buf;
  return director_compute_camera_input__xbe(local_player_index);
}
#ifdef MSVC
#pragma comment(linker, "/alternatename:_director_init_player_cameras=_director_init_player_cameras__thunk")
#endif

void (*director_init_player_cameras__xbe)(/* int16_t local_player_index@<ax> */ void) = (void*)0x86600;

void THUNK(director_init_player_cameras)(int16_t local_player_index)
{
  asm mov ax, local_player_index;
  director_init_player_cameras__xbe();
}
#ifdef MSVC
#pragma comment(linker, "/alternatename:_director_set_player_camera_normal=_director_set_player_camera_normal__thunk")
#endif

void (*director_set_player_camera_normal__xbe)(/* int16_t local_player_index@<ax>, */ char reset_flag, char mode_flags) = (void*)0x86de0;

void THUNK(director_set_player_camera_normal)(int16_t local_player_index, char reset_flag, char mode_flags)
{
  asm mov ax, local_player_index;
  director_set_player_camera_normal__xbe(reset_flag, mode_flags);
}
#ifdef MSVC
#pragma comment(linker, "/alternatename:_director_set_player_camera_scripted=_director_set_player_camera_scripted__thunk")
#endif

void (*director_set_player_camera_scripted__xbe)(/* int16_t local_player_index@<si>, */ char reset_flag) = (void*)0x86fa0;

void THUNK(director_set_player_camera_scripted)(int16_t local_player_index, char reset_flag)
{
  asm mov si, local_player_index;
  director_set_player_camera_scripted__xbe(reset_flag);
}
#ifdef MSVC
#pragma comment(linker, "/alternatename:_screenshot_render=_screenshot_render__thunk")
#endif

void (*screenshot_render__xbe)(/* void * a1@<edi> */ void) = (void*)0x102510;

void THUNK(screenshot_render)(void *a1)
{
  asm mov edi, a1;
  screenshot_render__xbe();
}
#ifdef MSVC
#pragma comment(linker, "/alternatename:_player_control_get_facing=_player_control_get_facing__thunk")
#endif

void (*player_control_get_facing__xbe)(/* int16_t local_player_index@<edi>, */ float delta_time) = (void*)0xb8650;

void THUNK(player_control_get_facing)(int16_t local_player_index, float delta_time)
{
  asm mov edi, local_player_index;
  player_control_get_facing__xbe(delta_time);
}
#ifdef MSVC
#pragma comment(linker, "/alternatename:_rumble_calculate=_rumble_calculate__thunk")
#endif

uint32_t (*rumble_calculate__xbe)(/* char * slot@<eax> */ void) = (void*)0xb9e30;

uint32_t THUNK(rumble_calculate)(char *slot)
{
  asm mov eax, slot;
  return rumble_calculate__xbe();
}
#ifdef MSVC
#pragma comment(linker, "/alternatename:_player_register_machine=_player_register_machine__thunk")
#endif

void (*player_register_machine__xbe)(/* unsigned short local_player_index@<eax>, */ int player_handle) = (void*)0xba290;

void THUNK(player_register_machine)(unsigned __int16 local_player_index, int player_handle)
{
  asm mov eax, local_player_index;
  player_register_machine__xbe(player_handle);
}
#ifdef MSVC
#pragma comment(linker, "/alternatename:_player_reset_action_result=_player_reset_action_result__thunk")
#endif

void (*player_reset_action_result__xbe)(/* int player_handle@<eax> */ void) = (void*)0xbaeb0;

void THUNK(player_reset_action_result)(int player_handle)
{
  asm mov eax, player_handle;
  player_reset_action_result__xbe();
}
#ifdef MSVC
#pragma comment(linker, "/alternatename:_render_window=_render_window__thunk")
#endif

void (*render_window__xbe)(/* int16_t * win@<esi>, */ void * offset_or_null) = (void*)0x185290;

void THUNK(render_window)(int16_t *win, void *offset_or_null)
{
  asm mov esi, win;
  render_window__xbe(offset_or_null);
}
#ifdef MSVC
#pragma comment(linker, "/alternatename:_render_window_pregame=_render_window_pregame__thunk")
#endif

void (*render_window_pregame__xbe)(/* int window_type@<ebx>, */ int16_t * win) = (void*)0x184bc0;

void THUNK(render_window_pregame)(int window_type, int16_t *win)
{
  asm mov ebx, window_type;
  render_window_pregame__xbe(win);
}
