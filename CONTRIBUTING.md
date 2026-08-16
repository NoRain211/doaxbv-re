# Contributing

Thank you for helping build a readable native PC port.

## Before changing code

1. Read `AGENTS.md` and `docs/public-status.md`.
2. Build and run the public test suite from `docs/building.md`.
3. Keep one concern per branch and commit.

## Custody

Contributions must contain only source and documentation that you have the
right to submit. Never commit:

- game executables, generated game C, instruction-byte windows, or assets;
- extracted game filenames, saves, BIOS or EEPROM data;
- private paths, traces, screenshots, or run receipts;
- proprietary or leaked SDK material;
- code copied from xemu, Cxbx, nxdk, or another project without preserving and
  satisfying its license.

Keep local inputs under `private/`. The pinned upstream `xboxrecomp` checkout
lives at `tools/xboxrecomp`. General lifter fixes should be sent upstream
separately; do not add a game-specific lifter fork here.

## Verification

Run:

```powershell
python tools/public_export.py verify --require-public-tree
cmake -S recomp-runtime -B build/recomp-runtime
cmake --build build/recomp-runtime --config Debug
ctest --test-dir build/recomp-runtime -C Debug --output-on-failure
```

Describe the exact checks and results in the pull request. If private local
inputs were used, report only safe metadata and observed behavior.

## Licensing

By submitting a contribution, you agree that it may be distributed under
GPL-3.0-or-later. Identify any third-party material and its license in the pull
request.
