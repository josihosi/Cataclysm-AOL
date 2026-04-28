# Bandit live product playtest matrix v0 (2026-04-28)

Status: GREENLIT / BANDIT PRODUCT-PROOF MATRIX

Product source of truth: `doc/bandit-camp-map-ecology-source-of-truth-2026-04-28.md`
Risk/reward contract: `doc/bandit-camp-map-risk-reward-dispatch-planning-packet-v0-2026-04-28.md`
Actual playtest stack: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`
Proof-freeze matrix: `doc/c-aol-harness-proof-freeze-matrix-v0-2026-04-28.md`

## Purpose

The live/playtest push now covers the bandit work too: bandit behavior needs audited product playtests, with every setup input and conclusion proven instead of inferred from a friendly harness report. This matrix turns the bandit camp-map/fire/signal/playtest obligations into named proof scenarios.

The product picture is: hostile camps read their own dirty map, notice value before danger, send eyes, remember what those eyes learn, size later action from live roster/risk/reward, and let the player feel buildup instead of spreadsheet murder.

## Global proof rules

Every scenario must record:

1. scenario name, run id, repo/runtime version, profile/save/fixture provenance;
2. preconditions: bandit camp id/location, camp roster, at-home/outside members, target/basecamp location, current signal/lead state, time/weather/light when relevant, and player/basecamp visibility setup;
3. action ledger: exact harness primitive, in-game keypress/action, wait duration, debug/setup transform, or save/load step;
4. immediate proof: screenshot/OCR or exact metadata/log/report field for each meaningful step;
5. failure rule: what makes the step yellow/red/blocked and whether later steps are allowed to continue;
6. final artifacts: report JSON, step ledger, screenshots, debug log excerpts, saved-map/overmap/NPC metadata, local-gate/dispatch reports, and route/member-state fields;
7. verdict: passed / failed / inconclusive, without feature-proof credit after a non-green prerequisite.

Load-and-close remains startup/load proof only. Deterministic evaluator tests remain support only. Product proof requires the running game path or an explicitly labeled fixture-backed setup whose limits are visible in the report.

## Scenario matrix

### 1. Player-created fire/smoke/light creates a bandit lead through the real action path

Claim: a normal player action chain can create real `fd_fire` / smoke / light state, and the bandit live-world path can observe it as a bounded lead rather than from an authored playback packet.

Required proof:
- player has live-accessible fuel/fire tools, not only legacy save inventory rows;
- deploy/fuel/light actions each have green step-ledger rows and target-tile metadata;
- real `fd_fire` / smoke / light state is visible in saved-map metadata or claim-scoped artifacts;
- bandit signal/log/report names source, strength, threshold, weather/light modifiers, matched site/camp, and refresh/lead result;
- if the fuel gate is still red, the scenario is blocked and must not continue to lighter/fire/bandit credit.

### 2. Scout memory two-OMT watch writes per-camp map knowledge

Claim: a bandit scout observes a camp/basecamp target from the new two-OMT envelope, watches for the bounded half-day window or a classified interruption, returns home, and writes a remembered lead to the source camp's own map.

Required proof:
- initial camp and target positions are proven;
- scout stand-off distance is two OMT, or fallback distance/reason is reported;
- wait ledger proves elapsed watch time or classified interruption;
- scout return/home-footprint writeback is visible in member state and logs;
- source camp map records lead fields: target, bounty, threat, confidence, age/last-seen, source/outcome, target-alert/scout-seen state where implemented;
- another camp's map does not accidentally receive the same lead.

### 3. Vanished-signal redispatch/planning from remembered camp-map lead

Claim: after the original live signal disappears or is absent, a later live dispatch cadence can still plan from the remembered scout lead instead of forgetting the target.

Status: **green feature-path evidence** in `.userdata/dev-harness/harness_runs/20260428_185947/` via `bandit.camp_map_vanished_signal_redispatch`: saved preflight proved the remembered lead exists, the long-wait ledger proved bounded 30-minute menu/cadence action plus all configured post-wait cadence artifacts, same-run logs named `candidate_reason=remembered_camp_map_lead` with `signal_packet=none`, `job=stalk`, `selected_members=4`, `reward=13`, `risk=1`, `margin=12`, `reserve=5`, `dispatchable=8`, and saved state persisted an active stalk dispatch with four concrete active members. This closes this guardrail only; it is not full variable-roster/no-opening/repeatability closure.

Required proof:
- precondition proves the remembered lead exists and current live signal is gone/absent;
- cadence/tick/wait step is proven, not assumed;
- decision report says memory/camp-map lead drove the decision;
- selected intent/job/member count/reserve/risk/reward fields are visible;
- if the correct decision is hold/re-scout rather than attack, that is green only when the report explains why.

### 4. Variable-roster dispatch sizing through live path

Claim: tiny, medium, and large camps size action from current living/ready/home roster, reserve, wounds/unready state, active outside groups, risk/reward, and target confidence rather than fixed folklore.

Status: **partial green feature-path evidence** for roster-size dispatch sizing across 2/5/10-member variants: tiny run `.userdata/dev-harness/harness_runs/20260428_193433/` via `bandit.variable_roster_tiny_dispatch_sizing_live`, medium run `.userdata/dev-harness/harness_runs/20260428_192059/` via `bandit.variable_roster_dispatch_sizing_live`, and large/cooled run `.userdata/dev-harness/harness_runs/20260428_193621/` via `bandit.variable_roster_large_cooled_dispatch_sizing_live`. Together these prove fixture-shaped roster footing, roster/reserve/dispatchable reporting, selected scout/stalk member count, and saved ready/active roster split for tiny, medium, and large/cooled cases. This still does **not** close the full row: live high-threat/low-reward and active-outside dogpile guardrails still need named evidence.

Required proof:
- fixture/precondition proves roster shape for at least 2-, 4/5-, and 7/10-member cases over bounded runs or variants;
- report names living roster, ready-at-home, wounded/unready, active outside, hard reserve, dispatchable count, selected intent, and selected member count;
- high-threat/low-reward case holds/re-scouts instead of escalating just because threat is high;
- active outside same-target group blocks dogpile;
- reserve is preserved or downgrade is explained.

### 5. Stalk/pressure waits for opening, then returns/holds if none appears

Claim: a follow-up stalk/pressure group can be larger than the scout where justified, waits for an opening rather than instantly fighting, and returns/holds/decays if no opening appears within the bounded window.

Required proof:
- remembered high-value/manageable-risk lead exists;
- selected stalk/pressure group has member count > scout where roster allows;
- opening state is named and initially absent or present;
- wait ledger proves the bounded observation window or classified interruption;
- no-opening result returns/holds/decays instead of hovering forever;
- opening-present result can escalate only with matching report evidence.

### 6. Scout/stalker sight-avoidance through live local turns

Claim: a scout/stalker exposed to player or basecamp NPC vision reacts with normal movement/pathing, hold, or abort by at most two visible local turns; no teleporting is credited.

Status: **green feature-path evidence** in `.userdata/dev-harness/harness_runs/20260428_173626/` via `bandit.scout_stalker_sight_avoid_live`: saved active scout/member footing, saved turn delta >= 2, same-run local-gate proof, and claim-scoped `sight_avoid: exposed -> repositioned ... distance=1` all matched. This closes this guardrail only; it is not the full remembered-lead/redispatch chain.

Required proof:
- before screenshot/metadata proves exposure state and approximate position;
- one- and two-turn observation steps are ledgered;
- after screenshot/metadata/log proves move/hold/abort or names blocked/no-cover/path-fail;
- repeated exposure raises target-alert/caution when that field exists;
- visible continued beeline pressure is red/inconclusive, not green.

### 7. Shakedown/toll control remains player-visible and bounded

Claim: ordinary bandit toll/shakedown contact still produces the expected pay/fight surface only for bandits, remains separate from cannibals, and reports reachable goods/demand/posture without silent extortion through the wrong profile.

Required proof:
- bandit contact path reaches the local demand surface with `pay` / `fight` where expected;
- report/log names reachable goods, demanded toll, profile, posture, and chosen branch;
- pay and fight branches are separate runs or clearly separated steps;
- no cannibal proof borrows this result.

### 8. Empty-camp retirement live sanity proof

Claim: a camp retires from active AI calculations only when both home/inside presence and outside/active pressure are empty; one-side-empty cases stay active.

Required proof:
- setup proves home-side empty or populated and active-outside/contact state;
- same run proves the fully inactive positive case and at least one negative with home or outside still populated;
- report/log names `retired_empty_site` or equivalent predicate and the blocking reason for negative cases;
- deterministic empty-camp tests remain support, not live proof.

### 9. Repeatability / fixture-bias check

Claim: at least one passing bandit product scenario is not a one-fixture miracle.

Required proof:
- rerun the smallest equivalent scenario or a second bounded fixture variant;
- record what changed and what stayed equivalent;
- if the second run fails due to fixture/setup, classify fixture bias honestly and do not erase the first run, but do not claim broad reliability.

## Preferred execution order

1. Preserve/repair any still-red prerequisite gates before dependent proof: especially real player fuel/fire if testing player-created fire/smoke/light.
2. Run scenario 2 scout memory two-OMT watch.
3. Run scenario 4 variable-roster dispatch sizing.
4. Run scenario 5 stalk/pressure wait-for-opening.
5. Run scenario 7 shakedown/toll control regression.
6. Run scenario 8 empty-camp live sanity only if the seam is still desired beyond deterministic proof.
7. Run scenario 1 player-created fire/smoke/light only after fuel/writeback is green; otherwise keep its blocked verdict explicit.
8. Run scenario 9 repeatability after the first passing scenario.

## Stop conditions

- Stop immediately when a prerequisite step is red and later steps would launder it.
- After two same-blocker attempts, consult Frau Knackal before attempt three.
- After four unresolved attempts, write the implemented-but-unproven packet for Josef and move to the next explicit greenlit target.
- Do not widen into unrelated faction war, global map AI, cannibal behavior, or basecamp job work while proving bandit behavior.
