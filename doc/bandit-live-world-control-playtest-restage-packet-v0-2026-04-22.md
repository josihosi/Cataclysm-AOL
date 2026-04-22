# Bandit live-world control + playtest restage packet v0 (2026-04-22)

Status: ACTIVE / GREENLIT.

## Greenlight verdict

Josef corrected the product bar plainly: this seam is not allowed to stop at one showcase anchor, one cute harness trick, or one disconnected proof packet.
If Andi's bandit work is meant to exist in the real game, the live bandit spawns themselves need to come under that system, and the resulting setup needs to be easy enough to restage nearby that Schani and Josef can actually playtest it and measure turn cost.

This packet therefore absorbs the remaining useful playtest-kit-v2 helper work instead of killing it, but makes that helper surface subordinate to the real product: live owned bandit spawns with controllable nearby restage.

## Why this packet exists

The current repo has real hostile bandit content in play, but it is still mostly static-content / mapgen footing.
The newer `src/bandit_*` stack has evaluator, mark-generation, and handoff substrate, but it does not yet honestly own the bandits that spawn into the world.

That gap is now the real product problem.
A broad playtest harness without live ownership would just be more elegant theater.
Likewise, a live owner without a restageable near-player setup would still leave playtesting and perf measurement too awkward to use.

So this packet binds the two needs together:
- bring the real current bandit spawn families tied to this lane under one live owner/control path
- make that live system deliberately restageable near the player/basecamp for repeatable testing and performance measurement

## Scope

1. Connect the relevant current bandit spawn families tied to this lane — including the real overmap-special and map-extra style bandit site/spawn footing — to one live saveable owner instead of leaving them as disconnected static spawns.
2. Track explicit per-site and per-spawn-tile bandit headcount / membership strongly enough that the system can reason about who exists there and what has changed.
3. Let the live system actually control, dispatch, reconcile, and cool those spawned bandits through the existing bandit evaluator / handoff substrate instead of only counting them after they already spawned.
4. Make dispatch size depend on the live remaining site population rather than a fixed folklore count, so site-backed camps keep a home presence and kills/losses shrink later outings.
5. Add one bounded cadence/tick and persistence path so the live owner survives save/load and updates on the real world side rather than only in playback/tests.
6. Add a bounded playtest restage helper that can intentionally seed a controlled bandit camp about `10 OMT` away from the player or current basecamp footing.
7. Split that restage helper into two honest modes:
   - **probe / capture mode** may collect artifacts and clean up afterwards
   - **playtest handoff mode** must leave the game/session running after setup instead of auto-terminating it
8. Produce a reviewer-clean performance packet on that real nearby setup, including baseline single-turn cost, wait/pass-time cost, bandit-cadence turn cost, spike ratio, and max turn cost.
9. Carry forward only the harness/helper work from `Bandit + Basecamp playtest kit packet v2` that directly serves this live-world-control packet.

## Non-goals

- every hostile human system in the whole game
- a giant generic map-authoring or scenario-authoring empire
- harness-only theater that still leaves live mapgen bandits unmanaged
- a full faction grand-strategy rewrite
- perfect declarative world authoring for every possible state on day one
- deleting useful playtest-kit helper work just because the headline lane changed

## Success shape

This packet is good enough when:
1. one live saveable owner exists for the relevant current bandit spawn families tied to this lane
2. the owner keeps explicit per-site / per-spawn-tile headcounts and membership rather than treating spawned bandits as anonymous folklore
3. the system can actually control or dispatch those spawned bandits through the live world path instead of leaving them as disconnected `place_npcs` islands
4. dispatch size comes from the live remaining site population strongly enough that site-backed camps keep a home presence and later losses shrink future outings
5. a controlled bandit camp can be restaged about `10 OMT` away on demand on current build
6. that restage can be used both for reviewer capture and for manual playtesting, with the manual handoff mode **not** auto-terminating the session
7. the setup exercises the real overmap/bubble handoff plus local writeback path rather than a fake isolated theater
8. local outcomes change later world behavior instead of leaving the live owner stale, including later site size / dispatch capacity
9. a reviewer-clean perf report exists for that nearby live setup, with the concrete metrics above plus at least one honest upper-pressure stress pass
10. the slice stays bounded instead of widening into a generic hostile-human or debug-platform empire

## Validation expectations

This packet touches real game-state ownership, persistence, live restage, and likely some supporting harness glue.
So validation should stay explicit and mixed:

- deterministic bridge coverage for the live owner / headcount / save-load path where practical
- narrow deterministic coverage for any helper logic that can be tested outside the full live run
- fresh current-build live proof that the nearby controlled camp can be restaged on demand
- fresh current-build proof that the manual handoff path leaves the playtest session running after setup
- reviewer-readable evidence that the live system, nearby restage, and later writeback all exercised the real path instead of a private fake path
- a concrete perf packet/report on the nearby setup using baseline single-turn cost, wait/pass-time cost, cadence turn time, spike ratio, and max turn cost

## Review questions this packet should answer

- Do the real live bandit spawns now belong to Andi's system instead of merely coexisting beside it?
- Are bandit counts/headcounts now explicit enough that later control and writeback are honest rather than guessed?
- Can Schani and Josef deliberately restage a nearby bandit camp quickly enough to use this as an actual playtest surface?
- Does the handoff mode now leave the game alive for manual play rather than rudely cleaning up the moment it gets interesting?
- Did we get concrete performance numbers on the real nearby setup instead of hand-waved vibes?
- Did the helper/tooling follow-through stay in service of the product instead of taking over as the product?

## Position in the sequence

This is now the active lane.
The useful open helper work from `Bandit + Basecamp playtest kit packet v2` survives here as supporting scope, but it is no longer allowed to masquerade as the whole product by itself.

## Current grounded reality this packet must consume

### 1. Real hostile bandit content already exists, but mostly as static spawn footing

Current-tree hostile bandit content tied to this lane is real and broad enough to matter:

- overmap-special camps/cabins such as `bandit_camp`, `bandit_work_camp`, and `bandit_cabin` exist in `data/json/overmap/overmap_special/specials.json`
- those sites still rely on fixed `place_npcs` rosters in `data/json/mapgen/hells_raiders/{bandit_camp,bandit_work_camp,bandit_cabin}.json`
- map-extra style hostile spawns still exist through paths like `mx_looters` in `src/map_extras.cpp` and `mx_bandits_block` in `data/json/mapgen/map_extras/bandits_block.json`
- the spawned NPC classes are concrete templates such as `bandit`, `thug`, `bandit_trader`, `bandit_quartermaster`, `bandit_mechanic`, and `hells_raiders_boss` in `data/json/npcs/hells_raiders/npc.json`
- `hells_raiders` faction data in `data/json/npcs/factions.json` is broad faction metadata, not a live camp-population ledger

That means the repo already has real bandit bodies, real sites, and real faction flavor.
What it does **not** yet have is one live owner that can say reviewer-cleanly which site owns which members, who left, who died, who came back wounded, and which pressure/writeback facts changed later world behavior.

### 2. Current mapgen spawn behavior is the first world-side trap

`map::place_npc( ... )` in `src/mapgen.cpp` currently:
- loads the NPC template
- spawns it into the world
- toggles `trait_NPC_STATIC_NPC`
- inserts it into the overmap buffer

So the current site-spawned bandits are not magically ready-made travelling agents just because they exist in the world.
Any live-owner implementation must explicitly **claim and normalize** them for controlled dispatch, rather than pretending the static mapgen roster is already the same thing as the abstract bandit system.

### 3. The repo already has the dynamic primitives; the missing piece is the glue

The live-world-control lane does **not** need to invent bandit reasoning from nothing.
Relevant current substrate already exists:

- abstract evaluation and report surface: `src/bandit_dry_run.{h,cpp}`
- multi-turn scenario/proof/benchmark surface: `src/bandit_playback.{h,cpp}`
- local entry/return/writeback packet surface: `src/bandit_pursuit_handoff.{h,cpp}`
- world-side NPC/site helpers: `overmapbuffer.insert_npc`, `overmapbuffer.overmap_special_at`, `overmapbuffer.place_special`, `overmapbuffer.get_npcs_near_player`, `npc::travel_overmap`, and `overmap_npc_move()`

So the honest job here is not “invent all bandit AI”.
The honest job is to build one bounded saved owner/control seam that feeds the already-landed abstract substrate with real world members and then writes the local result back.

### 4. Playtest/helper footing is already strong enough to stop pretending feasibility is the open question

The surrounding packets already settled several setup questions:

- `doc/live-bandit-basecamp-playtesting-feasibility-probe-v0-2026-04-21.md` proved that live relaunch plus intentional hostile-bandit restage is already practical on the current tree
- `doc/live-bandit-basecamp-playtest-packaging-helper-packet-v0-2026-04-22.md` packaged the named-bandit debug seam into `tools/openclaw_harness/scenarios/bandit.basecamp_named_spawn_mcw.json`
- `doc/bandit-basecamp-playtest-kit-packet-v1-2026-04-22.md` and `v2` already earned useful helper substrate like fixture aliasing, manifest `save_transforms`, bounded `player_mutations`, and stronger observability
- `tools/openclaw_harness/scenarios/bandit.basecamp_playtestkit_restage_mcw.json` already shows the repeatable helper shape for menu capture plus immediate post-spawn proof

So this packet should **not** rerun feasibility theater.
It should spend that budget on turning the real live-bandit control path into the thing being restaged.

### 5. Current harness behavior exposes the second trap: probe is not yet honest manual handoff

`tools/openclaw_harness/startup_harness.py` already has the necessary ingredients for live setup, capture, repeatability, and fixture install.
But the current `probe` path still finalizes through `finalize_probe_report( ... , cleanup_pid=pid )`, which falls into `cleanup_game_process(...)` unless the report already has explicit cleanup state.

That is fine for reviewer capture mode.
It is **not** yet the same thing as “leave the session alive so a human can actually play the scene.”
So the two-mode contract in this packet is real implementation work, not wording polish.

## How this packet should stay aligned with the surrounding canon

The deeper path here is already constrained by earlier packets.
Do not silently unlearn them.

- **Basecamp AI capability audit/readout packet v0** means the Basecamp side is mostly deterministic craft/board routing today, not some missing general speech-intelligence layer. This lane should not widen into broad Basecamp-AI redesign just because Basecamp is nearby.
- **Live bandit + Basecamp playtesting feasibility probe v0** means the open question is no longer “can we restage anything at all?” The open question is whether the restaged thing is the real owned system.
- **Live bandit + Basecamp playtest packaging/helper packet v0** means debug-menu folklore is already packaged once. Keep the useful harness shape, but graduate the product proof beyond named-NPC spawn theater.
- **Bandit overmap/local handoff interaction packet v0** means live dispatch should enter local play as coarse posture and return as explicit writeback, not as exact-tile puppetry or stale certainty.
- **Bandit bounded scout/explore seam v0** means no-path must still fail closed; explicit explore remains deliberate bounded outing, not consolation-prize random wandering.
- **Bandit moving-bounty memory seam v0** means moving prey can stay warm briefly, but the live owner must not turn that into immortal chase bookkeeping.
- **Bandit repeated-site revisit behavior packet v0** means corroborated sites may earn one more bounded revisit/cautious-watch window, but not free raid truth or endless pressure.
- **Bandit overmap benchmark suite packet v0** means reviewer-facing output should expose concrete metrics and state cleanly enough to judge whether the system is alive/leiwand or merely legal.
- **Bandit + Basecamp first-pass encounter/readability packet v0** means the current named-spawn seam already produces real danger, but its readability is still too right-panel-log-first. That is useful playtest guidance, but it does not lower the live-ownership bar.

## Complete implementation path

### Phase 1 - Freeze one ownership map before code starts wandering

First freeze the exact world-side population this lane is claiming.
The minimal honest first ownership map is:

1. **site-backed overmap specials**
   - `bandit_camp`
   - `bandit_work_camp`
   - `bandit_cabin`
2. **map-extra/singleton hostile spawns tied to the same lane**
   - `mx_looters`
   - `mx_bandits_block`

The clean v0 shape is not one global faction simulation.
It is one bounded owner/site ledger where every controlled bandit group resolves to:
- a stable site or spawn anchor
- a site kind
- absolute location/origin
- explicit member identities
- explicit per-site and per-spawn-tile counts
- current outing/return state if somebody is away from home

For overmap specials, the anchor should come from real site footing such as the special location / origin rather than guessed folklore.
For map extras, the owner shape can be a deliberately smaller singleton or micro-site record so the same machinery works without pretending roadside ambushers are a full camp government.

### Phase 2 - Claim real NPC identities instead of inventing a second truth

Once the ownership map is frozen, the first world-side implementation burden is identity.
The owner must derive its roster from **real NPC ids** and real spawn tiles, not from vague faction membership or copied JSON counts.

Minimum honest v0 member record:
- `character_id`
- source template/class
- home site id
- home spawn tile or local home slot
- current state (`at_home`, `outbound`, `local_contact`, `dead`, `missing`, or equally bounded equivalent)
- last known writeback summary strong enough to explain headcount change later

Why this matters:
- the current faction JSON is too coarse to be the truth
- copied counts become stale the moment somebody dies or leaves
- local writeback cannot stay honest if the system does not know which bodies actually belonged to the outing

Important implementation caution:
mapgen-spawned camp members begin life as static NPCs.
So “claiming” a site must include whatever small normalization is needed so a controlled member can later travel or hand off, rather than leaving the owner attached to permanently static furniture.

### Phase 3 - Bridge the live owner into the existing abstract bandit substrate

After identity is real, the next job is not a broad new AI layer.
It is the bridge.

Recommended v0 flow:

1. Build live-world lead inputs from the owned site state.
   - existing marks/signals can still matter
   - site state, repeated-site corroboration, and moving-memory refresh can still matter
   - but every outward option must now be attributable to a real owned site/group
2. Feed those leads into `bandit_dry_run::evaluate(...)`.
3. Convert the winning outward intent through `bandit_pursuit_handoff` when the winner supports live local pursuit.
4. Dispatch actual owned members on the world side as a bounded party chosen from the live remaining site roster.
   - site-backed camps should keep some members home instead of stripping the whole camp
   - dispatch pressure should scale with the current remaining site size, so kills/losses shrink later outings
   - tiny singleton/micro-site cases can keep their smaller bounded rules without pretending they are full camp governments
5. When the group hits local contact, treat the local scene as able to rewrite the plan.
6. Apply the return packet back onto the owner/site ledger so later world behavior changes honestly.

The important thing is not that every part becomes “smart”.
The important thing is that the ownership seam now connects:
- **who went**
- **why they went**
- **what posture they entered with**
- **what happened locally**
- **what state changed afterward**

If the implementation can still only answer the middle two questions, the lane is not done.

### Phase 4 - Add one bounded cadence + persistence seam, not a world-sim bureaucracy

This lane does need save/load truth.
It does **not** need a grand simulation state dump.

The good v0 persistence shape is still the one earlier budget thinking was pushing toward:
- site identity and anchor
- member roster / headcount state
- active outing summary if a group is away
- bounded recent marks / moving-memory / repeated-site pressure state needed for the next decision
- last writeback facts that materially change later behavior

Bad shape:
- per-turn path scrapbooks
- huge terrain snapshots
- free-form biography logs
- “just serialize everything and pray”

Cadence should stay equally bounded.
The system needs one honest live tick/cadence that is cheap enough to measure and easy enough to reason about.
If near-player or recently-active sites need more frequent evaluation than distant idle ones, keep that distinction explicit and cheap rather than magical.

### Phase 5 - Build the real nearby restage seam around owned world content

The restage seam must graduate from “spawn a hostile named NPC” to “seed a controlled nearby owned bandit site/group.”
That is the product upgrade.

Cleanest first shape:
- intentionally restage one bounded real bandit camp about `10 OMT` away from the player or current Basecamp footing
- use real world placement/site ownership rather than only the named-NPC debug menu
- record the chosen site id, anchor, expected roster, and any forced conditions reviewer-cleanly
- fail loudly if placement/claiming cannot be done honestly on the current build

Current grounded trap on this packet:
- copying a `bandit_camp` footprint nearby is **not** enough by itself if the saved result still lands as `bandit_live_world.owner_id` with `sites: []`; that is only terrain placement, not owned-site truth
- the obvious moved-player bootstrap fallback is also not automatically acceptable here, because the disposable `player_near_overmap_special` helper was caught rewriting `player.location` without moving the top-level save load anchor; until the corrected helper is re-proved live, prior nearby-site bootstrap conclusions from that path are not honest ownership proof

Existing helpers should be reused, not discarded:
- prepared McWilliams/Basecamp footing
- fixture aliases and snapshot aliases
- manifest `save_transforms`
- OCR/screenshot capture conventions
- existing repeatability/report summary shape

But the upgraded proof target must be different.
A successful v0 restage should be able to say:
- which owned site was seeded
- which members belong to it
- which ones were dispatched or activated
- what later changed after local contact

### Phase 6 - Split reviewer probe mode from manual playtest handoff mode honestly

The packet already froze this, but the deeper path makes the implementation consequence clearer.

**Probe / capture mode** should:
- set up the nearby owned site/group
- capture reviewer-readable artifacts
- be allowed to clean up afterwards
- optimize for repeatability and comparison

**Manual playtest handoff mode** should:
- perform the same truthful setup
- surface what was forced and where the site landed
- then deliberately **leave the game/session running**
- avoid the current probe-style auto-cleanup path

Because the current harness `probe` contract auto-cleans, the manual handoff path likely needs either:
- a distinct harness verb, or
- an explicit no-cleanup / handoff mode on top of the existing startup/setup machinery

Do **not** fake this requirement with a probe run whose output merely says “you could have played here if we had not killed it.”
That would be peak nonsense.

### Phase 7 - Prove local writeback and aftermath, not just initial contact

The lane is not closed when the nearby camp spawns and gets angry.
That only proves the opening beat.

A real closeout needs at least one bounded aftermath loop:
- fight / flee / partial contact happens
- the site roster or headcount changes
- the owner remembers the changed state
- a later revisit/reload shows that the world did not snap back to untouched folklore

Good aftermath proof candidates:
- one member dies or is missing and the site no longer reports the old full roster or old dispatch capacity
- one outing comes back wounded/shaken and the return packet changes later posture
- one moving target is lost and the owner drops the pursuit state instead of stalking forever
- one repeatedly-probed site cools back out instead of hardening into immortal pressure

### Phase 8 - Land the perf packet on the honest nearby setup, not on an unrelated toy case

Josef already called out the reviewer-facing perf burden clearly.
The packet should therefore report concrete metrics on the **same nearby live-owned setup**:
- baseline single-turn cost
- wait/pass-time cost on repeated turns, because that is where slowdown becomes actively miserable to play
- bandit-cadence turn cost
- spike ratio
- max turn cost

That should include at minimum:
- one calm repeated-turn packet on the honest nearby owned setup
- one messier or heavier stress packet on honest bandit footing, so the report does not hide behind the clean idle case

The perf packet should not hide behind an abstract benchmark divorced from the actual restaged product surface.
The abstract benchmark suite remains useful precedent for packet shape and readable metrics, but this lane needs the live nearby owned setup specifically.

## Testing ladder

### A. Deterministic game-side coverage that belongs directly to this lane

Add narrow deterministic coverage for the world-side truths that the older abstract packets could not prove yet:

1. **ownership/bootstrap tests**
   - relevant site/spawn families map into the intended owner/site shape
   - multi-tile camp anchors do not lose member attribution
   - map-extra singleton cases do not accidentally get treated like full camps
2. **identity/headcount tests**
   - claiming a site records real member ids and per-spawn-tile counts
   - losses/returns update the ledger instead of leaving stale anonymous counts behind
   - later dispatch size follows the live remaining site population instead of folklore-reset counts
3. **persistence round-trip tests**
   - the owner/site roster survives save/load
   - active outing / return state survives save/load
   - bounded marks/memory/writeback state survives without ballooning into junk
4. **dispatch/handoff tests**
   - outward winners that support pursuit produce the expected entry payload
   - site-backed camps do not strip the whole camp for ordinary outings when enough members remain to leave a home presence
   - local return packets rewrite the owner/group state as intended
   - no-path still fails closed; explicit explore remains explicit
5. **guardrail tests**
   - moving-prey memory still collapses cleanly
   - repeated-site pressure still cools back out
   - local loss can still rewrite stale intent to withdrawal or safer behavior

Existing bandit tests remain part of the base footing here, especially `tests/bandit_dry_run_test.cpp`, `tests/bandit_mark_generation_test.cpp`, `tests/bandit_playback_test.cpp`, and `tests/bandit_pursuit_handoff_test.cpp`.
The new work should extend that footing rather than bypassing it with “trust me, the live run looked okay.”

### B. Narrow harness/helper validation

For harness-side work, keep validation proportionate:
- `python3 -m py_compile tools/openclaw_harness/startup_harness.py`
- dry-run or narrow validation on any new scenario/handoff helper logic
- repeatability checks where the scenario contract claims repeatable menus, captures, or post-setup state

If the harness grows a new handoff/no-cleanup mode, validate that explicitly instead of assuming the current probe path “basically covers it.”

### C. Live proof ladder

The live proof should climb in deliberate steps:

1. **owned-site setup proof**
   - restage the nearby controlled site/group
   - prove the owner/site id, roster, and location are what the packet claims
2. **manual handoff proof**
   - same setup, but the session stays alive after setup
3. **short-horizon interaction proof**
   - prove the nearby setup really reaches overmap/bubble interaction and visible local contact
4. **aftermath/writeback proof**
   - prove later world behavior changed because of what happened locally
5. **perf proof**
   - record the concrete nearby metrics on the same setup, including wait/pass-time behavior instead of only one isolated turn cost

The exact command names can evolve, but the evidence classes should not.

### D. Required ugly-interaction / adversarial proof matrix

Happy-path setup proof is necessary and still not sufficient.
This lane is exactly where weird interactions between local NPC AI, player action, overmap intent, save/load, and ownership bookkeeping are most likely to lie.
So closeout should include **at least four explicit ugly-interaction proofs**, covering at minimum the first four families below.

1. **claim/bootstrap drift proof**
   - prove a freshly claimed site does not duplicate, lose, or misattribute members when the real static mapgen roster is converted into owned live state
   - include at least one multi-tile site case and one smaller/singleton hostile spawn case
   - failure shape we are trying to kill: copied JSON counts or partial claim state disagreeing with the actual NPC roster on the map
2. **player-disruption proof**
   - prove the owner stays honest when the player interferes before or during the outing
   - strong first cases: kill one member early, enter the site before cadence dispatch finishes, lure zombies into the contact, or leave after a partial skirmish
   - failure shape we are trying to kill: the owner still believing the original full-strength roster or blindly continuing a stale plan after the player already changed reality
3. **local-AI-versus-OMT-intent proof**
   - prove local NPC behavior can diverge from the abstract plan without corrupting writeback
   - strong first cases: local flee, target loss, split pursuit, or opportunistic fight with unrelated pressure
   - failure shape we are trying to kill: the overmap owner insisting the original posture succeeded even though local play rewrote it
4. **save/load and unload/reload proof**
   - prove ownership, headcount, outing state, and writeback survive save/load and map unload/reload without duplication or resurrection
   - strong first cases: save mid-outing, reload after one death/missing member, and revisit after leaving the area
   - failure shape we are trying to kill: duplicated members, revived dead members, dropped ownership, or an outing that forgets it ever happened
5. **aftermath/revisit cooling proof**
   - prove the same site does not become immortal pressure theater after contact
   - strong first cases: partial wipe, wounded return, moving-target loss, and repeated-site revisit cooling after the local scene changed
   - failure shape we are trying to kill: endless stale pursuit, endless site pressure, or instant reset back to untouched folklore
6. **messy-case perf proof**
   - measure not only the calm nearby setup but at least one messier interaction state where the player already disturbed the scene or local contact already happened
   - if possible, push one honest nearby setup toward the upper pressure this packet can really generate and show whether repeated time-passing stays acceptable there too
   - failure shape we are trying to kill: a perf report that only looks fine in the clean idle setup while the real integrated scene spikes badly once play starts

The exact scripts/runs can evolve.
What must stay fixed is the bar: Andi does **not** get to close this lane on one neat restage, one happy-path contact, and one perf number from an undisturbed toy case.
The feature is only real if the system stays coherent after humans and local AI do rude things to it.

## Playtest ladder

The live playtest sequence should stay honest and staged, not all-or-nothing.

### 1. Owner-debug pass

Use the strongest observability footing first.
That may include the prepared-base / clairvoyance-backed helper footing already earned in `v1` and `v2`, because the first question is: did the right owned site and members actually get set up and handed off?

This pass is allowed to be a bit debuggy.
Its purpose is ownership truth, not cinematic drama.

### 2. Real nearby-contact pass

Once owner truth is stable, run the nearby restage on ordinary current-build footing and check:
- did the camp/group appear where promised
- did contact happen through the real path
- did the encounter still read mostly through log spam, or is spatial staging improving

This is where the earlier first-pass readability findings matter.
Do not lose them.
The current system already proved it can be dangerous; now the question is whether the upgraded owned restage becomes readable enough to play with.

### 3. Manual human play pass

Now let the human actually play the scene.
This is the reason the handoff mode exists.
The operator should be able to:
- launch the nearby setup
- be told what was forced
- be told roughly where the bandit site/group is
- then keep playing without the harness politely strangling the session

### 4. Aftermath/revisit pass

After the live play scene, revisit or reload and verify the world changed honestly.
This is the pass that proves the product is not just spawn theater.

### 5. Perf/readability judgment pass

Finally, collect the metrics and the taste notes together:
- are the concrete perf numbers acceptable
- does the encounter feel alive on the map
- does it read as pressure/stalking/contact rather than debug-puppet nonsense
- is the setup fast enough that Schani and Josef would actually use it repeatedly

## Anti-scope guardrails

Do not let this packet quietly mutate into any of the following:

- a generic hostile-human architecture rewrite
- a broad Basecamp command-language redesign
- a universal scenario-authoring/modding platform
- a fake success case where only the old named-NPC spawn seam works
- a persistence blob that stores every possible detail because nobody wanted to choose
- an immortal pursuit/revisit machine that forgets the earlier bounded-memory and bounded-revisit rules

The active lane is narrower and more useful than that:
make the real current bandit spawns belong to one live saved system, make that system restageable nearby, make local outcomes write back honestly, and make the whole thing measurable enough to trust.
