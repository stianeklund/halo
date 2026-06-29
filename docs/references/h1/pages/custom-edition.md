# Halo 1 Reference: Halo Editing Kit

## Source Page

- Source: `https://c20.reclaimers.net/h1/custom-edition/`
- Local path: `custom-edition/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

**Halo Custom Edition**, or **CE**, is a standalone version of Halo PC which supports custom maps created by the Halo Editing Kit, originally released in 2004. It also features a server browser and its own dedicated server, but lacks the campaign. Maps are incompatible between the editions.

Custom Edition became the de facto standard Gearbox title due to its support of custom maps, campaign ports, and actively maintained client and server mods. Like retail, its last update was `1.0.10` in 2014. The release of MCC and its associated mod tools has reduced the active player base, but CE still remains popular for gamenights due to dedicated servers and client mods with automatic map downloads like HAC2 and Chimera. It's also used for mods which require engine extensions with Lua scripts or OpenSauce.

After installing Custom Edition, make sure to update it to 1.0.10. Installers and updaters can be found here.

### Halo Editing Kit

A screenshot of classic Sapien from the HEK tutorial.

The **Halo Editing Kit** or **HEK** is the legacy suite of modding tools released by Gearbox Software with Custom Edition in 2004. It includes Sapien, Tool, Guerilla, the Blitzkrieg exporter for 3ds Max, and a tutorial for custom multiplayer map development (now adapted into the Blender-based level guides here on c20).

Unlike the modern Halo CE mod tools, the HEK only includes a subset of the game's tags. With today's reliable tag extraction that's no longer a barrier and you can create custom singleplayer or modified stock maps. It's also possible (though a bit more complicated) to use the new tools and benefit from all their bug fixes and new features.

The tools in the HEK work mostly the same as their newer counterparts, so documentation on this site is combined. You can learn about the differences by reading comparison to the HEK and changelogs for the new tools.

### Installation

Download and run the installer. The HEK is installed in the same location as Halo Custom Edition itself.

After installing the HEK you must prevent the Windows VirtualStore from interfering with it.

Windows versions since Vista have included a security feature called the _VirtualStore_. It prevents applications from modifying or creating files in protected locations like `C:\Program Files (x86)\...` by performing such operations under `C:\Users\%USERNAME%\AppData\Local\VirtualStore` instead with the application being unaware of this.

As a result, some applications may end up with conflicting "views" of what files are actually present. This is a problem since the HEK uses the `data`, `tags`, and `maps` directories as editable workspaces. Users have experienced Halo loading incorrect versions of maps, and tag edits done in Guerilla not being visible to Sapien. To avoid these issues, use one of the following workarounds:

*   Take ownership of Halo's installation files. Right click the `Halo Custom Edition` folder, select _Properties_, and from the _Security_ tab change the owner to yourself. Once ownership has been taken, any conflicting files in the Virtual Store location above also need to removed.
*   Permanently move Halo's installation directory out of `Program Files (x86)`, like to your `C:` drive, `Desktop` or `Documents`.

### Differences from Halo PC

Beyond supporting custom maps and lacking the campaign, some notable differences from retail are:

*   Addition of the the gamemode info menu (hold F2).
*   Addition of the teammate names toggle (hold F3).
*   Addition of new server-related console commands like `sv_say`.
*   Regression in rendering of certain objects through fog.
*   Some tags were modified, such as stun effects, possibly as a workaround for netcode desyncs.

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/custom-edition/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
