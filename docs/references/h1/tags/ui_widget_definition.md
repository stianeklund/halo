# Halo 1 Tag: ui_widget_definition

## Summary

55.   unicode_string_list          56.   unit_hud_interface          57.   virtual_keyboard          58.   weapon_hud_interface          59.   weather_particle_system          60.   wind

## Tag Information

Tag group: `ui_widget_definition`.

Defined fields and enum values mentioned:

- background bitmap
- bounds
- bounds bottom
- bounds left
- bounds right
- bounds top
- child widgets
- child widgets custom controller index
- child widgets flags
- child widgets flags use custom controller index
- child widgets horizontal offset
- child widgets name
- child widgets vertical offset
- child widgets widget tag
- conditional widgets
- conditional widgets custom controller index
- conditional widgets flags
- conditional widgets flags load if event handler function fails
- conditional widgets name
- conditional widgets widget tag
- controller index
- controller index any player
- controller index player 1
- controller index player 2
- controller index player 3
- controller index player 4
- event handlers
- event handlers event type
- event handlers event type a button
- event handlers event type b button
- event handlers event type back button
- event handlers event type black button
- event handlers event type created
- event handlers event type custom activation
- event handlers event type deleted
- event handlers event type double click
- event handlers event type dpad down
- event handlers event type dpad left
- event handlers event type dpad right
- event handlers event type dpad up
- event handlers event type get focus
- event handlers event type left analog stick down
- event handlers event type left analog stick left
- event handlers event type left analog stick right
- event handlers event type left analog stick up
- event handlers event type left analog stick up 1
- event handlers event type left mouse
- event handlers event type left thumb
- event handlers event type left trigger
- event handlers event type lose focus
- event handlers event type middle mouse
- event handlers event type post render
- event handlers event type right analog stick down
- event handlers event type right analog stick left
- event handlers event type right analog stick right
- event handlers event type right mouse
- event handlers event type right thumb
- event handlers event type right trigger
- event handlers event type start button
- event handlers event type white button
- event handlers event type x button
- event handlers event type y button
- event handlers flags
- event handlers flags close all widgets
- event handlers flags close current widget
- event handlers flags close other widget
- event handlers flags give focus to widget
- event handlers flags go back to previous widget
- event handlers flags open widget
- event handlers flags reload other widget
- event handlers flags reload self
- event handlers flags replace self w widget
- event handlers flags run function
- event handlers flags run scenario script
- event handlers flags try to branch on failure
- event handlers function
- event handlers function 1wide plyr prof set for game
- event handlers function 3wide plyr prof set for game
- event handlers function audio screen defaults
- event handlers function begin music fade out

## Details

1. The Reclaimers Library

1. Size on screen
 1. game data inputs
 2. event handlers
 3. search and replace functions
 4. conditional widgets
 5. child widgets

## ui_ widget_ definition

**ui_ widget_ definition (`DeLa`?)**

Direct references
* bitmap
* unicode_ string_ list
* font
* ui_ widget_ definition
* sound

Referenced by: ui_ widget_ definition

* H1-Guerilla
* invader-edit
* invader-edit-qt
* OS_ Guerilla
* Mozzarilla
* reclaimer-python

...

## Size on screen

The game uses a 640x480 UI space stretched to the player's resolution. The size of widgets within this space is usually 1:1 with their bitmap's size, making it hard to scale them down while retaining detail. You can work around it somewhat by using negative _bounds_ values, but this doesn't help with some existing widget bitmaps that are heavily cropped by bounds already (like map previews, which use the same bitmaps from Xbox but are cropped for PC).

## Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| widget type | `enum` |  |
Option values:
- container (`0x0`)
- text box (`0x1`)
- spinner list (`0x2`)
- column list (`0x3`)
- game model not implemented (`0x4`)
- movie not implemented (`0x5`)
- custom not implemented (`0x6`)
| controller index | `enum` |  |
Option values:
- player 1 (`0x0`)
- player 2 (`0x1`)
- player 3 (`0x2`)
- player 4 (`0x3`)
- any player (`0x4`)
| name | `TagString` |  |
| bounds | `Rectangle2D` |  |
Field values:
- top (`int16`)
- left (`int16`)
- bottom (`int16`)
- right (`int16`)
| flags | `bitfield` |  |
Flag values:
- pass unhandled events to focused child (`0x1`)
- pause game time (`0x2`)
- flash background bitmap (`0x4`)
- dpad up down tabs thru children (`0x8`)
- dpad left right tabs thru children (`0x10`)
- dpad up down tabs thru list items (`0x20`)
- dpad left right tabs thru list items (`0x40`)
- dont focus a specific child widget (`0x80`)
- pass unhandled events to all children (`0x100`)
- render regardless of controller index (`0x200`)
- pass handled events to all children (`0x400`)
- return to main menu if no history (`0x800`)
- always use tag controller index (`0x1000`)
- always use nifty render fx (`0x2000`)
- don't push history (`0x4000`)
- force handle mouse (`0x8000`)
| milliseconds to auto close | `uint32` |  |
| milliseconds auto close fade time | `uint32` |  |
| background bitmap | `TagDependency`: bitmap |  |
| game data inputs | `Block` | * HEK max count: 64 |
Option values:
- function (`enum`)
- null (`0x0`)
- player settings menu update desc (`0x1`)
- unused (`0x2`)
- playlist settings menu update desc (`0x3`)
- gametype select menu update desc (`0x4`)
- multiplayer type menu update desc (`0x5`)
- solo level select update (`0x6`)
- difficulty menu update desc (`0x7`)
- build number textbox only (`0x8`)
- server list update (`0x9`)
- network pregame status update (`0xA`)
- splitscreen pregame status update (`0xB`)
- net splitscreen prejoin players (`0xC`)
- mp profile list update (`0xD`)
- 3wide player profile list update (`0xE`)
- plyr prof edit select menu upd8 (`0xF`)
- player profile small menu update (`0x10`)
- game settings lists text update (`0x11`)
- solo game objective text (`0x12`)
- color picker update (`0x13`)
- game settings lists pic update (`0x14`)
- main menu fake animate (`0x15`)
- mp level select update (`0x16`)
- get active plyr profile name (`0x17`)
- get edit plyr profile name (`0x18`)
- get edit game settings name (`0x19`)
- get active plyr profile color (`0x1A`)
- mp set textbox map name (`0x1B`)
- mp set textbox game ruleset (`0x1C`)
- mp set textbox teams noteams (`0x1D`)
- mp set textbox score limit (`0x1E`)
- mp set textbox score limit type (`0x1F`)
- mp set bitmap for map (`0x20`)
- mp set bitmap for ruleset (`0x21`)
- mp set textbox (`0x22`)
- mp edit profile set rule text (`0x23`)
- system link status check (`0x24`)
- mp game directions (`0x25`)
- teams no teams bitmap update (`0x26`)
- warn if diff will nuke saved game (`0x27`)
- dim if no net cable (`0x28`)
- pause game set textbox inverted (`0x29`)
- dim unless two controllers (`0x2A`)
- controls update menu (`0x2B`)
- video menu update (`0x2C`)
- gamespy screen update (`0x2D`)
- common button bar update (`0x2E`)
- gamepad update menu (`0x2F`)
- server settings update (`0x30`)
- audio menu update (`0x31`)
- mp prof vehicles update (`0x32`)
- solo map list update (`0x33`)
- mp map list update (`0x34`)
- gt select list update (`0x35`)
- gt edit list update (`0x36`)
- load game list update (`0x37`)
- checking for updates (`0x38`)
- direct ip connect update (`0x39`)
- network settings update (`0x3A`)
| event handlers | `Block` | * HEK max count: 32 |
Option values:
- flags (`bitfield`)
- close current widget (`0x1`)
- close other widget (`0x2`)
- close all widgets (`0x4`)
- open widget (`0x8`)
- reload self (`0x10`)
- reload other widget (`0x20`)
- give focus to widget (`0x40`)
- run function (`0x80`)
- replace self w widget (`0x100`)
- go back to previous widget (`0x200`)
- run scenario script (`0x400`)
- try to branch on failure (`0x800`)
- event type (`enum`)
- a button (`0x0`)
- b button (`0x1`)
- x button (`0x2`)
- y button (`0x3`)
- black button (`0x4`)
- white button (`0x5`)
- left trigger (`0x6`)
- right trigger (`0x7`)
- dpad up (`0x8`)
- dpad down (`0x9`)
- dpad left (`0xA`)
- dpad right (`0xB`)
- start button (`0xC`)
- back button (`0xD`)
- left thumb (`0xE`)
- right thumb (`0xF`)
- left analog stick up (`0x10`)
- left analog stick down (`0x11`)
- left analog stick left (`0x12`)
- left analog stick right (`0x13`)
- left analog stick up 1 (`0x14`)
- right analog stick down (`0x15`)
- right analog stick left (`0x16`)
- right analog stick right (`0x17`)
- created (`0x18`)
- deleted (`0x19`)
- get focus (`0x1A`)
- lose focus (`0x1B`)
- left mouse (`0x1C`)
- middle mouse (`0x1D`)
- right mouse (`0x1E`)
- double click (`0x1F`)
- custom activation (`0x20`)
- post render (`0x21`)
- function (`enum`)
- null (`0x0`)
- list goto next item (`0x1`)
- list goto previous item (`0x2`)
- unused (`0x3`)
- unused1 (`0x4`)
- initialize sp level list solo (`0x5`)
- initialize sp level list coop (`0x6`)
- dispose sp level list (`0x7`)
- solo level set map (`0x8`)
- set difficulty (`0x9`)
- start new game (`0xA`)
- pause game restart at checkpoint (`0xB`)
- pause game restart level (`0xC`)
- pause game return to main menu (`0xD`)
- clear multiplayer player joins (`0xE`)
- join controller to mp game (`0xF`)
- initialize net game server list (`0x10`)
- start network game server (`0x11`)
- dispose net game server list (`0x12`)
- shutdown network game (`0x13`)
- net game join from server list (`0x14`)
- split screen game initialize (`0x15`)
- coop game initialize (`0x16`)
- main menu intialize (`0x17`)
- mp type menu initialize (`0x18`)
- pick play stage for quick start (`0x19`)
- mp level list initialize (`0x1A`)
- mp level list dispose (`0x1B`)
- mp level select (`0x1C`)
- mp profiles list initialize (`0x1D`)
- mp profiles list dispose (`0x1E`)
- mp profile set for game (`0x1F`)
- swap player team (`0x20`)
- net game join player (`0x21`)
- player profile list initialize (`0x22`)
- player profile list dispose (`0x23`)
- 3wide plyr prof set for game (`0x24`)
- 1wide plyr prof set for game (`0x25`)
- mp profile begin editing (`0x26`)
- mp profile end editing (`0x27`)
- mp profile set game engine (`0x28`)
- mp profile change name (`0x29`)
- mp profile set ctf rules (`0x2A`)
- mp profile set koth rules (`0x2B`)
- mp profile set slayer rules (`0x2C`)
- mp profile set oddball rules (`0x2D`)
- mp profile set racing rules (`0x2E`)
- mp profile set player options (`0x2F`)
- mp profile set item options (`0x30`)
- mp profile set indicator opts (`0x31`)
- mp profile init game engine (`0x32`)
- mp profile init name (`0x33`)
- mp profile init ctf rules (`0x34`)
- mp profile init koth rules (`0x35`)
- mp profile init slayer rules (`0x36`)
- mp profile init oddball rules (`0x37`)
- mp profile init racing rules (`0x38`)
- mp profile init player opts (`0x39`)
- mp profile init item options (`0x3A`)
- mp profile init indicator opts (`0x3B`)
- mp profile save changes (`0x3C`)
- color picker menu initialize (`0x3D`)
- color picker menu dispose (`0x3E`)
- color picker select color (`0x3F`)
- player profile begin editing (`0x40`)
- player profile end editing (`0x41`)
- player profile change name (`0x42`)
- player profile save changes (`0x43`)
- plyr prf init cntl settings (`0x44`)
- plyr prf init adv cntl set (`0x45`)
- plyr prf save cntl settings (`0x46`)
- plyr prf save adv cntl set (`0x47`)
- mp game player quit (`0x48`)
- main menu switch to solo game (`0x49`)
- request del player profile (`0x4A`)
- request del playlist profile (`0x4B`)
- final del player profile (`0x4C`)
- final del playlist profile (`0x4D`)
- cancel profile delete (`0x4E`)
- create edit playlist profile (`0x4F`)
- create edit player profile (`0x50`)
- net game speed start (`0x51`)
- net game delay start (`0x52`)
- net server accept conx (`0x53`)
- net server defer start (`0x54`)
- net server allow start (`0x55`)
- disable if no xdemos (`0x56`)
- run xdemos (`0x57`)
- sp reset controller choices (`0x58`)
- sp set p1 controller choice (`0x59`)
- sp set p2 controller choice (`0x5A`)
- error if no network connection (`0x5B`)
- start server if none advertised (`0x5C`)
- net game unjoin player (`0x5D`)
- close if not editing profile (`0x5E`)
- exit to xbox dashboard (`0x5F`)
- new campaign chosen (`0x60`)
- new campaign decision (`0x61`)
- pop history stack once (`0x62`)
- difficulty menu init (`0x63`)
- begin music fade out (`0x64`)
- new game if no plyr profiles (`0x65`)
- exit gracefully to xbox dashboard (`0x66`)
- pause game invert pitch (`0x67`)
- start new coop game (`0x68`)
- pause game invert spinner get (`0x69`)
- pause game invert spinner set (`0x6A`)
- main menu quit game (`0x6B`)
- mouse emit accept event (`0x6C`)
- mouse emit back event (`0x6D`)
- mouse emit dpad left event (`0x6E`)
- mouse emit dpad right event (`0x6F`)
- mouse spinner 3wide click (`0x70`)
- controls screen init (`0x71`)
- video screen init (`0x72`)
- controls begin binding (`0x73`)
- gamespy screen init (`0x74`)
- gamespy screen dispose (`0x75`)
- gamespy select header (`0x76`)
- gamespy select item (`0x77`)
- gamespy select button (`0x78`)
- plr prof init mouse set (`0x79`)
- plr prof change mouse set (`0x7A`)
- plr prof init audio set (`0x7B`)
- plr prof change audio set (`0x7C`)
- plr prof change video set (`0x7D`)
- controls screen dispose (`0x7E`)
- controls screen change set (`0x7F`)
- mouse emit x event (`0x80`)
- gamepad screen init (`0x81`)
- gamepad screen dispose (`0x82`)
- gamepad screen change gamepads (`0x83`)
- gamepad screen select item (`0x84`)
- mouse screen defaults (`0x85`)
- audio screen defaults (`0x86`)
- video screen defaults (`0x87`)
- controls screen defaults (`0x88`)
- profile set edit begin (`0x89`)
- profile manager delete (`0x8A`)
- profile manager select (`0x8B`)
- gamespy dismiss error (`0x8C`)
- server settings init (`0x8D`)
- ss edit server name (`0x8E`)
- ss edit server password (`0x8F`)
- ss start game (`0x90`)
- video test dialog init (`0x91`)
- video test dialog dispose (`0x92`)
- video test dialog accept (`0x93`)
- gamespy dismiss filters (`0x94`)
- gamespy update filter settings (`0x95`)
- gamespy back handler (`0x96`)
- mouse spinner 1wide click (`0x97`)
- controls back handler (`0x98`)
- controls advanced launch (`0x99`)
- controls advanced ok (`0x9A`)
- mp pause menu open (`0x9B`)
- mp game options open (`0x9C`)
- mp choose team (`0x9D`)
- mp prof init vehicle options (`0x9E`)
- mp prof save vehicle options (`0x9F`)
- single prev cl item activated (`0xA0`)
- mp prof init teamplay options (`0xA1`)
- mp prof save teamplay options (`0xA2`)
- mp game options choose (`0xA3`)
- emit custom activation event (`0xA4`)
- plr prof cancel audio set (`0xA5`)
- plr prof init network options (`0xA6`)
- plr prof save network options (`0xA7`)
- credits post render (`0xA8`)
- difficulty item select (`0xA9`)
- credits initialize (`0xAA`)
- credits dispose (`0xAB`)
- gamespy get patch (`0xAC`)
- video screen dispose (`0xAD`)
- campaign menu init (`0xAE`)
- campaign menu continue (`0xAF`)
- load game menu init (`0xB0`)
- load game menu dispose (`0xB1`)
- load game menu activated (`0xB2`)
- solo menu save checkpoint (`0xB3`)
- mp type set mode (`0xB4`)
- checking for updates ok (`0xB5`)
- checking for updates dismiss (`0xB6`)
- direct ip connect init (`0xB7`)
- direct ip connect go (`0xB8`)
- direct ip edit field (`0xB9`)
- network settings edit a port (`0xBA`)
- network settings defaults (`0xBB`)
- load game menu delete request (`0xBC`)
- load game menu delete finish (`0xBD`)
- widget tag (`TagDependency`: ui_widget_definition)
- sound effect (`TagDependency`: sound)
- script (`TagString`)
| search and replace functions | `Block` | * HEK max count: 32 |
Option values:
- search string (`TagString`)
- replace function (`enum`)
- null (`0x0`)
- widget s controller (`0x1`)
- build number (`0x2`)
- pid (`0x3`)
| text label unicode strings list | `TagDependency`: unicode_string_list |  |
| text font | `TagDependency`: font |  |
| text color | `ColorARGB` |  |
| justification | `enum` |  |
Option values:
- left justify (`0x0`)
- right justify (`0x1`)
- center justify (`0x2`)
| flags 1 | `bitfield` |  |
Flag values:
- editable (`0x1`)
- password (`0x2`)
- flashing (`0x4`)
- don't do that weird focus test (`0x8`)
| string list index | `uint16` |  |
| horiz offset | `int16` |  |
| vert offset | `int16` |  |
| flags 2 | `bitfield` |  |
Flag values:
- list items generated in code (`0x1`)
- list items from string list tag (`0x2`)
- list items only one tooltip (`0x4`)
- list single preview no scroll (`0x8`)
| list header bitmap | `TagDependency`: bitmap |  |
| list footer bitmap | `TagDependency`: bitmap |  |
| header bounds | `Rectangle2D`? |  |
| footer bounds | `Rectangle2D`? |  |
| extended description widget | `TagDependency`: ui_widget_definition |  |
| conditional widgets | `Block` | * HEK max count: 32 |
Flag values:
- widget tag (`TagDependency`: ui_widget_definition)
- name (`TagString`)
- flags (`bitfield`)
- load if event handler function fails (`0x1`)
- custom controller index (`uint16`)
| child widgets | `Block` | * HEK max count: 32 |
Flag values:
- widget tag (`TagDependency`: ui_widget_definition)
- name (`TagString`)
- flags (`bitfield`)
- use custom controller index (`0x1`)
- custom controller index (`uint16`)
- vertical offset (`int16`)
- horizontal offset (`int16`)

## Acknowledgements

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `ui_widget_definition`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
