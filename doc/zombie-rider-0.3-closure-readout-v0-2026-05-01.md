# CAOL-ZOMBIE-RIDER-0.3-v0 — closure readout

Status: CLOSED / CHECKPOINTED GREEN V0 INITIAL DEV

## Scope closed

Initial release `0.3` zombie rider dev is green for v0: endpoint late-game mounted ranged predator, exact flavor preservation, scary-fast movement, shoot/flee/reposition local combat, overmap light attraction, rider convergence/band formation, and staged-but-live funness proof that open-ground terror remains fair through counterplay.

## Evidence

- Monster/evolution footing: commit `d50715f00e` adds `mon_zombie_rider`, exact three-line Josef description, huge/scary-fast endpoint stats, pseudo bone-bow footing, mature `GROUP_ZOMBIE` direct gate at `730 days`, and focused `[zombie_rider]` tests.
- Large-body passage seam: `zombie_rider_large_body_small_passage_pathing` proves the spawned rider rejects `TFLAG_SMALL_PASSAGE` / `t_window_empty` while normal-sized pathing can use the same passable seam.
- Local combat AI: checkpoint `4343dbdad1` proves live `monster::plan()` consumption for line-of-fire bow targeting, shot cadence/cooldown, post-shot withdrawal/reposition, close/injured retreat, and blocked-LOS no-shot counterplay.
- Overmap light attraction: checkpoint `d2ffbd54c3` proves mature-world light interest through the existing light projection / horde signal seam, no-rider/no-light/weak/daylight controls, temporary memory decay, and `max_riders_drawn_by_light = 2`.
- Rider convergence/band pressure: checkpoint `ce05ef44ec` proves capped selection by distance then `rider_id`, no convergence after decayed memory, lone-rider non-band behavior, and camp pressure postures including `circle_harass` instead of wall-suicide.
- Live open-field pressure: `zombie_rider.live_open_field_pressure_mcw` -> `.userdata/dev-harness/harness_runs/20260501_013055/`, green staged-but-live bow pressure then withdrawal.
- Live wounded disengagement: `zombie_rider.live_wounded_disengagement_mcw` -> `.userdata/dev-harness/harness_runs/20260501_014613/`, green wounded withdrawal with no bow-pressure spam.
- Live cover/LOS escape: `zombie_rider.live_cover_escape_mcw` -> `.userdata/dev-harness/harness_runs/20260501_021656/`, green opaque-cover/no-target/no-line-of-fire control.
- Live camp-light band: `zombie_rider.live_camp_light_band_mcw` -> `.userdata/dev-harness/harness_runs/20260501_030432/`, green mature-world lit-camp aggregation with `selected_riders=2`, `band_formed=yes`, `band_size=2`, and `posture=circle_harass`.
- Live no-camp-light control: `zombie_rider.live_no_camp_light_control_mcw` -> `.userdata/dev-harness/harness_runs/20260501_032016/`, green matched negative control with no rider camp-light/horde-light/band/circle-harass traces.

## Boundaries and caveats

- These are staged-but-live v0 rows, not natural random discovery, full siege/navigation behavior, or a normal release packaging claim.
- No year-one routine spawn, unavoidable camp deletion, omniscient light doom, or infinite rider accumulation claim is made.
- No unresolved tuning problem surfaced in the v0 evidence; future feel tuning is product judgment, not a blocker for this closure.
- Do not reopen this lane without explicit Schani/Josef promotion of a stricter follow-up.
