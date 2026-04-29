# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active target: `Fuel writeback repair via wood source zone packet v0` under `C-AOL actual playtest verification stack v0` is **JOSEF-REOPENED STEP-SCREENSHOT ACTION AUDIT**.

Current state: the harness stages a real saved-world log source, `SOURCE_FIREWOOD` zone footing, preflight-wielded charged lighter with nested butane, and a narrow player-message-log bridge. Final changed probe `.userdata/dev-harness/harness_runs/20260429_091438/` proved setup rows green but missed feature closure: no save mtime advance, no new `You successfully light a fire.`-style message, and no copied saved-map `fd_fire`/`fd_smoke`. Josef's 2026-04-29 suspicion is that the macro is probably taking a wrong action somewhere, so the next run must be a step-by-step screenshot/action audit, not another blind rerun.

Canonical anchors for the packaged target:

- Stack contract: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`.
- Active fuel repair packet: `doc/fuel-writeback-repair-via-wood-source-zone-packet-v0-2026-04-29.md`.
- Fuel repair imagination source: `doc/fuel-writeback-repair-via-wood-source-zone-imagination-source-of-truth-2026-04-29.md`.
- Josef package: `/Users/josefhorvath/.openclaw/workspace/runtime/josef-playtest-package.md`.

Next narrow work queue:

1. Rerun the source-zone/lighter path once as a step-screenshot action audit: capture screenshots/OCR and exact metadata after every meaningful key/action boundary.
2. Required checkpoints: loaded scene, wielded lighter preflight, brazier/log/source-zone footing, activate/apply flow, each target/confirmation prompt, turn advance/activity result, player-message bridge, save prompt/confirmation, mtime, and saved `fd_fire`/`fd_smoke`.
3. Stop at the first wrong-screen/wrong-action/prompt mismatch and name the mistaken step; do not press onward through guessed prompts.
4. If the step audit still cannot locate a materially different route, return to Josef-package state instead of looping. Roof-fire horde proof remains downstream behind real player-lit fire.

Proof discipline:

- This fuel repair is a bridge to real player-lit fire proof, not a debug `fd_fire` shortcut.
- Setup helpers may create the wood pile/zone footing, but closure requires normal player ignition and saved fire/smoke/light writeback.
- Do not promote roof-horde work while real player-lit fire remains unproven.
