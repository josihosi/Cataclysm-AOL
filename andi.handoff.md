# Andi handoff: no active unblocked fuel retry

## Current target state

`Fuel writeback repair via wood source zone packet v0` is **implemented-but-unproven / Josef playtest package**, not an active rerun target.

Do not keep trying the lighter selector, pre-wield fixture, or source-zone macro unless Josef supplies manual evidence or Schani/Josef/Frau explicitly reopen a materially repaired normal-map entry primitive. The last agent-side blocker is no longer “can we stage fuel/lighter footing?” — that is green enough as setup. The blocker is that the live run loads into a stale item-info/raw-JSON screen even after the guarded `escape`, so activation/targeting cannot be trusted as normal player ignition.

Canonical anchors:

- `Plan.md`
- `TODO.md`
- `TESTING.md`
- `SUCCESS.md`
- `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`
- fuel repair packet: `doc/fuel-writeback-repair-via-wood-source-zone-packet-v0-2026-04-29.md`
- fuel repair imagination source: `doc/fuel-writeback-repair-via-wood-source-zone-imagination-source-of-truth-2026-04-29.md`
- Josef package: `/Users/josefhorvath/.openclaw/workspace/runtime/josef-playtest-package.md#2026-04-29--fuel-writeback-source-zone-repair-v0`
- completed performance matrix: `doc/c-aol-live-ai-performance-matrix-v0-2026-04-29.md`
- completed cannibal confidence support: `doc/cannibal-camp-confidence-push-live-playtest-matrix-v0-2026-04-29.md`

## Final fuel evidence boundary

Credited setup/support:

- `.userdata/dev-harness/harness_runs/20260429_093118/` proves saved `required_weapon=lighter`, nested butane charge, saved `f_brazier`, saved `log`, and saved `SOURCE_FIREWOOD` zone.
- The pre-wielded lighter proves setup state only. It does **not** prove inventory selector/wield UI.
- Setup helpers may stage wood/source-zone/brazier/lighter footing, but do **not** close player-lit fire.

Non-credit closure:

- `.userdata/dev-harness/harness_runs/20260429_090634/` is blocked, not proof: it proved log/source-zone footing, but then spent wait/save after a failed or depleted-lighter action, so no mtime/`fd_fire` closure can be credited.
- `.userdata/dev-harness/harness_runs/20260429_093118/` typed through a stale item-info/raw-JSON screen (`pocket_type`, `contents`, `specific energy` OCR), so activation/target/save keys are not trusted normal player action. Save mtime did not advance; no saved `fd_fire`/`fd_smoke`.
- `.userdata/dev-harness/harness_runs/20260429_093509/` added guarded `escape` and stopped when the stale JSON/info screen remained instead of normal map UI.
- Josef's 09:30 action-audit reopen `.userdata/dev-harness/harness_runs/20260429_095021/` staged per-boundary guards for activation/targeting/confirm/save/writeback, then stopped at the first wrong boundary: after `escape`, OCR still showed raw item JSON (`pocket_type`, `contents`, `specific energy`) and missed `YOU`; activation was not sent.
- No normal ignition, mtime, `fd_fire`, smoke, light, or bandit signal proof is credited.

Guard correction now encoded:

- Activation must show `Light where?` before any direction key.
- East targeting must show `Do you really want to burn your firewood source?` before any confirmation key.
- Unhandled confirmation/targeting prompts abort instead of guessed keypresses.
- After targeting/advance, capture player-message/OCR before any save/`fd_fire` audit.
- Red-block depleted-lighter/no-ignition text such as `lighter has 0 charges, but needs 1`.
- Proceed to save/mtime/`fd_fire` only on a real ignition line such as `You successfully light a fire.`

## If Josef or Frau reopens this later

A reopened path must start from normal map UI and close all of these, in order:

1. saved/preflight footing: `f_brazier`, real fuel, source-firewood zone, wielded charged lighter;
2. normal player activation/targeting from normal map UI;
3. decisive post-target/advance action-path guard: player-message/OCR red-blocks depleted-lighter/no-ignition text and requires a real ignition line;
4. actual gameplay/menu screenshot artifact path plus named visible fact at each relevant boundary; verify the next input against the visible menu/prompt before sending it; if the image cannot be directly inspected, say that and classify OCR/metadata as fallback rather than visual proof;
5. visible normal-map/fire-or-no-fire screenshot fact paired with player message/OCR;
6. guarded same-run save mtime advance;
7. saved `fd_fire` plus `fd_smoke`/smoke-light-relevant state;
8. only then bounded wait/time-passage and bandit signal response or classified no-response.

## Non-goals/cautions

- Do not rerun completed performance rows as ritual.
- Do not reopen Smart Zone Manager live proof unless Josef/Schani explicitly provides or approves a materially repaired UI-entry/key-delivery primitive or Josef manual evidence.
- Do not jump to roof-horde proof while real player-lit fire remains unproven.
- Do not credit debug map-editor `fd_fire`, synthetic loaded-map fields, stale-window screenshots, or pre-wielded setup as player-lit fire proof.
- If a reopened screenshot still shows stale raw JSON/item-info, report that exact visible failure and stop before action keys; do not treat JSON/info screens as the target proof surface when Josef asked for gameplay/menu evidence.
