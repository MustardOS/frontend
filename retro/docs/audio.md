# Audio

See [`architecture.md`](architecture.md#audio) for the file-by-file breakdown of `audio/`.

- Locking free single produce and consumer sample ring with low/high watermarks.
- Latency profiles (Low / Balanced / Compatible), expressed in device periods.
  - Cores can adjust the audio floor via `SET_MINIMUM_AUDIO_LATENCY` variable.
- Sample rate override (auto or fixed 44100/48000 Hz), volume, underrun fade in, mute during fast forward/slow motion.
  - Cores can also fix their own audio frequency depending on their info file.
