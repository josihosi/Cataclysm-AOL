# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active target: `Bandit local sight-avoid + scout return cadence packet v0`.

- Work from `/Users/josefhorvath/Schanigarten/Cataclysm-AOL` on `dev`; `josihosi/Cataclysm-AOL` is project/release truth and `josihosi/C-AOL-mirror` is green-dot-only.
- Use `doc/bandit-local-sight-avoid-and-scout-return-cadence-packet-v0-2026-04-26.md` as the active contract.
- Preserve the just-closed bridge caveat: `Bandit live-wiring audit + visible-light horde bridge correction v0` has loaded-map visible fire/light -> live horde signal bridge proof, not full player-lit brazier/wood/lighter product proof.
- Do **not** continue the parked smoke/fire site-refresh proof loop for `Bandit live signal + site bootstrap correction v0`; that item is in Josef review as `bandit-live-signal-smoke-source-site-refresh-proof` after attempt 5.
- First implementation target: inspect current local-contact / hold-off / scout outing state in `src/do_turn.cpp`, `src/bandit_live_world.cpp`, `src/bandit_live_world.h`, and existing `[bandit][live_world]` tests.
- Shape deterministic tests before live harness work: current/recent exposure classification, bounded no-teleport/no-perfect-omniscience reposition choice, finite scout sortie expiry, and return-home/writeback state transition.
- Then shape one bounded live/harness proof on `bandit.live_world_nearby_camp_mcw` or equivalent real owned-site footing showing a scout either repositions out of exposure or returns after the sortie window.
- No Lacapult work, no release publication, no repo-role surgery, no user-data mutation, and no broad local AI rewrite unless this active slice truly requires it.

Keep this file focused on the active lane only, ja, otherwise it turns into a junk drawer with headings.
