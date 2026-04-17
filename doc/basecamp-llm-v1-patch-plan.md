# Basecamp LLM Tool v1 Patch Plan

This document is the concrete patch-plan draft for a first pass of **LLM-assisted Basecamp work requests**.
It is not the vague dream version. It is the version that tries to answer:

- what should the player be able to say,
- what part should the LLM do,
- what part must stay deterministic,
- what should appear on the bulletin board,
- and in what order this can be landed without turning Basecamp into a haunted spreadsheet.

Related docs:
- Roadmap: [`../Plan.md`](../Plan.md)
- Current harness notes: [`OPENCLAW_HARNESS.md`](OPENCLAW_HARNESS.md)

---

## Implementation goal

Allow a Basecamp-assigned NPC who hears a natural player utterance to create and manage a **camp work request** through the existing Basecamp ecosystem.

For v1, the focus is **crafting requests**:
- player says something like `make 40 bandages`
- Basecamp hearing path routes the utterance into a specialized Basecamp LLM tool chain
- the system selects a recipe candidate intelligently
- deterministic code checks feasibility and planning facts
- a request appears on a **bulletin-board scratchpad**
- feasible work can be queued / started through existing camp crafting machinery
- blocked work remains visible, with reasons and a player-facing note

This should feel like:
- the camp has a foreman board,
- workers hear requests,
- planning happens visibly,
- and the player can inspect / cancel / track progress instead of trusting invisible camp telepathy.

---

## Strong design rules

## 1. The LLM is the foreman, not the accountant
The LLM should:
- parse utterance intent
- choose among candidate recipes
- write short camp-board notes in personality/voice
- optionally decide whether a feasible request should auto-start or wait for approval

The deterministic layer must:
- gather candidate recipes
- check recipe availability
- check skills
- check books
- check tools
- check ingredients
- check food state
- check worker availability
- score worker candidates
- build recursive subplans
- start the actual Basecamp mission

If the LLM is allowed to improvise material logic, it will hallucinate pliers and pretend that rags are a state of mind.

## 2. Bulletin board scratchpad is the source of truth
The player should not need to remember what was asked five minutes ago.
Requests should be visible on the board with status, blockers, assignment, and progress.

## 3. Prefer deterministic facts + personality-flavored notes
The board entry should have hard facts from deterministic code.
The final explanatory/player-facing note can be authored in an NPC-ish voice.

## 4. Start with crafting only
Basecamp will likely get more LLM-enabled workflows later.
That is fine.
Do not design v1 as if it already has to solve every camp task class.
Crafting is enough to prove the pattern.

## 5. Keep recursion bounded
Recursive subcraft planning is valuable, but dangerous.
For v1:
- recursion depth max = 2
- no cycles
- no giant global planner

That already gets a lot of useful behavior.

---

## Current Basecamp code map

From the existing code shape, the main anchors are already present.

### Basecamp data / crafting inventory / camp storage
- `src/basecamp.h`
- `src/basecamp.cpp`
  - `form_crafting_inventory()`
  - `add_available_recipes(...)`
  - camp storage / requirement / recipe helpers

### Bulletin board / mission UI / camp control flow
- `src/faction_camp.cpp`
- `src/mission_companion.cpp`

### Companion mission launch + return plumbing
- `src/faction_camp.cpp`
  - `start_crafting(...)`
  - crafting mission return paths
- `src/talker_npc.cpp`
  - companion mission hook surface

This is good news.
The feature should sit **on top of** this machinery, not beside it.

---

## Problem statement refined

A Basecamp crafting request is constrained by at least these factors:

1. **recipe availability**
   - worker knows it from skill progression
   - or required book exists in camp area

2. **worker skill fit**
   - can craft it at all
   - how fast / how competently

3. **tools in camp area**
   - plus camp pseudo-tools/upgrades

4. **ingredients in camp area**

5. **food state**
   - not a hard blocker by default
   - but should influence planning tone / willingness / urgency notes

6. **tool hoarding by camp-assigned NPCs**
   - camp workers often carry relevant tools and starve the shared work pool
   - auto-drop into a camp tool pool is valuable

7. **worker availability / active jobs**

8. **multiple recipe candidates for the same concept**
   - e.g. different bandage recipes are meaningfully different
   - recipe choice is not “first recipe containing the string”

---

## Core user stories

### Story A: direct feasible craft
Player says:
- `Make 40 bandages.`

Flow:
- basecamp NPC hears it
- candidate recipes for “bandage” are prepared
- LLM chooses best candidate from facts
- deterministic layer checks feasibility
- board entry appears
- request either auto-starts or waits for approval depending on policy
- assigned worker leaves to do the job

### Story B: blocked but understandable
Player says:
- `Make 40 bandages.`

Flow:
- best recipe chosen
- blocked because no rags / no book / no suitable worker / missing tool
- request stays on board
- scratchpad note explains what is missing
- player may later fix the blocker and retry / approve / keep queued

### Story C: recursive subcraft planning
Player says:
- `Make 40 bandages.`

Flow:
- chosen recipe needs rags
- not enough rags in camp storage
- rags are themselves craftable from a camp-available chain
- planner creates subrequest(s) up to depth 2
- board shows parent + child relationship

### Story D: cancel via utterance or board
Player says:
- `Scrap that plan for the bandages.`

Flow:
- system resolves which active board request that means
- board entry removed / cancelled
- queued work or unstarted subrequests are cancelled
- if something is already in progress, status changes appropriately instead of vanishing like a liar

---

## High-level architecture

## Layer 1: Hearing + request routing
A Basecamp-assigned NPC hearing an utterance should be able to route into a **Basecamp tool chain** instead of the normal follower-style action pipeline.

Suggested gate:
- NPC is assigned to a Basecamp
- player is within hearing range / local enough to matter
- Basecamp bulletin board exists / request system enabled
- Basecamp hearing should override ordinary follower-hearing for these camp-work intents

Output:
- a `camp_request_heard` event / request draft

## Layer 2: Intent parse
The LLM parses the utterance into a small structured intent.

Example output shape:
```json
{
  "kind": "craft_request",
  "item_query": "bandage",
  "count": 40,
  "allow_subcraft": true,
  "approval_preference": "auto_if_clear"
}
```

This stage should stay small and boring.
Do not let it invent mission ids or recipe ids from the void.

## Layer 3: Deterministic candidate recipe generator
Given `item_query = bandage`, deterministic code should gather a small candidate set.

Each candidate entry should include facts like:
- recipe id
- displayed product name
- output count
- time estimate
- required skills
- tool requirements
- ingredient requirements
- whether required book is available
- whether any camp worker can perform it
- whether directly craftable now
- whether recursively craftable within depth limit

This keeps the LLM from rummaging blindly through the whole recipe graph.

## Layer 4: LLM candidate chooser
The LLM chooses the best candidate **from the prepared set**.

This is where nuanced semantic choice belongs.
For example:
- pick `boiled bandage` over inferior alternatives if the request and camp state support it
- prefer the version that best matches what the player probably meant

This is a good LLM job.
It is meaning-sensitive, but bounded.

## Layer 5: Deterministic planner / feasibility engine
Once a candidate recipe is chosen, deterministic code computes:
- worker assignment candidates
- blockers
- tool-drop needs
- subcraft chain (depth <= 2)
- estimated duration
- food pressure note inputs
- whether the job is:
  - auto-startable
  - queueable pending approval
  - blocked

## Layer 6: Bulletin board scratchpad
The board stores and shows the request state.

## Layer 7: Mission execution
If approved / auto-started:
- prepare tool and ingredient state
- call existing camp crafting mission start flow
- track active mission and ETA

## Layer 8: Completion / return
Mission returns update:
- request status
- result notes
- child/parent request links
- board cleanup/archive

---

## Proposed data model

## `camp_llm_request`
Suggested fields:
- `request_id`
- `camp_id`
- `created_turn`
- `source_utterance`
- `heard_by_npc_id`
- `board_voice_npc_id`
- `assigned_worker_npc_id`
- `request_kind` (`craft_request` for v1)
- `requested_item_query`
- `requested_count`
- `chosen_recipe_id`
- `status`
- `approval_state`
- `active_mission_id`
- `eta_turn`
- `food_days_snapshot`
- `food_mood_hint`
- `notes`
- `blockers`
- `subrequests`
- `parent_request_id`
- `children_request_ids`

### `status` values (suggested)
- `heard`
- `planning`
- `awaiting_approval`
- `queued`
- `blocked`
- `in_progress`
- `waiting_subcraft`
- `completed`
- `cancelled`
- `failed`

### `approval_state` values (suggested)
- `not_needed`
- `suggested_auto`
- `waiting_player`
- `approved`
- `rejected`

## `camp_llm_blocker`
Suggested fields:
- `kind`
- `detail`
- `severity`
- `can_auto_resolve`

Suggested blocker kinds:
- `missing_recipe`
- `missing_book`
- `insufficient_skill`
- `missing_tool`
- `tool_held_by_worker`
- `missing_component`
- `worker_busy`
- `food_low`
- `subcraft_possible`
- `no_valid_worker`
- `approval_required`

## `camp_llm_note`
Suggested fields:
- `kind` (`ack`, `plan`, `blocked`, `progress`, `completion`)
- `author_npc_id`
- `text`
- `created_turn`
- `deterministic_facts` (optional structured summary for debugging)

---

## Bulletin board UX shape

## New submenu / top-level board area
Suggested labels:
- `Scratchpad`
- `Camp Requests`
- `Work Orders`

The board should list active requests like:

- `#12 Make 40 bandages` — `blocked`
- `#13 Craft 2 wooden spears` — `in progress`
- `#14 Make 100 clean water` — `awaiting approval`

Selecting one should show:
- original utterance
- chosen recipe
- assigned worker
- blockers / substeps
- ETA if active
- recent notes in NPC/camp voice

## Manual player operations
At minimum the player should be able to:
- inspect request
- approve request
- cancel request
- clear completed/abandoned entry

And yes, cancellation by utterance should also exist later:
- `scrap that bandage plan`
- `cancel Ricky's spear job`

The board should remain the canonical state even if cancellation is initiated by speech.

---

## Board voice vs worker assignment
These are different decisions.

## Board voice / who writes on the scratchpad
Suggested policy:
1. if there is an explicit camp leader / foreman, use them
2. otherwise choose a stable “camp clerk”/leader based on traits or camp role
3. otherwise fall back to the NPC who heard the utterance
4. otherwise fall back to the assigned worker

This keeps note voice consistent instead of flickering randomly between whoever happened to be nearest.

## Worker assignment tiebreakers
Suggested deterministic ranking:
1. recipe available to worker
2. can satisfy skill requirements
3. idle / not already committed
4. shortest ETA
5. highest relevant skill
6. least conflict with active jobs
7. hearing NPC gets small preference only after competence/availability

This prevents nearest-neighbor stupidity.

---

## Tool pooling / auto-drop policy

Your added requirement is correct: Basecamp-assigned NPCs frequently hoard tools that the camp should really be able to use.

## Proposed rule
When a tool is needed for a camp request and a Basecamp-assigned NPC currently:
- carries it,
- wields it,
- or wears it (where relevant),

then deterministic camp logic should try to deposit it:
1. in the camp **tool zone** if one exists
2. else in the center of the Basecamp area if valid
3. else at the NPC’s feet

Scope this to:
- Basecamp-assigned NPCs only
- camp work / camp request resolution only

Do **not** globally strip tools off everybody all the time.
That way lies legitimate mutiny.

---

## Food model

Food should be part of planning context, but not a hard blocker by default.

## Proposed handling
Deterministic planner computes:
- `camp_food_days`
- maybe `camp_food_margin_state` such as:
  - `comfortable`
  - `watchful`
  - `lean`
  - `anxious`
  - `critical`

This should feed into:
- board notes
- LLM voice/tone
- possibly approval recommendation

Example deterministic fact:
- `camp_food_days: 0.8`
- `food_state: anxious`

Example note flavor:
- `Can do it, but this camp is running lean. We should not start three comfort jobs while the pantry is dying.`

For v1, low food should **warn and sour tone**, not hard-block ordinary crafting automatically.

---

## Approval policy

You suggested this might itself be an LLM decision.
I think the safest version is:

### Deterministic layer computes recommendation facts
- request is trivially feasible / low-risk
- request is feasible but expensive
- request is recursive / multi-step
- request is blocked
- request is food-risky

### LLM can recommend
- `auto_start`
- `queue_for_board_approval`
- `blocked_note_only`

### Final v1 policy suggestion
- if direct and clearly feasible: may auto-start
- if recursive / expensive / food-lean / tool displacement required: queue for board approval
- if blocked: keep on board as blocked

This gives the system some judgment without letting it quietly overcommit the whole camp.

---

## Reactive utterances and state sensing

This should not be board-only.
Basecamp NPCs should also react verbally when relevant.

Examples:
- heard request acknowledgment
- blocked complaint
- low-food grumbling
- assignment confirmation
- completion return line

But the board remains the memory.
Reactive utterances are flavor + immediate feedback, not the durable record.

---

## Recipe selection details

This is one of the most important parts.

## Deterministic candidate generation
For a request like `bandage`, gather a small list of candidate recipes.
Each candidate should include:
- output item id / name
- output count per craft
- relevant quality tier / variant note if useful
- direct craftability now
- recursive craftability within depth 2
- required book availability
- required skill availability
- tool availability
- ingredient availability
- candidate worker set
- ETA range

## LLM choice prompt shape
The LLM should receive:
- original utterance
- camp state summary
- maybe board context
- candidate recipe table

And respond with something like:
```json
{
  "chosen_recipe_id": "bandage_boiled",
  "reason": "Closest match to requested medical bandages; camp can support it with current tools and skills.",
  "approval_preference": "queue_for_board_approval"
}
```

That is a bounded, useful use of the model.

---

## Recursive planning v1

### Policy
- max depth = 2
- no cycles
- no global optimization fantasy

### Behavior
If parent request cannot be directly crafted but a required intermediate can be:
- create child request(s)
- mark parent `waiting_subcraft`
- show relationship on board

Example:
- Parent: `Make 40 bandages`
- Child: `Craft 8 rags`
- Parent status: `waiting_subcraft`

Auto-queue is fine, but approval may still be required depending on policy.

---

## Cancellation model

Requests should stay on the board until:
- completed and optionally archived
- manually removed
- cancelled

And yes, the player should be able to cancel via utterance eventually.

Example:
- `Scrap that plan for the bandages.`

Suggested resolution order:
1. exact active request match
2. best fuzzy match among active requests
3. if ambiguous, write clarification note instead of deleting the wrong thing

---

## Proposed patch series

## Patch 1: request data scaffolding
Files likely:
- `src/basecamp.h`
- `src/basecamp.cpp`
- maybe `src/faction_camp.h/cpp`

Add:
- `camp_llm_request`
- blocker/note structs
- request storage on camp state
- save/load/serialization path if needed

## Patch 2: board scratchpad UI
Files likely:
- `src/faction_camp.cpp`
- `src/mission_companion.cpp`

Add:
- new bulletin board submenu/tab
- list active requests
- inspect request details
- approve/cancel/clear actions

## Patch 3: hearing + request creation hook
Files likely:
- NPC hearing / LLM intent routing files
- maybe `src/llm_intent.cpp`
- maybe talker / camp NPC plumbing

Add:
- basecamp-aware utterance hearing path
- request draft creation
- board acknowledgment note

## Patch 4: deterministic candidate recipe gathering
Files likely:
- `src/basecamp.cpp`
- maybe dedicated helper file if it grows

Add:
- item-query -> candidate recipe set
- recipe/book/skill/tool/ingredient facts
- worker candidate facts

## Patch 5: LLM candidate chooser tool
Files likely:
- LLM tool plumbing / prompt definitions
- probably new prompt/tool path rather than follower snapshot path

Add:
- structured candidate chooser prompt
- recipe choice + rationale + approval suggestion

## Patch 6: deterministic planner and worker assignment
Files likely:
- `src/basecamp.cpp`
- `src/faction_camp.cpp`

Add:
- worker tiebreaker scoring
- feasibility resolution
- blockers
- ETA
- food-state facts

## Patch 7: mission launch integration
Files likely:
- `src/faction_camp.cpp`
- existing `start_crafting(...)` call path

Add:
- request -> crafting mission bridge
- request status updates
- mission id / eta tracking

## Patch 8: tool auto-drop pooling
Files likely:
- basecamp / NPC inventory handling points

Add:
- when required tool is held by a Basecamp worker, deposit to tool zone / camp center / feet
- log / note what happened

## Patch 9: recursive subcraft depth 2
Add:
- child request creation
- parent waiting state
- approval rules

## Patch 10: reactive utterances and cancellation by speech
Add:
- acknowledgment lines
- blocked/grumpy lines
- completion lines
- `scrap that plan` style utterance handling

---

## Open questions to resolve during implementation

1. how exactly should board approval UX look when a request is feasible but not auto-startable?
2. should board notes always be from the same camp leader voice, or may they vary by request type?
3. what is the smallest useful candidate recipe set size for LLM choice?
4. should low-food auto-start suppression happen at `lean`, `anxious`, or only `critical`?
5. how should ambiguous cancellation utterances be disambiguated on the board?
6. should child requests appear flat on the board or nested under the parent?

---

## Practical v1 success criteria

This Basecamp LLM tool v1 is a success if all of these become true:
- a natural camp crafting utterance creates a visible scratchpad request
- the chosen recipe is selected from a bounded candidate set, not guessed blindly
- deterministic planning explains blockers correctly
- a feasible request can be started through existing camp crafting machinery
- blocked requests stay visible on the board until resolved/cancelled
- food pressure affects tone/notes without causing nonsense hard-blocks
- Basecamp workers stop pointlessly hoarding the exact tools the planner needs
- at least one cancel-via-board path exists

If those are true, the system is already interesting, legible, and worth expanding.
