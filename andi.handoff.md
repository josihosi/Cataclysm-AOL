# Andi handoff: fuel normal-map entry primitive packet v0

## Active target

`Fuel normal-map entry primitive packet v0` is the active execution slice under `C-AOL actual playtest verification stack v0`.

Josef greenlit this on 2026-04-29 after the source-zone fire proof correctly stopped at the normal-map gate instead of typing through stale raw JSON. The job is not to prove fire yet. The job is to prove the harness can start the fuel/source-zone fixture from actual normal gameplay map UI before any fire/lighter action key is sent.

Canonical anchors:

- `Plan.md`
- `TODO.md`
- `TESTING.md`
- `SUCCESS.md`
- stack contract: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`
- active packet: `doc/fuel-normal-map-entry-primitive-packet-v0-2026-04-29.md`
- imagination source: `doc/fuel-normal-map-entry-primitive-imagination-source-of-truth-2026-04-29.md`
- downstream fuel repair packet: `doc/fuel-writeback-repair-via-wood-source-zone-packet-v0-2026-04-29.md`

## Current work

Build/prove one narrow normal-map entry primitive:

1. Prove saved/preflight footing or name the setup blocker: saved `f_brazier`, real logs, saved `SOURCE_FIREWOOD`, and charged wielded lighter.
2. Capture the first live post-load screen without blind `Escape`, `Return`, or other cleanup keys before the gate.
3. Require screenshot artifact path plus named visible fact proving actual normal gameplay map UI before any action key.
4. If the screen is raw JSON, item-info, stale, debug/internal, or otherwise not gameplay map/menu, classify it as blocker evidence and stop before activation/targeting/fire keys.
5. If the fixture consistently opens into stale JSON/item-info, rebuild or resave the fixture from normal gameplay view and record the new fixture path/name.
6. Once normal-map entry is green, resume the downstream source-zone fire proof guards: activation must show `Light where?`; east/source target must show the source-firewood confirmation prompt; post-target/advance must show a real ignition message/OCR and no depleted-lighter/no-ignition text; only then save mtime and saved `fd_fire`/`fd_smoke` may be audited.

## Evidence expectations

Minimum packet for this slice:

- command/scenario and run directory;
- setup metadata excerpt or named setup blocker;
- screenshot artifact path for the normal-map entry gate;
- named visible fact proving normal gameplay map UI;
- OCR/metadata explicitly labeled as fallback if direct image inspection is unavailable;
- blocker screenshot path and narrow OCR excerpt if raw JSON/item-info/stale/debug screen appears;
- clear statement that no fire/lighter action key was sent unless the normal-map gate was green.

## Non-goals/cautions

- This primitive is not player-lit fire proof by itself.
- Do not credit debug map-editor `fd_fire`, synthetic loaded-map fields, setup metadata, or stale-window screenshots as fire proof.
- Do not keep pressing through wrong screens to see what happens.
- Do not reopen inventory-selector/wield UI proof; the charged wielded lighter is setup footing only.
- Do not rerun completed performance rows as ritual.
- If two materially different normal-map entry attempts hit the same blocker, consult Frau before attempts 3-4; after four unresolved attempts, package the exact manual recipe/blocker instead of looping.
