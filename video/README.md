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

Audio files display their title, artist, album, year, genre, artwork and playback progress when that information is available. Wasabi uses nearby `cover`, `folder`, `front` or `album` images when embedded artwork is unavailable. Standard, lossless, tracker and console music formats are supported.

Each audio extension belongs to exactly one backend, listed in `content_path_audio_backend()` in `common/content/content.c`. Wasabi opens each file with its backend directly instead of guessing, so two backends never compete for the same file:

- MIDI, KAR and RMI use SDL_mixer with the selected MustardOS soundfont.
- AY, GBS, GYM, HES, KSS, NSF, NSFE, SAP, SPC, VGM and VGZ use Game Music Emu. Tracks without a stored length play for their looped length and then fade out.
- SID uses libsidplayfp with its SIDLite engine. Song lengths are read from an HVSC `Songlengths.md5` found beside the tune or in a parent `DOCUMENTS` folder, otherwise each song plays for three minutes. Seeking replays the tune up to the new position, so long jumps take a moment and SID files do not resume from History.
- Console stream formats that FFmpeg cannot read, such as Nintendo DSP, HPS, BCWAV, BFWAV and BWAV, PlayStation XA, NPSF and VGS, Xbox XWB, CRI ACB, Interplay ACM and Westwood AUD, use vgmstream. Looped audio plays twice and then fades out over ten seconds.
- Everything else uses FFmpeg, which checks the file header to choose between libopenmpt for tracker modules and its own decoders for standard, lossless, DSD, Dolby TrueHD and console audio such as GameCube AFC, AST and DTK, PlayStation 2 ADS, SVAG, SVS and VPK, and PlayStation 3 MSF and XVAG.

Where one extension is shared by unrelated formats, the file header decides. A CRI AWB bank is sent to vgmstream while an AMR-WB `.awb` stays with FFmpeg, and a DSF file only plays when it contains DSD audio.

Game Music Emu, SID and vgmstream files that contain several songs list them under Tracks, using the same controls as a multi-entry playlist. A single song can also be opened directly, or listed in an M3U, by adding `#` and its number to the path, for example `Commando.sid#3`.

The SID and vgmstream demuxers are built into Wasabi's FFmpeg profile from `external/ffmpeg-video`, which holds the demuxer sources and the patch that registers them.

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

Multi-entry Audio M3U content adds Tracks below Bookmarks. Other M3U content adds Episodes, while multi-entry M3U8 channel lists add Channels in the same position. Selecting an entry changes playback without returning to the frontend.

Opening a single local file builds a folder playlist from the other files beside it, using `video_folder_playlist()` in `content/library.c`. Audio only lists audio and video only lists video, entries are sorted naturally so `Episode 2` comes before `Episode 10`, and they appear as Tracks or Episodes. A file holding several songs lists those songs instead, then continues with the next file in the folder.

Auto Play in Advanced, enabled by default, moves on to the next entry when one finishes, with Gapless Playback and Cross Fade applying to audio. Turning it off stops at the end of each entry unless Repeat is set to All.

The playback menu also provides Information and Restart. Restart seeks local content back to its beginning without leaving Wasabi.

Wasabi captures a clean screenshot for each bookmark and when playback exits. These previews are shown in Bookmarks and History.

Saving a bookmark opens the same on-screen keyboard flow used by Pickles save states. A generated name is ready to use or edit, and bookmark rows show the playback time followed by that name.

## Rendering

YUV420 video is uploaded directly to an SDL YUV texture. Other decoded formats use the FFmpeg scaler before upload. Brightness, contrast, saturation, hue and gamma adjustments use FFmpeg's optimised video filters, while scaling, rotation, mirroring, borders, viewport changes, generated overlays and vignettes stay within the presentation path.
