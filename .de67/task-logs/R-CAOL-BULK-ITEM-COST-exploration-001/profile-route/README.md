# Bulk item cost profile route

This is a staged plan for two equivalent native workloads: advanced-inventory bulk planks and zone sorting of many unsorted items. It does not launch the game or register a shared scenario/fixture.

## Gate

Wait for Smartzone’s return and verify its exact session, game PID/birth identity, and bridge processes have exited. Obtain explicit Mac GUI/game-input handoff from Sol. Do not use `.userdata/reference-saves` or another worker’s profile. No native input or heavy build has been used to prepare this route.

## Prepared artifacts

- `route.json` — workload, baseline/after isolation, measurement, conservation, blocked/full and interruption controls.
- `startup-dry-run.json` — plan from the read-only `startup_harness.py ... --dry-run`; its disposable `.userdata/caol-bulk-item-cost-exploration-001` output has been removed.
- `workload-and-measurement-design.md` — prior investigation notes, including source entrypoints and profiler availability.

## Next steps after handoff

1. The current-source Tiles binding is ready: source SHA-256 `ebd59cf4...da7ba`, executable SHA-256 `171616a6...c8114`.
2. The native saved world is named `Westcreek` (the requested label `BulkItemCostSandbox` was absent, so the play-now default route generated the world). The player is at `[84,228,0]`; a default nearby NPC was removed through the advertised setup-only debug action before saving.
3. Owner-specific workload counts were unavailable in the supplied compact evidence, so the prepared 256 physical-item populations are explicitly diagnostic. The data item ID for the item named “plank” is `2x4`; a first setup attempt used nonexistent ID `plank`, failed at startup, was recovered and closed, and is retained as counterevidence. Valid workload copies use `2x4`.
4. Stage each valid workload from the saved native world into byte-identical before/after copies and record the recursive item census before each operation.
5. Run unprofiled before measurements, separate `/usr/bin/sample` captures, and equivalent after measurements. Preserve native activity semantics; record input/UI, activity and game-turn cost separately.
6. Exercise partial interruption/reload/resume and blocked/full controls on separate copies; reconcile conservation and every owned process/broker identity to OS exit.

Previously completed isolated focused tests are recorded in the worker context: `[items][advanced_inv]` and `[zones][items][activities][sorting]`. Do not rebuild or rerun heavy tests while Smartzone’s native check is active.
