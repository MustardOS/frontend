# Settings, Core Options, Cheats and Hotkeys

See [`architecture.md`](architecture.md#settings) for the file-by-file breakdown of `settings/`.

## Settings

Settings are organised into categories: **Hotkey Controls, Video (incl. Viewport), Display (incl. Colour Filter /
Shader), Sound, Input, Performance, Screen Info, Storage**. Every screen is driven by the table engine in
`settings/submenu.c`: a screen supplies label/glyph tables, a value/cycle switch, and optional action/child hooks. The
engine does row building, focus, navbar switching, hold repeats, the save dialogue, and submenu dispatch. Adding a row
is an enum entry, a label, a unique glyph name, and typically two switch cases.

## Core Options, Cheats, Information

- Core options v0/v1/v2 (+ intl), and are generally categorised from the Libretro info files
- Cheats loaded from ini and applied via `retro_cheat_set`.
- Information screen: core name/version, content name, CRC32 hash, applied softpatches, resolution, display/audio
  output, disc count, BIOS file presence.

## Hotkeys

All hotkeys are `MENU + <button>` combos, each individually toggleable in the Hotkey Controls screen:

| Combo                           | Action                                                                            |
|---------------------------------|-----------------------------------------------------------------------------------|
| MENU+R1                         | Toggle Fast Forward                                                               |
| MENU+R2                         | Quick Save                                                                        |
| MENU+L1                         | Toggle Slow Motion                                                                |
| MENU+L2                         | Quick Load                                                                        |
| MENU+B                          | Toggle Pause Content - freezes the core in place without opening the pause menu   |
| MENU+Y                          | Toggle FPS display                                                                |
| MENU+X                          | Cycle header visibility (None / Clock / Battery / Clock+Battery)                  |
| MENU+START                      | Quit (autosaves first if Auto Save covers "On Quit")                              |
| MENU (release, no combo)        | Open the pause menu                                                               |
| MENU (hold, in settings screen) | Peek at the content underneath for a live preview of the current display settings |

Fast Forward, Slow Motion, and Pause Content each have an independent on screen glyph toggle, so a hotkey can keep
working with its indicator hidden. Pausing this way won't disturb Fast Forward/Slow Motion. Resuming drops straight back
into whichever of the two, if any, was active before the pause. All rumble is suppressed while the pause menu is open.
