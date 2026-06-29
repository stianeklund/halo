# Halo 1 Reference: General

## Source Page

- Source: `https://c20.reclaimers.net/h1/scripting/advanced-scripting/`
- Local path: `scripting/advanced-scripting/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

The set of HaloScript functions covers most reasonable needs for singleplayer scripting. However, modders being modders means this sometimes isn't enough and creative solutions are needed.

### General

### Using globals for parameters

Only H1A supports parameters on static scripts. Use globals to pass data to static scripts in other versions of the engine. For example, this script with a parameter:

```
(script static void (kill_player (short player_index))
  (unit_kill (unit (list_get (players) player_index)))
)
; (kill_player 5)
```

Can be equivalently written with a global as:

```
(global short kill_player_param1 0)
(script static void kill_player
  (unit_kill (unit (list_get (players) kill_player_param1)))
)
; (set kill_player_param1 5) (kill_player)
```

This page features some example static scripts which use parameters, so be aware that you can use them outside H1A too if you use globals.

### Using globals for local variables

H1 HaloScript doesn't have local variables, so it's sometimes useful to create globals for scripts to set/get during their execution. This can avoid having to recalculate the same expressions repeatedly, or lets you force a cast to a different data type. Various examples on this page use globals this way.

### Control

### For-Looping

HaloScript does not natively support loops, but you can use `continuous` scripts and an incrementing global to emulate one with the caveat that iterations are spread across game ticks. Since continuous scripts are executed once per tick and the game's tick rate is 30 Hz, this means a continuous script which needs to iterate over 60 items will take 2s to complete.

```
(global short player_index 0)

(script continuous loop_over_players
  ; do what you need with player_index:
  (if (volume_test_object kill_volume (list_get (players) player_index))
    (unit_kill (unit (list_get (players) player_index)))
  )

  ; increment/reset the index for the next tick
  (set player_index (+ 1 player_index))
  (if (>= player_index (list_count (players)))
    (set player_index 0)
  )
)
```

### While-Looping

Another approach to looping in HaloScript is to use `sleep_until` to construct a blocking while loop. The `sleep_until` function takes as its first argument a boolean value which can be the result of an expression including one with side effects, while the second argument controls how often the loop block is executed. By creating a block of functions using `begin` we can use it as a continious while-loop with a execution rate (in ticks) and exit condition we can decide on. In most cases this is not a clear improvement over just using a `continuous` script but in some situations it can be easier to work with - for instance when scripting complex AI behaviour, especially in later games that use `command scripts`.

```
(global boolean exit_loop false)
(script dormant our_function
  ; loop until exit_loop is true
  (sleep_until
    (begin ; open a multi-expression block
      (print "this is printed every two seconds")
      ; check something or do something
      (if (<= (ai_living_count reinforcements) 3) (ai_place reinforcements))
      ; final statement inside the begin must be our exit condition
      ; in this case a simple variable
      exit_loop
    )
    60 ; run every 60 ticks (2 seconds)
  )

  ; script can now continue on to do other stuff
  (ai_kill reinforcements)
  (print "Cleaned up remaining reinforcements")
)
```

### Recursion

Parameterized static scripts _can_ call themselves, but also easily result in stack overflows. Prefer iterative solutions over recursive ones.

```
(script static long (fib (long n))
  (cond
    ((< n 1) 0)
    ((= n 1) 1)
    (true (+ (fib (- n 1)) (fib (- n 2))))
  )
)
```

```
fib 1 ; 1
fib 2 ; 1
fib 3 ; 2
fib 4 ; 3
fib 5 ; 5
fib 6 ; crash/fatal due to stack overflow
```

### Math

### Modulo

You can create a modulo/modulus operator with a static script and a global. The global is necessary because it forces a cast from `real` to `short`.

```
(global short modulo_tmp 0)

(script static short (modulo (short x) (short y))
  (set modulo_tmp (/ x y))
  (- x (* modulo_tmp y))
)

(modulo 10 3) ; returns 1
```

If your divisor `n` is a power of 2, e.g. `16`, you can use H1A's `bitwise_and` with `n - 1` for a simpler modulo:

`(bitwise_and 20 15) ; equivalent to 20 % 16 = 4`
### Square root

If you need a square root, it requires an approximation since there is no built-in way to calculate it. For positive values of `x`, we can use _Heron's Method_ to iteratively refine an estimate until a desired accuracy is reached:

```
(global real sqrt_tmp 0)
(script static real (sqrt (real x))
  (set sqrt_tmp (/ (+ sqrt_tmp (/ x (/ x 2))) 2))
  (set sqrt_tmp (/ (+ sqrt_tmp (/ x sqrt_tmp)) 2))
  (set sqrt_tmp (/ (+ sqrt_tmp (/ x sqrt_tmp)) 2))
  (set sqrt_tmp (/ (+ sqrt_tmp (/ x sqrt_tmp)) 2))
  (set sqrt_tmp (/ (+ sqrt_tmp (/ x sqrt_tmp)) 2))
  (set sqrt_tmp (/ (+ sqrt_tmp (/ x sqrt_tmp)) 2))
)
```

```
sqrt 100 ; 10.000000
sqrt 16 ; 4.000000
sqrt 15 ; 3.872983
sqrt 2 ; 1.414214
```

### Clamp

A clamp function limits a given value to a range:

```
(script static real (clamp (real x) (real low) (real high))
  (max low (min high x))
)
```

```
clamp -1.5 0 1 ; 0.000000
clamp 0.5 0 1 ; 0.500000
clamp 100 0 1 ; 1.000000
```

### Other

### Controlling object functions

Suppose you need to control an object function") with scripts. For example, an object uses functions to alter various aspects of its appearance and this needs to happen during a scripted event. Depending on the object type, you have a variety of options available to you:

*   Use `device_set_power` to scale a device's power input").
*   Use `object_set_facing` to alter the object's orientation and scale the compass input").
*   Use `unit_set_current_vitality` to scale a unit's body or shields inputs").

This is not a comprehensive list, but just be aware that scriptable properties may be exposable as object function sources.

### Getting object coordinates

There is no built-in way to get object world unit coordinates (x, y, z) in HaloScript. For most needs you probably just want trigger volumes to check if objects or players are in an area. However, if you really need to know coordinates then you can use `objects_distance_to_flag` and 4 cutscene flags with known coordinates as "base stations" to build a sort of GPS system to do this. Place cutscene flags in your scenario with these exact coordinates and names:

| x | y | z | name   |
|---|---|---|--------|
| 0 | 0 | 0 | `gps1` |
| 1 | 0 | 0 | `gps2` |
| 0 | 1 | 0 | `gps3` |
| 0 | 0 | 1 | `gps4` |

The following script then gets a object's distance to each of these flags and calculates its coordinates using 3D trilateration. The math has been derived and simplified for the above cutscene flag positions, so make sure they're correct.

```
; temporary variables for the calculation
(global real gps_tmp1 0)
(global real gps_tmp2 0)
(global real gps_tmp3 0)
(global real gps_tmp4 0)
; holds the output coordinates
(global real gps_x 0)
(global real gps_y 0)
(global real gps_z 0)

(script static void gps_trilateration
  (set gps_tmp1 (objects_distance_to_flag (player0) gps1))
  (set gps_tmp2 (objects_distance_to_flag (player0) gps2))
  (set gps_tmp3 (objects_distance_to_flag (player0) gps3))
  (set gps_tmp4 (objects_distance_to_flag (player0) gps4))
  (set gps_tmp1 (* gps_tmp1 gps_tmp1))
  (set gps_tmp2 (* gps_tmp2 gps_tmp2))
  (set gps_tmp3 (* gps_tmp3 gps_tmp3))
  (set gps_tmp4 (* gps_tmp4 gps_tmp4))
  (set gps_tmp1 (+ gps_tmp1 (* gps_tmp2 -1) 1))
  (set gps_tmp2 (- gps_tmp2 gps_tmp3))
  (set gps_tmp3 (- gps_tmp3 gps_tmp4))
  (set gps_x (/ gps_tmp1 2))
  (set gps_y (/ (+ gps_tmp1 gps_tmp2) 2))
  (set gps_z (/ (+ gps_tmp1 gps_tmp2 gps_tmp3) 2))
)

; print coordinates every 1s
(script continuous gps
  (gps_trilateration)
  (print "x/y/z:")
  (inspect gps_x)
  (inspect gps_y)
  (inspect gps_z)
  (sleep 30)
)
```

The function `objects_distance_to_flag` technically accepts an object list, but you can pass it a single object like `(player0)` and it will be cast to a list for you. Use cases for a script like this might include testing player positions against complex distance functions rather than scenario trigger volumes, determining _how far_ a player has entered a given volume, or knowing of an object is north/south/east/west of another moving object by comparing coordinates.

### Detecting game mode

There is no script function to directly detect the game mode, but we can create a custom equipment set to spawn for each game mode which has a damaging fire effect. If we place a series of named bipeds at the same locations as these equipment spawns then we can detect damage to those bipeds via scripts to find out the game mode.

Firstly, create an item_ collection and equipment:

The equipment uses the model `scenery\emitters\burning_flame\burning_flame.gbxmodel` and effect `scenery\emitters\burning_flame\effects\burning.effect` attached to the `smoker` marker.

Hide some bipeds in your level named `king_detector`, `oddball_detector`, `race_detector`, and `ctf_detector`. Slayer can be assumed in the absense of any damage to these bipeds. Make sure they're placed far enough away from each other that the fire effect won't damage multiple:

Now place the damage emitter equipment spawns on each biped. Make sure to set _type 0_ to the corresponding game mode for that biped:

The game mode detection should be slightly delayed from startup since it takes a moment for the items to spawn and damage the bipeds. In your level's script, you'll need something like this:

```
(script startup detect_game_mode
  (sleep 30)
  (print
    (cond
      ((< (unit_get_shield king_detector) 1) "game is king")
      ((< (unit_get_shield oddball_detector) 1) "game is oddball")
      ((< (unit_get_shield race_detector) 1) "game is race")
      ((< (unit_get_shield ctf_detector) 1) "game is ctf")
      (true "game is slayer")
    )
  )
)
```

Now when your level loads, a biped will burn according to the game mode. Once the biped dies its shield value will go to `-1` which still satisfies the condition. You can test different game modes in Standalone using `game_variant <mode>` before running `map_name`.

### Acknowledgements

Thanks to the following individuals for their research or contributions to this topic:

*   Bungie _(Inventing while-looping in the H2 command scripts)_
*   Conscars _(GPS and game mode detection)_
*   num0005 _(Documenting `while-looping` for H1+)_

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/scripting/advanced-scripting/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
