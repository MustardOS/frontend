# Relish Example Pack

These scripts are starting points, not universal cheats. Copy an `.rls` script and any `.rli` file it includes into one
content macro folder, then tune the buttons and timing for that game. Controller layout, region, frame rate, facing
direction and in-game input buffering can all change the result.

## Practical Examples

- `accessibility_rapid_fire.rls` repeatedly taps `A` until physical `B` is held.
- `accessibility_hold_assist.rls` turns a short trigger into repeated half-second holds.
- `fighting_fireball_right.rls` performs a right-facing quarter-circle motion and attack with the left analogue stick.
- `platformer_jump_selector.rls` chooses a short or long jump according to physical `R1`.
- `shmup_focus_burst.rls` combines focus and fire for a short controlled burst.
- `reusable_confirm_sequence.rls` shows two calls to one labelled confirmation subroutine.

## Playful Examples

- `konami_code.rls` enters the familiar sequence and finishes with `START`.
- `trig_wave_walk.rls` uses a sine counter to alternate left and right across twelve angular samples.
- `counter_math_showcase.rls` demonstrates decimals, another counter as an operand, floor, ceiling, sine, cosine and
  tangent before reporting pass or fail with a button.

## Limits Worth Seeing

Relish can calculate and branch, but a counter cannot directly become a dynamic button duration or analogue-stick
position. Trigonometric input is in degrees and results have three decimal places. A right-facing motion usually needs a
mirrored left-facing copy. `STICK` support also depends on the core and emulated controller type. Every compiled macro
still has 32 executable steps, 8 counters and 8 nested calls, so reusable subroutines save repetition in the source but
each `CALL` and `RETURN` still consumes a step.

`common_timings.rli` is deliberately an include rather than a runnable macro. It stays out of the macro list and gives
the examples one place to tune their shared tap, gap and burst values.
