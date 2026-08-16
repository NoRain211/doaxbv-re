# Project Agent Rules

## Goal

Produce readable, hand-written source for a native PC port of DOAXBV. The
whole-program recomp is a temporary scaffold: first make the program run using
native kernel, input, audio, and D3D8 replacements, then replace game-owned
generated functions in coherent clusters.

`docs/public-status.md` states what the public tree currently proves.
`docs/building.md` defines the public and authenticated local build routes.

## Architecture

The tracked runtime under `recomp-runtime/` owns guest memory, registers,
dispatch, kernel adapters, device models, D3D8 replacements, and host
presentation. A separately cloned `xboxrecomp` lifts a user-owned XBE into an
ignored local directory.

D3D8 is statically linked into the game image. Replace it at the API level
through `recomp_lookup_manual()`. Keep device models callable as plain
functions, independent of interception. Do not build an NV2A emulator beneath
the game.

## Decomp loop

1. Select a game-owned function that already runs.
2. Read its local generated form beside bounded static analysis.
3. Write clear source with real names, types, and control flow.
4. Register it in `recomp_lookup_manual()` so it wins over generated dispatch.
5. Run the same bounded gate and retain the change only when behavior holds.

Work in data or call clusters. Generated neighbors still depend on fixed guest
addresses and layouts, so redesign a structure only after replacing every
function that owns it.

## Hard rules

1. Never hand-edit generated C. Fix `xboxrecomp` and regenerate locally.
2. Never commit an XBE, generated game C, retail instruction bytes, extracted
   assets or filenames, saves, BIOS data, private paths, or run evidence.
3. Keep private inputs and output under `private/`; only
   `private/README.md` is public.
4. Report observed behavior. Forced state is a hypothesis, not a result.
5. Keep models separable from interception and host delivery details.
6. Replace library code such as D3D8, XAPI, CRT, and middleware wholesale;
   decompile only game-owned logic.

## Progress and verification

A runtime iteration is bounded when its observable completes in under a
minute. It is forward when the program reaches a later natural event. A test,
receipt, refactor, or crash-free run alone is not gameplay progress.

Public-only changes must configure without private inputs and pass the tracked
CTest suite. Changes tested with local generated code must also preserve the
authenticated SHA and manifest failures and report the observed frontier.

After two attempts stop before the same required event, stop varying runtime
runs. Compare both receipts, identify the earliest divergence, state one
falsifiable mechanism, and test the smallest safe change.

## Tool routing

- Translation or regeneration: use a separate checkout of
  `https://github.com/sp00nznet/xboxrecomp` and keep its generated output
  ignored.
- Static addresses, xrefs, or function bounds: use one bounded Ghidra query and
  record the binary identity.
- Real kernel or hardware behavior: use xemu as a live oracle.
- XDK or NV2A semantics: consult public Cxbx, nxdk, or public headers, then
  implement independently.

Do not vendor game-specific `xboxrecomp` forks or generated output into this
repository. General lifter fixes belong in focused upstream pull requests.

## Agent skills

### Issue tracker

Issues, specs, and Wayfinder maps live in GitHub Issues for
`NoRain211/doaxbv-re`. See `docs/agents/issue-tracker.md`.

### Triage labels

Use `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, and
`wontfix`. See `docs/agents/triage-labels.md`.

### Domain docs

This is a single-context repository: use root `CONTEXT.md` and system-wide ADRs
under `docs/adr/`. See `docs/agents/domain.md`.

## Governance

Changes to this file must keep the custody rules at least as strict. Propose
any self-initiated governance change before editing it.
