# recomp-net source patch

`rollback-seals.patch` applies to submodule revision
`859ba28b93f7374f2ac1eb70d2171594c5cd4f4b`. It fixes the 64-row completion-mask
limit, overflow at the last tick, stale authority on reseal, and replacement
of confirmed remote rows with unconfirmed history. It also repairs the episode
test fixture and adds boundary/authority regressions.

The recorded submodule now includes this repair in commit `694e493`.
It is published in [our fork](https://github.com/NoRain211/recomp-net/commit/694e493460740ceb7f6e0171bad6ca484c57f631). Do not reapply the patch to that commit.
For a checkout of the published base revision above, run from the repository root:

```powershell
git -C third_party/recomp-net apply --check ../../tools/recomp-net-patches/rollback-seals.patch
git -C third_party/recomp-net apply ../../tools/recomp-net-patches/rollback-seals.patch
```

Apply once. To check whether it is already applied, use `git apply --reverse
--check` with the same path. Keep this patch as a portable repair against the
published base until an upstream revision contains the fix, then update the
pin and remove the patch. Publishing the parent gitlink also requires making
its referenced submodule commit available remotely.

The maximum includes the baseline row. Invalid or oversized initial seals
clear `inputs_sealed` and preserve the correction target. A host must check
`rnet_rb_inputs_sealed` before replay and reject or split excessive windows.
No game runtime calls this API yet.

Run the focused existing tests and standalone probe using the commands in the
[audit](../../docs/research/rollback-engine-audit.md#verification-and-next-implementation-slice).
The patch does not change the wire format or implement Xbox input/snapshot support.
