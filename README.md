# MustardOS Frontend

This is where all the magic of the user interface of MustardOS (muOS) comes to life.

---

## Building

### Prerequisites

You will need `make`, `ccache`, and a C compiler. Everything else depends on which target you are building for.

**Cross-compiling for ARM devices (normal workflow)**

A pre-built aarch64 toolchain is expected at `~/x-tools/aarch64-buildroot-linux-gnu/`. Other toolchain roots can be pointed to by setting the `XTOOL`
environment variable. The toolchain must include `gcc`, `g++`, `ar`, `ld`, and `strip` for the target host tuple.

**Native builds for x86 / x86-64**

A system `gcc` and the following development packages are required:

| Package      | Purpose           |
|--------------|-------------------|
| `SDL2`       | Display and input |
| `SDL2_mixer` | Audio             |
| `SDL2_ttf`   | Font rendering    |
| `SDL2_image` | Image loading     |
| `libcurl`    | Network requests  |
| `libpng`     | PNG support       |

On Fedora/RHEL: `sudo dnf install SDL2-devel SDL2_mixer-devel SDL2_ttf-devel SDL2_image-devel libcurl-devel libpng-devel`

On Debian/Ubuntu: `sudo apt install libsdl2-dev libsdl2-mixer-dev libsdl2-ttf-dev libsdl2-image-dev libcurl4-openssl-dev libpng-dev`

---

### Cross-Compile Build (ARM Devices)

All cross-compile builds go through `xtool.sh`, which sets up the toolchain environment and then calls `make`.

```sh
# Standard release build (aarch64 Cortex-A53, the most common target)
BUILD=release ./xtool.sh make -j$(nproc)

# Verbose build (shows each compiler command and all error output)
DEBUG=1 BUILD=release ./xtool.sh make -j$(nproc)
```

**DEVICE targets**

| `DEVICE`           | Use for                         |
|--------------------|---------------------------------|
| `ARM64_A53`        | H700, A133P - default if unset  |
| `ARM64_A53_CRYPTO` | A53 with hardware AES/CRC       |
| `ARM64`            | Generic ARMv8-A                 |
| `ARM32`            | Original 35x (ARMv7 hard-float) |
| `ARM32_A9`         | Cortex-A9 with NEON             |

```sh
# Example: build for generic ARM64
DEVICE=ARM64 BUILD=release ./xtool.sh make -j$(nproc)
```

`BUILD` accepts `release` (production image) or `test` (development image with test flags). It defaults to `test` if unset.

---

### Native Build (x86 / x86-64)

Use `DEVICE=NATIVE` to build and run directly on the host machine. This is useful for quick iteration and debugging without hardware.

```sh
DEVICE=NATIVE BUILD=release ./xtool.sh make -j$(nproc)
```

Binaries and shared libraries land in `bin/` just like a cross-compiled build.

---

### Dependency

* `common`: Common Libraries and Functions
* `lookup`: Friendly name lookup table mainly for arcade content
* `lvgl`: [LVGL Embedded Graphics Library](https://github.com/lvgl/lvgl)
* `module`: Frontend menu system modules
* `stage`: Hardware overlay staging system

### Independent

* `mubattery`: Background battery monitor daemon
* `mucredits`: Supporter Credits
* `mufbset`: Customised framebuffer resolution switcher
* `muhotkey`: Global Hotkey System
* `mulog`: System log viewer
* `mulookup`: Content Name Lookup
* `muremap`: Background input remap daemon
* `murgb`: RGB LED and MCU controller daemon
* `musplash`: Standalone PNG splash screen
* `muwarn`: First Install Disclaimer Message
* `muxcharge`: Charging Information Screen
* `muxfrontend`: Main Frontend Specific Runner
* `muxmessage`: Information and Progress Screen

### Modules

* `muxactivity`: Activity Tracker Information
* `muxapp`: Application List
* `muxappcon`: Application Control Manager
* `muxarchive`: Archive Manager
* `muxassign`: Assignable System/Core for Content
* `muxbackup`: Device Backup Menu
* `muxbtall`: Bluetooth Device List
* `muxbtcon`: Bluetooth Connection Manager
* `muxbtdev`: Bluetooth Device Information
* `muxchrony`: System Clock Information
* `muxcoladjust`: Content Colour Adjustment Menu
* `muxcolfilter`: Content Colour Filter Menu
* `muxcollect`: Content Collection Manager
* `muxconfig`: Configuration Menu
* `muxconnect`: Connectivity Menu
* `muxcontrol`: Content Control Scheme Selector
* `muxcustom`: Customisation Menu
* `muxdanger`: Dangerous Settings
* `muxdevice`: Device Settings
* `muxdownload`: Archive Downloader
* `muxfont`: Font Settings
* `muxgov`: System Governor Selector
* `muxhdmi`: HDMI Configuration
* `muxhistory`: Content History Menu
* `muxinfo`: Information Menu
* `muxinstall`: First Time Install Menu
* `muxkiosk`: Kiosk Mode Management
* `muxlanguage`: Language Selector
* `muxlaunch`: Main Menu
* `muxnetadv`: Advanced Network Settings
* `muxnetinfo`: Network Information
* `muxnetprofile`: Network Profile Manager
* `muxnetscan`: Network SSID Scanner
* `muxnetwork`: Network Configuration
* `muxnews`: Community News
* `muxoption`: Content Explorer Options
* `muxoverlay`: Content Overlay Settings
* `muxpass`: Passcode Screen
* `muxpasscfg`: Passcode Configuration
* `muxpicker`: Customisation Package Selector
* `muxplore`: Content Explorer
* `muxpower`: Power Settings
* `muxraopt`: RetroArch Options
* `muxremap`: Button Remapping
* `muxrgb`: RGB LED Settings
* `muxrtc`: Date and Time
* `muxsearch`: Content Search
* `muxshader`: Shader Settings
* `muxshare`: Shared Frontend Module State
* `muxshot`: Screenshot Viewer
* `muxsort`: Sorting Settings
* `muxspace`: Disk Usage Information
* `muxsplash`: Simple PNG Based Splash Screen
* `muxstorage`: Storage Migrate/Sync Information
* `muxsysinfo`: System Information
* `muxtag`: Content Tag Manager
* `muxtask`: Task Toolkit
* `muxtester`: Input Tester
* `muxtext`: Basic Text File Viewer
* `muxtheme`: Theme Picker
* `muxthemedown`: Custom Theme Download
* `muxthemefilter`: Custom Theme Filter
* `muxthemeopt`: Theme Options
* `muxtimezone`: Timezone Selector
* `muxtweakadv`: Advanced Settings
* `muxtweakgen`: General Settings
* `muxvisual`: Interface Options
* `muxwebserv`: Web Services

---

## Third Party Libraries

The following third party libraries are bundled in this repository.

### [LVGL](https://github.com/lvgl/lvgl)

Embedded graphics library used as the core UI toolkit for all menus and widgets. Includes the TinyTTF font renderer for glyph rasterisation.

- Version: 8.4.0
- License: MIT
- Location: `lvgl/`

### [PlutoSVG](https://github.com/sammycage/plutosvg)

Compact SVG rendering library written in C. Used to parse and render SVG icons for list and grid view glyphs, with scaling driven by the LVGL custom image
decoder pipeline. Bundles [PlutoVG](https://github.com/sammycage/plutovg), the 2D vector graphics canvas and rasteriser it is built on.

- PlutoSVG Version: 0.0.8
- PlutoVG Version: 1.3.3
- Author: Samuel Ugochukwu
- License: MIT
- Location: `plutosvg/`

### [json.c](https://github.com/tidwall/json.c)

Single-file C library for parsing JSON. Used throughout the codebase to read language translation files, configuration data, and API responses.

- Author: Josh Baker
- License: MIT
- Location: `common/json/`

### [minic](https://github.com/univrsal/minic)

Minimal C INI file parser. Used for reading and writing `.ini` configuration files.

- Author: univrsal
- License: BSD 2-Clause
- Location: `common/mini/`

### [miniz](https://github.com/richgeldreich/miniz)

Single-file C library for deflate/inflate, zlib-compatible compression, and ZIP archive reading and writing. Used to extract downloaded ZIP archives.

- Version: 11.3.0
- License: MIT (portions also released as public domain / Unlicense)
- Location: `common/miniz/`

### [xxHash](https://github.com/Cyan4973/xxHash)

Extremely fast non-cryptographic hash algorithm. Used to compute file checksums for content verification.

> **Note:** xxHash is non-cryptographic and is not resistant to deliberate collision attacks. It is suited for detecting accidental data corruption (content
> verification) but must not be used for tamper detection or cryptographic integrity verification.

- Version: 0.8.3
- Author: Yann Collet
- License: BSD 2-Clause
- Location: `common/xxhash/`

### [stb_truetype](https://github.com/nothings/stb)

Single-header C library for TrueType font parsing and glyph rasterisation. Used by the LVGL TinyTTF renderer to load and render custom TTF fonts at runtime.

- Version: 1.26
- Author: Sean Barrett
- License: Public domain
- Location: `common/stb/stb_truetype.h`

### [stb_rect_pack](https://github.com/nothings/stb)

Single-header C library for rectangle packing. Used by the LVGL TinyTTF renderer to pack glyph bitmaps into atlas textures.

- Version: 1.01
- Author: Sean Barrett
- License: Public domain
- Location: `common/stb/stb_rect_pack.h`

### [stb_image_write](https://github.com/nothings/stb)

Single-header C library for writing PNG, BMP, TGA, JPEG, and HDR image files. Used to capture and save screenshots from the framebuffer.

- Version: 1.16
- Author: Sean Barrett
- License: Public domain
- Location: `common/stb/stb_image_write.h`
