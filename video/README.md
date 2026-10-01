# Wasabi

Wasabi is the native MustardOS audio and video player. Its executable is `muxmedia`, following the same module naming used throughout muX.

## Layout

- `core/` contains the entry point and playback runtime.
- `content/` discovers local audio and videos, and reads M3U, M3U8, PLS and JSON channel lists.
- `settings/` owns Wasabi settings, values and persistence.
- `state/` stores History, bookmarks and Collection entries.
- `ui/` contains the playback and settings interface.
- `video/` owns frame presentation and the future shader pipeline.

The player links a dedicated static FFmpeg profile from `external/prefix/<device>/video`. The smaller FFmpeg profile used by frontend previews remains independent.

## Content

Wasabi is assigned to media through the normal system and core assignment flow. It does not appear in Applications and does not have a separate library or global settings screen.

Regular media opens directly. An M3U file is treated as an ordered track or episode collection unless it is launched as live content. An M3U8 file containing HLS control tags is opened as a single live stream, while an M3U8 file containing multiple channel entries is treated as an ordered channel list. PLS radio lists and JSON manifests containing named `mjh_master`, `url`, `stream` or `stream_url` values are also supported. Kodi and raw M3U metadata is accepted. TVHeadend `pipe://ffmpeg` entries are read without executing the command, and only their HTTP or HTTPS input URL is used. Relative playlist entries are resolved from the directory containing the playlist.

Audio files display their title, artist, album, year, genre, artwork and playback progress when that information is available. Wasabi uses nearby `cover`, `folder`, `front` or `album` images when embedded artwork is unavailable. Standard, lossless, tracker and console music formats are supported. MIDI uses the selected MustardOS soundfont, tracker modules use the static libopenmpt integration, and AY, GBS, GYM, HES, KSS, NSF, NSFE, SAP, SPC, VGM and VGZ files use Game Music Emu. FFmpeg handles the remaining formats, including QOA, DSD and common console audio containers.

Visualisers are available in the Display section for Audio. Spectrum, Waveform, Pulse, Orbit, Stereo Meter, Phase Scope, Radial Wave and Starfield modes are included. Visualisers are disabled by default to keep CPU and battery use low, run at no more than 30 frames per second when enabled and use fixed working buffers without per-frame allocation.

Audio bookmarks store their name and playback position without capturing a screenshot. Video bookmarks retain their optional thumbnail support.

Bookmarks, playback history and their screenshots are stored below:

```text
/run/muos/storage/save/wasabi
```

## Controls

- `A` or `START`: pause or resume
- `B`: return from the playback menu
- `Y`: name and save a bookmark at the current position
- `A` in Bookmarks: load the selected bookmark
- `X` in Bookmarks: remove the selected bookmark
- `LEFT` and `RIGHT`: seek by 10 seconds
- `L1` and `R1`: seek by 60 seconds
- `MENU`: open or close the playback menu
- `UP` and `DOWN` during channel playback: previous or next channel

Settings are arranged into Video, Visuals, Overlay, Display, Sound, Input and Advanced sections. The section bar uses LEFT and RIGHT, matching Pickles. Video and display changes are applied while playback remains open. Audio device rate and period changes are used the next time Wasabi starts.

Multi-entry Audio M3U content adds Tracks below Bookmarks. Other M3U content adds Episodes, while multi-entry M3U8 channel lists add Channels in the same position. Selecting an entry changes playback without returning to the frontend. Ordered local playlists continue with the next entry after playback finishes.

The playback menu also provides Information and Restart. Restart seeks local content back to its beginning without leaving Wasabi.

Wasabi captures a clean screenshot for each bookmark and when playback exits. These previews are shown in Bookmarks and History.

Saving a bookmark opens the same on-screen keyboard flow used by Pickles save states. A generated name is ready to use or edit, and bookmark rows show the playback time followed by that name.

## Rendering

YUV420 video is uploaded directly to an SDL YUV texture. Other decoded formats use the FFmpeg scaler before upload. Brightness, contrast, saturation, hue and gamma adjustments use FFmpeg's optimised video filters, while scaling, rotation, mirroring, borders, viewport changes, generated overlays and vignettes stay within the presentation path.
