# Original Xbox custom soundtracks

## APIs and title integration

The pinned [RXDK-Libs soundtrack implementation](https://github.com/Team-Resurgent/RXDK-Libs/blob/016a682dab90d03176c300874032d795bcb01621/libs/libxapi/k32/xsndtrk.c) implements `XFindFirstSoundtrack` / `XFindNextSoundtrack` enumeration, indexed `XGetSoundtrackSongInfo`, and `XOpenSoundtrackSong`. Enumeration returns soundtrack name, ID, song count, and total length; song-info returns a song ID, duration, and optional name. Opening a song returns a WMA file handle. Its async flag selects unbuffered reads; the other mode uses synchronous reads. The source marks itself as reworked/modified and retains some Microsoft XDK-authored portions, so treat it as public implementation evidence, not an untouched official SDK release. A [Microsoft-authored Xbox ATG talk](https://www.slideserve.com/wilson/xbox-launch-lessons-learned) independently shows `XFindNextSoundtrack` and identifies user-created soundtracks as a platform feature. FMOD's [Xbox changelog](https://github.com/g-truc/shooter/blob/master/external/fmod-3.75/documentation/Revision.txt) says `FSOUND_Stream_OpenFromHandle` was added to play a dashboard track after `XOpenSoundtrackSong`, evidencing middleware playback from that handle.

## Database, files, and audio

RXDK-Libs opens `E:\TDATA\FFFE0000\MUSIC\ST.DB`; its [public Xbox header](https://github.com/Team-Resurgent/RXDK-Libs/blob/016a682dab90d03176c300874032d795bcb01621/libs/libxapi/support/inc/xboxp.h) models 512-byte database pages, up to 100 soundtracks and 500 songs per soundtrack, with song metadata batched six entries per list. Its song opener derives a soundtrack directory from the song ID's high word and opens a `.WMA` file. These describe the implementation's model and behavior.

The community [XboxDevWiki format notes](https://xboxdevwiki.net/Soundtracks), explicitly based on decompiling the dashboard's `StDB.dll`, report `E:\TDATA\fffe0000\music`, a little-endian `ST.DB`, six-song groups, and decimal-padded folder/file names. They also report the dashboard's default encoding as WMA 8, stereo, 44.1 kHz, 128 kbps, 16-bit. Those are reverse-engineered findings, not an official Microsoft specification. The reported decimal naming conflicts with RXDK-Libs' hexadecimal path construction; the inspected local target agrees with RXDK-Libs on hexadecimal ID formatting, but the exact database record layout still needs validation against a known database.

## Fit to this recomp

`recomp-runtime/kernel_file.c` already translates guest hard-disk partition 1
into `.recomp-storage/partition1` and serves guest file opens, reads, and
directory queries. The inspected local target's soundtrack lookup uses that
file path flow for its database and WMA tracks, so no soundtrack-specific
kernel import is needed. This is a bounded Ghidra observation of the local
target; no game addresses or extracted strings are included here.

A known-good Xbox soundtrack database and WMA tree remain useful as a
baseline probe through the existing partition-1 backing storage. Keep all
music and derived data under `private/`. This probe does not meet the user's
requested MP3, WAV, and FLAC support: a folder alias alone cannot make those
files satisfy the title's database and WMA expectations.

## Recommended MP3, WAV, and FLAC support

The user-facing design is a `UserMusic` folder in the private local game root:
drop in MP3, WAV, or FLAC files, then select them through the game's custom
soundtrack menu. Start with one soundtrack containing the supported files;
use filenames for display names and stable IDs for selection across restarts.
The runtime supplies the Xbox-facing catalog; users should not need an Xbox
dashboard database or manually converted tracks. This is a proposal, not
implemented or verified behavior.

Prefer host decoding to PCM over converting every input to lossy WMA. Windows'
[desktop codec table](https://learn.microsoft.com/en-us/windows/apps/develop/media-authoring-processing/supported-codecs)
lists MP3, FLAC, and LPCM WAV support. Media Foundation's
[Source Reader tutorial](https://learn.microsoft.com/en-us/windows/win32/medfound/tutorial--decoding-audio)
demonstrates requesting decoded PCM. These make the built-in Windows decoder
the first candidate, subject to a local decode/seek probe for all three formats;
the codec table alone does not prove this recomp's integration. WAV is a
container: include ordinary PCM and float WAV in the probe, and report
unsupported encodings or malformed files without breaking the whole library.

Two connections must be proven before choosing implementation details:

1. Supply soundtrack enumeration, names, IDs, and durations in the form the
   existing menu consumes. If this uses generated `ST.DB` data, validate its
   layout first; otherwise replace the relevant XAPI library boundary with an
   independent implementation. Do not copy XDK-derived implementation code.
2. Trace the selected song through decoder creation, packet delivery, seek,
   completion, and release. Replace the relevant library decoder boundary so
   host PCM participates in the existing game-controlled audio lifecycle.
   Renaming an MP3 to WMA or returning PCM from raw WMA file reads is insufficient.
   Keep decoding separate from interception and reuse the existing PCM output.

Read original music without modifying it. Decode incrementally rather than
loading entire tracks. The current `audio_output.h` accepts mono/stereo PCM
with unsigned 8-bit or signed 16-bit samples, so high-resolution FLAC/WAV needs
format conversion at that boundary; do not claim bit-perfect playback.

An importer that builds a compatible WMA library is a fallback if the native
playback boundary proves impractical. It adds import time, cached files, and
lossy conversion, including another encode for MP3. Microsoft's
[WMA encoder documentation](https://learn.microsoft.com/en-us/windows/win32/medfound/windowsmediaaudioencoder)
establishes encoding support, but Xbox-compatible output still needs a real
playback check. It is not the preferred user workflow.

Acceptance requires natural in-game selection and audible playback of each
format, plus stop, next, seek where exposed, end-of-track advancement, volume,
and return to ordinary game music. Include a malformed file and confirm the
original files remain unchanged. No runtime implementation or playback test
has been performed for this proposal.

## Existing WMA path remains unverified

The handwritten `recomp-runtime/dsound_service_adapter.c` output path accepts
PCM and Xbox ADPCM; that alone does not prove the title's WMA decode reaches
XAudio2. First verify one real WMA track through the title's existing decoder
and output path. If that fails, Windows Media Foundation documents WMA decode
support and a Source Reader route to PCM ([supported formats](https://learn.microsoft.com/en-us/windows/win32/medfound/supported-media-formats-in-media-foundation),
[audio decoding tutorial](https://learn.microsoft.com/en-us/windows/win32/medfound/tutorial--decoding-audio)); only then choose the narrow host-side fallback.
Keep the database and WMA inputs user-provided and out of the public tree.
