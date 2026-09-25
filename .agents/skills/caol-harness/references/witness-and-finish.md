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

At the honest boundary, seal `run.witness` and call `run.finish`. State the smallest conclusion
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

Report startup, feature outcome, contradictions, evidence ceiling, and cleanup separately.
The primary worker owns every game launched by it or its helpers, including failed starts and
replacement generations. Native save/quit preserves the requested game state; explicit `run.finish`
seals the witness and requests process cleanup, which does not earn native-save/exit proof.
Inspect cleanup's current PID/birth identity and verify OS exit for all owned attempts and their
brokers, or hand over an explicitly retained run with its purpose and exact session. A minimized
window, stopped broker or finished report alone leaves that obligation open. A failed startup's
retained-process result requires an explicit owner action. Preserve the run and report the exact
blocker if supported graceful closure fails; do not silently force-kill it.
