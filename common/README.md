# Common source layout

`libmuxcom.so` remains the single shared frontend support library. Its sources are grouped by responsibility so a file's
location communicates its ownership without changing the library ABI.

| Directory | Responsibility |
| --- | --- |
| `base` | Small, broadly reusable types, strings, paths and utilities |
| `compat` | Compatibility shims that must retain their existing external behaviour |
| `config` | Configuration parsing, values, policy and persistent variables |
| `content` | Content metadata, catalogues, lookup tables, collections and core assignment |
| `display` | Theme, colour, text, images, overlays and other visual rendering code |
| `generated` | Generated headers; do not edit these by hand |
| `platform` | Device, board, input, audio, video and other hardware-facing services |
| `runtime` | Initialisation, logging, process execution, tasks, crashes and performance state |
| `saver` | Screensaver framework and implementations |
| `storage` | Files, archives, downloads, watches, unions and verification |
| `tooling` | Parsers shared by small standalone command-line tools |
| `ui` | Reusable LVGL widgets, navigation and interaction helpers |

## Adding or moving code

- Include common headers by their repository-root path, for example `#include <common/display/theme.h>`.
- Add every new implementation file to the matching source group in `common/Makefile`. Source discovery is explicit so
  experimental or misplaced files cannot silently enter `libmuxcom.so`.
- Keep third-party projects and their licence files in `vendor`; do not place vendored code under `common`.
- Preserve `libmuxcom.so` symbols when relocating existing code. Directory boundaries are organisational and are not a
  reason to split the runtime ABI.
- Put generated output in `generated` and update its generator and build or packaging integration together.

`compat/sdl_cursor.c` is intentionally built into both `libmuxcom.so` and the standalone `libmucursor.so`. The former
preserves existing frontend behaviour and symbols; the latter remains available for SDL compatibility injection.
