# Locker service-parity test battery (2026-04-08)

Status: required current Package 3 deterministic expansion

This auxiliary turns Josef's greenlit concern into a concrete current-lane testing contract:
locker logic is a seam we will likely keep tweaking, so deterministic proof should cover more than classification alone.

## Core purpose

Prove that locker logic stays sane across three layers:
- classification
- planning
- actual service/equip behavior

The target failure class is:
- technically legal
- practically stupid

## Current test expansion families

### 1. Full-body protective suits
Cases where a current item provides broad built-in coverage across torso/legs and possibly head, mouth, hands, or feet.

Expected rule:
- do not replace or split the suit if the replacement set would lose critical built-in coverage
- do allow replacement only when the replacement set honestly preserves that coverage

### 2. Footed one-pieces
Jumpsuits or similar gear with built-in footwear.

Expected rule:
- do not split into barefoot nonsense unless replacement footwear exists
- positive counterpart should still pass once replacement footwear exists

### 3. Dress / gown / abaya / cheongsam-style one-pieces
Long draped or full-length one-piece clothing that can be split stupidly if the logic only sees legs and torso in the abstract.

Expected rule:
- only split when the replacement set is genuinely sane
- otherwise keep the current item

### 4. Planning / service parity
For the same clothing families above:

Expected rule:
- if planning says "keep current", service must not strip/split the item anyway
- if planning says "valid upgrade", service should actually perform that upgrade cleanly

### 5. Bullet-vs-melee armor seam
CDDA armor is not one scalar. Some gear is stronger against bullets, some against melee, and locker logic should not flatten that into obvious nonsense.

Expected rule:
- deterministic cases should prove the locker logic does not collapse meaningful armor-class differences into a dumb flat-number "upgrade"
- the logic should preserve or prefer the intended armor class sensibly instead of blindly swapping into the wrong protection style

## Deterministic shape

Each case should stay deterministic:
- controlled starting outfit
- controlled locker contents
- clear expected plan
- clear expected service result

## Acceptance

- each covered family gets meaningful deterministic proof
- classification/service mismatch is not allowed
- sane upgrades still happen when the full sane replacement set exists
- obvious bullet-vs-melee tradeoff cases do not degrade into flat-number stupidity

## Non-goals

- not a whole tactical doctrine engine
- not "solve zombies vs bandits forever"
- not a broad outfit AI rewrite
- not a giant clothing taxonomy project

## Scope discipline

Still Package 3, still narrow:
- expand the deterministic proof battery
- tighten service parity
- only deepen policy if the new deterministic cases prove a real gap
