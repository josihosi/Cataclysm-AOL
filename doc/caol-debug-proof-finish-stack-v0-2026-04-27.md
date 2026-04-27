# C-AOL debug-proof finish stack v0 (2026-04-27)

Status: CLOSED / CHECKPOINTED

Source: Josef's 2026-04-27 handoff after the C-AOL status readout. This supersedes the old "move on / Josef review" posture for current debug-proof notes.

## Intent

Finish the current C-AOL debug notes instead of leaving implemented-but-unproven work in a vague parked state.

The Basecamp job-spam debounce prerequisite is closed/checkpointed. Andi should now work this stack in order and move each item to its next honest state boundary.

## Escalation rule for this stack

For the same item or phase-blocker:

1. Attempts 1-2 may be Andi solo retries, including multiple focused harness attempts in one cron run, if each attempt changes setup, instrumentation, or evidence class.
2. Before attempt 3, Andi must consult Frau Knackal with the item key, blocker, what changed, what stayed the same, missing evidence, and suspected real cause.
3. Attempts 3-4 are the final materially changed retries after consultation.
4. If still unresolved after attempt 4, do **not** close it and do **not** park it as dead. Add it to `/Users/josefhorvath/.openclaw/workspace/runtime/josef-playtest-package.md` as implemented-but-unproven / Josef playtest package, preserve the caveat in canon, and move to the next greenlit unblocked debug note.

Failed proof is not completion. It is also not permission to loop forever. Sehr elegant, this bureaucracy with a guillotine.

## Greenlit stack

### 0. Closed prerequisite: Basecamp message-debug lane

`Basecamp job spam debounce + locker/patrol exceptions packet v0` is closed/checkpointed by `5ecf5ebdf2 Debounce basecamp job chatter`. Do not reopen it unless new evidence shows the landed debounce lied.

### 1. Bandit live signal + site bootstrap product proof finish

Reopened from the old Josef-review / move-on state.

Use `doc/bandit-live-signal-site-bootstrap-correction-v0-2026-04-26.md` as the detailed contract, with these 2026-04-27 corrections:

- Raw saved `fd_fire` / `fd_smoke` field proof is reader/consumer proof only. It cannot close player-fire product behavior.
- Try to prove a normal running-game fire/smoke/light source through the live bandit path: source exists in game, signal packet forms, owned site matches/refreshed or honestly rejects, and logs distinguish why.
- A bounded synthetic smoke-source shortcut is allowed only when labeled `synthetic smoke-source/live-signal proof`; it proves smoke-source live-signal behavior, not ordinary player-lit fire.
- The full player-fire recipe remains the honest product bar: deployed brazier, wood beside it, firewood source on the wood, lighter/ignition through normal game mechanics, visible fire/smoke, safe player placement and survival needs handled, several-hour wait, and matched-site bandit signal evidence.
- Clean threshold-surviving light proof is now satisfied for the synthetic loaded-map `fd_fire` source path by run `.userdata/dev-harness/harness_runs/20260427_114034/`: current-runtime probe at commit `daa2f1694c`, night fire source, `light_packets=1`, horde light signal power, and `matched_light_sites=1` / `refreshed_sites=1` while inside the 40 OMT scan envelope. This is **not** full player-lit-fire proof.
- Full player-fire product proof is now moved to Josef's playtest package as implemented-but-unproven after the allowed changed retries. Post-Frau attempt 3 used exact-id fixture items (`brazier`, `2x4`, charged `lighter`) and the normal deploy/drop/wielded-lighter route as far as the harness could drive it, then stopped at the midpoint gate: run `.userdata/dev-harness/harness_runs/20260427_124445/`, `tile_fire_audit.east.json` still missing target-tile `fd_fire`; saved-player inspection showed the GUI route had not actually moved/activated those items. Deterministic non-GUI primitive `fire_start_activity_exact_brazier_wood_lighter_creates_fd_fire` proves the current-build fire-start activity creates target-tile `fd_fire` from exact `f_brazier` + `2x4` + charged `lighter`, but only as activity-seam proof. Final activity-bridge experiment `.userdata/dev-harness/harness_runs/20260427_134253/` serialized `ACT_START_FIRE` near a seeded `bandit_camp`; the live log produced smoke/fire/light signal lines, but saved-world audit still showed no target-tile `fd_fire`/`fd_smoke` and the activity remained pending, so it is not player-fire closure. The manual close recipe, paired no-signal/control, range/candidate split, signal decay, and hold/chill expectations are listed under `runtime/josef-playtest-package.md` for `bandit-live-signal-real-fire-product-proof`.

### 2. Smart Zone Manager live layout separation correction

Josef reopened Smart Zone Manager on 2026-04-27 after live testing: generated zones are still lumping onto a single tile / overlapping incorrectly. The deterministic zone-id/type/option and serialize/reload proof is useful but no longer closes the player-visible layout.

Greenlit path / current state:

- The planner now keeps intended-separate generated zones on reserved separate tiles where the deterministic fixture can prove it, with only documented overlap exceptions.
- The v1 aux-plan separation expectations are covered deterministically where practical: fire-source `SOURCE_FIREWOOD` on the fire tile, adjacent `splintered`, nearby-but-distinct wood, readable crafting/food-equipment niches, clothing/dirty support, rotten outside Basecamp, and a larger unsorted intake area.
- Deterministic tests now assert zone geometry/separation and the intended-overlap allowlist, not only zone ids/options.
- The disposable Play Now startup readiness seam was repaired for current `.mm1/*.zzip` save markers, but the clean Smart Zone live/UI macro still drove the wrong UI/menu path and could not honestly inspect the generated layout.
- Do not rerun the old McWilliams/bandit-contaminated macro as closure proof. Smart Zone is now in Josef's playtest package as implemented-but-unproven live; move on unless Josef reopens it or the harness primitive materially changes.

### 3. Any remaining C-AOL debug intake item still open after the above

Use `runtime/c-aol-debug-intake-2026-04-26.md`, `SUCCESS.md`, and `TESTING.md` to verify no current debug note is still hiding behind old parked/review wording.

Fresh 2026-04-27 live follow-up from Josef reopened the local bandit stalking/return product gap after Smart Zone: smoke attraction now works, but the scout/hold-off bandit got far too close and was not visibly timing out/returning home in the current save. That reopened product gap is now reclosed on the current runtime: the code handles timed-out scout return-home before local-contact gating, deterministic coverage asserts the pre-local-contact timeout, and live run `.userdata/dev-harness/harness_runs/20260427_154309/` proves five-OMT hold-off, return-home, home-footprint observation, and returned pressure refresh on the McWilliams/Basecamp product path.

Other known closed notes should stay closed unless fresh evidence says they lied: Basecamp medical consumable readiness, bandit horde visible-light bridge implementation, locker armor blocker clearing. Smart Zone layout was reopened by Josef's live evidence and has now reached the implemented-but-unproven-live boundary; reopen only a real unproven product/debug gap, not the whole historical packet museum.

## Non-goals

- No Lacapult work.
- No GitHub release publishing until the debug-proof stack is honest enough for release posture again.
- No broad bandit concept-chain expansion beyond the explicitly reopened debug-proof gaps.
- No claim that deterministic playback/adapters are live product proof unless the live path is actually reached.

## Success state

- [x] The active Basecamp job-spam debounce lane is closed/checkpointed honestly before this stack became active.
- [x] The bandit live signal/site-bootstrap stack has either live/source-hook proof for the remaining smoke/fire/light product claims or a precise Josef playtest package for the exact unproven implemented behavior.
- [x] Signal range, candidate/scoring split, decay/maintenance, and hold/chill instrumentation are either completed with tests/evidence or explicitly listed as implemented-but-unproven playtest items.
- [x] The Smart Zone live layout separation correction is implemented and proven to its honest agent-side boundary: deterministic geometry/separation assertions are green, and the missing clean live/UI proof is covered by a precise Josef playtest package without rerunning the contaminated McWilliams macro.
- [x] The reopened bandit local standoff / scout return live correction is inspected and fixed/proven on the current product path: copied-save inspection answered the stale fixture timer state, deterministic coverage asserts the pre-local-contact scout timeout, and current-runtime live proof `.userdata/dev-harness/harness_runs/20260427_154309/` logs five-OMT hold-off (`live_dispatch_goal=140,46,0`), return-home timeout, `home footprint observed`, and `scout_report: returned -> pressure refreshed`.
- [x] `Plan.md`, `TODO.md`, `TESTING.md`, and `SUCCESS.md` stop calling current debug notes parked/review-only when Josef has explicitly greenlit another finish pass.
