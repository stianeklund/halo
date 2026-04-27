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
2. Weapons do not spin when shot (weapons should spin around their center, this is a known quirk in Halo, it not working is a bug).
Only weapons that are dropped by the opponent can spin when shot, but even newly spawned weapons should spin when shot.

Laf8529b4befd254f1d808157899d5c7709af8529b4befd254f1d808157899d5c7709af8529b4befd254f1d808157899d5c7709LM Context: These are worth investigating, especially decals_dot3() in decals.c:1149 — that's right in the decal rendering pipeline we suspect.
If Ghidra confused a callee-saved register and we passed bitangent twice instead of bitangent
and tangent (or similar), the dot3 lighting calculation would be wrong and decals could render as invisible/black

