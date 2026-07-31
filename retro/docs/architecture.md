# Architecture

Sources are organised by subsystem, with explicit relative-path includes throughout:

```
retro/
  core/      entry point, core hosting, environment callback, runahead
  video/     frame pipeline, hardware render, colour grading, video UI screens
  audio/     audio bridge and sound settings screen
  input/     input/rumble/hotkey bridges and their UI screens
  state/     save states, SRAM, VFS, softpatching, BIOS checks
  settings/  session settings model, submenu engine, settings UI screens
  ui/        pause menu, core options, information, disc control, cheats
```

## core/

| File                        | Purpose                                                                                                                                                                                                                                           |
|-----------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `main.c`                    | Entry point: startup sequence, main loop (frame batching, pacing, runahead hook), shutdown.                                                                                                                                                       |
| `core.c` / `core.h`         | `dlopen`s the core `.so`, resolves every `retro_*` symbol into a global `current_core`, wires muxretro's own video/audio/input callbacks in via the core's `retro_set_*_cb` setters, loads content (archives, softpatches).                       |
| `environment.c`             | Implements `mux_retro_environment_cb` - the big `RETRO_ENVIRONMENT_*` switch (pixel format, directories, log, core options v0/v1/v2 + intl, disk control, messages, VFS, rumble, hardware render, AV info, frame time, throttle state, shutdown). |
| `runahead.c` / `runahead.h` | Preemptive-frames runahead (see [Run Ahead](video.md#runahead)).                                                                                                                                                                                  |
| `muxretro.h`                | Shared declarations for every bridge and UI screen.                                                                                                                                                                                               |
| `libretro.h`                | Vendored upstream libretro API header.                                                                                                                                                                                                            |
| `paths.h`                   | Every `RETRO_*_PATH` macro used across the app.                                                                                                                                                                                                   |

## video/

| File                                      | Purpose                                                                                                                                                                                                                 |
|-------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `video.c`                                 | SDL2 texture pipeline: raw frame upload/conversion, all scaling modes (including Fit Screen and the Shimmer Fix snap), rotation, mirroring. Dispatches CPU texture filters to `filters/` rather than implementing them. |
| `filters/filters.c` / `filters.h`         | CPU texture filter dispatch layer - the only part of `filters/` video.c talks to (scale factor, CPU-scaled/linear-sample queries, apply).                                                                               |
| `filters/scale2x.c` / `scale2x.h`         | Scale2x / Scale3x (EPX-style edge-detect upscalers).                                                                                                                                                                    |
| `filters/super_eagle.c` / `super_eagle.h` | Super Eagle (2xSaI-family, sharper diagonals than 2xSaI).                                                                                                                                                               |
| `hw_render.c` / `hw_render.h`             | `RETRO_ENVIRONMENT_SET_HW_RENDER` support for GLES2 cores (see [Hardware render](video.md#hardware-render)).                                                                                                            |
| `colour.c` / `colour.h`                   | GLES2 colour grading (brightness/contrast/saturation/hue/gamma) plus filter/shader preset loading.                                                                                                                      |
| `overlay_bridge.c` / `overlay_bridge.h`   | Predefined pattern overlays and per-game catalogue overlays, composited into the video content layer.                                                                                                                   |
| `frame_pacer.c`                           | Frame Delay: adaptive pre-run wait (p95 of recent frame costs) so input is sampled as late as possible before each frame.                                                                                               |
| `ui_display.c`                            | Display screen (filter/shader pickers, colour grading, overlay).                                                                                                                                                        |
| `ui_videosettings.c`                      | Video screen (viewport entry, scaling, rotation, mirror, aspect, integer scale, texture filter, shimmer fix, border).                                                                                                   |
| `ui_viewport.c`                           | Viewport Offsets screen (X/Y offset, zoom, edge cropping, centre crop, reset).                                                                                                                                          |
| `ui_colfilter.c` / `ui_shader.c`          | Colour filter / shader picker screens.                                                                                                                                                                                  |

## audio/

| File                 | Purpose                                                                                                                                           |
|----------------------|---------------------------------------------------------------------------------------------------------------------------------------------------|
| `audio.c`            | SDL audio device, lock-free SPSC sample ring, latency profiles, queued-ms watermarking, underrun fade, mute/pause, one-pole low/high-pass filter. |
| `ui_soundsettings.c` | Sound screen (volume, sample rate, audio latency profile, audio filter).                                                                          |

## input/

| File                                      | Purpose                                                                                                                                                                                                                                                                                          |
|-------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `input_bridge.c`                          | Input poll/state callbacks with epoch-based snapshotting (deterministic within a frame), analog deadzone/sensitivity transforms, multi-port support, suppress-until-release, per-port macro step playback.                                                                                       |
| `hotkeys.c` / `hotkeys.h`                 | MENU+X combo dispatcher (see [Hotkeys](settings.md#hotkeys)).                                                                                                                                                                                                                                    |
| `rumble.c` / `rumble.h`                   | Rumble bridge with board-specific on/off magnitude quirks and menu/replay suppression.                                                                                                                                                                                                           |
| `nav_repeat.c` / `nav_repeat.h`           | Shared d-pad hold-to-repeat helper used by every UI screen.                                                                                                                                                                                                                                      |
| `core_input_meta.c` / `core_input_meta.h` | Captures the core's `SET_CONTROLLER_INFO`/`SET_INPUT_DESCRIPTORS` environment calls, exposing per-port device-type lists and per-button labels to the button mapper.                                                                                                                             |
| `ui_hotkeys.c`                            | Hotkey Controls screen.                                                                                                                                                                                                                                                                          |
| `ui_inputsettings.c`                      | Input hub screen (Port 1-4, Auto Assign, Controller Options, Reset Input).                                                                                                                                                                                                                       |
| `ui_inputport.c`                          | Per-port screen (Controller, Core Device, Button Mapping, Macros, Reset Port).                                                                                                                                                                                                                   |
| `ui_buttonmapping.c`                      | Per-port physical-input -> RetroPad target mapping, with L/R turbo-rate (ms) cycling per row. `A` captures a button press or a stick push; `X` opens a target picker listing Unbound plus all 24 targets, which is the only route to the 8 stick directions on a pad that has no sticks to push. |
| `ui_controlleroptions.c`                  | Controller Options screen (rumble, analog deadzone/anti-deadzone/sensitivity/invert Y).                                                                                                                                                                                                          |
| `ui_macros.c`                             | Macros screen - per-port macro list and step editor (see [Macros](macros.md)).                                                                                                                                                                                                                   |

## state/

| File                                | Purpose                                                                                                                                                                                                                                        |
|-------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `state.c`                           | `state_save`/`state_load` with hardware-render context bracketing, serialize buffer slack, per-core disablement.                                                                                                                               |
| `gamestate.c` / `gamestate.h`       | Quicksave/autosave/numbered-slot savestate management, screenshot thumbnails, most-recent auto load on launch.                                                                                                                                 |
| `sram.c` / `sram.h`                 | SRAM (battery save) bridge: dirty-aware, atomic (tmp+rename+fsync), written on a background worker thread.                                                                                                                                     |
| `vfs.c` / `vfs.h`                   | libretro VFS incl. reading content directly out of an archive member, with a persistent size/mtime-keyed cache.                                                                                                                                |
| `content_hash.c` / `content_hash.h` | Background-threaded CRC32 of the content file, cached by size+mtime.                                                                                                                                                                           |
| `patch.c` / `patch.h`               | Softpatch engine - IPS, BPS, and UPS appliers, stacking numbered patches (`.ips`, `.1.ips`, ...).                                                                                                                                              |
| `bios_check.c` / `bios_check.h`     | Reads the core's RetroArch-style `.info` file for `firmware*` entries and checks presence.                                                                                                                                                     |
| `macro.c` / `macro.h`               | Per-port macro definitions - named button sequences with a per-step hold duration in milliseconds, persisted alongside save states (see [Macros](macros.md)). Also compiles `.rls` Relish scripts via `relish.c` into the same representation. |
| `relish.c` / `relish.h`             | Compiler for the Relish (`.rls`) macro scripting language - tokenizer, label resolution, jump-safety validation, and the on device index-stability registry (see [Relish scripting](relish.md)).                                               |
| `manual.c` / `manual.h`             | Locates a core/content's bundled manual and tracks read position, font size, and word-wrap preference.                                                                                                                                         |
| `ui_gamestate.c`                    | Game State screen (slots, naming, preview mode).                                                                                                                                                                                               |
| `ui_storagesettings.c`              | Storage screen (auto save, SRAM flush interval).                                                                                                                                                                                               |

## settings/

| File                        | Purpose                                                                                                             |
|-----------------------------|---------------------------------------------------------------------------------------------------------------------|
| `settings.c` / `settings.h` | `session_settings_t` model, every enum, cycling functions, three-tier ini persistence, shared save-choice dispatch. |
| `submenu.c` / `submenu.h`   | Table-driven engine behind every settings screen (see [Settings screens](settings.md#settings-screens)).            |
| `ui_settings.c`             | Settings hub - the category list.                                                                                   |
| `ui_performancesettings.c`  | Performance screen (FPS limit, frame delay, run ahead).                                                             |
| `ui_hudsettings.c`          | Screen Info screen (FPS counter, header visibility).                                                                |

## ui/

| File                      | Purpose                                                                                                                                                              |
|---------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `ui_pause.c`              | Top-level pause menu: row list, FPS/speed/pause corner indicators (footer-themed), header clock/battery (header-themed), toasts, clean savestate screenshot capture. |
| `options.c` / `options.h` | Core options parsing/persistence with dirty/baseline tracking.                                                                                                       |
| `ui_options.c`            | Core Options screen - categorised libretro core variables.                                                                                                           |
| `ui_information.c`        | Information screen - core/content/AV/BIOS details.                                                                                                                   |
| `ui_diskcontrol.c`        | Disc Control screen (eject/insert + disc selection).                                                                                                                 |
| `cheats.c` / `cheats.h`   | Cheat file (ini) load/apply via `retro_cheat_set`.                                                                                                                   |
| `ui_cheats.c`             | Cheats screen.                                                                                                                                                       |
| `ui_patch.c`              | Patches screen - enable/disable stacked softpatches per content.                                                                                                     |
| `ui_manual.c`             | Manual screen - renders a core/content's manual with adjustable font size and word wrap.                                                                             |
