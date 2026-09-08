# Recent implementation quality review

Read-only diagnosis requested by Josef after owner gate c895367b1fc2; no current exclusive mutation
invocation is assumed. Source snapshot HEAD 1a23b7b0de3df7b1871b5177f2d574d1c344cc93. Source hashes
and exact outputs are in reproduction.json; reproduce.py uses only temporary source files and derived
SQLite databases. No live game, source, active ledger, DFS, policy or deadline state was changed.

## Findings

1. High impact within the retrieval tool: sources.active_generation is written as a per-path generation
   number in evidence_search_index.py::_new_generation (lines 127-136) but read as the global
   generations.generation_id in ingest (line 189) and coverage. With A then B in one index, each source
   has local generation 1 but globally different row IDs. Re-ingesting unchanged B selects A's row,
   marks A stale and creates a second B occurrence. The reproduction returns current [B,B] instead of
   [A,B]. This breaks cross-source isolation, idempotence and coverage, even with unchanged inputs.
   Repair must choose one identity representation consistently, bind it to its owning source and
   verify multiple files across repeat, append/replacement and reopen; do not merely special-case B.
2. Explicit original-record filters are unreachable for non-column fields: evidence_search_query.py:52
   tests every filter with row.get before recovery. A non-null feature=crafting filter is rejected
   because feature is not a stored occurrence column. The intended original-record test at lines 69-70
   is never reached, yielding no_match for a record that exists. Repair must distinguish indexed
   metadata from original payload selectors, preserve missing versus null semantics and make unsupported
   filters explicit. The fixture demonstrates one unfiltered record and zero feature-filtered records.

Both counterexamples reproduce against the hashed current source. Eight relevant existing tests
(four index tests excluding the unrelated discovery test, and all four query tests) pass; complete
output/exit are in existing-tests.log/exit. This is a gap in discriminating test coverage, not evidence
that tests failed to run. The index suite uses one source file; the query suite exercises stored
identity fields. Neither varies the dimension that triggers its corresponding bug.

## Context and review implications

The sample suggests checking whether invariants survive a second source/identity and whether public
API promises hold outside the fields convenient for the implementation. These are evidence-backed
review questions for this code class, not established universal traits of Terra. No implementation
model attribution is assumed before checking its actual binding. Similar errors in two modules do
not establish individual incapability or a model-wide rate.

A more code-specific DFS/brief can name the distinction between globally unique IDs and per-source
revision counters, how state ownership crosses the schema/API boundary, and missing/null/payload
filter semantics. However the existing retrieval contract already requires occurrence identities,
repeatable ingestion and explicit filters. Greater verbosity alone cannot guarantee those properties.
Coordinator acceptance should connect the changed data flow and interfaces to tests that could
falsify those requirements, not infer comprehensive quality from a green suite or a receipt's label.
Review scope should be proportional to impact; use a bounded independent reviewer when the coordinator
needs implementation expertise. Keep useful findings in selected current task context and remove
obsolete advice as the tooling/tests absorb it, rather than adding a universal warning catalogue.

## Delivery

Sol was sent the two counterexamples through the authorized mailbox (message
8f46164630374779ae5ba0b6653243f9) for ordinary retrieval repair and proof reconciliation. Sending is
not acknowledgement or completed repair. DFS/context/method changes remain deferred owner-review
input, preserving current coordinator/worker ownership. Retain these source-bound findings if the
live files change before implementation, and recheck changed inputs before treating them as open.

## Delivered-brief check

Exact hash-bound implementation packet references and projections are retained in brief-evidence.json.
The index packet already required source/occurrence/version identity and repeatable ingestion. The
query packet already required explicit filters and verified original retrieval. These requirements
were therefore not simply absent from the delivered assignments. A more concrete identity/selector
contract may help, but the proven remaining gap is implementation and tests discriminating the
specified behavior. The supplied receipts bound implementation worker UUIDs and embedding-model
identity; they did not establish the actual worker-agent model. No Terra-specific tendency is claimed.
The implementation receipts also explicitly leave usefulness, adoption and savings unproved, which
is valid restraint and is preserved by this diagnosis.
