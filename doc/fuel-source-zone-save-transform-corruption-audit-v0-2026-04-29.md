# Fuel source-zone save-transform corruption audit v0 (2026-04-29)

## Verdict

The raw item-info/JSON startup blocker was caused by backend save-transform corruption, not by the fire/lighter gameplay path.

Exact breaker:

- harness function: `apply_map_furniture_near_player_transform` in `tools/openclaw_harness/startup_harness.py`
- fixture transform: `fuel_writeback_source_zone_v0_2026-04-29` `save_transforms[]` entry `kind: "map_furniture_near_player"`
- intended edit: place `f_brazier` at `offset_ms: [1, 0, 0]`
- touched save field: map submap `furniture` list in `maps/4.1.0.zzip`, file `140.41.0.map`, submap coordinates `[280, 82, 0]`, local map-square `[8, 10, 0]`
- broken write shape: flattened furniture entries, e.g. `[3, 6, "f_bench", 4, 6, "f_bench", ...]`
- correct write shape: native furniture triples, e.g. `[[3, 6, "f_bench"], [4, 6, "f_bench"], ..., [8, 10, "f_brazier"]]`

The bad implementation used `kept.extend([x, y, payload])` and `kept.extend([local_ms[0], local_ms[1], furn_id])`, flattening triples into scalar list members. The fixed implementation uses `kept.append([x, y, payload])` and `kept.append([local_ms[0], local_ms[1], furn_id])`. The fix is in commit `4500fb48e8` (`Prove fuel normal-map entry primitive`).

## Evidence

Broken run:

- run: `.userdata/dev-harness/harness_runs/20260429_135043/`
- verdict: `blocked_starting_ui_not_normal_map`
- screen artifact: `.userdata/dev-harness/harness_runs/20260429_135043/normal_map_entry_gate_before_activation.png`
- OCR artifact: `.userdata/dev-harness/harness_runs/20260429_135043/normal_map_entry_gate_before_activation.screen_text.json`
- OCR excerpt class: raw item-info/JSON soup, including `pocket_type`, `contents`, and `_seal`-like fragments
- transformed map pack: `.userdata/dev-harness/harness_runs/20260429_135043/saved_world/McWilliams/maps/4.1.0.zzip`
- inspected map file after copying/extracting the pack: `140.41.0.map`, submap `[280, 82, 0]`
- observed corrupt `furniture` shape: scalar-flattened list; first entries were `3, 6, "f_bench", 4, 6, "f_bench", ...`


Retained stale-run evidence, postmortem-only:

- `.userdata/dev-harness/harness_runs/20260429_093118/`: `fuel_writeback_source_zone_v0_2026-04-29`, same transform chain, non-proof. OCR after attempted target path showed raw `pocket_type` / `contents` item JSON fragments.
- `.userdata/dev-harness/harness_runs/20260429_093509/`: `blocked_starting_ui_not_normal_map_after_escape`, same transform chain, non-proof. OCR after stale-view dismissal still showed raw `pocket_type` / `contents` item JSON fragments.
- `.userdata/dev-harness/harness_runs/20260429_095021/`: `blocked_starting_ui_not_normal_map_after_escape`, same transform chain, non-proof. OCR after stale-view dismissal still showed raw `pocket_type` / `contents` item JSON fragments.
- `.userdata/dev-harness/harness_runs/20260429_122807/`: `blocked_starting_ui_not_normal_map`, same transform chain, non-proof. Normal-map gate OCR showed raw `pocket_type` / `contents` item JSON fragments.
- `.userdata/dev-harness/harness_runs/20260429_122955/`: `blocked_starting_ui_not_normal_map`, same transform chain, non-proof. Normal-map gate OCR showed raw `pocket_type` / `contents` item JSON fragments.

Those runs all resolve the same source chain (`fuel_writeback_source_zone_v0_2026-04-29` -> `bandit_live_world_nearby_camp_real_fire_exact_items_v0_2026-04-27` -> `bandit_basecamp_playtest_kit_v0_2026-04-22` -> `mcwilliams_live_debug_2026-04-07`) and apply the same relevant transform sequence, including `map_furniture_near_player`. They are evidence of the repeated symptom only; none is a fire proof surface.

Known-good fixed run:

- run: `.userdata/dev-harness/harness_runs/20260429_140645/`
- verdict: `artifacts_matched`, `feature_proof=true`
- screen artifact: `.userdata/dev-harness/harness_runs/20260429_140645/normal_map_entry_gate_before_activation.png`
- OCR fallback: `Wield:` and `YOU`
- transformed map pack: `.userdata/dev-harness/harness_runs/20260429_140645/saved_world/McWilliams/maps/4.1.0.zzip`
- same map field after fixed transform: `furniture` is a list of `[x, y, id]` triples and includes `[8, 10, "f_brazier"]`

Clean captured fixture:

- fixture: `fuel_source_zone_clean_normal_map_v0_2026-04-29`
- first clean proof run: `.userdata/dev-harness/harness_runs/20260429_143149/`
- first screenshot artifact: `.userdata/dev-harness/harness_runs/20260429_143149/normal_map_entry_gate_before_activation.png`
- same inspected map field remains correct triple-shaped furniture including `[8, 10, "f_brazier"]`

## Comparison against last known normal-map-loadable source

The source chain for the broken fixture resolves through `bandit_live_world_nearby_camp_real_fire_exact_items_v0_2026-04-27` into `bandit_basecamp_playtest_kit_v0_2026-04-22` / `mcwilliams_live_debug_2026-04-07`. The later green normal-map entry run (`20260429_140645`) uses the same fixture manifest and transform list but the fixed furniture writer. That isolates the visible startup break to the furniture-transform serialization shape, not to the fixture's intended scenario data, the player-item lighter edit, the log edit, the source-firewood zone edit, or the gameplay fire action.

The raw screen text mentioning inventory fields was a symptom of entering a corrupted/invalid UI/session state after load; it is not evidence that the `player_items` lighter transform was the root cause.

## Rule going forward

Do not run fire/lighter proof from a transformed source-zone fixture unless the first screenshot gate proves normal gameplay map UI. If a future transform lands on raw JSON/item-info soup, stop at that transform and inspect the exact save field shape before any action key.
