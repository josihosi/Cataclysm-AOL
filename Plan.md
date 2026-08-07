# Plan

Canonical roadmap for Cataclysm-AOL. This file states the active outcome, sequence, constraints,
and next milestone. Detailed mechanics live in tests and `TechnicalTome.md`; completed receipts
live in Git history and `doc/work-ledger.md`.

## Active outcome

`CAOL-HOSTILE-CAMP-OVERMAP-ECOLOGY-v0` is active on `dev`.

Goal: naturally generated bandit and cannibal camps send coherent two-person scouts that discover
targets through honest perception, travel and stalk physically, withdraw coherently when exposed,
return physical reports, and produce faction-specific shakedown or night-raid consequences. Josef
and GPT-5.6 must be able to observe the authoritative state and collect compact causal evidence
without save archaeology, OCR dependence, or a parallel simulation.

Production posture:

- Implementation and Mac verification stay in the isolated `dev` worktree.
- Clean `port/cdda-master` at `660057ff728bdf77531f607b1bd42a175f027a5f` remains the playtest
  candidate until a reviewed integration milestone promotes a newer commit.
- Integration remains `dev` -> `master` -> `tools/porting/orchestrate_ports.ps1`.
- No upstream refresh, push, release, or direct candidate edit belongs to the current lane.

## Vertical sequence

1. Debug/harness observability: one `DEBUG_CLAIRVOYANCE`-gated authoritative ecology view serves
   overmap UI, compact JSON, watches, incident capture, and clearly labelled interventions.
2. Natural scout lifecycle: real camp -> exact pair -> discovery -> travel -> stalk/watch -> burn
   or quiet completion -> egress, with no avatar-coordinate radar.
3. Physical return/report/decision: real survivors carry only their evidence home; missing/dead
   scouts do not leak dossiers; the camp consumes the final report into one decision.
4. Bandit outcome: the decision produces a physical shakedown loop with authoritative roster,
   contact, payment/fight, casualty, return, and writeback.
5. Cannibal outcome: the decision produces a physical night-raid loop with equivalent ownership,
   casualty, return, and writeback guarantees.
6. Qualification: CPU, retained allocation/memory plateau, scheduler fairness, save/load wall time
   and growth, load/unload, representative platform runtime-save packets, `UltimateCataclysm`,
   standalone package/data launch, secure-store runner handling, artifact manifests/platform
   selection, and release-candidate integration are green at the required scale.

`SUCCESS.md` is the only active success-state ledger for these outcomes.

## Current checkpoint

The authority, persistence, roster, paired-route, fairness, finite-resource, perception,
single-writer, physical-return, covert-watch/burn/egress, report assessment, response selection,
and denial foundations are checkpointed. Overdue total loss is green at `a46897c637`: it produces
only one bounded camp-to-target missing-route mark at the persisted deadline and cannot import the
absent party's dossier, cargo, or report.

The comfort-first observer/editor v0 is usable:

- `DEBUG_CLAIRVOYANCE` is the sole gate; normal starts are unchanged.
- Camp/dispatch markers, filters, selection, pin/follow, cursor detail, compact JSON, transition
  deltas, six watches, and incident capture share one read-only authoritative projection.
- Performance/save neutrality is green at `117857f551`.
- The first authoritative loaded-member wound/heal/kill path and both-faction casualty
  reconciliation are green at `1e6a0924e7`; field run `20260805_101713` preserves the screenshot /
  incident pair and explicit debug provenance.
- Mobile horde/stalker adapters remain disabled because no durable movement/load-transfer identity
  exists; do not invent position IDs or a parallel registry.

The long implementation receipt was compacted in
`doc/hostile-camp-overmap-ecology-implementation-ledger-v0.md`. The observer product contract
remains in `doc/ecology-observer-editor-roadmap-and-success-state-v0-2026-08-05.md`.

The first live return blocker from run `20260807_015545` is repaired at the loaded-owner seam:
an already-homeward pair now validates and repairs stale/non-covert camp routes before the
target-facing local gate exits, while a deferred homeward phase transition still skips target
behavior. The focused real-NPC regression and adjacent Mac NPC/bandit slice are green; the same
observer contract must now prove physical return/report/decision in play.

The two post-fix observer attempts (`20260807_030535`, `20260807_031409`) were both stopped by
ordinary non-contaminating Shadow story popups during waits. That OCR footing is capped. The
harness now emits an opt-in bounded structured event around the real EOC popup call and feeds its
message through the existing fail-closed interruption classifier; normal starts emit nothing and
unknown, contaminating, release-blocking, and truncated events remain blocked. Deterministic
in-process proof now drives a production-planned real inactive pair through ordinary overmap NPC
travel, camp arrival, transactional dematerialization, authoritative roster return, final report,
and camp-decision acceptance without post-boundary outcome writes. The focused Mac regression is
green at 1 case / 50 assertions. This closes the isolated owner seam but does not credit either
live success row; a new structured-observer footing must join the already-natural first half to
this physical second half in one incident.

Run `20260807_041806` on clean `38ab88e8cf+SDL3` naturally reached dispatch, local handoff,
cohesion, observer selection, and the final six-hour watch. The structured trace correctly
identified and closed a safe Shadow popup, then exposed the separate authoritative activity
distraction query that OCR only partially recognized. That run is retained as non-credit evidence.
The same opt-in channel now traces the real activity-query open/returned action and permits only
the existing `IGNORE` response for a complete known-type query when OCR is clear/unobservable or
recognizes that same partial prompt; unrelated unknown, malformed, stale, and truncated inputs
remain fail-closed.

Run `20260807_043746` on clean `5010d98e3d+SDL3` naturally reached exact dispatch and committed the
two-person local handoff, then exposed a gameplay ownership defect: the structural scout pair
opened the bandit Pay/Fight shakedown dialogue before physical return, report, and camp decision.
That footing has exhausted its two attempts and receives no success credit. The bounded repair
requires explicit `toll` intent from the authoritative external outing and, for the new operation
owner, an actual shakedown operation kind; ordinary scout contact remains a probe.

## Next milestone

Produce one observer-backed vertical proof from a real naturally generated bandit scout
through physical return, final report, and camp decision. Begin from a declared pre-transition
fixture boundary, select the real dispatch in the ecology observer, use the existing wait/watch
flow with structured popup receipts, and preserve one compact incident chain. The lifecycle row is
not credited at egress; the same artifact must continue through a surviving physical return,
report, and decision.
Repair only a gameplay or tooling seam that directly blocks this proof.

The exhausted east footing was `bandit_scout_to_decision_observer_east_v0_2026-08-07`. It inherited
the same idle zero-lead camp, deterministic clock, road, and observer gate, then moved only the player by
96 map squares from OMT `(162,35,0)` to `(166,35,0)`: two OMTs east rather than west of the local
handoff waypoint. Discovery, dispatch, observation, return, report, and decision remain unwritten
at the fixture boundary. This is a new loaded-bubble geometry, not a third attempt on the exhausted
west footing.

Both east-footing attempts are now exhausted. Runs `20260807_052130` and `20260807_053354`
naturally discovered, dispatched, and committed the same pair locally, but produced no local route
attempt before the ten- and twenty-minute deadlines respectively. Extending the deadline was
disproved and reverted. The abstract owner can see both NPCs through the overmap buffer while the
local motor cannot address them in the active reality bubble; local ownership must not commit at
that boundary. No further east run is allowed until a deterministic ownership fix is green, and a
later live proof must use a materially different footing.

The local ownership repair is now green in a deterministic production-seam regression. A route OMT
that intersects only the map's non-activating fringe leaves both real NPCs inactive and preserves
the abstract outing byte-for-byte; the adjacent geometry selects load-addressable slots, commits
the complete pair, and activates both through `game::load_npcs`. This is ownership proof, not live
lifecycle credit. The next proof contract must choose a new observer footing and retain the same
natural discovery-to-decision causal boundary.

The current footing is `bandit_scout_to_decision_observer_northeast_v0_2026-08-07`. It derives
directly from the original zero-lead source and applies exactly one declared player transform,
`[96,-24,0]`, placing the observer at OMT `(166,34,0)`: two OMTs east and one north of the handoff
waypoint `(164,35,0)`. It leaves the observer off the x=164 route, changes both loaded-bubble axes,
and writes no lead, dispatch, observation, return, report, or decision. The scenario's existing
evidence contract remains the run contract; all waits, authoritative audits, and pass/fail rules are
unchanged.

Its first run, `20260807_060258` on clean `de9b904389+SDL3`, naturally reached discovery,
dispatch, local handoff, observer selection, and the final observation window. The pair then
oscillated between `assembled=yes` and `assembled=no` before aborting into `returning_home`, so the
run ended `red_wait_completion_artifact_timeout` with no lifecycle credit. The defect was in the
authoritative local owner: it re-required exact staging tiles after the one-time rendezvous had
already succeeded. The current slice latches that success while the living pair remains cohesive
on its owned route; real separation or leaving the route still restarts bounded assembly. Focused
production-seam and pure-owner tests, including unload back to the abstract owner, are green. This
left one northeast live attempt, consumed by the final run below.

The second and final northeast run, `20260807_063000` on clean `7e92528d87+SDL3`, reproduced the
same visible failure after the latch/writeback repair: the exact pair assembled, but ordinary NPC
turns alternated `assembled=no/yes` until the ten-minute abort and `returning_home`. The footing is
exhausted. The concrete missing seam is motor ownership, not another latch: an assembled outbound
pair at its selected observation OMT receives only a soft AI guard, so each scout may wander away
independently. The next slice keeps hard staging motor priority while that local pair remains
non-homeward; a real observation/return phase must still release it through the existing homeward
path.

The motor-hold repair is checkpointed at `b14b9c9ca9`: assembled ordinary non-homeward pairs keep
their exact staging orders, while homeward, abort, and pending alternate-watch owners release them.
The same-turn motor cache refresh follows assessment transitions. Focused Mac evidence is green at
3 cases / 878 assertions, and the exact clean binary is `b14b9c9ca9+SDL3`.

The next materially distinct footing is
`bandit_scout_to_decision_observer_southeast_v0_2026-08-07`. It derives directly from the original
zero-lead source and applies exactly one declared player transform, `[96,24,0]`, placing the
observer at OMT `(166,36,0)`: two OMTs east and one south of waypoint `(164,35,0)`. It changes the
loaded-bubble y-axis to the camp side of the waypoint while remaining off the x=164 route, and it
writes no lead, dispatch, observation, return, report, or decision. The scenario's waits, audits,
proof rules, and causal boundary remain unchanged.

Its first run, `20260807_070159` on clean `f09ac3882f+SDL3`, naturally reached discovery,
dispatch, local handoff, observer selection, and one stable `assembled=yes` transition without the
previous cohesion oscillation or abort. This is live evidence that the motor-hold repair works at
its layer, not lifecycle credit. The visible six-hour wait completed, but the step timed out because
it inherited one-shot dispatch/handoff artifact patterns that were already behind the audit cursor.
The harness now owns that wait endpoint with the exact later maintenance markers
`scheduler_hour=142` and `now_minutes=8520`; downstream authoritative audits still own return,
report, and decision. One southeast attempt remains after this tooling checkpoint.

The proof contract must name claim, preconditions/interventions, causal boundary, production owner
path, expected transitions, control, deadline, and commit/binary/scenario/tool identities before a
new run. Structured state supersedes OCR. Fixture transforms may establish preconditions but may
not pre-write discovery, return, report, or decision.

Do not resume helper-by-helper Phase-6 expansion. A visible failure in this vertical path chooses
the next code slice. The capped smoke/light/sound, decoy, and visible-burn probes already consumed
two attempts each; retain them for Josef's disposable playtest packet and do not rerun them merely
to seek green artifacts.

## Constraints

- Bandit and cannibal ecology only. Writhing-stalker behavior, zombie-rider behavior/progression,
  and flesh-raptor behavior are out of scope.
- Authoritative owners remain the only gameplay truth. No map notes, fake sightings, duplicate
  registries, direct discovery switches, teleporting parties, arbitrary phase setters, or saved
  overlay state.
- Debug interventions require confirmation, before/after receipts, `debug_intervention=true`, and
  stale-selection rejection. Later natural consequences remain distinguishable from the edit.
- Closed observer work must be effectively zero; open queries stay bounded. Rich detail is selected
  first, and transition-only tracing uses the existing console ring/export machinery.
- Keep Windows, Linux/WSL, and macOS compatibility. Claim only the platforms actually built or run.
- Use a GPT-5.6 Terra / Medium subagent only for a bounded coding job with a small owned file set,
  explicit interfaces, acceptance checks, and test command. Lead-owned persistence, atomicity,
  cross-system integration, and uncertain failures remain direct work.
- One implementation plus ledger checkpoint per meaningful behavior slice. Use fresh read-only
  review at phase exits and broad-blast-radius decisions.

## Held decisions

- Refreshing `port/cdda-master` from newer CDDA upstream waits until ecology qualification and
  Josef's product pass.
- Windows free-play and release packaging remain held validation lanes, not active implementation.
- Historical closed lanes are discoverable in `doc/work-ledger.md` and Git history; they are not
  copied back into this active plan.
