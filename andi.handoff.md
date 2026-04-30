# Andi handoff: CAOL-WRITHING-STALKER-v0

## Current canon state

`CAOL-WRITHING-STALKER-v0` is the **ACTIVE / GREENLIT IMPLEMENTATION PACKET**.

Authoritative canon is `Plan.md`, with `TODO.md`, `SUCCESS.md`, and `TESTING.md` aligned downstream. This handoff is only a terse executor packet; if it ever disagrees with those files, repair this file from canon instead of treating it as truth.

The previous Bandit structural bounty Phase 6 handoff is superseded. `CAOL-BANDIT-STRUCT-BOUNTY-v0` is closed/checkpointed green v0 in canon and must not pull execution away from the current creature packet.

## Current state boundary

Monster/stat/spawn footing is implemented and validated:

- `mon_writhing_stalker` exists in `data/json/monsters/zed_misc.json` as a rare first-generation zombie-adjacent predator.
- `GROUP_ZOMBIE` has exactly one direct rare singleton spawn entry for `mon_writhing_stalker` in `data/json/monstergroups/zombies.json`.
- `tests/writhing_stalker_test.cpp` covers initial creature footing, flags/triggers, special attacks, no-upgrade footing, and rarity/singleton guardrails.
- Checkpoint commits on `dev`:
  - `e0649a71be Add writhing stalker footing`
  - `aefa526335 Record writhing stalker footing boundary`

## Next executor slice

Build the deterministic stalker-interest/latch/opportunity/withdraw substrate before any live harness scene.

1. Add the smallest helper/evaluator seam that can explain stalker decisions without bandit-camp economy logic.
2. Add deterministic tests for:
   - human/player evidence outranking empty terrain;
   - weak smoke/indirect evidence staying weak;
   - exposed night light, cover/edge terrain, and zombie distraction affecting interest/opportunity with named reasons;
   - recent believable evidence creating a bounded latch;
   - stale/weak/no evidence decaying or failing to latch;
   - opportunity rising for hurt/bleeding/low-stamina/noisy/resting/crafting/boxed-in player states;
   - alert/bright/open/focused states holding or repelling pressure;
   - hurt/exposed/focused stalker withdrawal and cooldown preventing immediate repeat spam.
3. Keep persistence honest: if new latch/cooldown state is persisted, add save/load proof; if not, explicitly keep this deterministic substrate stateless/future-persistence.
4. Only after the deterministic substrate can name decisions, wire the smallest live seam for shadow/strike/withdraw behavior.
5. Promote harness scenarios later, not now:
   - `writhing_stalker.live_shadow_strike_mcw`
   - `writhing_stalker.live_no_omniscient_beeline_mcw`
   - `writhing_stalker.live_exposed_retreat_mcw`

## Credited evidence for current boundary

- `git diff --check`
- `python3 -m json.tool data/json/monsters/zed_misc.json`
- `python3 -m json.tool data/json/monstergroups/zombies.json`
- `make -j4 tests LINTJSON=0 ASTYLE=0`
- `./tests/cata_test "[writhing_stalker]"`
- Focused Python JSON audit proving exactly one `mon_writhing_stalker` entry, in `GROUP_ZOMBIE`, with `weight: 1`, `cost_multiplier: 50`, and no pack-size spam.

## Non-goals/cautions

- Do **not** reopen Bandit structural bounty, Smart Zone, fire/signal, roof-horde, release, or cannibal lanes from this handoff.
- Do **not** implement bandit shakedown/camp logic, Basecamp economy hooks, common spawn spam, teleport ambushes, global exact scans, or long-term nemesis arcs in writhing stalker v0.
- Do **not** close gameplay behavior from JSON or deterministic tests alone. A test is not the product; live claims need a live game path or stay open.
- Do **not** jump to harness scenes before the decision substrate exists; otherwise the stalker becomes spreadsheet mugging wearing a bedsheet.

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

This file replaces the stale Bandit structural bounty Phase 6 handoff after Frau Knackal's 2026-04-30 post-crunch nudge. Bandit structural bounty remains closed/checkpointed green v0; the current executor path is writhing stalker deterministic behavior substrate next.
