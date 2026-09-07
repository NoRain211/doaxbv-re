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
