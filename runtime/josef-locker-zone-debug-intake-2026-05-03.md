# Josef locker-zone debug intake — 2026-05-03

Raw Josef playtest/debug notes captured before classification. Do not package into Plan/TODO until Josef says the batch is done and follow-up classification has happened.

## Report A — NPC keeps orphan magazine/ammo after matching rifle is gone

Josef debug note: An NPC is carrying a magazine but not the rifle for that magazine. Ammo/magazine pickup should only happen when it matches a carried/kept compatible gun. Possible live path: NPC exchanged/dropped the rifle in favor of a melee weapon, but retained the now-orphan magazine/ammo.

Initial classification hypothesis:
- Locker/equipment cleanup likely needs an invariant: if a firearm is dropped/replaced, ammo and magazines that were only justified by that firearm should be dropped/returned too.
- Ammo/magazine pickup scoring should require a compatible kept weapon, not just generic usefulness.
- Follow-up check should distinguish: magazine for another compatible weapon still carried vs truly orphan magazine/ammo.

Desired behavior sketch:
- Pick up ammo/magazine only when it matches a carried/kept compatible firearm or an explicit camp policy need.
- If the matching rifle/firearm is dropped during weapon replacement, also shed the ammo/magazines that no longer match any carried/kept weapon.

## Report B — broken/XX backpack retained after better backpack pickup, likely because contents were inside

Josef debug note: An NPC is carrying an `XX` backpack / broken backpack. They picked up a better backpack, but likely failed to drop the broken one because items were stored inside it.

Initial classification hypothesis:
- Clothing/backpack rearrangement may be treating worn containers as simple wear/drop decisions without first relocating contained items.
- If a better container replaces a broken/obsolete worn container, the swap should attempt to move stored items into the new container, other worn/container inventory, or camp storage/locker zone before deciding the old container cannot be dropped.
- The bug may be in clothing rearrangement/drop failure handling rather than item scoring itself.

Desired behavior sketch:
- Clothing rearrangement should also rearrange items stored in the clothing/container.
- Broken/obsolete backpack should not be kept merely because it contains items, if there is a better backpack and safe storage/transfer capacity exists.
- If transfer fails, produce a diagnosable reason rather than silently keeping the broken container forever.
