# MustardOS Frontend

This is where all the magic of the user interface of MustardOS comes to life.

---

## Building

### Prerequisites

You will need `make`, `ccache`, and a C compiler.

Some dependencies are fetched and built from source on first use (see [External Dependencies](#external-dependencies)), which additionally needs `curl` or
`wget`, `perl`, and `tar`. Optionally,
`dialog` or `whiptail` gives `build.sh` a guided setup, and `nasm` is needed for ffmpeg's x86 assembly on x86 hosts.

Everything else depends on which target you are building for.

**Cross-compiling for ARM devices (normal workflow)**

A pre-built aarch64 toolchain is expected at `$HOME/x-tools/aarch64-buildroot-linux-gnu/`. Other toolchain roots can be pointed to by setting the `XTOOL`
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

On Fedora/RHEL:
`sudo dnf install SDL2-devel SDL2_mixer-devel SDL2_ttf-devel SDL2_image-devel libcurl-devel libpng-devel`

On Debian/Ubuntu:
`sudo apt install libsdl2-dev libsdl2-mixer-dev libsdl2-ttf-dev libsdl2-image-dev libcurl4-openssl-dev libpng-dev`

---

### Cross-Compile Build (ARM Devices)

All builds go through `build.sh`, which sets up the toolchain environment and then calls `make`.

```sh
# Shipping build (aarch64 Cortex-A53, the most common target)
./build.sh release

# Development build with debug symbols, for gdb and crash backtraces
./build.sh debug
```

`release` builds with `BUILD=release`. `debug` builds the development image (`BUILD=test`, which keeps test only code) with `DEBUGSYM=1`, so binaries keep
their symbols and are not stripped. Every build runs one job per CPU core, so there is no `-j` to pass. Anything after `release` or `debug` is handed to
`make`, such as `./build.sh release clean`.

Run `build.sh` with no arguments for a guided setup that asks for the device, build type and toolchain, then starts the build. It uses `dialog` or `whiptail`
when either is installed, and falls back to plain numbered prompts otherwise.

```sh
# Guided / Spoon Fed
./build.sh
```

`./build.sh make` still works as before and takes `BUILD` from the environment, defaulting to `test` when it is unset.

#### What every build does

Before compiling, each build:

1. Sets every file in the repository to `755` and owned by the current user, the same as `chmod -R 755 .` and `chown -R "$(id -un):" .`. A file that
   cannot be changed, such as one left owned by root, gives a warning and the build carries on.
2. Refreshes the generated files, see [Generated metadata](#generated-metadata).
3. Copies the generated language template to `../language/mux/en.json` when it has changed.
4. Formats any C source you have changed or added with the pinned clang-format, keeping the executable bit.
5. Runs `shellcheck -s sh` over any changed script in `../internal` and warns about problems. This is skipped with a warning when `shellcheck` is not
   installed.

Set `SKIP_CHECKS=1` to skip steps 2 to 5 and compile straight away. Permissions and ownership are always set.

#### Build output and the build log

Each build starts with a summary of what it is about to use: the build type and device, the toolchain with its GCC version, location and sysroot, whether
`ccache` is available, the job count and log location, whether the sibling `internal` and `language` repositories were found, and which helper tools are
installed. Anything missing is highlighted along with what it affects, and the first build for a device warns that external libraries are compiled first.
If no toolchain can be found at all, `build.sh` explains where one is expected and how to point it elsewhere.

After that the screen shows a short summary: each stage, every component as it builds, generated and formatted files, and any warnings or errors with the offending
source line. A failed build ends with a list of the first errors.

The complete output of every step, including each sub `make`, is written to `build.log` beside `build.sh`, which is replaced on each build and ignored by Git.
Look there for anything the summary leaves out, rather than rebuilding with extra output.

| Variable   | Effect                                                     |
|------------|------------------------------------------------------------|
| `RAW=1`    | Shows the full output on screen as well as in the log      |
| `DEBUG=2`  | Also writes every compiler command into the log            |
| `NO_COLOR` | Turns off coloured output, which is also off when piped    |

**DEVICE targets**

| `DEVICE`           | Use for                          |
|--------------------|----------------------------------|
| `ARM64_A53`        | H700, A133P - default if unset   |
| `ARM64_A53_CRYPTO` | A53 with hardware AES/CRC        |
| `ARM64`            | Generic ARMv8-A                  |
| `ARM32`            | Original 35x (ARMv7 hard-float)  |
| `ARM32_A9`         | Cortex-A9 with NEON              |
| `X86_64`           | Generic 64-bit x86               |
| `RISCV64`          | Generic 64-bit RISC-V            |
| `GENERIC`          | Anything else, with `ARCH_FLAGS` |

The toolchain is detected automatically from `$HOME/x-tools` (override with `XTOOL`), picking one whose architecture matches `DEVICE`. Set `XDIR` to a directory
name under it to choose a specific toolchain.

```sh
# Example: build for generic ARM64
DEVICE=ARM64 ./build.sh release
```

---

### Native Build (x86 / x86-64)

Use `DEVICE=NATIVE` to build and run directly on the host machine. This is useful for quick iteration and debugging without hardware.

```sh
DEVICE=NATIVE ./build.sh release
```

Binaries and shared libraries land in `bin/` just like a cross-compiled build.

---

### Incremental Builds

Builds are incremental. Header dependencies are tracked with `-MMD -MP`, and the generated dependency files are kept under `.deps/`, one directory per
component, so a rebuild only touches what actually changed.

```sh
# Full rebuild from nothing
./build.sh release clean
./build.sh release
```

Changing `DEVICE`, `BUILD`, `OPT_LEVEL` or `DEBUGSYM` forces a clean automatically, since objects compiled with different flags cannot be reused. Switching
between `release` and `debug` therefore rebuilds everything. The current configuration is recorded in `.build-config`.

### Compilation database

Generate `compile_commands.json` from a clean build when using CLion, clangd or another editor that supports compilation databases:

```sh
BUILD=release ./build.sh database
```

The command uses the selected cross-toolchain and records the real compiler arguments from the root project and its nested components. Temporary capture files
are written beneath `/tmp/mustardos`. The resulting `compile_commands.json` is local development metadata and remains excluded from Git.

To use it as the CLion project model, enable CLion's bundled **Compilation Database** plugin and restart the IDE. Close any existing Makefile project for this
directory, then select **Open** from the welcome screen, choose `compile_commands.json`, press **OK**, and open it as a project. An existing `.idea` directory
created by the Makefile importer must be moved aside before the one-time import. Afterwards, select the appropriate compiler under **Build, Execution,
Deployment → Compilation Database**.

### Generated metadata

Every build refreshes the generated language template, third-party version table, profile schema and internal script hashes before compiling, and again after
any configuration-driven clean. Output is compared before replacement, so unchanged metadata keeps its timestamp and does not cause unnecessary recompilation.

```sh
# Refresh generated files without compiling
./build.sh generate
```

The standard outputs are `common/generated/language.json`, `common/generated/thirdparty.h`, `common/generated/profile_schema.json` and
`common/generated/verify_data.h`. Verification data is refreshed from the sibling `internal/script` tree when it is available; otherwise the checked-in data is
retained. Set `INTERNAL_SCRIPT_DIR`, `INTERNAL_ROOT`, `LANGUAGE_OUTPUT`, `PROFILE_SCHEMA_OUTPUT`, `THIRDPARTY_OUTPUT`, or `VERIFY_OUTPUT` to override those
paths, and `LANGUAGE_REPO_FILE` to change where the language template is copied. Generating verification data requires `xxhsum`.

### Formatting

Sources are formatted with clang-format 21.1.8, matching CLion. `build.sh` installs that exact version into `.cache` when it is not already available, or uses
the binary named by `CLANG_FORMAT`.

```sh
# Format everything tracked by Git
./build.sh format

# Format or check particular files
./build.sh format retro/ui/ui_manual.c
./build.sh format --check
```

Builds format only the files you have changed, so committed files that predate the formatter are left alone until you format them yourself.

---

### External Dependencies

Some third party libraries are not kept in this repository. They are fetched from upstream, checksum verified, built static and installed into
`external/prefix/$DEVICE` by `external/build.sh`, which the build calls for you. Each library stamps what it built, so repeat runs cost nothing.

```sh
# Rebuild one of them after bumping its pinned version
DEVICE=ARM64_A53 EXT_ARCH_FLAGS="-mcpu=cortex-a53" ./external/build.sh ffmpeg
```

Linking them statically means the version shipped in a device rootfs never matters, and their symbols are hidden with
`--exclude-libs` so libretro cores keep their own copies.

---

### Dependency

* `common`: Common Libraries and Functions
* `emmc`: TrimUI internal eMMC probe module
* `external`: Third party dependencies fetched and built from source
* `input`: Input service and per device input drivers
* `module`: Frontend menu system modules
* `mugl`: 32-bit GLES2 forwarder for TrimUI devices
* `retro`: LibRetro core host
* `stage`: Hardware overlay staging system
* `terminal`: Mustard Terminal emulator, settings menu and on-screen keyboard
* `tooling`: Build and development helpers
* `vendor`: Bundled third party source and licence files
* `video`: Wasabi media player

The `common` source tree is grouped into `base`, `compat`, `config`, `content`, `display`, `generated`, `platform`,
`runtime`, `saver`, `storage`, `tooling`, and `ui`. See [`common/README.md`](common/README.md) for ownership and migration rules. Its Makefile uses explicit
source groups so adding a file does not silently alter `libmuxcom.so`.

### Independent

* `mubattery`: Background battery monitor daemon
* `mucredits`: Supporter Credits
* `mudisp`: Display and HDMI output control
* `mudns`: Local network discovery over multicast DNS
* `mufbset`: Customised framebuffer resolution switcher
* `muhotkey`: Global Hotkey System
* `muinput`: Input service and remote input socket
* `mulink`: Direct Link daemon for cable connections between devices
* `mulog`: System log viewer
* `mulookup`: Content Name Lookup
* `muremap`: Background input remap daemon
* `murgb`: RGB LED and MCU controller daemon
* `muscreen`: Remote View screen streamer
* `musplash`: Standalone PNG splash screen
* `mususpend`: Suspend and wake helper
* `muswitch`: Launches content directly
* `muvarctl`: Configuration value store control
* `muverify`: Signed manifest verification
* `muwarn`: First Install Disclaimer Message
* `muweb`: Landing page web server, catalogue and save management API
* `muxcharge`: Charging Information Screen
* `muxfrontend`: Main Frontend Specific Runner
* `muxmedia`: Wasabi media player
* `muxmessage`: Information and Progress Screen
* `muxretro`: Pickles, the LibRetro core host
* `muxterm`: Mustard Terminal

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
* `muxcolfilter`: Content Colour Filter Menu
* `muxcollect`: Content Collection Manager
* `muxconfig`: Configuration Menu
* `muxconnect`: Connectivity Menu
* `muxcontrol`: Content Control Scheme Selector
* `muxcore`: Core Management
* `muxcustom`: Customisation Menu
* `muxdanger`: Dangerous Settings
* `muxdetail`: Device Details
* `muxdevice`: Device Settings
* `muxdistemp`: Display Colour Temperature
* `muxgov`: System Governor Selector
* `muxhdmi`: HDMI Configuration
* `muxhistory`: Content History Menu
* `muxinfo`: Information Menu
* `muxinstall`: First Time Install Menu
* `muxkiosk`: Kiosk Mode Management
* `muxlanguage`: Language Selector
* `muxlaunch`: Main Menu
* `muxlink`: Direct Link Settings
* `muxlogo`: Boot Logo Selector
* `muxnetadv`: Advanced Network Settings
* `muxnetprofile`: Network Profile Manager
* `muxnetproxy`: Network Proxy Settings
* `muxnetscan`: Network SSID Scanner
* `muxnetwork`: Network Configuration
* `muxoption`: Content Explorer Options
* `muxorder`: Content Sort Order
* `muxoverlay`: Content Overlay Settings
* `muxpass`: Passcode Screen
* `muxpasscfg`: Passcode Configuration
* `muxpicker`: Customisation Package Selector
* `muxplore`: Content Explorer
* `muxpower`: Power Settings
* `muxprofile`: Settings Profiles
* `muxraopt`: RetroArch Options
* `muxremap`: Button Remapping
* `muxrgb`: RGB LED Settings
* `muxrgbzone`: RGB LED Zone Settings
* `muxrtc`: Date and Time
* `muxsearch`: Content Search
* `muxshader`: Shader Settings
* `muxshare`: Shared Frontend Module State
* `muxshot`: Screenshot Viewer
* `muxsort`: Sorting Settings
* `muxsoundfont`: Soundfont Selector
* `muxspace`: Disk Usage Information
* `muxsplash`: Simple PNG Based Splash Screen
* `muxstorage`: Storage Migrate/Sync Information
* `muxtag`: Content Tag Manager
* `muxtask`: Task Toolkit
* `muxtester`: Input Tester
* `muxtext`: Basic Text File Viewer
* `muxtheme`: Theme Picker
* `muxthemedown`: Custom Theme Download
* `muxthemefilter`: Custom Theme Filter
* `muxthird`: Third Party Licences
* `muxtimezone`: Timezone Selector
* `muxtweakadv`: Advanced Settings
* `muxtweakgen`: General Settings
* `muxwebcode`: Web Dashboard Code
* `muxwebserv`: Web Services

---

## Third Party Libraries

### Bundled In This Repository

#### [LVGL](https://github.com/lvgl/lvgl)

Embedded graphics library used as the core UI toolkit for all menus and widgets. Includes the TinyTTF font renderer for glyph rasterisation.

- Version: 8.4.0
- License: MIT
- Location: `vendor/lvgl/`

#### [PlutoSVG](https://github.com/sammycage/plutosvg)

Compact SVG rendering library written in C. Used to parse and render SVG icons for list and grid view glyphs, with scaling driven by the LVGL custom image
decoder pipeline. Bundles [PlutoVG](https://github.com/sammycage/plutovg), the 2D vector graphics canvas and rasteriser it is built on.

- PlutoSVG Version: 0.0.8
- PlutoVG Version: 1.3.3
- Author: Samuel Ugochukwu
- License: MIT
- Location: `vendor/plutosvg/`

#### [mdns](https://github.com/mjansson/mdns)

Header-only multicast DNS and DNS-SD implementation used by the `mudns` local discovery helper.

- Revision: `a569c475`
- License: Public domain
- Location: `vendor/mdns/`

#### [json.c](https://github.com/tidwall/json.c)

Single-file C library for parsing JSON. Used throughout the codebase to read language translation files, configuration data, and API responses.

- Author: Josh Baker
- License: MIT
- Location: `vendor/json/`

#### [minic](https://github.com/univrsal/minic)

Minimal C INI file parser. Used for reading and writing `.ini` configuration files.

- Author: univrsal
- License: BSD 2-Clause
- Location: `vendor/mini/`

#### [miniz](https://github.com/richgeldreich/miniz)

Single-file C library for deflate/inflate, zlib-compatible compression, and ZIP archive reading and writing. Used to extract downloaded ZIP archives.

- Version: 11.3.0
- License: MIT (portions also released as public domain / Unlicense)
- Location: `vendor/miniz/`

#### [xxHash](https://github.com/Cyan4973/xxHash)

Extremely fast non-cryptographic hash algorithm. Used to compute file checksums for content verification.

> **Note:** xxHash is non-cryptographic and is not resistant to deliberate collision attacks. It is suited for detecting
> accidental data corruption (content
> verification) but must not be used for tamper detection or cryptographic integrity verification.

- Version: 0.8.3
- Author: Yann Collet
- License: BSD 2-Clause
- Location: `vendor/xxhash/`

#### [stb_truetype](https://github.com/nothings/stb)

Single-header C library for TrueType font parsing and glyph rasterisation. Used by the LVGL TinyTTF renderer to load and render custom TTF fonts at runtime.

- Version: 1.26
- Author: Sean Barrett
- License: Public domain
- Location: `vendor/stb/stb_truetype.h`

#### [stb_rect_pack](https://github.com/nothings/stb)

Single-header C library for rectangle packing. Used by the LVGL TinyTTF renderer to pack glyph bitmaps into atlas textures.

- Version: 1.01
- Author: Sean Barrett
- License: Public domain
- Location: `vendor/stb/stb_rect_pack.h`

#### [stb_image_write](https://github.com/nothings/stb)

Single-header C library for writing PNG, BMP, TGA, JPEG, and HDR image files. Used to capture and save screenshots from the framebuffer.

- Version: 1.16
- Author: Sean Barrett
- License: Public domain
- Location: `vendor/stb/stb_image_write.h`

---

### Fetched At Build Time

These are not kept in the repository. `external/build.sh` downloads a pinned release, verifies its SHA-256, and builds a trimmed static copy into
`external/prefix/$DEVICE`. Versions and checksums are pinned at the top of each
`external/<name>.sh`.

#### [FFmpeg](https://ffmpeg.org)

Audio and video decoding for theme wallpapers, the screensaver, the boot logo, and content video previews. Built with
`--disable-everything` plus only the MP4 demuxer and the handful of decoders `common/platform/video.c` uses.

- Version: 9.0.1
- License: LGPL 2.1 or later
- Build: `external/ffmpeg.sh`

#### [libarchive](https://libarchive.org)

Multi-format archive reading. Used read-only by muxretro for content stored in archives. Compression codecs are detected per sysroot, since they are not present
everywhere.

- Version: 3.8.9
- License: BSD 2-Clause
- Build: `external/libarchive.sh`

#### [OpenSSL](https://openssl.org)

TLS, hashing and signature verification for Network Play and achievement accounts. Built statically so the 1.1 and 3.x split across device rootfs images stops
mattering.

- Version: 3.5.7 (LTS branch)
- License: Apache 2.0
- Build: `external/openssl.sh`

#### [rcheevos](https://github.com/RetroAchievements/rcheevos)

RetroAchievements client, hashing and libretro memory helper. Desktop integration and the RetroAchievements integration DLL sources are excluded.

- Version: 12.4.0
- License: MIT
- Build: `external/rcheevos.sh`

#### [Mojibake](https://github.com/zaerl/mojibake)

Unicode text processing library covering normalisation, collation, and case mapping without external dependencies. Used for locale-aware, Unicode-correct
natural sorting of content and file lists.

- Version: 0.3.6
- Author: Francesco Bigiarini
- License: MIT
- Build: `external/mojibake.sh`
