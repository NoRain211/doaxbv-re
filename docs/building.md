# Building

## Public test route

The public route requires no game files or generated source:

```powershell
cmake -S recomp-runtime -B build/recomp-runtime
cmake --build build/recomp-runtime --config Debug
ctest --test-dir build/recomp-runtime -C Debug --output-on-failure
```

The test executable links `runtime_public_fixture.c` at the same dispatch seam
used by a generated function. The fixture exercises guest register, memory, and
dispatch behavior; it is not generated game code and proves no game parity.

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

### Configure the runner

Supply generated inputs only with their exact receipt hashes:

```powershell
cmake -S recomp-runtime -B build/recomp-authenticated `
  -DRECOMP_FUNCTION_SOURCE="<local-generated-function.c>" `
  -DRECOMP_FUNCTION_SHA256="<sha256>"
```

Supplying only one value, a mismatched hash, an incomplete program snapshot,
or a wrong manifest fails configuration. The public fixture does not weaken or
replace those authenticated gates.

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
