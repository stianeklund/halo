# Halo: Combat Evolved — Known Bugs & Glitches
## Original Xbox (NTSC) Version Only

> **Scope:** Genuine bugs and behavioral glitches confirmed present in the original 2001 Xbox release (NTSC). Player-initiated tricks that don't expose an underlying engine bug (stunts, grenade jumps for sequence-breaking, out-of-bounds exploration) are excluded.

### 2-player split screen HUD positioning

**Description:** In a 2-player multiplayer or coop game, the HUD positions are in different locations for each player respectively, i.e. the screen is divided horizontally but the HUD elements are not in the same relative positions. Player 1 (the top half of the horizontal split) has much more room aove the HUD elements than Player 2 (the bottom half of the the horizontal split). Also interestingly, the motion tracker position is much higher for player 2 than player 1, so it seems maybe that the HUD elements; maybe even the camera positions? are squished slightly from the bottom and top?

**Reproduction:** Split screen, Coop or multiplayer 


---

### 2-player split screen camera positions 

**Description:** In a 2-player multiplayer or coop game, the camera position for player 1 and player 2 are not "equal", relatively speaking, player 2 (bottom half of screen)'s camera position seems to be slightly lower or further back resulting in more of the bottom of the gun / assault rifle visible than player 1 (top half of the screen). 

**Reproduction:** Split screen, Coop or multiplayer 

---

### Infinite Ammo Glitch

**Description:** Any weapon that uses magazines can be reloaded without consuming reserve ammo. The game continues processing a reload on a weapon even after it has been swapped out, and when picked back up the weapon has a full magazine with reserve count unchanged.

**Reproduction:** Empty a weapon's magazine. Before the reload animation begins, swap the weapon for any other weapon on the ground. Wait for the reload sound to finish (the game reloads the dropped weapon in the background). Pick the original weapon back up — full magazine, reserves untouched. Must be repeated each time a reload is needed.

---

### Unlimited Ammo / Unlimited Invisibility (UA/UI Loading Zone Glitch)

**Description:** On certain levels, crossing specific loading zone boundaries while charging a Plasma Pistol in co-op causes a state corruption that gives unlimited ammunition for all weapons. If the player has Active Camouflage, it also becomes permanent until death or level end.

**Reproduction:** Requires co-op. At a loading zone boundary (works on six of ten levels), one player charges a Plasma Pistol until it shakes violently (~10 seconds). The second player walks back and forth across the loading zone. If the Plasma Pistol stops shaking, the glitch has activated — ammo will no longer deplete for any weapon. Plasma weapons still overheat but never run out of charge.

---

### Backpack Reload

**Description:** A weapon reloads in the background while the player is firing a different weapon. The game continues processing the reload on the holstered weapon, allowing seamless cycling between two weapons with no reload downtime.

**Reproduction:** With the weapon needing reloading equipped, press reload twice quickly, then immediately switch to the secondary weapon. The reload animation continues in third person and reload sounds still play. Once the sounds complete, the original weapon is reloaded when switched back to.

---

### Speedy Reload (Reload Cancel)

**Description:** Weapon reload animations can be cancelled early via weapon-switching or meleeing. The game registers the reload as complete once a certain internal threshold in the animation is passed, even though the full animation has not played out.

**Reproduction:** Begin a reload, then quickly switch weapons and switch back, or melee partway through. If cancelled past the internal completion point (varies per weapon), the weapon is fully reloaded. Especially effective with the Shotgun and Pistol.

---

### Double Melee

**Description:** Two melee hits land in the time normally allowed for one, by chaining a melee with a grenade throw and a second melee. This is an animation-cancelling bug in the melee/grenade input queue.

**Reproduction:** Within melee range: press melee; the instant it connects, press grenade; immediately press melee again. Two melee hits register in rapid succession. The grenade also deploys, so back away after.

---

### No Weapon Glitch

**Description:** The player's weapon model disappears entirely. Master Chief runs with empty hands, but the weapon still functions — firing, reloading, and damage all work normally with nothing visible.

**Reproduction:** Rapidly switch weapons while simultaneously picking up a new weapon from the ground. The rendering state desyncs from the gameplay state. Picking up another weapon or reverting to a checkpoint fixes it.

---

### Three Weapons Glitch

**Description:** The player carries three weapons instead of the normal two. The third weapon occupies a phantom inventory slot that the game does not properly account for.

**Reproduction:** Drop a weapon near another pickup-able weapon. Quickly pick up one and immediately swap again before the first swap animation completes. With precise timing, the game fails to drop the original weapon from inventory.

---

### Plasma Rifle Overheat Glitch

**Description:** The Plasma Rifle can be fired in a rhythm that prevents the heat meter from ever reaching the overheat threshold, allowing effectively continuous fire. This is a bug in the heat accumulation/cooldown curve that allows micro-pauses to reset heat faster than intended.

**Reproduction:** Fire the Plasma Rifle and release the trigger for a split second at precise intervals just before the overheat threshold. The heat meter partially resets each micro-pause, and with the correct rhythm the weapon never overheats.

---

### Frozen Elite

**Description:** An Elite becomes permanently stuck in a single pose. It remains alive with active shields but does not move, attack, or respond to any stimulus. This is an AI state machine failure.

**Reproduction:** Occurs when an Elite's AI is interrupted mid-animation by conflicting triggers — commonly a sticky grenade that does not kill it, or a patrol path conflicting with a combat state. The door-skip Zealot on Silent Cartographer is always frozen. Also reported on Truth and Reconciliation.

---

### Unresponsive Enemies

**Description:** Enemies spawn with no active AI — they stand motionless and do not react to anything. Unlike Frozen Elites, these enemies never had working AI to begin with.

**Reproduction:** Encountered when the player reaches areas ahead of intended scripted progression, or when the AI actor limit is near capacity. The enemy models spawn but their behavior trees fail to initialise.

---

### Shielded Dead Elite

**Description:** An Elite's energy shield visual effect persists on its corpse after death. The dead body continues shimmering despite the AI being dead.

**Reproduction:** Kill an Elite while its shields are actively recharging or flickering. The shield particle effect fails to despawn with the AI death state.

---

### Weaponless Elite

**Description:** An Elite spawns holding no weapon. It performs melee attacks but never fires any projectile.

**Reproduction:** Occurs when weapon assignment fails during spawning — typically in areas with many AI actors, or if the scripted weapon has been picked up or despawned due to object limits.

---

### Invincible Marines

**Description:** Marines become completely invulnerable, surviving any damage including direct rockets. They continue behaving normally otherwise.

**Reproduction:** Occurs when Marines are flagged as essential for an untriggered scripted event. Also reported as a rare spawn bug where the invincibility flag is never properly cleared.

---

### Unarmed Marines

**Description:** Marines are found with no weapon. They follow the player and react vocally to enemies but cannot fight.

**Reproduction:** The lone Marine found via the "Stay in the Pelican" glitch on 343 Guilty Spark is always unarmed. Also occurs when weapon-swapping with Marines breaks their weapon assignment, or when spawned Marines exceed the weapon object pool.

---

### Duplicating Marine

**Description:** A Marine produces a copy of himself upon death. A duplicate body appears at or near the original's location.

**Reproduction:** Appears related to the Marine's death coinciding with a loading zone transition or checkpoint. Exact trigger conditions are not fully understood.

---

### Character Stretch Glitch

**Description:** Character models become grotesquely stretched, with limbs extending across the screen in distorted shapes. Purely visual — no gameplay effect.

**Reproduction:** Triggers when a character's skeletal animation encounters an interpolation error, typically when ragdoll physics coincides with an animation state transition. Killing enemies with explosives near walls or in tight spaces increases likelihood.

---

### Floating Assault Rifles

**Description:** Assault Rifles dropped by dead Marines hover in mid-air instead of falling to the ground. They remain interactive.

**Reproduction:** Kill Marines in areas with complex floor geometry. The weapon's physics collision snags on invisible collision surfaces, leaving the rifle suspended.

---

### Floating Plasma Grenade

**Description:** A thrown plasma grenade becomes stuck floating in mid-air at the point where it was supposed to attach to a target, then detonates in place.

**Reproduction:** Throw a plasma grenade at a moving target at the exact moment the target dodges out of the attachment hitbox. The grenade attaches to the now-empty coordinate in space.

---

### Bouncing Items

**Description:** Dropped weapons or items bounce endlessly on the ground, vibrating in place as the physics engine fails to resolve their resting state.

**Reproduction:** Drop weapons on uneven terrain or near seams between geometry surfaces.

---

### Spinning Gun

**Description:** A dropped weapon on the ground begins spinning continuously on its axis. Can still be picked up normally.

**Reproduction:** Drop a weapon on a sloped surface or near physics-active objects. The collision system imparts angular momentum that never dissipates.

---

### Invisible Warthog Ammo

**Description:** The Warthog turret fires and deals damage, but tracer rounds are invisible. The turret appears to shoot nothing.

**Reproduction:** Occurs after sustained continuous turret fire, likely when the particle/tracer effect pool is exhausted. Reloading a checkpoint resolves it.

---

### White Weapons

**Description:** Weapons render as entirely white, losing all texture detail. The weapon still functions.

**Reproduction:** Texture streaming failure, most commonly after extended play sessions or under heavy texture memory load. Reloading a checkpoint or restarting the level fixes it.

---

### Bullet Hole Glitch

**Description:** Bullet impact decals appear at incorrect locations — far from where the shot landed, or on surfaces that were not shot at all.

**Reproduction:** Fire rapidly at walls while moving. Decal placement miscalculates on curved or angled geometry.

---

### Phantom Flood Whip

**Description:** A Flood combat form's tentacle melee attack deals damage well beyond the visual animation's reach, hitting the player from what appears to be a safe distance.

**Reproduction:** Engage Flood combat forms at medium range. The melee damage hitbox extends significantly further than the visual tentacle swing.

---

### Headless Master Chief

**Description:** Glitching into a cryo pod on The Pillar of Autumn reveals Chief's third-person model has no head. Bungie never modelled the head onto the cutscene body. Entering the pod can kill the player.

**Reproduction:** During the cryo bay sequence on The Pillar of Autumn, clip into a cryo pod via precise movement before the scripted sequence completes.

---

### Death by Cryo Tube

**Description:** The player is killed instantly by clipping into cryo tube geometry. The game applies a kill trigger when the player enters solid geometry.

**Reproduction:** On The Pillar of Autumn, attempt to jump back into the cryo tube at the start of the level.

---

### Immobile Banshee

**Description:** A Banshee becomes permanently stuck in place. The player can enter and fire weapons, but it will not respond to movement input.

**Reproduction:** Damage a Banshee with explosives or ram it with a vehicle while landed/hovering low. The physics state corrupts, locking it in position. Also occurs when boarding an AI Banshee at the moment it completes a scripted manoeuvre.

---

### Fifty-Degree / Perpendicular Banshee Fire

**Description:** The Banshee's plasma bolts fire at a severe angle offset from the player's aim — roughly 50 degrees off or nearly perpendicular to the flight direction.

**Reproduction:** Pitch the Banshee to extreme angles (near-vertical climb or dive) and fire. The projectile direction vector desyncs from the camera aim at steep pitch values.

---

### Pelican Death Glitch

**Description:** Riding on top of a Pelican as it departs causes sudden unexplained death. The engine interprets the Pelican's pitch as a vehicle flip, ejects the player, and applies fatal fall damage.

**Reproduction:** Jump onto a departing Pelican. As it pitches forward, the game treats the angle as a flip event and kills the player via the fall damage script.

---

### Air Swimming

**Description:** The player becomes stuck in a swimming animation while airborne, with sluggish movement as if in water despite being in mid-air.

**Reproduction:** Drive a vehicle off a cliff at certain angles or get launched by an explosion while exiting a vehicle. The game briefly applies water physics to the player in mid-air.

---

### Suspended Dropships

**Description:** Covenant Spirit dropships freeze in mid-air during their scripted flight paths, hovering motionless indefinitely.

**Reproduction:** Occurs when the player interrupts a scripted sequence at an unexpected time, such as killing all enemies before a dropship's script completes. The dropship reaches a waypoint but fails to advance.

---

### Warthog Door Skip / Stuck Door (Silent Cartographer)

**Description:** Driving a Warthog into a closing door on The Silent Cartographer causes the door to close on the vehicle and loop endlessly trying to shut. The door's collision and state logic cannot resolve the obstruction. As a side effect, the Zealot Elite on the far side has completely non-functional AI.

**Reproduction:** On The Silent Cartographer, drive the Warthog at full speed into the doorway as it begins closing. If positioned correctly, the door closes on the vehicle body and cannot complete its close cycle. The player can dismount on the far side.

---

### Ghost Push Through Doors

**Description:** Exiting a Ghost while strafing causes the vehicle to push the player. This force can shove the player through closed/locked door geometry, bypassing progression gates.

**Reproduction:** While riding a Ghost, strafe right and exit. The Ghost's dismount physics push the player. Position yourself against a closed door and use this push to clip through.

---

### Cutscene Vehicle Interference

**Description:** Vehicles and physics objects present in a cutscene trigger zone continue interacting during the in-engine cutscene, causing unintended effects: Marines dying, the Warthog running over Chief, Chief's corpse lying on the ground while dialogue plays, or a Ghost plowing through the scene.

**Reproduction:** On The Silent Cartographer, drive a Warthog into the Cartographer cutscene zone. On Assault on the Control Room, drive a Ghost through the final cutscene trigger. The in-engine cutscene plays while the vehicle physics continue, causing visual chaos.

---

### Spinning Technician (Crewman Glitch)

**Description:** Sticking a cowering technician with a plasma grenade causes a broken animation — the crewman stands up, T-poses, and spins rapidly before the grenade detonates. The crewman lacks the animation states to handle the cowering-to-stuck transition.

**Reproduction:** On the level Halo, at the final structure, kill the Marines near the two technicians. The technicians crouch in fear. Stick one with a plasma grenade while cowering. They T-pose and spin until the detonation.

---

### Leak (BSP Rendering)

**Description:** The void or skybox behind level geometry becomes visible through gaps in walls, floors, or ceilings — standard BSP leak where the level geometry is not fully sealed.

**Reproduction:** Look at specific angles near geometry edges, particularly where terrain meshes meet structural walls.

---

### Clipping (General Collision Failures)

**Description:** Players, vehicles, or objects pass through geometry that should be solid. A pervasive class of collision detection failures present on nearly every level.

**Reproduction:** Drive vehicles into specific walls at speed, crouch-walk into geometry seams, or use explosive knockback against imperfect collision meshes. Specific locations are well-documented by the speedrunning community.

---

*Sources: Halopedia (halopedia.org), Halo Alpha / Halo Fandom (halo.fandom.com), StrategyWiki (strategywiki.org), GameFAQs, SuperCheats community reports, and original Xbox community testing. All glitches verified as present on the original 2001 Xbox NTSC retail release.*

