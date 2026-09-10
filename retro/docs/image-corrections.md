# Image Corrections

**Image Corrections** is the Video settings child page for narrowly targeted fixes to source or scaling artifacts. These
controls are separate from Texture Filter and Visuals: they should correct a recognisable problem, be disabled by
default when content-dependent, and have a bounded fast path when enabled.

## Shipped corrections

- **Shimmer Fix** snaps the output rectangle to whole multiples of the native core frame. It addresses uneven pixel
  sizing during scrolling and remains available for software- and hardware-rendered cores.
- **Anti-Flicker** detects exact, high-contrast A/B/A and AA/BB/AA temporal alternation and selectively blends confirmed
  pixels. Contrast is measured per colour channel rather than from luminance alone, preserving detection when a saturated
  transparency colour and its background have similar brightness. The second cadence covers games or cores that hold each
  flicker phase for two output frames. It operates on libretro software frame buffers, so it is hidden for hardware-rendered
  cores.

Both settings retain their existing content/core/directory/session persistence. Moving them into the child page changes
only navigation, not configuration keys or profile compatibility.

## Candidate review

| Candidate                                              | Decision                       | Production requirement                                                                                                                                                                                                                             |
|--------------------------------------------------------|--------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Selective de-dither / fake-transparency reconstruction | Investigate next               | Detect only known checkerboard/line patterns, preserve genuine texture detail, provide scalar/SIMD parity, and remain inside the 1080p H700 budget. A representative libretro shader describes both sensitivity limits and possible posterisation. |
| Interlace de-flicker / weave                           | Hold                           | Pickles needs a trustworthy way to distinguish true alternating fields from ordinary progressive content. Blind weaving can trade flicker for combing on motion.                                                                                   |
| Black-frame insertion                                  | Do not add for current devices | RetroArch documents this for presenting 60 Hz content on a 120 Hz display. Current H700 targets are approximately 60 Hz, where inserting black frames would halve visible updates and brightness rather than correct motion.                       |
| Overscan and pixel-aspect correction                   | Already covered                | Viewport Cropping and Aspect Ratio provide explicit, reversible controls without another correction setting.                                                                                                                                       |
| Colour, LCD, CRT, and signal simulation                | Already covered                | These are aesthetic/display emulation choices and belong under Visuals filters and shaders rather than Image Corrections.                                                                                                                          |

Primary references:

- [Libretro shader collection](https://github.com/libretro/slang-shaders) — established categories include dithering,
  deblurring, deinterlacing, interpolation, and motion processing.
- [Koko-aio de-dither documentation](https://github.com/libretro/slang-shaders/blob/master/bezel/koko-aio/docs-ng.md)
  — documents sensitivity, fake-transparency reconstruction, and posterisation risk.
- [RetroArch reference configuration](https://github.com/libretro/RetroArch/blob/master/retroarch.cfg) — documents
  black-frame insertion as a 120 Hz technique for 60 Hz material.
