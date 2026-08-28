# MustardOS Input Daemon

Extensible userspace input service for MustardOS devices. Hardware backends read device-specific GPIO, serial, I2C, or
kernel input sources and publish one stable `muOS-Keys` uinput controller contract. The backends cover the TRIMUI Smart
Pro, Smart Pro S, Brick, and Brick Pro plus the complete canonical Anbernic H700 family, GCS H36S, MagicX Zero 28, G350
V, GKD Pixel 2 and Anbernic RG Vita Pro and whatever else will come in the future...

## Features

- Automatic registry selection from `/opt/muos/device/config/board/name`, with an explicit `--device` override for
  diagnostics and debug purposes only.
- Maps physical buttons, triggers, sticks, and hat to a single virtual type gamepad.
- Automatic neutral and noise calibration at startup, with held stick and motion rejection, followed by continuous range
  and drift adaptation. (_This will require a lot of testing!_)
- Batches uinput events into one write per synchronisation frame.
- Strength scaled rumble support for pwm, serial, and evdev force feedback motors where supported.
- Simple calibration helpers for analogue sticks.

## Running

- Requires access to `/dev/uinput`, serial ports (`/dev/ttyS3/4` or `/dev/ttyAS5/7`), `/dev/mem` (Brick/Brick Pro GPIO),
  `/dev/i2c-3` (Brick Pro analogue sticks), and `/sys/class/gpio` (A133 rumble).
- Run the built binary as root (or with the needed capabilities):

```bash
./build/bin/muinput --foreground
```

The service reads the canonical board name from `/opt/muos/device/config/board/name`, selects the matching backend, and
starts polling. On development hosts where the board file is not installed, it falls back to hardware probing.
`--device` remains available as a diagnostic override.

```text
muinput --list-devices
muinput --device tui-brick-pro --foreground --verbose --rumble-strength 75
```

## Hardware Testing

Run the interactive CLI tester with the production backend stopped first:

```bash
muinput --test --rumble-strength 50
```

The dashboard shows every advertised button and whether it has been pressed, live axes hats with observed min and max,
switch state, raw calibration centres, spans, deadzones, and startup calibration rejection counts.

There are also interactive commands (when using through SSH or ADB):

- `R`: exercise the full evdev or uinput force feedback path for 750 ms
- `C`: restart analogue neutral calibration (_release the sticks first_)
- `X`: clear button, axis, switch coverage
- `Q`: quit and print a coverage and range summary

The rumble result confirms that the system accepted and completed the effect. You must still confirm that the physical
motor moved. Test mode always runs in foreground and restores normal working order on exit or a handled shutdown signal.

## Controller Identity

The service's virtual controllers are exposed as `muOS-Keys` on `BUS_VIRTUAL`. Their identity encodes `mu` in the vendor
field (`0x756d`) and `os` in the version field (`0x736f`), with a stable, unique product ID between them:

| Device          | Product ID | SDL2 GUID                          |
|-----------------|-----------:|------------------------------------|
| `tui-spoon`     |   `0x0001` | `060000006d750000010000006f730000` |
| `tui-brick`     |   `0x0002` | `060000006d750000020000006f730000` |
| `tui-brick-pro` |   `0x0003` | `060000006d750000030000006f730000` |
| `tui-smpro-s`   |   `0x0004` | `060000006d750000040000006f730000` |
| `gcs-h36s`      |   `0x0005` | `060000006d750000050000006f730000` |
| `mgx-zero28`    |   `0x0006` | `060000006d750000060000006f730000` |
| `rgsp`          |   `0x0701` | `060000006d750000010700006f730000` |
| `rg28xx-h`      |   `0x0702` | `060000006d750000020700006f730000` |
| `rg34xx-h`      |   `0x0703` | `060000006d750000030700006f730000` |
| `rg34xx-sp`     |   `0x0704` | `060000006d750000040700006f730000` |
| `rg35xx-2024`   |   `0x0705` | `060000006d750000050700006f730000` |
| `rg35xx-h`      |   `0x0706` | `060000006d750000060700006f730000` |
| `rg35xx-plus`   |   `0x0707` | `060000006d750000070700006f730000` |
| `rg35xx-pro`    |   `0x0708` | `060000006d750000080700006f730000` |
| `rg35xx-sp`     |   `0x0709` | `060000006d750000090700006f730000` |
| `rg40xx-h`      |   `0x070a` | `060000006d7500000a0700006f730000` |
| `rg40xx-v`      |   `0x070b` | `060000006d7500000b0700006f730000` |
| `rgcubexx-h`    |   `0x070c` | `060000006d7500000c0700006f730000` |
| `rk-g350-v`     |   `0x3501` | `060000006d750000013500006f730000` |
| `rk-pixel-2`    |   `0x3502` | `060000006d750000023500006f730000` |
| `rg-vita-pro`   |   `0x3503` | `060000006d750000033500006f730000` |

Product IDs are an input compatibility contract. Do **NOT** reuse or renumber one unless the corresponding controller
layout intentionally changes. Smart Pro S is not A133 based but shares this service identity namespace, _one day!_

## Configuration Misc Stuff

- `BRICK_ACTIVE_LOW=0` forces Brick buttons to be treated as active-high (defaults to active-low).
- `BRICK_PRO_ACTIVE_LOW=0` forces Brick Pro buttons to be treated as active-high (defaults to active-low).
- `/opt/muos/device/config/board/strength` sets rumble strength from 0 to 100 percent. The `muxtweakadv` (Advanced
  Settings) module writes this board setting in 5 percent steps.
- `MUOS_RUMBLE_STRENGTH=0..100` overrides the board setting, and `--rumble-strength 0..100` overrides both. A value of
  `0` disables physical movement without changing the advertised controller contract. Defaults to `100`.
- Sending `SIGHUP` reloads the board strength without recreating the virtual controller. Environment and command line
  overrides remain fixed for the life of the process.
- `MUOS_INPUT_SOURCE=/dev/input/eventN` selects a kernel source explicitly. `MUOS_INPUT_SOURCE_NAME` selects it by input
  name when a fixed event number is unavailable.
- A133 production devices use GPIO 227 active high, H700 profiles proxy rumble through the source evdev node, and the
  portable evdev profiles use their configured AXP or PWM interface.

## H700 Source Isolation

The H700 kernel controller remains the transport for `muinput`, but it is not left in SDL's `/dev/input` scan path.
Which is why tools like `evtest` will show missing event inputs. After opening and exclusively grabbing the source,
`muinput` atomically relocates its device node to `/dev/muinput/h700-source`. That private node is reused after a
service restart, so isolation does not depend on event numbering or device management.

The H700 startup module also unbinds the redundant vendor `dierct-keys-polled` platform driver after `muinput` is ready.
Consequently the only public game controller is the normalised `BUS_VIRTUAL` `muOS-Keys` device. New H700 DTBs label the
internal controller `muOS-Input-Source`. The service accepts the former `muOS-Keys`source name for compatibility with
already produced images. Then `EVIOCGRAB` remains active as an additional guard for processes that opened the source
before relocation.

The H700 profile is selected from the canonical board name automatically. A source-name override remains available when
diagnosing the physical event node:

```text
MUOS_INPUT_SOURCE_NAME=muOS-Input-Source muinput --foreground --verbose
```

## Repo layout

- `devices/`: Device profiles, normalised mappings, and the shared rumble core.
- `devices/registry.*`: Uniform backend discovery and lookup.
- `drivers/`: Low-level serial, evdev, GPIO, sunxi GPIO mmap, I2C, and rumble drivers.
- `common/`: Flat shared implementation for calibration, uinput, and the interactive CLI tester.

# Adding Another MustardOS Input Backend

The service separates the stable virtual controller from hardware acquisition:

```text
hardware -> backend poller -> normalised Linux events -> muOS-Keys uinput -> SDL/Apps/Content
```

## Backend Contract

Each device exports one `struct device_backend` containing:

- A stable ID matching `/opt/muos/device/config/board/name`
- A human readable hardware name
- A side effect _free_ probe and priority
- The `gamepad_desc` (name, identity, capabilities)
- Initialise, poll, refresh, and close operations
- The preferred polling interval

Flags and status returns use `int` (`0`/non-zero) consistently across the C API. Device backends should not introduce a
separate type unless it really needs one. Add the exported backend pointer once in `devices/registry.c`.

The handheld selection is generally automatic. The board name file is looked up directly from the registry.
`--device <id>` is only a diagnostic override. Automatic hardware probes are a development fallback for hosts without a
valid board name file and must only look at what is actually physically available.

## Stable Identity Rules

- The virtual name is `muOS-Keys`.
- Software-created devices use Linux `BUS_VIRTUAL`.
- Vendor `0x756d` encodes `mu`, version `0x736f` encodes `os`, and neither is a USB assignment.
- Allocate a new product ID for every incompatible control layout.
- **Never** impersonate a commercial USB controller.
- Treat the product ID and SDL GUID as a compatibility contract.

## Analogue Input

We feed raw 12bit samples through `cal_update2()` and `cal_apply()`. Startup calibration needs a stable, near neutral
window. On startup, movement or a held stick is rejected until the controls are released. The service then learns noise,
centre drift, and effective positive/negative spans while running.

If a device uses a different ADC range, add range metadata to the calibration API rather than pre scaling in the
hardware backend. This way it keeps the calibration policy uniform and easier to debug later on.
