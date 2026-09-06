# Debug observations and suspected gameplay bugs

Current intake follows the 2026-09-06 WEC. DE67 loop faults go to method mutation.
Harness faults may be diagnosed, repaired and verified by the coordinator, mutator or repair worker.
CAOL gameplay observations are suspected bugs, not authorized findings or repair tasks. Only Josef's
explicit promotion authorizes a finding, followed by a mutator DFS update and fix/retest plan.
Classify the failed responsibility rather than the source filename; uncertainty remains explicit.

Record expected versus observed behavior, exact run/artifact/source evidence, affected and unaffected
tests, evidence ceiling, blocking consequence and current disposition. Keep the original observation
and any later owner decision, correction and verification distinct. Continue independent tests while
waiting; preserve blocked tests and report all-blocked work as awaiting the owner, never complete.

## Current triage index — consolidated 2026-09-06

This index contains unresolved observations. Verified registry/build/bridge resolutions were
removed in review 78843add986c; the review artifact and Git retain their evidence. No entry here
authorizes a gameplay repair.

| Record | Current reading | Next evidence needed |
| --- | --- | --- |
| R029-F001, roof-fire run `e89a4ea4` | Source-bound smoke/light rejection was observed at camp `(140,51,0)` for source `(140,41,1)`. Whether the intended nearby eligible observer was exercised remains unproved; rejection alone does not establish a defect. | Establish intended camp identity, eligible observer and actual visibility/range before judging detection. |
| R029-F001, sound run `1ceb9ded` | Corrected to inconclusive: the observed recipient was the distant camp. No missing-sound gameplay defect established. | Resolve intended nearby camp roster/readiness and stimulus eligibility. |
| R031-F001, follow run `dc908a45` | Inconclusive follow-travel test: acknowledgement/application and no NPC displacement are recorded, but the short route did not establish a meaningful pursuit requirement. | Native-order one-OMT repeat now proves pursuit in its prepared fixture (`review-owner-78843add986c/follow-final-trajectory.json`); the original non-camp free-text-to-eligibility route remains unresolved. |
| R026-F004 | Historical mixed camp-membership, ambient-recipient and relationship-snapshot observation; these are distinct questions. | Reconcile camp membership separately from ambient identity/relationship evidence. Later camp success does not settle ambient behavior. |
| R026-F006 | Historical addressed craft-parsing observation; current repair/retest status is not established by this record. | Check current parsing and a matching craft-state/persistence witness before treating it as open or fixed. |

The two R029-F001 headings below reuse a historical ID for independent experiments. Cite the
experiment and exact run alongside that ID; do not combine their stimuli or verdicts. The old
stopped R-026-exploration-002 launch produced no new gameplay observation.

## R029-F001 — source-bound staffed observer rejects the persisted roof fire

- Date: 2026-09-06
- Bound route: scenario `cannibal.r029_natural_route_roof_mcw`, run
  `e89a4ea4e587d7915a44d00d3b71bd8754dccb9805d3c8fc5a647d02ebb08948`, binding
  `7b0b80aee7f37d97852eef4c3abd0ab4efcee59ff91e621d62e5875e4fa3d93c`, source
  `2d06b28f54639940e14836d5be2a9aa193db355ba5ebbebb24964a1222a6f8dc`, executable
  `8b627165011c76dba636f30b6844695c4587378edc91f1c4fc7863e19db5ff15`.
- Expected: after the ordinary UI-created, persisted `f_brazier`/`fd_fire` and the exact two-tile
  north retreat, the first eligible at-home staffed observer at the following five-minute cadence
  should produce a source-bound lead and camp-memory transition for the configured natural route.
- Observed: the native keep-watch reached minute `8230` with 27 accepted receipts.  The run-bound
  transition records `29` and `30` in
  `.userdata/dev-harness/harness_runs/20260906_150231_d041c9f38fc944d0a0609d0b6db68197/transition.events.jsonl`
  identify observer `4`, camp `overmap_special:bandit_camp@140,51,0`, and source `(140,41,1)`.
  Smoke was rejected as `blocked_line_of_sight`; light was rejected as `out_of_range`.  Both retain
  `lead_id=""`, `lead_count_before=0`, `lead_count_after=0`, and idle camp response, despite the
  production-channel receipts for source-bound local smoke/light at minutes `8225` and `8230` in
  `r008.production.channels.jsonl`.
- Affected claims/tests: the R-029 first staffed-observer, camp-memory, scout/report, and later
  cannibal-response claims; `tools/openclaw_harness/r029_natural_route_recipe_test.py`'s intended
  post-relaunch natural-route path.
- Explicitly unaffected: source/executable binding; native physical-fire creation; guarded retreat;
  native Save and Quit, original-process exit, and relaunch; the wait-completion repair and its
  focused counterexample tests.
- Evidence ceiling: source-bound production observation and native receipt evidence.  This is a
  suspected CAOL gameplay contradiction, not authorization to change gameplay.
- Disposition: preserve this first divergence and await owner direction; no gameplay repair made.

## Retained unresolved evidence

The dated entries below retain their original observations and dispositions. Their old finding labels
and proposed fixes do not establish Josef's promotion under the current contract. Consult current
source before assuming a listed repair remains missing; retain prior valid work and its evidence.



## R026-F004 — current living-NPC route has camp and ambient-identity divergence

- Date: 2026-09-03
- Bound run: source-bound registry run
  `5899e6957e17aef803a3848660c8b2b9ecfe6ff0b7110c7e0ea0dc26bf1c735b`, executable
  `cataclysm-tiles` SHA-256 `1162d79dcb73421f288941ce668ffa6f6b59e60ac4aadb99aadeb5d466f2dc16`,
  and source receipt
  `.userdata/openclaw_harness/source_bindings/cataclysm-tiles-9c12cd9305daf8f1.json`.
  The direct/ambient native action receipts are in
  `.userdata/openclaw_harness/r026_living_npc_session_bound_20260903/responses/`; current LLM
  prompt, snapshot, response, and routing records are in `config/llm_intent.log` and
  `config/llm_intent_events.log`.
- Observed defect: the direct Katharina request records `uses_basecamp=no`, `camp_found=no`, and
  `assigned_camp=none`, even though the same current native world frame exposes Basecamp Food,
  Locker, and Storage zones.  Separately, the unaddressed utterance is routed to Giuseppe Bachman
  (actor id 4), who is not in that frame's visible entities, while the resulting Giuseppe snapshot
  calls visible friendly Katharina and Robbie hostile.
- Affected claims: basecamp request context; ambient recipient identity; snapshot relationship
  fidelity; gameplay feel for ambient speech; and persistence of any camp operation that would
  depend on that request route.
- Explicitly unaffected: the accepted current string-prompt owner; the direct follower free-text
  utterance/response; the accepted current ambient request/response; the current executable
  binding receipt; prior prepared-base startup footing; R-027; and all standalone claims at their
  own ceilings.
- Evidence ceiling: current mechanical and product-observation evidence only.  The reply is real
  LLM-run output (`use_api: true`), but an offscreen speaker and contradictory relationship
  snapshot cannot establish a coherent ambient/basecamp gameplay result.
- Disposition: inspect fixture-to-basecamp membership and ambient-target eligibility before
  attempting save/reload or camp-operation credit.  Keep the logged direct and ambient response
  receipts as independent positive observations; do not substitute zone presence for camp routing.

## R026-F006 — addressed camp craft utterance reaches camp routing but is not parsed as a craft order

- Date: 2026-09-03
- Bound run: `f74aa9800e2c9a09d4aa96251702506a9d3fd0d9f26150084039d8a8394e1087`, bridge binding
  `70540eca55418d7cded979e82ee607871af86fd6b370ad8a3aba89594049b314`, report
  `.userdata/dev-harness/harness_runs/20260903_055636_928be2ec3ff04c2888f0f0cbb9c374a4/probe.report.json`,
  and current source receipt `.userdata/openclaw_harness/source_bindings/cataclysm-tiles-9c12cd9305daf8f1.json`.
- Observed defect: the descriptor-native World/chat/menu/string-prompt sequence accepted every
  current request, and the event log records Katharina as `uses_basecamp=yes`, `camp_found=yes`,
  `assigned_camp=140,41,0`, `reason=camp_grouped`, then `camp heard` for `Katharina, craft a
  bandage for the camp.`  The leading direct address remains in the utterance presented to camp
  craft parsing, so no durable craft request is queued and ordinary LLM handling follows.
- Affected claims: camp craft state change and save/reload continuity only.
- Explicitly unaffected: source/executable binding, native descriptor authority, fixture camp
  membership, visible-friendly actor fidelity, the native request receipts, and accepted cleanup.
- Evidence ceiling: current mechanical routing evidence; not a camp-operation or persistence proof.
- Disposition: normalize the selected listener's direct-address prefix before camp request parsing,
  rebuild/rebind, then independently submit the same route, quicksave, and relaunch the saved world
  to verify the new request record.

## R029-F001 — sound experiment is inconclusive about the intended nearby observer

- Review correction, 2026-09-06: the original negative interpretation is superseded at its evidence ceiling, not erased. Exact original record: `.de67/state/review-owner-ffc9168450f6/before-debug-findings.md`; durable receipt `1cb7b56d1eb471a139c5bc017ab3818a137955dbba5815101063b10679c30f55` remains unchanged.
- Bound run `1ceb9ded123614f2fbe6b32982ad7e05077843a9fa774aef94a1dc002605f017`, binding `32243b4c7c414b6081c1962403c19387a3981387dab6e65859bf67d7fdbeb72f`, session `.userdata/openclaw_harness/bridge-sessions/selected-r029-bandit-sound-near-046`.
- Preserved observations: reload 0/6 to 6/6, two shots, and staffed callbacks at 8225/8230 with `no_signal_source`, zero reads and no lead. Native callback records retain hashes `87ee3d364b7f81d79d52c5055950eea6abd3e59cbec69e75c401c53e242968cf` and `bb77ebe9ca2cb6ed607ef5f06e34588f53c17f2f2cca99df3063228a49fccffe`.
- Both callbacks name camp `overmap_special:bandit_camp@140,51,0` and observer 4, home `(3371,1230,0)` absolute map squares. Run-bound World frames put the player at `(3372,996,1)` / OMT `(140,41,1)`. The observed camp is ten OMT away with a vertical difference of one. With SEEX=12 the adapter requires effective volume `(10+1)*24-1+120 = 383`. The intended nearby footprint's calculated threshold 59 does not describe this recipient.
- The five-minute callbacks did execute. `no_signal_source` is an empty adapter result and can include inaudible/rejected sounds. Retained run debug at 15:45:07.595 and 15:49:54.215 reports `significant_sounds=1`, `sites=2`, `eligible=1`, `callbacks=1`; it supports investigation, not attribution of every shared-log record to this run. Exact native frames and callbacks are retained under `.de67/state/review-owner-ffc9168450f6/`.
- Supported conclusion: native shots and evaluation of the distant staffed camp are proved; an eligible nearby recipient was not established. The reason that intended nearby camp was not selected is unresolved. Investigate its actual site/roster/readiness, then source age, hearing, weather and range. No missing-sound product bug, automatic gameplay repair or promotion follows from this experiment. Earlier roof-fire observations remain independent. The natural route remains actionable repository-owned proof work, not automatically blocked on owner bug promotion.

## R031-F001 — short follow-close route leaves physical pursuit inconclusive

- Date: 2026-09-06. Current status: inconclusive test, not an established gameplay contradiction. The original suspected-bug interpretation is superseded by the test-validity correction below; retained witnesses remain unchanged. This record does not authorize a gameplay change.
- Bound run: `dc908a45aeda578448c2d67c8b47f48a6230b7f6eee55070ab4fb9cc37511a15`, bridge binding `79bc0d97950ed454bbe876bb1e52c2a32e7385eee56d90fed42c9d636c777ab9`, session `.userdata/openclaw_harness/bridge-sessions/r031-follow-sealed-002`, and focused witness `.userdata/openclaw_harness/bridge-sessions/r031-follow-sealed-002/r031-follow-witness.json` SHA-256 `ba6756ab39002cd4d9f586314f4c599b496f82dda6d435500898fc3513525190` (mechanically valid receipt witness SHA-256 `7a7693a1eaa4db47bbf6fff0e35094be6fd86e81032c29c3206c2f6b35a022ca`). Runtime source identity was `7df9c068421a6da8394fdea3e203eb91d6ebbac862e5bf63db34363e498064f9`; product source identity was `4f6921e9f99e0808045514bd570a40ed220ccde9a9d423e1609bde291d6a053e`; executable `cataclysm-tiles` SHA-256 was `382e1f311107d023d4fd7a524da603a529b376e65f4fff15cc33ba219c6a84e5`.
- Expected behavior: a fresh non-camp `GUARD_ALLY` who accepts “please follow me closely” should physically travel toward the player when the player separates, so the actor becomes or remains close rather than holding the old square.
- Observed behavior: R031 Route Ally 1 (`character:18`) began at `[3374,994,0]` with `attitude=Ignoring`, `mission=GUARD_ALLY`, no assigned camp, and the player at `[3372,996,0]`. The native request `play-e8dee279e57148279d42e77425c99897` submitted `R031 Route Ally 1, please follow me closely.` The runner returned a named affirmative reply; applying turn `play-af32ebf1a2c74c7497ec80a7ee49b91b` advanced `5241594 -> 5241595`; subsequent native output said `R031 Route Ally 1 begins to follow you.` The final actor inspection also showed `follow_close` enabled. Yet the actor stayed at `[3374,994,0]` throughout. The player finished at `[3374,997,0]`, a final Manhattan distance of 3.
- Ordinary-turn witness: three `world.pause` turns were observed — the applying turn `5241594 -> 5241595`, then `5241597 -> 5241598`, then `5241599 -> 5241600` — with three intervening player movement turns (east, east, south). This is a focused non-travel observation over that exact sequence, not a general claim about every later follow route.
- Affected test: R-031 fresh non-camp follow-travel proof. The acknowledgement, native application, rule state, and spoken follow-start message cannot receive physical follow-travel credit while the observed actor does not move.
- Test-validity correction, 2026-09-06: the player moved east, east, south and ended at Manhattan distance 3; the record does not establish that this exceeded the applied follow distance or otherwise required pursuit. No NPC displacement is an observation, not sufficient evidence of broken following. Verify eligibility, actual separation and elapsed game time in an informative continuation or fresh test.
- Blocking consequence: physical follow travel remains unproved. This inadequate test does not by itself create an owner-promotion blocker; further authorized follow testing remains available. Stay/guard holding, ambient/camp routing, persistence and package closure retain their independent evidence requirements.
- Cleanup: explicit terminal cleanup terminated PID `32798` with `SIGTERM`; `native_exit_credit=false`. Cleanup is not native-exit credit.
