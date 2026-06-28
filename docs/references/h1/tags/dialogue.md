# Halo 1 Tag: dialogue

## Summary

The **dialogue** tag group gives units") situational sounds, like when they take damage, see allies die, or are startled. Dialogue usage is not just limited to AI bipeds; it's responsible for the player's death sounds and can even be referenced for vehicles, though not all situations apply. Dialogue is an important part of Halo's game design and helps communicate the internal state of characters to the player.

## Tag Information

Tag group: `dialogue`.

HaloScript entries:

- `ai_print_speech [boolean]`
- `ai_print_vocalizations [boolean]`
- `ai_render_dialogue_variants [boolean]`

Defined fields and enum values mentioned:

- advance
- advance re
- alert friend
- alert friend re
- alert lost contact
- alert lost contact re
- alliance broken
- alliance reformed
- alone
- anyone kill grenade cm
- attempted flee
- attempted flee re
- berserk
- blocked
- blocked re
- celebration
- check body enemy
- check body friend
- cover
- damaged enemy
- damaged enemy cm
- damaged friend
- damaged friend player
- dead friend found
- death agonizing
- death falling
- death flying
- death instant
- death quiet
- death violent
- dive
- flee
- flee leader died
- flee re
- friend betrayed
- friend died
- friend killed by covenant
- friend killed by enemy
- friend killed by enemy player
- friend killed by flood
- friend killed by friend
- friend killed by friendly player
- friend killed by sentinel
- friend player died
- grenade danger enemy
- grenade danger friend
- grenade danger self
- grenade sighted
- grenade startle
- grenade throwing
- group uncover
- group uncover re
- hiding finished
- hurt enemy
- hurt enemy bullet
- hurt enemy cm
- hurt enemy explosion
- hurt enemy flame
- hurt enemy grenade
- hurt enemy melee
- hurt enemy mountedweapon
- hurt enemy needler
- hurt enemy plasma
- hurt enemy re
- hurt enemy shotgun
- hurt enemy sniper
- hurt enemy vehicle
- hurt friend
- hurt friend player
- hurt friend re
- idle combat
- idle flee
- idle noncombat
- killed enemy
- killed enemy bullet
- killed enemy cm
- killed enemy covenant
- killed enemy covenant cm
- killed enemy explosion
- killed enemy flame

## Details

The **dialogue** tag group gives units") situational sounds, like when they take damage, see allies die, or are startled. Dialogue usage is not just limited to AI bipeds; it's responsible for the player's death sounds and can even be referenced for vehicles, though not all situations apply. Dialogue is an important part of Halo's game design and helps communicate the internal state of characters to the player.

The randomization of voice lines is not part of the dialogue tag but rather the referenced sound tags, which can contain multiple permutations.

The following stock dialogue tags correspond to canonical characters:

| Character | Tag path |
| --- | --- |
| 343 Guilty Spark | `sound\dialog\monitor\monitor` |
| Jacob Keyes | `sound\dialog\captain\captain` |
| Avery Johnson | `sound\dialog\sargeant\conditional\sargeant` |
| Marcus Stacker | `sound\dialog\sarge2\conditional\sarge2` |
| Chips Dubbo | `sound\dialog\marines\aussie\conditional\aussie` |
| Private Bisenti | `sound\dialog\marines\bisenti\conditional\bisenti` |
| M. Fitzgerald | `sound\dialog\marines\fitzgerald\conditional\fitzgerald` |
| Manuel Mendoza | `sound\dialog\marines\mendoza\conditional\mendoza` |

Additional level-specific character sound tags, e.g. for cinematics, can be found under the `sound\dialog\x**` folders.

The following are related functions that you can use in your scenario scripts and/or debug globals that you can enter into the developer console for troubleshooting.

|  | Function/global | Type |
| --- | --- | --- |
|  | ``` (<void> ai_dialogue_triggers <boolean>) (ai_dialogue_triggers true) (ai_dialogue_triggers false) ``` Turns impromptu dialogue on or off. When off, units will still play some sounds (like when they take damage) but will not speak when exiting vehicles, seeing friends die, etc. | Function |
|  | `(ai_print_speech [boolean])` Displays red text over AI whenever they vocalize, with the name of the dialogue field played. For example, `pain body minor`. | Global |
|  | `(ai_print_vocalizations [boolean])` Prints vocalizations to the console as they happen, including the encounter, squad, actor, dialogue type (e.g. exclaim), and line. For example, `beach_lz/camp_center_grunt/grunt: talk flee [72/flee]`. | Global |
|  | `(ai_render_dialogue_variants [boolean])` Shows pink text overlays over each unit with dialogue showing which variant they use, like if marines are a "mendoza" or a "bisenti". For example, `variant 11 dialogue 0 aussie`. | Global |

### Structure and fields

| Field | Type | Comments |
| --- | --- | --- |
| idle noncombat | `TagDependency`: sound |  |
| idle combat | `TagDependency`: sound |  |
| idle flee | `TagDependency`: sound |  |
| pain body minor | `TagDependency`: sound |  |
| pain body major | `TagDependency`: sound |  |
| pain shield | `TagDependency`: sound |  |
| pain falling | `TagDependency`: sound |  |
| scream fear | `TagDependency`: sound |  |
| scream pain | `TagDependency`: sound |  |
| maimed limb | `TagDependency`: sound |  |
| maimed head | `TagDependency`: sound |  |
| death quiet | `TagDependency`: sound |  |
| death violent | `TagDependency`: sound |  |
| death falling | `TagDependency`: sound |  |
| death agonizing | `TagDependency`: sound |  |
| death instant | `TagDependency`: sound |  |
| death flying | `TagDependency`: sound |  |
| damaged friend | `TagDependency`: sound |  |
| damaged friend player | `TagDependency`: sound |  |
| damaged enemy | `TagDependency`: sound |  |
| damaged enemy cm | `TagDependency`: sound |  |
| hurt friend | `TagDependency`: sound |  |
| hurt friend re | `TagDependency`: sound |  |
| hurt friend player | `TagDependency`: sound |  |
| hurt enemy | `TagDependency`: sound |  |
| hurt enemy re | `TagDependency`: sound |  |
| hurt enemy cm | `TagDependency`: sound |  |
| hurt enemy bullet | `TagDependency`: sound |  |
| hurt enemy needler | `TagDependency`: sound |  |
| hurt enemy plasma | `TagDependency`: sound |  |
| hurt enemy sniper | `TagDependency`: sound |  |
| hurt enemy grenade | `TagDependency`: sound |  |
| hurt enemy explosion | `TagDependency`: sound |  |
| hurt enemy melee | `TagDependency`: sound |  |
| hurt enemy flame | `TagDependency`: sound |  |
| hurt enemy shotgun | `TagDependency`: sound |  |
| hurt enemy vehicle | `TagDependency`: sound |  |
| hurt enemy mountedweapon | `TagDependency`: sound |  |
| killed friend | `TagDependency`: sound |  |
| killed friend cm | `TagDependency`: sound |  |
| killed friend player | `TagDependency`: sound |  |
| killed friend player cm | `TagDependency`: sound |  |
| killed enemy | `TagDependency`: sound |  |
| killed enemy cm | `TagDependency`: sound |  |
| killed enemy player | `TagDependency`: sound |  |
| killed enemy player cm | `TagDependency`: sound |  |
| killed enemy covenant | `TagDependency`: sound |  |
| killed enemy covenant cm | `TagDependency`: sound |  |
| killed enemy floodcombat | `TagDependency`: sound |  |
| killed enemy floodcombat cm | `TagDependency`: sound |  |
| killed enemy floodcarrier | `TagDependency`: sound |  |
| killed enemy floodcarrier cm | `TagDependency`: sound |  |
| killed enemy sentinel | `TagDependency`: sound |  |
| killed enemy sentinel cm | `TagDependency`: sound |  |
| killed enemy bullet | `TagDependency`: sound |  |
| killed enemy needler | `TagDependency`: sound |  |
| killed enemy plasma | `TagDependency`: sound |  |
| killed enemy sniper | `TagDependency`: sound |  |
| killed enemy grenade | `TagDependency`: sound |  |
| killed enemy explosion | `TagDependency`: sound |  |
| killed enemy melee | `TagDependency`: sound |  |
| killed enemy flame | `TagDependency`: sound |  |
| killed enemy shotgun | `TagDependency`: sound |  |
| killed enemy vehicle | `TagDependency`: sound |  |
| killed enemy mountedweapon | `TagDependency`: sound |  |
| killing spree | `TagDependency`: sound |  |
| player kill cm | `TagDependency`: sound |  |
| player kill bullet cm | `TagDependency`: sound |  |
| player kill needler cm | `TagDependency`: sound |  |
| player kill plasma cm | `TagDependency`: sound |  |
| player kill sniper cm | `TagDependency`: sound |  |
| anyone kill grenade cm | `TagDependency`: sound |  |
| player kill explosion cm | `TagDependency`: sound |  |
| player kill melee cm | `TagDependency`: sound |  |
| player kill flame cm | `TagDependency`: sound |  |
| player kill shotgun cm | `TagDependency`: sound |  |
| player kill vehicle cm | `TagDependency`: sound |  |
| player kill mountedweapon cm | `TagDependency`: sound |  |
| player killling spree cm | `TagDependency`: sound |  |
| friend died | `TagDependency`: sound |  |
| friend player died | `TagDependency`: sound |  |
| friend killed by friend | `TagDependency`: sound |  |
| friend killed by friendly player | `TagDependency`: sound |  |
| friend killed by enemy | `TagDependency`: sound |  |
| friend killed by enemy player | `TagDependency`: sound |  |
| friend killed by covenant | `TagDependency`: sound |  |
| friend killed by flood | `TagDependency`: sound |  |
| friend killed by sentinel | `TagDependency`: sound |  |
| friend betrayed | `TagDependency`: sound |  |
| new combat alone | `TagDependency`: sound |  |
| new enemy recent combat | `TagDependency`: sound |  |
| old enemy sighted | `TagDependency`: sound |  |
| unexpected enemy | `TagDependency`: sound |  |
| dead friend found | `TagDependency`: sound |  |
| alliance broken | `TagDependency`: sound |  |
| alliance reformed | `TagDependency`: sound |  |
| grenade throwing | `TagDependency`: sound |  |
| grenade sighted | `TagDependency`: sound |  |
| grenade startle | `TagDependency`: sound |  |
| grenade danger enemy | `TagDependency`: sound |  |
| grenade danger self | `TagDependency`: sound |  |
| grenade danger friend | `TagDependency`: sound |  |
| new combat group re | `TagDependency`: sound |  |
| new combat nearby re | `TagDependency`: sound |  |
| alert friend | `TagDependency`: sound |  |
| alert friend re | `TagDependency`: sound |  |
| alert lost contact | `TagDependency`: sound |  |
| alert lost contact re | `TagDependency`: sound |  |
| blocked | `TagDependency`: sound |  |
| blocked re | `TagDependency`: sound |  |
| search start | `TagDependency`: sound |  |
| search query | `TagDependency`: sound |  |
| search query re | `TagDependency`: sound |  |
| search report | `TagDependency`: sound |  |
| search abandon | `TagDependency`: sound |  |
| search group abandon | `TagDependency`: sound |  |
| group uncover | `TagDependency`: sound |  |
| group uncover re | `TagDependency`: sound |  |
| advance | `TagDependency`: sound |  |
| advance re | `TagDependency`: sound |  |
| retreat | `TagDependency`: sound |  |
| retreat re | `TagDependency`: sound |  |
| cover | `TagDependency`: sound |  |
| sighted friend player | `TagDependency`: sound |  |
| shooting | `TagDependency`: sound |  |
| shooting vehicle | `TagDependency`: sound |  |
| shooting berserk | `TagDependency`: sound |  |
| shooting group | `TagDependency`: sound |  |
| shooting traitor | `TagDependency`: sound |  |
| taunt | `TagDependency`: sound |  |
| taunt re | `TagDependency`: sound |  |
| flee | `TagDependency`: sound |  |
| flee re | `TagDependency`: sound |  |
| flee leader died | `TagDependency`: sound |  |
| attempted flee | `TagDependency`: sound |  |
| attempted flee re | `TagDependency`: sound |  |
| lost contact | `TagDependency`: sound |  |
| hiding finished | `TagDependency`: sound |  |
| vehicle entry | `TagDependency`: sound |  |
| vehicle exit | `TagDependency`: sound |  |
| vehicle woohoo | `TagDependency`: sound |  |
| vehicle scared | `TagDependency`: sound |  |
| vehicle collision | `TagDependency`: sound |  |
| partially sighted | `TagDependency`: sound |  |
| nothing there | `TagDependency`: sound |  |
| pleading | `TagDependency`: sound |  |
| surprise | `TagDependency`: sound |  |
| berserk | `TagDependency`: sound |  |
| melee attack | `TagDependency`: sound |  |
| dive | `TagDependency`: sound |  |
| uncover exclamation | `TagDependency`: sound |  |
| leap attack | `TagDependency`: sound |  |
| resurrection | `TagDependency`: sound |  |
| celebration | `TagDependency`: sound |  |
| check body enemy | `TagDependency`: sound |  |
| check body friend | `TagDependency`: sound |  |
| shooting dead enemy | `TagDependency`: sound |  |
| shooting dead enemy player | `TagDependency`: sound |  |
| alone | `TagDependency`: sound |  |
| unscathed | `TagDependency`: sound |  |
| seriously wounded | `TagDependency`: sound |  |
| seriously wounded re | `TagDependency`: sound |  |
| massacre | `TagDependency`: sound |  |
| massacre re | `TagDependency`: sound |  |
| rout | `TagDependency`: sound |  |
| rout re | `TagDependency`: sound |  |

## Provenance

Adapted from the Reclaimers Library Halo 1 tag page for `dialogue`. Retrieved 2026-06-24. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
