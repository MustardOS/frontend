# Audio

See [`architecture.md`](architecture.md#audio) for the file-by-file breakdown of `audio/`.

- Locking free single produce and consumer sample ring with low/high watermarks.
- Latency profiles (Low / Balanced / Compatible), expressed in device periods.
  - Cores can adjust the audio floor via `SET_MINIMUM_AUDIO_LATENCY` variable.
- Sample rate override (auto or fixed 44100/48000 Hz), volume, underrun fade in, mute during fast forward/slow motion.
  - Cores can also fix their own audio frequency depending on their info file.
- Dynamic rate control trims the stream by up to +/-0.5% to hold the ring at the midpoint of the active profile.
    - Absorbs the drift between the cores declared frame rate and the panels vsync rate, which the emulator loop is
      actually paced by. Without it a core running above the panel rate (i.e. FCEUmm at 60.0988 fps on a 60.00 Hz panel)
      starves the ring within a minute and clicks on every zero fill.
    - Underruns decay to silence over a millisecond and fade back in, so what does slip through is not a hard cut.
