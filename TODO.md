
# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active target: `Smart Zone Manager harness-audit retry packet v0`.

Current state: Josef reopened the Smart Zone proof with a concrete live-readable coordinate primitive. The Zone Manager shows relative coordinates beside each zone, e.g. `2E` for two tiles east of the player. Andi should use that UI list as the cheap primary observable: generated zones that are lumped onto one tile will show matching coordinates; intended-separated zones should show distinct expected offsets.

Canonical anchors for the active target:

- Contract: `doc/smart-zone-manager-harness-audit-retry-packet-v0-2026-04-28.md`.
- Imagination source: `doc/smart-zone-manager-harness-audit-retry-imagination-source-of-truth-2026-04-28.md`.
- Prior non-green boundary: `smart_zone.live_probe_safe_clean` on `smart-zone-audit-live-20260428a`, run `.userdata/smart-zone-audit-live-20260428a/harness_runs/20260428_151053/`, is not feature proof.

Next narrow work queue:

1. Repair or replace the loadable Smart Zone fixture/profile if needed, and rebuild/relink to current runtime.
2. Open Zone Manager through the live UI and invoke Smart Zone generation.
3. Inspect the generated zone list and capture/read the visible relative coordinate labels beside each generated zone.
4. Compare coordinate labels: same-coordinate labels prove lumping; distinct expected offsets prove separated placement. Pair with saved zone metadata and save/reopen evidence where possible.
5. Classify the result as green feature-path, blocked with a named UI/fixture primitive gap, or non-green with exact evidence.

Proof discipline:

- Coordinate labels are live UI evidence for the product question; screenshots/OCR are acceptable here if they capture the actual zone-list coordinates clearly, but prefer structured screen text or saved metadata when available.
- Deterministic `clzones` geometry tests remain support only, not feature closure.
- Do not rerun the old contaminated McWilliams macro or the non-loadable clean profile as closure without material repair.
- Keep `Generic clean-code boundary review packet v0` queued after this retry checkpoint; do not run it mid-Smart-Zone proof.
