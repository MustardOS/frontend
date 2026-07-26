# Technical Notes

The dirty stuff, with the pieces described elsewhere in [`docs/`](.) actually fit together at runtime and at build time.
Not required reading to use or extend a single feature. Start here if you need to understand the whole lifecycle of a
running session of Pickles.

## Startup Sequence

Installs SIGUSR1/SIGUSR2 (sleep/wake) signal handlers > load device/config > `init_module`/`init_theme`/`init_display` >
`board_init`/`mux_input_open` > `core_open` (dlopen) > `state_saves_init` (per core savestate gate) >
`core_load_content` (archive extraction / VFS streaming and softpatching first) > init SRAM/cheats/overlay bridges >
`gamestate_init` > `options_capture_baseline` > `session_settings_init` (three tier settings INI) > configures the
hardware render target if the core negotiated one > open audio, init video > warm up and auto load the most recent save
state (unless `--fresh` is used and only when save states are enabled for the core) > `pause_menu_init`. _That's it._

## Main Loop

Poll input > idle/suspend-signal handling (may autosave + toggle pause) > periodic status and SRAM flush timers > if
paused, tick the pause menu UI else run the hotkey dispatcher... otherwise decide the frame batch (fast forward batch,
or audio catchup frames granted out of measured headroom) > `run_core_batch` (GL context bracket, per frame Frame Delay
wait, late input poll, optional runahead preparation, `retro_run`) > flush the video frame, refresh LVGL only when the
HUD is dirty, composite, present > pacing sleeps (audio headroom / 50 Hz / slow motion) _after_ present.

Hidden frames (fast forward intermediates, audio catchup, runahead replays) skip the video path entirely and report
video disabled via `RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE`, so cooperating cores skip their own rendering too.

## Shutdown

Tear down the pause menu, runahead, video (incl. hardware render target), overlay, audio, and rumble bridges > flush
SRAM one last time > unload content and the core > close input > `sdl_cleanup`.

## Settings Persistence

The `session_settings_t` (in `settings/settings.h`) holds every per-session setting - video
(scaling/rotate/mirror/aspect/integer scale/texture filter/shimmer fix/border), viewport, colour grading +
filter/shader, overlays, sound (volume/sample rate/latency profile/filter), input (rumble/analog tuning), performance
(fps limit/frame delay/run ahead), screen info, hotkey enables and speeds, auto-save mode, and SRAM flush interval.

Settings are stored as three `.ini` tiers under `RETRO_SET_PATH` (`<share>/retro/settings/`):

```
settings/core/<core_name>.ini
settings/directory/<crc32-of-content-directory>.ini
settings/content/<content_basename>.ini
```

Applied in the order of **core > directory > content**. So the most specific tier wins! Each tier stores only a
**delta** with saving writes just the keys that differ from the tiers beneath it (and removes the file entirely when
nothing differs), so a content-level override never pins unrelated settings against later core or directory level
changes. The `session_settings_init` snapshots a baseline right after loading. Dirty checking against it drives the
'Save Changes?' dialogue on leaving any settings screen, and saving writes only the tier the user picked.

## Content Loading

The `core_load_content` function prefers streaming archive members straight to `need_fullpath` cores via the VFS
(`archive#member` convention, backed by a persistent extraction cache keyed by path/entry/size/mtime), falling back to
extraction. The soft patch (`patch.c`) applies before the final path/data reaches `retro_load_game`. Savestate, SRAM,
and settings paths derive from the core name and content basename/directory. This way cores and directories never
collide especially for same content names and directory structure organisation.

## Build

Builds to `../bin/muxretro` via `retro/Makefile` (_sources listed per subdirectory_). Links against the shared
`libmuxcom`/`libmuxmod`/`libui` libraries plus `plutosvg`, `z`/`lzma` and `bz2`, and compiles the bundled
`common/libarchive` sources directly into the binary.
