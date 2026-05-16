# Fuel normal-map entry primitive — imagination source of truth — 2026-04-29

## Finished scene

The harness loads the fuel/source-zone fixture and the first captured screen is the actual Cataclysm gameplay map, not an item-info panel, raw JSON/debug view, stale window, or accidental text dump. The player location is visible enough for the harness to name the screen as normal map UI, with a screenshot artifact path and the visible fact it proves. Only after that gate is green may the fire action path begin.

The important lived difference is simple: before any key that could alter the fire proof, the harness can answer the same question a human would ask while sitting at the keyboard: “What screen am I on, and does this visible prompt make the next input correct?”

## Why this exists

The source-zone fire repair has credible setup footing — saved `f_brazier`, real logs, `SOURCE_FIREWOOD`, and a charged wielded lighter — but agent-side runs repeatedly loaded into stale item-info/raw-JSON text. Those screenshots are useful blocker evidence only. They are not gameplay proof, and no activation/targeting keypress through them can be trusted as normal player ignition.

This primitive isolates that upstream problem so Andi stops re-running fire logic before the harness has proven it is looking at the game.

## Outside-visible behavior

A green run shows, in order:

1. fixture/save setup exists by exact state metadata;
2. the first live screenshot after load is normal map UI;
3. the screenshot path and named visible fact are recorded;
4. raw JSON/item-info/stale/debug views are red-blocked before any action key;
5. if normal map is green, the downstream fire scenario may continue to `Light where?` and the source-firewood confirmation gates.

## Failure smells

- The run tries `Escape`, `Return`, or another blind cleanup key before proving the visible screen.
- OCR text from raw JSON is treated as a useful “screen” rather than a blocker.
- The report says “screenshot proof” when the image was not directly inspected and only OCR/metadata were used.
- The primitive proves setup metadata but never proves the live UI is normal gameplay.
- A fire/lighter key is pressed before the normal-map gate is green.

## Review questions

- Does the fixture itself need rebuilding so it opens from normal gameplay view?
- Is the normal-map marker too brittle (`YOU`) and should the gate use a stronger combination of visible map/sidebar facts?
- Does the harness need a dedicated normal-map entry probe separate from the fire scenario, so failure is cleaner and reusable?
- Once this gate is green, does the source-zone fire path continue through the Frau-approved proof tree: visible UI gate, action bridge, save mtime, saved `fd_fire`/`fd_smoke`?
