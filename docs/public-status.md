# Public Status

The public tree proves that the tracked runtime, model, and adapter tests build
without private game input. It does not prove that the game boots, reaches a
menu, renders correctly, or is playable.

The active implementation includes:

- guest memory and register dispatch;
- kernel and device model scaffolding;
- input, audio-service, and filesystem adapters;
- logical D3D8 model and adapter seams;
- D3D11 presentation scaffolding;
- XBE parsing and hashing for authenticated local runners.

Generated game source, private execution receipts, frozen internal reference
code, research archives, and extracted symbol datasets are intentionally not
part of the initial public export.

Progress claims must identify a natural observed event and the exact local
build identity. Public CTest success is regression evidence, not gameplay
progress.
