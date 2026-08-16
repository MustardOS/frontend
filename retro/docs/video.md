# Video

See [`architecture.md`](architecture.md#video) for the file-by-file breakdown of `video/`.

## Video

- **Scaling Modes**: Fit Screen (default - largest aspect-correct size that fully fits, height-first with width
  fallback), Aspect, Integer, Stretch, Full Height, Full Width.
- **Shimmer Fix**: optional snap of the destination rect to exact integer multiples of the native frame size on both
  axes, eliminating the fractional-scale resampling shimmer visible on scrolling repeated textures (e.g. SMB1 bricks).
  Well it at least _tries_ to, it's not exactly perfect but gets the job done in most cases.
- **Rotation**: 0°/90°/180°/270° via an off screen canvas, composable with **Mirrored** (horizontal flip).
  Core-requested rotation (`SET_ROTATION`) combines with the user's setting.
- **Viewport Offsets**: X/Y pixel offset and zoom with one-tap reset, applied on top of any scaling mode.
- **Viewport Cropping**: per-edge source pixel cropping (top/bottom/left/right) with an optional Centre Crop mode that
  recentres the cropped image on the display, ignoring the X/Y offsets.
- **Texture Filters**: none (nearest), smooth (linear), scale2x, scale3x, sharp bilinear, scale2x smooth, super eagle.
  Implemented in `filters/`, not `video.c` - see [`architecture.md`](architecture.md#video).
- **Colour grading**: brightness/contrast/saturation/hue-shift/gamma, plus drop-in filter presets (`.ini`) and shader
  presets (`.frag`) scanned from `/opt/muos/share/{filter,shader}/`. Works for software and hardware-rendered cores.
- **Border Colour**: theme / black / dark grey / white, filled outside the game's `dest_rect`.
- **Overlays**: predefined fullscreen patterns or a per game catalogue overlay, rendered as part of the video content
  layer. Below the pause menu, header, and indicators.

## Hardware Render

Cores that require an OpenGL ES 2 context (`RETRO_ENVIRONMENT_SET_HW_RENDER` with `RETRO_HW_CONTEXT_OPENGLES2`, e.g.
flycast) render into an FBO owned by Pickles, sharing SDL_Renderer's GLES2 context. Key invariants, all handled in
`video/hw_render.c`:

- `SDL_RenderFlush()` before any raw-GL draw (render batching would otherwise reorder the queued clear over the frame).
- SDL_Renderer caches GL state and skips reissuing it, so the core's GL activity is bracketed by exact snapshot/restore
  around every `retro_run()` batch (`context_save`/`context_restore`).
- The core keeps its _own_ GL state cache too, so out-of-run entry points that may drive its renderer
  (serialise/unserialise/reset/context_destroy) get the inverse bracket (`enter_core_call`/`exit_core_call`), handing
  the core back exactly the state it left.
- `bottom_left_origin` cores are V-flipped at composite. Colour filters/shaders route through an intermediate texture.

Other context types (GL core profile, GLES3, Vulkan) are rejected so the core can fall back to software rendering.

## Performance and Latency

- **Late Input Polling**: input is repolled inside `input_bridge_begin_run()`, _after_ the Frame Delay wait, so the core
  always sees the freshest possible input.
- **Frame Delay**: off / auto (p95-adaptive) / 1-16 ms. Delays the core run within the frame period to shrink the
  'input to run' gap.
- **Pacing After Present**: all pacing sleeps (audio headroom, 50 Hz, slowmotion timing etc.) run _after_ the frame is
  presented, never betwixt the core runs and the present frame.
- **Adaptive Audio**: when the audio queue runs low, extra hidden frames are only granted out of measured headroom using
  frame calculations (`frame period / rolling core cost`), so a heavy core is never pushed into a catch up death spiral.
- **Run Ahead**: see below.
- **FPS Limit**: 60 (vsync), 50 (paced), or none.

## Run Ahead

Implemented as **preemptive frames**. The cheaper, and easier to implement, variant of actual run ahead: each frame the
engine serialise a one frame state anchor. When the input snapshot changes, it rolls back to the anchor and replays the
previous frame hidden (video skipped, audio muted, rumble suppressed etc.) with the new input before the visible frame
runs. The new input, hopefully, then lands one frame earlier than the games internal lag would allow.

This typically will only benefit software rendered cores only, also save states must be supported, steps aside during
fast forward and slow motion, and disables itself if the core ever fails to take advantage of it. The state anchor is
invalidated across every timeline discontinuity (state load, reset, post unpause audio priming). Intended primarily for
8 and 16-bit cores with small, fast states.
