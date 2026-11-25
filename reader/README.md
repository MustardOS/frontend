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

Positions in EPUB and TXT documents are kept as a place within a chapter, so they survive changes to the text size, margins, line spacing and rotation. The position is saved a few seconds after each page turn as well as on exit, so a flat battery loses almost nothing. Bookmarks are kept beside it in `bookmark/`, in the same form.

The page breaks of an EPUB are saved in `layout/` once it has been laid out, so the next time it opens with the same font, text size, margins, line spacing and rotation it shows straight away instead of laying out every chapter again. The saved breaks are tied to the document's size and modification time, the chosen font and the interface language, and are rebuilt when any of these change.

## Performance

- Once the screen settles, the pages either side of the current one are drawn ahead in the direction of reading and kept with the current page, so turning forwards or back shows a page that is already drawn. Up to 24 MB of drawn pages is kept, and very large zoomed pages are not drawn ahead.
- The menu only checks whether a document has contents when it opens. Working out the page of every entry happens when Contents is opened.
- Crossfade is blended by the renderer from two textures. Slide and Page Turn only work out the pixels around the moving edge.
- Rotated pages are copied to the screen in small tiles, and the text in the page status is drawn once and reused until it changes.

## Controls

- `UP` and `DOWN`: scroll the page, moving on to the next or previous page at either end
- `LEFT` and `RIGHT`: move across a zoomed page, or turn a page when the whole width is shown
- `A` and `B`: next and previous page
- `L1` and `R1`: previous and next page
- `L2` and `R2`: smaller and larger text, or zoom out and in on PDF, CBZ and CBT pages
- `X`: contents
- `Y`: next colour theme
- `SELECT`: next rotation, turning clockwise through Landscape, Portrait Right, Upside Down and Portrait Left
- `MENU` or `START`: open or close the menu, with `MENU` opening it when let go on its own
- `MENU` held with another button: a hotkey from the Hotkeys section of Settings

When the page is rotated, the directions follow the rotated page.

The Chutney Menu holds Resume, Contents, Bookmarks, Go to Page, Settings and Quit, and opens on the row used last. Quit is never remembered, so pressing `A` straight after opening the menu cannot close Chutney. Go to Page moves by one page with `LEFT` and `RIGHT` and by ten pages with `L1` and `R1`, then `A` jumps there.

Quitting saves a picture of the page on screen as `state/<hash>/exit.png`, and each bookmark saves `state/<hash>/bookmark_<position>.png`, named by a hash of the document path as Wasabi does. History and Explore show the quit picture the same way they show a Pickles save state or a Wasabi exit frame, following the Save Screenshot setting. The Bookmarks list shows the picture of the selected bookmark in the corner, and removing a bookmark removes its picture.

Bookmarks starts with Bookmark This Page, which becomes Remove Bookmark on a page that is already marked. Below it are the bookmarks in reading order, named by chapter where the document has contents. `A` jumps to a bookmark and `X` removes it. Up to 64 bookmarks are kept for each document.

Settings is split into four sections, moved between with `LEFT` and `RIGHT` on the section bar or `L1` and `R1` anywhere:

- Layout holds Font, Text Size, Margins and Line Spacing for EPUB and TXT documents, or Page Fit, Zoom, Trim Margins and Page Order for PDF, CBZ and CBT documents.
- Reading holds Theme, Rotation, Transition, Header Visibility, Page Number, Current Readtime, Auto Turn and Stay Awake. Theme is left out for comics.
- Display holds Colour Filter, Brightness, Contrast, Saturation, Hue Shift, Gamma and Sharpness.
- Hotkeys holds the `MENU` button combinations.

Trim Margins crops the blank border around the content of each page, so the text of a PDF with wide margins fills the screen. Pages with little or no content are shown whole.

Display settings match Wasabi and apply to every theme and to comics, and to the page status bands as well as the page:

- Colour Filter opens the same filter browser as Wasabi: Downloads, Collection, None, the bundled MustardOS filters and any downloaded filters, which are kept in `chutney/filter`. `X` adds a filter to the Collection and `Y` removes a downloaded filter. `A` on the row opens the browser; `LEFT` and `RIGHT` do nothing there. Moving through the browser previews each filter on the page once the cursor settles for a quarter of a second, `A` keeps it, and `B` anywhere in the browser or closing the menu goes back to the filter in use.
- Brightness runs from -100% to +100%, Contrast, Saturation and Gamma from 0% (Gamma from 50%) to 200%, and Hue Shift from -180° to +180°, all in steps of 5.
- Sharpness is Normal, High or Maximum. High and Maximum sharpen edges, which helps soft scans and thin text. A `contrast=` value saved by an older Chutney is read as Sharpness.

The colour maths follow the Wasabi colour shader, so a filter looks the same in both.

## Saved settings

Leaving Settings after a change asks where it applies, as Pickles and Wasabi do: This Content, This Directory, All Content, This Session Only or Discard. Content and directory settings are kept in `settings/content/` and `settings/directory/`, named by a hash of the document or folder path, and are applied over the global `settings.ini` when a document opens. Hotkeys and the remembered menu row are always global. Changes made without being asked, such as text size with `L2` and `R2`, are saved on exit to the content settings when the document has them, then the directory settings, otherwise globally.

## Hotkeys

Hotkeys are `MENU` plus a button. `LEFT` and `RIGHT` on a row move through the buttons and Unbound, and a button taken by another hotkey is unbound there. They are saved in `settings.ini` as `hotkey_<name>=<button>`, with `-1` for unbound:

| Hotkey | Default |
|---|---|
| Quick Quit (saves the position) | `MENU` + `START` |
| Toggle Bookmark | `MENU` + `A` |
| Next Colour Filter | `MENU` + `X` |
| Next Header Visibility | `MENU` + `Y` |
| Next Sharpness | `MENU` + `SELECT` |
| Toggle Auto Turn (back to the last time used) | `MENU` + `B` |
| Brightness Down and Up | `MENU` + `DOWN` and `UP` |
| Contrast Down and Up | `MENU` + `L2` and `R2` |
| Back and Forward Ten Pages | `MENU` + `LEFT` and `RIGHT` |
| Previous and Next Chapter | `MENU` + `L1` and `R1` |

Previous Chapter goes to the start of the current chapter first. Changes made by a hotkey show briefly as a message.

Header Visibility is None, Clock, Battery or Clock and Battery. It uses the theme's own header over the page with a transparent background, so the page keeps the whole screen, and the battery follows the theme's battery style.

Page Number shows the page in a small panel at the bottom left, and Current Readtime shows how long the document has been open at the bottom right, both styled like the FPS and Current Playtime panels in Pickles. Neither takes space from the page. An older `page_status` setting of Page Number turns Page Number on, and Full also turns on Clock and Battery.

While any menu is open, holding `MENU` hides it to preview the page, as in Pickles and Wasabi, and letting go brings it back. A short press still closes the menu.

Auto Turn moves on by itself after 10 to 120 seconds without a button press. A page taller than the screen is scrolled a screen at a time before the next page, and it stops at the last page.

Page Order reads a PDF or comic from its last page to its first, for manga stored in that order. It is kept for each document in its position file, written as the file page followed by `1` for reversed, so other documents are not affected. The page number, Go to Page and Contents follow the reversed order, and switching on the first page moves to the new first page.

Transition chooses how a new page appears: Instant, Crossfade, Slide or Page Turn. Turning back plays the same effect in reverse, and a new page turn during an animation starts from the frame on screen. Stay Awake keeps the device from dimming or sleeping while a page is open.

## Fonts

Font opens a list of Book Default, the fonts in `/opt/muos/share/font/internal` and the user fonts in `MUOS/font` on each storage device, loose or one folder deep. Book Default keeps the fonts the document asks for. Any other choice is applied to EPUB and TXT documents through a reserved `ChutneyFont` family that the engine answers from the chosen file. Files ending in `-Bold`, `-Italic` or `-BoldItalic` beside the font are used for bold and italic text, and MuPDF draws bold and italic itself when they are missing. Chinese, Japanese and Korean glyphs the font lacks still come from the Noto Sans fonts. The chosen font is saved as a path and falls back to Book Default if the file is removed. Moving through the list previews each font on the page once the cursor settles for a quarter of a second, `A` keeps it and `B` goes back to the font used before the list opened.
