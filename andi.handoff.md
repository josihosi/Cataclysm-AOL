# Andi handoff: CAOL-WRITHING-STALKER-v0

## Current canon state

`CAOL-WRITHING-STALKER-v0` is the **ACTIVE / GREENLIT IMPLEMENTATION PACKET**.

Authoritative canon is `Plan.md`, with `TODO.md`, `SUCCESS.md`, and `TESTING.md` aligned downstream. This handoff is only a terse executor packet; if it ever disagrees with those files, repair this file from canon instead of treating it as truth.

The previous Bandit structural bounty Phase 6 handoff is superseded. `CAOL-BANDIT-STRUCT-BOUNTY-v0` is closed/checkpointed green v0 in canon and must not pull execution away from the current creature packet.

## Current state boundary

Monster/stat/spawn footing and deterministic AI-decision substrate are implemented and validated:

- `mon_writhing_stalker` exists in `data/json/monsters/zed_misc.json` as a rare first-generation zombie-adjacent predator.
- `GROUP_ZOMBIE` has exactly one direct rare singleton spawn entry for `mon_writhing_stalker` in `data/json/monstergroups/zombies.json`.
- `tests/writhing_stalker_test.cpp` covers initial creature footing, flags/triggers, special attacks, no-upgrade footing, and rarity/singleton guardrails.
- `src/writhing_stalker_ai.*` plus `tests/writhing_stalker_test.cpp` cover the pure decision seam: interest ranking, bounded latch/decay, cover-first approach, opportunity scoring, zombie distraction, withdrawal, and cooldown/no-repeat rules.
- Checkpoint commits on `dev`:
  - `e0649a71be Add writhing stalker footing`
  - `aefa526335 Record writhing stalker footing boundary`
  - `25181aca49 Add writhing stalker AI decision helpers`
  - `cd7dc50a6b Update writhing stalker deterministic substrate state`

## Next executor slice

Wire the smallest live game seam for shadow/strike/withdraw behavior. The deterministic substrate is green support evidence only; do not redo it, and do not treat it as gameplay proof.

1. Connect the minimal live path that feeds believable evidence, exposure/focus, vulnerability, zombie pressure, and route context into the stalker decision helpers without bandit-camp economy logic.
2. Make the live monster behavior consume those decisions for shadow/hold/strike/withdraw/cooldown in the game path.
3. Keep persistence honest: if the live seam stores new latch/cooldown state, add save/load proof; if it remains stateless for v0, record persistence as not applicable/future-only.
4. After the live seam exists, promote harness scenarios:
   - `writhing_stalker.live_shadow_strike_mcw`
   - `writhing_stalker.live_no_omniscient_beeline_mcw`
   - `writhing_stalker.live_exposed_retreat_mcw`

## Credited evidence for current boundary

- `git diff --check`
- `python3 -m json.tool data/json/monsters/zed_misc.json`
- `python3 -m json.tool data/json/monstergroups/zombies.json`
- `make -j4 tests LINTJSON=0 ASTYLE=0`
- `make -j4 obj/writhing_stalker_ai.o tests/writhing_stalker_test.o tests LINTJSON=0 ASTYLE=0`
- `./tests/cata_test "[writhing_stalker]"` (88 assertions / 7 test cases after deterministic substrate landed)
- Focused Python JSON audit proving exactly one `mon_writhing_stalker` entry, in `GROUP_ZOMBIE`, with `weight: 1`, `cost_multiplier: 50`, and no pack-size spam.

## Non-goals/cautions

- Do **not** reopen Bandit structural bounty, Smart Zone, fire/signal, roof-horde, release, or cannibal lanes from this handoff.
- Do **not** implement bandit shakedown/camp logic, Basecamp economy hooks, common spawn spam, teleport ambushes, global exact scans, or long-term nemesis arcs in writhing stalker v0.
- Do **not** close gameplay behavior from JSON or deterministic tests alone. A test is not the product; live claims need a live game path or stay open.
- Do **not** redo the already-green deterministic substrate; the next useful work is the live game seam. Harness scenes come after that seam exists, otherwise the stalker becomes spreadsheet mugging wearing a bedsheet.

## Canonical anchors

- Active roadmap: `Plan.md`
- Active queue: `TODO.md`
- Success ledger: `SUCCESS.md`
- Testing ledger: `TESTING.md`
- Receipt ledger: `doc/work-ledger.md` (`CAOL-WRITHING-STALKER-v0`)
- Raw intake: `doc/writhing-stalker-raw-intake-2026-04-30.md`
- Imagination source: `doc/writhing-stalker-imagination-source-of-truth-2026-04-30.md`
- Contract: `doc/writhing-stalker-behavior-packet-v0-2026-04-30.md`
- Testing/playtest ladder: `doc/writhing-stalker-playtest-ladder-v0-2026-04-30.md`

## Superseded handoff note

This file originally replaced the stale Bandit structural bounty Phase 6 handoff after Frau Knackal's 2026-04-30 post-crunch nudge. Bandit structural bounty remains closed/checkpointed green v0; after the deterministic stalker substrate landed, the current executor path is the smallest live writhing-stalker shadow/strike/withdraw seam next.
