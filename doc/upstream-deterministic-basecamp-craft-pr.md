# Upstream deterministic Basecamp craft PR package

This note is the small human-readable package for the current deterministic Basecamp craft slice.
It is not the grand Basecamp AI plan.
It is the narrow patch story that can plausibly be reviewed upstream without dragging LLM design, hybrid routing, or future command grammars into the room.

Related docs:
- roadmap / work order: [`../Plan.md`](../Plan.md)
- current test ledger: [`../TESTING.md`](../TESTING.md)
- broader Basecamp direction: [`basecamp-llm-v1-patch-plan.md`](basecamp-llm-v1-patch-plan.md)

---

## Patch goal

Make the spoken Basecamp craft path deterministic, reviewable, and less likely to do something stupid when the player gives a clear craft request.

In practice that means:
- only the exact standalone verb `craft` triggers the spoken camp craft path
- craft matching uses exact words / ordered phrases instead of loose substring guessing
- ambiguous top matches are surfaced instead of silently guessed
- blocked craft resolution stays deterministic and preserves blocker text
- tests cover the deterministic routing behavior directly

---

## In scope

### 1. Exact spoken trigger
Examples:
- `craft knife` -> craft path triggers
- `craft five bandages` -> quantity survives
- `witchcraft knife` -> does not trigger
- `make knife` / `build knife` -> do not trigger this path

Reason:
The upstream-facing slice should prefer a narrow explicit command over fuzzy natural-language reach.

### 2. Deterministic craft query matching
The craft resolver now prefers:
1. exact product-name match
2. exact ordered phrase / whole-word phrase match
3. deterministic tie handling

This prevents partial-word nonsense and makes specific phrases beat generic noun fallback.

Examples:
- `boiled bandages` should beat plain `bandages`
- `band` should not match `bandages`

### 3. Deterministic ambiguity handling
If the top spoken craft match is genuinely ambiguous across multiple subjects, camp should ask for a clearer target instead of pretending certainty.

Example:
- `craft boiled` -> clarify, do not silently pick one of several `boiled ...` recipes

### 4. Shared deterministic craft resolution helper
The spoken craft path now uses a shared deterministic resolution helper (`resolve_camp_craft_query`) for:
- top recipe matching
- per-recipe worker evaluation
- blocked candidate preservation
- deterministic final choice among same-subject candidates

This keeps the logic from splitting into multiple ad-hoc loops before the later structured `craft=<query>` path reuses it.

---

## Explicit non-goals for this PR package

Do **not** mix these into the upstream deterministic craft PR story:
- any LLM dependency or prompt/snapshot plumbing
- richer Basecamp AI handoff behavior
- follower command semantics beyond this narrow camp-craft path
- broader board/job token grammar (`delete_job=<id>`, `job=<id>`, etc.)
- movement-contract work (`move=<dx>,<dy>`) 
- personality/design debates about NPC obedience in general

This package should read like a deterministic Basecamp speech-routing cleanup, because that is what it is.

---

## User-visible behavior summary

Before:
- spoken craft intake accepted broader verbs
- recipe matching could be too loose
- substring false positives were possible
- ambiguous top matches could collapse into an arbitrary choice

After:
- only explicit `craft ...` speech enters the craft path
- matching is whole-word / ordered-phrase driven
- ambiguity is reported instead of guessed
- blocked results keep deterministic blocker text

---

## Code surface

Primary touched files:
- `src/basecamp.h`
- `src/faction_camp.cpp`
- `tests/faction_camp_test.cpp`

The mechanical story is intentionally small.
If a future squash/transplant needs more than these plus incidental build/version churn, the package has probably grown sloppy.

---

## Deterministic test coverage in place

Current direct coverage in `tests/faction_camp_test.cpp` includes:
- quantity parsing for spoken craft requests
- exact standalone `craft` trigger
- rejection of non-craft verbs and `witchcraft` substring false positive
- ordered multi-word phrase preference over generic noun fallback
- no partial-word substring match (`band` != `bandages`)
- ambiguity reporting for equally good top subjects
- blocked-match preservation via shared deterministic resolver

Current targeted run used during this pass:
- `tests/cata_test "[camp][basecamp_ai]"`
- result during latest pass: `All tests passed (34 assertions in 1 test case)`

---

## Runtime validation already done

On current `dev` binary version `2dd18e3fb0`:
- compiled cleanly
- startup harness launched the game successfully
- known save/world autoload succeeded (`Sandy Creek` / `Danial Gonzalez`)
- zero debug popups
- clean stdout/stderr/debug logs for the harness run

This matters because the finish line is not "the diff exists."  The finish line is "the game still survives contact with the diff."

---

## Still pending before calling it truly ready

Human-side validation still needed:
- Josef runs the spoken camp craft smoke packet from `TESTING.md`
- confirm the in-game behavior feels clear rather than bureaucratic
- confirm bark wording is understandable and not spammy

Packaging/humanization still needed before an actual public submission:
- final squash / commit curation
- final PR description in upstream voice
- one last pass to remove any fork-local narration or unnecessary scaffolding from the public patch story

---

## Suggested PR description skeleton

### Summary
- narrow spoken Basecamp craft intake to explicit `craft` commands
- make craft matching deterministic and whole-word / phrase based
- report ambiguous spoken craft targets instead of guessing
- add regression coverage for trigger, ambiguity, blocked, and phrase-matching behavior

### Why
- keeps the upstream-facing camp speech path explicit and reviewable
- avoids substring false positives and accidental recipe guesses
- preserves deterministic blocker handling without introducing LLM dependencies

### Testing
- `tests/cata_test "[camp][basecamp_ai]"`
- startup harness launch/load validation on known save

---

## Candidate commit set to humanize/squash around

Code-bearing commits in the current local series:
- `76c01d611f` — tighten deterministic craft routing
- `50d03fb24c` — require exact spoken craft trigger
- `edc22cf276` — report ambiguous camp craft matches
- `2dd18e3fb0` — share deterministic camp craft resolution

Local packaging / tracking commits that are useful reference but not necessarily part of the final public patch as-is:
- `78485cb5cb` — document camp craft smoke testing
- `82307a181d` — record startup harness validation

Use judgment instead of cargo culting the exact hashes into a public branch.
The point is the patch story, not preserving every local breadcrumb like a museum exhibit.
