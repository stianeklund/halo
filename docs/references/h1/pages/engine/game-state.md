# Halo 1 Reference: Datum arrays

## Source Page

- Source: `https://c20.reclaimers.net/h1/engine/game-state/`
- Local path: `engine/game-state/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

**Game state** is the in-memory data which describes the state of the game world as it is simulated over time. It differs from tags which, although they are also loaded into memory, describe static or initial properties of classes of game objects rather than the current properties of individual ones.

Game state also includes global data for systems like scripting (script globals), multiplayer game modes (scores), and physics (game speed and gravity) to name a few. During each simulation tick (30 per second) the game runs _updates_ across the game state which result in the ongoing changes to the game world as time progresses. As an example, this might include moving a glow particle some distance based on its speed in the glow tag definition.

The game state is saved and loaded from game saves or core saves.

### Datum arrays

Much of the game state is maintained in _datum arrays_, also called _tables_. Each entry (_datum_) in these arrays is used to store the current state of some object or effect. The structure and purpose of these datums require reverse engineering to understand.

Since the game world is dynamic, the datum count can rise up to a limit. The following limits are known ("-" if unchanged):

| Table                                  | Legacy limit | H1A limit | Purpose                                                                                                                                                                                                                                                                       |
|----------------------------------------|--------------|-----------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| objects")                              | 2048         | -         |                                                                                                                                                                                                                                                                               |
| cluster collidable object reference    | 2048         | -         |                                                                                                                                                                                                                                                                               |
| cluster noncollidable object reference | 2048         | -         |                                                                                                                                                                                                                                                                               |
| collidable object cluster reference    | 2048         | -         |                                                                                                                                                                                                                                                                               |
| noncollidable object cluster reference | 2048         | -         |                                                                                                                                                                                                                                                                               |
| cached object render states            | 256          | -         |                                                                                                                                                                                                                                                                               |
| widget")                               | 12           | -         |                                                                                                                                                                                                                                                                               |
| flag                                   | 2            | -         |                                                                                                                                                                                                                                                                               |
| antenna                                | 12           | 24        |                                                                                                                                                                                                                                                                               |
| glow                                   | 8            | -         | Stores the state of glow systems/paths. For example, the energy sword has 2 glow paths so up to 4 energy swords can be glowing.                                                                                                                                               |
| glow particles                         | 512          | -         | Stores the state of individual glow particles, like colour and position.                                                                                                                                                                                                      |
| light volumes                          | 256          | -         |                                                                                                                                                                                                                                                                               |
| lightnings                             | 256          | -         |                                                                                                                                                                                                                                                                               |
| device groups                          | 1024         | -         |                                                                                                                                                                                                                                                                               |
| lights                                 | 896          | -         |                                                                                                                                                                                                                                                                               |
| cluster light reference                | 2048         | -         |                                                                                                                                                                                                                                                                               |
| light cluster reference                | 2048         | -         |                                                                                                                                                                                                                                                                               |
| decals                                 | 2048         | -         | Stores the state of dynamic decals. This likely includes their position, tag ID, and expiry timer.                                                                                                                                                                            |
| players                                | 16           | -         |                                                                                                                                                                                                                                                                               |
| teams                                  | 16           | -         |                                                                                                                                                                                                                                                                               |
| contrail                               | 512          | -         |                                                                                                                                                                                                                                                                               |
| contrail point                         | 1024         | -         |                                                                                                                                                                                                                                                                               |
| effect                                 | 256          | -         |                                                                                                                                                                                                                                                                               |
| effect location                        | 512          | -         |                                                                                                                                                                                                                                                                               |
| particle                               | 1024         | -         |                                                                                                                                                                                                                                                                               |
| particle systems                       | 64           | -         |                                                                                                                                                                                                                                                                               |
| particle system particles              | 512          | -         |                                                                                                                                                                                                                                                                               |
| object looping sounds                  | 1024         | -         |                                                                                                                                                                                                                                                                               |
| actor                                  | 256          | -         |                                                                                                                                                                                                                                                                               |
| swarm                                  | 32           | -         |                                                                                                                                                                                                                                                                               |
| swarm component                        | 256          | -         |                                                                                                                                                                                                                                                                               |
| prop                                   | 768          | -         | Part of the AI knowledge model and stores a web of relationships between units like allies and enemies. Likely used when determining if actors have more enemies than allies nearby, if enemies are reachable, occluded, etc. Props can be visualized with `ai_render_props`. |
| encounter                              | 128          | -         |                                                                                                                                                                                                                                                                               |
| ai pursuit                             | 256          | -         |                                                                                                                                                                                                                                                                               |
| object list header                     | 48           | -         | Stores object lists returned by scripting functions like `ai_actors`. These are reference-counted and periodically garbage collected.                                                                                                                                         |
| list object reference                  | 128          | -         | Stores object references belonging to object lists (above). These datums reference each other by ID to form linked lists.                                                                                                                                                     |
| hs thread                              | 256          | -         |                                                                                                                                                                                                                                                                               |
| hs globals                             | 1024         | -         | Stores the state of HS globals, including external globals, not just those in the level script.                                                                                                                                                                               |
| recorded animations                    | 64           | -         |                                                                                                                                                                                                                                                                               |
| AI knowledge                           | 768          | -         |                                                                                                                                                                                                                                                                               |
| mounted weapon units")                 | 8            | -         |                                                                                                                                                                                                                                                                               |

### Glow datum

Glow datums hold the state of glows. For example, the gold Elite's energy sword is composed of 2 glows, one for the bottom blade and one for the top.

| Field                                          | Offset (relative) | Type           | Comments |
|------------------------------------------------|-------------------|----------------|----------|
| id                                             | `0x0`             | `uint16`       |          |
| initialized                                    | `0x2`             | `bool`         |          |
|                                                | `0x3`             | `pad(1)`       |          |
| number of markers                              | `0x4`             | `uint16`       |          |
|                                                | `0x6`             | `pad(2)`       |          |
| markers                                        | `0x8`             | `byte(108)[5]` |          |
| definition index                               | `0x224`           | `uint32`       |          |
| bitmap dimension                               | `0x228`           | `uint16`       |          |
| marker order                                   | `0x22A`           | `uint16[5]`    |          |
| total time                                     | `0x234`           | `float`        |          |
| marker time index                              | `0x238`           | `float[5]`     |          |
| accumulated trailing particle generation ticks | `0x24C`           | `uint16`       |          |
| number of particles                            | `0x24E`           | `uint16`       |          |
| head particle offset                           | `0x250`           | `uint32`       |          |
| tail particle offset                           | `0x254`           | `uint32`       |          |

| Field                    | Offset (relative) | Type     | Comments                                                                                     |
|--------------------------|-------------------|----------|----------------------------------------------------------------------------------------------|
| id                       | `0x0`             | `uint16` |                                                                                              |
|                          | `0x2`             | `pad(2)` |                                                                                              |
| reference count          | `0x4`             | `uint16` |                                                                                              |
| count                    | `0x6`             | `uint16` |                                                                                              |
| list object reference id | `0x8`             | `uint16` | ID of the first (or last?) element of this list, from the list object reference datum array. |
|                          | `0xA`             | `pad(2)` |                                                                                              |

|  | Function/global                                                                                                                                                                                                                             | Type   |
|--|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|--------|
|  | `(ai_show_swarms [boolean])` Displays debug information in the bottom left with the current counts for swarms and swarm component datum arrays. Swarms are groups of Flood infection forms while components are individual infection forms. | Global |

### Acknowledgements

Thanks to the following individuals for their research or contributions to this topic:

*   Conscars _(Reversing object list header datum)_
*   gbMichelle _(Reversing stock table limits)_

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/engine/game-state/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
