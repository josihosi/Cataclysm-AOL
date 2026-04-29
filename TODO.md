# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

No active Andi execution target is currently promoted. `C-AOL actual playtest verification stack v0` reached its completed state boundary and is ready for Schani plans-aux review.

Preserve, do not rerun as ritual:

- source-zone player fire: `.userdata/dev-harness/harness_runs/20260429_153253/`, `bandit.live_world_nearby_camp_visible_brazier_source_zone_firestarter_action_mcw`; real normal Apply inventory/deploy/lighter path, saved `f_brazier` + `fd_fire`;
- player-lit fire -> bandit signal: `.userdata/dev-harness/harness_runs/20260429_162100/`, `bandit.player_lit_fire_signal_wait_mcw`; bounded wait, live signal/dispatch, saved active scout response;
- roof/elevated fire source: `.userdata/dev-harness/harness_runs/20260429_172847/`; normal player-created roof `t_tile_flat_roof` + `f_brazier` + `fd_fire`;
- roof-fire horde split proof: `.userdata/dev-harness/harness_runs/20260429_180239/`, `bandit.roof_fire_horde_split_wait_from_player_fire_mcw`; live roof-fire horde signal plus saved horde retarget/movement-budget response;
- Smart Zone Manager live layout remains implemented-but-unproven/Josef-package; do not rerun without a materially repaired UI-entry primitive or explicit reopen.

Future-only watchlist unless Schani/Josef promotes it:

- natural three/four-site player-pressure behavior;
- true zero-site idle baseline;
- stricter roof-horde positive `tracking_intensity` proof.

Recommended next action: Schani plans-aux review/promote next lane. Josef decision is needed only if Schani wants stricter proof or a new product requirement.
