# Relish

**R**etro **E**vent **L**oop **I**nterpreted **S**cript **H**andler.

Relish is a small-ish, plain text scripting language for creating Pickles button macros *off device*, in any text
editor. An `.rls` file dropped into a content macro folder (the same folder the on device macro editor uses for its
`.ini` files - see [`macros.md`](macros.md)) is compiled _on device_ at load time into the exact same step
representation the editor produces, so it'll play back through the identical runtime.

We have kept the on device editor deliberately simple. A straight list of button presses and pauses, anyone can build
one in a few seconds. Relish is where all the fun advanced stuff lives. You got access to loops, conditions, forward
jumps, breaking early etc. If you want repetition, branching, or anything past the "press these buttons in this order"
then you'll need to create a relish script. _Fun!_

## Compatibility Rules

**An `.rls` file can never be edited on device!**  The macro list shows Relish sourced macros with a distinct glyph.
They can still be bound to a button and deleted like any other macro. In fact, since Edit was never on the table for one
anyway, pressing `A` on a Relish macro skips the Edit/Bind choice entirely and goes straight to Bind. This is because
creating such a UI for Pickles is just quite overwhelming!  Using a text editor is simple and costs less, without
headaches, in the end.

## Grammar

Separated tokens, one instruction per line, no braces or semicolons, case insensitive throughout. The indentation is
entirely optional and ignored. Leading spaces, or tabs (_gross_), on any line are skipped before the line is read, so
you are free to indent a loop body or an `IF` line for readability without it changing anything. _This ain't Python!_

| Keyword            | Form                                                              | Meaning                                                                                                                                                                                                                                  |
|--------------------|-------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `NAME`             | `NAME <text>`                                                     | Optional friendly display name for the macro list. Set at most once. If omitted, the filename (minus `.rls`) is used instead.                                                                                                            |
| `REM`              | `REM <text>`                                                      | Comment. Ignored, exactly like a blank line.                                                                                                                                                                                             |
| `LABEL`            | `LABEL <id>`                                                      | Names the position of the _next_ instruction. Consumes no step slot of its own.                                                                                                                                                          |
| `BUTTON`           | `BUTTON <btn> [<btn>...] [WAIT <ms>] [HOLD <ms>] [REPEAT <n>]`    | Presses one or more buttons together (a chord) for `HOLD` milliseconds, `REPEAT` times, with `WAIT` milliseconds between repeats. Omitted modifiers default to `WAIT 0`, `HOLD 96`, `REPEAT 1` - the same as a fresh Add Step on device. |
| `PAUSE`            | `PAUSE <ms>`                                                      | Waits `<ms>` milliseconds, pressing nothing. See [PAUSE vs. HOLD](#pause-vs-hold) below - they're not the same thing.                                                                                                                    |
| `GOTO`             | `GOTO <label>`                                                    | Jumps to `<label>` every time this line is reached, unconditionally. May reference a label anywhere in the file, before or after this line.                                                                                              |
| `LOOP` / `ENDLOOP` | `LOOP <count>` ... `ENDLOOP`                                      | Repeats everything between the two, `<count>` times, then continues after `ENDLOOP`. Nest freely.                                                                                                                                        |
| `BREAK`            | `BREAK`                                                           | Exits the nearest enclosing `LOOP` immediately, skipping straight to the step after its `ENDLOOP`. A compile error outside of any `LOOP`.                                                                                                |
| `IF`               | `IF <condition> THEN GOTO <label>` or `IF <condition> THEN BREAK` | Conditionally jumps or breaks - see [Conditionals](#conditionals) below.                                                                                                                                                                 |

Button Tokens: `A B X Y L1 R1 L2 R2 L3 R3 START SELECT UP DOWN LEFT RIGHT`.

### PAUSE vs. HOLD

These _might sound_ similar but mean different things, and mixing them up is the easiest way to get timing wrong:

- **`HOLD`** is a modifier _on a `BUTTON` line_. It is how long _that particular press_ lasts. It only exists in the
  context of a button already being pressed. `BUTTON A HOLD 200` means "press A and keep it down for 200ms".
- **`PAUSE`** is its own instruction, with no button involved at all. `PAUSE 200` means "press nothing for 200ms". The
  dead time between two other steps, e.g. a moment of silence before a follow up input.

Internally `PAUSE <ms>` compiles to a `BUTTON` step that happens to press no buttons. So `WAIT`/`REPEAT` still
technically apply to it if you are curious, but in practice you will only ever need the plain `PAUSE <ms>` keyword.

### Timing Semantics

`WAIT` is the gap _between_ repeats only!  Never before the first press of a `BUTTON` line, never after the last. It
only has any effect once `REPEAT` is 2 or higher. Even with `WAIT 0` the repeats always get at least one frame of
release between them, so they register as separate presses rather than merging into one continuous hold. This matches
the on device editor Wait/Hold/Repeat behaviour. See [`macros.md`](macros.md) for the full explanation - including an
important note on why a `REPEAT` count you set doesn't always match what the game visibly does with it. Pickles
guarantees the presses it _sends_. What the game _does_ with them is out of our hands really.

### Playback Semantics

Pressing the bound physical button fires the macro once. It plays to completion on its own regardless of whether the
button is released partway through. Without a `GOTO` looping back, a script simply finishes and sits idle. Pressing the
trigger button again restarts it from the top once more. A `GOTO` never falls through on its own, so a script that ends
in a `GOTO` back to an earlier label repeats indefinitely until the trigger button is pressed again or the controller
disconnects.

## Conditionals

`IF <condition> THEN GOTO <label>` and `IF <condition> THEN BREAK` are the only two forms. The `IF` always either jumps
somewhere or breaks out of a loop. There is no `ELSE` (_just yet anyway_). So to do something only on one branch and
skip it otherwise. A `condition` is one of:

| Form                | Meaning                                                                                                                  |
|---------------------|--------------------------------------------------------------------------------------------------------------------------|
| `<button> HELD`     | True if that physical button is currently held down.                                                                     |
| `NOT <button> HELD` | True if it isn't.                                                                                                        |
| `COUNT <op> <n>`    | True if the nearest enclosing `LOOP`'s current pass number compares to `<n>` as `<op>` says. Only valid inside a `LOOP`. |

`<op>` is one of `EQUALS`, `NOTEQUALS`, `LESS`, `GREATER`, `ATLEAST`, `ATMOST`
We use words here and not symbols (_like BASIC you know?_), matching the rest of the grammar. The `COUNT` is **1 index**
on a loop's first pass,
`COUNT EQUALS 1` is true and on its tenth, `COUNT EQUALS 10` is.

## Examples

Roughly simplest to most advanced and each does one thing.

**A plain macro, no control flow at all** the simplest possible script:

```
NAME Just A
BUTTON A HOLD 100
```

**A basic loop** - press A five times:

```
NAME Five Taps
LOOP 5
    BUTTON A HOLD 48
ENDLOOP
```

**A forward reference** - the one thing the on device editor genuinely cannot express:

```
NAME Skip Ahead
GOTO landing
BUTTON A HOLD 500
LABEL landing
BUTTON B HOLD 100
```

**Loop the whole macro while the trigger button is held**, by jumping back to the top instead of letting it end:

```
NAME Hold To Repeat
LABEL start
    IF NOT A HELD THEN GOTO stop
    BUTTON A HOLD 48 WAIT 48 REPEAT 4
GOTO start
LABEL stop
REM All Done!
```

Or alternatively:
```
NAME Hold To Repeat
REM This uses a loop rather than label
LOOP 65535
    IF NOT A HELD THEN BREAK
    BUTTON A HOLD 48 WAIT 48 REPEAT 4
ENDLOOP
```

**An extra step partway through a loop** - the "10th of 20" case: a bonus press only on one specific pass, guarded by
the unconditional `GOTO` so it is skipped every _other_ time:

```
NAME Extra On Tenth
LOOP 20
    BUTTON A HOLD 48
    IF COUNT EQUALS 10 THEN GOTO bonus
    GOTO after_bonus
    LABEL bonus
    BUTTON B HOLD 96
    LABEL after_bonus
ENDLOOP
```

**An early exit** - stop looping the moment a modifier button is pressed, instead of waiting for the full count:

```
NAME Cancellable Burst
LOOP 50
    IF R1 HELD THEN BREAK
    BUTTON A HOLD 48 WAIT 48
ENDLOOP
```

**Nested loops** - The `BREAK` always targets the *inner most* enclosing loop, so this only cuts the inner short, never
the outer one:

```
NAME Nested Burst
LOOP 3
    LOOP 10
        IF B HELD THEN BREAK
        BUTTON A HOLD 32 WAIT 32
    ENDLOOP
    PAUSE 300
ENDLOOP
```

**Branching between two behaviours** - a slower loop normally, a faster one while a modifier is held:

```
NAME Modifier Speed
LABEL start
    IF NOT SELECT HELD THEN GOTO check_speed
    GOTO stop
LABEL check_speed
    IF R1 HELD THEN GOTO fast
    BUTTON A HOLD 96 WAIT 96
    GOTO start
LABEL fast
    BUTTON A HOLD 32 WAIT 32
    GOTO start
LABEL stop
```

## Jump Safety

Be careful with this one, may cause unexpected behaviours!  Because the `GOTO`/`LOOP`/`IF` can reference any label, both
forward or backward, and `IF` branches on input... The compiler doesn't know in advance, it runs a validation pass
before ever accepting a script. But for every step, it explores every possible outcome (both branches of every
`IF`, the real counted behaviour of every `LOOP`) and confirms all of them eventually reach a `BUTTON`/`PAUSE` step or
the natural end of the script, within a bounded amount of exploration.

A script where some chain of `GOTO`/`LOOP`/`IF` can never reach a real step - two `LOOP`s pointing only at each other.
For example if it fails to compile with a line reference error instead of being silently accepted and behaving
strangely. This check protection is made so that if a script is "too complex to verify" it will just stop altogether
instead of running something and ruining your game.

## Compile Errors

If a `.rls` file fails to compile, it will still appear in the macro list but as a **Broken Script** entry. You can
still delete it but it can't be bound to a button.

Selecting it will show the specific error as an on device message, some common errors include:

- `Line N: unrecognised keyword 'xyz'` - the first word on a line isn't one of the keywords above.
- `Line N: undefined label 'xyz'` - a `GOTO`/`IF ... THEN GOTO` references a label that doesn't exist anywhere in the
  file.
- `Line N: label 'xyz' already defined` - the same `LABEL` name used twice.
- `Line N: unknown button 'xyz'` - a `BUTTON`/`IF` line references something other than the sixteen button tokens above.
- `Line N: ENDLOOP without a matching LOOP` / `Line N: LOOP without a matching ENDLOOP` - unbalanced loop blocks.
- `Line N: BREAK outside of a LOOP` - a bare `BREAK` or `IF ... THEN BREAK` with no enclosing `LOOP`.
- `Line N: COUNT used outside of a LOOP` - an `IF COUNT ...` condition with no enclosing `LOOP`.
- `Line N: unknown comparison operator 'xyz'` - the word after `COUNT` isn't one of the six listed above.
- `Line N: this Goto/Loop/If chain never reaches a Button step or the end of the macro (or is too complex to verify)` -
  see [Jump safety](#jump-safety).
- `Line N: macro exceeds 32 steps` - `BUTTON`/`PAUSE`/`GOTO`/`ENDLOOP`/`BREAK`/`IF` lines share the same 32-step ceiling
  the on device editor uses (The `LABEL` and `LOOP` lines themselves don't count against it).

## Stable Bindings

Binding a macro to a physical button records a numeric index, not a name or file path. The `.ini` based macros get that
index written into their own file the first time they're loaded. An `.rls` file is never written to, so instead Pickles
keeps a small file of its own, `.relish_index`, alongside the scripts in the same macro folder, mapping each script's
filename to its assigned index. This file is just for Pickles housekeeping. Nothing really worth editing manually. If
you do decide to delete the `.relish_index` (_for whatever reason_) it will just reassign the index on next load, which
_may_ break existing button bindings to Relish macros.

## What Relish doesn't do (_yet..._)

There aren't any variables, arithmetic, or subroutines, or call returns. The `COUNT` is read only and tied to a `LOOP`,
and it's not a general counter you can declare and manipulate. If a concrete need for any of that comes up, it'll be a
separate, late extension to this scripting language. _Who knows what we'll come up with in the future!_
