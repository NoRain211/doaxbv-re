# DOAXBV Native PC Port

This project is building a native PC port of Dead or Alive Xtreme Beach
Volleyball from a user-owned Xbox copy. It combines a whole-program static
recompilation scaffold with hand-written runtime, kernel, input, audio, and
D3D8 replacement code.

Local builds have demonstrated Exhibition gameplay, character selection,
movie playback, audio, and representative save/reload flows. This is an
experimental port: lighting, rendering fidelity, and offline-mode coverage
remain incomplete. See [current status](docs/public-status.md) and the
[offline acceptance tracker](https://github.com/NoRain211/doaxbv-re/issues/16).

The public source builds the runtime tests without game files. It does **not
yet provide a complete ISO-to-playable-build workflow**: reproducing the
accepted game build still requires unpublished generation prerequisites.
A [local ISO extraction script](docs/building.md#extract-a-user-owned-iso) is
available; generation and compilation prerequisites remain separate. See
[building instructions and limitations](docs/building.md).

The repository contains no game binary, generated game C, assets, BIOS data,
saves, or private run evidence. Users must supply their own legally obtained
game copy and keep all derived game files outside Git.

## Build the public tests

Requirements:

- Windows 10 or 11
- Visual Studio with the C and C++ toolchain
- CMake 3.20 or newer

Clone with the pinned lifter:

```powershell
git clone --recurse-submodules https://github.com/NoRain211/doaxbv-re.git
```

```powershell
cmake -S recomp-runtime -B build/recomp-runtime
cmake --build build/recomp-runtime --config Debug --parallel 2
ctest --test-dir build/recomp-runtime -C Debug --output-on-failure
```

These tests use a hand-written fixture at the generated-function seam. They do
not require or contain generated game code. See `docs/building.md` for the
authenticated local-input requirements. The tested playable runner is 32-bit
on Windows; public test success does not establish a working 64-bit game build.

For locally built runners, see [controller controls](docs/recomp-controls.md),
[save behavior](docs/recomp-save-contract.md), and
[stop/crash log interpretation](recomp-runtime/README.md#reading-stop-and-crash-logs).

## Repository layout

- `recomp-runtime/` - active runtime, adapters, models, presenters, and tests.
- `xbe/` - XBE parsing and hashing used by local runners.
- `tools/` - pinned upstream lifter and public custody tooling.
- `docs/` - public build guidance and an honest status summary.
- `private/` - ignored local inputs and generated output; only its README is
  tracked.
- `third_party/` - ignored local tool checkouts; only its README is tracked.

The internal frozen host, private evidence ledger, generated snapshots, and
historical research corpus are deliberately absent from the public export.

## Contributing

Read `CONTRIBUTING.md` before opening a change. In particular, never commit
game-derived bytes, generated game C, extracted assets or filenames, or private
run output.

## LLM use

This project has been developed with substantial use of large language models
(LLMs), including AI coding agents, for code, reverse-engineering analysis,
debugging, tests, and documentation. LLM-produced work can contain mistakes;
passing tests does not establish complete game accuracy or compatibility. Any
Alpha release is experimental with known bugs and unverified behavior. See
`docs/public-status.md` and the issue tracker for what has actually been
tested.

## License

Original project code and documentation are licensed under
GPL-3.0-or-later. See `LICENSE` and `NOTICE`. The license does not grant rights
to the game or other third-party material.
