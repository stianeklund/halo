# Halo 1 Reference: Usage

## Source Page

- Source: `https://c20.reclaimers.net/h1/source-data/rename-txt/`
- Local path: `source-data/rename-txt/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

The file `rename.txt` is for reusing animations under different names.

### Usage

Animation data files go in a folder named `animations` as described here. Create a text file named `rename` in that folder. Open the file in a text editor. Write one or more lines in the expected format. Save, and then go compile the animation data.

After compiling the animation data, Tool will try to read `rename.txt` and modify the model_ animations tag according the contents of that file.

### Format

To reuse an animation as another animation, write a line in this format:

`name-of-animation = name-of-animation`
There are three parts: the name of an animation that does not already exist, the equal sign, and the name of an animation that does exist.

H1CE Tool considers any spaces around the equal sign as part of the animations' names. Files typically do not have spaces at the beginning or at the end of their names. That means Tool probably will fail to find the animation, if there are any spaces on the right side of the equal sign.

H1A Tool seems to ignore spaces around the equal sign. They are not considered as part of the animations' names.

### Example

Source data files

```
first-person firing.JMM
first-person idle.JMM
first-person light-off.JMM
first-person melee.JMM
first-person moving.JMO
first-person overlays.JMO
first-person posing.JMM
first-person put-away.JMM
first-person ready.JMM
first-person reload-full.JMM
first-person reload-full2.JMM
first-person stealth-melee.JMM
first-person throw-grenade.JMM
rename.txt
```

Contents of `rename.txt`

`first-person reload-empty=first-person reload-full`
Output from Tool

```
### first-person firing.JMM
### first-person idle.JMM
### first-person light-off.JMM
### first-person melee.JMM
### first-person posing.JMM
### first-person put-away.JMM
### first-person ready.JMM
### first-person reload-full.JMM
### first-person reload-full2.JMM
### first-person stealth-melee.JMM
### first-person throw-grenade.JMM
### first-person moving.JMO
### first-person overlays.JMO
renamed "first-person reload-full" ==> "first-person reload-empty"

model animation compression saved 0 bytes
```

### Acknowledgements

Thanks to the following individuals for their research or contributions to this topic:

*   Abstract Ingenuity _(Research and documentation)_

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/source-data/rename-txt/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
