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
- Deterministic implementation checkpoint is landed in-tree: local sight-avoid selection, active scout sortie clocks, return-home/writeback cleanup, active job serialization, and reviewer-readable local-gate/sortie output.
- Current validation gate: `git diff --check`; `make -j4 tests LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[bandit][live_world]"` -> 524 assertions in 22 test cases passed on 2026-04-27.
- Next target: shape one bounded live/harness proof on `bandit.live_world_nearby_camp_mcw` or equivalent real owned-site footing showing a scout either repositions out of exposure or returns after the sortie window. The first current-build live attempts were inconclusive (`.userdata/dev-harness/harness_runs/20260427_040319/` no new artifact; `.userdata/dev-harness/harness_runs/20260427_041554/` hung during the long standoff advance and was killed), so change the probe method before rerunning.
- No Lacapult work, no release publication, no repo-role surgery, no user-data mutation, and no broad local AI rewrite unless this active slice truly requires it.

Keep this file focused on the active lane only, ja, otherwise it turns into a junk drawer with headings.
