# Bandit extortion playthrough audit + harness-skill packet v0 (2026-04-22)

Status: checkpointed / done for now.

## Why this exists

Even with a real extortion scene and a real restage point, the job is still half-baked if only one person remembers the right incantations.
Andi should be able to drive the whole chain deliberately:
bring bandits in, hit the first demand, choose pay or fight, observe defender loss if it happens, and play through the reopened higher-demand scene with the second reconsideration fork.

This packet exists to package that full playthrough surface into named audits plus skill/docs updates so the knowledge does not stay trapped in chat or one operator's head.

## Scope

1. Land one named full-playthrough audit packet for the Basecamp extortion chain.
2. Package a reviewer-usable path that can exercise at minimum:
   - approach or stand-off arrival at camp
   - first `pay` versus `fight` demand
   - one fight-forward branch
   - one defender-loss / lowered-threat case that reopens the scene with a higher demand
   - the second explicit reconsideration fork where the player can again `pay` or `fight`
3. Keep screen proof, deterministic proof where applicable, and artifact/report proof clearly separated instead of flattening everything into one soup verdict.
4. Update the usable harness-facing documentation so later agents can actually run the path:
   - `skills/c-aol-harness/SKILL.md`
   - `tools/openclaw_harness/CONTROL_LOOKUP.md`
   - any relevant harness docs/readme surface if the packet lands new named scenarios or operating rules
5. Keep the packet focused on making the extortion chain playable/testable/teachable, not on reopening feature design arguments the earlier packets already settled.

## Non-goals

- inventing new robbery mechanics by stealth while writing the audit
- giant fully scripted automation that fakes every combat outcome
- hiding the whole method inside one brittle operator-only macro
- radio warfare, stalker pressure, or broader social-horror packaging
- replacing real gameplay proof with documentation-only theater
- treating missing deterministic hooks as proof that the live scene did not happen

## Success shape

This packet is good enough when:
1. one named audit/playthrough surface exists for the full Basecamp extortion chain rather than only for the first setup moment
2. the packaged flow can cover first demand, fight branch, defender-loss reopen, raised-price second demand, and second `pay` versus `fight` reconsideration clearly enough for review
3. reports separate screen, tests, and artifacts/logs cleanly enough that later debugging does not devolve into folklore
4. the relevant harness skill/docs are updated so another agent can discover the named paths and run them without archaeological guessing
5. the slice stays bounded and does not widen into a fake total-automation empire or a feature-redesign side quest

## Validation expectations

- prefer at least one honest live playthrough packet/report that reaches the reopened higher-demand scene, not just a first-demand smoke test
- if some branches still need human steering rather than full automation, the packet should say so plainly and package the steering steps instead of bluffing repeatability
- the updated harness skill/docs should name the exact scenario/fixture/report surfaces rather than waving toward `the harness somehow`
- the important reviewer question is whether Andi can now deliberately restage and audit the whole extortion chain instead of re-deriving the ritual each time

## Closed evidence packet

Named scenario surface:
- `bandit.extortion_at_camp_standoff_mcw` stages the honest controlled-site stand-off / local-gate moment.
- `bandit.extortion_first_demand_fight_mcw` captures the first Basecamp shakedown demand, then chooses `fight` and captures the hostile-forward result.
- `bandit.extortion_first_demand_pay_mcw` captures the same first fork, chooses `pay`, and saves/copies the world for writeback inspection.
- `bandit.extortion_reopened_demand_mcw` captures the controlled defender-loss reopen tier with the raised second demand and `pay` / `fight` still available.

Current decisive evidence:
- screen: run `.userdata/dev-harness/harness_runs/20260424_162002/` shows the first `Bandit shakedown` menu with `Reachable goods: 45134`, `Demanded toll: 15797`, `p Pay the demanded goods`, `f Fight`, and after `f` the message `You refuse the shakedown. The bandits come at you.`
- current-runtime screen/artifact: stable scenario run `.userdata/dev-harness/harness_runs/20260424_174913/` uses rebuilt runtime `9e3c14260b-dirty` with `version_matches_repo_head=true` and `version_matches_runtime_paths=true`; the screen shows reopened `Bandit shakedown`, `Reachable goods: 45134`, `Demanded toll: 22116`, `p Pay the demanded goods`, and `f Fight`; `probe.artifacts.log` records `shakedown_surface ... pay_option=yes fight_option=yes ... demanded_toll=22116 ... basecamp_inventory=yes`
- deterministic: `./tests/cata_test "[bandit][live_world][shakedown]"` passed with 88 assertions in 3 test cases, covering first pay/fight, aftermath/reopen contracts, one-use raised reopen, and cooled bandit-loss pressure.
- setup/handoff: `.userdata/dev-harness/harness_runs/20260424_153309/` proves `bandit.extortion_at_camp_standoff_mcw` handoff mode leaves the controlled-site stand-off session alive with `cleanup.status = deferred_handoff`.

Caveat: a natural redispatch probe from a persisted defender-loss/no-active-group state (`.userdata/dev-harness/harness_runs/20260424_170908/`) stayed artifact-silent after 6000 turns on the rebuilt runtime.  The packaged second-demand proof is therefore a controlled reopen-local-contact tier, not a claim that the natural redispatch trigger is fully playthrough-clean.  That is acceptable for this audit packet because the goal is teachable reproducibility without a fake total-automation empire.

## Dependency note

This queued packet sits directly behind `doc/bandit-extortion-at-camp-restage-handoff-packet-v0-2026-04-22.md` and depends on the earlier robbery-chain packets being real enough to play through.
It is the audit/teaching follow-through, not the first setup seam itself.
