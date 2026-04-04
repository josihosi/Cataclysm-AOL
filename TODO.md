# TODO

_Current actionable queue. Keep this aligned with `Plan.md`, not with last week’s ghost stories._

## Active finish line right now
- [ ] Close the deterministic upstreamable PR slice before spending meaningful time on the other green projects, unless a side task directly unblocks the PR or the PR is temporarily hard-blocked.
- [ ] Work serially through PR close-out items in this order: finish line -> deterministic tests -> agent play test -> Josef play test -> tweak round if needed.
- [ ] Do not move to the next PR-closing item just because deterministic tests passed; if Josef testing is the next gate, prepare that handoff first.
- [ ] Current close-out gate: get Josef to live-check the subject-first / trailing-id camp bark polish on current HEAD before moving to the next deterministic PR item.

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

## GREEN later — movement system improvements (resume after deterministic PR close-out)
- [x] Replace local follower step-chain movement payloads (`E E E`, etc.) with a relative signed-delta destination contract while keeping the current pathing / target-tile behavior intact.
- [x] Preserve the existing post-move state suffixes exactly:
  - `wait_here`
  - `hold_position`
- [ ] Use the same relative-delta idea for overmap-targeted movement / planner output where appropriate.
  - [x] Land the shared signed `dx` / `dy` parser/tests for future planner consumers.
  - [x] Land a shared `stay` / `move_omt dx=<signed_int> dy=<signed_int>` token parser/tests so future planner consumers agree on one small overmap movement grammar.
  - [x] Land the shared absolute-target resolver so planner consumers reuse one signed-axis convention instead of re-deriving it badly.
  - [x] Land the shared token formatter so future planner output can emit the same overmap grammar it already parses/resolves.
- [ ] Build a small overmap snapshot grid (about 5x5 or 6x6) for the broader Basecamp AI snapshot, not just the job-sending selector.
- [ ] Use collapsed terrain symbols with lowercase normal / UPPERCASE horde-present variants.
- [ ] Show only a present-only legend for symbols actually visible in the current overmap snapshot.
- [x] Update prompt/snapshot explanations and examples so the model is told to emit coordinates rather than micro-step chains.
- [x] Consider lightweight grid/axis hints in the snapshot if they help the model reason about offsets more reliably.
- [ ] Add deterministic parser/tests for positive/negative deltas, state suffix retention, malformed fallback behavior, and present-only overmap legend generation.

## GREEN later — richer Basecamp AI on `dev` (resume after deterministic PR close-out, except PR-closing bugfixes)
- [ ] Keep deterministic-first command extraction as the control spine.
- [x] Surface a compact deterministic handoff snapshot in crafting-request details, including exact board/detail/follow-up tokens (`show_board`, `show_job=<id>`, `job=<id>` / `delete_job=<id>`), so later Basecamp-AI glue does not have to rediscover resolved recipe/blocker facts from scratch.
- [x] Define the next structured Basecamp action tokens clearly:
  - [x] `craft=<query>`
  - [x] `show_board`
  - [x] `show_job=<id>`
  - [x] `delete_job=<id>`
  - [x] `job=<id>`
  - [x] `launch_ready_jobs`
  - [x] `retry_blocked_jobs`
  - [x] `clear_archived_jobs`
- [x] When deterministic craft handling cannot complete alone, hand off a compact structured snapshot instead of forcing the LLM to rediscover deterministic facts.
- [x] Externalize the deterministic Basecamp craft-handoff snapshot into `data/llm_prompts` / `config/llm_prompts` so the structured handoff can be tuned without C++ edits.
- [ ] Externalize any future Basecamp AI prompt text once a stable prompt contract exists, rather than hardcoding it in the camp flow.

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
