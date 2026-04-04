# C-AOL Plan

## What this document is for
This is the working roadmap for the **next meaningful stretch** of Cataclysm: Arsenic and Old Lace.
It should stay practical:

1. What is already done enough to stop pretending it is still “next”?
2. What is green for implementation right now?
3. What still needs discussion before anyone freelances architecture?
4. What is parked on purpose so it does not haunt chat?

This file is not a trophy shelf and not a landfill.
If something is done, move it out of the active plan.
If something is not active, park it clearly.
If something needs discussion, label it that way instead of letting the cron goblin guess.

---

## Operating status labels
Use these labels consistently across planning/testing notes.

- **DISCUSS** — not specified enough yet; do not implement autonomously.
- **GREEN** — structure agreed; implementation can proceed autonomously.
- **AGENT TESTING** — code exists; Schani should run deterministic tests / compile / launch-load checks.
- **JOSEF TESTING** — human gameplay/feel checks are needed.
- **TWEAK** — known follow-up pass from testing feedback.
- **PARKED** — intentionally not active right now, but kept as reference or for later reuse.
- **DONE** — crossed off only after the agreed finish line, not when the diff merely exists.

---

## Current project state baseline
These are baseline realities, not the current milestone:

- `dev` is the active iteration branch.
- Release-branch flow was cleaned up around `dev`, `master`, and the `port/*` targets.
- The startup harness can build, launch, and load a known save without Josef hand-driving every menu.
- The deterministic Basecamp request-board / spoken-camp-control v1 slice landed on `dev`:
  - request data model
  - bulletin-board scratchpad UI
  - spoken camp craft order intake
  - request approval / retry / cancel / status controls
  - request-number references
  - worker reassignment / retry handling
  - tool reclaiming from hoarded camp stock
  - save/load persistence for board state
- Recent follow-up hardening already landed:
  - map-lettered follower snapshot targets are now explicitly addressable as attack handles
  - deterministic camp craft routing was tightened so ordered phrase matches beat generic noun fallbacks
  - first deterministic `llm_intent` snapshot/prompt tests exist

These matter because the next stretch is building **on top of an existing deterministic spine**, not inventing camp control from zero again.

---

## Current branch policy
- Do active iteration on `dev`.
- Promote `dev` to `master` when the current slice is stable and verified.
- Use `master` as the source branch for orchestrated propagation to `port/*`.

That remains the default until reality humiliates it again.

---

## Active status board

### 1. Basecamp AI completion on `dev`
**Status:** GREEN (queued behind movement-system work, except for follow-up bugfixes)

This has been discussed deeply enough to proceed through completion.
The overall structure is considered clear enough for autonomous work.

#### Current intended flow
1. implement the agreed deterministic states / routing / contracts
2. add deterministic tests
3. compile
4. launch the game / load a save / inspect debug output
5. hand Josef a concrete manual test packet
6. do one tweak round after human testing
7. cross it off only after signoff

#### Current direction
- deterministic first-pass recognition for obviously structured camp commands
- deterministic execution remains the legal/action spine
- richer Basecamp AI on `dev` can sit on top of that spine instead of replacing it
- eventually externalize prompt/snapshot text where that helps modding and iteration

---

### 2. Upstreamable deterministic PR package
**Status:** PARKED (kept as reference / possible Reddit-post material, not the active finish line)

This was the public-facing deterministic slice for upstream `cdda-master`.
The code/test/gameplay side landed and passed locally, but the actual upstream PR attempt was closed quickly on project-social grounds.
So this is no longer the active queue item.
Keep it around as:
- a reference branch/worktree for the extracted deterministic slice
- a record of what passed locally
- possible material for a later write-up or narrower future upstream attempt

#### Local finish state reached
- exact-word `craft` routing landed
- deterministic craft query / ambiguity / blocker handling landed
- deterministic tests passed
- game launch / save-load checks passed
- Schani smoke passed
- Josef live trio passed on the cleaned upstream-based branch

#### Why it is parked instead of active
The current blocker is not local implementation quality.
The current blocker is upstream interest / social acceptance.
That means further local work should go back to actual `dev` development instead of spending more autonomous cycles trying to resurrect the same PR package.

#### Reuse rule
If we later want a Reddit post, a retrospective, or a fresh deterministic upstreamable slice, use this parked branch/worktree as reference material.
Do not treat it as the active development branch.

---

### 3. Movement-system improvements
**Status:** AGENT TESTING — PRIMARY FINISH LINE

This is now the active queue item.
The upstream deterministic PR slice is parked, so movement-system work becomes the next real stretch on `dev`.

#### Current direction
- replace the LLM-facing coordinate payload for local tactical movement with relative signed deltas instead of step spam
- keep the existing pathing / target-tile / follow-state machinery intact
- keep the existing post-move state suffixes intact:
  - `wait_here`
  - `hold_position`
- in other words, do **not** erase the current system; only replace how the LLM expresses the destination
- use the same relative-delta idea for overmap-targeted movement/planning as well
- update prompt/snapshot explanation accordingly, and consider lightweight grid/axis hints if they help the model reason about offsets more reliably

#### Current active sub-item
The first structured Basecamp/planner-consumer slice now exists locally on `dev`, but one important path distinction is now explicit:
- [x] threaded the overmap snapshot formatter into the **structured** Basecamp board handoff token path (`show_board`) instead of leaving it as a dead helper
- [x] kept the signed-axis contract live in prompt text via `planner_move=stay | move_omt dx=<signed_int> dy=<signed_int>`
- [x] kept the same present-only legend / axis-hint snapshot text when the structured board handoff receives a real camp origin
- [x] added deterministic coverage for the structured board-handoff consumer path
- [x] re-ran compile / filtered deterministic tests / startup harness checks after landing the live-consumer slice
- [x] fixed the stale/brittle overmap snapshot expectations in `88f2e3eeb7` (`Stabilize overmap snapshot formatter tests`) after Schani caught the uppercase-horde mismatch
- [ ] live natural speech `show me the board` / `what's on the board` currently still goes through the older concise board-summary bark path rather than surfacing the richer structured overmap snapshot block
- [ ] so this slice is **not** Josef-testing clean as a live speech-path change yet; the remaining agent-side question is whether the richer snapshot belongs in the spoken path, stays internal/structured only, or needs a different exposure plan

#### Next movement continuation
- verify the intended product shape before sending Josef another readability packet:
  - if the goal is only a structured/internal handoff artifact, document that clearly and stop claiming the live spoken bark should include the snapshot block
  - if the goal is a richer live spoken board reply, wire that path explicitly and then re-test it live
- keep the two issues separate while doing that:
  - live spoken board replies already work on-screen
  - logging visibility for direct deterministic `show_board` remains incomplete / ambiguous
- only after the intended path is explicit should Josef get a fresh movement-system readability packet

#### Overmap snapshot contract (approved direction)
- represent the overmap as a **small grid**, roughly **5x5 or 6x6**
- this is not just for the job-sending selector; it should become part of the broader Basecamp AI snapshot vocabulary
- use **collapsed terrain symbols** rather than raw overmap tile soup
- use **lowercase** for normal terrain and **UPPERCASE** for the same terrain when a **horde** is present on that tile
- show a **present-only legend**: the legend should list only symbols actually visible in the current snapshot, not the full master symbol table every time
- current preferred terrain buckets include:
  - `c` camp
  - `h` house
  - `r` road
  - `m` meadow/grass
  - `f` field
  - `t` forest
  - `s` swamp
  - `w` water
  - and the extended set is allowed where useful (`b` bridge, `u` urban, `p` point of interest, `k` shop, `n` riverbank/shore)
- the key architectural rule is that this grid should be legible enough to support later bandit movement and other overmap-brain behavior, not just immediate camp job targeting

This work should feed both follower movement cleanup and later Basecamp job-subflow architecture instead of happening as a disconnected side quest.

---

## Discuss-before-implementation board

### Harness rework / in-engine driver API
**Status:** PARKED

Not active right now, but there is now a saved auxiliary note at [`doc/openclaw-harness-symbolic-rework-note-2026-04-04.md`](doc/openclaw-harness-symbolic-rework-note-2026-04-04.md).
If this gets revived later, the current preferred direction is:
- symbolic/stateful harness evolution, not screenshot-first automation
- action-result / failure-reason contracts before broader agent playtest loops
- a tiny in-engine driver API for explicit UI state and scenario control
- external orchestration for profiles, scenarios, artifacts, notebooks, and adapters
- clearer separation between live behavior, deterministic test contracts, and artifact/log visibility so probe-method changes do not muddy conclusions


### 4. Bandit AI
**Status:** DISCUSS

Bandit AI is real future work, but it has **not** been discussed in enough detail yet to let autonomous implementation start safely.
Do not treat “eventually” as permission.
We should talk through architecture, constraints, and intended test checkpoints first.

### 5. Follower NPC deterministic-first rework
**Status:** DISCUSS / PARKED

Important longer-term direction, but not active implementation yet.
The key design tension is obvious:
- deterministic command extraction is useful,
- but follower NPCs must still be able to remain reluctant, weird, characteristic, defiant, or even hostile.

So this cannot degrade into “NPC always obeys the parsed command.”
Keep it remembered; do not tackle it casually.

---

## Recommended implementation order (current reality)

### First
Improve the movement contract without erasing the existing system:
- local tactical movement should replace step chains with relative signed deltas
- overmap/job movement should use the same relative-delta idea where appropriate
- current first concrete sub-item is the small overmap snapshot grid / present-only legend / terrain-symbol contract
- keep `wait_here` / `hold_position`
- keep deterministic pathing / target-tile logic
- add tests
- avoid accidental behavioral regressions while changing only the LLM-facing coordinate expression

### Second
Continue the richer Basecamp AI on `dev`:
- deterministic-first command extraction
- structured legal action tokens
- deterministic execution of those tokens
- richer snapshot/prompt handoff only when deterministic handling is insufficient

### Third
Return to broader camp job types / deeper board QoL once the previous contracts stop moving.

### Parked reference
The upstreamable deterministic camp-command slice is kept as reference material, not as the active queue.

That is enough work already.
There is still no need to turn one successful half-day into a full municipal bureaucracy.

---

## Finish lines / signoff gates

### Deterministic PR slice
Call it ready for packaging when it:
- compiles cleanly
- has relevant deterministic tests
- contains no LLM dependency
- launches successfully in-game
- loads a save/world successfully
- gets at least a small manual smoke/play test of the actual deterministic feature
- is small enough to explain honestly in one sane PR description
- looks like something a human can clean up and defend upstream

### Basecamp work on `dev`
Call it done only when it is:
- implemented on `dev`
- compiles
- the game launches
- a save loads successfully
- there are zero new debug messages / zero crashes

The finish line is not “the patch exists.”
The finish line is “it survives contact with the actual game.”

---

## Autonomous work-loop rules
The cron/autonomous loop should:
- continue only **GREEN** items
- prepare deterministic tests whenever behavior is supposed to be deterministic
- run compile / launch / save-load checks when appropriate
- prepare Josef-facing test packets when human gameplay validation is the next step
- ping Josef on meaningful progress, blockers, or ready-for-testing states
- stop and ask when only **DISCUSS** work remains

Narrow recon subagents are allowed for GREEN work if useful.
Do **not** use subagents to invent architecture for DISCUSS items.
