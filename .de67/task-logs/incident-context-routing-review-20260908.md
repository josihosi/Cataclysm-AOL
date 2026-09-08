# Context-routing incident review — 2026-09-08

Current authority: reviewer run `mutation-4d08160f0f5a4c4d978ef78a393e1391`, incident
`R-MAINT-CONTEXT-ROUTING-001`, lineage `semantic-surface-cockpit`.
Evidence: `.de67/state/review-incident-context-routing-20260908/`.

The routing task completed at 1788877097.0002701. Its claim expired at 1788883553.50048;
incident 9 was recorded 0.10456 seconds later. There is no closure phase, closure gap or claim
acceptance. The remaining boundary is unstarted normal whole-claim closure/acceptance, not an
unfinished routing implementation or a named live-adoption gap.

Receipt `58408053cedd8e95298f2780b445a05f8a7c8e3279962f6f6a94fcf5804c73e7` proves selected
S002/S003 packet output, exclusion of earlier S001, invalid-selector rejection and 73 focused tests.
The implementation, both test files and complete validation-log hashes still match that receipt.
The later isolated packet checks from the preceding review used the same kernel bytes. Their
valid evidence is reused; rerunning unchanged tests would not resolve the missing lifecycle step.
The exact routing source SHA-256 is `8356ca3717382df43a0147f9c5fad47f027b1e4504cc36b6cf1811fd7ebb3b47`.

The coordinator retained the completed task result while continuing other work; it never opened
this claim's closure frontier. The available state/session evidence supports that omission, but
not a narrower policy-ordering defect. `diagnosis.md` distinguishes process-attempt times from
continuation-session history. Do not assign the separate prompt-live-adoption gap to routing.

Immediate recovery compresses the routing ledger paragraph around existing proof and the exact
remaining normal closure transition. All original assignment rows remain byte-identical;
structural ledger/slice validation passes. No implementation, policy, prompt, FS, clock or
acceptance is changed. The earlier correction already makes remaining whole-claim proof visible
at worker returns and natural adoption events. No coordinator has yet had an opportunity to use
that correction. Adding another instruction before testing it is unsupported; the macro component
is therefore resolved with the existing reviewed `no_change_required` route.

Both complete pending owner entries were read and dispositioned. Code-specific FS/coordinator-review
context remains applied, with actual successor use unobserved. The FS terminology/refinement remains
applied, while canonical-file/status/proof migration and its caller/acceptance compatibility remain
assigned to `R-MAINT-FS-MIGRATION-001`. There was no coordinator between the preceding reviewer and
this invocation, so neither gap has advanced. Both deferred entries remain unchanged; none is
complete and none is deleted. Existing repair assignments, accepted proof and owner scope survive.

Restart 41 is still unclaimed. The durable harness coalesces a new resolution request into that
pending generation rather than creating a second successor. Resolution output and the before/after
restart-count check are retained under the evidence root. Only the external supervisor launches;
this review does not start a coordinator or override an owner stop.

Full-tree usage is recorded in `accounting-asof.json`, filtered to this invocation and deduplicated
by response ID across parent and Luna helper, including retries. At 16:31:15.287Z it was 4,207,697
tokens: 4,196,388 input (4,095,488 cached) and 11,309 output. The reused helper accounted for
2,704,585 input tokens; using Luna did not itself demonstrate context savings. The ledger paragraph
is six bytes shorter, which is not a meaningful token-savings measurement. Later closeout and delayed
records are outside this cutoff; reasoning is included in output. No reduction claim is made.
