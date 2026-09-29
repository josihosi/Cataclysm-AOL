# Debug error catalogue

For dismissible native game debug warnings during playtesting. Search this file by message
fragment; character IDs, item names and source line numbers may change.

Known warning: retain its occurrence in the run evidence, Ignore, reobserve and continue.
New warning: add its message/signature and evidence here, send a compact report to the
coordinator, then Ignore, reobserve and continue without waiting for a reply. Reporting and
investigating the underlying bug happen alongside the playtest. Do not repeatedly report the
same warning unless its consequence changes. A crash or recovery that cannot restore progress
is a blocker; a warning alone is not. This applies to debug warnings with an Ignore action,
not unrelated game choices or destructive confirmation dialogs.

Use the advertised `play --session SESSION debug-ignore --capture CURRENT_SHA` for a
startup debug dialog; copy the current capture from `play look`, then reobserve. Refresh the
capture for the next dialog and stop sending Ignore once World is ready. If that CLI action
is unavailable, use the existing verified Peekaboo route for the owned PID/birth with literal
`type "i"` (the tested route), then reobserve. Do not mistake input delivery for recovery or send keys blindly to a changed surface.

If a warning bears on the current claim, inspect the relevant state while continuing:
for example, verify actual item ownership/counts for an inventory test. Ignoring a dialog
neither fixes its cause nor proves save integrity.

## item-location-owner

- Match: `Failed to find item_location owner with character_id` (any ID).
- Meaning: a saved item reference could not resolve its character owner during loading.
- Handling: Ignore and continue; check actual item/actor state when relevant to the test.
- Source: `src/item_location.cpp`, observed line388; sorting deserialization appeared in the retained trace.
- Evidence: `build_logs/first-smoke-013/revision6-startup-blocker-checkpoint.json`,
  run `446128da066173112dac7cd7319432d53325ea11dac1d75dfb38b91ffe477069`.
- Observed recovery: I/i advanced to another debug warning; this alone did not establish World readiness.

## item-location-target

- Match: `item_location lost its target item during a save/load cycle`.
- Meaning: `ensure_unpacked()` could not restore the saved item reference.
- Handling: Ignore and continue; verify affected item state for inventory/locker/pickup claims.
- Source: `src/item_location.cpp::ensure_unpacked`, observed line174.
- Evidence: same revision6 checkpoint/run above; appeared after the owner-ID warning.
- Disposition: Ignore-and-continue; subsequent R014 pilot below proves World recovery, not item integrity.

## parent-location-target

- Match: `parent location doesn't exist. Item_location has lost its target over a save/load cycle`.
- Meaning: deserialization could not resolve a parent item location and reports the target lost.
- Handling: Ignore once through the exact current capture, reobserve, and verify affected inventory/actor state if relevant; Ignore is not a save-integrity claim.
- Source: `src/item_location.cpp:1012`, observed in R013 startup.
- Evidence: `build_logs/first-smoke-013/revision6-recovery-closeout.json` (run `446128da066173112dac7cd7319432d53325ea11dac1d75dfb38b91ffe477069`, successful recovery to visible map after this and the preceding owner/target warnings); current R013 run `42b5643fd4bf2991ab085c5ee841f293a9a526b099da36efdf9ee54140da1646`, profile log offset11453, capture `.userdata/openclaw_harness/bridge-sessions/selected-dcdd6efced334aacb0e34b23c2f33e9e/startup-dialog-captures/a928c48334e98c320c97.png`.
- Recovery evidence: the subsequent R014 pilot below proves this error family can recover to World; retain the separate v2 run evidence at its own scope.

## Verified CLI recovery

`build_logs/first-smoke-014/r014-native-debug-pilot-v7.json` binds three successive
owner/target/parent warnings to run `7b3cbe177ede1c365eafa8171a65a34483f49b2f1d74dd12213ad3d0503d972c`.
The CLI typed one literal i per current capture, reached same-run World at turn5216234,
and accepted `world.move.east` at turn5216235. A later stale-capture attempt sent no input
after readiness. Original response: session `selected-97909f82dca84d2882ac317972ba4315`,
`responses/play-2edb8cf3db024f23b3b73c278fd3e771.json`. This validates recovery and an
operable controller, not clean item deserialization, smoke admission or faction behavior.

## Maintenance

Luna/Sol workers may add observed warning signatures and update outcomes from evidence.
Merge equivalent messages; keep full logs in run artifacts and link them here. Revise an
entry when observed consequences change; do not accumulate speculative warnings or duplicate
recovery procedures. Keep one short entry per meaningful error family.
