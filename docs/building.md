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

Contributors who own the game may generate source locally with a separate
`xboxrecomp` checkout. Keep all inputs and generated output under ignored
directories.

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
