# Upstream spoken camp craft PR package

This note is the small human-readable package for the current upstreamable spoken camp craft slice.
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
- blocked craft resolution stays deterministic instead of falling through to generic chatter
- conservative plural cleanup helps obvious requests like `makeshift bandages`
- player-facing bark stays tied to the heard request text and explains the resolved recipe / blocker clearly enough to recover
- tests cover the deterministic routing behavior directly

---

## Current close-out state

Agent-side close-out is basically done.
The live/signoff packet is now intentionally narrow:
- Schani smoke baseline: `311c7ab1b7`
- current gameplay signoff target: `4a39c70ac7`
- main behavior-bearing fix: `696f5c8b61`
- tiny follow-up polish since the smoke head: `1df9e378c8` trims the duplicate-period blocked-bark punctuation

The current `dev` / `Sandy Creek` live trio is satisfactory:
1. `craft 5 makeshift bandages`
   - positive deterministic path
   - resolved recipe: `makeshift bandage`
   - request pinned cleanly
2. `craft boiled`
   - deterministic ambiguity / clarification path
   - no silent guess
3. `craft 5 bandages`
   - deterministic blocked / no-crash path
   - resolved recipe + real blocker surfaced clearly enough to understand what happened

Current signoff target `4a39c70ac7` has already re-passed:
- `make -j4 tests`
- `./tests/cata_test "[camp][basecamp_ai]"`
- `make version TILES=1 -j4 cataclysm-tiles`
- `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'`

Result:
- zero recorded debug popups
- no reproduced crash in the live spoken-craft trio
- only Josef final smoke / “does this still feel stupid?” signoff remains before final humanized PR prep

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
4. a conservative final-word trailing-`s` fallback when that is the only thing standing between the heard phrase and the obvious recipe subject

This prevents partial-word nonsense, makes specific phrases beat generic noun fallback, and recovers plain plural phrasing without turning the matcher into soup.

Examples:
- `boiled bandages` should beat plain `bandages`
- `band` should not match `bandages`
- `makeshift bandages` can still resolve to `makeshift bandage`

### 3. Deterministic ambiguity handling
If the top spoken craft match is genuinely ambiguous across multiple subjects, camp should ask for a clearer target instead of pretending certainty.

Example:
- `craft boiled` -> clarify, do not silently pick one of several `boiled ...` recipes

### 4. Deterministic blocked / no-match handling
If the request resolves to a blocked craft, camp should stay in the deterministic craft path and explain the blocker rather than dissolving into unrelated generic chatter.
If the request does not match a camp-known craft target at all, camp should say so plainly.

### 5. Shared deterministic craft resolution helper
The spoken craft path uses a shared deterministic resolution helper (`resolve_camp_craft_query`) for:
- top recipe matching
- broader craft-set lookup before worker/storage evaluation
- blocked candidate preservation
- deterministic final choice among same-subject candidates
- explicit final outcome (`start`, `blocked`, `ambiguous`, `no match`)

This keeps the logic from splitting into multiple ad-hoc loops and closes the regression where plausible requests like `craft boiled` or `craft 5 bandages` could collapse into the wrong deterministic bucket on the live camp bed.

### 6. Human-readable bark polish
The public patch story should also include the small human-facing cleanup that makes the behavior less bureaucratic:
- prefer the heard request subject in bark / board text
- keep request ids as trailing detail instead of leading filing-cabinet noise
- surface matched recipe / blocker detail when it differs from the heard request
- trim silly punctuation artifacts

This is still deterministic behavior cleanup, not AI dressing.

---

## Explicit non-goals for this PR package

Do **not** mix these into the upstream spoken craft PR story:
- any LLM dependency or prompt/snapshot plumbing
- richer Basecamp AI handoff behavior
- follower command semantics beyond this narrow camp-craft path
- broader board/job token grammar (`show_board`, `job=<id>`, `delete_job=<id>`, etc.)
- movement-contract work (`move=<dx>,<dy>`)
- personality/design debates about NPC obedience in general

This package should read like a deterministic Basecamp speech-routing cleanup, because that is what it is.

---

## User-visible behavior summary

Before:
- spoken craft intake accepted broader verbs
- recipe matching could be too loose
- substring false positives were possible
- ambiguous or blocked requests could fall through poorly
- bark could sound too much like internal request bookkeeping

After:
- only explicit `craft ...` speech enters the craft path
- matching is whole-word / ordered-phrase driven
- conservative plural cleanup catches obvious requests without opening the floodgates
- ambiguity is reported instead of guessed
- blocked results keep deterministic blocker text
- bark leads with the human request subject and keeps the request id as trailing detail

---

## Code surface

Primary touched files:
- `src/basecamp.h`
- `src/faction_camp.cpp`
- `tests/faction_camp_test.cpp`

The mechanical story should stay intentionally small.
If a future squash/transplant needs a giant side caravan of unrelated prompt plumbing, token grammar, or movement work, the package has grown sloppy.

---

## Deterministic test coverage in place

Current direct coverage in `tests/faction_camp_test.cpp` includes:
- quantity parsing for spoken craft requests
- exact standalone `craft` trigger
- rejection of non-craft verbs and `witchcraft` substring false positive
- ordered multi-word phrase preference over generic noun fallback
- no partial-word substring match (`band` != `bandages`)
- ambiguity reporting for equally good top subjects
- blocked-match preservation via the shared deterministic resolver
- conservative plural fallback for final-word `s`
- display/bark-summary handling when heard request text differs from the resolved recipe

Current targeted run used during the latest pass:
- `./tests/cata_test "[camp][basecamp_ai]"`
- latest recorded result on the close-out path: `All tests passed (232 assertions in 1 test case)` before the final signoff-doc retargeting commits

---

## Runtime validation already done

Current gameplay signoff target `4a39c70ac7` has already survived contact with reality:
- compiled cleanly
- targeted tests passed
- tiles build passed
- startup harness launched the game successfully
- known save/world autoload succeeded (`dev` / `Sandy Creek`)
- zero recorded debug popups in `.userdata/dev/harness_runs/20260404_171141`

This matters because the finish line is not “the diff exists.”
The finish line is “the game still survives contact with the diff.”

---

## Still pending before calling it truly ready

Human-side validation still needed:
- Josef runs the narrow final smoke/signoff trio from `TESTING.md`
- confirm the bark + board/detail wording feels clear rather than bureaucratic
- confirm the tiny punctuation trim really killed silliness like `tools..` in live play

Packaging / humanization still needed before an actual public submission:
- final squash / commit curation
- final PR description in upstream voice
- one last pass to remove fork-local narration or unnecessary scaffolding from the public patch story

So the real remaining work is not another archaeology dig.
It is Josef signoff plus final human cleanup.

---

## Ready-to-paste PR description draft

### Summary
- narrow spoken Basecamp craft intake to explicit `craft` commands
- make spoken craft matching deterministic and whole-word / phrase based
- report ambiguous spoken craft targets instead of guessing
- keep blocked spoken craft requests in the deterministic path with clearer blocker feedback
- add regression coverage for trigger, ambiguity, blocked, and phrase-matching behavior

### Why
Camp speech handling is easier to review when it behaves like an explicit command path instead of loose fuzzy magic.
This keeps spoken craft requests narrow, deterministic, and easier to debug:
- exact `craft` routing avoids accidental trigger matches
- phrase-aware matching avoids substring nonsense
- ambiguity is surfaced instead of guessed
- blocked requests explain themselves instead of collapsing into unrelated chatter

### Testing
- `./tests/cata_test "[camp][basecamp_ai]"`
- `make -j4 tests`
- `make version TILES=1 -j4 cataclysm-tiles`
- `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'`
- live spoken-craft smoke on `dev` / `Sandy Creek`:
  - `craft 5 makeshift bandages`
  - `craft boiled`
  - `craft 5 bandages`

---

## Candidate commit set to humanize / squash around

Probable code ingredients for the public patch story:
- `76c01d611f` — tighten deterministic craft routing
- `50d03fb24c` — require exact spoken craft trigger
- `edc22cf276` — report ambiguous camp craft matches
- `2dd18e3fb0` — share deterministic camp craft resolution
- `b7a1d6adca` — prefer heard craft text in request display / summaries
- `1887c0a08f` — add conservative plural fallback and deterministic no-match handoff cleanup
- `83c32c5710` — polish camp request bark references
- `696f5c8b61` — tighten spoken camp craft resolution
- `1df9e378c8` — trim blocked camp bark punctuation

Local tracking / packaging commits that are useful reference but not necessarily part of the final public patch as-is:
- `78485cb5cb` — document camp craft smoke testing
- `51859112de` — refocus spoken camp craft smoke packet
- `7100ef42ba` — retarget signoff docs to the actual gameplay head

Use judgment instead of cargo culting the exact hashes into a public branch.
The point is the patch story, not preserving every local breadcrumb like a museum exhibit.
