# TODO

_Current actionable queue. Keep this aligned with `Plan.md`, not with last week’s ghost stories._

## GREEN now — upstreamable deterministic PR slice
- [x] Narrow the spoken camp craft trigger to the exact standalone word `craft` for the upstream-facing package.
- [x] Keep deterministic quantity parsing intact (numeric quantities plus the current small number-word support) while tightening the trigger boundary.
- [x] Reuse one deterministic craft resolver for both spoken craft intake and the later structured `craft=<query>` action token path.
- [x] Add tests for:
  - [x] `craft knife` → craft path triggers
  - [x] `witchcraft` → does **not** trigger craft path
  - [x] ambiguous craft query → reports ambiguity instead of guessing
  - [x] blocked craft query → reports blocker / missing requirement state
  - [x] ordered multi-word phrases beat generic noun fallbacks
- [x] Prepare the small PR-friendly explanation/packaging pass once the code/test slice is stable.

## GREEN now — movement system improvements
- [ ] Replace local follower step-chain movement payloads (`E E E`, etc.) with a relative signed-delta destination contract while keeping the current pathing / target-tile behavior intact.
- [ ] Preserve the existing post-move state suffixes exactly:
  - `wait_here`
  - `hold_position`
- [ ] Use the same relative-delta idea for overmap-targeted movement / planner output where appropriate.
- [ ] Update prompt/snapshot explanations and examples so the model is told to emit coordinates rather than micro-step chains.
- [ ] Consider lightweight grid/axis hints in the snapshot if they help the model reason about offsets more reliably.
- [ ] Add deterministic parser/tests for positive/negative deltas, state suffix retention, and malformed fallback behavior.

## GREEN now — richer Basecamp AI on `dev`
- [ ] Keep deterministic-first command extraction as the control spine.
- [ ] Define the next structured Basecamp action tokens clearly:
  - `craft=<query>`
  - `delete_job=<id>`
  - `job=<id>`
- [ ] When deterministic craft handling cannot complete alone, hand off a compact structured snapshot instead of forcing the LLM to rediscover deterministic facts.
- [ ] Externalize Basecamp prompt/snapshot text into prompt files once the deterministic craft router and token grammar are stable enough to stop moving every ten minutes.

## Agent testing / automation
- [x] Compile touched objects/binaries after each meaningful slice.
- [x] Run startup harness / launch / save-load checks on `dev` after meaningful Basecamp changes.
- [x] Keep filtered debug-log deltas clean enough to isolate new regressions.
- [ ] Revisit full `tests/cata_test` linking on this Mac when broader deterministic test coverage needs an end-to-end run; current local framework/library link trouble should be treated as environment work, not as a reason to skip logic tests.

## Josef manual testing pending / upcoming
- [ ] Later live-check the follower snapshot legend change in-game (`friendly` / `neutral` / `hostile` + threat) and confirm live follower behavior interprets the new target wording correctly.
- [x] Once the exact-word `craft` router lands, hand Josef a concise manual test packet for spoken camp crafting behavior.
- [ ] After the next Basecamp slice, preserve good in-game settings/state if the current `dev` profile becomes the preferred testing baseline again.

## DISCUSS before implementation
- [ ] Bandit AI architecture / constraints / test checkpoint design.
- [ ] Follower NPC deterministic-first command extraction without erasing reluctance, personality, defiance, or hostile edge cases.

## Parked but remembered
- [ ] Basecamp zone preset helpers/mod flow for new camps.
- [ ] Broader board QoL / feasibility summaries once the current command contracts stop moving.
- [ ] Broader harness growth after the current Basecamp finish-line work is stable.
