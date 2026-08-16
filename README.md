# DOAXBV Native PC Port

This project is building a native PC port of Dead or Alive Xtreme Beach
Volleyball from a user-owned Xbox copy. It combines a whole-program static
recompilation scaffold with hand-written runtime, kernel, input, audio, and
D3D8 replacement code.

The project is not playable yet. The public repository contains no game
binary, generated game C, assets, BIOS data, saves, or private run evidence.
Each contributor must supply and keep their own legally obtained inputs
outside Git.

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
cmake --build build/recomp-runtime --config Debug
ctest --test-dir build/recomp-runtime -C Debug --output-on-failure
```

These tests use a hand-written fixture at the generated-function seam. They do
not require or contain generated game code. See `docs/building.md` for the
authenticated local-input workflow.

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

## License

Original project code and documentation are licensed under
GPL-3.0-or-later. See `LICENSE` and `NOTICE`. The license does not grant rights
to the game or other third-party material.
