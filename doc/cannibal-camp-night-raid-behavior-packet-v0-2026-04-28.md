# Cannibal camp night-raid behavior packet v0 (2026-04-28)

Status: ACTIVE / DETERMINISTIC SUBSTRATE LANDED

Imagination source: `doc/cannibal-camp-night-raid-imagination-source-of-truth-2026-04-28.md`
Code audit: `doc/cannibal-camp-night-raid-code-audit-2026-04-28.md`

## Why this exists

Josef clarified the next cannibal-camp identity target after the earlier adopter/no-extort work:

- cannibal camps should remain mostly bandit-camp-shaped hostile sites;
- they should be more vicious, trigger-happy, and less transactional;
- they should not send lone cannibals as ordinary attack groups;
- they should prefer bigger raid groups and pounce when it is dark or otherwise favorable;
- smoke/light/human routine should create scout/stalk/dispatch pressure, not instant combat.

The implementation already had a real cannibal profile and no-extort local surface. The first deterministic substrate packet now adds pack-size gates, smoke/light nearby-lead classification, avoid-detection stalking response, and darkness/concealment local-gate input; live night-raid product proof is still a later slice. This packet keeps that product difference bounded without reopening the already-closed cannibal adopter/no-extort packets.

## Scope

1. Preserve the shared hostile-site/bandit-live-world substrate: cannibal camps use the same owned-site, roster, active-group, local-gate, save/load, and writeback footing where it is genuinely generic.
2. Add profile-specific cannibal pack-size logic so ordinary planned attack/raid pressure does not dispatch a lone cannibal as the whole fight.
3. Add smoke/light/human-routine lead interpretation for cannibals where the shared camp-map/live-signal substrate is available: raw signals should justify scout/stalk/dispatch pressure, not instant combat.
4. Add an explicit darkness/concealment/night-window input seam for cannibal local-gate decisions.
5. Make darkness/concealment shift cannibal behavior toward `attack_now` when roster/pressure/target weakness are favorable, while daylight or high threat still produces stalk/probe/hold/abort as appropriate.
6. Ensure cannibal stalking/approach avoids vision honestly: exposure should cause bounded reposition, hold-off, or abort behavior rather than continued visible approach.
7. Keep cannibals blocked from bandit shakedown/extortion surfaces.
8. Keep reports reviewer-readable: lead source, profile, pack size, darkness/concealment input, sight/exposure state, posture, and reason should be visible in deterministic output and later live artifacts.
9. Keep save/load and member-state behavior honest: dead/wounded/outbound/home-reserve members must affect whether a pack attack is possible.

## Non-goals

- broad cannibal society/lore expansion
- player capture/cooking mechanics
- perfect stealth AI
- teleporting attackers
- ordinary bandit shakedown rewrites
- singleton writhing-stalker work
- making cannibals always attack regardless of risk
- live/harness product closure before deterministic seams exist

## Success state

This packet is complete only when:

1. deterministic coverage proves cannibal planned attacks/raids require a meaningful pack size under ordinary conditions, while scout/probe behavior may remain smaller when explicitly classified that way;
2. deterministic coverage proves a one-dispatchable-member cannibal camp does not perform an ordinary full attack/raid against a defended target;
3. deterministic/code coverage proves smoke/light/human-routine leads are interpreted as scout/stalk/dispatch pressure rather than instant combat where that substrate is available;
4. deterministic coverage proves darkness/concealment can change a cannibal local-gate outcome from stalk/probe/hold to attack when the rest of the scene is favorable;
5. deterministic coverage proves daylight/no-cover and high-threat cases still delay, hold, probe, stalk, or abort instead of making cannibals suicidal;
6. deterministic coverage proves cannibal stalk/approach sight-avoidance reacts to exposure with bounded reposition, hold-off, or abort rather than visible beeline pressure;
7. bandit shakedown/pay/fight behavior still works unchanged and cannibal shakedown remains blocked;
8. reports name lead source, `profile=cannibal_camp`, pack size/strength, darkness/concealment input, sight/exposure state, posture, and no-shakedown/attack reason;
9. save/load proof preserves cannibal roster, active member IDs, target state, and profile after a multi-member dispatch;
10. any later harness/product proof reaches the real lead/scout/dispatch/local-contact path and does not close from evaluator-only tests.

## Testing expectations

First deterministic slice:

- focused `[bandit][live_world][cannibal]` coverage for pack-size gates;
- focused lead-source tests for smoke/light/human-routine pressure becoming scout/stalk/dispatch intent instead of instant combat where the substrate is available;
- focused local-gate tests comparing same cannibal strength/opportunity/threat in daylight versus darkness/concealment;
- focused sight-avoidance/exposure tests proving stalking cannibals reposition, hold off, or abort when seen instead of continuing visible approach;
- no-regression coverage for `bandit_live_world_makes_cannibal_camp_attack_instead_of_extort` and bandit shakedown tests;
- save/load test for multi-member cannibal active groups.

Later live/harness slice, only after deterministic seams are landed and proof-freeze rules can support it:

- named scenario with a cannibal camp, enough living at-home members, smoke/light or repeated human routine creating a target lead, and equivalent day/night conditions;
- scout/stalk/dispatch must be visible in artifacts before attack credit: raw smoke/light may not become instant combat;
- day/no-cover result must not credit night attack;
- night/dark/concealed result must show multi-member cannibal attack posture through the real path;
- no shakedown surface opens;
- save/load or artifact metadata preserves active group/roster/profile.

## First implementation packet

Current Andi target:

`Cannibal camp pack-size + smoke-light/darkness/sight-avoid substrate v0`

Bounded instruction:

- Add the smallest profile-specific seams needed for pack-size, smoke/light/human-routine lead interpretation, darkness/concealment input, and stalking sight-avoidance/exposure response.
- Do not widen into lore/content or giant AI doctrine.
- Keep existing cannibal adopter/no-extort checkpoints closed and build on them.
- Stop at deterministic/code proof unless the active canon separately greenlights a live/harness product scenario.

## Current classification

Active / deterministic substrate landed.

The first implementation packet landed deterministic/code support for cannibal pack-size dispatch, smoke/light nearby-lead classification, explicit darkness/concealment local-gate input, exposure hold-off, high-threat abort, no-extort/no-shakedown reporting, and multi-member active-group save/load proof.

This is not live/harness night-raid product proof. A later promoted product slice must still wire real time/light/visibility into the live path and prove a named cannibal night-raid scenario through dispatch/local-contact under proof-freeze rules.

Josef requested this as the next cannibal vision/audit packet on 2026-04-28. The prior harness trust-audit/proof-freeze lane is checkpointed/held rather than deleted, and the bandit camp-map stack remains greenlit behind this unless Schani/Josef promote it separately.
