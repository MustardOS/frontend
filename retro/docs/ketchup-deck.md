# Ketchup Deck

A **Ketchup Deck** turns a spare controller into a helper for another players controller slot. The controller stops
being a "player" of its own and everything it does gets added to whichever player you point it at. From the cores side
nothing unusual is happening at all, hopefully. Player 1 just receives ordinary RetroPad input, and Pickles has quietly
folded two, or more, controllers into it before handing it over.

A rapid fire board, a macro pad, a second person helping someone else (_your annoying cousin?_) past a hard section, or
use it as like a makeshift accessibility device. If you can connect it via USB or Bluetooth it _should_ work. Also, it's
just called Ketchup because you know what the big tomato said to the little tomato on their walk... KETCHUP!

## How it all works

| Controller | May be a player | May be a deck | Can help       |
|------------|-----------------|---------------|----------------|
| Port 1     | Yes             | **No**        | -              |
| Port 2     | Yes             | Yes           | Port 1, 3 or 4 |
| Port 3     | Yes             | Yes           | Port 1, 2 or 4 |
| Port 4     | Yes             | Yes           | Port 1, 2 or 3 |

Port 1 is always a _real_ player, so there is always one controller used by a human. A deck can never point at itself,
and it can never point at another deck, so there is no way to build a chain or a loop. Two ports cannot wear the same
layout either. Pick one that's already in use and the other port will just give up the control.

## Setting one up

On the port you want to hand over (**Settings → Input → Port 2**, say):

1. **Controller** - cycle to **`Ketchup Deck`**, which sits just after the connected pads. The row list changes as soon
   as you do. The value keeps naming the controller in use and marks it as a deck, so it reads something like
   `8BitDo Pro 2 (KD)` and you can always see which pad is doing the helping. (_KD means Ketchup Deck fyi..._)

   Because that entry sits directly after the pads, picking a particular controller first and then stepping onto
   `Ketchup Deck` keeps that controller as the deck's own. Step onto it from `Auto` and the deck takes whichever pad is
   going spare.
2. **Ketchup Layout** - `X` makes a new layout and names it. If you already have layouts, `L`/`R` picks one and `Y`
   renames or deletes the current one.
3. **Ketchup Controls** - who this deck is helping. Only ports that aren't decks themselves are offered.
4. **Ketchup Priority** - see [Priority](#priority) below. Off is the sensible default for most things.
5. **Button Mapping** and **Macros** now edit *the layout*, not the port. Set up the buttons however you like.

## Layouts

A layout is the whole deck setup in one named file: who it helps, every button mapping, every turbo rate, every macro
binding, and the priority flag etc. They live on their own in `save/pickles/deck/<name>.ini`. The same layout can be
used by other controllers on any content. Up to 16 of them, mange them wisely.

Which port is *using* which layout is an ordinary Pickles setting, so it inherits per content, per core and per
directory like everything else in [settings](settings.md). The uptake is that one `Fight Stick` layout can be used
across every fighting game, while others still remember whether it wanted a deck at all.

### Deck the ports with lots of macros

[Macros](macros.md) are stored per content, but a layout is global, so a layout records the macro's **name** rather than
its position in any one game's macro folder. When content loads, every macro button in every layout is looked up by name
against that game's macros (case-insensitively).

The practical upshot: name a Relish script `Fireball` in each fighting games macro folder and one `Fight Stick` layout
activates all of them, doing the right thing in each order. Where a game has no macro of that name the button simply
does absolutely nothing. Add a macro of that name later, or go back to content that has one, and the button starts
working again on its own. Magic.

## What a deck button can do

Everything a normal button can do, because a deck reuses the same mapping stuff as a player port. It's just the
standard [Button Mapping](architecture.md#input) and [Macros](macros.md) screens acting on the Ketchup layout instead of
on the port.

| Want                | Set it up as                                                                                 |
|---------------------|----------------------------------------------------------------------------------------------|
| A second `A` button | Button Mapping - point the button at `A`.                                                    |
| Rapid fire          | Button Mapping - point it at a target, then `L`/`R` to pick a turbo cycle length.            |
| Two buttons at once | A one-step macro - on Add Step → Button, hold both buttons.                                  |
| A timed sequence    | A macro with several steps (see [Macros](macros.md)).                                        |
| Anything cleverer   | A [Relish](relish.md) `.rls` script - loops, conditions, randomness, counters, stick sweeps. |
| A stick direction   | Button Mapping's target picker, or a Relish `STICK` step.                                    |

Turbo rates are the length of the full press-and-release cycle in milliseconds (48 / 96 / 192 / 384 / 768 / 1536 /
3072 / 6144), not a frequency, and the first press always fires immediately so a tap still feels instant. 96ms works out
around 10Hz at 60fps. The `L`/`R` turbo hint only appears on rows that are actually bound to something.

## How the two controllers combine

- If **either** the player's own controller or the deck asks for a button, the core sees it pressed.
- A deck can never *stop* the player pressing something so the actual physical input stays responsive the whole time a
  macro is running.
- Sticks take whichever side is pushing further so the deck controller sitting at rest won't do anything to the actual
  player controller.
- Fake ketchup presses are worked out fresh every frame, so nothing can get stuck down. Change the controller port from
  Ketchup, open the menu, pause, change layout, or exit the content and its input simply stops existing in Pickles.

If Player 1 is using a Bluetooth controller and runs out of battery the Ketchup connected controller will just take its
place.

### Priority

Turn **Ketchup Priority** on and the deck will override anything on the other port:

- Opposed directions resolve in the Ketchups favour. If the player is holding Left and the deck presses Right, the game
  character will go Right - rather than Left and Right at once. Some cores will handle that functionality different.
- The deck controller stick takes over outright while it's being moved, that was sort of explained above.

Everything not in direct conflict is still added together as normal, and the player is technically never locked out. Let
go of the deck based controller and the player 1 controls will return instantly. This is what you want when somebody is
*taking over* a tricky bit. But typically you'll just want to leave it off when two people are genuinely playing
together to play hotseat / swap type games.

## Accessibility

Okay so this is _really_ what Ketchup was built for.

- **Mash buttons onto one button.** A game that wants L1+R1+X becomes one button on the deck, via a one step macro.
- **Holding the buttons.** Anything that expects repeated tapping becomes hold to turbo/tap, which is enormously easier
  on hands that don't want to do that.
- **Reachable placement.** Shoulder buttons and stick clicks can move to face buttons on a pad that suits the person
  playing, without touching how Player 1 own controller behaves.
- **Timing removed.** A sequence that needs _"frame accurate"_ input becomes one press, as a macro or a Relish script.
- **A second pair of hands.** Somebody else can help without taking the controller away, which is a very different
  experience from handing over. Helping is handy!

Because the deck is a whole separate physical "controller", the person being helped keeps their own pad exactly as it
was. Nothing about their setup changes. Remember you can simply save the settings for that session only!

## Co-pilot

_No, not that one._

Point a deck at Player 1 and map its buttons straight through with no macros or turbo at all, and you'll have co-pilot
mode - two people sharing one character. A common shape is to give the helper only the hard parts:

```
Controller 2 -> Ketchup Deck -> Ketchup Controls: Port 1

  D-pad   unbound          the player keeps all the movement
  A       unbound
  B       unbound
  L1      -> L1 + R1       the combo they can't do
  R1      -> Turbo A       the mashing they'd rather not
```

Leave anything unbound that the helper shouldn't touch. With priority off, the helper genuinely can't take control
away.  Perhaps one player can steer, and another can accelerate and brake?

## Working examples

### A Bluetooth macro board

The classic use! Pair a spare Bluetooth pad in MustardOS as usual, then launch your content and give it a deck layout.
It never needs to be a player, of sorts.

```
Port 2  Controller       Ketchup Deck
        Ketchup Layout   SF2 Board
        Ketchup Controls Port 1
        Ketchup Priority Disabled

  A   -> Macro "Fireball"        Relish script, down / down-forward / forward + punch
  B   -> Macro "Dragon Punch"    forward / down / down-forward + punch
  X   -> Turbo Y @ 96ms          jab pressure, about 10Hz
  Y   -> Macro "Throw"           one-step combo, forward + heavy punch
  L1  -> Macro "Block String"    a longer Relish sequence with its own timing
  R1  -> L1                      plain passthrough, nothing clever
```

Player 1 keeps playing normally on their own pad the entire time. The board just adds to them. Because the layout is a
file of its own, the same `SF2 Board` can be worn on any fighting game, on any port, by any controller.

### A rapid-fire board for shmups

No macros at all, just turbo:

```
  A -> A @ 48ms      fastest cycle
  B -> B @ 96ms
  X -> A @ 384ms     slower tapping for the weapons that don't like being mashed
```

Hold the button on the deck, keep your own hands free for dodging.

### Helping a kid past a boss

```
Ketchup Controls Port 1
Ketchup Priority Enabled

  D-pad -> D-pad     with priority on, an adult can steer out of a corner
  A     -> A
  B     -> B
```

Git gud scrub.

### A second Relish trigger pad

Drop `.rls` scripts into the content's macro folder as usual (see [Relish](relish.md)), then bind them to deck buttons
rather than to the player's own controller. The player's pad keeps all sixteen buttons for actually playing, and the
scripts live on a pad that does nothing else.
