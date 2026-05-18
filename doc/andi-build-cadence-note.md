# Andi build cadence note

This is a standing note for Andi. It should prevent both fake confidence from stale binaries and time-wasting rebuild rituals.

## Default rule

Build only when the evidence you are about to claim depends on compiled code matching the current source. Do not rebuild merely because a note, plan, aux doc, or proof receipt changed.

## Cadence

- Docs/plans/proof notes only: no build.
- JSON/data-only changes: no compile; run the narrow load/smoke/probe that exercises the changed data if it is on a live path.
- One narrow `.cpp` implementation change: incremental build of the relevant binary/object before claiming runtime behavior.
- Header/API/signature/template/build-file changes: incremental build before claiming compile health; if errors smell stale or impossible, do one clean rebuild and say why.
- Test-only changes: build/run the narrow affected test target; do not rebuild the whole game unless the test target honestly requires it.
- Harness/probe script changes: no C++ rebuild unless the probe depends on newly changed compiled code; verify the probe with the smallest representative run.
- Before closing a player-facing/live-world lane: run the lane's required proof gate from the handoff/packet. A deterministic seam alone does not prove the live game path.

## Clean rebuild trigger

Use a clean/forced rebuild only when one of these is true:

- broad header or template churn could leave stale objects lying;
- build config, generated files, dependencies, or target shape changed;
- linker/compiler/runtime version behavior is contradictory after an incremental build;
- a proof row is about to be used as final current-binary evidence and stale-runtime risk is material;
- the current packet explicitly demands a forced rebuild.

When you clean rebuild, record the reason and the command/log path. Otherwise prefer the narrowest build that makes the next claim honest.
