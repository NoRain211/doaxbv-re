# Engine rollback audit

Audited 2026-09-11. Scope: adopt the two requested dependencies and trace the
psxrecomp engine responsibilities that DOAXBV needs for #24 and #25. This is a
source audit with dependency tests and one live suspension trace, not a live
rollback result or an exhaustive
review of psxrecomp's media/network stack.

## Decision and source identity

Use `rbengine` for host policy/storage helpers and `recomp-net` for transport,
input correction, and episode state. Both are now pinned submodules under
`third_party/`. They are not yet linked into the native executable: the safe
simulation/restore seam and Xbox input representation still need implementation.

| Source | Audited revision | Role |
|---|---|---|
| [rbengine](https://github.com/RetroPortingToolKit/rbengine/tree/a7b98507a62fe00e5aec3b90c52a4134f3c174bc) | `a7b98507a62fe00e5aec3b90c52a4134f3c174bc` | Added submodule |
| [recomp-net](https://github.com/RetroPortingToolKit/recomp-net/tree/859ba28b93f7374f2ac1eb70d2171594c5cd4f4b) | `859ba28b93f7374f2ac1eb70d2171594c5cd4f4b` | Added submodule |
| [psxrecomp](https://github.com/RetroPortingToolKit/psxrecomp/tree/0e0644631094805071740e51dc398c49c2ed869e) | `0e0644631094805071740e51dc398c49c2ed869e` | Read-only reference |

psxrecomp pins the same rbengine revision and recomp-net
`bfa20b403a6d71d01685e8e0a8100d63026716bc`. The diff from that recomp-net pin to
our pin changes authentication, chat reporting, documentation, and CMake;
rollback/input/protocol implementation files are unchanged. DOAXBV observations
below refer to the existing working tree based on
`039a2805b6c30849b00cfa904b9602c942bed140`, including its uncommitted runtime work.

The dependency root licenses are [MIT for rbengine](https://github.com/RetroPortingToolKit/rbengine/blob/a7b98507a62fe00e5aec3b90c52a4134f3c174bc/LICENSE)
and [MIT for recomp-net](https://github.com/RetroPortingToolKit/recomp-net/blob/859ba28b93f7374f2ac1eb70d2171594c5cd4f4b/LICENSE).
psxrecomp's [root license is PolyForm Noncommercial](https://github.com/RetroPortingToolKit/psxrecomp/blob/0e0644631094805071740e51dc398c49c2ed869e/LICENSE).
No psxrecomp implementation was copied into this repository; use it to identify
requirements and implement the Xbox-specific behavior independently.

## What is already reusable

| Piece | Reuse and remaining engine responsibility |
|---|---|
| `RbeSnapRing` | Reuse blob ownership, tick lookup, replacement, and eviction. Supply an actual DOAXBV serializer and safe restore. The ring cannot capture native execution. |
| `RbeHashConfirm` / `rbe_hc_*` | Reuse hash-pair bookkeeping. Define canonical authoritative state and invalidate speculative hashes on rewind. |
| `RbeInputHist` / `rbe_ih_*` | Reuse the history/prediction behavior only after resolving the limited rollback row format below. |
| `RbeSchedBridge` / `rbe_sched_*` | Reuse admit pacing, prediction grace, and delay adjustment. Bind session state, wall-clock pacing, and episode gates; keep guest simulation time separate. |
| `RNetSession` / `RNetRbSession` | Reuse transport and episode APIs. The engine must pump messages, apply snapshots, publish sealed inputs, run frames, compare digests, and handle failure. |

[rbengine's public interfaces](https://github.com/RetroPortingToolKit/rbengine/tree/a7b98507a62fe00e5aec3b90c52a4134f3c174bc/include/retcomm_rbengine)
own these portable pieces. psxrecomp's [scheduler bridge](https://github.com/RetroPortingToolKit/psxrecomp/blob/0e0644631094805071740e51dc398c49c2ed869e/runtime/src/psx_netplay_sched.c#L134)
and [snapshot glue](https://github.com/RetroPortingToolKit/psxrecomp/blob/0e0644631094805071740e51dc398c49c2ed869e/runtime/src/netplay_snap_ring.c#L34)
show the remaining engine boundary. Do not duplicate their compatibility shims.

## Findings and integration requirements

### 1. Confirmed dependency defect: initial seals can exceed their completion mask

**[verified: executable probe; high priority before session integration]**
`rnet_rb_seal_inputs` accepts an initial span up to the default 128 rows, while
peer completion uses a 64-bit mask. With a 65-row span covering ticks 100–164,
receiving only 100–163 makes `rnet_rb_all_peer_seal_rows_complete` return true;
`rnet_rb_seat_row_authoritative(..., 164)` still returns false.

The [initial seal path](https://github.com/RetroPortingToolKit/recomp-net/blob/859ba28b93f7374f2ac1eb70d2171594c5cd4f4b/src/rollback/rnet_rollback.c#L507)
caps only against `seal_max_span`. The [completion check](https://github.com/RetroPortingToolKit/recomp-net/blob/859ba28b93f7374f2ac1eb70d2171594c5cd4f4b/src/rollback/rnet_rollback.c#L674)
uses all 64 bits for every span of 64 or more. Some row-application paths also
shift by the offset without a `< 64` guard. Tip extension already rejects a
span beyond 64; initial sealing does not enforce that same bound.

Keep DOAXBV episodes at **at most 64 sealed rows**, including any baseline row,
and validate the actual sealed target before replay. Reject or split oversized
requests rather than silently pretending the original target was covered.
The supplied [regression probe](../../tools/check-rollback-seal-span.c) fails on
the pinned source. This demonstrates a library inconsistency, not an observed
DOAXBV desync or proof that psxrecomp reaches this span in play.

**Repair applied locally:** the [source patch](../../tools/recomp-net-patches/README.md)
sets capacity to 64, rejects excessive/reversed ranges without shortening the
correction target, checks distance before inclusive addition, and handles a
valid span ending at `UINT32_MAX`. Re-signing visits at most the sealed rows;
resealing clears old peer credit. Missing, predicted, or invalid remote history
cannot overwrite previously confirmed rows. The gitlink now records the repair
in commit `694e493`, based on the audited published revision above.
The commit is published in [our fork](https://github.com/NoRain211/recomp-net/commit/694e493460740ceb7f6e0171bad6ca484c57f631); the source patch remains available for the
published base and must not be reapplied to the repaired commit.

### 2. The existing episode test has a stale fixture

**[verified: upstream test failure and isolated fixture experiment]**
MSVC Debug `rollback_episode_test` fails four assertions:

```text
FAIL: peer rows not complete yet
FAIL: invalid rows do not complete seal
FAIL: slot 2 incomplete (no rows)
FAIL: extend clears peer completeness for new offsets
```

Its [host fixture](https://github.com/RetroPortingToolKit/recomp-net/blob/859ba28b93f7374f2ac1eb70d2171594c5cd4f4b/tests/rollback_episode_test.c#L65)
supplies valid, non-predicted rows for every slot. The newer
[history pre-sealing path](https://github.com/RetroPortingToolKit/recomp-net/blob/859ba28b93f7374f2ac1eb70d2171594c5cd4f4b/src/rollback/rnet_rollback.c#L301)
correctly treats those rows as received remote authority. In a private copy,
changing only the fixture's prediction flag to `slot == 0 ? 0u : 1u` makes the
episode test pass with GCC. The local patch now initializes the full fixture
row, defaults remote history to predicted, and separately checks confirmed,
missing, and invalid history. Its capacity and authority regressions pass.

### 3. The rollback pad format loses Xbox controls

**[verified: source; integration blocker]** The ordinary session input payload
supports [32 opaque bytes](https://github.com/RetroPortingToolKit/recomp-net/blob/859ba28b93f7374f2ac1eb70d2171594c5cd4f4b/include/recomp_net/config.h#L11),
but [rollback rows](https://github.com/RetroPortingToolKit/recomp-net/blob/859ba28b93f7374f2ac1eb70d2171594c5cd4f4b/include/recomp_net/rollback.h#L101)
contain only 16 button bits, two signed 8-bit axes, and an analog-mode flag.
The [seal wire codec](https://github.com/RetroPortingToolKit/recomp-net/blob/859ba28b93f7374f2ac1eb70d2171594c5cd4f4b/src/protocol/rnet_protocol.c#L366)
has the same limitation. psxrecomp's conversion drops its right stick too.

DOAXBV's [input model](../../recomp-runtime/input_model.h) carries eight pressure
bytes and four signed 16-bit axes in addition to digital buttons. The explicit
18-byte gamepad representation fits the ordinary payload, but cannot round-trip
through rollback history/seals as currently defined. Also, rbengine's idle
prediction uses `buttons = 0xffff`; Xbox's existing input model uses zero for
released buttons. A direct struct/bitfield copy would invent pressed buttons.

Extend the shared rollback payload/codec and correction comparison to preserve
the selected Xbox contract, with round-trip and late-pressure-input checks.
Alternatively, a reduced match-input format needs evidence that every omitted
value is irrelevant; that evidence does not exist here. Do not inherit stick
deadband promotions as proof of exact deterministic replay.

### 4. Restore must replace execution, not just memory

**[verified: source; integration blocker]** psxrecomp's
[`host_load_state`](https://github.com/RetroPortingToolKit/psxrecomp/blob/0e0644631094805071740e51dc398c49c2ed869e/runtime/src/psx_netplay_rb.c#L2827)
queues a load; `host_advance_sim` is a no-op. The engine applies the snapshot,
resynchronizes device deadlines, then [resumes at a safe scheduler boundary](https://github.com/RetroPortingToolKit/psxrecomp/blob/0e0644631094805071740e51dc398c49c2ed869e/runtime/src/psx_netplay_rb.c#L6840).
Its frame loop avoids jumping across C++ destructors. Copying the callback
table without that loop would not replay anything.

DOAXBV's [fiber adapter](../../recomp-runtime/fiber_adapter.c) uses
`CreateFiberEx`/`SwitchToFiber`, retaining native call stacks and generated C
locals. [Runtime state](../../recomp-runtime/runtime.h) and
[per-fiber state](../../recomp-runtime/fiber_model.h) contain registers and FP
state, but copying these plus RAM does not rewind native continuations.
The [swap adapter](../../recomp-runtime/d3d_frame_adapter.c) is a useful place
to observe frames, not yet a proven restartable simulation boundary.

Next: trace one real rally update and its fiber switches, identify a boundary
where execution can return and restart from explicit state, and prove two-frame
offline replay. If that requires replacing a game-owned update cluster or
changing lifter continuations, do that work at its owner; do not patch generated C.

**Static follow-up:** bounded Ghidra queries against the original image confirm
that the main loop polls four ports before an ordered 17-task scheduler. The
shared task entry calls its callback once and marks the task dead when it
returns; yields occur inside callbacks and sometimes nested finite routines.
Several persistent workers have simple outer update/yield loops, but replacing
only the main loop or scheduler would still leave suspended native continuations.

The next candidate is an explicit continuation for each measured active worker,
admitting a snapshot only when every child is at a supported outer yield.
Nested/unknown yields and task-topology changes must reject that snapshot.
This is a hypothesis, not an implemented restore path.

**Live follow-up:** an isolated muted run captured **78 switches across three
complete post-serve presentation intervals**, with 13 active child tasks in the
same order. Its trigger additionally required post-hit ready/ball state; phase 3
alone also includes the serve toss. Presentation intervals 2085–2087 covered
updates 870–873. Play continued to update 907 with a new native court frame,
then exited normally (45.891 seconds total). The source profile, title data and
cache were unchanged; one file in the independent writable profile changed
during the natural route. This was not a speculative replay or a save-rollback
test.

The shared dispatch history was truncated and contained other fibers' calls.
The private diagnostic therefore also recorded the exact source call site
immediately before each SwitchToFiber dispatch. Every site was checked against
its generated source. A call-site receipt identifies the suspension instruction;
it does not serialize the native callers or prove their local state is recoverable.

All 13 observed child sites map to their callback's outer loop yield. This
supports a concrete continuation set for the sampled configuration, while the
previously identified conditional nested waits still require rejection or
support in other states. The gameplay update counter advances inside the third
scheduled task, after two other workers have already run. Use a checkpoint
between complete scheduler sweeps; that counter's edge is not a global frame
boundary. The scheduler itself is a finite sweep and need not be duplicated
just to remove the child loops' native continuations.

The smallest measured replacement cluster is those 13 child-loop wrappers plus
an owned main-loop boundary and managed fiber execution. Keep finite update
bodies; separate their one-time initialization from each resumed step. The
existing shared trampoline treats callback return as task termination, so
one-step returns must have a different execution path. Several wrapper-local
EBP values are recoverable at these outer yields because they are published
before the switch; this does not resolve snapshots taken at nested yields.
Complete host-state restoration, replayable clocks/inputs and output/save
suppression still precede the requested two-frame replay gate.

The trace candidate was
`8743cebfc84697e97d04bb8b1c18acb78be2b94afcf098acd2fdb770b5386957`, based on
r360 candidate
`f5cc96da7dec57ca34aa9ec422b06ba022de9332df01db0ce4a97e485b534c5d` and generated
manifest `7b26fb53628647ae38229075896be06f77988146e6a8ae66efed3d296984b396`.
Its isolated build reused 18 copied, authenticated r360 generated objects and
rebuilt the host sources with the diagnostic. Shared runtime/build sources and
generated C were not edited by this work. Private receipts and source mapping
are under `private/research/rollback-runtime-boundary/` and the `rbtrace-*`
evidence in the private acceptance archive.

### 5. Snapshot and digest coverage must be Xbox-specific

**[verified: source; completeness remains unproven]** psxrecomp serializes CPU,
RAM, scratchpad, clocks, interrupts, timers, GPU/VRAM, SPU/RAM, CD, DMA, SIO,
MDEC, and dispatch/cache metadata in [boot state](https://github.com/RetroPortingToolKit/psxrecomp/blob/0e0644631094805071740e51dc398c49c2ed869e/runtime/src/boot_state.c#L358).
It pins baselines separately so live/resimulation snapshots cannot overwrite
the recovery state. Reuse that invariant, not those PSX device implementations.

Inventory DOAXBV's RAM, per-fiber FP/register/continuation state, allocator and
kernel objects, input state, and every guest-visible adapter value. Host pointers
are not portable snapshot data. [XAPI counters](../../recomp-runtime/xapi_time_adapter.cpp),
[kernel system time](../../recomp-runtime/kernel_thread.c), and
[audio cursors](../../recomp-runtime/dsound_service_adapter.c) currently derive
values from host time; [vblank pacing](../../recomp-runtime/d3d_vblank.cpp) also
uses wall time. Determine which reads affect the match and make those readings
replayable. Preserve pure host pacing separately.

psxrecomp's current baseline gate checks core, AV, and extended device digests;
its [POST comparison](https://github.com/RetroPortingToolKit/psxrecomp/blob/0e0644631094805071740e51dc398c49c2ed869e/runtime/src/psx_netplay_rb.c#L4680)
checks core plus AV while logging several other partitions. Thus neither its
older core-only notes nor a matched POST establishes complete Xbox state.
Define normalized authoritative partitions and compare each replayed tick;
omit a value only after proving it cannot affect subsequent simulation.
Keep the existing D3D API replacement architecture; this does not call for a GPU emulator.

The initial host-state inventory identifies concrete owners for that work:

| Owner | State outside guest RAM that needs a decision |
|---|---|
| `fiber_adapter.c`, `fiber_model.h` | Guest register/FP contexts, active handles, stack allocation metadata, and explicit continuations. Native fiber pointers cannot be serialized. |
| `program_adapters.c`, `kernel_memory.c` | Heap/contiguous cursors, thread IDs/handle counter, and allocation records. Native `jmp_buf` is execution context, not snapshot data. |
| `kernel_thread.c` | Event table, handle counter, priority and suspend count; system-time reads need replay semantics. |
| `kernel_interrupt.c` | Pending DPCs and arguments, IRQL, interrupt records and drain state. A snapshot inside a running deferred callback also has a continuation to account for. |
| `input_adapter.c`, `input_model.h` | Connection/open masks, packet numbers, complete gamepads and feedback. Preserve tick input independently of host sampling callbacks. |
| `kernel_file.c` | Guest handle IDs, cursor/directory state, symbolic links and save ownership. Host handles need retained/reopened resources; speculative writes need a commit boundary. |
| `cri_service_adapter.c`, `dsound_service_adapter.c` | Service/reentrancy state, voice models and guest-visible buffer clocks. Keep output cursors/submission separate from simulation state. |
| D3D creation/draw/render/frame adapters | Device models, render/draw state and swap count; rebind presenter resources and suppress intermediate replay output. |

These are source-confirmed owners, not proof of complete snapshot coverage.
Before implementation, trace each value's readers and classify telemetry or
derived host resources only where excluding them cannot alter later simulation.

Snapshot cost needs measurement. The runner maps 64 MiB of RAM: forty full RAM
copies alone would consume 2.5 GiB, before host state or a pinned baseline.
That is arithmetic, not measured allocation. Start with the small window needed
for the two-frame gate, measure capture/load/replay, and size ring depth in
stored snapshots rather than assuming it equals ticks.

### 6. Replay side effects need their own boundary

**[verified: source; DOAXBV policy still to implement]** psxrecomp
[skips its host audio update during replay](https://github.com/RetroPortingToolKit/psxrecomp/blob/0e0644631094805071740e51dc398c49c2ed869e/runtime/src/main.cpp#L6864),
but [may display replay progress](https://github.com/RetroPortingToolKit/psxrecomp/blob/0e0644631094805071740e51dc398c49c2ed869e/runtime/src/main.cpp#L7084).
Its [SIO write path](https://github.com/RetroPortingToolKit/psxrecomp/blob/0e0644631094805071740e51dc398c49c2ed869e/runtime/src/sio.c#L1837)
calls `memcard_flush` immediately; the [flush function](https://github.com/RetroPortingToolKit/psxrecomp/blob/0e0644631094805071740e51dc398c49c2ed869e/runtime/src/memcard.c#L243)
has no confirmed-tick check. Card sandboxing does not make this a general
speculative-write commit mechanism. A save occurring in that path is a source
risk, not a reproduced psxrecomp data-loss result.

DOAXBV #24 requires no intermediate catch-up display, duplicated audible
effects, or speculative saves/rewards. Keep guest-visible audio/device state
advancing deterministically, gate output submission, and retain the last
presented frame until catch-up completes. Defer persistent effects until their
simulation is confirmed. The existing profile transaction journal provides
crash recovery, not simulation rollback.

### 7. Keep the engine protocol invariants, skip the MotK-specific recovery policy

**[verified: source; implementation guidance]** The engine's
[episode pump](https://github.com/RetroPortingToolKit/psxrecomp/blob/0e0644631094805071740e51dc398c49c2ed869e/runtime/src/psx_netplay_rb.c#L8762)
handles seals arriving before BEGIN, deferred baseline application, retransmits,
epoch/target filtering, and timeouts. Its
[POST filter](https://github.com/RetroPortingToolKit/psxrecomp/blob/0e0644631094805071740e51dc398c49c2ed869e/runtime/src/psx_netplay_rb.c#L9198)
rejects stale target ticks after tip extension. Preserve these ordering and
identity checks in the eventual Xbox session bridge. Require authoritative rows
for every replayed tick and a retained, agreed baseline before starting it.

Use one session owner thread; rbengine's scheduler bridge/state is global.
Freeze published inputs per simulation tick; DOAXBV currently samples its
single [input source](../../recomp-runtime/input_adapter.h) inside every
`XInputGetState` interception, without a port argument. Independent player
routing and tick latching must precede networking. Use `rbe_sched_wire_for_sim`
for consumption: its default consumes T and samples into T+D; applying the
generic T+D helper to consumption would erase the intended delay cushion.

Do not transplant MotK menu-release heuristics, FMV keyframe healing, title
addresses, or media-specific delay tuning. Keep lobby/selection/results outside
the rollback domain. First require identical build/content/mod identity,
bounded correction, and an explicit failed-session outcome for unrecoverable
divergence, as [#25](https://github.com/NoRain211/doaxbv-re/issues/25) requires.

## Verification and next implementation slice

- MSVC 19.44, x64, Debug, ICE off: both libraries build. Upstream warnings include
  local-variable shadowing and `rnet_chat_report.c`'s uninitialized const declaration.
- rbengine: **5/5** tests pass.
- Unpatched recomp-net: **4/5** selected tests pass (`ring_admit`, `wire_map`,
  `input_contract`, `rb_wire`); `rollback_episode` has the four failures above.
  The 65-row probe fails with `seal_span=65 complete=1 last_authoritative=0`.
- Patched recomp-net: **5/5** selected tests pass with MSVC Debug. GCC episode
  and standalone rejection checks pass. The expanded episode test covers
  capacities 0/1/63/64/65/128/`UINT32_MAX`, missing final rows, extension,
  resealing, overflow, and confirmed-history preservation.
- Isolated r360 host build: both focused runtime/fiber checks pass; trace
  trigger and supervisor checks pass. The one muted natural-rally capture
  completed with matching input receipts and a normal exit, as detailed above.
- No rewind/restore, PSX game run, two-process correction, ICE/relay validation,
  or cross-machine determinism claim was made. No player-visible milestone
  changed; GitHub sync is not required.

The recorded submodule already contains the repair. Run from the repository
root (existing CMake/GCC installations; outputs ignored):

```powershell
cmake -S third_party/rbengine -B private/research/rbengine-build -DRNET_ENABLE_ICE=OFF
cmake --build private/research/rbengine-build --config Debug --parallel 2
ctest --test-dir private/research/rbengine-build -C Debug --output-on-failure

cmake -S third_party/recomp-net -B private/research/recomp-net-build -DRNET_ENABLE_ICE=OFF -DRNET_BUILD_EXAMPLES=OFF -DRNET_BUILD_TESTS=ON
cmake --build private/research/recomp-net-build --config Debug --parallel 2 --target input_contract_test rollback_episode_test rb_wire_test ring_admit_test wire_map_test
ctest --test-dir private/research/recomp-net-build -C Debug --output-on-failure -R '^(input_contract_test|rollback_episode_test|rb_wire_test|ring_admit_test|wire_map_test)$'

gcc -std=c11 -I third_party/recomp-net/include tools/check-rollback-seal-span.c third_party/recomp-net/src/rollback/rnet_rollback.c third_party/recomp-net/src/input/rnet_input_contract.c -o private/research/check-rollback-seal-span.exe
& private/research/check-rollback-seal-span.exe
```

The next game implementation slice remains [#24's two-frame rally rewind](https://github.com/NoRain211/doaxbv-re/issues/24),
after independent local player control (#23): first establish restartable
execution, replayable time/input, and complete state, then compare identical and
late-changed input against an on-time reference. Use the submodule snapshot ring
at that seam. Resolve the input codec and apply the seal repair before attaching
network sessions. Passing these dependency tests alone does not satisfy either gate.
