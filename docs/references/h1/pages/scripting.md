# Halo 1 Reference: scripting

## Source Page

- Source: `https://c20.reclaimers.net/h1/scripting/`
- Local path: `scripting/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

General information
Halo 1
H1 Editing Kit
Community tools
Halo Custom Edition
Guides
Source data
Tags
Scripting
Gotchas and limits
Advanced techniques
Maps
Engine systems
Halo 2
Halo 3
Halo 3 ODST
Halo Reach
Halo 4
Halo 2 Anniversary MP
Contributing to c20
The Reclaimers LibraryHalo 1
Scripting

h1/hsc

Second edition.

To h1/scenario with
H1-Sapien
OS_Sapien
From h1/scenario with
invader-recover
Mozzarilla

HaloScript is a scripting language that H1 map designers can use to have greater control over how their map works. It is primarily used in controlling the mission structure of singleplayer maps, but can also be used to achieve certain effects in multiplayer, though AI is not synchronized.

To learn HaloScript basics see our introduction page and a list of gotchas and limits. Reading the stock campaign scripts included with the H1A mod tools can also be a great way to learn.

Compiling scripts into a scenario

Scripts source files end with .hsc and live in a data\...\your_level\scripts folder. You're limited with the number and size of these files. With the newer H1A modding tools these scripts will be automatically compiled and added to the processed scenario tag when the level is loaded with the tools or built into a map.

Users of the legacy HEK must instead use Sapien to compile scripts before they will take effect.

Extensions

Advanced users may be interested in the Halo Script Preprocessor, which allows you to use C-like preprocessor directives like #define. Modders targeting OpenSauce also have access to additional non-standard scripting functions, although some of these are now present in H1A.

HSC reference

To learn more about HSC's general syntax and execution model, see our cross-game scripting page.

Declaring scripts

Within a script source file you can delcare multiple scripts which contain a series of expressions. There are different types of scripts which run at different times:

Type

Comments

Example

continuous

Runs every tick (simulation frame of the engine), at 30 ticks per second.

(script continuous kill_players_in_zone
  (if (volume_test_object kill_volume (list_get (players) 0))
    (unit_kill (unit (list_get (players) 0)))
  )
  (if (volume_test_object kill_volume (list_get (players) 1))
    (unit_kill (unit (list_get (players) 1)))
  )
)

dormant

Sleeps until started with wake, runs until there are no instructions left, then stops. Calling wake a second time will not restart the script.

(script dormant save_valley_canyon
  (sleep_until (= 0 (ai_living_count valley_canyon)) 10)
  (game_save_no_timeout)
)

startup

Begins executing when the level initially loads and would be used for setting up AI alliances, running intro cinematics, and orchestrating the overall mission structure. You can have multiple startup scripts in the scenario.

(script startup mission_a30
  (hud_show_motion_sensor false)
  (fade_out 0 0 0 0)
  (print "mission script is running")
  (ai_allegiance player human)
  ;...
)

static

Performed when called from another script, possibly returning a value. These can be called as many times as needed and are run in the thread of the calling script. Static scripts are ideal for shared and reusable logic.

In H1A these can support up to 16 parameters like later games. Parameters shadow any globals with the same name and cannot be modified. Outside H1A, you'll need to use globals to emulate parameters.

; without parameters, call like (player0)
(script static "unit" player0
  (unit (list_get (players) 0))
)
; with parameters (H1A only), call like (my_add 1 2)
(script static short (my_add (short x) (short y))
  (+ x y)
)

stub

A stub script by itself does nothing. It allows you to pre-declare a static script without providing a script body. This allows other scripts to invoke a static script without the full function body being declared yet.

The script you override a stub with must have the same "signature" (i.e. name, parameters, and return type). A stub cannot be declared more than once. The full implementation of a stub may be placed in another script file.

If you're familiar with C function prototypes, this is very similar.

(script stub object player0)

The number of scripts you can declare is limited.

Declaring globals

Globals are variables that can be read and set by your scripts. They exist for the lifetime of the level so can be used to store state related to a mission, or commonly used constant values that you want to declare once and reuse. Globals must be declared before they are used:

(global <value type> <name> <inital value>)

Globals for stock levels are typically declared in the base_*.hsc file but used in the mission_*.hsc and/or cinematics_*.hsc files, but there is no requirement that you separate them this way. Here are some examples from a30's scripts:

(global long delay_calm (* 30 3)) ; 3 seconds
(global boolean global_rubble_end false)

(script dormant mission_rubble
  ; ...
  (set global_rubble_end true)
)

(script dormant mission_river
  ; ...
  (sleep delay_calm)
  (if (and global_rubble_end global_cliff_end) (set play_music_a30_07 true))
)

The number of globals you can declare is limited.

Value types

Type

Details

Example

void

Void means "no value", used for the return type of functions or static scripts which are not meant to return anything, e.g. cinematic_start.

N/A

passthrough

This means the value type is passed through from an inner expression, used by set and some control flow functions like if.

N/A

boolean

A value that is either true or false. Boolean values are commonly set as just 1 or 0 even in official level scripts. You can also use on and off. Values are case-insensitive in H1A, so False and TRUE will be understood too.

true, false, 1, 0, on, off

real

Floating point (decimal) value. Value Range: 3.4E +/- 38 (6 digits)

3.000000, 1.5, .3

short

16-bit short integer value, stored in two's complement. Value Range: −32,768 to 32,767. See when to use short vs long.

2, -120, 1000

long

32-bit long integer value, stored in two's complement. Value Range: -2,147,483,648 to 2,147,483,647

2000000000

string

String of characters in double quotes. Max number of characters: 32

"This is a string."

trigger_volume

A "Trigger Volumes" value (a block in the scenario tag)

cutscene_flag

A "Cutscene Flags" value (a block in the scenario tag)

cutscene_camera_point

A "Cutscene Camera Points" value (a block in the scenario tag)

cutscene_title

A "Cutscene Titles" value (a block in the scenario tag)

cutscene_recording

A "Cutscene Recording" value that isn't in the public HaloCE scenario tag?

device_group

A "Device Groups" value (a block in the scenario tag)

ai

An "Encounters" value

ai_command_list

A "Command Lists" value (a block in the scenario tag)

starting_profile

A "Player Starting Profile" value (a block in the scenario tag)

conversation

A "AI Conversations" value (a block in the scenario tag)

navpoint
hud_message

HUD message text

object_list

A list of objects, such as players or AI actors. Once the list is created, it is semi-fixed but may shrink in size over time if objects are deleted from the world. For example, if you use vehicle_riders to obtain a list and then assign it to a global, the list will continue to contain whichever units were in the vehicle at the time the list was created, regardless of current riders.

Dead units will also still remain in the list and are counted by list_count, but not list_count_not_dead. However, any objects deleted from the world will get removed from the list and the count will reflect that. Examples include object_destroy or garbage collection of dead bodies (either naturally or with garbage_collect_now). This frees up space for other object lists.

(players)

sound

Sound

effect

Effect

damage

Damage Effect

looping_sound

Sound Looping

animation_graph

Model Animations

actor_variant

Actor variant

damage_effect

Damage Effect

object_definition
game_difficulty

easy, normal, hard, impossible

team

player, human, covenant, flood, sentinal

ai_default_state

none, sleeping, alert, moving, guarding, searching, fleeing

actor_type

ie, elite

hud_corner

top_left, top_right, bottom_left, bottom_right

object

Object

unit

Unit

vehicle

Vehicle

weapon

Weapon

device

Device

scenery

Scenery

object_name

An "Object Names" value (a block in the scenario tag)

unit_name

A "Bipeds" value (a block in the scenario tag)

vehicle_name

A "Vehicles" value (a block in the scenario tag)

weapon_name

A "Weapons" value (a block in the scenario tag)

device_name

A "Device" value (a block in the scenario tag)

scenery_name

A "Scenery" value (a block in the scenario tag)

When to use short vs long

There are two integer variable types: short and long. Both hold whole numbers, but long variables can hold much larger numbers than short variables. It's worth noting both use the same amount of memory, so you should decide the type you use based on what range of values makes sense or the values the functions you call accept (avoids a cast). If you need to optimize memory usage you can use the bitwise functions to implement a bitfield.

Functions
Control

Control functions include waking and sleeping script theads and conditionally executing expressions.

Function

(<passthrough> begin <expression(s)>)
(begin
  (object_destroy mid_banshee_1)
  (ai_erase mid_banshee/pilot_1)
)

The begin expression evaluates a series of expressions in order. It's primarily used to wrap a series of expressions into a single expression to be used as a cond result or one of the branches of an if expression. It is not necessary to wrap all the top-level expressions within a script declaration with a begin; there is already one implicit. The return value of the last expression in the sequence will be returned by cond.

(<passthrough> begin_random <expression(s)>)
(begin_random
  (begin (vehicle_unload fort_cship "cd-passengerl01") (sleep 5))
  (begin (vehicle_unload fort_cship "cd-passengerl02") (sleep 5))
  (begin (vehicle_unload fort_cship "cd-passengerl03") (sleep 5))
)

Like begin, but evaluates the sequence of expressions in random order and returns the last value evaluated. An example use case is making passengers exit a dropship in random order. This function can contain up to 32 expressions.

(<passthrough> cond (<boolean1> <result1>) [(<boolean2> <result2>) [...]])
(cond
  ((game_is_cooperative) (wake fast_setup))
  ((not (= normal (game_difficulty_get))) (wake fast_setup))
  (true (wake tutorial_setup))
)

Tests each branch's condition expression. On the first true condition, the associated result expression is evaluated and its value returned. Has no default value. This is similar to an if-elseif-elseif... chain in other languages.

(<passthrough> if <boolean> <then> [<else>])
(if (game_safe_to_speak)
  (ai_conversation first_arrival)
)
(if (game_is_cooperative)
  (begin
    (ai_place area_a_cov/elites)
    (ai_place area_a_cov/jackals)
  )
  (ai_place area_a_cov/grunts)
)

Conditionally evaluates expressions based on a condition. If the condition is true, the <then> branch is evaluated. If the condition is false, the optional [<else>] branch is evaluated instead. If you want to execute a series of expressions then you must wrap them with a begin. This returns the value of the branch expression chosen.

(<void> sleep <short> [<script>])
(sleep (* 30 10)) ; sleep this thread for 10s
(sleep 30 more_weapons) ; sleep more_weapons for 1s
(sleep -1) ; sleep indefinitely

Pauses execution of this script (or, optionally, another script) for the specified number of ticks (1 tick = 1/30 second). You can "park" a thread with (sleep -1) and it will sleep until another thread wakes it or sleeps it with a positive number of ticks.

(<void> sleep_until <boolean> [short_1] [short_2])
; sleeps until less than 3 enemies remain, checking 2x per second:
(sleep_until (> 3 (ai_living_count first_wave)) 15)

Pauses execution of this script until condition is true. By default, this checks once per second (every 30 ticks), but if short_1 is specified, it checks every short_1 ticks instead.

By default, this will await the condition indefinitely but short_2 can be used to timeout after a certain number of ticks instead. After the timeout, the script will continue on as if condition became true.

(<void> thread_sleep <long>)
(thread_sleep 20)

Sleeps the calling thread for the specified number of ms.

(<void> wake <script name>)
(wake more_weapons)

Wakes a sleeping or dormant script in the next update (tick).

See also advanced control methods.

Math

Function

(<long> abs_integer <long>)

return the absolute (non-negative) value of an integer

(<real> abs_real <real>)

return the absolute (non-negative) value of a real

(<real> / <number> <number>)
(/ 10 5)
; returns: 2
(/ 2.5 2)
; returns: 1.25

Returns the quotient of two expressions. The second argument, the denominator, must NOT be in the range -0.0001 to 0.0001 (zero or near zero) or an assertion will be thrown.

(<real> max <number(s)>)
(max 60 5 10)
; returns: 60

Returns the maximum of all specified expressions.

(<real> min <number(s)>)
(min 60 5 10)
; returns: 5

Returns the minimum of all specified expressions.

(<real> - <number(s)>)
(- 10 5 1)
; returns: 4

Returns the first expression subtracted by all following expressions.

(<real> * <number(s)>)
(* 5 5)
; returns: 25
(* 5.5 6)
; returns: 33

Returns the product of all specified expressions.

(<real> + <number(s)>)
(+ 5 6 7 8 9)
; returns: 35

Returns the sum of all specified expressions.

Other unsupported math operations can be built using functions the game does support.

Logic and comparison

These functions can be used to perform logical comparisons and bitwise integer operations. Bitwise functions are new to H1A only.

Function

(<boolean> and <boolean(s)>)
(and (player_action_test_action) (= 1 (list_count (players))))

Returns true if all specified expressions are true. This function is short circuiting, meaning it will not evaluate subsequent expressions if a a prior one evaluates to false.

(<long> bitwise_and <long> <long>)
(bitwise_and 6 3) ;2 (0110 & 0011 = 0010)

Returns the bitwise AND of two numbers. If a bit is 1 in both of the inputs, it is 1 in the output and otherwise 0.

(<long> bitwise_flags_toggle <long> <long> <boolean>)
(bitwise_flags_toggle 13 3 true) ;15 (1101 => 1111)
(bitwise_flags_toggle 13 3 false) ;12 (1101 => 1100)

Uses the second argument as a mask to toggle bits on or off in the first argument.

(<long> bitwise_left_shift <long> <short>)
(bitwise_left_shift 1 4) ;16 (00001 << 4 = 10000)
(bitwise_left_shift 5 1) ;10 (00101 << 1 = 01010)

Performs a bitwise left shift of a value by the given bit count. All bits are moved to the left by a number of positions, with the rightmost bits filling with 0's. Returns 0 if the positions argument is negative. This has the effect of multiplying the argument by a power of 2.

(<long> bitwise_or <long> <long>)
(bitwise_or 6 3) ;7 (0110 | 0011 = 0111)

Returns the bitwise OR of two numbers. If a bit is 1 in either or both of the inputs, it is 1 in the output and otherwise 0.

(<long> bitwise_right_shift <long> <short>)
(bitwise_right_shift 13 2) ;3 (1101 >> 2 = 0011)
(bitwise_right_shift -1 1) ;2147483647

Performs an unsigned right shift (also called logical or zero filling shift) of a value by the given bit count. All bits are moved to the right by a number of positions, with the leftmost bits filling with 0's. Returns 0 if the positions argument is negative.

(<long> bitwise_xor <long> <long>)
(bitwise_xor 5 5) ;0 (0101 ^ 0101 = 0000)
(bitwise_xor 12 15) ;3 (1100 ^ 1111 = 0011)

Returns the bitwise XOR (exclusive or) of two numbers. If a bit differs in both inputs, it is 1 in the output and otherwise 0. In other words, one or the other bit must be 1 but not both.

(<long> bit_test <long> <short>)
(bit_test 2 1) ;1
(bit_test 2 0) ;0

Returns 1 or 0 for the bit at the given position.

(<long> bit_toggle <long> <short> <boolean>)
(bit_toggle 0 2 true) ;4 (0000 => 0100)
(bit_toggle 7 0 false) ;6 (0111 => 0110)

Sets the value's bit at the given position on or off.

(<boolean> = <expression> <expression>)
(= (hud_get_timer_ticks) 0)

Returns true if two expressions are equal.

(<boolean> >= <number> <number>)
(>= 10 10)
; returns: true
(>= 5 10)
; returns: false

Returns true if the first number is larger than or equal to the second.

(<boolean> > <number> <number>)
(> 10 5)
; returns: true
(> 5 10)
; returns: false

Returns true if the first number is larger than the second.

(<boolean> <= <number> <number>)
(<= 4 4)
; returns: true
(<= 8 4)
; returns: false

Returns true if the first number is smaller than or equal to the second.

(<boolean> < <number> <number>)
(< 4 8)
; returns: true
(< 8 4)
; returns: false

Returns true if the first number is smaller than the second.

(<boolean> != <expression> <expression>)
(!= (hud_get_timer_ticks) 0)

Returns true if two expressions are not equal.

(<boolean> not <boolean>)
(not bsl_sucks)
; returns: false

Returns the opposite of the expression.

(<boolean> or <boolean(s)>)
(or (player_action_test_action) true)
; returns: true

Returns true if any specified expressions are true. This function is short circuiting, meaning it will not evaluate subsequent expressions if a a prior one evaluates to true.

Server functions

These functions apply to running a dedicated server or locally-hosted server for Custom Edition/PC retail.

Function

(sv_ban <player index or name> [duration(m,h,d)])

(Server Only) The given player is kicked and added to banned.txt. Use sv_players to find the player's index. You can also specify an optional duration for timed ban. Use 0 to follow sv_ban_penalty rules.

(sv_ban [player # or name] opt:[duration (#)(m,h,d)])

<Server Only> Player is kicked and added to banned.txt. Use sv_players to find the index.Specify optional duration for timed ban. Use 0 to follow sv_ban_penalty rules.

(sv_banlist)

Print a list of banned players

(sv_banlist_file [alphanumeric banlist file suffix])

Sets and opens the file to be used for the player ban list.

(sv_ban_penalty [(#)(m,h,d), 0=infinite])

Specify up to 4 ban times for repeat ban/TK offenders.

(sv_end_game)

End the current game

(sv_friendly_fire ["0" = defaults, "1" = off, "2" = shields, "3" = on])

Use to provide a global override for the gametype friendly fire setting.

(sv_gamelist [substring])

Display a list of game types, matching an optional substring.

(sv_get_player_action_queue_length <string>)

Displays the action queue length for the specified player.

(sv_kick <player index or name>)
(sv_kick "Micro$oft")

(Server Only) Kicks the specified player from the server

(sv_log_echo_chat [preference])

Enables or disbles chat echo to the console. Set the preference to 0to disable chat echo, or 1 to enable chat echo.If the preference is not specified, displays the current preference.

(sv_log_enabled ["1" to enable, "0" to disable])

Enables or disables server logging. If 0/1 is not given, displays thecurrent logging status.

(sv_log_file [log file name])

Sets the server log file name. If no name is given, displays thecurrent log file name.

(sv_log_note <string>)

Leave a note in the server log

(sv_log_rotation_threshold [threshold in kilobytes])

Sets the log rotation threshold. When a log file's size (in kilobytes) exceedsthis number, it will be rotated. Set to 0 to disable log rotation.If the threshold is not specified, displays the current threshold.

(<void> sv_map <string> <string>)
(sv_map bloodgulch slayer)

(Server Only) Usage: sv_map <mapname> <variantname> Aborts the current game and playlist, and starts the specified game mode on the specified map.

(sv_mapcycle)

Print the contents of the currently loaded mapcycle file

(sv_mapcycle_add <string> <string>)

Usage: sv_mapcycle_add <mapname> <variantname> Add a new game to the end of the mapcycle file

(sv_mapcycle_begin)

Restart or begin playing the currently loaded mapcycle file

(sv_mapcycle_del <long>)

Usage: sv_mapcycle_del <index> Removes the game at <index>. Will not affect running games.

(sv_maplist [substring])

Display a list of maps, matching an optional substring.

(sv_map_next)

(Server Only) Abort the current game and begin the next game in the playlist

(sv_map_reset)

(Server Only) Reset the current game

(sv_maxplayers [short])
(sv_maxplayers 10)

Sets the maximum number of players (between 1 and 16). If no value is given, displays the current value.

(sv_motd [motd file name])

Sets the server message of the day file name. If no name is given, displays thecurrent motd file name. Set to "" to turn motd off.

(sv_name [name])
(sv_name)
(sv_name "yousuck")

Sets the name of the server. If no name is given, displays the current name.

(sv_parameters_dump)

Dumps out the local parameter configuration to parameters.cfg file

(sv_parameters_reload)

(Server Only) Reloads the parameters.cfg file

(sv_password [password])
(sv_password)
(sv_password "1234")

Sets the server password. If no password is given, displays the current password.

(sv_players)

(Server Only) Prints (not returns) a list of players in the current game

(sv_rcon_password [remote console password])

Sets the server remote console password. If no password is given, displays thecurrent password. Enter "" to disable rcon.

(sv_say <string>)

<Server Only> Usage: "sv_say <message>"Send a message to users

(sv_single_flag_force_reset [boolean])

Force the flag to reset in single flag CTF games when the timer expires, even if held by a player.If not specified, displays the current value.

(sv_status)

Shows status of the server

(sv_timelimit ["-1" = default, "0" = infinite, <time in minutes>])

Use to provide a global override for the gametype timelimit setting.

(sv_tk_cooldown [time (#)(s,m)])

Specify a TK point cooldown period, after which players lose a TK point.

(sv_tk_grace [time (#)(s,m)])

Specify the grace period for TK during which you don't get a TK point.

(sv_unban <long>)
(sv_unban 1)

(Server Only) Usage: sv_unban <index> Removes player at index in the banlist. Use sv_banlist to find the index.

Other functions

All other functions are used for debugging and gameplay scripting purposes, such as controlling AI, cinematics, checkpoints, and more.

A · B · C · D · E · F · G · H · I · L · M · N · O · P · Q · R · S · T · U · V (506 total)

Function

(<void> activate_nav_point_flag <navpoint> <unit> <cutscene_flag> <real>)

Activates a nav point type <string> attached to (local) player <unit> anchored to a flag with a vertical offset <real>. If the player is not local to the machine, this will fail.

(<void> activate_nav_point_object <navpoint> <unit> <object> <real>)

Activates a nav point type <string> attached to (local) player <unit> anchored to an object with a vertical offset <real>. If the player is not local to the machine, this will fail.

(<void> activate_team_nav_point_flag <navpoint> <team> <cutscene_flag> <real>)

Activates a nav point type <string> attached to a team anchored to a flag with a vertical offset <real>. If the player is not local to the machine, this will fail.

(<void> activate_team_nav_point_object <navpoint> <team> <object> <real>)

Activates a nav point type <string> attached to a team anchored to an object with a vertical offset <real>. If the player is not local to the machine, this will fail.

(<void> ai <boolean>)

turns all AI on or off.

(<object_list> ai_actors <ai>)

Converts an ai reference to an object list

(<void> ai_allegiance <team> <team>)

Creates an allegiance between two teams. The engine supports at most 8 allegiances at a time.

(<boolean> ai_allegiance_broken <team> <team>)

Returns whether two teams have an allegiance that is currently broken by traitorous behavior

(<void> ai_allegiance_remove <team> <team>)

Destroys an allegiance between two teams

(<void> ai_allow_charge <ai> <boolean>)

Either enables or disables charging behavior for a group of actors

(<void> ai_allow_dormant <ai> <boolean>)

Either enables or disables automatic dormancy for a group of actors

(<void> ai_attach <unit> <ai>)

Attaches the specified unit to the specified encounter

(<void> ai_attach_free <unit> <actor_variant>)

Attaches a unit to a newly created free actor of the specified type

(<void> ai_attack <ai>)

Makes the specified platoon(s) go into the attacking state

(<void> ai_automatic_migration_target <ai> <boolean>)

Enables or disables a squad as being an automatic migration target

(<void> ai_berserk <ai> <boolean>)

Forces a group of actors to start or stop berserking

(<void> ai_braindead <ai> <boolean>)

Makes a group of actors braindead, or restores them to life (in their initial state)

(<void> ai_braindead_by_unit <object_list> <boolean>)

Makes a list of objects braindead, or restores them to life. If you pass in a vehicle index, it makes all actors in that vehicle braindead (including any built-in guns).

(<void> ai_command_list <ai> <ai_command_list>)

Tells a group of actors to begin executing the specified command list

(<void> ai_command_list_advance <ai>)

Tells a group of actors that are running a command list that they may advance further along the list (if they are waiting for a stimulus)

(<void> ai_command_list_advance_by_unit <unit>)

Just like ai_command_list_advance but operates upon a unit instead

(<void> ai_command_list_by_unit <unit> <ai_command_list>)

Tells a named unit to begin executing the specified command list

(<short> ai_command_list_status <object_list>)

Gets the status of a number of units running command lists: 0 = none, 1 = finished command list, 2 = waiting for stimulus, 3 = running command list

(<boolean> ai_conversation <conversation>)

Tries to add an entry to the list of conversations waiting to play. Returns FALSE if the required units could not be found to play the conversation, or if the player is too far away and the 'delay' flag is not set.

(<void> ai_conversation_advance <conversation>)

Tells a conversation that it may advance

(<short> ai_conversation_line <conversation>)

Returns which line the conversation is currently playing, or 999 if the conversation is not currently playing.

(<short> ai_conversation_status <conversation>)

Returns the status of a conversation (0=none, 1=trying to begin, 2=waiting for guys to get in position, 3=playing, 4=waiting to advance, 5=could not begin, 6=finished successfully, 7=aborted midway)

(<void> ai_conversation_stop <conversation>)

Stops a conversation from playing or trying to play

(<void> ai_debug_communication_focus <string(s)>)

Focuses (or stops focusing) a set of unit vocalization types

(<void> ai_debug_communication_ignore <string(s)>)

Ignores (or stops ignoring) a set of AI communication types when printing out communications

(<void> ai_debug_communication_suppress <string(s)>)

Suppresses (or stops suppressing) a set of AI communication types

(ai_debug_sound_point_set)

Drops the AI debugging sound point at the camera location

(ai_debug_speak <string>)
(ai_debug_speak "pain minor")

Makes the currently selected AI speak a vocalization

(ai_debug_speak_list <string>)
(ai_debug_speak_list "involuntary")

Makes the currently selected AI speak a list of vocalizations

(ai_debug_teleport_to <ai>)

Teleports all players to the specified encounter

(ai_debug_vocalize <string> <string>)

Makes the selected AI vocalize

(<void> ai_defend <ai>)

Makes the specified platoon(s) go into the defending state

(ai_deselect)

Clears the selected encounter

(<void> ai_detach <unit>)

Detaches the specified unit from all AI

(<void> ai_dialogue_triggers <boolean>)
(ai_dialogue_triggers true)
(ai_dialogue_triggers false)

Turns impromptu dialogue on or off. When off, units will still play some sounds (like when they take damage) but will not speak when exiting vehicles, seeing friends die, etc.

(<void> ai_disregard <object_list> <boolean>)
(ai_disregard (players) true)
(ai_disregard (players) false)

If true, forces all actors to completely disregard the specified units, otherwise lets them acknowledge the units again.

(<void> ai_erase <ai>)

Erases the specified encounter and/or squad

(<void> ai_erase_all)

Erases all AI

(<void> ai_exit_vehicle <ai>)

Tells a group of actors to get out of any vehicles that they are in

(<void> ai_follow_distance <ai> <real>)

Sets the distance threshold which will cause squads to migrate when following someone

(<void> ai_follow_target_ai <ai> <ai>)

Sets the follow target for an encounter to be a group of AI (encounter, squad or platoon)

(<void> ai_follow_target_disable <ai>)

Turns off following for an encounter

(<void> ai_follow_target_players <ai>)

Sets the follow target for an encounter to be the closest player. AI follow their target by moving to firing positions near their target with the same letter label. Note that the AI may need to be migrated to follow the player through multiple sets of firing positions.

(<void> ai_follow_target_unit <ai> <unit>)

Sets the follow target for an encounter to be a specific unit.

(<void> ai_force_active <ai> <boolean>)

Forces an encounter to remain active (i.e. not freeze in place) even if there are no players nearby

(<void> ai_force_active_by_unit <unit> <boolean>)

Forces a named actor that is NOT in an encounter to remain active (i.e. not freeze in place) even if there are no players nearby

(<void> ai_free <ai>)

Removes a group of actors from their encounter and sets them free

(<void> ai_free_units <object_list>)

Removes a set of units from their encounter (if any) and sets them free

(<short> ai_going_to_vehicle <unit>)

Return the number of actors that are still trying to get into the specified vehicle

(<void> ai_go_to_vehicle <ai> <unit> <string>)

Tells a group of actors to get into a vehicle, in the substring-specified seats (e.g. passenger for pelican). Does not interrupt any actors who are already going to vehicles.

You can test if a vehicle is flipped by calling this function, then in the same tick testing ai_going_to_vehicle. If it returns 0 it means the vehicle is flipped.

(<void> ai_go_to_vehicle_override <ai> <unit> <string>)

Tells a group of actors to get into a vehicle, in the substring-specified seats (e.g. passenger for pelican). Any actors who are already going to vehicles will stop and go to this one instead!

(<void> ai_grenades <boolean>)

Turns grenade inventory on or off

(<boolean> ai_is_attacking <ai>)

Returns whether a platoon is in the attacking mode (or if an encounter is specified, returns whether any platoon in that encounter is attacking)

(<void> ai_kill <ai>)

Instantly kills the specified encounter and/or squad

(<void> ai_kill_silent <ai>)

Instantly and silently (no animation or sound played) kills the specified encounter and/or squad

(ai_lines)

Cycles through AI line-spray modes

(<void> ai_link_activation <ai> <ai>)

Links the first encounter so that it will be made active whenever it detects that the second encounter is active

(<short> ai_living_count <ai>)

Return the number of living actors in the specified encounter and/or squad

(<real> ai_living_fraction <ai>)

Return the fraction [0-1] of living actors in the specified encounter and/or squad

(<void> ai_look_at_object <unit> <object>)

Tells an actor to look at an object until further notice

(<void> ai_magically_see_encounter <ai> <ai>)

Makes one encounter magically aware of another

(<void> ai_magically_see_players <ai>)

Makes an encounter magically aware of nearby players

(<void> ai_magically_see_unit <ai> <unit>)

Makes an encounter magically aware of the specified unit

(<void> ai_maneuver <ai>)

Makes all squads in the specified platoon(s) maneuver to their designated maneuver squads

(<void> ai_maneuver_enable <ai> <boolean>)

Enables or disables the maneuver/retreat rule for an encounter or platoon. The rule will still trigger, but none of the actors will be given the order to change squads.

(<void> ai_migrate <ai> <ai>)

Makes all or part of an encounter move to another encounter

(<void> ai_migrate_and_speak <ai> <ai> <string>)

Makes all or part of an encounter move to another encounter, and say their advance or retreat speech lines.

(<void> ai_migrate_by_unit <object_list> <ai>)

Makes a named vehicle or group of units move to another encounter

(<short> ai_nonswarm_count <ai>)

Return the number of non-swarm actors in the specified encounter and/or squad

(<void> ai_place <ai>)

Places the specified encounter on the map

(<void> ai_playfight <ai> <boolean>)

Sets an encounter to be playfighting or not

(<void> ai_prefer_target <object_list> <boolean>)

If true, ALL enemies will prefer to attack the specified units. If false, removes the preference.

(<void> ai_reconnect)

Reconnects all AI information to the current structure bsp (use this after you create encounters or command lists in sapien, or place new firing points or command list points)

(<void> ai_renew <ai>)

Refreshes the health and grenade count of a group of actors, so they are as good as new

(<void> ai_retreat <ai>)

Makes all squads in the specified platoon(s) maneuver to their designated maneuver squads

(<void> ai_select <ai>)

Selects the specified AI for use with AI debug functions like ai_debug_speak.

(<void> ai_set_blind <ai> <boolean>)

Enables or disables sight for actors in the specified encounter

(<void> ai_set_current_state <ai> <ai_default_state>)

Sets the current state of a group of actors. WARNING: may have unpredictable results on actors that are in combat.

(<void> ai_set_deaf <ai> <boolean>)

Enables or disables hearing for actors in the specified encounter

(<void> ai_set_respawn <ai> <boolean>)

Enables or disables respawning in the specified encounter

(<void> ai_set_return_state <ai> <ai_default_state>)

Sets the state that a group of actors will return to when they have nothing to do

(<void> ai_set_team <ai> <team>)

Makes an encounter change to a new team

(<void> ai_spawn_actor <ai>)

Spawns a single actor in the specified encounter and/or squad

(<short> ai_status <ai>)

Returns the most severe combat status of a group of actors (0=inactive, 1=noncombat, 2=guarding, 3=search/suspicious, 4=definite enemy(heard or magic awareness), 5=visible enemy, 6=engaging in combat)

(<void> ai_stop_looking <unit>)

Tells an actor to stop looking at whatever it's looking at

(<real> ai_strength <ai>)

Return the current strength (average body vitality from 0-1) of the specified encounter and/or squad

(<short> ai_swarm_count <ai>)

Return the number of swarm actors in the specified encounter and/or squad

(<void> ai_teleport_to_starting_location <ai>)

Teleports a group of actors to the starting locations of their current squad(s)

(<void> ai_teleport_to_starting_location_if_unsupported <ai>)

Teleports a group of actors to the starting locations of their current squad(s), only if they are not supported by solid ground (i.e. if they are falling after switching BSPs).

(<void> ai_timer_expire <ai>)

makes a squad's delay timer expire and releases them to enter combat.

(<void> ai_timer_start <ai>)

makes a squad's delay timer start counting.

(<void> ai_try_to_fight <ai> <ai>)

Causes a group of actors to preferentially target another group of actors

(<void> ai_try_to_fight_nothing <ai>)

Removes the preferential target setting from a group of actors

(<void> ai_try_to_fight_player <ai>)

Causes a group of actors to preferentially target the player

(<void> ai_vehicle_encounter <unit> <ai>)

Sets a vehicle to "belong" to a particular encounter/squad. Any actors who get into the vehicle will be placed in this squad. Vehicles potentially drivable by multiple teams need their own encounter!

(<void> ai_vehicle_enterable_actors <unit> <ai>)

Sets a vehicle as being impulsively enterable for a certain encounter/squad of actors

(<void> ai_vehicle_enterable_actor_type <unit> <actor_type>)

Sets a vehicle as being impulsively enterable for actors of a certain type (grunt, elite, marine etc)

(<void> ai_vehicle_enterable_disable <unit>)

Disables actors from impulsively getting into a vehicle (this is the default state for newly placed vehicles)

(<void> ai_vehicle_enterable_distance <unit> <real>)

Sets a vehicle as being impulsively enterable for actors within a certain distance

(<void> ai_vehicle_enterable_team <unit> <team>)

Sets a vehicle as being impulsively enterable for actors on a certain team

(attract_mode_start)

On Xbox, this would trigger the "attract" videos to play when the main menu is left idle. Not applicable to PC.

(<void> bind <string> <string> <string>)

Binds an input device/button combination to a game control

(<boolean> breadcrumbs_nav_points_active)

Returns true if breadcrumbs improved nav points are enabled.

(<void> breakable_surfaces_enable <boolean>)
(breakable_surfaces_enable false)

Enables or disables breakability of all breakable surfaces in the level

(<void> breakable_surfaces_reset)

Restores all breakable surfaces

(<void> camera_control <boolean>)
(camera_control true)
(camera_control false)

Toggles script control of the camera

(<void> camera_set <cutscene_camera_point> <short>)
(camera_set somewhere_point 100)

Moves the camera to the specified camera point over the specified number of ticks

(<void> camera_set_animation <animation_graph> <string>)

Begins a prerecorded camera animation

(<void> camera_set_dead <unit>)
(camera_set_dead (player0))

Makes the scripted camera zoom out around a unit as if it were dead

(<void> camera_set_first_person <unit>)
(camera_set_first_person (player0))

Makes the scripted camera follow a unit

(<void> camera_set_relative <cutscene_camera_point> <short> <object>)
(camera_set_relative somewhere_point 200 warthog_mp_1)

Moves the camera to the specified camera point over the specified number of ticks (position is relative to the specified object)

(<short> camera_time)

Returns the number of ticks remaining in the current camera interpolation

(<void> change_team <short>)
(change_team 0)
; changes you to red
(change_team 1)
; changes you to blue
(change_team 2)
; auto balance

Change your team (0=red, 1=blue, else=auto). Removed in H1A.

(<void> cheats_load)

Reloads the cheats.txt file

(<void> cheat_active_camouflage)

Gives the player active camouflage

(<void> cheat_active_camouflage_local_player <short>)
(cheat_active_camouflage_local_player 1)

Gives the player active camouflage

(<void> cheat_all_powerups)

Drops all powerups near player. The set of powerups is controlled by the globals tag.

(<void> cheat_all_vehicles)

Drops all vehicles on player

(<void> cheat_all_weapons)

Drops all weapons near player

(<void> cheat_spawn_warthog)

Drops a warthog near player

(<void> cheat_teleport_to_camera)

Teleports player to camera location

(<void> checkpoint_load <string>)

Load a saved checkpoint

(checkpoint_save)

Save last solo checkpoint

(<void> cinematic_abort)

Aborts a cinematic

(<void> cinematic_screen_effect_set_convolution <short> <short> <real> <real> <real>)

Sets the convolution effect

(<void> cinematic_screen_effect_set_filter <real> <real> <real> <real> <boolean> <real>)

Sets the filter effect

(<void> cinematic_screen_effect_set_filter_desaturation_tint <real> <real> <real>)

Sets the desaturation filter tint color

(<void> cinematic_screen_effect_set_video <short> <real>)

Sets the video effect: <noise intensity[0,1]>, <overbright: 0=none, 1=2x, 2=4x>

(<void> cinematic_screen_effect_start <boolean>)

Starts screen effect; pass true to clear

(<void> cinematic_screen_effect_stop)

Returns control of the screen effects to the rest of the game

(<void> cinematic_set_near_clip_distance <real>)

(<void> cinematic_set_title <cutscene_title>)

Activates the chapter title

(<void> cinematic_set_title_delayed <cutscene_title> <real>)

Activates the chapter title, delayed by <real> seconds

(<void> cinematic_show_letterbox <boolean>)
(cinematic_show_letterbox true)
(cinematic_show_letterbox false)

Sets or removes the letterbox bars

(<void> cinematic_skip_start_internal)

(<void> cinematic_skip_stop_internal)

(<void> cinematic_start)

Initializes game to start a cinematic (interruptive) cutscene

(<void> cinematic_stop)

Initializes the game to end a cinematic (interruptive) cutscene

(<void> cinematic_suppress_bsp_object_creation <boolean>)
(cinematic_suppress_bsp_object_creation true)
(cinematic_suppress_bsp_object_creation false)

Suppresses or enables the automatic creation of objects during cutscenes due to a bsp switch

(<void> cls)

Clears console output and tab completions from the screen.

(config_one_control <string>)

Test function to configure a single control

(connect <string> <string>)

Attempt to connect to server - use ip:port password as parameters

(<void> core_load)

Loads debug game state from core\core.bin.

(<void> core_load_at_startup)

Loads debug game state from core\core.bin as soon as the map is initialized.

(<void> core_load_name <string>)

Loads debug game state from core\<path>.

(<void> core_load_name_at_startup <string>)

Loads debug game state from core\<path> as soon as the map is initialized.

(<void> core_save)

Saves debug game state to core\core.bin.

(<boolean> core_save_name <string>)

Saves debug game state to core\<path> in pre-H1A versions of the game. In H1A this function was hijacked for campaign segments.

(<void> crash <string>)
(crash "Something is wrong")

Crashes (for debugging)

(<boolean> custom_animation <unit> <animation_graph> <string> <boolean>)

Starts a custom animation playing on a unit (interpolates into animation if last parameter is true)

(<boolean> custom_animation_list <object_list> <animation_graph> <string> <boolean>)

Starts a custom animation playing on a unit list (interpolates into animation if last parameter is true)

(<void> damage_new <damage> <cutscene_flag>)
(damage_new "scenery\emitters\burning_flame\flame" enter_lava_flag)

Causes the specified damage at the specified flag

(<void> damage_object <damage> <object>)
(damage_object "weapons\assault rifle\bullet" (player0))

Causes the specified damage at the specified object

(<void> deactivate_nav_point_flag <unit> <cutscene_flag>)

Deactivates a nav point type attached to a player <unit> anchored to a flag

(<void> deactivate_nav_point_object <unit> <object>)

Deactivates a nav point type attached to a player <unit> anchored to an object

(<void> deactivate_team_nav_point_flag <team> <cutscene_flag>)

Deactivates a nav point type attached to a team anchored to a flag

(<void> deactivate_team_nav_point_object <team> <object>)

Deactivates a nav point type attached to a team anchored to an object

(<void> debug_camera_load)

Loads the saved camera position and facing

(<void> debug_camera_load_name <string>)

loads the camera position and facing from <name>_<map_name>.txt

(<void> debug_camera_load_simple_name <string>)

loads the camera position and facing from camera_<name>.txt

(<void> debug_camera_load_text <string>)

loads the camera position and facing from a passed in string

(<void> debug_camera_save)

Saves the camera position and facing

(<void> debug_camera_save_name <string>)

saves the camera position and facing to <name>_<map_name>.txt

(<void> debug_camera_save_simple_name <string>)

saves the camera position and facing to camera_<name>.txt

(debug_memory)

Dumps memory leaks

(debug_memory_by_file)

Dumps memory leaks by source file

(debug_memory_for_file <string>)
(debug_memory_for_file "\halopc\haloce\source\tag_files\tag_groups.c")

Dumps memory leaks from the specified source file

(<void> debug_pvs <boolean>)

Calling this function with true enables both the debug_portals and structures_use_pvs_for_vs globals, which has the effect of enabling portal borders and rendering all faces in the PVS. Call with false to disable these globals.

(debug_sounds_distances <string> <real> <real>)

Changes the minimum and maximum distances for all sound classes matching the substring

(<void> debug_sounds_enable <string> <boolean>)

Enables or disabled all sound classes matching the substring

(debug_sounds_wet <string> <real>)

Changes the reverb level for all sound classes matching the substring

(debug_tags)

Writes all memory being used by tag files into tag_dump.txt

(<void> debug_teleport_player <short> <short>)

Given two local player indices, teleports the first to the second if they both exist.

(delete_save_game_files)

Delete all custom profile files

(device_get_position <device>)

Gets the current position of the given device (used for devices without explicit device groups)

(<real> device_get_position <device>)

Gets the current position of the given device (used for devices without explicit device groups)

(<real> device_get_power <device>)

Gets the current power of a named device

(<void> device_group_change_only_once_more_set <device_group> <boolean>)

true allows a device to change states only once

(<real> device_group_get <device_group>)

Returns the desired value of the specified device group

(<boolean> device_group_set <device_group> <real>)

Changes the desired value of the specified device group

(<void> device_group_set_immediate <device_group> <real>)

Instantaneously changes the value of the specified device group

(<void> device_one_sided_set <device> <boolean>)

true makes the given device one-sided (only able to be opened from one direction), false makes it two-sided

(<void> device_operates_automatically_set <device> <boolean>)

true makes the given device open automatically when any biped is nearby, false makes it not

(<void> device_set_never_appears_locked <device> <boolean>)

changes a machine's never_appears_locked flag, but only if paul is a bastard

(<boolean> device_set_position <device> <real>)
(device_set_position <device> 1.0)

Set the desired position of the given device (used for devices without explicit device groups)

(<void> device_set_position_immediate <device> <real>)
(device_set_position_immediate <device> 1.0)

Instantaneously changes the position of the given device (used for devices without explicit device groups

(<void> device_set_power <device> <real>)
(device_set_power <device> 1.0)

Immediately sets the power of a named device to the given value

(disconnect)

Disconnect from a server

(<void> display_scenario_help <short>)
(display_scenario_help 1)

Display in-game help dialog

(<void> effect_new <effect> <cutscene_flag>)
(effect_new "effects\coop teleport" teleporting_flag)

Starts the specified effect at the specified flag

(<void> effect_new_on_object_marker <effect> <object> <string>)
(effect_new_on_object_marker "effects\burning large" warthog_mp "driver")

Starts the specified effect on the specified object at the specified marker

(<void> enable_hud_help_flash <boolean>)
(enable_hud_help_flash true)
(enable_hud_help_flash false)

Starts/stops the help text flashing

(<void> error_overflow_suppression <boolean>)
(error_overflow_suppression true)
(error_overflow_suppression false)

Enables or disables the suppression of error spamming

(<void> fade_in <real> <real> <real> <short>)
(fade_in 0.0 0.0 0.0 100)

Does a screen fade in from a particular color in the amount of ticks

(<void> fade_out <real> <real> <real> <short>)
(fade_out 1.0 1.0 1.0 100)

Does a screen fade out to a particular color in the amount of ticks

(fast_setup_network_server <string> <string> <boolean>)

Sets up a network server with the given map name, game variant, and true for remote connections, false for not

(<boolean> game_all_quiet)

Returns false if there are bad guys around, projectiles in the air, etc.

(<game_difficulty> game_difficulty_get)

Returns the current difficulty setting, but lies to you and will never return easy, instead returning normal

(<game_difficulty> game_difficulty_get_real)

Returns the actual current difficulty setting without lying

(<void> game_difficulty_set <game_difficulty>)
(game_difficulty_set easy)
(game_difficulty_set normal)
(game_difficulty_set hard)
(game_difficulty_set impossible)

Changes the difficulty setting for the next map to be loaded

(<boolean> game_is_authoritative)

(<boolean> game_is_cooperative)

Returns true if the game is cooperative

(<void> game_lost)

Causes the player to revert to their previous saved checkpoint. For example, this is used when Keyes dies in Truth and Reconciliation.

(<void> game_revert)

Reverts to last saved game, if any (for testing, the first bastard that does this to me gets it in the head)

(<boolean> game_reverted)

Don't use this for anything, you black-hearted bastards

(<boolean> game_safe_to_save)

Returns false if it would be a bad idea to save the player's game right now

(<boolean> game_safe_to_speak)

Returns false if it would be a bad idea to save the player's game right now

(<void> game_save)

Checks to see if it is safe to save game, then saves (gives up after 8 seconds)

(<void> game_save_cancel)

Cancels any pending game_save, timeout or not. This prevents a checkpoint from being created during a known loss situation.

(<void> game_save_no_timeout)

Checks to see if it is safe to save game, then saves (this version never gives up)

(<void> game_save_totally_unsafe)

Saves disregarding player's current situation

(<boolean> game_saving)

Checks to see if the game is trying to save a checkpoint

(<void> game_skip_ticks <short>)
(game_skip_ticks 5)

Skips <short> amount of game ticks. ONLY USE IN CUTSCENES!!!

(<void> game_speed <real>)
(game_speed 0.5)

Changes the game speed. Values above 20 are likley to be uncontrollable. This function doesn't have an effect in H1A but you can now use the game_paused and game_speed_value globals instead.

(<long> game_time)

Gets ticks elapsed since the start of the game (when the map loaded). This is not authoritative game time in the case of multiplayer since clients can join at different times.

(<long> game_time_authoritative)

Gets ticks elapsed since the start of the game (relative to the sim authority).

(<void> game_variant <string>)
(game_variant ctf)
(game_variant team_oddball)
(game_variant none)

Sets the multiplayer game engine, which will take effect the next time the map is loaded with map_name and apply to all subsequent map loads. See game modes for available engines. You can reset the desired game mode to singleplayer by providing a string which is not a valid game mode, e.g. none.

(<void> game_won)

Causes the player to successfully finish the current level and move on to the next

(<void> garbage_collect_now)

Causes all non-visible garbage objects like dead bipeds and garbage to be garbage collected (removed) immediately rather than on a timer. You can enable debug_object_garbage_collection for statistics.

(get_digital_forward_throttle <short>)

Gets the amount of forward throttle applied by digital device stimuli

(get_digital_pitch_increment <short>)

Gets the increment in pitch applied by digital device stimuli

(get_digital_strafe_throttle <short>)

Gets the amount of strafe throttle applied by digital device stimuli

(get_digital_yaw_increment <short>)

Gets the increment in yaw applied by digital device stimuli

(get_gamepad_forward_threshold <short>)

Gets the threshold beyond which gamepad movement is full forward throttle

(get_gamepad_pitch_scale <short>)

Gets the scale for gamepad control of pitch

(get_gamepad_strafe_threshold <short>)

Gets the threshold beyond which gamepad movement is full strafe throttle

(get_gamepad_yaw_scale <short>)

Gets the scale for gamepad control of yaw

(get_mouse_forward_threshold <short>)

Gets the threshold beyond which mouse movement is full forward throttle

(get_mouse_pitch_scale <short>)

Gets the scale for mouse control of pitch

(get_mouse_strafe_threshold <short>)

Gets the threshold beyond which mouse movement is full strafe throttle

(get_mouse_yaw_scale <short>)

Gets the scale for mouse control of yaw

(<real> get_pitch_rate <short>)

Gets the yaw rate for the given player number

(<real> get_yaw_rate <short>)

Gets the yaw rate for the given player number

(hammer_begin <string> <string> <long> <short> <short>)

Hammers the server by connecting and disconnecting repeatedly

(hammer_stop)

Stops hammering the server

(<void> help <string>)
(help cheats_load)

Prints a description of the named function

(<void> hud_blink_health <boolean>)
(hud_blink_health true)
(hud_blink_health false)

Starts/stops manual blinking of the health panel

(<void> hud_blink_motion_sensor <boolean>)
(hud_blink_motion_sensor true)
(hud_blink_motion_sensor false)

Starts/stops manual blinking of the motion sensor panel

(<void> hud_blink_shield <boolean>)
(hud_blink_shield true)
(hud_blink_shield false)

Starts/stops manual blinking of the shield panel

(<void> hud_clear_messages)

Clears all non-state messages on the hud

(<short> hud_get_timer_ticks)

Returns the ticks left on the hud timer

(<void> hud_help_flash_restart)

Resets the timer for the help text flashing

(<void> hud_set_help_text <hud_message>)

displays <message> as the help text

(<void> hud_set_objective_text <hud_message>)

sets <message> as the current objective

(<void> hud_set_timer_position <short> <short> <hud_corner>)

sets the timer upper left position to (x, y)=>(<short>, <short>)

(<void> hud_set_timer_time <short> <short>)

sets the time for the timer to <short> minutes and <short> seconds, and starts and displays timer

(<void> hud_set_timer_warning_time <short> <short>)

sets the warning time for the timer to <short> minutes and <short> seconds

(<void> hud_show_crosshair <boolean>)

hides/shows the weapon crosshair

(<void> hud_show_health <boolean>)

hides/shows the health panel

(<void> hud_show_motion_sensor <boolean>)

hides/shows the motion sensor panel

(<void> hud_show_shield <boolean>)

hides/shows the shield panel

(hud_team_background_set_pos <long> <long>)

shit

(hud_team_background_set_scale <real> <real>)

shit

(hud_team_icon_set_pos <long> <long>)

shit

(hud_team_icon_set_scale <real> <real>)

shit

(input_activate_joy <short> <short>)

Activates an enumerated joystick into a logical joystick slot

(input_deactivate_joy <short>)

Deactivates an enumerated joystick, freeing up the logical joystick slot

(input_find_default <string>)

Test function that looks up a default profile for a deviceid

(input_find_joystick <string>)

Test function to find a joystick by GUID (string representation)

(input_get_joy_count)

Test function to return the number of joysticks enumerated

(input_is_joy_active <short>)

Test function to determine if an enumerated joystick is activated or not

(input_show_joystick_info)

Test function to show the enumerated joystick information for all joystick

(<void> inspect <expression>)
(inspect (+ 3 4))
; outputs: 7

Prints the value of an expression to the screen for debugging purposes.

(<short> list_count <object_list>)
(list_count the_warthogs)

Returns the number of objects in a list

(<short> list_count_not_dead <object_list>)

returns the number of objects in a list that aren't dead

(<object> list_get <object_list> <short>)
(list_get the_warthogs 3)

Returns an item in an object list

(<object_list> local_players)

returns a list of the living player units on the local machine

(<void> log_print <string>)

prints a string to the hs log file.

(<void> magic_melee_attack)

Causes player's unit to start a melee attack

(<void> magic_seat_name <string>)

All units controlled by the player will assume the given seat name (valid values are 'asleep', 'alert', 'stand', 'crouch' and 'flee')

(<void> map_name <string>)
; in cache builds:
(map_name "a10")
; in tags builds:
(map_name "levels\a10\a10")

Loads a singleplayer map by scenario name. If using the H1A standalone build, which directly loads tags instead of cache files, use the scenario tag path instead.

(<void> map_reset)

Starts the map from the beginning

(<boolean> mcc_mission_segment <string>)
(mcc_mission_segment "03_escape") ;from a10
(mcc_mission_segment "04_first_shoot") ;from a10

Signals a mission segment being reached to MCC. Usually done at the same time as a checkpoint (game_save). H1A MCC only.

(message_metrics_clear)

Clears network messaging metrics

(message_metrics_dump <string>)
(message_metrics_dump "")

Dumps network messaging metrics to given file ("" for default)

(<void> multiplayer_map_name <string>)
(multiplayer_map_name "schwinnzno1_alpha01a")

Changes the name of the multiplayer map

(network_client_dump)

Dumps info on network client

(network_server_dump)

Dumps info on network server

(net_graph_clear)

Clears the net_graph

(net_graph_show <string> <string>)

Changes the net_graph display (bytes/packets, sent/received)

(<short> numeric_countdown_timer_get <short>)
(numeric_countdown_timer_get 1)
(numeric_countdown_timer_get -1)

<digit_index>

(<void> numeric_countdown_timer_restart)

Reset the timer

(<void> numeric_countdown_timer_set <long> <boolean>)
(numeric_countdown_timer_set 15500 false)
(numeric_countdown_timer_set 10000 false)

<milliseconds>, <auto_start>

(<void> numeric_countdown_timer_stop)

Stop the timer

(<void> objects_attach <object> <string> <object> <string>)
(objects_attach chief "right hand" ar1 "")

Attaches the second object to the first; both strings can be empty

(<boolean> objects_can_see_flag <object_list> <cutscene_flag> <real>)
(objects_can_see_flag (players) tunnel_flag 45)

Returns true if any of the specified units are looking within the specified number of degrees of the flag. This is a simple direction test; obstructions like scenery or the BSP are not considered blockers.

(<boolean> objects_can_see_object <object_list> <object> <real>)
(objects_can_see_object (player0) the_warthog 90)

Returns true if any of the specified units are looking within the specified number of degrees of the object. This is a simple direction test; obstructions like scenery or the BSP are not considered blockers.

(<void> objects_delete_by_definition <object_definition>)

Deletes all objects of type <definition>

(<void> objects_detach <object> <object>)
(objects_detach chief ar1)

Detaches from the given parent object the given child object

(<real> objects_distance_to_flag <object_list> <cutscene_flag>)

returns minimum distance from any of the specified objects to the specified flag. (returns -1 if there are no objects, or no flag, to check)

(<real> objects_distance_to_object <object_list> <object>)

returns minimum distance from any of the specified objects to the specified destination object. (returns -1 if either of the objects does not exist)

(<real> objects_distance_to_position <object_list> <real> <real> <real>)

returns minimum distance from any of the specified objects to the specified position. (returns -1 if there are no objects to check)

(<void> objects_dump_memory)

debugs object memory usage

(<void> objects_predict <object_list>)
(objects_predict the_bipeds)

Loads textures necessary to draw objects that are about to come on-screen, using the object's predicted resources.

(<void> object_beautify <object> <boolean>)
(object_beautify chief true)
(object_beautify chief false)

Makes an object use its highest quality LOD for the remainder of the levels' cutscenes.

(<void> object_cannot_take_damage <object_list>)
(object_cannot_take_damage (players))

Prevents an object from taking damage

(<void> object_can_take_damage <object_list>)
(object_can_take_damage (players))

Allows an object to take damage again

(<void> object_create <object_name>)
(object_create warthog_mp_1)

Creates an object from the scenario

(<void> object_create_anew <object_name>)
(object_create_anew banshee_mp_1)

Creates an object, destroying it first if it already exists

(<void> object_create_anew_containing <string>)
(object_create_anew_containing "pelican")

Creates anew all objects from the scenario whose names contain the given substring

(<void> object_create_containing <string>)
(object_create_containing "warthog")

Creates all objects from the scenario whose names contain the given substring

(<void> object_destroy <object>)

Destroys an object

(<void> object_destroy_all)

Destroys all non player objects

(<void> object_destroy_containing <string>)
(object_destroy_containing "pelican")

Destroys all objects from the scenario whose names contain the given substring

(<void> object_pvs_activate <object>)

Just another (old) name for object_pvs_set_object.

(<void> object_pvs_clear)

Removes the special place that activates everything it sees.

(<void> object_pvs_set_camera <cutscene_camera_point>)

Sets the specified cutscene camera point as the special place that activates everything it sees.

(<void> object_pvs_set_object <object>)

Sets the specified object as the special place that activates everything it sees. Make sure to use object_pvs_clear before switching BSPs or you'll trigger the assertion EXCEPTION halt in c:\mcc\qfe1\h1\code\h1a2\sources\structures\structure_bsp_definitions.c,#39: cluster_index>=0 && cluster_index<structure_bsp->clusters.count

(<void> object_set_collideable <object> <boolean>)
(object_set_collideable (player0) true)
(object_set_collideable (player0) false)

false prevents any object from colliding with the given object

(<void> object_set_facing <object> <cutscene_flag>)
(object_set_facing (player0) blue_base_flag)

Turns the specified object in the direction of the specified flag

(<void> object_set_melee_attack_inhibited <object> <boolean>)
(object_set_melee_attack_inhibited (player0) true)
(object_set_melee_attack_inhibited (player0) false)

false prevents object from using melee attack

(<void> object_set_permutation <object> <string> <string>)
(object_set_permutation (player0) "right arm" ~damaged)

Sets the desired region (use "" for all regions) to the permutation with the given name

(<void> object_set_ranged_attack_inhibited <object> <boolean>)
(object_set_ranged_attack_inhibited (player0) true)
(object_set_ranged_attack_inhibited (player0) false)

false prevents object from using ranged attack

(<void> object_set_scale <object> <real> <short>)
(object_set_scale (player0) 1.5 10)
(object_set_scale insertion_pelican 0.25 30)

sets the scale for a given object and interpolates over the given number of frames to achieve that scale

(<void> object_set_shield <object> <real>)
(object_set_shield (player0) 1.0)

Sets the shield vitality of the specified object (between 0 and 1)

(<void> object_teleport <object> <cutscene_flag>)
(object_teleport (player0) red_base_flag)

Moves the specified object to the specified flag

(<void> object_type_predict <object_definition>)

Loads textures necessary to draw an object that's about to come on-screen

(<void> pause_hud_timer <boolean>)
(pause_hud_timer true)
(pause_hud_timer false)

Pauses or unpauses the hud timer

(<void> physics_constants_reset)

Resets all physics constants to earthly values.

(<real> physics_get_gravity)

Get the current global gravity acceleration relative to Halo standard gravity.

(<void> physics_set_gravity <real>)

Set global gravity acceleration relative to Halo standard gravity. The change in gravity is NOT network synchronized.

(<void> playback)

Starts game in film playback mode

(<boolean> player0_joystick_set_is_normal)

Returns true if (player0) is using the normal joystick set

(<void> player0_look_invert_pitch <boolean>)

Invert player0's look

(<boolean> player0_look_pitch_is_inverted)

Returns true if (player0)'s look pitch is inverted

(<object_list> players)

returns a list of the players

(<object_list> players_on_multiplayer_team <short>)

returns a list of the living player units on the MP team

(<void> players_unzoom_all)

Resets zoom levels on all players

(<boolean> player_action_test_accept)

Returns true if any player has hit accept since the last call to (player_action_test_reset)

(<boolean> player_action_test_action)

Returns true if any player has hit the action key since the last call to (player_action_test_reset)

(<boolean> player_action_test_back)

Returns true if any player has hit the back key since the last call to (player_action_test_reset)

(<boolean> player_action_test_grenade_trigger)

Returns true if any player has used grenade trigger since the last call to (player_action_test_reset)

(<boolean> player_action_test_jump)

Returns true if any player has jumped since the last call to (player_action_test_reset)

(<boolean> player_action_test_look_relative_all_directions)

Returns true if any player has looked up, down, left, and right since the last call to (player_action_test_reset)

(<boolean> player_action_test_look_relative_down)

Returns true if any player has looked down since the last call to (player_action_test_reset)

(<boolean> player_action_test_look_relative_left)

Returns true if any player has looked left since the last call to (player_action_test_reset)

(<boolean> player_action_test_look_relative_right)

Returns true if any player has looked right since the last call to (player_action_test_reset)

(<boolean> player_action_test_look_relative_up)

Returns true if any player has looked up since the last call to (player_action_test_reset)

(<boolean> player_action_test_move_relative_all_directions)

Returns true if any player has moved forward, backward, left, and right since the last call to (player_action_test_reset)

(<boolean> player_action_test_primary_trigger)

Returns true if any player has used primary trigger since the last call to (player_action_test_reset)

(<void> player_action_test_reset)

Resets the player action test state so that all tests will return false

(<boolean> player_action_test_zoom)

Returns true if any player has hit the zoom button since the last call to (player_action_test_reset)

(<void> player_add_equipment <unit> <starting_profile> <boolean>)

Adds/resets the player's health, shield, and inventory (weapons and grenades) to the named profile. Resets if third parameter is true, adds if false.

(<boolean> player_camera_control <boolean>)
(player_camera_control true)
(player_camera_control false)

Enables/disables camera control globally

(<void> player_effect_set_max_rotation <real> <real> <real>)

<yaw> <pitch> <roll>

(<void> player_effect_set_max_rumble <real> <real>)

This is not a real HSC function, but rather a hard-coded alias for player_effect_set_max_vibrate present in pre-H1A season 7 versions of Halo.

(<void> player_effect_set_max_translation <real> <real> <real>)

<x> <y> <z>

(<void> player_effect_set_max_vibrate <real> <real>)

<left> <right>

(<void> player_effect_start <real> <real>)

<max_intensity> <attack time>

(<void> player_effect_stop <real>)

<decay>

(<void> player_enable_input <boolean>)
(player_enable_input true)
(player_enable_input false)

Toggle player input. The player can still free-look, but nothing else. If the player unit has been attached to an AI encounter, the AI will "take over" control if this is disabled.

(play_update_history <long> <boolean>)

Playback client input history starting from the specified last completed update id

(<void> print <string>)
(print "50 dollars for this?!")

Prints a string to the console. Printed text will not appear in the console unless devmode is enabled (devmode 4). You can give a print call a format string in H1CE (e.g. "I have %d apples"), but cannot give it format arguments, meaning a format string can cause the function to read invalid memory and crash the game!. The string is no longer interpreted as a format string in H1A.

(<void> print_binds)

Prints a list of all input bindings

(<void> print_if <boolean> <string>)

prints a string to the console if the condition is true.

(profile_activate <string>)

Activates profile sections based on a substring

(profile_deactivate <string>)

Deactivates profile sections based on a substring

(profile_dump <string>)

Dumps profile based on a substring

(profile_graph_toggle <string>)

Enables or disables profile graph display of a particular value

(<void> profile_load <string>)
(profile_load "a hobo")
; loads the profile "a hobo"

Load any included builtin profiles and create profiles on disk

(profile_reset)

Resets profiling data

(profile_service_clear_timers)

Clears the timers that are present in the profiling service

(profile_service_dump_timers)

Dumps the profiling service timers

(profile_unlock_solo_levels)

Unlocks all the solo player levels for player 1's profile

(<void> quit)

Quits the game

(<void> radiosity_debug_point)

tests sun occlusion at a point.

(<void> radiosity_save)

saves radiosity solution.

(<void> radiosity_start)

starts radiosity computation.

(<short> random_range <short> <short>)

returns a random value in the range [lower bound, upper bound)

(<void> rasterizer_decals_flush)

Destroys all dynamic and permanent decals. You shouldn't use this to optimize your map's framerate because it doesn't preserve decorative environmental decals. Instead, set reasonable lifetimes for your dynamic decals and limit the number of decals created by custom effects.

(<void> rasterizer_fixed_function_ambient <long>)
(rasterizer_fixed_function_ambient 200)

Set the ambient light value for fixed function. Removed in H1A since there is no longer a fixed function render pipeline.

(<void> rasterizer_fps_accumulate)

Average fps

(<void> rasterizer_lights_reset_for_new_map)

(<void> rasterizer_model_ambient_reflection_tint <real> <real> <real> <real>)

(<void> rasterizer_reload_effects)

Check for shader changes

(rcon [rcon password] [command])

Sends a command for server to execute at console. Use " to send quotes.

(<real> real_random_range <real> <real>)

returns a random value in the range [lower bound, upper bound)

(<void> recording_kill <unit>)
(recording_kill (player0))

Kill the specified unit's cutscene recording

(<boolean> recording_play <unit> <cutscene_recording>)

Make the specified unit run the specified cutscene recording

(<boolean> recording_play_and_delete <unit> <cutscene_recording>)

Make the specified unit run the specified cutscene recording, deletes the unit when the animation finishes

(<boolean> recording_play_and_hover <vehicle> <cutscene_recording>)

Make the specified vehicle run the specified cutscene recording, hovers the vehicle when the animation finishes

(<short> recording_time <unit>)
(recording_time (player0))

Return the time remaining in the specified unit's cutscene recording

(<void> reload_shader_transparent_chicago)

Reloads all shader_transparent_chicago tags. Use this in Sapien or Standalone to see changes to their bitmaps or parameters without having to reload the scenario.

(remote_player_stats <string>)

Displays the prediction stats of the specified remote player.

(<void> render_effects <boolean>)
(render_effects true)
(render_effects false)

Render game effects if true

(<boolean> render_lights <boolean>)
(render_lights true)
(render_lights false)

Enables/disables dynamic lights

(<void> scenery_animation_start <scenery> <animation_graph> <string>)
(scenery_animation_start fighter_clouds "cinematics\animations\h_fighter\x70\x70" "x70_3")

Starts a custom animation playing on a piece of scenery.

(<void> scenery_animation_start_at_frame <scenery> <animation_graph> <string> <short>)
(scenery_animation_start_at_frame fighter_launch "cinematics\animations\h_fighter\x70\x70" "x70_2" 100)

Starts a custom animation playing on a piece of scenery at a specific frame.

(<short> scenery_get_animation_time <scenery>)
(scenery_get_animation_time fighter_launch)

Returns the number of ticks/frames remaining in a custom animation (or zero, if the animation is over). This function intentionally returns a value 2 frames lower than the actual remaining frame count so scripts waiting for the animation to end can do something before it's over.

(<void> screenshot_cubemap <string>)

Takes a cubemap screenshot from the camera's point of view and saves as <name>.tif. It can then be used to import a cubemap bitmap tag.

(<void> script_doc)

Saves a file called hs_doc.txt with parameters for all script commands. In the H1A-EK this also includes external globals at the end of the file.

(<void> script_recompile)

Recompiles scripts

(<void> script_screen_effect_set_value <short> <real>)

Sets a screen effect script value

(<passthrough> set <variable name> <expression>)
(set bsl_sucks true)

Sets the value of a defined global variable.

(set_digital_forward_throttle <short> <real>)

Sets the amount of forward throttle applied by digital device stimuli

(set_digital_pitch_increment <short> <real>)

Sets the increment in pitch applied by digital device stimuli

(set_digital_strafe_throttle <short> <real>)

Sets the amount of strafe throttle applied by digital device stimuli

(set_digital_yaw_increment <short> <real>)

Sets the increment in yaw applied by digital device stimuli

(set_gamepad_forward_threshold <short> <real>)

Sets the threshold beyond which gamepad movement is full forward throttle

(set_gamepad_pitch_scale <short> <real>)

Sets the scale for gamepad control of pitch

(set_gamepad_strafe_threshold <short> <real>)

Sets the threshold beyond which gamepad movement is full strafe throttle

(set_gamepad_yaw_scale <short> <real>)

Sets the scale for gamepad control of yaw

(<void> set_gamma <long>)
(set_gamma 200)

Set the gamma. Removed in H1A.

(set_mouse_forward_threshold <short> <real>)

Sets the threshold beyond which mouse movement is full forward throttle

(set_mouse_pitch_scale <short> <real>)

Sets the scale for mouse control of pitch

(set_mouse_strafe_threshold <short> <real>)

Sets the threshold beyond which mouse movement is full strafe throttle

(set_mouse_yaw_scale <short> <real>)

Sets the scale for mouse control of yaw

(<void> set_pitch_rate <short> <real>)

Sets the yaw rate for the given player number

(<void> set_yaw_rate <short> <real>)

Sets the yaw rate for the given player number

(<boolean> show_hud <boolean>)
(show_hud true)
(show_hud false)

Shows or hides the hud

(<boolean> show_hud_help_text <boolean>)
(show_hud_help_text true)
(show_hud_help_text false)

Shows or hides the hud help text

(<void> show_hud_timer <boolean>)
(show_hud_timer true)
(show_hud_timer false)

Displays the hud timer

(show_player_update_stats)

Shows update history playback stats

(<void> sound_cache_dump_to_file)

(<void> sound_cache_flush)

(<void> sound_class_set_gain <string> <real> <short>)

Changes the gain on the specified sound class(es) to the specified game over the specified number of ticks.

(<boolean> sound_eax_enabled)

Returns true if EAX extensions are enabled

(<void> sound_enable <boolean>)
(sound_enable true)
(sound_enable false)

Enables or disables all sound

(<void> sound_enable_eax <boolean>)
(sound_enable_eax true)
(sound_enable_eax false)

Enable or disable EAX extensions

(<void> sound_enable_hardware <boolean> <boolean>)

Enable or disable hardware sound buffers

(<real> sound_get_effects_gain)

Returns the game's effects gain

(<real> sound_get_gain <string>)

Absolutely do not use this either

(<real> sound_get_master_gain)

Returns the game's master gain

(<real> sound_get_music_gain)

Returns the game's music gain

(<short> sound_get_supplementary_buffers)

Get the amount of supplementary buffers

(<void> sound_impulse_predict <sound> <boolean>)
(sound_impulse_predict "sound\sfx\impulse\ting\ting" true)
(sound_impulse_predict "sound\sfx\impulse\ting\ting" false)

Loads an impulse sound into the sound cache ready for playback

(<void> sound_impulse_start <sound> <object> <real>)

Plays an impulse sound from the specified source object (or "none"), with the specified scale. Note that the sound may not play under certain situations, like if its sound class is scripted_dialog_player and the player is dead.

(<void> sound_impulse_stop <sound>)

Stops the specified impulse sound

(<long> sound_impulse_time <sound>)

Returns the time remaining for the specified impulse sound

(<void> sound_looping_predict <looping_sound>)

(<void> sound_looping_set_alternate <looping_sound> <boolean>)

Enables or disables the alternate loop/alternate end for a looping sound

(<void> sound_looping_set_scale <looping_sound> <real>)

Changes the scale of the sound (which should affect the volume) within the range 0 to 1

(<void> sound_looping_start <looping_sound> <object> <real>)

Plays a looping sound from the specified source object (or "none"), with the specified scale

(<void> sound_looping_stop <looping_sound>)

Stops the specified looping sound

(<void> sound_set_effects_gain <real>)
(sound_set_effects_gain 2.0)

Set the game's effects gain

(<void> sound_set_env <short>)
(sound_set_env 1)

Change environment preset

(<void> sound_set_factor <real>)

Set the DSound factor value

(<void> sound_set_gain <string> <real>)

Absolutely do not use this

(<void> sound_set_master_gain <real>)
(sound_set_master_gain 0.5)

Set the game's master gain

(<void> sound_set_music_gain <real>)
(sound_set_music_gain 0.5)

Set the game's music gain. This should be in the range 0-1. Setting it outside this range will crash Standalone.

(<void> sound_set_rolloff <real>)

Set the DSound rolloff value

(<void> sound_set_supplementary_buffers <short> <boolean>)

Set the amount of supplementary buffers

(<short> structure_bsp_index)

Returns the current structure bsp index

(<void> structure_lens_flares_place)

Places lens flares in the structure bsp

(<void> switch_bsp <short>)
(switch_bsp 0)

Switches to a different structure bsp

(<void> TestPrintBool <string> <boolean>)

Prints the specified boolean with the format <string> = <boolean> to the Shell. Currently this does not work.

(<void> TestPrintReal <string> <real>)

Prints the specified real with the format <string> = <real> to the Shell. Currently this does not work.

(texture_cache_flush)

Don't make me kick your ass

(thread_sleep <long>)

Sleeps the calling thread for the specified number of ms.

(<void> time_code_reset)

Resets the time code timer

(<void> time_code_show <boolean>)
(time_code_show true)
(time_code_show false)

Shows the time code timer

(<void> time_code_start <boolean>)
(time_code_start true)
(time_code_start false)

Starts/stops the time code timer

(track_remote_player_position_updates <string>)

Sets the name of the remote player whose position update are to be tracked.

(<void> ui_widget_show_path <boolean>)
(ui_widget_show_path true)
(ui_widget_show_path false)

(<void> unbind <string> <string>)

Unbinds an input device/button combination

(<unit> unit <object>)
(unit (list_get (players) 0))

Converts an object to a unit

(<void> units_set_current_vitality <object_list> <real> <real>)
(units_set_current_vitality (players) 75 75)

Sets a group of units' current body and shield vitality

(<void> units_set_desired_flashlight_state <object_list> <boolean>)
(units_set_desired_flashlight_state (players) true)
(units_set_desired_flashlight_state (players) false)

Sets the units' desired flashlight state

(<void> units_set_maximum_vitality <object_list> <real> <real>)
(units_set_maximum_vitality (players) 75 75)

Sets a group of units' maximum body and shield vitality

(<void> unit_aim_without_turning <unit> <boolean>)

Allows a unit to aim in place without turning

(<void> unit_can_blink <unit> <boolean>)

Allows a unit to blink or not (units never blink when they are dead)

(<void> unit_close <unit>)

Closes the hatches on a given unit

(<boolean> unit_custom_animation_at_frame <unit> <animation_graph> <string> <boolean> <short>)

Starts a custom animation playing on a unit at a specific frame index (interpolates into animation if next to last parameter is true).

(<void> unit_doesnt_drop_items <object_list>)
(unit_doesnt_drop_items (players))

Prevents any of the given units from dropping weapons or grenades when they die

(<void> unit_enter_vehicle <unit> <vehicle> <string>)
(unit_enter_vehicle (player0) warthog_mp_2 "gunner")

Puts the specified unit in the specified vehicle (in the named seat)

(<void> unit_exit_vehicle <unit>)

Makes a unit exit its vehicle

(<boolean> unit_get_current_flashlight_state <unit>)
(unit_get_current_flashlight_state (player0))

Gets the unit's current flashlight state

(<short> unit_get_custom_animation_time <unit>)
(unit_get_custom_animation_time chief_insertion)

Returns the number of ticks remaining in a unit's custom animation (or zero, if the animation is over). This function intentionally returns a value 2 frames lower than the actual remaining frame count so scripts waiting for the animation to end can do something before it's over.

(<real> unit_get_health <unit>)
(unit_get_health (player0))

Returns the health of the given unit as a real between 0 and 1]. Returns -1 if the unit does not exist.

(<real> unit_get_shield <unit>)
(unit_get_shield (player0))

Returns the shield of the given unit as a real between 0 and 1. Returns -1 if the unit does not exist.

(<short> unit_get_total_grenade_count <unit>)
(unit_get_total_grenade_count (player0))

Returns the total number of grenades for the given unit, or 0 if the unit does not exist.

(<boolean> unit_has_weapon <unit> <object_definition>)
(unit_has_weapon (player0) plasma_cannon)

Returns true if the <unit> has <object> as a weapon, false otherwise

(<boolean> unit_has_weapon_readied <unit> <object_definition>)
(unit_has_weapon_readied (player0) plasma_cannon)

Returns true if the <unit> has <object> as the primary weapon, false otherwise

(<void> unit_impervious <object_list> <boolean>)
(unit_impervious (players) true)
(unit_impervious (players) false)

Prevents any of the given units from being knocked around or playing ping animations

(<boolean> unit_is_playing_custom_animation <unit>)

Returns true if the given unit is still playing a custom animation

(<void> unit_kill <unit>)

Kills a given unit, no saving throw. This will crash pre-H1A MCC versions of the game if the unit doesn't exist.

(<void> unit_kill_silent <unit>)

Kills a given unit silently (doesn't make them play their normal death animation or sound). This will crash pre-H1A MCC versions of the game if the unit doesn't exist.

(<void> unit_open <unit>)

Opens the hatches on the given unit

(<void> unit_set_current_vitality <unit> <real> <real>)

Sets a unit's current body and shield vitality

(<void> unit_set_desired_flashlight_state <unit> <boolean>)
(unit_set_desired_flashlight_state (player0) true)
(unit_set_desired_flashlight_state (player0) false)

Turns the unit's flashlight on or off

(<void> unit_set_emotion <unit> <short>)

Sets a unit's facial expression (-1 is none, other values depend on unit)

(<void> unit_set_emotion_animation <unit> <string>)

Sets the emotion animation to be used for the given unit

(<void> unit_set_enterable_by_player <unit> <boolean>)
(unit_set_enterable_by_player warthog_mp_3 true)
(unit_set_enterable_by_player warthog_mp_3 false)

Can be used to prevent the player from entering a vehicle

(<void> unit_set_maximum_vitality <unit> <real> <real>)

Sets a unit's maximum body and shield vitality

(<void> unit_set_seat <unit> <string>)
(unit_set_seat (player0) "alert")

This unit will assume the named seat, which forces them into a certain pose if the unit supports it (e.g. 'alert', 'stand', 'crouch', 'noncombat').

(<boolean> unit_solo_player_integrated_night_vision_is_active)

Returns whether the night-vision mode could be activated via the flashlight button

(<void> unit_stop_custom_animation <unit>)

Stops the custom animation running on the given unit

(<void> unit_suspended <unit> <boolean>)
(unit_suspended (player0) true)
(unit_suspended (player0) false)

Stops gravity from working on the given unit

(<unit> vehicle_driver <unit>)
(vehicle_driver the_ghost)

Returns the driver of a vehicle

(<unit> vehicle_gunner <unit>)
(vehicle_gunner the_warthog)

Returns the gunner of a vehicle

(<void> vehicle_hover <vehicle> <boolean>)
(vehicle_hover "vehicles\warthog\warthog" true)
(vehicle_hover "vehicles\warthog\warthog" false)

Stops the vehicle from running real physics and runs fake hovering physics instead

(<short> vehicle_load_magic <unit> <string> <object_list>)
(vehicle_load_magic pelican_1 "" (players))

Makes a list of units (named or by encounter) magically get into a vehicle, in the substring-specified seats (e.g. "CD-passenger". Empty string matches all seats).

(<object_list> vehicle_riders <unit>)
(vehicle_riders the_tanks)

Returns a list of all riders in a vehicle

(<boolean> vehicle_test_seat <vehicle> <string> <unit>)
(vehicle_test_seat example_ghost "G-driver" (player0))

Tests whether the named seat has a specified unit in it. This accepts a seat label which is not case sensitive.

(<boolean> vehicle_test_seat_list <vehicle> <string> <object_list>)
(vehicle_test_seat_list warthog5 "W-driver" (players))

Tests whether the named seat has an object in the object list. This accepts a seat label which is not case sensitive.

(<short> vehicle_unload <unit> <string>)
(vehicle_unload hog "W-driver")

Makes units get out of a vehicle from the substring-specified seats (e.g. "CD-passenger". Empty string matches all seats). The seat label is not case sensitive.

(<void> version)

Prints the build version

(<void> volume_teleport_players_not_inside <trigger_volume> <cutscene_flag>)
(volume_teleport_players_not_inside hidden_trigger omgtelprot)

Moves all players outside a specified trigger volume to a specified flag

(<boolean> volume_test_object <trigger_volume> <object>)
(volume_test_object trig_volume1 (player0))

Returns true if the specified object is within the specified volume

(<boolean> volume_test_objects <trigger_volume> <object_list>)
(volume_test_objects trig_volume2 (players))

Returns true if any of the specified objects are within the specified volume

(<boolean> volume_test_objects_all <trigger_volume> <object_list>)
(volume_test_objects_all trig_volume2 (players))

Returns true if all of the specified objects are within the specified volume

External globals

External globals are globals which belong to the engine itself, as opposed to declared in level scripts, are are typically used to toggle debug features. If manipulating them via scripts, you need to use set. From the console you can just set them like this:

rasterizer_fog_atmosphere false

A · B · C · D · E · F · G · H · L · M · N · O · P · R · S · T · U · V · W (499 total)

Global

(ai_debug_ballistic_lineoffire_freeze [boolean])

If enabled, the ballistic arc drawn by ai_render_ballistic_lineoffire will not update when new grenades are thrown.

(ai_debug_blind)

(ai_debug_communication_focus_enable)

(ai_debug_communication_random_disabled)

(ai_debug_communication_timeout_disabled)

(ai_debug_communication_unit_repeat_disabled)

(ai_debug_deaf)

(ai_debug_disable_wounded_sounds)

(ai_debug_evaluate_all_positions)

(ai_debug_fast_los)

(ai_debug_flee_always)

(ai_debug_force_all_active)

(ai_debug_force_crouch)

(ai_debug_force_vocalizations)

(ai_debug_ignore_player)

(ai_debug_invisible_player)

(ai_debug_oversteer_disable)

(ai_debug_path)

(ai_debug_path_accept_radius)

(ai_debug_path_attractor)

(ai_debug_path_attractor_radius)

(ai_debug_path_attractor_weight)

(ai_debug_path_disable_obstacle_avoidance)

(ai_debug_path_disable_smoothing)

(ai_debug_path_end_freeze)

(ai_debug_path_flood)

(ai_debug_path_maximum_radius)

(ai_debug_path_start_freeze)

(ai_fix_actor_variants)

(ai_fix_defending_guard_firing_positions)

(ai_print_acknowledgement)

(ai_print_allegiance)

(ai_print_automatic_migration)

(ai_print_bsp_transition)

(ai_print_command_lists)

(ai_print_communication)

(ai_print_communication_player)

(ai_print_conversations)

(ai_print_damage_modifiers [boolean])

When an encounter is selected, damage modifiers are logged at the bottom of the screen.

(ai_print_evaluation_statistics)

(ai_print_killing_sprees)

(ai_print_lost_speech)

(ai_print_major_upgrade)

(ai_print_migration)

(ai_print_oversteer)

(ai_print_placement)

(ai_print_pursuit_checks)

(ai_print_respawn)

(ai_print_rules)

(ai_print_rule_values)

(ai_print_scripting)

(ai_print_secondary_looking)

(ai_print_speech [boolean])

Displays red text over AI whenever they vocalize, with the name of the dialogue field played. For example, pain body minor.

(ai_print_speech_timers)

(ai_print_surprise)

(ai_print_uncovering)

(ai_print_unfinished_paths)

(ai_print_vocalizations [boolean])

Prints vocalizations to the console as they happen, including the encounter, squad, actor, dialogue type (e.g. exclaim), and line. For example, beach_lz/camp_center_grunt/grunt: talk flee [72/flee].

(ai_profile_disable)

(ai_profile_random)

(ai_render [boolean])

Toggles the display of any enabled AI debug overlays. Defaults to true.

(ai_render_activation)

(ai_render_active_cover_seeking)

(ai_render_aiming_validity)

(ai_render_aiming_vectors)

(ai_render_all_actors)

(ai_render_audibility)

(ai_render_ballistic_lineoffire [boolean])

Toggles the rendering of ballistic aiming arcs when AI throw grenades only (does not include hunter guns, wraiths, or other ballistic weapons). The arc is shown in green when unobstructed and orange when obstructed by an object or the BSP. Its origin point and vector are shown in yellow. Only a single arc is shown at a time and updates whenever a new grenade is thrown. This can be paused with ai_debug_ballistic_lineoffire_freeze.

(ai_render_burst_geometry)

(ai_render_charge_decisions)

(ai_render_control)

(ai_render_current_state)

(ai_render_danger_zones)

(ai_render_detailed_state)

(ai_render_dialogue_variants [boolean])

Shows pink text overlays over each unit with dialogue showing which variant they use, like if marines are a "mendoza" or a "bisenti". For example, variant 11 dialogue 0 aussie.

(ai_render_emotions)

(ai_render_encounter_activeregion)

(ai_render_evaluations)

(ai_render_firing_positions)

(ai_render_grenade_decisions)

(ai_render_gun_positions)

(ai_render_idle_look)

(ai_render_inactive_actors)

(ai_render_lineoffire)

(ai_render_lineoffire_crouching)

(ai_render_lineofsight)

(ai_render_melee_check)

(ai_render_paths)

(ai_render_paths_avoidance_obstacles)

(ai_render_paths_avoidance_search)

(ai_render_paths_avoidance_segment)

(ai_render_paths_avoided)

(ai_render_paths_current)

(ai_render_paths_destination)

(ai_render_paths_failed)

(ai_render_paths_nodes)

(ai_render_paths_nodes_all)

(ai_render_paths_nodes_closest)

(ai_render_paths_nodes_costs)

(ai_render_paths_nodes_polygons)

(ai_render_paths_raw)

(ai_render_paths_selected_only)

(ai_render_paths_smoothed)

(ai_render_player_aiming_blocked)

(ai_render_player_ratings)

(ai_render_postcombat)

(ai_render_projectile_aiming)

(ai_render_props [boolean])

Toggles the display of how many props each actor has in green. If ai_render_props_web is enabled this switches from a total to being split out by type, e.g. friend or enemy.

(ai_render_props_no_friends [boolean])

Hides prop lines for friends if enabled, which can make seeing enemy props easier.

(ai_render_props_target_weight)

(ai_render_props_unopposable)

(ai_render_props_unreachable)

(ai_render_props_web [boolean])

Toggles the display of props as a web of lines.

(ai_render_pursuit)

(ai_render_recent_damage)

(ai_render_secondary_looking)

(ai_render_shooting)

(ai_render_spatial_effects)

(ai_render_speech)

(ai_render_states)

(ai_render_support_surfaces)

(ai_render_targets)

(ai_render_targets_last_visible)

(ai_render_teams)

(ai_render_threats)

(ai_render_trigger)

(ai_render_vector_avoidance)

(ai_render_vector_avoidance_avoid_t)

(ai_render_vector_avoidance_clear_time)

(ai_render_vector_avoidance_intermediate)

(ai_render_vector_avoidance_objects)

(ai_render_vector_avoidance_rays)

(ai_render_vector_avoidance_sense_t)

(ai_render_vector_avoidance_weights)

(ai_render_vehicles_enterable)

(ai_render_vehicle_avoidance)

(ai_render_vision_cones)

(ai_render_vitality)

(ai_show)

(ai_show_actors)

(ai_show_line_of_sight)

(ai_show_paths)

(ai_show_prop_types)

(ai_show_sound_distance)

(ai_show_stats)

(ai_show_swarms [boolean])

Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms.

(allow_client_side_weapon_projectiles)

(allow_out_of_sync)

(biped_incremental_rate [short])

Unknown purpose. Default value is 7.

(breadcrumbs_navpoints_enabled_override [boolean])

Enables on-demand/revised nav points like MCC. Off by default. Newly added to the MCC tools in the July 2023 CU.

(breakable_surfaces [boolean])

Enables or disables the breakable surfaces effect. If disabled, breakable surfaces will simply disappear rather than shatter.

(cheat_bottomless_clip [boolean])

Prevents player weapons from generating heat and depleting ammo rounds. Battery will still be depleted.

(cheat_bump_possession [boolean])

Allows the player to "possess" other characters by walking into them. The player will then be able to control the possessed character and see the world from its view.

(cheat_controller [boolean])

If enabled, the game loads a cheats.txt file which binds controller buttons to console commands. Known only to work in Halo beta versions.

(cheat_deathless_player [boolean])

Prevents players from being killed, including all multiplayer clients if enabled on the server. Although the players can still take damage, being reduced to 0 health or falling long distances will not kill them. This also prevents players from being killed with unit_kill.

(cheat_infinite_ammo [boolean])

Prevents weapons from depleting battery or reserve ammo. Weapons will still generate heat. Magazine-based weapons will still empty their current magazine, but reloading will not use up any reserve ammo.

(cheat_jetpack [boolean])

If enabled, players take no fall damage. In MCC, holding crouch while mid-air will also cause the player to hover and holding jump will cause you to fly.

(cheat_medusa [boolean])

Causes enemy AI to be killed instantly if they "look" at or become aware of the player. This does not include allies like Marines, even after allegiance has been broken by killing them. It is unknown if the command is hard-coded to kill certain AI types.

(cheat_omnipotent [boolean])

The player will instantly kill any unit they damage, including destroying vehicles. Even multiplayer vehicles can be "killed" and rendered inoperable.

(cheat_reflexive_damage_effects [boolean])

Any damage_effect applied to other characters, even dead ones, will appear on the player's screen regardless of who applied the damage. For example, if an Elite shoots a flood infection form, the player will see their screen flash blue and damage direction indicators appear. The player takes no actual damage. This can be helpful for testing visual effects.

(cheat_super_jump [boolean])

Players jump to an extreme height, and will die of fall damage if cheat_jetpack or cheat_deathless_player are not used. This can be used to quickly reach areas when testing maps.

(client_log_destination [long])

Unknown purpose.

(cl_remote_player_action_queue_limit [long])

Unknown purpose. May be used to limit how many total actions from clients are allowed to be queued on the server. Defaults to 2.

(cl_remote_player_action_queue_tick_limit [long])

Unknown purpose. May be used to limit how long actions from clients are allowed to be queued on the server before they are ignored. Defaults to 6.

(collision_debug [boolean])

If enabled, a ray is continually shot from the camera (by default) to troubleshoot ray-object and ray-BSP collisions. A red normal-aligned marker will be shown where the ray collides with a surface. The collision surface itself, whether BSP or model, will be outline in red. Information about the collision surface will be shown in the top left corner of the screen, including plane and surface indices from the BSP structure, material type, and how many degrees the surface's normal differs from vertical.

The types of surfaces which the test ray hits or ignores can be toggled with the collision_debug_flag_* commands. The maximum range of the ray is controlled with collision_debug_length.

This feature can be frozen in place with collision_debug_repeat and also moved directly with the collision_debug_point_* and collision_debug_vector_* globals. A red ray is visible in these situations when the ray is no longer shot from the camera's point of view.

(collision_debug_features [boolean])

Toggles the display of collision features near the camera, which can be spheres (red), cylinders (blue), or prisms (green). Collision size can be adjusted with collision_debug_width and collision_debug_height. The test point can be frozen in place using collision_debug_repeat.

(collision_debug_flag_back_facing_surfaces [boolean])

When collision_debug or collision_debug_spray are enabled, causes the test rays to collide with back-facing surfaces (those facing away from the camera). Defaults to false.

(collision_debug_flag_front_facing_surfaces [boolean])

When collision_debug or collision_debug_spray are enabled, causes the test rays to collide with front-facing surfaces (those facing towards the camera). Defaults to true. Disabling this will have no effect unless collision_debug_flag_back_facing_surfaces is also enabled.

(collision_debug_flag_ignore_breakable_surfaces [boolean])

Toggles if collision debug rays ignore breakable surfaces. Defaults to false.

(collision_debug_flag_ignore_invisible_surfaces [boolean])

Toggles if collision debug rays ignore invisible surfaces (e.g. collision-only player clipping). Defaults to true.

(collision_debug_flag_ignore_two_sided_surfaces [boolean])

Toggles if collision debug rays ignore two-sided surfaces. Defaults to false.

(collision_debug_flag_media [boolean])

Toggles if collision debug rays collide with water surfaces.

(collision_debug_flag_objects [boolean])

Toggles if collision debug rays collide with any class of object's collision geometry. This setting will be ignored if any more specific object flag is enabled, such as collision_debug_flag_objects_equipment.

(collision_debug_flag_objects_bipeds [boolean])

Toggles if collision debug rays collide with bipeds.

(collision_debug_flag_objects_controls [boolean])

Toggles if collision debug rays collide with device_control.

(collision_debug_flag_objects_equipment [boolean])

Toggles if collision debug rays collide with equipment.

(collision_debug_flag_objects_light_fixtures [boolean])

Toggles if collision debug rays collide with device_light_fixture.

(collision_debug_flag_objects_machines [boolean])

Toggles if collision debug rays collide with device_machine.

(collision_debug_flag_objects_placeholders [boolean])

Toggles if collision debug rays collide with placeholders. This would require a custom placeholder to have an effect, since the placeholder tags that come with the HEK have no collision model.

(collision_debug_flag_objects_projectiles [boolean])

Toggles if collision debug rays collide with projectiles. Note that most projectiles do not have a collision model.

(collision_debug_flag_objects_scenery [boolean])

Toggles if collision debug rays collide with scenery.

(collision_debug_flag_objects_vehicles [boolean])

Toggles if collision debug rays collide with vehicles.

(collision_debug_flag_objects_weapons [boolean])

Toggles if collision debug rays collide with weapons.

(collision_debug_flag_skip_passthrough_bipeds [boolean])

Unknown purpose. Does not seem to affect collision ray tests against bipeds.

(collision_debug_flag_structure [boolean])

Toggles if collision debug rays collide with the structure BSP. Collisions with model_collision_geometry BSPs are unaffected.

(collision_debug_flag_try_to_keep_location_valid [boolean])

Unknown purpose.

(collision_debug_flag_use_vehicle_physics [boolean])

If enabled, collision debug rays will collide with vehicle physics spheres rather than their model_collision_geometry.

(collision_debug_height <real>)

When and collision_debug_features is enabled, controls the height in world units of collision features. Defaults to 0.0.

(collision_debug_length [real])

Sets the maximum test ray length for collision_debug and collision_debug_spray in world units. When the ray reaches this maxmimum, a floating green marker will be shown for collision_debug while spray rays will simply not be shown. Defaults to 100.0.

(collision_debug_phantom_bsp [boolean])

Causes a floating pink cube and label "phantom bsp" to appear whenever a test ray from the center of the screen intersects with phantom BSP. It can be helpful to pair this with collision_debug_spray.

(collision_debug_point_x [real])

Represents the current origin of the collision_debug test ray. While collision_debug is active, this value will be continually updated with the camera's location unless collision_debug_repeat is also enabled. In repeat mode, you are able to set this global to move the ray origin to any point you need. See also collision_debug_vector_i.

(collision_debug_point_y [real])

See collision_debug_point_x.

(collision_debug_point_z [real])

See collision_debug_point_x.

(collision_debug_repeat [boolean])

Setting this to true will freeze the test rays and points for collision_debug, collision_debug_phantom_bsp, collision_debug_spray, and collision_debug_features, allowing you to move and view the debug information from another angle or manually set the origin and direction with the collision_debug_point_* and collision_debug_vector_* globals.

(collision_debug_spray [boolean])

Setting this to true will cause collision ray tests to be performed in a dense grid from the viewport. This operates independently of the collision_debug setting, and only the destination hit markers are shown. Can be affected by collision flags, length, and frozen with collision_debug_repeat.

(collision_debug_vector_i [real])

Represents the current direction of the collision_debug test ray as ijk vector components. While collision_debug is active, this value will be continually updated with the camera's direction unless collision_debug_repeat is also enabled. In repeat mode, you are able to set this global to orient the ray as needed.

(collision_debug_vector_j [real])

See collision_debug_vector_i.

(collision_debug_vector_k [real])

See collision_debug_vector_i.

(collision_debug_width [real])

When collision_debug and collision_debug_features are enabled, controls the width in world units of collision features. Defaults to 0.0.

(collision_log_detailed)

(collision_log_extended)

(collision_log_render)

(collision_log_time)

(collision_log_totals_only)

(console_dump_to_file)

(controls_enable_crouch [boolean])

No visible effect. May be related to controller input during development.

(controls_enable_doubled_spin [boolean])

No visible effect. May be related to controller input during development.

(controls_swapped [boolean])

No visible effect. May be related to controller input during development.

(controls_swap_doubled_spin_state [boolean])

No visible effect. May be related to controller input during development.

(debug_bink)

(debug_biped_limp_body_disable [boolean])

When set to true, pauses the biped limp body system, which is responsible for moving model nodes towards the ground when an eligible biped has died and has come to rest on the structure BSP. Bipeds will lay roughly at the right angle, but they will not conform to the shape of the surfaces below them. If set to false, the limp body system will take effect again even for existing dead bipeds.

(debug_biped_physics [boolean])

For this to be visible, collision_debug must also be enabled which has the side-effect of making the collision_debug feature itself unusable until the game is restarted. This displays several markers and vectors within the player's physics pill (debug_objects_biped_physics_pill) which can be more easily observed in third person and with framerate_throttle 1. The number and colours of the debug overlays can vary by which map is loaded and it's not known why.

(debug_biped_skip_collision [boolean])

If true, disables collision checks for bipeds. They will be able to keep "walking" horizontally, but cannot jump or collide with any objects the BSP, and are unaffected by gravity. They will still be forced to stay crouched if crouching below or within a collideable surface.

(debug_biped_skip_update)

(debug_bsp [boolean])

Toggles the display of structure BSP node traversal for the camera location. At each level, the node and plane indices are shown as well as a + or - symbol indicating if the camera was on the front or back side of the plane.

(debug_camera [boolean])

Shows debug information about the camera in the top left corner, including:

3D coordinates in world units
BSP leaf indices
Cluster indices, e.g. (#2 [2]). If the camera leaves the BSP then the first index will be -1 while the second index tracks the last known valid cluster. This is what allows the game to keep rendering part of the BSP while the camera is outside it.
Ground point coordinates and BSP surface index (point directly below the camera on the ground)
Facing direction (yaw angle)
Tag path of the shader pointed at by the camera. The surface must be collideable and this feature cannot be configured with collision debug flags.

(debug_collision_skip_objects [boolean])

Disables collision tests against objects. For example, projectiles will pass through scenery and bipeds through non-moving vehicles. Vehicle-to-vehicle collision still happens, as does moving vehicle against biped. Everything still collides with the BSP.

(debug_collision_skip_vectors [boolean])

Globally disables vector/ray collision tests, with the following observed effects:

Projectiles, particles and moving items will pass through everything, even the BSP.
Melees will have no effect.
Vehicle suspension, such as the Warthog's wheels, will hang.
Object lighting will be unable to determine the ground point below the object when it moves, resulting in incorrect lighting and shadow direction.
Sounds behind obstructions will not be muffled.

(debug_damage [boolean])

When enabled, looking at any collideable object and pressing Space will display the object's body and shield vitalities on the HUD.

(debug_damage_taken [boolean])

Logs damage to the player as messages at the bottom of the screen. Includes body and shield vitality and the damage source.

(debug_decals)

Displays red numbers over each dynamic and permanent decal in the environment. The mesh of the most recently created decal (initially a permanent decal if one exists, then any new dynamic one) will also be highlighted so you can see how it conforms around the BSP.

White points indicate the original 4 corners of the decal, while red ones were added during the conformation to the BSP. The meaning of the number is not known definitely, but seems to be the number of vertices needed if we assume each projected decal region must be converted into quads.

(debug_detail_objects [boolean])

When enabled, active detail object cells will be outlined in blue and individual detail objects are highlighted with red markers.

(debug_effects_nonviolent)

(debug_fog_planes [boolean])

Outlines the edges of fog plane volumes with white lines.

(debug_framerate [boolean])

No visible effect. Replaced with the Ctrl + F12 hotkey?

(debug_frustum [boolean])

Draws a series of red lines between corners and midpoints of the screen within the view frustrum. These can be seen to intersect with level geometry.

(debug_game_save)

(debug_inactive_objects)

(debug_input [boolean])

Displays a series of ticks at the top of the screen which do not seem to be affected by player input. May be a broken feature?

(debug_input_target)

(debug_leaf_index)

(debug_leaf_portals)

(debug_leaf_portal_index)

(debug_lights [boolean])

Shows orange and white spheres with the radius of each dynamic light. White seems to show when a light is not yet been activated, such as the Warthog's brake lights until their first use. Lens flare only lights are not shown since their radius is 0.

(debug_looping_sound [boolean])

Displays active sound_looping with cyan spheres for their maxmimum distance and blue spheres for their minimum.

(debug_material_effects [boolean])

Displays cyan spheres wherever material_effects are being generated, like under the player's feet and where physics spheres intersect with the BSP or model_collision_geometry.

(debug_motion_sensor_draw_all_units)

(debug_no_drawing [boolean])

If enabled, completely stops the game from rendering new frames. This includes preventing the rendering of the developer console itself, so disabling this feature can be tricky. If you think the console is still open, press Up to re-enter the previous command, then replace its final argument with true or 1 and press Enter to resume rendering.

(debug_no_frustum_clip [boolean])

Disables portal-based occlusion culling for objects and BSP faces (use rasterizer_wireframe 1 to see this). In addition to the PVS, which determines the set of clusters which are potentially visible, groups of faces and objects within those clusters can still be culled further using the limited view from the camera's location through the series of portals leading to those clusters (a portal-diminished frustum). The game uses object bounding spheres for this and likely subcluster bounding boxes.

Disabling this type of culling does not cause the game to ignore the PVS and it will still cull entire clusters if no portals to them are on-screen. To force the game to render all clusters and subclusters in the PVS, enable structures_use_pvs_for_vs.

(debug_objects [boolean])

When enabled, toggles if debug information is visible on objects (such as bounding sphere and collision models). Individual debug features can be toggled with the debug_objects_* commands. In H1A this now has debug_objects_root_node enabled by default; turn it off if you don't want the orange text.

(debug_objects_biped_autoaim_pills [boolean])

When debug_objects is enabled, displays biped autoaim pills in red.

(debug_objects_biped_messages)

(debug_objects_biped_physics_pills [boolean])

When debug_objects is enabled, displays biped physics pills in white.

(debug_objects_bounding_spheres [boolean])

When debug_objects is enabled, displays yellow spheres for object bounding radius. The sphere will be black when the object is inactive. This setting defaults to true in HEK Sapien but not H1A Sapien.

(debug_objects_collision_models [boolean])

When debug_objects is enabled, displays green meshes for object collision models. This setting defaults to true in HEK Sapien but not H1A Sapien.

(debug_objects_devices)

(debug_objects_equipment_messages)

(debug_objects_names [boolean])

Toggles the display of object names, for named objects. The names are shown in purple.

(debug_objects_pathfinding_spheres [boolean])

When debug_objects is enabled, displays object pathfinding spheres in blue.

(debug_objects_physics)

When debug_objects is enabled, displays physics mass points as white spheres.

(debug_objects_position_velocity)

When debug_objects is enabled, displays red, green, and blue object-space reference axes and a yellow velocity vector on each object.

(debug_objects_projectile_messages)

(debug_objects_root_node)

When debug_objects is enabled, displays red, green, and blue object-space reference axes and orange text including object ID, class, and tag name on each object. Defaults to true in H1A.

(debug_objects_unit_mouth_apeture)

(debug_objects_unit_seats)

When debug_objects is enabled, displays blue markers at unit seat locations, red markers at their entry points, and a yellow marker at the object origin.

(debug_objects_unit_vectors)

When debug_objects is enabled, displays white and red vectors on objects. Their meaning is unknown.

(debug_objects_vehicle_messages)

(debug_objects_vehicle_powered_mass_points)

No visible effect, even with debug_objects_physics enabled.

(debug_objects_weapon_messages)

(debug_object_garbage_collection)

When garbage_collect_now is run, enabling this causes information to print to the console about the total number of objects, garbage objects, and how many objects were garbage collected.

(debug_object_lights)

Shows the incoming light colour and vector for all objects which results from sampling lightmap data at the ground point beneath the object. This data is used to shade the object and cast its shadow.

(debug_obstacle_path)

(debug_obstacle_path_goal_point_x)

(debug_obstacle_path_goal_point_y)

(debug_obstacle_path_goal_surface_index)

(debug_obstacle_path_on_failure)

(debug_obstacle_path_start_point_x)

(debug_obstacle_path_start_point_y)

(debug_obstacle_path_start_surface_index)

(debug_permanent_decals)

Toggles the display of yellow bounding spheres around each permanent decal in the environment.

(debug_physics_disable_penetration_freeze)

(debug_player)

(debug_player_color [short])

Sets the player's armour color in non-team games. The setting will take effect when the player respawns. The default value -1 causes the player's profile setting to be used.

(debug_player_teleport [boolean])

Displays a green physics pill where a co-op player would spawn if safe.

(debug_point_physics [boolean])

Renders green or red markers wherever point_physics are being simulated. This includes flags, antenna, contrails, particles, and particle_systems. For weather_particle_system, markers are only shown in their simulation cube and while the weather global not disabled.

Red markers indicate point_physics with the collides with structures flag, which are more computationally expensive.

It can help to enable framerate_throttle for these markers to render consistently.

(debug_portals [boolean])

Draws BSP portals as red outlines. You may wish to pair this with rasterizer_wireframe 1 to help you understand how portals result in culling parts of the BSP from rendering. The related function debug_pvs will enable/disable this global and structures_use_pvs_for_vs.

(debug_recording)

(debug_recording_newlines)

(debug_render_freeze [boolean])

Freezes the rendering viewport at the current camera location. The camera may still move after frozen, but all portal-based culling, skybox origin, FP models, and some debug overlays will still be based on the previously frozen camera location. This command is useful for inspecting portal behaviour where the camera cannot directly see.

(debug_score [long])

Unknown purpose. Does not affect the player's multiplayer score.

(debug_scripting [boolean])

Displays a table of active script threads, with their name, sleep time, and currently executing function.

(debug_sound [boolean])

Sound sources will be labeled in 3D with their tag path and a their minimum and maximum distances shown as red and yellow spheres, respectively.

(debug_sound_cache [boolean])

If enabled, sound cache statistics will be shown in the top left corner of the screen, including how full it is.

(debug_sound_cache_graph [boolean])

Shows a graph of sound cache slots at the top of the screen, similar to texture_cache_graph. Can be used in Sapien/debug builds.

(debug_sound_channels [boolean])

Displays the utilization of sound channel limits in the top left corner of the screen.

(debug_sound_channels_detail [boolean])

Displays two columns of number pairs while sounds play with an unknown meaning.

(debug_sound_environment [boolean])

If enabled, shows the tag path of the cluster's current sound_environment.

(debug_sound_hardware [boolean])

Unknown purpose.

(debug_sprites [boolean])

Renders 2D sprite effects like particles and weather_particle_system with white triangle outlines. This also displays some sprite statistics at the top of the screen (coverage and big sprites count).

(debug_structure [boolean])

When enabled, all scenario_structure_bsp collision surfaces will be rendered with green outlines. A red bounding box surrounds renderable surfaces.

(debug_structure_automatic [boolean])

Like debug_structure, but only appears when the camera is in "solid" space (outside the BSP). This can be handy for finding your way back to the level when switching BSPs. It is set to true by default in Sapien. This is a backported feature from later Halos and only available in the newer MCC tools.

(debug_texture_cache [boolean])

If enabled, causes red messages to appear in the HUD which are related to texture and/or sound cache events. Exact meaning unknown.

(debug_trigger_volumes [boolean])

Renders all scenario trigger volumes and their names.

(debug_unit_all_animations [boolean])

Logs lines to the console output as unit animations occur. For example: cyborg_mp: animation stand pistol move-right.

(debug_unit_animations [boolean])

Shows red console log output whenever a unit animation is missing. For example: MISSING: cyborg 'G-driver unarned aim-still'.

(debug_unit_illumination)

(developer_mode)

(director_camera_switching)

(director_camera_switch_fast)

(disconnect)

Force-disconnects from the current multiplayer session and returns to the menu. This can be handy for quickly leaving a server after a game has ended without having to wait for the "Quit" option in the post-game lobby.

(display_framerate [boolean])

Displays the current framerate in green in the bottom-right corner of the screen.

(display_precache_progress)

(effects_corpse_nonviolent [boolean])

Toggles if shooting bodies produces additional blood effects.

(equipment_incremental_rate)

(error_suppress_all)

(f0)

(f1)

(f2)

(f3)

(f4)

(f5)

(find_all_fucked_up_shit)

(force_all_player_views_to_default_player)

(framerate_lock)

(framerate_throttle [boolean])

Limits rendering to 30 FPS.

(freeze_flying_camera)

(game_paused [boolean])

Pauses the game world simulation. Backported from later titles for H1A.

(game_speed_value [real])

Sets the speed of the game simulation, with 1.0 being the default rate. Rates above 20 should be avoided. This global is new to H1A and should be used in place of the game_speed function.

(global_connection_dont_timeout)

(hud_filter [boolean])

Unknown purpose.

(leaf_to_leaf_latency [boolean])

If enabled, causes the game to perform server latency analysis. Periodically, 20 samples will be taken and statistics will appear on the screen.

(local_player_log_level)

(local_player_update_rate)

(local_player_vehicle_update_rate)

(log_server_player_update_history)

(loud_dialog_hack)

(model_animation_bullshit0)

(model_animation_bullshit1)

(model_animation_bullshit2)

(model_animation_bullshit3)

(model_animation_compression)

(model_animation_data_compressed_size)

(model_animation_data_compression_savings_in_bytes)

(model_animation_data_compression_savings_in_bytes_at_import)

(model_animation_data_compression_savings_in_percent)

(model_animation_data_uncompressed_size)

(mouse_acceleration [real])

Controls the amount of mouse input acceleration. Set to 0 for none. Defaults to 0.7.

(multiplayer_draw_teammates_names)

(multiplayer_hit_sound_volume [real])

Sets the gain of the hit sound heard when damage is dealt in multiplayer. Defaults to 0.2.

(network_connect_timeout)

(net_bandwidth)

(net_graph_enabled)

(net_graph_period)

(object_light_ambient_base)

Sets the amount of ambient light all objects receive. Note that this only affects moving objects when their lighting updates, so you will not see any change on scenery. Setting this to 1 makes objects fullbright. Defaults to 0.03.

(object_light_ambient_scale)

Scales ambient light from the lightmap. Defaults to 0.4.

(object_light_interpolate)

Toggles if object lighting transitions smoothly when the object moves between different ground point surfaces or default lighting.

(object_light_secondary_scale)

Scales secondary light on objects, but doesn't apply to default lighting. Defaults to 1.

(object_prediction)

(oddball_baseline_rate)

(pad3)

(pad3_scale)

(player_autoaim)

(player_magnetism [boolean])

Enables or disables the magnetism aim assist for controllers.

(player_spawn_count)

(profile_display [boolean])

Displays profiling and budget information in the upper-left of the screen, including object, effects, particles, AI encounters, collision tests, and more. You may find it useful to open the console while using this feature in order to stop the game simulation.

(profile_dump_frames)

(profile_dump_lost_frames)

(profile_graph [boolean])

Broken feature -- causes a crash.

(profile_timebase_ticks)

(projectile_incremental_rate)

(rasterizer_active_camouflage [boolean])

Toggles the active camouflage distortion effect. When disabled, objects with active camouflage are rendered as they normally would without the effect.

(rasterizer_active_camouflage_multipass [boolean])

Toggles whether or not transparent shaders are shown through active camouflage. If disabled, shaders like glass or lights will not be visible through a camouflaged unit.

(rasterizer_bump_mapping [boolean])

Toggles bump mapping on the BSP, affecting both environmental bump lighting and specular reflections. If you only want to toggle bump lighting, use rasterizer_lightmaps_incident_radiosity.

(rasterizer_d3dlight_attenuation0)

(rasterizer_d3dlight_attenuation1)

(rasterizer_d3dlight_attenuation2)

(rasterizer_d3dlight_falloff)

(rasterizer_d3dlight_phi)

(rasterizer_d3dlight_theta)

(rasterizer_debug_geometry)

(rasterizer_debug_geometry_multipass)

(rasterizer_debug_meter_shader)

(rasterizer_debug_model_lod [short])

Forces certain object LODs to be used. A value of 4 is the highest quality, and 0 the worst. The default value -1 returns to automatic behaviour.

(rasterizer_debug_model_vertices)

(rasterizer_debug_shader_transparent_generic [boolean])

Disabling this silences "generic shader has no maps or stages" warnings in the console. Newly added to the MCC tools in the July 2023 CU.

(rasterizer_debug_transparents)

(rasterizer_detail_objects [boolean])

Toggles the rendering of detail objects.

(rasterizer_detail_objects_offset_multiplier [boolean])

Defaults to 0.4. No visible effect on detail objects when changed.

(rasterizer_draw_first_person_weapon_first [boolean])

Controls whether the first person weapon/arms is rendered before or after the rest of the scene. Defaults to true. If set to false, parts of the FP model can be occluded by nearby objects because they fail the depth test against the depth buffer. This is easily seen in the Warthog passenger seat or with the sniper rifle against scenery.

Typically, the FP view draws first and also writes to a stencil buffer which is later used to prevent the background from overlapping the FP view. You can disable rasterizer_stencil_mask for the same effect as disabling this global.

(rasterizer_DXTC_noise)

(rasterizer_dynamic_lit_geometry)

(rasterizer_dynamic_screen_geometry)

(rasterizer_dynamic_unlit_geometry)

(rasterizer_effects_level)

(rasterizer_environment)

(rasterizer_environment_alpha_testing [boolean])

Toggles alpha testing for BSP shader_environment. These shaders are rendered opaquely when disabled.

(rasterizer_environment_decals)

Toggles the display and creation of both permanent and dynamic decals. While false, effects cannot create new decals but previous ones will reappear when reset to true.

(rasterizer_environment_diffuse_lights [boolean])

Toggles the rendering of dynamic light diffuse illumination on the BSP. Does not affect specular highlights.

(rasterizer_environment_diffuse_textures [boolean])

Disables diffuse textures in the BSP, showing just the lightmap shading and specular components.

(rasterizer_environment_fog)

Toggles both environmental sky fog and fog plane colors. Does not affect fog screen. Use rasterizer_fog_plane or rasterizer_fog_atmosphere to individually toggle fog types.

(rasterizer_environment_fog_screen [boolean])

Toggles the cloudy fog screen effect of fog planes, seen in the levels a30, c10, and c40.

(rasterizer_environment_lightmaps [boolean])

Toggles the rendering of structure BSP lightmaps. When disabled, the level will be completely invisible.

(rasterizer_environment_reflections [boolean])

Toggles specular reflections in the BSP.

(rasterizer_environment_reflection_lightmap_mask)

(rasterizer_environment_reflection_mirrors [boolean])

Toggles the rendering of dynamic mirrors.

(rasterizer_environment_shadows [boolean])

Toggles dynamic shadow mapping for objects. Same effect as render_shadows.

(rasterizer_environment_specular_lightmaps)

(rasterizer_environment_specular_lights)

(rasterizer_environment_specular_mask)

(rasterizer_environment_transparents)

(rasterizer_far_clip_distance [real])

Sets the far clip distance, which is the maximum draw distance (world units). Defaults to 1024.0.

(rasterizer_filthy_decal_fog_hack)

(rasterizer_first_person_weapon_far_clip_distance [real])

Sets the far clip distance, for the first person arms and weapon. Defaults to 1024.0. The world clipping distance can be set with rasterizer_far_clip_distance.

(rasterizer_first_person_weapon_near_clip_distance [real])

Sets the near clip distance of the first person arms and weapon. Defaults to 0.011719.

(rasterizer_floating_point_zbuffer)

(rasterizer_fog_atmosphere [boolean])

Toggles atmospheric fog as defined in the active sky tag.

(rasterizer_fog_plane [boolean])

Toggles the rendering of fog planes.

(rasterizer_fps)

(rasterizer_framerate_stabilization)

(rasterizer_framerate_throttle)

(rasterizer_frame_bounds_bottom)

(rasterizer_frame_bounds_left)

(rasterizer_frame_bounds_right)

(rasterizer_frame_bounds_top)

(rasterizer_frame_drop_ms)

(rasterizer_hud_motion_sensor [boolean])

Toggles the yellow and red dots seen in the motion sensor.

(rasterizer_lens_flares [boolean])

Toggles rendering of all lens flares.

(rasterizer_lens_flares_occlusion [boolean])

Toggles lens flare occlusion. If set to false, lens flares will no longer be occluded and stay visible even through objects in the foreground.

(rasterizer_lens_flares_occlusion_debug [boolean])

Displays red squares over lens flares in the environment. How much the square is occluded by other geometry or the view frustrum is how much the lens flare fades out. The size of the square relates to the occlusion radius.

(rasterizer_lightmaps_filtering [boolean])

Enables or disables texture filtering for lightmaps. When disabled, lightmaps will appear blocky and jagged. Has no effect in H1A.

(rasterizer_lightmaps_incident_radiosity [boolean])

Toggles directional environmental bump mapped lighting. Does not affect the sampling of stored incident radiosity for object shadows.

(rasterizer_lightmap_ambient [real])

Sets the amount of ambient light when rendering the BSP in fullbright mode (like when radiosity has not yet been baked or rasterizer_lightmap_mode 2) This defaults to 1.0. It has no effect on the normal lightmap rendering mode.

(rasterizer_lightmap_mode [short])

Changes the rendering mode of lightmaps:

Mode	Description
0	Normal (default).
1	BSP specular reflections will not be multiplied by the lightmap.
2	Fullbright mode. You can set the ambient light with rasterizer_lightmap_ambient.
3	Colours BSP surfaces by what lightmap bitmap index they use (technically, with a random colour seeded by the lightmap address). It can help to disable rasterizer_environment_diffuse_textures and rasterizer_environment_reflections to see this more clearly.
4	Fullbright without environmental bump map lighting. Ambient light is not configurable in this mode.

Any higher value will appear the same as 1. Repeated changing of this value can cause degraded performance and eventually a crash.

(rasterizer_mode [short])

Changes rendering mode of the level and its objects:

Mode	Description
0	Normal (default).
1	Additive blending.
2	Disables specular on the BSP (like rasterizer_environment_reflections).

(rasterizer_models [boolean])

Toggles the rendering of all models. When disabled, all objects like scenery, units, projectiles, and even the skybox and FP arms will become invisible. The BSP and effects like particles and decals are still visible.

(rasterizer_model_lighting_ambient)

(rasterizer_model_transparents [boolean])

Toggles the rendering of transparent shaders in models. For example, the Warthog's windshield.

(rasterizer_near_clip_distance [real])

Sets the near clip distance, which is the minimum draw distance (world units). Defaults to 0.0625. This does not appear to work fully, as the near clip distance will only be adjusted for one frame.

(rasterizer_plasma_energy)

(rasterizer_profile_log)

(rasterizer_profile_objectlock_time)

(rasterizer_profile_print_locks)

(rasterizer_ray_of_buddha [boolean])

Toggles the lens flare "god rays" effect, present on sky lights or lens flares explicitly set to sun.

(rasterizer_refresh_rate)

(rasterizer_safe_frame_bounds)

(rasterizer_screen_effects)

(rasterizer_screen_flashes)

Toggles the display of screen flashes, such as those from a damage_effect or powerup equipment.

(rasterizer_secondary_render_target_debug)

(rasterizer_shadows_convolution [boolean])

Toggles the blurring of dynamic object shadows. Defaults to true.

(rasterizer_shadows_debug [boolean])

When enabled, all dynamic object shadow maps will be rendered with a partially-shadowed background, making their rectangular boundaries visible. The size of this rectangle depends on the object's bounding radius.

(rasterizer_smart [boolean])

No visible effect.

(rasterizer_soft_filter [boolean])

No visible effect.

(rasterizer_splitscreen_VB_optimization)

(rasterizer_stats [short])

Displays renderer statistics on the screen, with several modes. All modes include at least framerate stats.

Mode	Description
0	Off (default).
1	Some unknown counts which usually read 0, though fast increases when flags are on-screen.
2	Vertices, triangles, and primitives counts for various model and environment features and effects.
3	GPU profiling for each stage of rendering. Uknown if this still works.
4	Memory usage for dynamic vertices, shadow buffers, and more.
5	Just framerate stats.

(rasterizer_stencil_mask [boolean])

Enables or disables the stencil mask used to prevent the background scene from overlapping the first person view. Defaults to true. Disabling this has the same effect as disabling rasterizer_draw_first_person_weapon_first and you will sometimes see nearby objects occluding parts of the FP model.

(rasterizer_transparent_pixel_counter)

(rasterizer_water [boolean])

Toggles the rendering of shader_transparent_water shaders.

(rasterizer_water_mipmapping [boolean])

No visible effect. Defaults to false.

(rasterizer_wireframe [boolean])

Toggles rendering in wireframe mode, which only draws pixels along triangle edges rather than filling trangles. This can be useful for troubleshooting portals.

(rasterizer_zbias)

(rasterizer_zoffset [real])

Controls how far away from surfaces new decals are generated, e.g. for projectile impacts. Defaults to 0.003906. The units are not world units.

(rasterizer_zsprites)

(recover_saved_games_hack)

(remote_player_action_baseline_update_rate)

(remote_player_action_update_rate)

(remote_player_log_level)

(remote_player_position_baseline_update_rate)

(remote_player_position_update_rate)

(remote_player_vehicle_baseline_update_rate)

(remote_player_vehicle_update_rate)

(render_contrails [boolean])

Toggles the display of all contrails.

(render_model_index_counts [boolean])

If true, displays a red number above each object with its model index count. If render_model_vertex_counts is also enabled, the vertex count and index count are separated by a slash like "<vertices>/<indices>".

(render_model_markers [boolean])

If enabled, all model markers will be rendered in 3D with their name and rotation axis.

(render_model_nodes [boolean])

If enabled, all model skeletons will be rendered. Nodes are shown as axis gizmos and connected to their parents by white lines.

(render_model_no_geometry)

(render_model_vertex_counts [boolean])

If true, displays a red number above each object with its model vertex count. If render_model_index_counts is also enabled, the vertex count and index count are separated by a slash like "<vertices>/<indices>".

(render_particles [boolean])

Toggles the display of all particles.

(render_psystems [boolean])

Toggles the rendering of particle_systems and weather_particle_system.

(render_shadows [boolean])

Toggles the display of dynamic object shadows. Same effect as rasterizer_environment_shadows.

(render_wsystems [boolean])

No visible effect on weather or particle systems.

(rider_ejection [boolean])

Toggles if bipeds are ejected from overturned vehicles, including players and AI characters. Defaults to true.

(run_game_scripts [boolean])

If enabled, causes level scripts to be run in Sapien. This would allow you to see how the beach battle plays out in b30, for example.

(screenshot_count [short])

When the -screenshot argument is enabled, pressing Prnt Scrn will generate a series of tiled screenhots in the screenshots directory. For example, a value of 2 will generate 4 screenshots meant to tile in a 2x2 arrangement for a high resolution result. You should disable the HUD with show_hud 0 before using this.

(screenshot_size [short])

Appears to set some kind of crop or scaling factor for generated screenshots, but is probably not working as intended. Setting this to a value other than 1 can crash when using screenshot_size.

(slow_server_startup_safety_zone_in_seconds)

(sound_cache_dump_to_file)

Writes the sound cache information from debug_sound_cache and a listing of currently cached sounds to sound_cache_dump.txt.

(sound_cache_size)

(sound_gain_under_dialog [real])

Controls how quiet non-dialog sounds are when scripted dialog is playing (sound class must be scripted_dialog_other, scripted_dialog_force_player, or scripted_dialog_force_unspatialized). Does not apply to involuntary AI dialog like death/pain lines. Defaults to 0.7. The effect takes about half a second to fade in/out.

(sound_obstruction_ratio [real])

Controls how "muffled" sounds are when heard behind an obstruction like the BSP or an object with collision geometry. Defaults to 0.1. A value of 0 means "not muffled", while a value above about 6 effectively mutes them.

(speed_hack_detection)

(speed_hack_log_level)

(structures_use_pvs_for_vs)

If enabled, forces the renderer to fully render all clusters and subclusters in the potentially visible set (PVS) without any culling of occluded faces, and even if portals into those clusters are off-screen. Pair with rasterizer_wireframe 1 to see the effects. Use this to debug which clusters are in the PVS of the camera's cluster, which can help you understand why a cluster is considered indoor or outdoor. When this debug option is enabled, objects may sometimes disappear while in view.

This global differs from the related debug_no_frustum_clip, which disables subcluster culling by portal-diminished frustrums but still allows for entire clusters to be culled when no portals into them are visible.

(stun_enable)

(sv_client_action_queue_limit)

(sv_client_action_queue_tick_limit)

(sv_mapcycle_timeout)

(sv_public)

(sv_tk_ban)

(temporary_hud [boolean])

Renders a debug HUD with basic weapon information in text and line-drawn circular reticules representing the current error angle (yellow) and autoaim angle (blue, or red if autoaim active). Works in debug builds only.

(terminal_render [boolean])

Toggles the display of console output. Defaults to true.

(texture_cache_flush)

Clears all loaded textures from the texture cache. Works in debug builds only.

(texture_cache_graph [boolean])

Toggles a live representation of the texture cache in the top left corner of the screen, depicting which entries are being loaded and evicted. Works in debug builds only.

(texture_cache_list [boolean])

Shows a live list of all bitmap tag paths currently loaded in the texture cache. Works in debug builds only.

(transport_dumping)

(use_new_vehicle_update_scheme)

(use_super_remote_players_action_update)

(vehicle_incremental_rate)

(weapon_incremental_rate)

(weather [boolean])

Toggles the rendering of all weather_particle_system. Defaults to true.

Removed

Some defunct parts of HaloScript were removed in H1A MCC. This is not a complete list.

Function/global

Type

(rasterizer_water_mipmapping [boolean])

No visible effect. Defaults to false.

	Global

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/scripting/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
