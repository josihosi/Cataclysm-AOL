# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active target: `Fuel writeback repair via wood source zone packet v0` under `C-AOL actual playtest verification stack v0` is now **JOSEF PLAYTEST PACKAGE / IMPLEMENTED-BUT-UNPROVEN** after the bounded source-zone repair attempt budget.

Current state: the harness now stages a real saved-world log source, `SOURCE_FIREWOOD` zone footing, preflight-wielded charged lighter with nested butane, and a narrow player-message-log bridge for decisive normal-lighting text immediately after targeting/turn advance and before save closure. Final changed probe `.userdata/dev-harness/harness_runs/20260429_091438/` proved the setup rows green but did not prove the feature: post-ignition save mtime did not advance after the save request/confirmation, no new player message log line such as `You successfully light a fire.` was captured, and saved copied map state still had no `fd_fire`/`fd_smoke`. Startup/load remains green; feature proof remains red/unproven.

Canonical anchors for the packaged target:

- Stack contract: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`.
- Active fuel repair packet: `doc/fuel-writeback-repair-via-wood-source-zone-packet-v0-2026-04-29.md`.
- Fuel repair imagination source: `doc/fuel-writeback-repair-via-wood-source-zone-imagination-source-of-truth-2026-04-29.md`.
- Josef package: `/Users/josefhorvath/.openclaw/workspace/runtime/josef-playtest-package.md`.

Next narrow work queue:

1. Do not rerun the fuel/source-zone/lighter harness packet unless Josef, Schani, or Frau reopens it with a materially changed normal-action/save-writeback primitive or manual evidence.
2. Wait for Josef playtest evidence or a new Schani-greenlit active slice; roof-fire horde proof remains blocked behind real player-lit fire.
3. If reopened, require the same evidence shape: narrow normal-lighting message bridge immediately after targeting/turn advance plus guarded saved `fd_fire`/smoke/light writeback; no debug `fd_fire` shortcut.

Proof discipline:

- This fuel repair is a bridge to real player-lit fire proof, not a debug `fd_fire` shortcut.
- Setup helpers may create the wood pile/zone footing, but closure requires normal player ignition and saved fire/smoke/light writeback.
- Do not promote roof-horde work while real player-lit fire remains unproven.
