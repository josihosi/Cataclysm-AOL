# Andi handoff: Smart Zone Manager harness-audit retry packet v0

## Active target

`Smart Zone Manager harness-audit retry packet v0` is active.

Canonical anchors:

- `Plan.md`
- `TODO.md`
- `TESTING.md`
- `SUCCESS.md`
- `doc/smart-zone-manager-harness-audit-retry-packet-v0-2026-04-28.md`
- `doc/smart-zone-manager-harness-audit-retry-imagination-source-of-truth-2026-04-28.md`

## New proof primitive

Josef found the cheap live observable: Zone Manager shows relative coordinate labels beside each zone, e.g. `2E` for two tiles east of the player.

Use that directly. Run the live Zone Manager generation path, inspect the generated zone list, and read/capture the coordinate labels. If zones are lumped onto one tile, they will show the same coordinate label. If the generated layout is separated correctly, the relevant zones should show distinct expected offsets.

## Preserved non-green boundary

Do not launder the old Smart Zone evidence into closure:

- `smart_zone.live_probe_safe_clean` on `smart-zone-audit-live-20260428a`, run `.userdata/smart-zone-audit-live-20260428a/harness_runs/20260428_151053/`, is `feature_proof=false`.
- That run had runtime mismatch, 25/25 yellow step-ledger rows, and only a temporary `ZONE_START_POINT`, not generated separated Smart Zone layout proof.
- The older clean-profile route also had loadability trouble; repair or replace the fixture honestly if needed.

## Next work

1. Rebuild/relink to current runtime and repair/stage a loadable Smart Zone fixture/profile if needed.
2. Open Zone Manager through the live UI.
3. Invoke Smart Zone generation through the live UI.
4. Inspect the generated zone list and capture/read visible relative coordinate labels beside each generated zone.
5. Compare coordinates: same labels prove lumping; distinct expected labels prove separated placement. Pair with saved zone metadata and save/reopen evidence where possible.
6. Classify the result as green feature-path, blocked with a named missing primitive/fixture, or non-green with exact evidence.

## Non-goals/cautions

- Do not redesign Smart Zone behavior.
- Do not use deterministic `clzones` tests as feature closure; they are support only.
- Do not rerun the contaminated old McWilliams macro or the non-loadable clean profile as closure without material repair.
- Do not treat startup/load or a menu screenshot without coordinate labels as layout proof.
- Keep `Generic clean-code boundary review packet v0` queued after this Smart Zone checkpoint.
