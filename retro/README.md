# muRetro

muRetro is MustardOS's own libretro core hosting frontend. It `dlopen()`s a libretro core directly and renders through
the same LVGL/SDL2 stack the rest of the frontend uses, instead of shelling out to RetroArch. The goal is a "MustardOS
Libretro" core kind with its own pause menu, core options, save states, cheats, and display settings that look and feel
like the rest of the frontend, for systems where the full RetroArch is not required.

## Invocation

```
muxretro <core.so> <content> [--fresh]
```

- `core.so` - path to the libretro core to `dlopen`.
- `content` - path to the game/content file (or an archive member, see [Content loading](#content-loading)).
- `--fresh` - skip the warm-up frames and most-recent-save-state auto-load that normally happen on launch.

## Layout

Sources are organised by subsystem, with explicit relative-path includes throughout:

```
retro/
  core/      entry point, core hosting, environment callback, run-ahead
  video/     frame pipeline, hardware render, colour grading, video UI screens
  audio/     audio bridge and sound settings screen
  input/     input/rumble/hotkey bridges and their UI screens
  state/     save states, SRAM, VFS, softpatching, BIOS checks
  settings/  session settings model, submenu engine, settings UI screens
  ui/        pause menu, core options, information, disc control, cheats
```

### core/

| File                        | Purpose                                                                                                                                                                                                                                           |
|-----------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `main.c`                    | Entry point: startup sequence, main loop (frame batching, pacing, run-ahead hook), shutdown.                                                                                                                                                      |
| `core.c` / `core.h`         | `dlopen`s the core `.so`, resolves every `retro_*` symbol into a global `current_core`, wires muxretro's own video/audio/input callbacks in via the core's `retro_set_*_cb` setters, loads content (archives, softpatches).                       |
| `environment.c`             | Implements `mux_retro_environment_cb` - the big `RETRO_ENVIRONMENT_*` switch (pixel format, directories, log, core options v0/v1/v2 + intl, disk control, messages, VFS, rumble, hardware render, AV info, frame time, throttle state, shutdown). |
| `runahead.c` / `runahead.h` | Preemptive-frames run-ahead (see [Run Ahead](#run-ahead)).                                                                                                                                                                                        |
| `muxretro.h`                | Shared declarations for every bridge and UI screen.                                                                                                                                                                                               |
| `libretro.h`                | Vendored upstream libretro API header.                                                                                                                                                                                                            |
| `paths.h`                   | Every `RETRO_*_PATH` macro used across the app.                                                                                                                                                                                                   |

### video/

| File                                    | Purpose                                                                                                                                                      |
|-----------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `video.c`                               | SDL2 texture pipeline: raw frame upload/conversion, all scaling modes (including Fit Screen and the Shimmer Fix snap), rotation, mirroring, texture filters. |
| `hw_render.c` / `hw_render.h`           | `RETRO_ENVIRONMENT_SET_HW_RENDER` support for GLES2 cores (see [Hardware render](#hardware-render)).                                                         |
| `colour.c` / `colour.h`                 | GLES2 colour grading (brightness/contrast/saturation/hue/gamma) plus filter/shader preset loading.                                                           |
| `overlay_bridge.c` / `overlay_bridge.h` | Predefined pattern overlays and per-game catalogue overlays, composited into the video content layer.                                                        |
| `frame_pacer.c`                         | Frame Delay: adaptive pre-run wait (p95 of recent frame costs) so input is sampled as late as possible before each frame.                                    |
| `ui_display.c`                          | Display screen (filter/shader pickers, colour grading, overlay).                                                                                             |
| `ui_videosettings.c`                    | Video screen (viewport entry, scaling, rotation, mirror, aspect, integer scale, texture filter, shimmer fix, border).                                        |
| `ui_viewport.c`                         | Viewport Offsets screen (X/Y offset, zoom, edge cropping, centre crop, reset).                                                                               |
| `ui_colfilter.c` / `ui_shader.c`        | Colour filter / shader picker screens.                                                                                                                       |

### audio/

| File                 | Purpose                                                                                                            |
|----------------------|--------------------------------------------------------------------------------------------------------------------|
| `audio.c`            | SDL audio device, lock-free SPSC sample ring, latency profiles, queued-ms watermarking, underrun fade, mute/pause. |
| `ui_soundsettings.c` | Sound screen (volume, sample rate, audio latency profile).                                                         |

### input/

| File                            | Purpose                                                                                                                                                                      |
|---------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `input_bridge.c`                | Input poll/state callbacks with epoch-based snapshotting (deterministic within a frame), analog deadzone/sensitivity transforms, multi-port support, suppress-until-release. |
| `hotkeys.c` / `hotkeys.h`       | MENU+X combo dispatcher (see [Hotkeys](#hotkeys)).                                                                                                                           |
| `rumble.c` / `rumble.h`         | Rumble bridge with board-specific on/off magnitude quirks and menu/replay suppression.                                                                                       |
| `nav_repeat.c` / `nav_repeat.h` | Shared d-pad hold-to-repeat helper used by every UI screen.                                                                                                                  |
| `ui_hotkeys.c`                  | Hotkey Controls screen.                                                                                                                                                      |
| `ui_inputsettings.c`            | Input screen (rumble, analog deadzone/anti-deadzone/sensitivity/invert Y).                                                                                                   |

### state/

| File                                | Purpose                                                                                                          |
|-------------------------------------|------------------------------------------------------------------------------------------------------------------|
| `state.c`                           | `state_save`/`state_load` with hardware-render context bracketing, serialize buffer slack, per-core disablement. |
| `gamestate.c` / `gamestate.h`       | Quicksave/autosave/numbered-slot save-state management, screenshot thumbnails, most-recent auto-load on launch.  |
| `sram.c` / `sram.h`                 | SRAM (battery save) bridge: dirty-aware, atomic (tmp+rename+fsync), written on a background worker thread.       |
| `vfs.c` / `vfs.h`                   | libretro VFS incl. reading content directly out of an archive member, with a persistent size/mtime-keyed cache.  |
| `content_hash.c` / `content_hash.h` | Background-threaded CRC32 of the content file, cached by size+mtime.                                             |
| `patch.c` / `patch.h`               | Softpatch engine - IPS, BPS, and UPS appliers, stacking numbered patches (`.ips`, `.1.ips`, ...).                |
| `bios_check.c` / `bios_check.h`     | Reads the core's RetroArch-style `.info` file for `firmware*` entries and checks presence.                       |
| `ui_gamestate.c`                    | Game State screen (slots, naming, preview mode).                                                                 |
| `ui_storagesettings.c`              | Storage screen (auto save, SRAM flush interval).                                                                 |

### settings/

| File                        | Purpose                                                                                                             |
|-----------------------------|---------------------------------------------------------------------------------------------------------------------|
| `settings.c` / `settings.h` | `session_settings_t` model, every enum, cycling functions, three-tier ini persistence, shared save-choice dispatch. |
| `submenu.c` / `submenu.h`   | Table-driven engine behind every settings screen (see [Settings screens](#settings-screens)).                       |
| `ui_settings.c`             | Settings hub - the category list.                                                                                   |
| `ui_performancesettings.c`  | Performance screen (FPS limit, frame delay, run ahead).                                                             |
| `ui_hudsettings.c`          | Screen Info screen (FPS counter, header visibility).                                                                |

### ui/

| File                      | Purpose                                                                      |
|---------------------------|------------------------------------------------------------------------------|
| `ui_pause.c`              | Top-level pause menu: row list, FPS/speed corner indicators, header, toasts. |
| `options.c` / `options.h` | Core options parsing/persistence with dirty/baseline tracking.               |
| `ui_options.c`            | Core Options screen - categorized libretro core variables.                   |
| `ui_information.c`        | Information screen - core/content/AV/BIOS details.                           |
| `ui_diskcontrol.c`        | Disc Control screen (eject/insert + disc selection).                         |
| `cheats.c` / `cheats.h`   | Cheat file (ini) load/apply via `retro_cheat_set`.                           |
| `ui_cheats.c`             | Cheats screen.                                                               |

## Features

### Video

- **Scaling modes**: Fit Screen (default - largest aspect-correct size that fully fits, height-first with width
  fallback), Aspect, Integer, Stretch, Full Height, Full Width.
- **Shimmer Fix**: optional snap of the destination rect to exact integer multiples of the native frame size on both
  axes, eliminating the fractional-scale resampling shimmer visible on scrolling repeated textures (e.g. SMB1 bricks).
- **Rotation**: 0°/90°/180°/270° via an off-screen canvas, composable with **Mirrored** (horizontal flip).
  Core-requested
  rotation (`SET_ROTATION`) combines with the user's setting.
- **Viewport Offsets**: X/Y pixel offset and zoom with one-tap reset, applied on top of any scaling mode.
- **Viewport Cropping**: per-edge source pixel cropping (top/bottom/left/right) with an optional Centre Crop mode
  that recentres the cropped image on the display, ignoring the X/Y offsets.
- **Texture filters**: nearest, smooth (linear), scale2x, scale3x, sharp bilinear.
- **Colour grading**: brightness/contrast/saturation/hue-shift/gamma, plus drop-in filter presets (`.ini`) and shader
  presets (`.frag`) scanned from `/opt/muos/share/{filter,shader}/`. Works for software and hardware-rendered cores.
- **Border colour**: theme / black / dark grey / white, filled outside the game's `dest_rect`.
- **Overlays**: predefined full-screen patterns or a per-game catalogue overlay, rendered as part of the video-content
  layer - below the pause menu, header, and indicators.

### Hardware render

Cores that require an OpenGL ES 2 context (`RETRO_ENVIRONMENT_SET_HW_RENDER` with `RETRO_HW_CONTEXT_OPENGLES2`, e.g.
flycast) render into an FBO owned by muxretro, sharing SDL_Renderer's GLES2 context. Key invariants, all handled in
`video/hw_render.c`:

- `SDL_RenderFlush()` before any raw-GL draw (render batching would otherwise reorder the queued clear over the frame).
- SDL_Renderer caches GL state and skips reissuing it, so the core's GL activity is bracketed by exact
  snapshot/restore around every `retro_run()` batch (`context_save`/`context_restore`).
- The core keeps its *own* GL state cache too, so out-of-run entry points that may drive its renderer
  (serialize/unserialize/reset/context_destroy) get the inverse bracket (`enter_core_call`/`exit_core_call`), handing
  the core back exactly the state it left.
- `bottom_left_origin` cores are V-flipped at composite; colour filters/shaders route through an intermediate texture.

Other context types (GL core profile, GLES3, Vulkan) are rejected so the core can fall back to software rendering.

### Performance & latency

- **Late input polling**: input is re-polled inside `input_bridge_begin_run()`, *after* the Frame Delay wait, so the
  core always sees the freshest possible input.
- **Frame Delay**: off / auto (p95-adaptive) / 1-16 ms - delays the core run within the frame period to shrink the
  input-to-run gap.
- **Pacing after present**: all pacing sleeps (audio headroom, 50 Hz / slow-motion timing) run *after* the frame is
  presented, never between the core run and the present.
- **Adaptive audio catch-up**: when the audio queue runs low, extra hidden frames are only granted out of measured
  headroom (`frame period ÷ rolling core cost`), so a heavy core is never pushed into a catch-up death spiral.
- **Run Ahead**: see below.
- **FPS limit**: 60 (vsync), 50 (paced), or none.

### Run Ahead

Opt-in, per-content/core/directory (Performance screen). Implemented as **preemptive frames** - the cheap variant of
run-ahead: each frame the engine serializes a one-frame state anchor (steady-state cost: one core run + one serialize).
When the input snapshot changes, it rolls back to the anchor and replays the previous frame hidden (video skipped,
audio muted, rumble suppressed) with the new input before the visible frame runs - new input lands one frame earlier
than the game's internal lag would allow.

Self-gating: software-rendered cores only, save states must be supported, steps aside during fast-forward/slow-motion,
and disables itself with a toast if the core's serialize ever fails. The state anchor is invalidated across every
timeline discontinuity (state load, reset, post-unpause audio priming). Intended for 8/16-bit-class cores with small,
fast states.

### Audio

- Lock-free single-producer/single-consumer sample ring with low/high watermarks.
- Latency profiles (Low / Balanced / Compatible), expressed in device periods; cores can raise the floor via
  `SET_MINIMUM_AUDIO_LATENCY`.
- Sample rate override (auto or fixed 44100/48000 Hz), volume, underrun fade-in, mute during FF/slow-motion.

### Save states & SRAM

- Up to 64 numbered slots plus dedicated quicksave and autosave slots, each with a screenshot thumbnail; naming via
  OSK; `L`/`R` preview mode; confirm-on-load/delete; auto-load of the most recent state on launch (unless `--fresh`).
- **Per-core disablement**: setting `savestate_support = "disabled"` in the core's RetroArch-style `.info` file removes
  the entire save-state surface for that core (menu row, hotkeys, autosave, auto-load) - used for cores whose serialize
  is known-broken (e.g. old reicast-lineage flycast with threaded rendering).
- Serialize buffers carry grow-only slack: threaded-rendering cores restart their emulation thread between the size
  query and the serialize call, so the state can grow in that window.
- SRAM is dirty-checked and written atomically (tmp + rename + fsync) on a background worker thread, flushed on a
  configurable interval and on idle/quit per the **Auto Save** setting.

### Settings screens

Settings are organised into categories: **Hotkey Controls, Video (incl. Viewport), Display (incl. Colour Filter /
Shader), Sound, Input, Performance, Screen Info, Storage**. Every screen is driven by the table engine in
`settings/submenu.c`: a screen supplies label/glyph tables, a value/cycle switch, and optional action/child hooks -
the engine owns row building, focus, nav-bar switching, hold-repeat, the save dialogue, and child dispatch. Adding a
row is an enum entry, a label, a unique glyph name, and two switch cases.

### Disc control

Disc swapping mirrors real hardware: **Eject Disc** (top row) opens the lid - resume so the core observes it - then
select the new disc, which sets the image index and closes the lid in one step. Selecting a disc with the lid closed
prompts to eject first.

### Core options, cheats, information

- Core options v0/v1/v2 (+ intl), categorized, dirty-tracked, discardable.
- Cheats loaded from ini and applied via `retro_cheat_set`.
- Information screen: core name/version, content name, CRC32 hash, applied softpatches, resolution, display/audio
  output, disc count, per-BIOS-file presence.

### Hotkeys

All hotkeys are `MENU + <button>` combos, each individually toggleable in the Hotkey Controls screen:

| Combo                             | Action                                                                            |
|-----------------------------------|-----------------------------------------------------------------------------------|
| MENU+R1                           | Toggle Fast Forward                                                               |
| MENU+R2                           | Quick Save                                                                        |
| MENU+L1                           | Toggle Slow Motion                                                                |
| MENU+L2                           | Quick Load                                                                        |
| MENU+Y                            | Toggle FPS display                                                                |
| MENU+X                            | Cycle header visibility (None / Clock / Battery / Clock+Battery)                  |
| MENU+START                        | Quit (autosaves first if Auto Save covers "On Quit")                              |
| MENU (release, no combo)          | Open the pause menu                                                               |
| MENU (hold, in a settings screen) | Peek at the content underneath for a live preview of the current display settings |

Fast Forward and Slow Motion each have an independent on-screen glyph toggle, so a hotkey can keep working with its
indicator hidden. All rumble is suppressed while the pause menu is open.

---

## Technical notes

### Startup sequence

Install SIGUSR1/SIGUSR2 (sleep/wake) signal handlers → load device/config → `init_module`/`init_theme`/`init_display` →
`board_init`/`mux_input_open` → `core_open` (dlopen) → `state_saves_init` (per-core save-state gate) →
`core_load_content` (archive extraction/VFS streaming and softpatching first) → init SRAM/cheats/overlay bridges →
`gamestate_init` → `options_capture_baseline` → `session_settings_init` (three-tier settings ini) → configure the
hardware-render target if the core negotiated one → open audio, init video → warm up and auto-load the most recent
save state (unless `--fresh`, and only when save states are enabled for the core) → `pause_menu_init`.

### Main loop

Poll input → idle/suspend-signal handling (may autosave + toggle pause) → periodic status and SRAM-flush timers → if
paused, tick the pause menu UI; else run the hotkey dispatcher; otherwise decide the frame batch (fast-forward batch,
or audio catch-up frames granted out of measured headroom) → `run_core_batch` (GL context bracket, per-frame Frame
Delay wait, late input poll, optional run-ahead preparation, `retro_run`) → flush the video frame, refresh LVGL only
when the HUD is dirty, composite, present → pacing sleeps (audio headroom / 50 Hz / slow-motion) *after* present.

Hidden frames (fast-forward intermediates, audio catch-up, run-ahead replays) skip the video path entirely and report
video-disabled via `RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE`, so cooperating cores skip their own rendering too.

### Shutdown

Tear down the pause menu, run-ahead, video (incl. hardware-render target), overlay, audio, and rumble bridges → flush
SRAM one last time → unload content and the core → close input → `sdl_cleanup`.

### Settings persistence

`session_settings_t` (in `settings/settings.h`) holds every per-session setting - video (scaling/rotate/mirror/aspect/
integer scale/texture filter/shimmer fix/border), viewport, colour grading + filter/shader, overlays, sound (volume/
sample rate/latency profile), input (rumble, analog tuning), performance (fps limit, frame delay, run ahead), screen
info, hotkey enables and speeds, auto-save mode, and SRAM flush interval.

Settings are stored as three `.ini` tiers under `RETRO_SET_PATH` (`<share>/retro/settings/`):

```
settings/core/<core_name>.ini
settings/directory/<crc32-of-content-directory>.ini
settings/content/<content_basename>.ini
```

Applied in that order - **core → directory → content** - so the most specific tier wins. Each tier stores only a
**delta**: saving writes just the keys that differ from the tiers beneath it (and removes the file entirely when
nothing differs), so a content-level override never pins unrelated settings against later core- or directory-level
changes. `session_settings_init` snapshots a baseline right after loading; dirty-checking against it drives the "save
changes?" dialogue on leaving any settings screen, and saving writes only the tier the user picked.

### Content loading

`core_load_content` prefers streaming archive members straight to `need_fullpath` cores via the VFS
(`archive#member` convention, backed by a persistent extraction cache keyed by path/entry/size/mtime), falling back to
extraction; softpatches (`patch.c`) apply before the final path/data reaches `retro_load_game`. Save-state, SRAM, and
settings paths derive from the core name and content basename/directory, so cores and directories never collide.

### Build

Builds to `../bin/muxretro` via `retro/Makefile` (sources listed per subdirectory). Links against the shared
`libmuxcom`/`libmuxmod`/`libui` libraries plus `plutosvg`, `z`/`lzma` (static on-device, dynamic for native builds),
and `bz2`, and compiles the bundled `common/libarchive` sources directly into the binary.
