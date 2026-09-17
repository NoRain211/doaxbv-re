# DOAXBV Native PC Port

A work-in-progress native Windows port of **Dead or Alive Xtreme Beach
Volleyball**, built through static recompilation and hand-written runtime code.
You supply your own legally obtained Xbox game copy; the project does not
include the game or its assets.

The goal is a playable PC port with readable source. Generated game code is
the starting point, with game logic gradually replaced by hand-written code.

## Try the Alpha

**0.35 Alpha:** fixes guest-memory exhaustion during extended play, missing
Take a Rest branches, dusk glare and stale rally-start inputs. It also includes
recovered Radio Station entry and an FPS counter with performance logging.
Local testing reports Jungle, Beach, Niki Beach, Private Beach, Take a Rest
and Hopping Game working. Occasional volleyball stalls remain under investigation.
Rebuild in a new folder; preserve your previous installation and saves.

Published downloads are on the
[release page](https://github.com/NoRain211/doaxbv-re/releases/tag/v0.35).
Use the named Alpha ZIP for bundled prerequisites. GitHub's automatic source
archives contain source only; see the source build guide for their prerequisites.

The Alpha requires Windows 10/11 x64, a supported USA game ISO, Git with
internet access, CMake 3.20 or newer, and Visual Studio 2022 Build Tools with
the C++ desktop workload and a Windows SDK.

1. Extract the whole ZIP into a new folder with a short path.
2. Drag your ISO onto `BuildGame.cmd` and wait for **Setup complete**.
3. Open `RunGame.cmd` in the same folder.

Setup extracts your ISO and builds the runner locally. The package includes
build tools and source, but no prebuilt game runner or game files. Follow the
release page and the ZIP's `README.txt` for prerequisites and troubleshooting.
Setup builds an x64 Release runner, matching the current local development build.

See [CHANGELOG.md](CHANGELOG.md) for changes in each release.

## Current progress

Development builds have demonstrated:

- Exhibition character selection and volleyball play, including Xbox Series
  controller input.
- Movie playback, natural endings and supported skips, with native audio.
- Representative purchases, saves and reloads with interrupted-save recovery.
- Restored character portraits and selected store and item previews.
- Pool water with visible floor/reflections, shaded characters and platforms,
  and playable Hopping Game entry at daytime and dusk.
- Working picture controls, remembered control-mode choices, and corrected
  accessory-equip/lotion-use action text.

This is still an experimental port. Lighting, camera behavior, rendering
coverage and offline activities remain incomplete. Development results do not
mean every route works in the Alpha package, which has not passed full gameplay
acceptance. Casino, collection, activity variants and complete rendering fidelity
remain open in the acceptance tracker.

The [offline acceptance tracker](https://github.com/NoRain211/doaxbv-re/issues/16)
records the remaining work. See the
[detailed status](https://github.com/NoRain211/doaxbv-re/blob/main/docs/public-status.md)
and [controller guide](docs/recomp-controls.md) for tested behavior and limits.
New and older saves start in Digital mode unless you have explicitly selected
a PC control-mode preference. Later Analog or Digital choices are remembered.

Planned PC features, including rollback multiplayer, are tracked in the
[extension dashboard](https://github.com/NoRain211/doaxbv-re/issues/20).
The rollback libraries are pinned dependencies, but are not yet integrated
into gameplay.

## Build the public tests

For contributors, the source checkout builds runtime tests without game files.
Install Git, CMake 3.20 or newer, and Visual Studio 2022 or Build Tools with the
C++ desktop workload and a Windows SDK, then run:

```powershell
git clone https://github.com/NoRain211/doaxbv-re.git
cd doaxbv-re
cmake -S recomp-runtime -B build/recomp-runtime -G "Visual Studio 17 2022"
cmake --build build/recomp-runtime --config Release --parallel 2
ctest --test-dir build/recomp-runtime -C Release --output-on-failure
```

These commands produce test executables, not the game runner. The tests use a
hand-written fixture and do not require the submodules. For playing, use the
Alpha package instructions above. For ISO setup from source or work with authenticated generated input,
see the [source build guide](https://github.com/NoRain211/doaxbv-re/blob/main/docs/building.md).
Passing runtime tests does not establish complete game accuracy or compatibility.

## Repository layout

- `recomp-runtime/` - runtime, kernel and input adapters, audio, D3D8 replacements,
  host presentation and tests.
- `xbe/` - XBE parsing and hashing.
- `tools/` - extraction and launcher scripts, lifter submodule, source patches
  and public export checks.
- `docs/` - build guidance, controls, save behavior, status and research.
- `third_party/` - pinned rbengine and recomp-net dependencies; see the
  [dependency notes](third_party/README.md).
- `private/` - ignored local game inputs, generated output and run evidence.

## Contributing and reporting bugs

Read the [contribution guide](https://github.com/NoRain211/doaxbv-re/blob/main/CONTRIBUTING.md)
before submitting changes. Fix translation defects in the lifter and regenerate;
do not patch generated game C.

[Report bugs](https://github.com/NoRain211/doaxbv-re/issues) with your build
version, the steps you took, what happened, hardware/controller details, and the
relevant stop or crash message. See the
[log guide](recomp-runtime/README.md#reading-stop-and-crash-logs).
Remove private paths before sharing logs. Never upload game binaries, generated
game C, assets, extracted filenames, saves, BIOS data or private run evidence.

## LLM use

This project uses large language models and AI coding agents extensively for
code, reverse-engineering analysis, debugging, tests and documentation. Their
output can contain mistakes; changes require review and testing, and unverified
behavior remains tracked as such.

## License

Original project code and documentation are licensed under GPL-3.0-or-later.
See [LICENSE](https://github.com/NoRain211/doaxbv-re/blob/main/LICENSE) and
[NOTICE](https://github.com/NoRain211/doaxbv-re/blob/main/NOTICE).
Third-party components retain their own licenses. This project is not affiliated
with or endorsed by the game's rights holders, and its license grants no rights
to the game or its assets.

If you like my work, please consider buying a coffee on my [Ko-fi](https://ko-fi.com/norainsrecomps).
