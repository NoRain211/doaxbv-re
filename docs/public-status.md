# Public Status

The public tree proves that the tracked runtime, model, and adapter tests build
without private game input. It does not prove that the game boots, reaches a
menu, renders correctly, or is playable.

The active implementation includes:

- guest memory and register dispatch;
- kernel/device models, checked RAM aliases and fiber-stack recycling;
- controller mapping, scripted input handover and file-backed input pulses;
- transactional saves with interrupted-write recovery;
- owned native PCM/ADPCM audio output and cooperative movie service;
- D3D8 model/adapter seams, render targets, mutable textures and D3D11 VSync flip presentation;
- paired monotonic movie timing, color conversion and native SSE helpers;
- XBE parsing and hashing for authenticated local runners.

Generated game source, private execution receipts, frozen internal reference
code, research archives, and extracted symbol datasets are intentionally not
part of the initial public export.

Progress claims must identify a natural observed event and the exact local
build identity. Public CTest success is regression evidence, not gameplay
progress.

The local runtime has separately passed representative save/reload (#17),
audible output (#15) and movie playback (#19) gates. Movie natural endings
return to title or character selection, and permitted skips repeat. Boot
delivery measured approximately 29.4 and 29.6 fps with stable captured audio
timing. Occasional frame drops, broader rendering gaps and untested offline
routes remain. These local results depend on authenticated generated inputs;
this source export does not bundle those inputs or make the pinned lifter alone
sufficient to reproduce the accepted game run.


The release branch also includes the local portrait, compressed-mipmap and
60 Hz gameplay-pacing changes. A pipeline-cache regression that dropped
character geometry after portrait rendering is fixed: new layouts replace
old cache entries, and evicted layouts rebuild when needed. A native
Exhibition run verified all four selection portraits and character geometry
in a match and closeup. Lighting remains incomplete; this does not establish
all character, outfit, court or offline-mode parity. See the
[verified progress report](https://github.com/NoRain211/doaxbv-re/issues/16#issuecomment-5594275984).

New and older saves default to Digital unless an explicit PC control-mode
preference exists; later Analog and Digital choices are remembered. See [controls](recomp-controls.md) for tested mappings
and limitations. Stop logs distinguish normal exits, diagnostic boundaries,
runtime errors and unhandled host crashes; an expectation-adjusted exit code
alone is not a gameplay result. See [log interpretation](../recomp-runtime/README.md#reading-stop-and-crash-logs).

The latest source update includes reflective item previews, virtual disk/cache
queries, controller routing fixes, and authenticated variable chunk enumeration.
Two natural local store runs restored visor surfaces and lotion previews;
the final guarded candidate passed nine runtime tests. Inventory equip/use,
full lighting fidelity, camera parity and the remaining offline matrix rows
remain unproven. See the [preview evidence](https://github.com/NoRain211/doaxbv-re/issues/28#issuecomment-5647654899)
and [acceptance dashboard](https://github.com/NoRain211/doaxbv-re/issues/16#issuecomment-5647656786).

The pinned rollback dependencies and their seal repair are available as source.
They are not linked into the game runtime; see the
[dependency audit](research/rollback-engine-audit.md) for limitations and checks.

The 0.3 Alpha source and recipe include subsequent inventory action-text,
picture-setting, activity-transition and pool-rendering repairs. Daytime water
shows its floor and reflections in two independent natural runs; one corrected
manual dusk run reaches gameplay and the retry screen with positive feedback.
The dusk run ends normally at its supervision limit, so it does not prove map
return or the complete activity row. Character/platform shading and shader-cache
reuse have bounded evidence. See the [water progress report](https://github.com/NoRain211/doaxbv-re/issues/16#issuecomment-5656165854).
All offline acceptance rows and original rendering parity remain subject to #16.

The current source recipe also preserves the recovered Radio Station entry and
input-history expiry correction. A manual check reached Radio Station. For the
delayed rally-start crouch, two old-build controls reproduced nine episodes after
the action button was released; two corrected matches completed fifteen rallies
without recurrence, retained fresh serving, and returned naturally to the map.
These were muted scripted-controller comparisons using identical initial save
copies. The recipe reproduces their generated program; the exact public source
branch has not had a fresh gameplay run. Physical-controller repeat, other action
outcomes, court variants and full offline acceptance remain open in #16.
