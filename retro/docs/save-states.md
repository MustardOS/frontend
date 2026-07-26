# Save States, SRAM & Disc Control

See [`architecture.md`](architecture.md#state) for the file-by-file breakdown of `state/`.

## Save States & SRAM

- Up to 64 numbered slots plus dedicated quicksave and autosave slots, each with a screenshot thumbnail. Naming via OSK
  (`SELECT` clears the field, `START` confirms). `L`/`R` for preview mode. Confirm on load with delete options. Auto
  load of the most recent state on launch (unless launched with `--fresh`).
- **Clean Thumbnails**: the FPS counter and header clock/battery are hidden for the single composited frame the
  thumbnail is captured from, so none of the on screen indicators end up visible into a save screenshot. This has the
  undesired effect of sometimes flashing during timeline saves, _will eventually work that out_.
- **Per core disablement**: setting `savestate_support = "disabled"` in the core's RetroArch style `.info` file removes
  the entire savestate surface for that core (menu row, hotkeys, autosave, auto load). Used for cores whose serial is
  known broken (e.g. old reicast lineage flycast with threaded rendering).
- Serial buffers carry grow only stacks: threaded rendering cores restart their emulation thread between the size query
  and the serial call, so the state can grow in that window.
- SRAM is dirty checked and written atomically (tmp + rename + fsync) on a background worker thread, flushed on a
  configurable interval and on idle/quit per the **Auto Save** setting.
- SRAM is also backed up within a configurable option setting.

## Disc Control

Disc swapping 'mirrors' real hardware: **Eject Disc** (top row) opens the 'lid' and then resume so the core observes it.
Then select the new disc, which sets the image index and closes the 'lid' in one step. Selecting a disc with the 'lid'
closed will prompt to eject first, because it makes sense.
