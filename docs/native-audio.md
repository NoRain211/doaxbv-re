# Native audio

The Windows runtime sends game PCM and decoded Xbox ADPCM to XAudio2.
Submitted buffers own their samples; guest memory can be reused immediately.
Playback tracks play/loop regions, frequency, volume, seek, stop, replacement,
and release. CRI status waits service the existing registered audio worker.

Set `RECOMP_AUDIO_GAIN=1` for full host gain, or a finite value from 0 to 1.
The default is muted. An unavailable output device leaves guest timing active.
The backend uses each source's sample rate; it does not force 22 kHz output.

Before regenerating the game, apply the FSUBP source correction documented in
[`tools/xboxrecomp-patches`](../tools/xboxrecomp-patches/README.md).
It fixes an incorrect x87 destination that corrupted ADX filter coefficients.
Generated source and game data remain private.

Issue [#15](https://github.com/NoRain211/doaxbv-re/issues/15) was accepted after
repeated natural title/menu, activity, pause, and return flows with audible
music and effects. The user confirmed synchronization. General game performance
stalls remain separate work; physical device unplug/reconnect is unproven.

The CMake checks `recomp-runtime-xbox-adpcm`, `recomp-runtime-audio-output`,
and `recomp-runtime-four-cases` cover codec samples and validation, backend
buffer lifetime/error handling, and adapter playback controls respectively.
