# R-MAINT-COMPACT-IO-observation-001

Extend the existing evidence route with optional selections, comparison and request/result retrieval.
This is ordinary tooling work commissioned by Sol, not gameplay acceptance. Reuse
`tools/openclaw_harness/cockpit_evidence.py::select,gameplay_fact,query`, `cockpit_archive.py`,
`cockpit_file_bridge.py`, `play_cli.py`, `registry_query_output.py` and the native inspector before
adding instrumentation. Current select decodes embedded JSON; query retains original record handles.
A source-code diff (`trajectory_sidecar.py`) is not a native state comparison. Fresh Patrol inspection currently calls `src/npc_inspection.cpp` →
`basecamp::get_current_patrol_shift_plan`, which calls `refresh_patrol_shift_cache`; its provenance
admits plan refresh. Retained-artifact selection is read-only, but a fresh native view must not use
that mutating accessor as a read-only oracle. Add a narrow non-refreshing peek with explicit absent/
stale cache state and verify observation leaves the plan, actor/order and native time unchanged.
Sol owns the native-view prerequisite assignment separately from presentation edits.

Crafting premises: `src/npctalk.cpp` selects camp hearers and calls
`src/faction_camp.cpp::basecamp::handle_heard_camp_request`. The handler requires actual basecamp
request routing plus a matching assigned camp, resolves recipe and capable worker, then queues an
identified request. Inspect `resolve_crafting_worker`, `queue_crafting_request`, the resource/tool
inventory in `src/basecamp.cpp` and the job-result owner. A non-camp GUARD_ALLY promise is not a camp
job. Show selected recipient/camp/capability, recipe requirements, actual resources/tools/location/
ownership, food when relevant and job with rejection reasons. Preserve pending/competing listeners.
Original accepted craft receipt34681646df11e45a133cb3eee507287f23719021b376dec2f2142bc6aee3189e and
R031's exact run references are retrievable through `work_context.py --task R-031-camp-craft-recipient-repair-001`.

Signals: `src/do_turn.cpp::live_bandit_staffed_camp_signal_reads` and
`src/bandit_live_world.cpp::record_staffed_camp_signal_observations` expose observer/source/LOS and
lead transitions. R033 v7 witness `.de67/task-logs/r033-v7-witness.json` and transition309 preserve the
same-source refresh contradiction. Model evaluates correctness; workbench displays content and
observation timestamps separately. A stopped/unloaded actor, absent field or unavailable read is
unknown, not a false/zero/empty value. Different-time views are not atomic snapshots.

Select stable entities/fields with source/run/frame/turn, generation/process and freshness plus
original artifact handles. Compare compatible bindings: changed/unchanged/added/removed observable
state, unknowns and incompatibility. Never join transient IDs across generations without durable
identity. Retrieve recorded links for utterance→recipient→native job accepted/rejected→result and
source→visibility→matched lead→update; expose missing links, ordering and other writers rather than
inventing causality from time adjacency or spoken OK. Instrument only a demonstrated missing link,
read-only and narrow, without adding another journal/receipt framework.

Prove both domains through retained native artifacts and isolated tests covering missing resources/
capability/fields, stale source, incompatible process identity, same-content timestamp changes and
unrelated writers. Reuse shared machinery for Patrol or Pay by changing selections/adapters only.
Patrol's counterexample: priority-zero Katharina while Robbie actually owns the active cache does
not exercise release. Retained cycle-9 J0141/J0166 and accepted release/persistence receipts in R-032
supply originals; expose roster/active owner, priority/order/runtime and compare the same actor.
Keep recorded startup uncertainty/recovery diagnostics from the existing assignment.

Selections live in existing scenario/brief/context and remain adjustable. No new DSL, mandatory
walkthrough, readiness oracle, automatic fixtures/time advancement or proof verdict. Use
`context_library.py put/reuse/prepare` and `work_context_provider.py::prepare_current_results` to give
Sol concrete commands/examples after implementation. Verify the actual selected worker packet and
next useful native adoption; isolated tests are not live use. No full campaign replay for adoption,
output quotas or billing savings inferred from byte reductions. Source-bind the smallest fresh native
integration test only when it resolves a live integration gap; preserve original evidence and owners.
