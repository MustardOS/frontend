# Relish

**R**etro **E**vent **L**oop **I**nterpreted **S**cript **H**andler.

Relish is a small-ish, plain text scripting language for creating Pickles button macros *off device*, in any text
editor. An `.rls` file dropped into a content macro folder (the same folder the on-device macro editor uses for its
`.ini` files - see [`macros.md`](macros.md)) is compiled _on device_ at load time into the exact same step
representation the editor produces, so it'll play back through the identical runtime.

We have kept the on-device editor deliberately simple, so anyone can build a straight list of button presses and pauses
in a few seconds. Relish is where the more advanced features live. You have access to loops, conditions, forward jumps
and early exits. If you want repetition, branching, or anything beyond "press these buttons in this order", create a
Relish script. _Fun!_

## Compatibility Rules

**An `.rls` file can never be edited on device!** The macro list shows Relish sourced macros with a distinct glyph.
They can still be bound to a button and deleted like any other macro. In fact, since Edit was never on the table for one
anyway, pressing `A` on a Relish macro skips the Edit/Bind choice entirely and goes straight to Bind. This is because
creating such a UI for Pickles is quite overwhelming! Using a text editor is simpler and costs less, without
headaches, in the end.

## Grammar

Separated tokens, one instruction per line, no braces or semicolons, case insensitive throughout. The indentation is
entirely optional and ignored. Leading spaces, or tabs (_gross_), on any line are skipped before the line is read, so
you are free to indent a loop body or an `IF` line for readability without it changing anything. _This ain't Python!_

| Keyword            | Form                                                                                      | Meaning                                                                                                                                                                                                                                  |
|--------------------|-------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `NAME`             | `NAME <text>`                                                                             | Optional friendly display name for the macro list. Set at most once. If omitted, the filename (minus `.rls`) is used instead.                                                                                                            |
| `REM`              | `REM <text>`                                                                              | Comment. Ignored, exactly like a blank line.                                                                                                                                                                                             |
| `DEFINE`           | `DEFINE <name> <value>`                                                                   | Names a number once so you can use the name everywhere a number is allowed. See [Defines](#defines) below.                                                                                                                               |
| `REDEFINE`         | `REDEFINE <name> <value>`                                                                 | Replaces the value of an existing `DEFINE`. Useful for overriding values supplied by an included file.                                                                                                                                   |
| `INCLUDE`          | `INCLUDE <file>`                                                                          | Pastes another file in at this exact spot, before anything else is read. See [Includes](#includes) below.                                                                                                                                |
| `LABEL`            | `LABEL <id>`                                                                              | Names the position of the _next_ instruction. Consumes no step slot of its own.                                                                                                                                                          |
| `BUTTON`           | `BUTTON <btn> [<btn>...] [WAIT <ms>] [HOLD <ms>] [REPEAT <n>]`                            | Presses one or more buttons together (a chord) for `HOLD` milliseconds, `REPEAT` times, with `WAIT` milliseconds between repeats. Omitted modifiers default to `WAIT 0`, `HOLD 96`, `REPEAT 1` - the same as a fresh Add Step on device. |
| `PAUSE`            | `PAUSE <ms>`                                                                              | Waits `<ms>` milliseconds, pressing nothing. See [Difference Between PAUSE vs. HOLD](#pause-vs-hold) below - they're not the same thing.                                                                                                 |
| `STICK`            | `STICK <LEFT\|RIGHT> <x> <y> [WAIT <ms>] [HOLD <ms>] [REPEAT <n>]`                        | Pushes an analogue stick to an exact position. See [Analogue Sticks](#analogue-sticks) below.                                                                                                                                            |
| Counter operations | `SET`, `INC`, `DEC`, `ADD`, `SUB`, `MUL`, `DIV`, `MOD`, `SIN`, `COS`, `TAN`, `FLR`, `TOP` | Performs bounded counter maths. See [Counters](#counters) below.                                                                                                                                                                         |
| `GOTO`             | `GOTO <label>`                                                                            | Jumps to `<label>` every time this line is reached, unconditionally. May reference a label anywhere in the file, before or after this line.                                                                                              |
| `CALL` / `RETURN`  | `CALL <label>` / `RETURN`                                                                 | Calls a labelled subroutine, then returns to the instruction after `CALL`. See [Subroutines](#subroutines) below.                                                                                                                        |
| `STOP`             | `STOP`                                                                                    | Ends the macro immediately. It can also be used as an `IF` or `ELSE` action.                                                                                                                                                             |
| `LOOP` / `ENDLOOP` | `LOOP <count>` ... `ENDLOOP`                                                              | Repeats everything between the two, `<count>` times, then continues after `ENDLOOP`. Nest freely.                                                                                                                                        |
| `BREAK`            | `BREAK`                                                                                   | Exits the nearest enclosing `LOOP` immediately, skipping straight to the step after its `ENDLOOP`. A compile error outside of any `LOOP`.                                                                                                |
| `IF`               | `IF <condition> THEN <action> [ELSE <action>]`                                            | Conditionally jumps, breaks or stops - see [Conditionals](#conditionals) below. An `<action>` is `GOTO <label>`, `BREAK` or `STOP`.                                                                                                      |

Button Tokens: `A B X Y L1 R1 L2 R2 L3 R3 START SELECT UP DOWN LEFT RIGHT`.

Anywhere a number is written you may instead write the name of a compatible `DEFINE`. `WAIT` and `HOLD` also take
`RANDOM <min> <max>` in place of a plain number - see [Random](#random) below.

### <a id="pause-vs-hold"></a>Difference Between PAUSE vs. HOLD

These _might sound_ similar but mean different things, and mixing them up is the easiest way to get timing wrong:

- **`HOLD`** is a modifier _on a `BUTTON` line_. It is how long _that particular press_ lasts. It only exists in the
  context of a button already being pressed. `BUTTON A HOLD 200` means "press A and keep it down for 200ms".
- **`PAUSE`** is its own instruction, with no button involved at all. `PAUSE 200` means "press nothing for 200ms". The
  dead time between two other steps, e.g. a moment of silence before a follow up input.

Internally `PAUSE <ms>` compiles to a `BUTTON` step that happens to press no buttons. So `WAIT`/`REPEAT` still
technically apply to it if you are curious, but in practice you will only ever need the plain `PAUSE <ms>` keyword.

### What is the deal with `.rls` and `.rli`?

- **`.rls` is a runnable macro.** It appears in the macro list and can be bound to a button.
- **`.rli` is a reusable include.** It never appears in the macro list and only runs when an `.rls` file includes it.

Use `.rls` for the macro the user launches. Use `.rli` for shared definitions, labels or steps used by other scripts.

### Defines

`DEFINE <name> <value>` gives a number a name, so the one place you tune a timing isn't scattered across thirty lines.
It is resolved entirely while compiling, it costs no step slot and there is nothing left of it at playback time.

```
NAME Tuned Burst
DEFINE TAP    48
DEFINE GAP    64
DEFINE ROUNDS 12

LOOP ROUNDS
    BUTTON A HOLD TAP WAIT GAP REPEAT 3
ENDLOOP
```

Order doesn't matter, a `DEFINE` may sit at the bottom of the file and still be used at the top. A `DEFINE` may also use
an earlier `DEFINE` as its value. The name can't be a number, or one of the sixteen button tokens, and can't be defined
twice. Up to 32 of them per script.

`REDEFINE <name> <value>` replaces an existing value without consuming another definition slot. The name must already
exist, so an accidental misspelling is reported instead of silently creating a second value. Definitions are resolved
before steps are emitted, meaning the final value applies throughout the compiled script rather than only to lines below
the `REDEFINE`. This makes it useful for overriding defaults from an included file:

```
INCLUDE timings
REDEFINE TAP 72

BUTTON A HOLD TAP
```

The replacement may be a number or the name of another definition that has already been declared. A definition may
contain up to three decimal places, although whole-number fields such as milliseconds, repeats and loop counts still
require a whole-number definition.

### Includes

`INCLUDE <file>` splices another file's lines in at that exact point, before a single instruction is looked at. It is a
straight textual paste, which is what makes it useful, everything spliced in shares the same labels, the same
`DEFINE` values and the same 32-step ceiling as the file that pulled it in.

If the file name has no extension, `INCLUDE` adds `.rli`. Include files stay out of the macro list; only `.rls` files
appear there as runnable macros.

`inputs.rli`:

```
REM Shared building blocks
DEFINE TAP 48

LABEL tap_a
    BUTTON A HOLD TAP
```

`combo.rls`:

```
NAME Combo
GOTO start
INCLUDE inputs
LABEL start
    BUTTON X HOLD TAP
    GOTO tap_a
```

The file name must be a plain `.rli` name and can't contain a path, so scripts can only pull from their own macro
folder. Pickles opens it relative to that already-open folder and rejects symbolic links, hard links, directories,
devices, pipes and every other non-regular source. Nesting works up to 4 deep, and a file may only be included once per
script, so an accidental include loop is a compile error rather than a hang. All source files together are limited to
128 KiB and 512 meaningful lines.

### Random

Two separate things use `RANDOM`, don't get them mixed up. One jitters _timing_, the other branches on _chance_.

`WAIT`/`HOLD`/`PAUSE` accept `RANDOM <min> <max>` in place of a plain millisecond number, and re-roll every single time.
Each hold and each gap picks its own value, so a `REPEAT 5` line gives you five different holds and not one value reused
five times. This is the difference between a burst that reads as a macro and one that doesn't:

```
NAME Human Burst
LOOP 20
    BUTTON A HOLD RANDOM 40 70 WAIT RANDOM 50 110
    PAUSE RANDOM 200 600
ENDLOOP
```

`REPEAT` cannot be `RANDOM`, it's a step count, not a duration.

`IF RANDOM <pct> THEN ...` is the other one, and it's a condition. It is true `<pct>` percent of the time, from `0`
(never) to `100` (always), rolled fresh each time the step is reached.

### Counters

Counters support whole numbers or decimals with up to three places. They start at `0` every time the macro is triggered
and stay within `-65535.000` to `65535.000`; arithmetic that crosses either boundary is clamped there.

| Instruction     | Effect                                                |
|-----------------|-------------------------------------------------------|
| `SET x <value>` | Replaces `x`.                                         |
| `INC x [value]` | Adds `value`, or `1` when omitted.                    |
| `DEC x [value]` | Subtracts `value`, or `1` when omitted.               |
| `ADD x <value>` | Adds `value`.                                         |
| `SUB x <value>` | Subtracts `value`.                                    |
| `MUL x <value>` | Multiplies by `value`.                                |
| `DIV x <value>` | Divides by `value`.                                   |
| `MOD x <value>` | Keeps the remainder after division by `value`.        |
| `SIN x [value]` | Replaces `x` with the sine of an angle in degrees.    |
| `COS x [value]` | Replaces `x` with the cosine of an angle in degrees.  |
| `TAN x [value]` | Replaces `x` with the tangent of an angle in degrees. |
| `FLR x [value]` | Rounds down to the nearest whole number.              |
| `TOP x [value]` | Rounds up to the nearest whole number.                |

`<value>` may be a number, a `DEFINE`, or another counter written as `VAR <name>`. Calculations use deterministic
fixed-point integers rather than floating point, so they give identical results on every supported device. Results
beyond three decimal places are truncated towards zero. A literal divide or modulo by zero is a compile error; if a
counter used as the divisor becomes zero during playback, that macro stops safely.

For `SIN`, `COS`, `TAN`, `FLR` and `TOP`, leaving `[value]` out operates on the named counter in place. For example,
`SIN angle` replaces `angle` with its sine, while `SIN result VAR angle` leaves `angle` intact. Trigonometric angles are
in degrees and may be negative or larger than one rotation. An exact tangent at `90 + 180n` degrees is undefined and
stops that macro safely; very large valid tangent results clamp to the normal counter range.

Counters are read back with the `IF VAR` condition:

```
NAME Every Third Time
SET fired 0
LOOP 30
    INC fired
    IF VAR fired ATLEAST 3 THEN GOTO big ELSE GOTO small
    LABEL big
        BUTTON Y HOLD 200
        SET fired 0
        GOTO next
    LABEL small
        BUTTON A HOLD 48
    LABEL next
ENDLOOP
```

Up to 8 counters per script. Names are picked up automatically the first time you mention one, in a `SET`, an `INC` or
another counter instruction, or an `IF VAR`. Unlike `COUNT` these are yours to do what you like with, they aren't tied
to a loop and they don't reset when one ends.

### Subroutines

`CALL <label>` jumps to a reusable block and remembers where it came from. `RETURN` resumes at the instruction after
that `CALL`. Put normal flow behind a `GOTO` so it does not fall through into the subroutine by accident:

```
NAME Two Bursts
GOTO main

LABEL burst
    BUTTON A HOLD 48 WAIT 48 REPEAT 3
    RETURN

LABEL main
    CALL burst
    PAUSE 250
    CALL burst
```

Calls may be nested 8 deep. Reaching `RETURN` without an active call, exceeding that depth, or otherwise corrupting call
flow stops only the running macro. Recursion is therefore bounded, but a loop is usually clearer when no return value is
needed.

### Analogue Sticks

`STICK <LEFT|RIGHT> <x> <y>` pushes a stick to an exact position, for content that reads the actual axis rather than the
digital d-pad. Both `<x>` and `<y>` run from `-32767` to `32767`, with `0 0` being centred. Negative `x` is left,
negative `y` is up.

```
NAME Look Around
STICK RIGHT 32767 0 HOLD 400
STICK RIGHT 0 -32767 HOLD 400
STICK RIGHT 0 0 HOLD 100
BUTTON A HOLD 96
```

`WAIT`, `HOLD` and `REPEAT` work exactly as they do on a `BUTTON` line, the stick returns to centre during the gap
between repeats, in the same way a button releases between them.

Two things worth knowing. While a `STICK` step is playing it takes that stick over completely, whatever the player is
physically doing with it is ignored until the step is done, there is no blending of the two. And the numbers you write
are sent to the game exactly as written, they deliberately skip the Deadzone, Anti-Deadzone, Sensitivity and Invert Y
settings. A macro is asking for one precise deflection on purpose, so running it back through those would make the
number you wrote a lie.

`STICK` is Relish only, as the on device editor has no way to express it.

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

`IF <condition> THEN <action>` where an `<action>` is `GOTO <label>`, `BREAK` or `STOP`. The `IF` always changes where
the macro continues; it never presses a button directly. A `condition` is one of:

| Form                  | Meaning                                                                                                                  |
|-----------------------|--------------------------------------------------------------------------------------------------------------------------|
| `<button> HELD`       | True if that physical button is currently held down.                                                                     |
| `COUNT <op> <n>`      | True if the nearest enclosing `LOOP`'s current pass number compares to `<n>` as `<op>` says. Only valid inside a `LOOP`. |
| `VAR <name> <op> <n>` | True if that counter compares to a number or `VAR <other>` as `<op>` says. See [Counters](#counters) above.              |
| `RANDOM <pct>`        | True `<pct>` percent of the time, rolled fresh each time the step is reached. See [Random](#random) above.               |

Any of them can be flipped by putting `NOT` in front, so `IF NOT A HELD THEN ...` and `IF NOT RANDOM 25 THEN ...` both
do what they look like.

`<op>` is one of `EQUALS`, `NOTEQUALS`, `LESS`, `GREATER`, `ATLEAST`, `ATMOST`
We use words here and not symbols (_like BASIC you know?_), matching the rest of the grammar. The `COUNT` is **1 index**
on a loop's first pass,
`COUNT EQUALS 1` is true and on its tenth, `COUNT EQUALS 10` is.

For example, `IF VAR score GREATER VAR target THEN GOTO won` compares two counters directly.

### ELSE

Adding `ELSE <action>` gives you the other branch, so you no longer need the "jump over the bit you didn't want"
dance:

```
IF R1 HELD THEN GOTO fast ELSE GOTO slow
```

It is pure convenience. That line compiles to the same `IF ... GOTO fast` step followed by a plain `GOTO slow`, which is
exactly what you'd have written by hand. Worth remembering though, an `ELSE` costs a second step slot out of the 32.

### Stopping

`STOP` ends the current run immediately. This is useful after an optional branch, or when the player should be able to
cancel a longer sequence without waiting for it to finish:

```
LOOP 100
    IF B HELD THEN STOP
    BUTTON A HOLD 48
ENDLOOP
```

A label at the very end of a script is also a valid `GOTO` destination and ends the macro, but `STOP` makes that intent
clearer.

## Examples

Roughly simplest to most advanced and each does one thing.

A larger set of ready-to-copy accessibility, game-input and playful demonstration scripts lives in
[`macro/examples`](../macro/examples/README.md).

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

Because `GOTO`, `CALL`, `LOOP` and `IF` can move between steps, the compiler runs a bounded validation pass before
accepting a script. For every step, it explores every possible outcome and confirms that control flow eventually reaches
a `BUTTON`, `PAUSE`, `STICK`, `RETURN` or the natural end of the script.

A chain that only jumps between control instructions fails to compile with a line reference instead of being accepted
and consuming CPU during playback. A script that is too complex for the fixed validation budget is rejected as well.

## Compile Errors

If a `.rls` file fails to compile, it will still appear in the macro list but as a **Broken Script** entry. You can
still delete it but it can't be bound to a button.

Selecting it will show the specific error as an on device message, some common errors include:

- `Line N: unrecognised keyword 'xyz'` - the first word on a line isn't one of the keywords above.
- `Line N: undefined label 'xyz'` - a `GOTO`, `CALL` or `IF ... THEN GOTO` references a label that doesn't exist.
- `Line N: label 'xyz' already defined` - the same `LABEL` name used twice.
- `Line N: unknown button 'xyz'` - a `BUTTON`/`IF` line references something other than the sixteen button tokens above.
- `Line N: ENDLOOP without a matching LOOP` / `Line N: LOOP without a matching ENDLOOP` - unbalanced loop blocks.
- `Line N: BREAK outside of a LOOP` - a bare `BREAK` or `IF ... THEN BREAK` with no enclosing `LOOP`.
- `Line N: COUNT used outside of a LOOP` - an `IF COUNT ...` condition with no enclosing `LOOP`.
- `Line N: unknown comparison operator 'xyz'` - the word after `COUNT`/`VAR` isn't one of the six listed above.
- `Line N: unexpected 'xyz' after the THEN action` - leftover words on an `IF` line, usually a typo'd `ELSE`.
- `Line N: 'xyz' is already defined` / `Line N: 'xyz' has not been defined` /
  `Line N: 'xyz' is not a usable DEFINE name`
    - see [Defines](#defines).
- `Line N: could not open included file 'xyz.rli'` / `Line N: 'xyz.rli' is included more than once` - see
  [Includes](#includes).
- `Line N: too many variables (max 8)` - see [Counters](#counters).
- `Line N: DIV cannot use zero` / `Line N: MOD cannot use zero` - use a non-zero literal or definition. A variable
  divisor is checked safely during playback.
- `Line N: STICK side must be LEFT or RIGHT, not 'xyz'` / `Line N: STICK requires an X and a Y between -32767 and
  32767` - see [Analogue Sticks](#analogue-sticks).
- `Line N: invalid HOLD RANDOM range` - the maximum is below the minimum, or one of them isn't a sensible number.
- `Line N: this Goto/Call/Loop/If chain never reaches a Button/Stick step or the end of the macro (or is too complex to
  verify)` - see [Jump safety](#jump-safety).
- `Line N: macro exceeds 32 steps` - executable lines share the same 32-step ceiling the on-device editor uses.
  `LABEL`, `LOOP`, `DEFINE`, `REDEFINE` and `INCLUDE` do not consume a step, while an `IF` with an `ELSE` consumes two.

Once a script uses `INCLUDE`, an error inside a spliced file reads `Line N of xyz.rli:` instead, so `Line N` always
means the line number in the file that's actually named.

## Stable Bindings

Binding a macro to a physical button records a numeric index, not a name or file path. The `.ini` based macros get that
index written into their own file the first time they're loaded. An `.rls` file is never written to, so instead Pickles
keeps a small file of its own, `.relish_index`, alongside the scripts in the same macro folder, mapping each script's
filename to its assigned index. This file is just for Pickles housekeeping. Nothing really worth editing manually. If
you do decide to delete the `.relish_index` (_for whatever reason_) it will just reassign the index on next load, which
_may_ break existing button bindings to Relish macros.

## Safety and Resource Limits

Relish is intentionally not a general-purpose interpreter. A running macro cannot open files, allocate memory, call the
operating system, invoke native code, inspect emulator memory or alter emulated CPU state. `INCLUDE` is the only script
feature that reads another file, and it is resolved entirely during compilation under the same-directory `.rli`
restrictions described above. Pickles' fixed `.relish_index` file is frontend bookkeeping and is never named or
controlled by a script.

Playback uses fixed per-port storage: 32 compiled steps, 8 counters and 8 nested calls, with at most 128 control
instructions handled per input poll. Counters use saturating fixed-point arithmetic, and invalid runtime operations stop
only that macro. Compilation is also bounded to 8 source files, 4 include levels, 512 meaningful lines and 128 KiB of
source, with all temporary compiler memory released before content resumes. These limits keep malformed or hostile
scripts from growing RAM use, monopolising CPU time, escaping their macro folder or affecting emulated memory.
