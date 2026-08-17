# Render

A core asks for the render path it wants through `RETRO_ENVIRONMENT_SET_HW_RENDER`. Pickles gives it that path, the
closest one that works, or nothing - and a core given nothing falls back to its own software renderer.

Negotiation lives in `hw_render_bridge_negotiate()` (`video/hw_render.c`).

## Context types

| requested          | profile           | version                             |
|--------------------|-------------------|-------------------------------------|
| `OPENGLES2`        | GLES              | 2.0                                 |
| `OPENGLES3`        | GLES              | 3.0                                 |
| `OPENGLES_VERSION` | GLES              | from the request, 3.0 when unstated |
| `OPENGL`           | GL, compatibility | from the request, 2.0 when unstated |
| `OPENGL_CORE`      | GL, core          | from the request, 3.0 when unstated |

Anything else - Vulkan, D3D - has no mapping and is refused.

Whether a given type is reachable is decided at runtime, not by this table. Desktop GL is mapped so that supporting it
becomes a question of the platform having a desktop GL driver rather than of reworking negotiation.

## Selection

1. `context_requirements()` maps the request onto a profile and version.
2. If the existing context already reports what the core asked for, the core shares it. A context of its own has to be
   made current around every core call and on a embedded based driver that switch costs far more than the isolation is
   worth.
3. Otherwise the `create_shared_context()` function creates the core a context of its own, in the same share group as
   the existing one.
4. `shared_context_usable()` proves two things that are otherwise only assumptions - that the new context reports the
   version asked for, and that sharing actually happened, by creating a texture in it and looking for that texture from
   the other context. A driver may hand back a version other than the one requested, and sharing is a request nothing
   reports back on.
5. Otherwise, the request is refused.

`current_context_is()` answers what the live context is by reading `GL_VERSION`. `SDL_GL_GetAttribute` cannot: it
reports the attributes SDL was configured with, which includes anything set while probing, so one probe's leftovers
become the next probe's answer.

## Sharing

Textures and programs are shared across a share group; framebuffers are not.

That split is what makes two contexts workable. The core's framebuffer is created in - and only ever deleted from - the
core's context, while its colour texture is shared, so the blit path keeps sampling it exactly as it did when there was
only one context.

## Sharing the UI's context

When the core shares the existing context, `enter_core_gl()` / `leave_core_gl()` capture and restore GL state around
every core call instead of switching contexts.

State the UI set and the core overwrites is saved and put back - that is `gl_state_capture()` / `gl_state_apply()`, and
it covers ES2 era state. The muX UI never sets an ES3 state: vertex arrays, samplers, and the pixel pack, unpack and
uniform buffer bindings. Nothing saved for those because SDL2 renderer never binds them, so the value to put back is
always zero. `gl_reset_es3_state()` clears them.

`GL_FRAMEBUFFER_BINDING` is the draw binding on ES3, not both of them. A core is free to leave different objects bound
to `GL_READ_FRAMEBUFFER` and `GL_DRAW_FRAMEBUFFER`, and putting the state back through `GL_FRAMEBUFFER` sets both at
once, which would quietly collapse a split the core was relying on. Both are captured and restored separately wherever
ES3 is available, and the single binding remains the ES2 path.

Note that a driver asked for ES2 may return a context reporting a much higher version, so "the existing context is
good enough" cannot be inferred from what was requested when it was made.

## Targets

One colour texture and framebuffer, which is what RetroArch settles on for hardware rendered cores as well. Rotating
through several was measured and made no difference. The hazard that would justify it - the core drawing into the
texture the UI is sampling from another context - only exists while the core has a context of its own, and it no
longer does.

`hw_render_bridge_flush_core_commands()` submits the core's queued commands after the last frame of a batch while the
core's context is still current. It does not wait for them, so the work is already in flight by the time state is put
back and the texture is sampled.

## Fallback

Every step refuses rather than half works. A context that cannot be created, or is created but is not sharing, is torn
down and the core is told no - leaving it on its own software renderer, which is always available.

Refusals are logged with the reason. A silent refusal is worse than a slow renderer: it looks like the core simply
chose to be slow.

`hw_render_bridge_description()` reports the context actually in use, which the information screen shows so the
outcome is visible without reading logs.

## Game Renderer

Video settings carry a `Game Renderer` choice of `Hardware` or `Software`. Choosing `Software` makes
`SET_HW_RENDER` return false, which is the documented way of telling a core the frontend will not provide hardware
rendering, and the core falls back to the renderer it ships with. No backend is ever exposed - the choice is only
whether Pickles offers one at all.

The row is hidden for cores that never ask for hardware rendering, where the choice would mean nothing.
`environment_core_wants_hw_render()` records whether the request was seen.

Negotiation happens once while the content loads, so a change only takes effect the next time the content is loaded,
and cycling the option says so rather than appearing to do nothing. Settings are read before `core_load_content()` for
the same reason - a core asks for its renderer while loading, so anything acted on during load has to be in place
before it starts.

## Hardware rendering effects

Colour filters, user shaders and colour adjustments add another render pass around an already hardware-rendered frame.
Pickles therefore hides and suppresses the entire effects section by default whenever
`hw_render_bridge_active()` reports a hardware renderer. The underlying content, core and directory settings remain
intact, and software-rendered cores retain the normal controls and behaviour.

Advanced > Display > Pickles Hardware Effects exposes an experimental global override. Enabling it restores the colour
filter, shader, brightness, contrast, saturation, hue and gamma rows and applies their saved selections to
hardware-rendered content on the next launch.

## Known issue - hardware rendered PSX content

SwanStation and DuckStation run some content near 40 FPS during gameplay while the same title's in game menus hold 60,
through the same code path. RetroArch runs the same core, the same options and the same content at 60 throughout.

Measured and ruled out, so nobody has to walk it again:

- CPU and GPU clocks. Pinning both to maximum gained three frames.
- Core options, fastmem mode and CPU execution mode. Identical to the RetroArch setup that does not have the problem.
- Memory footprint. RetroArch resides larger than Pickles does and is unaffected.
- Audio pacing and rate control. `audio_bridge_wait_for_headroom()` never sleeps during the fault.
- Vsync quantisation. Disabling vsync entirely changed nothing.
- Frontend CPU cost. Around 11ms of a 25ms frame, and the main thread sits under half busy, so the loop is waiting
  rather than working.
- Render path shape. A single target, a shared context and one blit all match what RetroArch does.

What does track the fault is `mali-utility-wo`, the driver's GPU memory and job worker. It sits near a fifth of a core
during gameplay, drops to nothing in the menus, and is idle under RetroArch in the same scene. Attributing that needs
GPU counters rather than frame timers, so `Game Renderer` exists as the way out until someone can point a profiler at
it.
