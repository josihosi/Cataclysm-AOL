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

## Harden Basecamp AI v1 so it stays working

The Basecamp request-board slice exists now.
That changes the priority.
The biggest risk is no longer "can we build this at all?".
It is now:
- silent regressions,
- weak feedback when camp staff interprets orders,
- and future edits breaking the board/speech path because nothing pins behavior down.

So the next pass should be a **small hardening pass**, not a giant new feature binge.

---

## Milestone 1: Basecamp AI hardening pass

### Goal
Make the new Basecamp board / speech layer feel finished enough to trust:
- covered by targeted tests,
- a little more alive in speech,
- and easier to understand when something is queued, blocked, or launched.

### Active scope
This is what we are doing now.

#### 1. Add regression coverage for spoken-board parsing and request flow
Targeted tests should cover the highest-value inputs first:
- craft requests
- cancel requests
- approve / launch requests
- status queries
- request-number references
- "all ready work orders" style commands

The goal is not a perfect universal test matrix.
The goal is to stop the obvious speech/board grammar from quietly rotting.

#### 2. Add short practical barks for the spoken board workflow
Camp staff should give short useful spoken feedback for:
- request heard and pinned
- request blocked
- request launched
- request cancelled
- board empty / board summary
- ambiguous or missing request references

Keep these short and functional.
This is not a radio drama.

#### 3. Tighten the finish-line verification loop
The working finish line for this slice remains:
- implemented on `dev`
- compiles
- game launches
- save loads successfully
- no crash
- no Basecamp-specific debug/pop-up nonsense from this feature path

The harness should remain strict enough to catch real problems without treating inherited upstream startup noise like divine punishment.

### Practical success criteria for this milestone
This hardening pass is successful if:
- the main spoken board commands have regression coverage,
- the camp gives short readable feedback instead of only third-person bookkeeping,
- the latest `dev` build still compiles,
- the game launches and loads the known save,
- and the remaining harness noise is generic startup clutter rather than fresh Basecamp breakage.

---

## Milestone 2: Basecamp AI quality-of-life pass

This is the next logical stretch **after** the hardening pass, not now.

### Goal
Make the board faster to read and less annoying to operate.

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

This is boring UI glue, which is exactly why it matters.

---

## Milestone 3: Better spoken disambiguation and camp-side queries

Useful, but explicitly **after** the hardening and QoL passes.

### Goal
Make spoken Basecamp control less brittle and more helpful when the player is vague.

### Candidate scope
- disambiguation when multiple recipes or work orders match
- richer quantity parsing
- follow-ups like:
  - "make 5 more"
  - "cancel request 12"
  - "status on 7"
- camp capability queries such as:
  - "what can we make?"
  - "what is blocked?"
  - "who is free?"
  - "what is queued?"

This is where the board starts feeling like a foreman tool instead of just a smarter clipboard.

---

## Milestone 4: Deeper feasibility / planning summaries

Also real, also not now.

### Goal
Expose why a request is or is not startable without making the player perform archaeology.

### Candidate scope
Per-request summaries for:
- missing tools
- missing ingredients
- liquid storage blockers
- estimated work time
- likely best worker
- subcraft / recursive planning hints

This would be valuable, but it is also where complexity starts breeding in dark corners.
Do this only once the current board behavior is stable and well-covered.

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
- Add targeted regression tests for spoken Basecamp request parsing and board references.

### Second
- Add short useful barks for the spoken board workflow.

### Third
- Re-run compile + startup/load verification on `dev` and confirm the feature path stays clean.

### Fourth
- If that pass is stable, move to board QoL improvements from Milestone 2.

That is enough work already.
There is no need to turn one successful half-day into a full municipal bureaucracy.
