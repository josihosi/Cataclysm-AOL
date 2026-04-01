# C-AOL Plan

## What this document is for
This is the working roadmap for the next meaningful stretch of Cataclysm: Arsenic and Old Lace.
It should answer four questions clearly:

1. What just got done?
2. What should happen next?
3. What problems are we actually trying to solve?
4. What can wait until later without causing drama?

This is not meant to be a vague dream board. It is the practical plan for the next iteration.

---

## Current project state

### Recently completed
- Release branches were validated and shipped across the current target set.
- Core branch flow was cleaned up around `dev`, `master`, and the `port/*` targets.
- Background-summary coverage was improved, including curated summaries for several important named NPCs.
- CI was cleaned up so `dev` can be used as the real iteration branch instead of a decorative side alley.

### Current branch policy
- Do active iteration on `dev`.
- When `dev` is in good shape and checks are meaningful, promote `dev` to `master`.
- Use `master` as the source branch for orchestrated propagation to `port/*`.

This is the current intended workflow and should stay the default unless reality proves otherwise again.

---

## Immediate strategic priority

## Build a proper action-status / failure-reason layer before building a larger automation harness

This should happen first.

Why:
- Right now, many NPC action failures still collapse into "it didn’t happen".
- That is bad for gameplay feedback.
- It is bad for debugging.
- It is also bad for automation, because a harness cannot reliably assert on vague behavior.

A test harness without clearer action status would still help, but it would mostly automate confusion faster.
A thin status/observability layer gives the project leverage in three places at once:
- player-facing feedback,
- developer debugging,
- automated testing.

---

## Milestone 1: Action status management and observability

### Goal
Introduce a small common contract for intent-driven actions so each action can report:
- what broad phase it is in,
- why it was blocked or failed,
- what the NPC should say about it,
- and what developers should see in logs/debug output.

Implementation sketch: [`doc/aol-action-status-v1-sketch.md`](doc/aol-action-status-v1-sketch.md)
Patch plan: [`doc/aol-action-status-v1-patch-plan.md`](doc/aol-action-status-v1-patch-plan.md)

### Scope for v1
Start with the actions that currently matter most:
- `look_around` pickup behavior
- `look_inventory` behavior
- `attack=<target>` behavior

These are the most useful first targets because they combine gameplay importance with many failure modes.
The status/failure-reason layer should be designed to port across the active `port/*` branches, even if some branch-specific adaptation is needed during landing.

### Proposed design shape
Use a small shared lifecycle, plus action-specific reason codes.

#### Shared lifecycle states
- `requested` — action was selected or queued
- `precheck` — fast validation before committing
- `executing` — action is actively in progress
- `waiting` — action is temporarily paused but may resume
- `completed` — action succeeded
- `blocked` — action could not proceed because a known condition was not met
- `failed` — action hit an unexpected or invalid execution outcome
- `cancelled` — action was superseded, interrupted, or invalidated by context change

The shared lifecycle should stay small.
Do not turn it into a ministry of sixteen nearly identical states.

#### Reason-code model
Each action should expose its own concrete reason codes under the shared lifecycle.
Examples:

**Pickup / `look_around`**
- `no_inventory_space`
- `too_heavy`
- `cannot_reach_item`
- `item_missing`
- `hostile_threat_nearby`
- `grabbed_or_panic`
- `path_blocked`
- `pickup_rules_disallow`

**Inventory / `look_inventory`**
- `requested_item_missing`
- `cannot_wear`
- `cannot_wield`
- `cannot_activate`
- `inventory_conflict`
- `unsafe_to_manage_inventory`
- `item_already_equipped`

**Attack / `attack=<target>`**
- `target_not_visible`
- `target_out_of_range`
- `friendly_fire_risk`
- `no_viable_weapon`
- `path_to_target_invalid`
- `target_no_longer_valid`
- `morale_or_panic_block`

The important distinction:
- **state** says what broad situation the action is in
- **reason code** says exactly why it is blocked or failed

### Player-facing utterances
Blocked/failed actions should have short, preset utterances so the player is not left guessing.
These should be brief and robust, not over-authored drama.

Examples:
- `no_inventory_space` -> "No room for that."
- `hostile_threat_nearby` -> "Not while that thing's on us."
- `requested_item_missing` -> "Don't have it."
- `friendly_fire_risk` -> "No clear shot."

These utterances should be:
- short,
- reusable,
- easy to map from reason codes,
- and easy to tune later.

### Debug-facing observability
The developer-facing layer should expose more detail than the player utterance.
Examples:
- lifecycle state
- reason code
- action target / item name / target id
- relevant local facts when cheap to compute

Examples:
- `pickup blocked: no_inventory_space item="steel jerrycan" volume_left=0 weight_margin=1`
- `attack blocked: friendly_fire_risk target="zombie dog" ally_in_line=true`

This should go to logs first.
Optional UI surfacing can come later if useful.

### Design constraints
- Keep the common action contract thin.
- Do not try to solve every action in one pass.
- Prefer visible, debuggable failure states over silent fallback behavior.
- Separate player bark text from debug detail.
- Prefer explicit known blocks over generic failure whenever possible.

### Likely first concrete tasks
- Define a lightweight action-status/result structure.
- Thread it through the `look_around` pickup path.
- Add reason-code-to-utterance mapping for pickup.
- Add debug log output for pickup outcome transitions.
- Extend the same pattern to `look_inventory`.
- Extend the same pattern to `attack=<target>`.

### Non-goals for v1
- A complete universal AI planner.
- Perfect handling of every action in the game.
- A giant generalized behavior-tree refactor.
- Rich UI tooling before the status vocabulary is stable.

---

## Milestone 2: Developer harness for repeatable smoke testing

### Goal
Build a practical test harness that can perform repeatable in-game smoke tests without Josef manually driving every keystroke.

The first version does **not** need to autonomously play the game in a broad sense.
It needs to reduce repetitive branch validation and make scenario testing less annoying.

### Why this matters
Current release/testing work still depends too much on manual navigation:
- launching the right build,
- getting through menus,
- loading a save,
- entering a scenario,
- issuing a few test commands,
- checking the visible result.

That is valuable work, but it is exactly the kind of repetitive nonsense a harness should absorb.

### Harness principles
- Start deterministic.
- Prefer prepared saves and scripted scenarios.
- Encode branch-specific menu differences instead of pretending they do not exist.
- Capture evidence when tests fail.
- Grow toward richer setup only after basic smoke tests are reliable.
- Build the first harness path for `master`; do not assume it is automatically a `port/*` concern.

### Practical v1 scope
#### 1. Deterministic checks before broader harness work
Before building a larger interactive harness, add a few deterministic checks against the new action-status output.
That means:
- fixture status logs
- checker assertions on phase/reason outcomes
- and a small stable vocabulary for the first useful blocked/success cases

This is the cheap confidence layer that should come before more expensive menu automation.

#### 2. Branch-aware startup profiles
The harness should know how to do basic startup tasks for each target branch/profile, such as:
- launch executable
- wait for menu readiness
- navigate to load screen
- load a known save
- enter gameplay

This must not depend on Josef supplying keystrokes every single time.
Store branch/profile-specific startup sequences in data/config so they can be adjusted when menus differ.

#### 3. Harness-owned fixture save management
Fixture save handling should be part of the harness, not a separate side ritual.
The harness should be able to:
- install a named fixture save into the correct branch/profile save directory
- load that fixture save as part of startup
- capture/update a fixture save from the active profile when a good debug save has been prepared manually
- keep fixture saves branch-aware instead of pretending one save is automatically valid across every profile

The branch/profile save split already exists in the local build flow, so the missing work is turning that into an explicit harness subsystem.

#### 4. Deterministic scripted smoke tests
Use prepared saves and repeatable command scripts for checks like:
- game launches successfully
- save loads successfully
- player can enter world and issue speech/action prompts
- NPC receives command opportunity
- target action either succeeds or returns an expected blocked reason

#### 5. Artifact capture
On failure, capture enough evidence to debug quickly:
- screenshot(s)
- harness log
- branch/profile used
- test step where failure occurred
- if possible, relevant game debug/log output

### Strongly desired capability
Being able to **see** the game window or menu state during harness development would help enormously.
For authoring and debugging the harness, visual inspection is far more useful than blind keystroke playback.

That means the long-term harness should ideally support some combination of:
- window screenshots,
- OCR or menu-state recognition,
- or a direct view/control loop via desktop automation.

This is not strictly required for the first automation pass, but it is highly desirable and worth planning around.

### Planned harness growth path
#### Phase 1: Menu/load automation
- Launch game
- Load known save
- Reach playable state
- Record success/failure cleanly

#### Phase 2: Basic smoke scenarios
- Issue a command
- Observe whether expected action state/result appears
- Assert on simple outcomes

#### Phase 3: Debug-menu scenario setup
- Start game
- open debug menu
- spawn items/NPCs/hostiles as needed
- run narrow action tests without depending entirely on handcrafted saves

#### Phase 4: Broader scenario library
- branch validation suite
- action regression suite
- release smoke suite
- targeted bug-repro scripts

### Relationship to Milestone 1
The harness becomes much more useful once actions return structured outcomes.
Examples:
- bad: "NPC did something weird"
- good: `blocked: hostile_threat_nearby`
- better: `blocked: hostile_threat_nearby`, bark shown, debug facts logged

That is why status management should come first.

---

## Milestone 3: Continue curated summary coverage, but as side work

This remains worthwhile, just not the main architectural milestone.

### Why keep doing it
- It directly improves NPC flavor and context quality.
- It is relatively low-risk compared to code/system work.
- It is easy to chip away at between heavier programming tasks.

### Suggested focus
Continue curated summaries for important named NPCs and other high-visibility content clusters.
A likely next high-value pass is the refugee-center cast, following the recent Hub/Robofac-related coverage.

### Important limitation
Summary/content polish should not displace the deeper work on action clarity and automation.
It is seasoning, not structural timber.

---

## Release/testing direction after the next milestone

Once the status layer and first harness slice exist, the release loop should look more like this:

1. Iterate on `dev`.
2. Run meaningful checks and smoke scenarios there.
3. Use harness-assisted validation instead of mostly manual repetition.
4. Promote `dev` to `master` when stable.
5. Use the orchestrator flow to propagate to `port/*`.
6. Run target-specific smoke checks with branch-aware harness profiles.

That should reduce both manual burden and the number of mysteries during release prep.

---

## Open design questions
These need answers during implementation, not necessarily all up front.

### For action status management
- How much detail should be logged by default versus behind debug toggles?
- Should blocked-action utterances be fully generic, partially character-flavored, or branch on NPC tone/personality later?
- Which failure states should retry automatically, and which should hard-stop immediately?
- How should queued multi-step actions expose intermediate state without becoming noisy?

### For the harness
- What desktop/view/control method is most robust across the current development environment?
- How should branch-specific menu differences be represented: data files, script tables, or code-level profiles?
- Which release smoke tests deliver the best confidence per minute spent?
- How much of the harness should live inside the repo versus external automation glue?

---

## Recommended next implementation order

### First
- Define the status/result contract.
- Apply it to pickup.
- Add utterance mapping and debug output for pickup.

### Second
- Extend the same model to `look_inventory`.
- Extend the same model to `attack=<target>`.

### Third
- Add 2-4 deterministic action-status fixtures/checks so the new reason-code layer is already testable before broader automation lands.

### Fourth
- Build the smallest useful branch-aware harness that can launch the game, install/load a fixture save, and reach a known playable state without Josef hand-holding the keystrokes.

### Fifth
- Add 1-3 real smoke scenarios that assert on the new action result layer.

### Sixth
- Expand curated summaries in parallel where useful.

---

## Practical success criteria

This stretch is successful if all of the following become true:
- Failed actions no longer feel like silent nonsense.
- The player gets short, understandable refusal/blocked feedback.
- Developers can see why an action failed without guesswork.
- At least one branch-aware harness path can launch the game and load a save automatically.
- At least a few key action scenarios can be tested repeatably.

If those are true, the next round of development gets faster, safer, and much less irritating.
