# C-AOL Plan

## What this document is for
This is the working roadmap for the **next** meaningful stretch of Cataclysm: Arsenic and Old Lace.
It should stay practical:

1. What is already done enough to stop pretending it is still "next"?
2. What are we actively doing now?
3. What should happen after that?
4. What ideas are real, but explicitly **not now**?

This file is not a trophy shelf and not a wish-list landfill.
If something is done, move it out of the active plan.
If something is not happening now, park it clearly instead of letting it haunt chat.

---

## Current project state

### Done enough to remove from the active plan
These items are no longer the immediate roadmap:

- `dev` is the active iteration branch again.
- Release-branch flow was cleaned up around `dev`, `master`, and the `port/*` targets.
- The startup harness can build, launch, and load a known save without Josef hand-driving every menu.
- The Basecamp LLM / request-board v1 slice landed on `dev`:
  - request data model
  - bulletin-board scratchpad UI
  - spoken camp craft order intake
  - request approval / retry / cancel / status controls
  - request-number references
  - worker reassignment / retry handling
  - tool reclaiming from hoarded camp stock
  - save/load persistence for board state
- Startup harness log handling was cleaned up enough to ignore the known inherited `attack_vector` startup noise and to evaluate filtered **per-run debug deltas** instead of dragging cumulative old junk into every new run.

These are now baseline reality, not the current milestone.

### Current branch policy
- Do active iteration on `dev`.
- Promote `dev` to `master` when the current slice is stable and verified.
- Use `master` as the source branch for orchestrated propagation to `port/*`.

That remains the default until reality humiliates it again.

---

## Immediate strategic priority

## Design Basecamp AI v2 as a hybrid camp-brain layer

Basecamp request-board v1 is real now.
That means the next interesting problem is no longer "can camp hear a craft request at all?".
It is now:
- how to keep the deterministic board/action spine,
- while giving camp replies actual personality and social texture,
- and extending the same pattern toward build / mission / broader camp-management actions.

So the next pass should not be a blind rewrite.
It should be a **hybrid v2 design pass**: deterministic action resolution underneath, personality-rich LLM reply + constrained action-token choice on top.

---

## Milestone 1: Basecamp AI v1 stabilization (done enough)

### Goal
Make the deterministic Basecamp request-board slice stable enough to trust and reuse.

### Landed in this pass
- regression coverage for spoken-board parsing and request references
- short spoken board barks instead of only dry bookkeeping text
- harness verification that builds, launches, and loads the known save
- harness filtering for inherited startup-noise lines so the checks focus on real regressions

### Why this milestone matters
This is the spine that v2 will stand on.
If the deterministic queue / board / retry / approval flow is not stable, any LLM layer on top will just become expensive confusion.

---

## Milestone 2: Basecamp AI v2 interface sketch

This is the current active design problem.

### Goal
Define the hybrid interface between:
- the deterministic Basecamp resolver, and
- the personality-rich LLM reply/action layer.

### Target shape
When the player says something at camp:
1. choose the relevant camp NPC voice
2. build a compact follower-style social snapshot
3. include ranked deterministic action candidates
4. let the LLM return:
   - one spoken reply
   - one constrained action token
5. apply that action token through deterministic code

### Constraints
- no minimap
- no raw inventory dump
- no giant recipe dump
- no XML circus if simple `key: value` lines are enough
- keep the snapshot close in style to the existing follower-NPC snapshot
- keep the prompt close in style to the existing follower-NPC prompt

### Candidate action-token shape
For the first craft-oriented version, the model should only choose from a narrow legal menu such as:
- `craft_go:candidate_n`
- `craft_wait:candidate_n`
- `status:request_n`
- `status_all`
- `approve:request_n`
- `cancel:request_n`
- `clarify`
- `idle`

The deterministic layer should prepare the legal tokens; the model should not invent its own.

### Candidate snapshot shape
The snapshot should stay lean and reuse proven follower-style fields where possible:
- player name / utterance / utterance present
- `your_name`
- `your_profession`
- `your_tone`
- `your_example_expression`
- `your_recent_memories`
- `your_state[0-10]`
- `your_emotions[0-10]`
- `your_personality[0-10]`
- `your_opinion_of_player[0-10]`
- compact camp context lines
- ranked craft / board candidates
- allowed action list

### Matching policy for craft candidates
The deterministic matcher should hand the model only a small ranked set:
- top 5 candidates max
- phrase-aware scoring, not just bag-of-words matching
- longer specific phrase matches should beat generic noun matches
- example: `boiled bandages` should rank above plain `bandages` when both are valid

### Why this milestone matters
This is the bridge between:
- useful deterministic camp automation, and
- the "alive" social feeling Josef actually wants from camp NPCs.

---

## Milestone 3: Broaden Basecamp action tokens beyond crafting

Useful next, but only after the v2 interface is nailed down.

### Goal
Extend the same constrained-token pattern from craft requests to broader camp management.

### Candidate scope
- build / construction requests
- board-side request queries and follow-ups
- local mission dispatch
- camp capability questions such as:
  - "what can we make?"
  - "what is blocked?"
  - "who is free?"
  - "what is queued?"
- richer disambiguation and follow-ups like:
  - "make 5 more"
  - "cancel request 12"
  - "status on 7"

This is where Basecamp starts becoming a real camp foreman interface rather than just a craft clipboard.

---

## Milestone 4: Board QoL and deeper feasibility summaries

Also real, also not first.

### Goal
Make the board easier to read and explain why a request is or is not startable.

### Candidate scope
- sort active requests ahead of archived ones
- clearer one-line status labels
- clearer blocker summaries
- clearer ETA display
- clearer assigned-worker display
- better grouping for active / blocked / archived requests
- bulk actions that stay sane:
  - clear completed
  - approve all ready
  - retry blocked
- per-request feasibility summaries for:
  - missing tools
  - missing ingredients
  - liquid storage blockers
  - estimated work time
  - likely best worker
  - subcraft / recursive planning hints

This is valuable glue, but it should follow the v2 interface design instead of happening in parallel confusion.

---

## Not-now parking lot

These are legitimate ideas that should **not** displace the current hardening pass.

### Action-status / failure-reason layer
Still worthwhile as a broader architecture direction, especially for:
- `look_around`
- `look_inventory`
- `attack=<target>`

But it is not the current stretch.
The Basecamp board is already real and needs stabilization first.

### Broader automation harness growth
Also worthwhile, especially for:
- richer smoke scenarios
- fixture-save management
- debug-menu scenario setup
- broader release validation

But the first harness slice already exists and is good enough for the current Basecamp finish-line work.
Do not let harness ambition eat the whole schedule again.

### Curated summary coverage / content polish
Nice side work, not the structural priority.
Flavor can continue opportunistically, but it should not crowd out code hardening.

---

## Recommended implementation order

### First
- Lock the follower-style Basecamp v2 snapshot field list.
- Lock the matching prompt format.
- Lock the first constrained action-token grammar.

### Second
- Implement the deterministic candidate-prep layer for v2:
  - top-5 craft candidates
  - phrase-aware ranking
  - legal action-token generation

### Third
- Let the LLM choose between reply + constrained action token, while deterministic code still executes the actual result.

### Fourth
- Expand the same pattern to build / mission / broader board actions.

### Fifth
- Improve board QoL and deeper feasibility summaries once the interface stops moving.

That is enough work already.
There is still no need to turn one successful half-day into a full municipal bureaucracy.
