# Basecamp AI capability audit/readout packet v0 (2026-04-21)

Status: checkpointed camp-side audit packet.

## Verdict

The current Basecamp AI surface is mostly deterministic camp-request plumbing around crafting requests plus board/job inspection and control.
It is **not** currently a broad freeform "say anything to camp" intelligence layer.

The player-facing spoken path is narrow and explicit.
The internal control path is richer, but still mostly structured token routing.
The still prompt-shaped pieces are mainly snapshot/template packaging and observability surfaces, not the core craft/board command interpretation path.

## Scope of this audit

This packet answers four bounded questions on the current tree:

1. What can the player currently ask Basecamp staff to do through spoken requests?
2. What internal structured actions/tokens exist behind that surface?
3. What board/job actions are actually supported now?
4. Does any prompt-shaped interpretation layer still materially matter, or is the current behavior already mostly deterministic plumbing?

## Readout

### 1. Player-facing spoken behavior that is actually supported now

The real spoken surface is centered on **craft requests** and **craft-request board management**.
The current tree proves all of this in deterministic code/tests, mainly through `basecamp::handle_heard_camp_request(...)` in `src/faction_camp.cpp` and the `[camp][basecamp_ai]` coverage in `tests/faction_camp_test.cpp`.

Supported spoken behavior now:

- **Explicit craft orders**
  - The spoken intake requires the standalone verb `craft`.
  - Examples the parser accepts include forms like `craft knife`, `please craft me five bandages`, and `could you please craft me five bandages`.
  - Non-`craft` verbs like `make knife` or partial-word accidents like `witchcraft knife` are rejected.
- **Board/status queries**
  - Organic status phrases like `what's on the board`, `what needs making`, `what needs doing`, `got any craft work`, and `show me what needs doing` are recognized.
  - These return ordinary spoken bark, not raw structured payloads.
- **Approval / launch / retry requests**
  - Spoken approval can target a specific request like `approve request #12`.
  - Bulk phrasing like `approve all ready requests`, `launch all ready jobs`, `retry all blocked requests`, and `resume all blocked jobs` is also recognized.
- **Cancel requests**
  - Spoken cancel commands can target a request id or a subject phrase like `cancel the work order for long string please`.
- **Clear archived paperwork**
  - Spoken clear commands can target archived requests in bulk or by id/subject, for example `clear archived requests` or `clear request #12`.
- **Status follow-up on individual requests**
  - Spoken status lookup can target the full board, an explicit request number, or a matched subject.

Important boundary:

- This is a **craft-request** surface, not a general all-Basecamp natural-language surface.
- The request snapshots and board/job machinery are still craft-centric. For example, `camp_request_handoff_snapshot(...)` returns nothing for non-craft request kinds.

### 2. Internal structured action/token surface that actually exists now

The current tree exposes a richer structured layer under that spoken surface.
The declarations live in `src/basecamp.h`, and the parsing / formatting logic lives in `src/faction_camp.cpp`.

Current structured tokens/actions:

- **Craft token**
  - `craft=<payload>`
- **Board/job inspection tokens**
  - `show_board`
  - `show_job=<id>`
- **Board/job action tokens**
  - `job=<id>`
  - `delete_job=<id>`
  - `launch_ready_jobs`
  - `retry_blocked_jobs`
  - `clear_archived_jobs`
- **Structured handoff packet fields**
  - board snapshots start with `board=show_board`
  - per-job follow-up fields include `details=show_job=<id>` and `next=job=<id>` or `next=delete_job=<id>`
- **Planner/overmap control token surface bundled into board snapshots**
  - `planner_move=stay | move_omt dx=<signed_int> dy=<signed_int>`
  - optional `overmap:` snapshot payload when an origin is provided

This is the part of the system that still looks like an internal command/control language.
It is real and tested.
It is also intentionally separate from the ordinary bark the player sees.

### 3. What board/job control behavior is actually supported now

The current board/job surface is not just passive display.
It already supports a bounded control loop around craft requests.

Supported control behavior now:

- **Queue and resolve craft requests**
  - spoken or structured craft input resolves recipe matches, worker choice, blockers, and batch count
  - ambiguous or missing matches are reported instead of guessed
- **Immediate launch vs pinned/blocked board state**
  - when a worker and recipe resolve cleanly, the request may start immediately
  - otherwise the request stays pinned or blocked with recorded blockers/notes
- **Show the board**
  - spoken board status returns natural bark
  - structured board snapshots expose counts plus per-job `details` and `next` tokens
- **Show an individual request/job**
  - `show_job=<id>` returns job-specific status on the same bounded path
- **Act on a job**
  - `job=<id>` is the structured follow-through token for approving/starting a live request when appropriate
  - archived or already-running jobs do not get silently reinterpreted as something else
- **Clear archived jobs**
  - `delete_job=<id>` clears archived/completed/cancelled work, not arbitrary live work
- **Batch launch / retry / clear**
  - the board supports `launch_ready_jobs`, `retry_blocked_jobs`, and `clear_archived_jobs`
- **Worker-facing spoken status bark**
  - the player-facing reply layer deliberately says human-readable things like `Board's empty.`, `Board's got 1 live and 0 old`, `...is blocked`, `...is underway`, `...is pinned`, or `...is done.`

Important boundary:

- This board/job system is still **craft-request board control**, not a general camp governance console.
- The current audit did not find a similarly broad deterministic surface for arbitrary camp tasks outside this craft-request seam.

### 4. Where prompt-shaped interpretation still matters, and where it no longer does

#### What is already mostly deterministic

The main spoken camp-request path is already mostly deterministic.

Evidence:

- `src/npctalk.cpp` checks `basecamp_ai::uses_basecamp_request_routing( *guy )` before generic LLM enqueue.
- Assigned-camp hearers that qualify are grouped by camp and routed through `basecamp::handle_heard_camp_request(...)` first.
- Only if that deterministic camp route does **not** consume the utterance does the flow fall back to `llm_intent::enqueue_requests(...)`.
- `tests/faction_camp_test.cpp` explicitly proves the routing gate keeps stationed camp assignees on the basecamp path while keeping follower/walking states out.

That means the current craft/board command interpretation is **not** mainly happening inside a vague prompt anymore.
It is already code-heavy and testable.

#### What still looks prompt-shaped or handoff-shaped

There is still a real prompt/template-facing layer, but it is narrower than the label "Basecamp AI" suggests.

That layer currently appears in:

- `llm_prompt_templates::load(...)` for
  - `basecamp_craft_handoff_snapshot.txt`
  - `basecamp_board_handoff_snapshot.txt`
  - the board job-line template
- structured board/craft snapshot formatting such as
  - `camp_request_handoff_snapshot(...)`
  - `camp_board_handoff_snapshot(...)`
- structured reply logging via `format_camp_reply_log_packet(...)`
- the planner-context prefix that adds `planner_move=...` and `overmap:` snapshot text to the board packet

So the honest current verdict is:

- **core craft/board request interpretation:** mostly deterministic now
- **snapshot packaging / control handoff / observability:** still template-shaped and still plausibly useful for an LLM-facing or planner-facing layer

#### The main still-unclear seam

The most obviously still-LLMish seam is the **planner snapshot layer** on the board packet.
It exposes signed `move_omt` tokens plus an overmap snapshot, which looks like a deliberate bridge for higher-level planning/control.
But the current audit did **not** find an equally broad player-facing deterministic speech surface on top of that planner bundle.

So if later work asks "what layer would prompt externalization even target?", the current best answer is:

- **not** the already-deterministic craft/board request parsing path
- **possibly** the planner/snapshot/control-handoff layer that still packages overmap context and structured next-step tokens

## Practical map of the territory

If someone says "what does Basecamp AI currently do?", the honest short answer is:

- It hears a narrow set of camp-crafting and board-management utterances from stationed camp NPCs.
- It parses those requests deterministically.
- It turns them into craft-request board entries with explicit status, blockers, worker assignment, and follow-up actions.
- It exposes a richer structured board/job token language under the hood.
- It still produces template-driven handoff/log packets, especially for board snapshots and planner context.
- It is **not** yet a broad natural-language camp manager.

## Evidence used for this audit

Primary current-tree evidence:

- `src/basecamp.h`
  - `namespace basecamp_ai`
  - parser/result/token declarations
- `src/faction_camp.cpp`
  - `uses_basecamp_request_routing(...)`
  - `camp_request_handoff_snapshot(...)`
  - `camp_board_handoff_snapshot(...)`
  - `parse_heard_camp_*` helpers
  - `parse_structured_camp_*` helpers
  - `basecamp::handle_heard_camp_request(...)`
- `src/npctalk.cpp`
  - assigned-camp routing before `llm_intent::enqueue_requests(...)`
- `tests/faction_camp_test.cpp`
  - `[camp][basecamp_ai]` parser coverage
  - routing-gate coverage
  - board/job snapshot coverage
  - spoken-vs-structured reply separation coverage
  - overmap planner snapshot token coverage

## Non-goals and boundaries kept

This packet does **not**:

- add new Basecamp features
- externalize prompts
- widen the craft-request seam into a general Basecamp-AI architecture rewrite
- reopen Locker Zone V3
- pretend the current tree already has a broad freeform camp command surface when it does not

## Conclusion

The current Basecamp AI surface is already much less mystical than the name suggests.
The core camp-craft and board/job request path is now a deterministic, test-backed router with a human-facing bark layer and a separate structured control layer.

The remaining prompt-shaped pieces are mostly the handoff/packet/planner side.
So any later cleanup or prompt-externalization discussion should start there, not by ripping apart the already-deterministic craft/board parser path just because the old label still sounds grandiose.
