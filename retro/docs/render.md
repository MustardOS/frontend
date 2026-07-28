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
2. `create_shared_context()` creates the core a context of its own, in the same share group as the existing one.
   Isolation is the point: whatever the core leaves behind in its own context, nothing else is drawing with it.
3. `shared_context_usable()` proves two things that are otherwise only assumptions - that the new context reports the
   version asked for, and that sharing actually happened, by creating a texture in it and looking for that texture from
   the other context. A driver may hand back a version other than the one requested, and sharing is a request nothing
   reports back on.
4. If that fails, and only for `OPENGLES2`, the core may instead borrow the existing context - see below.
5. Otherwise the request is refused.

`current_context_is()` answers what the live context is by reading `GL_VERSION`. `SDL_GL_GetAttribute` cannot: it
reports the attributes SDL was configured with, which includes anything set while probing, so one probe's leftovers
become the next probe's answer.

## Sharing

Textures and programs are shared across a share group; framebuffers are not.

That split is what makes two contexts workable. The core's framebuffer is created in - and only ever deleted from - the
core's context, while its colour texture is shared, so the blit path keeps sampling it exactly as it did when there was
only one context.

## Borrowing

When a core cannot be given its own context it may share the existing one, and `enter_core_gl()` / `leave_core_gl()`
then capture and restore GL state around every core call instead of switching.

That only holds while every piece of state the core touches is on the list being saved, and the list covers ES2 era
state. An ES3 core leaves bindings behind that are not on it - vertex arrays, samplers, uniform buffers - and the
result is not a slightly wrong frame, it is the whole interface no longer drawing. So borrowing is offered only for
`OPENGLES2`, the case it was written for. Anything else is refused instead, and refusing costs a core its hardware
renderer rather than costing a usable screen.

Note that a driver asked for ES2 may return a context reporting a much higher version, so "the existing context is
good enough" cannot be inferred from what was requested when it was made.

## Fallback

Every step refuses rather than half works. A context that cannot be created, or is created but is not sharing, is torn
down and the core is told no - leaving it on its own software renderer, which is always available.

Refusals are logged with the reason. A silent refusal is worse than a slow renderer: it looks like the core simply
chose to be slow.

`hw_render_bridge_description()` reports the context actually in use, which the information screen shows so the
outcome is visible without reading logs.
