Level Maw: Player dies, load from checkpoint, intro video plays but cortana speaks / audio playback from checkpoint..?

Repro: Hard to reproduce
Affected build: 6229951e

---

Level Maw: During ending of the level when the countdown timer is active, slight audio corruption on left audio channel

Repro: Hard to reproduce
Description: Audio corruption, ticking sound, seems to have same "rate" as the countdown timer's (the hundredths of a second / millisecond), e.g. last two digits of 00:00:00.
During ending of the level when the countdown timer is active, slight audio corruption on left audio channel
 
Affected build: NA

---

Level: The Maw
Description:  You can pick up empty weapons. This shouldn't be possible I think? Double check with unpatched 2276 build.
Repro: Hold a weapon, empty weapon, pick up new weapon replacing empty weapon, walk over empty dropped weapon, hold X to pick up shows, holding X will pick up empty weapon.
Affected build: 76437e7
This isn't a bug

---

Level: The Pillar of Autumn
Description: Audio from AI that is close by is non existent. Other AI speech is very muffled / lower amplitude. Does not affect cutscenes.
Repro: Play campaign levels.
Affected build: 63d8220
Fixed in latest main


---
Level: All
Description: When shooting walls etc there are no bullet holes.
Affected build: f720a8
1. Blood stains and similar damage inflicted by grenades etc do not show.
   Fixed in build/commit: 1d9f7ecc
2. Weapons do not spin when shot on flat ground (weapons should spin around their center when shot).
   Grenades still moved weapons (area damage). Weapons on inclines spun correctly after baaef268.
   Root cause: item_set_position read ground marker normal from wrong offset (+0x50 instead of +0x54).
   This corrupted the spin axis every tick, gradually rotating the collision model out of position
   so projectile rays missed the weapon entirely. Ghidra decompiler showed +0x50; disassembly
   confirms +0x54 (MOV ECX,[EBP-0x38] with marker_buf at EBP-0x8c).
   Fixed in build/commit: 0cdae111

