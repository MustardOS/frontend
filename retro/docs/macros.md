# Macros

Per-port, per-content button macros, reached from the port's **Macros** row alongside **Button Mapping**
(`input/ui_inputport.c`). Want loops, conditions, or forward jumps instead of a straight line list?
That's [Relish](relish.md), the companion scripting language. This page just covers the on device editor.

## Steps

- A macro is a named, ordered list of steps; each step holds a RetroPad target plus three timing values - **Hold**
  (how long the button stays down on each press), **Repeat** (how many times to press it), and **Wait** (the gap
  _between_ repeats only. Never before the first press or after the last) independent of the core FPS. A step fires its
  first Hold immediately. Wait only matters once Repeat is 2 or higher. The step list shows a compact
  `W<wait>-H<hold>-R<repeat>` summary per row (e.g. `W0ms-H96ms-R1`).
- Pressing `A` on a step opens its **Timing** submenu (Wait/Hold/Repeat rows). On each row, `L`/`R` cycles that field
  through its own preset table (Wait/Hold: 48/96/192/384/768/1536/3072/6144ms; Repeat: 1/2/3/4/5/10/20/50) and
  `A` opens the numeric OSK to type an exact value (Wait/Hold: 0-65535ms; Repeat: 1-65535). New steps default to Wait
  0ms, Hold 96ms, Repeat 1 - identical to a plain single press.
- Pressing `Y` on a step list opens an **Add Step** dialogue with two kinds:
    - **Button** captures the next physical button pressed on the port, then drops it into the step list with the same
      Wait/Hold/Repeat defaults as above. Buttons only - a step holds a digital chord, so a stick push isn't offered
      here even though one can _trigger_ the macro. To have a macro move a stick, use a [Relish](relish.md) `STICK`
      step.
    - **Pause** adds a step that presses nothing for its Hold duration - a plain dead-time gap between two other steps,
      shown as **Pause** in the step list. Pressing `A` on a Pause row skips the Wait/Hold/Repeat submenu entirely and
      opens the numeric OSK directly for its duration in milliseconds.
    - This is just a super basic no frills list of presses and pauses, nothing else. Any looping, branching, or forward
      jumps belong in a [Relish](relish.md) script instead.

## Binding and Playback

- Pressing `A` on a macro (_list view_) offers **Edit** or **Bind**. Bind enters the same capture flow as Button
  Mapping, so a macro can be triggered by any of the sixteen buttons _or_ by a stick push - flick the stick and that
  direction becomes the trigger. Binding a direction this way turns that whole stick digital, exactly as binding one to
  a normal target does. On a Relish sourced macro, the dialogue is skipped entirely and `A` goes straight to Bind, since
  Edit is never available for one anyway. Assigning a macro to a button is mutually exclusive with that button's normal
  target/turbo binding so assigning one clears the other.
- Pressing the bound physical button triggers the macro. It plays through to completion on its own, regardless of
  whether the button is still held or released partway through. It is a fire once and run, not a hold to play type of
  macro run. Pressing the trigger button again while a macro is still running restarts it from the top. A Button step
  with Repeat > 1 always gets at least one frame of release between presses, even if Wait is set to 0ms, so the repeats
  never merge into a single continuous hold.
- Macros persist per content next to save states (`macro/macro.c`), and the button mapper shows `Macro: <name>` in place
  of the usual target label for any row bound to one.

## What a macro sends vs. what the game registers

Pickles guarantees the _output_ is exactly that. A step with Hold 768ms and Repeat 20 sends exactly 20 clean,
individually timed presses to the core, _every time_. It has no visibility into, or control over, what the running game
then does with those presses. A game samples input on its own schedule and often imposes its own cooldown on a given
action (an attack or jump that cannot retrigger for a fixed number of frames, for example). So 20 presses sent can
easily look like only ~10 "worked" in practice because the game itself is ignoring the presses that land inside its own
cooldown window. That is up to what the core and content do; it is not a Pickles issue.

If a macro seems to under-deliver, that is the first thing to check, and it is usually the game's
behaviour to work around (e.g. lengthening Wait so each press lands outside the cooldown) rather than something to fix
by changing Hold.

## Relish

Power users can manually create a macro off device as a plain text **Relish** script (use the extension `.rls`) and drop
it into the same per content macro folder. Pickles will compile it on device into the same step representation, with
real structure the on-device editor doesn't offer: loops, conditions, `BREAK`, `STOP`, and forward-referencing jumps.
Relish
sourced macros show a distinct glyph in the list and can be bound/deleted but never edited on device. Any broken scripts
show as a selectable, deletable "Broken Script" entry with a somewhat helpful compile error message.

See [`relish.md`](relish.md) for the full language reference.
Ready-to-copy scripts and their known limitations are in [`macro/examples`](../macro/examples/README.md).
