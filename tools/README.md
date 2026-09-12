# Tools

## xboxrecomp

`xboxrecomp` is pinned as a submodule at `tools/xboxrecomp`. Clone this
repository with `--recurse-submodules`, or initialize it afterward:

```powershell
git submodule update --init --recursive
```

Use its supported XBE parsing, disassembly, function-identification, and
recompilation entry points. Keep every user-owned input and all generated
output under ignored local directories. General lifter fixes belong upstream;
do not commit a game-specific fork here.

The upstream repository also contains experimental runtime code. That code is
not part of this port's architecture: the tracked runtime remains
`recomp-runtime/`, D3D8 is replaced at the API level, and no NV2A emulator may
be placed beneath the game.

## Public export verifier

`public_export.py` enforces the tracked public allowlist, scans regular files,
and verifies each declared submodule's path, URL, and pinned commit.

## Deliberately absent

The private repository contains historical frontier runners, provenance locks,
candidate-specific leaf gates, and an NV097 capsule raster tool. They embed
game-specific evidence or obsolete workflow assumptions and are not public
tooling. New import or analysis tools must expose a game-independent interface
and keep all derived game data local.

## Build from an ISO

`BuildGame.cmd` runs `build_game.py` to extract a supported ISO, reproduce the
accepted generation recipe and compile a Win32 Release runner. The recipe reproduces
the current proven local generated program, including its x87 destination
correction, with the original function boundaries and recoveries. Generated files
must match their recorded hashes before compilation. See the
[build guide](../docs/building.md#build-a-local-runner-from-a-user-owned-iso).
`RunGame.cmd` uses the successful build receipt and verifies the selected
runner and game executable before launch.

## ISO extraction

Windows users can drop one ISO onto the root `ExtractIso.cmd` launcher.
Python 3.12 or newer is required. The launcher finds an extractor in
`tools/artifacts/extract-xiso.exe` or on PATH and extracts to
`private/imported-disc/disc`, refusing existing output. `RunGame.cmd` launches
a separately built runner against that completed extraction.

`extract_iso.py` wraps a separately installed XboxDev `extract-xiso` executable
in extraction mode. See [usage and boundaries](../docs/building.md#extract-a-user-owned-iso).
`test_extract_iso.py` uses synthetic listings and can additionally exercise a
real extractor when `EXTRACT_XISO_TEST_TOOL` names its executable. Neither tool
contains game data or replaces the remaining local generation pipeline.
