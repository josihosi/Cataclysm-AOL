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
**Status:** JOSEF TESTING — PRIMARY FINISH LINE

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
- keep the current Basecamp AI focused on crafting / existing camp behavior as it stands right now, not on launching broader in-game jobs through the LLM path yet
- keep human-facing camp bark informal and survivor-like: no job numbers, no filing-cabinet tone, no faux military command-center voice
- eventual prompt/snapshot growth can happen later, but today the finish line is to make the existing Basecamp AI feel right

#### Current active sub-item
- [x] do one more bark pass on Basecamp craft / board / blocker replies so they sound like a loose apocalyptic camp, not a bureaucracy
- [x] remove or suppress job-number-forward phrasing from human-facing bark where possible
- [x] keep the richer overmap snapshot out of the short spoken bark path; for now, spoken replies stay concise/human
- [ ] put the current bark pass in front of Josef for a short live tone/feel packet on craft / board / blocker replies
- [x] follower NPC movement already uses the signed coordinate contract (`move=<dx>,<dy> <state>`) instead of the old `E E E N`-style step spam

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

### 3. Harness observability / deterministic probe slice
**Status:** GOOD ENOUGH FOR NOW (park the remaining tail unless it directly blocks game work)

This slice did enough useful work to stop being the primary finish line.
The immediate goal was not a grand harness architecture. It was to reduce testing confusion enough that real game work could continue.
That goal is met well enough for now, even though a few cleanup edges remain.

#### Current direction
- stick to the symbolic/stateful direction from the parked harness note instead of drifting toward screenshot-first automation
- make live speech probes deterministic and explicit about which path is being exercised (`say` vs `yell`)
- make board/base-AI snapshot replies land in a reliable artifact/log path instead of forcing archaeology after every probe
- keep three evidence classes separate:
  - live on-screen behavior
  - deterministic test contract
  - artifact/log visibility
- improve the testing path enough to unblock real game work, not enough to become a week-long harness theology project

#### Current result
- [x] add one reliable artifact/log sink for board/base-AI snapshot replies during live probes (`show_board` / `show_job` now emit explicit `camp board reply` / `camp job reply` blocks into `config/llm_intent.log` when `DEBUG_LLM_INTENT_LOG` is enabled; plus live Basecamp hearing now appends `camp heard ... / utterance=...` lines for speech probes that actually reach the camp path)
- [x] make probe runs preserve their evidence class clearly instead of silently switching from GUI/live evidence to PTY/test evidence (the current ledger/test packet now explicitly distinguishes on-screen behavior, deterministic checks, and log visibility)
- [x] record the practical startup truth for this save/profile: `game::load: Finalizing end` is too early as a live-input marker; this bed needs a much longer post-load settle before GUI probes are trustworthy
- [x] make the live probe path explicit and reproducible for `say sentence` vs `yell a sentence` (`C` → `b` opens `say a sentence`; `C` → `y` opens `yell a sentence`; both are now re-proved live on `dev` / `Sandy Creek` after the long settle, using unique board-query utterances and `config/llm_intent.log` checkpoints)
- [x] prove end-to-end where a natural-speech board reply lands (at least one live `show me the board` probe is now confirmed in both places: on-screen message log bark and `config/llm_intent.log` artifact lines)

### 4. Movement-system improvements
**Status:** GREEN (queued behind the current Basecamp bark/feel pass and Locker Zone v1)

This remains important, but it is no longer the immediate finish line.

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
The first structured Basecamp/planner-consumer slice now exists locally on `dev`, and the intended path split is now explicit:
- [x] threaded the overmap snapshot formatter into the **structured / LLM-side** board handoff path instead of leaving it as a dead helper
- [x] kept the signed-axis contract live in prompt text via `planner_move=stay | move_omt dx=<signed_int> dy=<signed_int>`
- [x] kept the same present-only legend / axis-hint snapshot text when the handoff receives a real camp origin
- [x] added deterministic coverage for the structured board-handoff consumer path
- [x] re-ran compile / filtered deterministic tests / startup harness checks after landing the live-consumer slice
- [x] fixed the stale/brittle overmap snapshot expectations in `88f2e3eeb7` (`Stabilize overmap snapshot formatter tests`) after Schani caught the uppercase-horde mismatch
- [x] fixed the remaining structured-board row assertion that still assumed lowercase terrain letters after a forced fresh `cata_test` rebuild showed legitimate horde capitalization on the camp/road row too
- [x] clarified the product shape: live natural speech `show me the board` / `what's on the board` should stay the concise human-facing bark path; the richer 5x5 overmap snapshot belongs in the LLM snapshot/prompt path when deterministic handling falls through to the LLM
- [ ] later, when this movement/LLM path returns to the top of the queue: inject the 5x5 overmap snapshot plus present-only legend into the real LLM snapshot path used when no deterministic craft/board route resolves the sentence
- [ ] later, when that path resumes: make sure the extended terrain-symbol list is handled correctly there too (`b`, `u`, `p`, `k`, `n` in addition to the core set)

#### Next movement continuation
- wire the overmap snapshot into the actual LLM prompt/snapshot assembly path, not into the short human-facing spoken bark
- keep the spoken board reply concise/human unless a later explicit design change says otherwise
- once the LLM path is wired, test the right thing honestly:
  - deterministic formatter/tests for the snapshot block itself
  - prompt/log inspection proving the snapshot entered the LLM call path
  - only then any live smoke about the human-facing reply if that path was touched

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

### 5. Locker Zone v1
**Status:** GREEN later (next feature after the current Basecamp bark/feel pass)

This is the next substantial Basecamp-management feature after the bark polish.
The goal is not "fashion AI." The goal is camp-managed **loadout maintenance**:
NPCs should periodically check a camp locker zone, compare what they are wearing/wielding against camp-approved categories, clean up duplicates, and equip a better set when an obvious upgrade exists.

#### V1 scope
Locker Zone v1 should stay deliberately narrow and useful:
- one camp-wide locker policy, not per-NPC fashion profiles yet
- one candidate zone containing approved loadout items
- one best item per managed category
- duplicate cleanup for managed categories
- simple better/worse evaluation per category
- periodic reevaluation via dirty-flag + cooldown, not constant churn
- melee and ranged weapon slots included

#### Managed categories for v1
- underwear
- socks
- shoes
- pants
- shirt
- vest
- body armor
- helmet
- glasses
- bag
- melee weapon
- ranged weapon

#### Player control (v1)
Use a camp-wide policy menu plus the physical locker zone supply.
The menu should let the player enable/disable broad categories such as:
- armor
- bag
- ranged weapon
- glasses
- etc.

The locker zone contents remain the actual supply.
So policy answers **what categories are allowed**, while the zone answers **what concrete items are available**.

#### Trigger model
Do not make NPCs reconsider clothes every few turns like nervous pigeons.
Use a dirty-flag + cooldown model instead.
Reevaluation should happen when one of these becomes true:
- the NPC is missing a managed category
- the player changes locker policy
- the locker zone gains new eligible items
- the NPC loses or drops important gear
- a scheduled camp maintenance window / daily check occurs

#### Selection rules (v1)
- for each enabled category, keep at most one chosen item equipped/wielded
- if the NPC has multiple equipped items in the same managed category, keep the best and remove the rest
- if the NPC is missing a category, equip the best valid candidate from the locker zone
- if the locker zone contains a meaningfully better candidate than the current item, upgrade
- do not churn for tiny score differences
- do not equip ranged weapons that are unusable in practice just because their headline stats look bigger

#### Better-item evaluation (v1)
Keep scoring simple and category-specific.
Do not try to solve perfect universal outfit optimization yet.
Examples:
- body armor favors protection/coverage heavily
- bags favor storage heavily
- socks/shoes favor valid coverage/warmth/encumbrance sanity
- melee weapons favor obvious usability/damage over junk
- ranged weapons only count as strong candidates if ammo/reload reality makes them actually useful

If a candidate conflicts with the already chosen set, skip it rather than doing heroic combinatorial outfit search.

#### Explicit non-goals for v1
- seasonal outfits
- per-NPC loadout overrides
- role/loadout presets
- advanced layering solver
- style/fashion preference logic
- sending NPCs to arbitrary in-game jobs through this system

#### Deterministic test picture for v1
When this becomes active, the first tests should cover:
- duplicate cleanup (e.g. two socks candidates/categories)
- obvious upgrade selection (`jeans` -> `military cargo pants`-style cases)
- disabled-category policy respected even when items are present
- no-ammo / unusable gun not chosen as the "best" ranged weapon
- cooldown/dirty-flag prevents constant reequip churn
- category-missing trigger causes a reevaluation

#### V2 / V3 are parked on purpose
- **V2 parked:** presets / broader policy variants (light, armored, ranged, hauler, etc.)
- **V3 parked:** seasonal dressing logic (winter coats/blankets, summer shorts/light wear) and per-NPC overrides

Those are good ideas, but they should stay parked until Locker Zone v1 exists and survives contact with real play.

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
Finish the current Basecamp bark/feel pass:
- keep human-facing camp replies informal, survivor-like, and non-bureaucratic
- remove job-number-forward phrasing from bark where possible
- preserve the existing deterministic craft/board behavior while making it sound more human

### Second
Build Locker Zone v1:
- one camp-wide locker policy
- one locker zone supply
- one best item per managed category
- duplicate cleanup
- simple category-specific upgrade logic
- dirty-flag + cooldown reevaluation
- deterministic tests before gameplay polish fantasies

### Third
Return to the movement contract work without erasing the existing system:
- local tactical movement already uses relative signed deltas
- overmap/job movement and the LLM-side 5x5 snapshot path can continue when this returns to the top of the queue
- keep `wait_here` / `hold_position`
- keep deterministic pathing / target-tile logic
- avoid accidental behavioral regressions while changing only the LLM-facing coordinate expression

### Fourth
Continue the richer Basecamp AI on `dev`:
- deterministic-first command extraction
- structured legal action tokens
- deterministic execution of those tokens
- richer snapshot/prompt handoff only when deterministic handling is insufficient

### Fifth
Return to broader camp job types / deeper board QoL once the previous contracts stop moving.

### Good-enough-for-now reference
The small harness observability / deterministic probe slice is no longer the active finish line. Keep its remaining tail parked unless it directly blocks game work again.

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
