# Chutney

Chutney is the native MustardOS document reader. Its executable is `muxreader`, following the same module naming used throughout muX.

## Layout

- `main.c` contains the entry point, reading controls, the menu and the contents list.
- `engine/` wraps MuPDF to open, lay out and draw documents. The Pickles manual screen builds the same file as its own object, so both share one engine.
- `list.c` draws the windowed list used by the menu and the contents list.
- `settings.c` stores reading settings and the position in each document.
- `view.c` places the current page on screen, applies the colour theme and rotation, and draws the page status.

Chutney links a static MuPDF built by `external/mupdf.sh`. MuPDF uses the system FreeType, HarfBuzz, libjpeg and zlib, so only its own decoders are bundled.

## Content

Chutney is assigned to documents through the normal system and core assignment flow, as the Chutney core of the Book Reader system. It does not appear in Applications.

PDF, EPUB, CBZ, CBT and TXT documents are supported. EPUB and TXT documents are laid out to fit the screen using the chosen text size, margins and line spacing. PDF, CBZ and CBT pages keep their own layout and can be fitted to the width or the whole page, then zoomed.

Chinese, Japanese and Korean text uses the Noto Sans SC, TC, JP and KR fonts already installed for the interface languages, so nothing extra is bundled. Text marked with a language uses the matching font. Unmarked Chinese characters use the font for the interface language, falling back to Simplified Chinese.

The reading position of each document and the reading settings are stored below:

```text
/run/muos/storage/chutney
```

Positions in EPUB and TXT documents are kept as a place within a chapter, so they survive changes to the text size, margins, line spacing and rotation.

## Controls

- `UP` and `DOWN`: scroll the page, moving on to the next or previous page at either end
- `LEFT` and `RIGHT`: move across a zoomed page, or turn a page when the whole width is shown
- `A` and `B`: next and previous page
- `L1` and `R1`: previous and next page
- `L2` and `R2`: smaller and larger text, or zoom out and in on PDF, CBZ and CBT pages
- `X`: contents
- `Y`: next colour theme
- `SELECT`: next rotation, turning clockwise through Landscape, Portrait Right, Upside Down and Portrait Left
- `MENU` or `START`: open or close the menu

When the page is rotated, the directions follow the rotated page.

The menu opens on the row used last, which is kept between sessions. Quit is never remembered, so pressing `A` straight after opening the menu cannot close Chutney. The menu holds Contents, Go to Page, the layout settings for the document type, Theme, Rotation, Transition, Page Status and Stay Awake. Transition chooses how a new page appears: Instant, Crossfade, Slide or Page Turn. Turning back plays the same effect in reverse, and a new page turn during an animation starts from the frame on screen. Go to Page moves by one page with `LEFT` and `RIGHT` and by ten pages with `L1` and `R1`, then `A` jumps there. Stay Awake keeps the device from dimming or sleeping while a page is open.
