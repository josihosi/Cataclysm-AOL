# Roof-fire horde staged-horde footing v0 (2026-04-29)

Setup-only artifact for `Roof-fire horde detection proof packet v0`.

This packet exists because the horde seam must be reproducible before any roof/fire product proof uses it. It proves only that the named fixture/scenario can install and audit one saved overmap `horde_map` entry near the player.

## Named surface

- Fixture: `roof_fire_horde_staged_horde_v0_2026-04-29`
- Scenario: `bandit.roof_fire_horde_staged_horde_audit_mcw`
- Command: `python3 tools/openclaw_harness/startup_harness.py probe bandit.roof_fire_horde_staged_horde_audit_mcw`
- Run: `.userdata/dev-harness/harness_runs/20260429_170116/`

## Evidence

`startup.result.json.fixture_install.applied_save_transforms` records the fixture transform:

- `kind=horde_entity_near_player`
- `monster_id=mon_zombie`
- player location `[3367,994,0]`
- horde location `[3367,754,0]`
- player-relative offset `[0,-240,0]`
- destination `[3367,754,0]`
- `tracking_intensity=0`, `last_processed=0`, `moves=0`
- overmap `overmaps/o.0.0.zzip`

`audit_saved_horde_before_roof_fire_product_path.metadata.json` is green metadata:

- `status=required_state_present`
- `missing_required_hordes=[]`
- `observed_horde_counts={"mon_robin":1,"mon_zombie":1}` within `radius_ms=300`
- matching observed horde has offset `[0,-240,0]`, destination `[3367,754,0]`, `tracking_intensity=0`, `last_processed=0`, and `moves=0`

`probe.step_ledger.json` / `probe.report.json` show 2/2 green step-local rows. Top-level `verdict=inconclusive_no_artifact_match`, `feature_proof=false`, `evidence_class=startup/load-or-inconclusive`; that is correct because this is metadata/setup footing, not a product-feature artifact match.

## Boundary

This closes reproducible staged-horde setup for the next roof/horde proof attempt.

It does **not** prove:

- roof/elevated player footing;
- player-created roof fire/light/smoke;
- elapsed-time horde detection or response;
- `fd_smoke`/`fd_fire` on a roof tile;
- horde movement toward, or non-response to, a roof fire.

Next proof must start from this named footing or an equally artifacted horde setup, then add the real roof/fire/time/product evidence.
