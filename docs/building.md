# Building

## Build a local runner from a user-owned ISO

The current `tools/build_game.py` workflow extracts a supported user-owned ISO,
checks its XBE, applies `tools/xboxrecomp-patches/local-parity.patch` to revision
`32da23872a552b12b4a932c9d5a6e952bb3f24bb`, generates the game program, and
builds a Win32 Release runner. The authenticated `tools/game-recipe/recipe.json`
provides the original function boundaries, 42 ordered recovery inputs, manual
call targets, and expected generated-file hashes. Setup stops before compiling
if any recipe input or generated file differs. It creates a new `private/setup-*` directory for
each attempt. The receipt, logs, generated program, and runner stay there.

Install these tools before you start:

- Python 3.12 or newer
- Capstone 5.0.9 distribution for that Python installation
- Git with network access
- CMake 3.20 or newer
- Visual Studio 2019 or Build Tools with the C++ desktop workload and a Windows SDK
- XboxDev `extract-xiso`

The repository does not include an all-in-one build executable. Place
`extract-xiso.exe` at `tools/artifacts/extract-xiso.exe`, add it to `PATH`, or
pass its path to `tools/build_game.py`.

The **DOAXBV-0.0.2-Alpha** ZIP bundles Python, Capstone, and the tagged
extractor. Git, CMake, and the Visual Studio 2019 C++ build tools are still
required. Extract the whole ZIP into a new folder with a short path before setup
(for example, a folder directly under Downloads). Use the scripts
from that folder.
The bundled Capstone distribution is 5.0.9; its Python binding reports 5.0.7.
If the extractor reports a missing `VCRUNTIME140.dll`, install the included
`Prerequisites/vc_redist.x86.exe` and retry.

To use the Windows wrapper, drop one supported ISO on `BuildGame.cmd` or run:

```powershell
.\BuildGame.cmd "D:\Games\game.iso"
```

This single command extracts the ISO, generates the program, and compiles the
runner. Output appears live in the setup window and is also saved in each
stage's log under `private/setup-*`. After success, double-click `RunGame.cmd`
in the same folder. Keep that folder in place after setup: build receipts
identify the exact local paths. Setup does not launch the game automatically.

The direct PowerShell route accepts an explicit extractor path:

```powershell
python tools/build_game.py `
  --iso "D:/Games/game.iso" `
  --extractor "C:/Tools/extract-xiso.exe"
```

To reuse a completed extraction, run:

```powershell
python tools/build_game.py --imported private/imported-disc
```

Use `--generate-only` to stop after generation. A completed build writes
`private/setup-*/build-receipt.json` with status `built-unverified`. This status
records a build. A supervised native run provides the behavior evidence.

The recipe reproduces the original local generated program, whose 19-file
manifest is `7426a5d5cf8cf5c5f5628d6c12271fe8e4570c605ae9388adf85b83daf588350`.
All 20 generated files, including unresolved stubs, are checked individually.
The receipt records the recipe, patch, generated manifest, XBE, and runner
identities. A matching generated program does not imply an identical executable:
hand-written runtime changes and compiler inputs also affect the build.

An earlier experimental recipe used the same upstream pin with
`runtime-bootstrap.patch` and fresh function discovery. Its two successful
Exhibition runs did not establish parity with the original local build. The
current builder retains the accepted 0.0.1 recipe unchanged. Gameplay with the new package
still requires validation; the original local runner also has an unresolved
target in a later island-menu flow.

## Public test route

Install Git, CMake 3.20 or newer, and Visual Studio 2022 or Build Tools with
Desktop development with C++ and a Windows SDK. In PowerShell, run:

```powershell
git clone --recurse-submodules https://github.com/NoRain211/doaxbv-re.git
cd doaxbv-re
cmake -S recomp-runtime -B build/recomp-runtime -G "Visual Studio 17 2022"
cmake --build build/recomp-runtime --config Release --parallel 2
ctest --test-dir build/recomp-runtime -C Release --output-on-failure
```

These commands build public tests in `build/recomp-runtime/Release`. They do
not build a playable game runner and do not require game files. The fixture
tests guest memory, registers, and dispatch at the generated-function seam. It
does not contain generated game code or prove game parity.

The named 0.0.2 Alpha ZIP includes the ISO setup workflow and bundled
Python, Capstone and extractor. GitHub's automatic source archives contain
the same project source and recipe but require separately installed tools.
Neither download includes generated game code or a prebuilt game runner.

## Extract a user-owned ISO

`ExtractIso.cmd` extracts one ISO to `private/imported-disc/disc`. Drop one ISO
on the command file or run:

```powershell
.\ExtractIso.cmd "D:\Games\game.iso"
```

For an explicit output directory and extractor, run:

```powershell
python tools/extract_iso.py "D:/Games/game.iso" `
  --extractor "C:/Tools/extract-xiso.exe" `
  --output private/imported-disc
```

The output directory must be new and beneath this checkout's `private/`
directory. The script lists the image, checks its paths, extracts its files,
and verifies names and sizes against the listing. It does not rewrite the image
or patch executables. The source image SHA-256 stays unchanged.

The script rejects an existing output directory and symlink or junction
destinations. An interrupted run leaves partial output without a completion
receipt. Use a new directory for another attempt. Do not use partial output.

This route accepts Xbox images supported by `extract-xiso`. It does not read a
physical DVD drive. Extraction alone does not authenticate the supported game
revision or build a runner.

## Run a generated runner

After `BuildGame.cmd` or `tools/build_game.py` completes, drop its
`build-receipt.json` on `RunGame.cmd` or run:

```powershell
.\RunGame.cmd "private\setup-<id>\build-receipt.json"
```

With no receipt argument, `RunGame.cmd` uses the last completed build recorded
in `private/active-build.json`. A failed build does not replace that selection.
For older builds without a selection, it searches `private/setup-*` for one
successful receipt and refuses to choose when multiple receipts match.
It checks the receipt status, runner file and runner SHA-256,
exactly one XBE, and the recorded XBE SHA-256 before launch. It does not
revalidate every extracted file. It writes the run log under `private/`, uses
VSync, and sets the default audio gain to 0.2. Recipe, lifter patch, generated
manifest, and generation-parity identities are included in each run log when
the build receipt provides them.

`RunGame.cmd` keeps the legacy runner locations at the repository root and
`build/recomp-program/Release`. That route uses the completed
`private/imported-disc` extraction and does not generate a program.

## Legacy: use an existing generated program

Use this route only when you already have a complete generated program and its
receipt. The current ISO workflow performs these steps for you.

```powershell
cmake -S recomp-runtime -B build/recomp-program -G "Visual Studio 16 2019" -A Win32 `
  -DRECOMP_PROGRAM_DIR="<generated-program-directory>" `
  -DRECOMP_PROGRAM_MANIFEST_SHA256="<generated-manifest-sha256>" `
  -DRECOMP_PROGRAM_EBP_EXPECTED="<receipt-ebp-count>"
cmake --build build/recomp-program --config Release --parallel 2 --target recomp_program_runner
.\RunGame.cmd
```

The manifest and EBP values come from the matching build receipt. A mismatched
manifest, incomplete program, or wrong EBP count fails configuration. Keep the
generated program and user-owned XBE under ignored local directories.

## Runtime storage

`recomp_program_runner.exe` loads the generated program and the user-owned XBE.
It creates storage beside the XBE under `.recomp-storage`. See
[`docs/recomp-save-contract.md`](recomp-save-contract.md) for save behavior.
Pass `--vsync` for paced presentation. Set `RECOMP_AUDIO_GAIN` from 0 to 1 for
sound. Audio is muted by default when you start the runner directly.
