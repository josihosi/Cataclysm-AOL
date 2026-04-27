# C-AOL debug-proof finish stack v0 (2026-04-27)

Status: ACTIVE / GREENLIT

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
- Full player-fire product proof is now in Josef's playtest package as implemented-but-unproven: the agent-side route proved the scanner on synthetic loaded-map `fd_fire`, but the normal brazier/fuel/lighter route failed at tile-level fire creation in run `.userdata/dev-harness/harness_runs/20260427_123256/`. Keep fire creation unproven separate from bandit scan consumption from that player fire unproven. Remaining paired refreshed no-signal control, range-matrix/candidate split audit, signal decay, and hold/chill evidence should be finished or explicitly moved to the playtest package under the same escalation discipline.

### 2. Smart Zone clean-save live proof, if still unproven

The deterministic Smart Zone proof is closed, but the clean live GUI proof was previously blocked by harness startup/readiness detection rather than product behavior.

Greenlit path:

- If a small harness readiness fix can make the disposable Play Now profile reach explicit save/gameplay readiness, do it and run the clean Smart Zone live proof.
- Do not rerun the old McWilliams/bandit-contaminated macro as closure proof.
- If the clean live proof still cannot be made honest inside the 4-attempt budget, write a Josef playtest package with the manual recipe and exact expected close condition, then move on.

### 3. Any remaining C-AOL debug intake item still open after the above

Use `runtime/c-aol-debug-intake-2026-04-26.md`, `SUCCESS.md`, and `TESTING.md` to verify no current debug note is still hiding behind old parked/review wording.

Known closed notes should stay closed: Basecamp medical consumable readiness, Smart Zone deterministic save/reload, bandit horde visible-light bridge implementation, local sight-avoid / scout return proof, locker armor blocker clearing. Reopen only a real unproven product/debug gap, not the whole historical packet museum.

## Non-goals

- No Lacapult work.
- No GitHub release publishing until the debug-proof stack is honest enough for release posture again.
- No broad bandit concept-chain expansion beyond the explicitly reopened debug-proof gaps.
- No claim that deterministic playback/adapters are live product proof unless the live path is actually reached.

## Success state

- [x] The active Basecamp job-spam debounce lane is closed/checkpointed honestly before this stack became active.
- [x] The bandit live signal/site-bootstrap player-fire proof has either live/source-hook proof or a precise Josef playtest package for the exact unproven implemented behavior.
- [ ] Signal range, candidate/scoring split, decay/maintenance, and hold/chill instrumentation are either completed with tests/evidence or explicitly listed as implemented-but-unproven playtest items.
- [ ] The Smart Zone clean-save live proof is either honestly run on a clean disposable profile or converted into a precise Josef playtest package without reopening the contaminated McWilliams macro.
- [x] `Plan.md`, `TODO.md`, `TESTING.md`, and `SUCCESS.md` stop calling current debug notes parked/review-only when Josef has explicitly greenlit another finish pass.
