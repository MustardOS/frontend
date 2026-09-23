# Mustard Terminal

Mustard Terminal is the native graphical terminal for MustardOS.

The terminal surface is rendered with SDL while its menu and on-screen keyboard use the same LVGL, theme, language and controller infrastructure as the rest of
the frontend. This keeps terminal output efficient while making every interactive element feel consistent with muX and Pickles.

## Launching

Open **Applications > Mustard Terminal**. The frontend closes cleanly before `muxterm` starts and returns to Applications when the terminal exits.

It can also run a command directly:

```sh
/opt/muos/frontend/muxterm -- top
/opt/muos/frontend/muxterm --readonly -- tail -f /opt/muos/log/system.log
```

## Controls

| Button             | Terminal                                 | On-screen keyboard                       | Settings                |
|--------------------|------------------------------------------|------------------------------------------|-------------------------|
| Menu               | Open settings                            | Open settings                            | Close and save          |
| Select             | Cycle keyboard position and transparency | Cycle keyboard position and transparency | -                       |
| D-pad / left stick | Send arrow keys                          | Move between keys                        | Navigate or adjust      |
| A / L3             | -                                        | Type selected key                        | Select                  |
| B                  | Backspace                                | Backspace                                | Close and save          |
| Y                  | Space                                    | Space                                    | -                       |
| Start              | Enter                                    | Enter                                    | -                       |
| L1 / R1            | -                                        | Previous or next keyboard layer          | -                       |
| L2 / R2            | Scroll history                           | Scroll history                           | -                       |
| X                  | -                                        | -                                        | Reset terminal settings |

Select cycles through:

1. Keyboard hidden
2. Bottom, opaque
3. Bottom, transparent
4. Top, opaque
5. Top, transparent

A connected physical keyboard can type directly into the terminal. Terminal control sequences are provided for navigation, editing, function and modifier keys.

## Settings

The graphical settings menu controls:

- Terminal font size
- Font hinting
- Foreground colour
- Background colour
- Quit Terminal

Settings are stored as individual values beneath:

```text
/opt/muos/config/terminal
```

The graphical menu writes `font_size`, `font_hinting`, `foreground` and `background`. Reset removes those overrides so the active theme is used again. Other
terminal values use the same one-file-per-value configuration model as the rest of MustardOS.

The optional custom keyboard layout path is stored in:

```text
/opt/muos/config/terminal/osk_layout
```

Runtime scrollback is stored beneath `/tmp/mustardos` so temporary terminal state can be cleared with other MustardOS temporary files.

An annotated configuration file for the `--config` command-line option is available in [muxterm.conf.example](muxterm.conf.example).

## Custom keyboard layers

The built-in keyboard has lower-case, upper-case and control layers. A custom layout can append additional layers using one level of named sections:

```text
[Symbols]
! | ! | 1
@ | @ | 1
\# | # | 1
$ | $ | 1

( | ( | 1
) | ) | 1
```

Each key uses `label | bytes | width`. Blank lines move to the next row. The byte value accepts `\t`, `\r`, `\n`, `\\` and `\xHH` escapes. A layout may contain
up to 13 custom layers, with four rows and 12 keys per row. L1 and R1 move between all built-in and custom layers.

## Command-line options

```text
-c, --config <path>       Use a specific configuration file
-s, --size <pt>           Set terminal font size
-f, --font <path>         Set the regular terminal font
--font-bold <path>        Set the bold font
--font-italic <path>      Set the italic font
--font-bold-italic <path> Set the bold italic font
--font-hinting <mode>     Set normal, light, mono or none
-i, --image <path>        Set a terminal background image
-bg, --bgcolour <RRGGBB>  Set the terminal background colour
-fg, --fgcolour <RRGGBB>  Override the terminal foreground colour
-sb, --scrollback <lines> Set retained scrollback lines
--sb-path <path>          Set the scrollback cache path
--no-sb-persist           Disable scrollback persistence
-ro, --readonly           Disable terminal input
--osk-layout <path>       Load additional keyboard layers
--force-redraw            Redraw every terminal row each frame
--version                 Show the version
--help                    Show command-line help
```

Arguments after `--` are executed directly through `execvp()`. The default is the configured login shell, then `$SHELL`, then `/bin/sh`. No command is passed
through `system()`.

## Rendering and performance

- Terminal rows are marked dirty by the VT parser and unchanged rows are not redrawn.
- Glyph textures are cached across frames.
- PTY output is read in batches and processed without blocking the interface.
- The compositor presents terminal and LVGL layers together, avoiding a second window or renderer.
- Idle rendering backs off when there is no terminal or interface activity.
- ANSI colour, alternate-screen applications, UTF-8, box drawing, true colour and sixel images remain supported.

## Building

The root frontend build includes `muxterm` automatically:

```sh
DEVICE=NATIVE BUILD=release ./build.sh make -j$(nproc)
```

The result is `bin/muxterm`. The terminal can also be built through `make dep-terminal` after its shared dependencies are available.

Mustard Terminal is covered by the frontend's [GNU General Public License v3.0](../LICENSE).
