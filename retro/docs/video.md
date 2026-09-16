# Video

See [`architecture.md`](architecture.md#video) for the file-by-file breakdown of `video/`.

## Video

- **Scaling Modes**: Fit Screen (default - largest aspect-correct size that fully fits, height-first with width
  fallback), Aspect, Integer, Stretch, Full Height, Full Width.
- **Image Corrections**: child page for targeted artifact fixes. See
  [`image-corrections.md`](image-corrections.md) for the shipped controls and candidate assessment.
- **Shimmer Fix**: optional snap of the final displayed rectangle to the largest uniform integer multiple of the visible
  source that fits the requested size, eliminating the fractional-scale resampling pattern visible on scrolling
  repeated textures without distorting or unnecessarily cropping the image. The snap runs after crop, zoom, stretch,
  and the logical-to-physical display mapping, so a later viewport transform cannot silently undo it.
- **Anti-Flicker**: optional, core-independent temporal filtering for software-rendered content. It requires each pixel
  to exactly repeat its value from two frames ago and differ strongly in at least one colour channel from the previous
  frame before confirming an A/B/A alternation. Per-channel contrast is intentional: saturated transparency colours can
  have nearly the same luminance as the background and must not bypass detection. Confirmed pixels remain blended for
  three further frames, covering the changing background visible during the opposite phase and one missed comparison
  without turning the effect into full-screen motion blur. This restores deliberate transparency and temporal smoothing
  effects designed around CRT phosphor or early LCD persistence. Exact repeat detection makes the ordinary non-flickering
  path inexpensive; held pixels skip repeat and contrast detection.
  ARM builds use NEON for all three libretro software pixel formats; frames at or above 1280×720 are split into four
  independent row ranges. The three helper threads, two-frame history, and one-byte-per-pixel confidence mask exist only
  while enabled and are released when it is disabled. History and confidence are reset across pauses, hidden core runs,
  geometry or pixel-format changes, resets, and state loads. Hardware-rendered cores do not expose the row.
- **Rotation**: 0°/90°/180°/270° via an off screen canvas, composable with **Mirrored** (horizontal flip).
  Core-requested rotation (`SET_ROTATION`) combines with the user's setting.
- **Viewport Offsets**: X/Y pixel offset and zoom with one-tap reset, applied on top of any scaling mode.
- **Viewport Cropping**: per-edge source pixel cropping (top/bottom/left/right) with an optional Centre Crop mode that
  recentres the cropped image on the display, ignoring the X/Y offsets.
- **Texture Filters**: none (nearest), smooth (linear), scale2x, scale3x, sharp bilinear, scale2x smooth, super eagle.
  Implemented in `filters/`, not `video.c` - see [`architecture.md`](architecture.md#video).
- **Colour grading**: brightness/contrast/saturation/hue-shift/gamma, plus drop-in filter presets (`.ini`) and shader
  presets (`.frag`) scanned from the `OPT_SHARE_PATH` filter and shader directories. Works for software and
  hardware-rendered cores.
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
  'input to run' gap. A reported panel rate is the scheduling period; a separately windowed presentation-cadence
  estimate is diagnostic only and cannot redefine the physical refresh rate.
- **Pacing After Present**: normal automatic, 50 Hz, slow-motion, and audio-headroom pacing all run _after_ the frame is
  presented, never between the core run and presentation. Normal automatic mode uses one monotonic frame deadline from
  the first frame; libretro audio callbacks only enqueue samples, while queue depth feeds dynamic rate correction and
  stale-queue recovery.
- **Adaptive Audio**: when the audio queue runs low, extra hidden frames are only granted out of measured headroom using
  frame calculations (`frame period / rolling core cost`), so a heavy core is never pushed into a catch up death spiral.
- **Run Ahead**: see below.
- **FPS Limit**: 60 (vsync), 50 (paced), or none.

## Fragment shader contract

Pickles runs a `.frag` preset against a source-resolution texture and scales that pass directly to the output. This
keeps source-pixel offsets stable and avoids an additional full-resolution preparation pass. The fragment preamble
provides:

- `u_tex`: prepared source texture.
- `u_resolution`: shader output size in pixels.
- `u_native_resolution`: native core frame size in pixels.
- `u_source_resolution`: active source texture size in pixels. This can differ from the native size after a CPU filter.
- `u_texture_resolution`: backing GL texture size in pixels.
- `u_source_uv_extent`: active source extent within the backing texture.
- `u_frame`: displayed shader frame counter. `u_time` retains the corresponding legacy floating-point frame value.
- `v_uv`: source coordinates from `(0, 0)` to `u_source_uv_extent`.

Shader sampling defaults globally to linear, matching the expected input filtering of common one-pass ports such as
zFast CRT without exposing another end-user setting. A shader can override that contract with `// Filter: Linear`,
`// Filter: Nearest`, or `#pragma filter linear|nearest|inherit`; `inherit` deliberately follows the selected Texture
Filter. A RetroArch port should map `OutputSize` to `u_resolution`, `InputSize` to `u_native_resolution`, and
`TextureSize` to `u_texture_resolution`.

The shader picker accepts explicit, evidence-owned guidance in the leading comment block. `// Cost: Low|Medium|High`
sets the general class, while `// Cost-720p:` and `// Cost-1080p:` can override it for those output tiers.
`// Compatibility: All|Software|Hardware` declares the supported source path. Unspecified cost and unrestricted
compatibility metadata are omitted from the shader list. Pickles never guesses cost from a file name. Ratings should
be assigned from measurements on the target GPU.

Both preset pickers place Download first. It fetches the configured filter or shader manifest only when a network is
connected, then reports each entry as Available, Installed or Update by comparing its SHA-256 with the local file. An
install is downloaded beside the destination, checked for HTTPS provenance, size, digest and content type, then
published with a final rename. A rejected download cannot replace the installed preset. Preset tables are dynamically
sized; a preset source may be up to 128 KiB and the separately bounded manifest may contain any number of entries that
fits its input and available memory.

Each manifest is a JSON array using this contract:

```json
[
  {
    "name": "Example Preset",
    "url": "https://example.invalid/example.frag",
    "version": "1.0.0",
    "sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
  }
]
```

The manifest URL comes from `extra/filter/data` or `extra/shader/data`. Stable preset names are saved alongside legacy
numeric values so installing a newly sorted file cannot silently change an existing content, directory or core choice.

The performance export records the effective filter and its source, every standard shader uniform, parameter values,
visible source size, logical destination, physical destination, and whether the physical mapping is an exact integer
multiple. This makes differences in input, texture, native, output, UV, frame, filter, or parameters explicit when a
shader issue is investigated.

## Run Ahead

Implemented as **preemptive frames**. The cheaper, and easier to implement, variant of actual run ahead: each frame the
engine serialise a one frame state anchor. When the input snapshot changes, it rolls back to the anchor and replays the
previous frame hidden (video skipped, audio muted, rumble suppressed etc.) with the new input before the visible frame
runs. The new input, hopefully, then lands one frame earlier than the games internal lag would allow.

This typically will only benefit software rendered cores only, also save states must be supported, steps aside during
fast forward and slow motion, and disables itself if the core ever fails to take advantage of it. The state anchor is
invalidated across every timeline discontinuity (state load, reset, post unpause audio priming). Intended primarily for
8 and 16-bit cores with small, fast states.
