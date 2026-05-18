# Anti-redundancy packaging imagination source of truth — 2026-05-01

This source covers the small anti-redundancy backlog packages Josef greenlit after Schani's `master...dev` redundancy audit.

## Finished scene

The game still feels the same where Josef already liked it: the writhing stalker is fair dread, the camp locker quietly makes workers less stupid, and bandits still react to visible pressure without pretending every camp is an omniscient radar dish. The difference is under the floorboards. The code no longer acts like each feature needed its own private copy of the engine.

A future maintainer can follow each feature from product behavior into a recognizable existing game seam: monster behavior hooks for stalker movement, item/wear/reload/zone APIs for camp locker decisions, and local field plus overmap horde signaling adapters for bandit smoke/light marks. The custom parts remain custom only where the product needs actual custom judgment.

## What should be visible from outside

- Existing green behavior does not regress just because the implementation gets less bespoke.
- Tests name the seam being reduced and prove the live-facing path still consumes it.
- Any moved logic has a smaller, clearer responsibility than before.
- The old behavior remains understandable from artifacts and tests; no “trust the refactor” fog.

## Boundaries

- These packages are backlog cleanup/refactor contracts, not active zombie-rider work.
- Do not reopen closed writhing-stalker, bandit, horde, or camp-lane product claims by drift.
- Do not expand into broad engine redesign unless a narrow seam proves impossible.
- If a supposedly redundant layer turns out to be carrying product-specific judgment, preserve it and document why it remains custom.

## Failure smells

- A package deletes custom code but loses the lived behavior Josef already approved.
- A unit test passes while the game path no longer consumes the seam.
- “Generic” code becomes a new private subsystem with a nicer hat.
- Andi touches the current zombie rider lane and these cleanup packages in one blob.

## Review questions

- What exact existing game seam did this package wire into?
- Which bespoke code disappeared, shrank, or became a thin adapter?
- Which custom behavior deliberately stayed custom, and why?
- What focused tests prove behavior preservation and seam consumption?
