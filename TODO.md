# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active: `CAOL-ZOMBIE-RIDER-0.3-v0`.

Monster/evolution footing checkpoint is green in `d50715f00e`; the large-body `SMALL_PASSAGE` / window terrain seam is green in `zombie_rider_large_body_small_passage_pathing`; local combat AI footing is green in `4343dbdad1`; overmap light-attraction footing is green in `d2ffbd54c3`; deterministic rider convergence/band/camp-pressure footing is green in `ce05ef44ec` via `zombie_rider_overmap_convergence_forms_capped_band_from_active_light_memory`, `zombie_rider_overmap_convergence_keeps_lone_rider_below_band_minimum`, `zombie_rider_overmap_convergence_stops_after_light_memory_decay`, and `zombie_rider_overmap_band_pressure_circles_instead_of_wall_suicide`. Do not reprove these by ritual unless changed.

Next narrow slice: live-shaped rider funness rows. Open-field local-combat seam row is green via `zombie_rider.live_open_field_pressure_mcw` / `.userdata/dev-harness/harness_runs/20260501_013055/`; wounded disengagement row is green via `zombie_rider.live_wounded_disengagement_mcw` / `.userdata/dev-harness/harness_runs/20260501_014613/` after a forced fresh executable relink; cover/LOS escape row is green via `zombie_rider.live_cover_escape_mcw` / `.userdata/dev-harness/harness_runs/20260501_021656/`; proof notes `doc/zombie-rider-live-funness-open-field-proof-v0-2026-05-01.md`, `doc/zombie-rider-live-funness-wounded-disengagement-proof-v0-2026-05-01.md`, and `doc/zombie-rider-live-funness-cover-escape-proof-v0-2026-05-01.md`. Do not rerun these by ritual unless changed.

1. Add the next narrow live row: camp-light attraction into bounded rider convergence/band circling if the harness can stage it honestly.
2. Report run id/artifact path, start conditions, decision trace, rider count/band state, shot/reposition/retreat counts, stability/log state, and whether the row is green/yellow/red.
3. Keep widening one honest row at a time toward camp-light control and rider-band circling/harassment; stricter natural discovery/full indoor navigation remains future-only unless promoted.
