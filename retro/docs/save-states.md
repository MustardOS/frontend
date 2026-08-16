# Save States, SRAM & Disc Control

See [`architecture.md`](architecture.md#state) for the file-by-file breakdown of `state/`.

## Save States & SRAM

- Up to 64 numbered slots plus dedicated quicksave and autosave slots, each with a screenshot thumbnail. Naming via OSK
  (`SELECT` clears the field, `START` confirms). `L`/`R` for preview mode. Confirm on load with delete options. Auto
  load of the most recent state on launch (unless launched with `--fresh`).
- **Clean Thumbnails**: the FPS counter and header clock/battery are hidden for the single composited frame the
  thumbnail is captured from, so none of the on screen indicators end up visible into a save screenshot. This has the
  undesired effect of sometimes flashing during timeline saves, _will eventually work that out_.
- **Per-core disablement**: Pickles reads the standard `savestate = "false"` value and the MustardOS
  `savestate_support = "disabled"` value from RetroArch-style `.info` files. Compiled definitions in `retro/coreinfo/`
  are applied last and can independently disable save states, run-ahead and Network Play for a specific core. This
  keeps safety policy inside Pickles even when packaged metadata is absent or out of date.
- Save-state capture passes the exact size declared by the core. A changed size is retried once only after an explicit
  failure, and further state captures are disabled for the session if serialisation remains unstable.
- Manual saves, run-ahead and Network Play use the same core-state broker. Loads require an exact current size unless
  an audited compiled core definition explicitly permits that core to judge compatibility.
- After capture, one bounded worker calculates the envelope checksums and writes the state atomically. A second state
  cannot queue behind it; loads, deletion, suspend, shutdown and core unload wait for pending persistence to finish.
- SRAM is dirty checked and written atomically (tmp + rename + fsync) on a background worker thread, flushed on a
  configurable interval and on idle/quit per the **Auto Save** setting.
- SRAM is also backed up within a configurable option setting.
- Each successful save refreshes a small shared `resume.ini` index for the frontend. It contains only the newest
  compatible state and preview for up to 12 games, along with the content path, state type, core and timestamp. Pickles
  removes stale entries when their final state is deleted and writes the small index atomically after successful state
  persistence.
- A save appears in the on-screen list straight away, but nothing is recorded on disk until its write lands. The
  `states.ini` entry and the `resume.ini` index are both written from the completion path, so a save that never reaches
  storage leaves no entry behind. If the write fails, the listing rolls back to what it was before the save and any
  orphaned thumbnail is removed. Any operation that touches the slots, saving, renaming, deleting or loading, settles
  the outstanding save first.

## Disc Control

Disc swapping 'mirrors' real hardware: **Eject Disc** (top row) opens the 'lid' and then resume so the core observes it.
Then select the new disc, which sets the image index and closes the 'lid' in one step. Selecting a disc with the 'lid'
closed will prompt to eject first, because it makes sense.
