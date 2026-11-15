# Package a witness and finish

Journal pages give action citations copyable `checks` objects. Observation rows give INDEX/PATH
for one shared inspect command; inspect only the fields needed for your conclusion. Scalar
inspection prints an exact typed `checks` object, including quotes around native string values.
Copy it directly. There is no need to dump an entire observation or inspect an action again
when its displayed check already establishes the fact you need.

The journal returns citation IDs and exact witness fields. Check paths start inside the cited
entry's `value`: action entries use `action_id`; observation entries have another nested `value`
and use `value.surface.facts.FIELD`. `checks` is an object mapping each path to its exact value,
not a list of paths. Keep the journal's `--request-id` when inspecting after another command.
Preserve the types printed in scalar checks: `false` differs from `"false"`.
For compound values or an explicitly truncated preview, use the supplied full-value retrieval
command. A mismatch reports recorded and supplied types.
The cockpit exposes structured movement/wait macros through `call --request` and diagnostic
retrieval through `inspect`; use those when the proof question needs them rather than reconstructing transport bookkeeping
for ordinary native actions.

After the exact bound native process exits, use `journal --reason REASON` to seal `run.witness`, then `finish --witness FILE` to report. While it is alive, use `inspect`, `evidence`, `messages` or status for evidence. Prefer the advertised native save/quit route when preserving a save. `play.quit` / `run.quit` requests harness termination and can send SIGTERM; its stop reason does not make it a native save. If native close is unavailable, use the [owned-process closure authority below](#owned-process-closeout) when it applies, preserving useful state and verifying exit. Early reporting refuses without revoking native controls. Dead/crashed runs can be reported honestly without a successful save or exit0; historical sealed journals remain immutable. State the smallest conclusion
supported by cited immutable evidence; do not invent facts or promote the evidence ceiling.
Stop reasons and witness text are your own conclusions, not independent game observations.
Reconcile them with the resulting native state and any later messages, including outcomes revealed
when a nested interaction returns to World. When
one run settles independent claims differently, submit a `caol-playtest-witness-bundle-v1`: each
claim keeps its own verdict, while bound product or harness defects name affected and explicitly
unaffected claims. Continue useful observation after a defect when the remaining causal footing is
clean. The coordinator records ordinary defects in `.de67/debug-findings.md`; reserve the durable
capability-gap history for missing reusable observation, action, or setup interfaces.

`registry-record-witness` persists the witness and `registry-review-witness` records the
coordinator's separate causal judgment.

For extraction examples, see [journal citation packaging](searching.md#journal-citation-packaging).

A successful scenario reaches its assigned endpoint with supporting native evidence. Report a
demonstrated gameplay contradiction under valid conditions with its first divergence and evidence,
including any open causal question; ambiguous premises remain suspected. A concrete harness
blocker identifies the failed supported operation and the behavior it prevents. Reporting any
finding does not itself terminate the task or accept a repair; apply the current assignment and
lifecycle rules. “Incomplete” records remaining work, and an owner interruption remains an
interruption. Preserve any repair or retest obligation. See [closing a game](../SKILL.md#closing-a-game).
Report startup, feature outcome, contradictions, evidence ceiling, and cleanup separately.
An already accepted positive outcome needs its supported conclusion and exact retained
retrieval handles, not additional fatal-actor proof, a second witness or reconstruction of final
serializer internals. Keep unresolved participation, defense, camera, legacy and needs questions
explicit at their actual scope. Reuse settled evidence and closeout; do not replay gameplay to
prove a guidance change. This does not turn an unsupported causal claim into a supported one.
## Owned process closeout

Luna performs current advertised native save/quit, reports exact exit/evidence handles and
removes its generated residue when closure and deletion are within its assigned ownership.
It need not wait for a chaperone to perform each ordinary closeout step. If the owner or transport
limits a needed OS check or cleanup operation, return that exact remaining operation with its
PID/birth and run binding; the chaperone performs or routes it once. Do not bypass an actual tool rejection through another utility.
The chaperone remains accountable for closing every completed or abandoned game launched by its
players, including failed starts and replacement generations, and reconciling their exit evidence. Close games, brokers, bridges and
helpers through the ordinary graceful lifecycle, then verify their exact PID/start identities have
exited at the OS level. Worker return, a hidden window, a report or `run.finish` is NOT closure.
Process closure and scenario completion are separate. Before deleting generated run/profile/session
residue, settle whether the unfinished scenario still needs its compatible save and continuation data.
Keep that necessary input under its existing owner with the remaining endpoint in task context;
it is current working data, not a history archive. A host handback can close the game while preserving
that input. Delete finished residue and remove the retained input when its continuation is settled
or disposal is authorized; retain concise commit/run/outcome and reproduction notes. Do not keep
old processes or saves indefinitely or create cleanup backups.

For Mac mini work, Josef authorizes the coordinator and owning player to close or terminate their
own games and helpers, including stuck runs and disposable unsaved test/setup state, without
asking again. Prefer native save/quit when useful; when unavailable, use an appropriate
identity-bound termination route, verify exit and continue recovery from useful retained saves.
Coordinate current input custody and preserve acquired results. Protect unrelated user data,
applications and other workers' active operations; actual tool restrictions still apply.
An intentionally continuing game must have an explicit current purpose and owner, not simply be
left running after its player returns. Native-save proof still requires an actual successful save;
closure or disposal never supplies that proof.

## Performance findings in ordinary playtests

Turns should generally stay below100ms. Occasional exceedances are allowed, but investigate them
and sustained deterioration promptly while the workload and recent changes are still known. Luna
surfaces the observation; the chaperone owns coherent diagnosis, repair and verification, and the
coordinator keeps that responsibility active. Do not wait for another owner prompt or continue
ordinary coverage while treating an unexplained material slowdown as harmless. Preserve current
input and a useful checkpoint; this does not require abruptly closing a game on every slow turn.

Distinguish individual simulation turns, averages over100-turn windows, rendering frames and
external waits. A low average does not explain a spike. Retrieve the useful prior baseline and
compare build, workload, actors, phase and instrumentation; an old fast number is neither forgotten
nor treated as a universal guarantee. Use existing timings and a bounded comparison that separates
actual costs. Inspect changes made by our agents first where relevant, without asserting their
causality before evidence. Keep run/build, metric/window, concise findings and reproduction in the
existing task context, not another tracker or growing raw journal.

If our code causes the cost, fix it and verify performance and behavior. A demonstrated inherited
CDDA cost may remain with its explanation and limits; calling upstream code more often or with
more work because of our changes is still our responsibility. Unknown cause is an investigation
state, not closure. Surface a concrete missing-evidence or tool blocker when necessary, rather than
asking whether to investigate an authorized regression. Reuse a supported explanation for unchanged
symptoms; reopen it for changed workload or contrary evidence instead of repeating identical tests.
Preserve intentional perception delays and diagnose cost before changing sensing frequency.

Use existing measurements first and targeted measurement when needed to decide a repair. No broad
optimization campaign, continuous profiler or extra per-turn logging follows automatically. Preserve
source/input custody, useful saves, accepted behavior and honest unresolved findings. This applies
to ordinary playtests and final soaks alike.

## Leave the catalogue ready for the next player

Finish by folding useful new setup, controls and diagnostic lessons into the existing recipe,
replacing wrong advice rather than appending a run diary. Delete settled temporary declarations and
superseded variants through the supported registry route; keep a reproducer only while its concrete
repair need remains. Small variations belong in the recipe or current brief, not another permanent
entry. Follow [authoring and carry-forward](../GAME-MANUAL.md#write-reusable-instructions-not-a-transcript)
for the shared policy; no extra playtest or approval is needed merely to tidy instructions.
