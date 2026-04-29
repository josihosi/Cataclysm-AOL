# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

No current unblocked agent-side execution target is active in the `C-AOL actual playtest verification stack v0`.

`Fuel writeback repair via wood source zone packet v0` is now **implemented-but-unproven / Josef playtest package** after the post-Frau attempt budget closed:

- `.userdata/dev-harness/harness_runs/20260429_090634/` is blocked, not proof: it proved log/source-zone footing, then spent wait/save after a failed or depleted-lighter action, so no save mtime/`fd_fire` closure can be credited.
- Attempt 3/B `.userdata/dev-harness/harness_runs/20260429_093118/` proved the setup footing green: saved `required_weapon=lighter`, nested butane charge, saved `f_brazier`, saved `log`, and saved `SOURCE_FIREWOOD` zone.
- The same run did not prove ignition/writeback because the loaded UI was already a stale item-info/raw-JSON screen (`pocket_type`, `contents`, `specific energy` OCR), so activation/target/save keys were not a trusted normal player path and save mtime did not advance.
- Attempt 4 `.userdata/dev-harness/harness_runs/20260429_093509/` added a guarded `escape` back to map UI and stopped honestly when OCR still showed the stale JSON/info screen instead of normal map UI.
- Josef's 09:30 action-audit reopen `.userdata/dev-harness/harness_runs/20260429_095021/` staged per-boundary guards for activation, targeting, confirmation, save, mtime, and saved `fd_fire`/`fd_smoke`, then stopped at the first wrong boundary: after `escape`, OCR still showed raw item JSON (`pocket_type`, `contents`, `specific energy`) and missed `YOU`, so activation was not sent.
- Frau's 09:13 correction is now encoded in the scenario: activation must first show `Light where?`, east targeting must show `Do you really want to burn your firewood source?`, unhandled confirmation prompts abort, and after targeting/advance the player-message/OCR guard red-blocks depleted-lighter/no-ignition text such as `lighter has 0 charges, but needs 1` before any save/`fd_fire` audit.
- Josef's 10:18/10:19 clarification is now part of the reopen contract: screenshot evidence is not optional and JSON/info screens are not the intended proof surface. A valid action audit must open/observe the actual in-game map or menu, take the screenshot of that UI state, verify the next input against the visible menu/prompt before sending it, then continue. Each relevant boundary must name the screenshot artifact path plus the visible fact it is meant to prove. If the model/agent cannot directly inspect the image, say so and classify OCR/metadata as fallback rather than implying visual inspection. If the screenshot still shows stale raw JSON/item-info, report that visible failure and stop before action keys.

Canonical anchors:

- Stack contract: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`.
- Fuel repair packet: `doc/fuel-writeback-repair-via-wood-source-zone-packet-v0-2026-04-29.md`.
- Fuel repair imagination source: `doc/fuel-writeback-repair-via-wood-source-zone-imagination-source-of-truth-2026-04-29.md`.
- Josef package: `/Users/josefhorvath/.openclaw/workspace/runtime/josef-playtest-package.md#2026-04-29--fuel-writeback-source-zone-repair-v0`.

Next narrow work queue:

1. Do not rerun the fuel/source-zone/lighter harness path again unless Josef supplies manual evidence or Schani/Josef/Frau reopen it with a materially repaired normal-map entry primitive; the 09:30 action audit already stopped before activation because the loaded UI remained stale JSON.
2. If reopened with normal-map entry repaired, use the corrected guard shape already staged in `bandit.live_world_nearby_camp_source_zone_fire_writeback_mcw`: `dev-harness` suppresses the automatic post-load Return, the first step is a direct normal-map capture gate (no blind Escape/Return through stale raw JSON), no direction key is sent until `Light where?`, no confirmation key until the source-firewood prompt, and no save/mtime/`fd_fire` audit until the post-target/advance player-message/OCR guard sees a real ignition line and no depleted-lighter/no-ignition text.
3. For any reopened fuel/fire path, drive it like a human menu audit: open/observe the in-game menu or normal map, screenshot it, verify the next key is correct from that visible state, then send the key. Include screenshot artifact path + named visible fact at each relevant boundary. Actual fire closure requires all four evidence classes together: visible normal-map/fire-or-no-fire screenshot fact, player message/OCR, save mtime, and saved `fd_fire`/`fd_smoke`.
4. Closure still requires normal player ignition, a guarded same-run save mtime advance, saved `fd_fire` plus smoke/light-relevant state, and only then bounded bandit signal response or classified no-response.
5. Roof-fire horde proof remains blocked behind real player-lit fire. Debug `fd_fire`, synthetic loaded-map fields, stale item-info screens, and inventory-selector/wield UI traces remain non-credit for this closure.
