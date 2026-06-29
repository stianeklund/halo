# Halo 1 Reference: Knowledge model

## Source Page

- Source: `https://c20.reclaimers.net/h1/engine/ai/`
- Local path: `engine/ai/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

AI firing positions and sight lines are shown for the b30 beach assault.

The **AI system** is the part of the game responsible for AI behaviour over short time scales. It is paired with level scripts which give the AI broader or situational goals.

Halo's game design goals heavily influenced the AI system; actors are meant to be predictable rather than completely random, react to the player, have incomplete knowledge, and communicate their internal state via dialogue, animations, and focus of attention.

### Knowledge model

AI have an _individual knowledge model_ with "real" perception. They do not have complete knowledge of the battlefield but rather remember key objects and rely on cues like visibility and sound to track their enemy. This allows the AI to be fooled and act more believably.

### Props

AI maintain a list of tracked units called _props_, which are classified as into a few types like friends and enemies. The game state stores a pool of up to 768 props to be shared among actors. Little is known about how props are used but they likely help with behaviours like _avoid friends line of fire_.

### Behaviour

According to quotes from Halo's AI programmer _Chris Butcher_, certain AI behaviours are triggered by hard-coded conditions. For example, the Grunt behaviour of fleeing when their Elite is killed is hard-coded, likely based on the actor type definition. Marines have an individual behaviour to stand close, but not too close, to other Marines, giving the illusion of them working in groups.

### Actions

AI decide between 14 actions. The rules for selecting these and what behaviours belong to each action are not well understood.

| Action index | Action name |
|--------------|-------------|
| `0`          | none        |
| `1`          | sleep       |
| `2`          | alert       |
| `3`          | fight       |
| `4`          | flee        |
| `5`          | uncover     |
| `6`          | guard       |
| `7`          | search      |
| `8`          | wait        |
| `9`          | vehicle     |
| `10`         | charge      |
| `11`         | obey        |
| `12`         | converse    |
| `13`         | avoid       |

### Encounters and squads

In Halo1, AIs were grouped into encounters, which also contained a set of firing positions. Various subsets of this set were made available to the AI depending on the state of their encounter (have many of their allies been killed? Are they winning? Are they losing? This was a mapping that was created by a designer).

### Firing positions

_Firing positions_ are discrete locations stored in the scenario's encounters where the AI can stand when trying to perform a spatial behaviour. They are _not_ pathfinding nodes, but rather a pathfinding _destination_. The AI will weigh and select firing positions based on a few factors:

*   Line of sight
*   Distance to target
*   Proximity of cover
*   Proximity of friends and enemies
*   Obstructions and hazards like vehicles, grenades, etc.

For example, the AI may move to a firing position if it has a clear line of sight to an enemy's _presumed_ location (remember, AI have an incomplete knowledge model) and is in the actor's desired range.

Firing positions are also used when AI are scripted to following a target with the `ai_follow_*` functions. They will move to firing positions with the same letter label that their target is near.

### Pathfinding

Pathfinding is the system which allows AI to navigate between locations. It relies in part on precomputed BSP pathfinding data and object pathfinding spheres to know available paths and possible obstacles.

When the AI wishes to navigate, this data and static obstructions like scenery are considered to create a "smoothed" path of nodes between source and destination, which the AI then follows. It is believed that the A* pathfinding algorithm is used at this phase. While following the path, dynamic obstructions like units") may force the AI to make detours to continue to its next node but it does not need to recalculate the entire path.

On a more technical level, pathfinding spheres are projected to the AI's ground-plane at pathfinding time to become pathfinding _discs_.

The maximum pathfinding distance that Halo's engine permits is 3276.7 world units, an _extremely_ long distance. For reference, the distance between bases in Timberland is approximately 100 units. Halo 1 does not support pathfinding in moveable reference frames, unlike Halo 2 for Scarabs.

### Acknowledgements

Thanks to the following individuals for their research or contributions to this topic:

*   Chris Butcher _(Quotes in article "The Artificial Intelligence of Halo 2" and GDC 2002 slides in "The Integration of AI and Level Design in Halo")_
*   Conscars _(Reversing AI action names)_
*   gbMichelle _(Discovering max pathfinding distance, following behaviour with firing positions)_
*   kornman00 _(Overview of AI pathfinding system)_

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/engine/ai/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
