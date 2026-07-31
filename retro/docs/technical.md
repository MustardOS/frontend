# Technical Notes

These technical notes describe how the pieces documented elsewhere in [`docs/`](.) fit together at runtime and build
time. You do not need to read them to use or extend a single feature. Start here if you need to understand the whole
lifecycle of a running Pickles session.

## Startup Sequence

Install SIGUSR1/SIGUSR2 (sleep/wake) signal handlers > load the device and configuration > call
`init_module`/`init_theme`/`init_display` > call `board_init`/`mux_input_open` > call `core_open` (`dlopen`) > call
`state_saves_init` (per-core save-state gate) > call `core_load_content` (archive extraction or VFS streaming, with soft
patching first) > initialise the SRAM, cheat and overlay bridges > call `gamestate_init` > call
`options_capture_baseline` > call `session_settings_init` (three-tier settings INI) > configure the hardware render target
if the core negotiated one > open audio and initialise video > select and automatically load the most recent compatible
save state (unless `--fresh` is used). No emulation warm-up occurs when there is no state. The default resume path restores
immediately; a core that requires initial frames before deserialisation can set `savestate_warmup_frames = "N"` in its
RetroArch-style `.info` file > call `pause_menu_init`. Startup logs report the time spent loading content, setting up the
renderer, performing an optional warm-up, reading the state and deserialising it. _That's it._

## Main Loop

Poll input > handle idle and suspend signals (which may trigger an automatic save and toggle pause) > service the periodic
status and SRAM-flush timers > when paused, tick the pause-menu UI; otherwise, run the hotkey dispatcher and determine the
frame batch (a fast-forward batch or audio catch-up frames granted from measured headroom) > call `run_core_batch` (GL
context bracket, per-frame Frame Delay wait, late input poll, optional run-ahead preparation and `retro_run`) > flush the
video frame, refresh LVGL only when the HUD is dirty, composite and present > apply pacing sleeps for audio headroom, 50 Hz
content and slow motion _after_ presentation.

The gameplay-governor policy samples effective FPS once per second. Two consecutive windows below 97% of the core target
temporarily select the performance governor; eight windows above 99% restore the governor observed at session start. Fast
forward requests the boost directly, while pause and slow motion release it. Save, load and startup boosts use the same
nested ownership so none of these paths can restore the governor out of order.

Hidden frames (fast-forward intermediate frames, audio catch-up and run-ahead replays) skip the video path entirely and
report video disabled via `RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE`, so cooperating cores skip their own rendering too. When
the gameplay HUD is hidden, the compositor also omits LVGL, its shadow layer and theme chrome; explicit Pickles content
overlays remain controlled by the session overlay settings. On the dedicated-context direct presentation path, the
hardware quad captures SDL's GL restore state once and reuses it without synchronous state queries. Render targets,
rotation, mirroring, colour passes, overlays, animations, fades or visible UI automatically select the full
state-preserving path and invalidate the cached state.

## Shutdown

Tear down the pause menu, run-ahead system, video (including the hardware render target), overlay, audio and rumble bridges >
flush SRAM one last time > unload the content and core > close input > call `sdl_cleanup`.

## Settings Persistence

The `session_settings_t` structure (in `settings/settings.h`) holds every per-session setting: video (scaling, rotation,
mirroring, aspect ratio, integer scaling, texture filter, shimmer fix and border); viewport; colour grading, filters and
shaders; overlays; sound (volume, sample rate, latency profile and filter); input (rumble and analogue tuning); performance
(FPS limit, frame delay and run-ahead); screen information; hotkey enablement and speeds; automatic-save mode; and SRAM
flush interval.

Settings are stored in three INI tiers under `RETRO_SET_PATH` (`<share>/retro/settings/`):

```
settings/core/<core_name>.ini
settings/directory/<crc32-of-content-directory>.ini
settings/content/<content_basename>.ini
```

Pickles applies them in the order **core > directory > content**, so the most specific tier wins. Each tier stores only a
**delta**: saving writes the keys that differ from the tiers beneath it and removes the file entirely when nothing differs.
This approach ensures that a content-level override never pins unrelated settings against later core-level or
directory-level changes. `session_settings_init` captures a baseline immediately after loading. Comparing changes against
that baseline drives the 'Save Changes?' dialogue when the user leaves a settings screen, and saving writes only the
selected tier.

## Content Loading

The `core_load_content` function prefers to stream archive members directly to `need_fullpath` cores through the VFS
(`archive#member` convention, backed by a persistent extraction cache keyed by path, entry, size and modification time),
with extraction as a fallback. The soft patch (`patch.c`) is applied before the final path or data reaches
`retro_load_game`. Save-state, SRAM and settings paths derive from the core name, content base name and directory. This
prevents collisions between cores and directories, especially when content shares a name or directory structure.

## Build

The build produces `../bin/muxretro` through `retro/Makefile` (_sources listed per subdirectory_). It links against the shared
`libmuxcom`/`libmuxmod`/`libui` libraries plus `plutosvg`, `z`/`lzma` and `bz2`, and compiles the bundled
`common/libarchive` sources directly into the binary.
