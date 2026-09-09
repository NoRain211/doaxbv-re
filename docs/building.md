# Building

## Public test route

Install Git, CMake 3.20 or newer, and Visual Studio 2022 (or Build Tools)
with Desktop development with C++ and a Windows SDK. In PowerShell:

```powershell
git clone --branch v0.0.1 --recurse-submodules https://github.com/NoRain211/doaxbv-re.git
cd doaxbv-re
cmake -S recomp-runtime -B build/recomp-runtime -G "Visual Studio 17 2022"
cmake --build build/recomp-runtime --config Release --parallel 2
ctest --test-dir build/recomp-runtime -C Release --output-on-failure
```

For a downloaded source ZIP, open PowerShell in its extracted root and start
at the CMake command. These commands require no game files and produce tests
in `build/recomp-runtime/Release`, **not a playable game executable**.
The submodule is for lifter work; the public tests do not require it.

The test executable links `runtime_public_fixture.c` at the same dispatch seam
used by a generated function. The fixture exercises guest register, memory, and
dispatch behavior; it is not generated game code and proves no game parity.

## Extract a user-owned ISO

Install Python 3.12 or newer and obtain `extract-xiso` from the
[XboxDev project](https://github.com/XboxDev/extract-xiso). Its source and build
instructions are available there; this repository does not bundle or download
that executable. Add it to PATH or supply its path explicitly:

```powershell
python tools/extract_iso.py "D:/My Games/game.iso" `
  --extractor "C:/Tools/extract-xiso.exe" `
  --output private/imported-disc
```

The script lists the image first, checks its paths, extracts into
`private/imported-disc/disc`, and verifies file names and sizes against the
listing. It uses extraction mode only; it does not rewrite the image or patch
executables. The source image's SHA-256 must remain unchanged. Extraction logs
and a completion receipt stay beside the disc directory, under ignored
`private/`. Keep them private because they contain derived filenames and hashes.

The output directory must be new and beneath this checkout's `private/`;
existing directories and symlink/junction destinations are rejected. Failure
or interruption leaves partial output for inspection, with no completion
receipt. Use a new destination for another attempt. Do not use partial output.

This accepts Xbox images supported by extract-xiso, not arbitrary PC ISO
files, and does not read a physical DVD drive. A successful extraction does
not authenticate a supported game revision or produce the generated game
program. The remaining generation limitations below still apply.

## Authenticated local route

Contributors who own the game may generate source locally with the pinned
`tools/xboxrecomp` submodule. Initialize it with
`git submodule update --init --recursive`. Keep all inputs and generated output
under ignored directories.

### Generation from a user-owned disc

The pinned lifter alone does not reproduce the accepted game build. Accepted
generations used locally developed lifter patches beyond the pinned revision:
the checked-in `tools/xboxrecomp-patches` (FSUBP destination and MMX
completion) are part of that chain, not all of it. An additional locally
developed conditional flag-join change and function-recovery inputs derived
from the same XBE also participate. Function boundaries and recovery data are
locally authored work products derived from the user's own copy; they are not
distributed through this repository.

The end-to-end shape, all through the lifter's supported module entry points
(`tools.xbe_parser`, `tools.disasm`, `tools.func_id`, `tools.recomp`):

1. Parse and hash the user-owned XBE.
2. Disassemble the image and detect function boundaries. Locally authored
   recovery and boundary-correction inputs are applied at this stage.
3. Translate detected functions to C.
4. Validate the generated tree: a manifest of generated files, no gained or
   lost functions against the previous validated tree, and the fail-loud stub
   count.
5. Build with the recorded per-snapshot EBP prologue override count (see the
   configure step below).

A locally reproduced build through this chain has passed the runtime test
suite; it is not claimed to reproduce the accepted executable byte for byte.

### Build a runner from an already-generated program

This requires a complete validated generated program and its build receipt.
A fresh public checkout plus an ISO cannot yet produce those inputs through
the public workflow. The placeholders below are not supplied by ISO extraction.
The accepted game build uses Visual Studio 2019 Build Tools with its C++
workload and Windows SDK, targeting Win32:

```powershell
cmake -S recomp-runtime -B build/recomp-program -G "Visual Studio 16 2019" -A Win32 `
  -DRECOMP_PROGRAM_DIR="<absolute-generated-program-directory>" `
  -DRECOMP_PROGRAM_MANIFEST_SHA256="<generated-manifest-sha256>" `
  -DRECOMP_PROGRAM_EBP_EXPECTED="<receipt-ebp-count>"
cmake --build build/recomp-program --config Release --parallel 2 --target recomp_program_runner
$env:RECOMP_AUDIO_GAIN = "0.2"
./build/recomp-program/Release/recomp_program_runner.exe --xbe "<absolute-extracted-xbe-path>" --vsync
```

Supplying a mismatched hash, an incomplete program snapshot,
or a wrong manifest fails configuration. The public fixture does not weaken or
replace those authenticated gates.

The manifest hash identifies the generated program, not the ISO. Use a new
build directory when changing generator or architecture. Keep the XBE beside
its extracted game files in a writable private directory.

The complete runner additionally needs a locally generated program directory,
its manifest identity, and a user-owned XBE at runtime. None belongs in Git.
The full-program configuration also takes
`-DRECOMP_PROGRAM_EBP_EXPECTED=<n>` with the applied count from the local
generation receipt; the check fails closed when the count differs, and the
default in `recomp-runtime/CMakeLists.txt` documents its receipt-derived
value.

### Run

`recomp_program_runner.exe --xbe <private-xbe>` loads the locally generated
program and the user-owned XBE from disc. Storage is created next to the XBE
under `.recomp-storage`; see `docs/recomp-save-contract.md` for the supported
save behavior. Pass `--vsync` for paced presentation and set
`RECOMP_AUDIO_GAIN` between 0 and 1 for sound; audio is muted by default.
