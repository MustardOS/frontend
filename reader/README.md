# Chutney

Chutney is the native MustardOS document reader. Its executable is `muxreader`, following the same module naming used throughout muX.

## Layout

- `main.c` contains the entry point, the reading controls and the reader actions used by the menu.
- `menu.c` builds the Chutney Menu, the sectioned Settings screen, the Font list and the Contents list with the same list and section framework as Pickles and Wasabi.
- `engine/` wraps MuPDF to open, lay out and draw documents. The Pickles manual screen builds the same file as its own object, so both share one engine.
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

The Chutney Menu holds Resume, Contents, Go to Page, Settings and Quit, and opens on the row used last. Quit is never remembered, so pressing `A` straight after opening the menu cannot close Chutney. Go to Page moves by one page with `LEFT` and `RIGHT` and by ten pages with `L1` and `R1`, then `A` jumps there.

Settings is split into two sections, moved between with `LEFT` and `RIGHT` on the section bar or `L1` and `R1` anywhere:

- Layout holds Font, Text Size, Margins and Line Spacing for EPUB and TXT documents, or Page Fit, Zoom and Page Order for PDF, CBZ and CBT documents.
- Reading holds Theme, Rotation, Transition, Page Status and Stay Awake. Theme is left out for comics.

Page Order reads a PDF or comic from its last page to its first, for manga stored in that order. It is kept for each document in its position file, written as the file page followed by `1` for reversed, so other documents are not affected. The page number, Go to Page and Contents follow the reversed order, and switching on the first page moves to the new first page.

Transition chooses how a new page appears: Instant, Crossfade, Slide or Page Turn. Turning back plays the same effect in reverse, and a new page turn during an animation starts from the frame on screen. Stay Awake keeps the device from dimming or sleeping while a page is open.

## Fonts

Font opens a list of Book Default, the fonts in `/opt/muos/share/font/internal` and the user fonts in `MUOS/font` on each storage device, loose or one folder deep. Book Default keeps the fonts the document asks for. Any other choice is applied to EPUB and TXT documents through a reserved `ChutneyFont` family that the engine answers from the chosen file. Files ending in `-Bold`, `-Italic` or `-BoldItalic` beside the font are used for bold and italic text, and MuPDF draws bold and italic itself when they are missing. Chinese, Japanese and Korean glyphs the font lacks still come from the Noto Sans fonts. The chosen font is saved as a path and falls back to Book Default if the file is removed.
