# Pickles
Formally known as _muRetro_ internally.

MustardOS's own Libretro core hosting frontend.  It uses `dlopen()` to open a Libretro core directly and renders through
the same LVGL/SDL2 stack the rest of the frontend uses, The goal is a "MustardOS Libretro" core kind with its own
pause menu, core options, save states, cheats, and display settings that look and feel like the rest of the frontend,
for systems where the full RetroArch is not required.

## Invocation

```
muxretro <core.so> <content> [--fresh]
```

- `core.so` - path to the libretro core to `dlopen`.
- `content` - path to the content file (or an archive member, see [Content loading](docs/technical.md#content-loading)).
- `--fresh` - skip the warm up frames and the most recent savestate autoload that normally happens on launch.

## Documentation

Detailed reference material lives in [`docs/`](docs), organised by topic so you can jump straight to what you need
rather than reading one bloody giant file:

| Doc | Covers                                                                                                                    |
|---|---------------------------------------------------------------------------------------------------------------------------|
| [`docs/architecture.md`](docs/architecture.md) | Full source layout - every file in `core/`, `video/`, `audio/`, `input/`, `state/`, `settings/`, `ui/`, and what it does. |
| [`docs/video.md`](docs/video.md) | Scaling/rotation/cropping/filters, hardware-rendered cores, Frame Delay and other latency tricks, Run Ahead.              |
| [`docs/audio.md`](docs/audio.md) | The audio pipeline, latency profiles, sample rate handling.                                                               |
| [`docs/macros.md`](docs/macros.md) | The on device Macros editor - Button/Pause steps, timing, binding.                                                        |
| [`docs/relish.md`](docs/relish.md) | Relish - the `.rls` scripting language for manual created macros with loops, conditions, and jumps.                       |
| [`docs/save-states.md`](docs/save-states.md) | Save states, SRAM, and disc swapping.                                                                                     |
| [`docs/settings.md`](docs/settings.md) | The settings-screen engine, core options, cheats, information screen, hotkeys.                                            |
| [`docs/technical.md`](docs/technical.md) | Startup sequence, main loop, shutdown, settings persistence, content loading, build.                                      |

New here and just want the short version? Start with [`docs/macros.md`](docs/macros.md) if you're after button macros, 
or [`docs/architecture.md`](docs/architecture.md) if you're trying to find where a particular piece of code lives.
