# Settings, Core Options, Cheats and Hotkeys

See [`architecture.md`](architecture.md#settings) for the file-by-file breakdown of `settings/`.

## Settings

Settings use switchable sections for **Profiles, Video, Visuals, Overlay, Display, Sound, Input, Storage** and, on
network-capable devices, **RetroAchievements**, followed by **Advanced**. Each section displays its settings directly;
only controls that need their own detailed screen open another menu. Safe prioritises steady play and reliable sound,
Balanced provides the recommended defaults, and Quality enables cleaner scaling and shimmer reduction when performance
allows. These built-in profiles change only their named picture, timing and audio fields. Controls, saves, colour
adjustments and every unrelated preference remain untouched. Pickles remembers the complete settings state when a
built-in profile is applied, so changing any setting removes its **Current** marker.

User-made profiles live in `/run/muos/storage/save/pickles/profile/`. Pickles reads ordinary `.ini` files from this
directory and shows a profile only when its optional core and content targets match the game being played. Leave both
targets out to make a profile available everywhere. Applying a profile changes only the valid settings named in its
`[settings]` section, then uses the normal save dialogue so the result can be kept for this content, its directory, its
core, or only the current session.

When the settings no longer match the current built-in or user-made profile, select **Save Current** in the **Profiles**
list. Name the profile with the same on-screen keyboard used by save states, then choose whether it is available for the
current content, the current core, or all content. Pickles records every supported scalar setting and selects the new
profile immediately. Highlight a user-made profile and press **X Delete** to remove it after confirmation. Built-in
profiles cannot be deleted.

```ini
[profile]
version = 1
name = Handheld Smooth
core = mgba
content = Example Game.gba

[settings]
texture_filter = 4
shimmer_fix = 1
audio_latency_profile = 1
```

`core` matches the Pickles core name. `content` matches either the complete file name or the file name without its final
extension. Both comparisons ignore letter case. Unknown settings and invalid values are ignored. To keep opening the
menu fast and predictable, Pickles reads regular files only, limits each file to 64 KiB, examines at most 64 files and
shows at most 24 matching profiles in alphabetical file-name order.

New profiles use a safe file name derived from the entered display name, never overwrite an existing file, and are
published only after their complete contents have reached storage. Deletion is restricted to a regular profile file that
Pickles has already discovered inside its own profile directory. Pickles creates the profile directory when it is first
needed.

**Visuals** contains colour adjustment, filter and shader choices, while **Overlay** has its own section. **Display**
contains Adjustment and Cropping submenus, Reset Viewport and Screen Info directly. **Advanced** contains Core Options,
Performance and Save All so experienced users retain full control without placing technical choices in the normal path.
Hotkey Controls remain under Input. Every screen is driven by the table engine in
`settings/submenu.c`: a screen supplies label/glyph tables, a value/cycle switch, and optional action/child hooks. The
engine does section and row building, focus, navbar switching, hold repeats, the save dialogue, and submenu dispatch.
Adding a row is an enum entry, a label, a unique glyph name, and typically two switch cases.

Pickles also records the settings that most recently reached gameplay for each content item. If the next launch is
interrupted before gameplay becomes ready, the last working scalar settings are restored into the content layer and a
short notice is shown. The launch marker and snapshot are written atomically beside that content's settings; they do not
alter core options, controls or save data.

## Core Options, Cheats, Information

- Core options v0/v1/v2 (+ intl), and are generally categorised from the Libretro info files
- Cheats loaded from ini and applied via `retro_cheat_set`.
- The pause menu shows Achievements only when the current content has at least one available achievement or leaderboard.
- Information opens as a section-based list. Use **L/R Change**, or left and right while the section bar is selected, to
  move through Core, Content, Hashes, Video, Audio, System and finally Checklist. Checklist reports save protection,
  save space, Network Play capability and whether launch settings needed recovery. Content and System retain applied
  softpatches and individual BIOS file presence without repeating them in Checklist. Each hash label has its own
  `archivehash`, `contenthash` or `achievementhash` theme glyph, followed by its value as a separate full-width
  highlighted row. The other sections retain core name/version, content name, resolution, display/audio output and disc
  count. The footer shows **L/R Change** only while the section selector is highlighted, with no select action on
  informational rows.
- Performance retains manual timing controls and optional diagnostic capture. Beginner-facing performance choices live
  in Profiles, while the individual controls remain available under Advanced.
- Core capabilities retain a typed reason when unavailable. Pickles can distinguish an upstream core declaration, a
  packaged MustardOS compatibility rule and a feature that depends on save states, rather than exposing the same generic
  failure for every case.

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
