# Fresh PC save contract

This is the current Windows recomp contract for [#11](https://github.com/NoRain211/doaxbv-re/issues/11). Its runtime acceptance gate is [#17](https://github.com/NoRain211/doaxbv-re/issues/17). Original Xbox save import is out of scope.

## Storage and profiles

The supported location is relative to the directory containing the file passed to `recomp_runner --xbe`:

```text
<disc-root>/.recomp-storage/partition1/UDATA/
<disc-root>/.recomp-storage/save-undo-v1/
```

`UDATA` contains game-owned profile data. `save-undo-v1` is reserved for the host's recovery journal and lock. Other platform storage beneath `.recomp-storage` is not part of the profile transaction. There is no separate save-directory command-line option. Different disc roots have separate storage; hosts using the same root share its profiles.

A fresh profile starts through the normal New Game flow with no existing profile data. The game creates its slot metadata, initial progress and required supporting data. Platform cache files may already exist; they are not a prebuilt profile. No private profile or save payload is bundled, imported or required to seed this flow.

The game's three Vacation slots provide profile selection and separation. The host preserves the game-created layout and bytes; it does not introduce a PC account system or a replacement gameplay-save schema. Profile creation and subsequent progress must use normal game operations. The host does not promise that every possible gameplay field or profile-management operation has been validated.

## Compatibility

The supported reload scope is the same game/runtime build. There is no cross-build compatibility or migration promise, and the host does not enforce a build identifier inside game payloads. The game's own validator remains responsible for accepting its profile data.

The host journal has its own version, independent of the game schema. Unknown versions or malformed journal contents stop startup before guest execution; they are not silently discarded or migrated.

## Process-interruption protection

The full-profile and profile-region save hooks in `save_adapter.c` wrap the original game routines. Each outer operation backs up the entire existing `UDATA` tree, with file times, into one `undo` file in the journal before allowing its writes. Nested saves from the same guest fiber join that operation. This protects all slots in the tree as one unit, rather than committing individual files independently.

The operation commits only after the original routine reports success, required file operations have succeeded, and its writable profile handles are closed. Short writes and required open, seek, truncate, flush or close failures mark the operation failed. A nested failure also aborts the outer save even if its caller ignores the return value. The adapter stops guest execution when the transaction cannot complete successfully.

Recovery runs before guest startup and before another outer save:

- An unfinished backup, shorter than the size recorded in its header, has not authorized guest writes and is discarded.
- A pending operation restores the complete pre-operation tree. If that tree was absent, recovery removes the incomplete first save.
- Deleting the `undo` file is the commit point.
- The undo copy remains available until restoration finishes, so restoration can be retried after another interruption. If validation or restoration fails, startup stops with the journal retained.

Windows holds an exclusive handle to the journal's empty `lock` file for the host's lifetime. A second host cannot initialize the same store while that handle is held. The file remains on disk after ownership ends. Journal and profile-tree checks reject reparse points.

Deleting the `undo` file retries temporary Windows access, sharing and lock denials against a 500 ms deadline; persistent denial still stops the operation. Builds before 2026-09-24 kept `staging`, `pending` and `committed` directory records instead. If one is present, startup stops; running the build that wrote it once recovers it.

This protection applies to the intercepted saves. It is not a blanket atomicity guarantee for profile deletion, arbitrary writes outside those operations, external file edits, or other platform storage. Closing the host does not itself request a new game save; only completed game save operations are committed.

The supported console-break shutdown waits for a frame boundary with no pending save, tears down the presenter, and exits normally. This path is distinct from closing the renderer window with X; live validation of that window-close path remains pending.

There is **no power-loss or storage-device-failure guarantee**. Profile handles are flushed on close, but the journal, backup and directory updates do not implement a complete durable-write protocol.

## Verified evidence and limits

As verified on 2026-09-06, two independent complete natural flows have passed from separate empty profile stores. In each flow, New Game created the profile, a normal purchase visibly reduced the balance from 30,000 to 29,700, and shop exit saved it. After normal console-break shutdown, a new host process loaded the same profile and visibly restored 29,700 in the inventory screen. All four processes exited with code zero through the console-break path, without forced termination or an expected-stop override. Each saved profile remained byte-for-byte unchanged during its reload. Neither flow used an imported or bundled profile or forced guest state.

An actual game save has also been interrupted after a protected write changed the live profile. On fresh-process startup, recovery restored the previous payload and save metadata exactly, cleared the pending journal, and normal Continue returned to the prior morning map. Read-only state checks recovered the prior balance and inventory state. This proves one real process-interruption recovery, not clean shutdown or power-loss safety.

Under the earlier directory-record journal, a short-write attempt stopped before injection because the staging-to-pending journal rename was denied; its prior profile remained unchanged. After adding the bounded rename retry, a separate real game save wrote only part of a requested payload. The process exited with a pending undo record. A fresh process restored the exact previous payload and save metadata, cleared the pending journal, and normal Continue returned to the prior morning map within 31 seconds. Read-only state checks also confirmed the previous balance and inventory state; those values were not displayed in the map screenshot.

That directory-record candidate passed all five runtime tests of its time, including transient and persistent child-handle rename denial. The historical holder responsible for the earlier denied rename was not identified.

The single-file journal replaced those directory records on 2026-09-24 because a save cost about 48 ms of file operations on the game thread. It now costs about 2.5 ms. On a real game save, both an interruption after the second protected write and a short second write left a changed live profile and a pending `undo` file. In each case a fresh process restored the previous `UDATA` tree with identical bytes and file times before guest startup. The synthetic tests cover commit, rollback, ownership, required I/O failures, a truncated and an oversized `undo` image, a legacy record, and transient and persistent delete denial.

The two complete flows and the real fault recoveries satisfy the acceptance gate in [#17](https://github.com/NoRain211/doaxbv-re/issues/17). This contract resolves the storage, profile, recovery and compatibility questions in [#11](https://github.com/NoRain211/doaxbv-re/issues/11); [audible native output (#15)](https://github.com/NoRain211/doaxbv-re/issues/15) is the next milestone. Both fault/recovery observations were bounded by the supervisor; the separate complete flows supply the clean-shutdown evidence.

Purchased-item thumbnails remain blank, and background-rendering defects remain unresolved; the displayed balance is the directly visible restored purchase change. These observations do not establish power-loss protection, recovery from a whole-PC lockup, hidden-field completeness, profile deletion or cross-build migration.

Implementation: [transaction model](../recomp-runtime/save_transaction.cpp), [transaction API](../recomp-runtime/save_transaction.h), [save interception](../recomp-runtime/save_adapter.c), [file I/O](../recomp-runtime/kernel_file.c), and [startup ordering](../recomp-runtime/runner.cpp). Detailed runtime receipts and all save payloads stay private.
