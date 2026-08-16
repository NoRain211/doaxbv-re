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
