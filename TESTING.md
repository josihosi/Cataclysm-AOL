# TESTING

Current validation policy and current evidence only.

This file is not a trophy wall.
Remove stale or completed fluff instead of stacking crossed-off test history forever.

## Validation policy

Use the **smallest evidence that honestly matches the change**.

- **Docs-only change** -> no compile
- **Small local code change** -> narrow build and the narrowest relevant test target
- **Broad or risky code change** -> broader rebuild and relevant filtered tests
- **Before a Josef handoff** or after suspicious stale-binary behavior -> rebuild the real targets and rerun the real smoke path

### Agent vs Josef

Schani should do agent-side playtesting first whenever the harness or direct in-game probing can answer the question.
Josef should be asked only for:
- product judgment
- tone/feel
- human-priority choices
- genuinely human-only interaction

Josef being unavailable is **not** a blocker by itself.
Josef self-testing is **not** a plan blocker and does not belong in the active success state as a gate.
If Josef-specific checks are useful, write them down as non-blocking notes so he can run them later from his own list.
If another good agent-side lane exists, keep moving there.
If several human-only judgments are likely soon, batch them instead of sending tiny separate asks.

### Anti-churn rule

Do not keep rerunning the same startup or test packet when it is no longer the missing evidence class.
If startup/load is already green, and the missing proof is live behavior, then the next probe must target live behavior.
If a target is merely waiting on Josef, do not keep revalidating it unless the code changed.

---

## Current relevant evidence

### Controlled locker / basecamp follow-through packet

Current honest state:
- The active lane is now a controlled follow-through packet, not a broad locker/basecamp reopen blob.
- Ordinary chat / ambient harness footing now points at the captured `McWilliams` / `Zoraida Vick` save instead of the older Sandy Creek default.
- The current McWilliams debug pass has already been reduced into five explicit packages in:
  - `doc/locker-basecamp-followthrough-work-packages-2026-04-07.md`
- Those packages are:
  1. harness zone-manager save-path polish
  2. basecamp toolcall routing fix
  3. locker outfit engine hardening
  4. locker zone policy + control-surface cleanup
  5. basecamp carried-item support + dump lane
- **Package 1** is now landed on real screenshots/artifacts instead of guesswork:
  - the zone name must be entered at creation time, not later through a separate edit-name handshake
  - the closeout path back to gameplay is a single `Esc` to open the save prompt, then uppercase `Y`
  - reopening Zone Manager after returning to gameplay still shows the custom `Probe Locker` entry on the current McWilliams path
  - latest screenshot packet: `.userdata/dev-harness/harness_runs/20260408_005852/`
- **Package 2** is now landed with both deterministic and real-path live proof:
  - camp-request routing no longer keys off bare `assigned_camp`
  - the current gate accepts idle assigned-camp hearers in `NPC_MISSION_NULL`, explicit `FACTION_CAMP` role-id workers, and stationed camp guards/patrol guards
  - walking-with-player companion states and `GUARD_ALLY` still stay on the ordinary follower side
  - deterministic coverage for that discriminator lives in `tests/faction_camp_test.cpp`
  - current deterministic recheck passed on this Mac after the fresh `make -j4 tests` rebuild in `build_logs/package2_idle_assignee_routing_20260408.log`, via `./tests/cata_test "[camp][basecamp_ai]"`
- The current McWilliams live probe path needed one honest harness fix before it could be trusted again:
  - startup on the McWilliams save was still declaring success while the window sat on the lingering load screen
  - the harness now sends the same post-load `Enter` continuation key that cleared the real window into gameplay on this path
  - fresh live-debug startup proof now reaches gameplay at `.userdata/live-debug/harness_runs/20260408_020648/success.png`
- With that live path repaired, the old ordinary-hearer packet is now understood more honestly:
  - manual live captures after startup are recorded in `.userdata/live-debug/harness_runs/20260408_020648/`
  - nearby hearers `Katharina Leach` / `Robbie Knox` answered in ordinary spoken bark form for freeform `show me the board` and `craft 1 bandage`
  - that result is now treated as follower-side behavior on the current save, not as proof that stationed basecamp routing is still wrong
- Stale-binary suspicion on the same live path was real and is now cleared:
  - an initial Package 2 talk-menu probe came back `inconclusive_version_mismatch`, so the current tiles binary was rebuilt from repo HEAD
  - fresh rebuild log: `build_logs/package2_live_rebuild_20260408_retry1.log`
- The old literal `show_board` packet at `.userdata/dev-harness/harness_runs/20260408_033437/` is now demoted harder than before:
  - the current McWilliams save still has player followers `[2, 3]`
  - fresh hearer-routing instrumentation plus rebuilt live rechecks at `.userdata/dev-harness/harness_runs/20260408_053336/` showed nearby hearers `Katharina Leach` / `Robbie Knox` hitting the ordinary lane with `assigned_camp=none`, so that packet never exercised a real assigned-camp hearer state
  - the current McWilliams save fixture search found no serialized non-null `assigned_camp` entries at all, so the real missing evidence class was a restaging source, not another rerun on the unmodified snapshot
- The nearby activity-menu probe stays landed as a **negative** helper, not as the restaging source:
  - scenario: `tools/openclaw_harness/scenarios/basecamp.package2_activity_menu_probe_mcw.json`
  - latest run: `.userdata/dev-harness/harness_runs/20260408_065958/`
  - this probe can flip the McWilliams hearers out of the simple walking-with-player state, but it still does **not** create the intended Package 2 state
  - routing artifacts after `Taking it easy` still showed `assigned_camp=none`, so the board/craft prompts on that state stayed on the ordinary lane
- The honest assigned-camp restaging source is now proven on the same real McWilliams footing:
  - scenario: `tools/openclaw_harness/scenarios/basecamp.package2_assign_camp_toolcall_probe_mcw.json`
  - current live restaging path: ally dialogue `C -> t -> 1 -> b -> d -> n -> a -> q -> c`, then let the stationed camp state settle before probing
  - that restaging path is now representable directly in the harness as `assign_nearby_npc_to_camp_dialog` instead of only a pile of raw keypress steps
  - the debug-menu folklore seam `} -> p -> p -> b -> A` was rechecked and is false on current McWilliams footing: after selecting an NPC, `b` opens bionics and `A` there is CBM install, not camp assignment
  - the useful debug-editor inspect path after the real restage is `} -> p -> p -> 2 -> Enter` on current McWilliams, which exposes Katharina's post-restage header state on-screen
  - first interim proof at `.userdata/dev-harness/harness_runs/20260408_081903/` showed why one-turn reruns were still lying: `assigned_camp=140,41,0` was written immediately, but Katharina still sat in interim `mission=6` / `GUARD_ALLY`, so routing honestly remained ordinary there
  - after about 100 turns of settling, `.userdata/dev-harness/harness_runs/20260408_082344/` showed the intended stationed state on the real save: Katharina logged `uses_basecamp=yes`, `camp_found=yes`, `assigned_camp=140,41,0`, `mission=8`, and `reason=camp_grouped`
  - latest exact-token live proof at `.userdata/dev-harness/harness_runs/20260408_083415/` now reaches the routed camp lane cleanly:
    - `show_board` logs `camp heard Katharina Leach`, `heard=show_board`, `board=show_board`, and emits the reviewer-clean board follow-through with `job=1 ... next=job=1`
  - visible state-inspection support scenario: `tools/openclaw_harness/scenarios/basecamp.package2_assign_camp_state_probe_mcw.json`
    - the follow-up `job=1` token also rides the same camp-aware lane on that real assigned-camp state
    - nearby ordinary hearer Robbie can still chirp on-screen at the same time, so mixed visible bark is now understood as a nearby-hearer overlap detail, not as proof that the assigned-camp route failed
  - the raw freeform `craft 1 bandage` phrasing is now demoted as the wrong live follow-up shape for this packet. On the true assigned-camp path the honest routed follow-up is the structured `job=1` token coming back from `show_board`, not another raw craft phrase.
- The first narrow Package 3 hardening slice is now landed in deterministic code/tests:
  - same-type bag upgrades now prefer the better-condition equivalent instead of leaving a damaged current bag equipped just because the raw score delta is below the normal bag-upgrade threshold
  - new deterministic coverage lives in `tests/faction_camp_test.cpp` for both the planner and direct locker service path
  - fresh recheck on this Mac passed after `make -j4 tests`, via `./tests/cata_test "[camp][locker]"` with logs in `build_logs/package3_bag_condition_build_20260408.log` and `build_logs/package3_bag_condition_tests_20260408.log`
- A second narrow Package 3 hardening slice is now landed in deterministic code/tests:
  - footed/full-body jumpsuits no longer get classified as shoes just because they cover feet; the locker planner now keeps them in the pants lane on the current logic instead of excluding them through the footwear bucket
  - new deterministic coverage in `tests/faction_camp_test.cpp` checks both direct classification and planner retention for the jumpsuit case
  - fresh recheck on this Mac passed after `make -j4 tests`, via `./tests/cata_test "[camp][locker]"` with logs in `build_logs/package3_onesie_build_20260408.log` and `build_logs/package3_onesie_tests_20260408.log`
- A third narrow Package 3 acceptance-bar slice is now closed in deterministic proof:
  - baseball cap -> army helmet replacement already works on the current locker path, and `tests/faction_camp_test.cpp` now proves both the planner swap decision and the direct locker-service swap/return path
  - fresh recheck on this Mac passed after `make -j4 tests`, via `./tests/cata_test "[camp][locker]"` with logs in `build_logs/package3_cap_helmet_build_20260408.log` and `build_logs/package3_cap_helmet_tests_20260408.log`
- A fourth narrow Package 3 lower-body slice is now closed in deterministic proof:
  - the hot-weather `antarvasa` + cargo pants -> cargo shorts cleanup path now has explicit planner + direct locker-service proof, so that specific lower-body conflict no longer lives only as debug-pass memory
  - fresh recheck on this Mac passed after `make -j4 tests`, via `./tests/cata_test "[camp][locker]"` with logs in `build_logs/package3_lower_body_conflict_build_20260408.log` and `build_logs/package3_lower_body_conflict_suite_20260408.log`
- A fifth narrow Package 3 lower-body slice is now closed in deterministic proof:
  - the hot-weather duplicate `cargo shorts + jeans` conflict now has explicit planner + direct locker-service proof, so the old visible "wearing shorts and jeans at once" eyesore is no longer relying on debug-pass folklore either
  - the planner proof keeps the already-worn cargo shorts as the best current pants item in heat and marks the jeans as duplicate current wear rather than pretending a fresh locker candidate is required first
  - the direct locker-service proof shows the duplicate jeans being removed and returned to locker stock while the already-worn shorts stay equipped
  - fresh recheck on this Mac passed after `make -j4 tests`, via `./tests/cata_test "[camp][locker]"` with logs in `build_logs/package3_shorts_jeans_build_20260408.log` and `build_logs/package3_shorts_jeans_tests_20260408.log`
- A sixth narrow Package 3 lower-body slice is now closed in deterministic proof:
  - full-leg skintight underlayers like `leggings` no longer hide in the underwear lane; they now classify into the pants lane so hot-weather locker cleanup can treat them as duplicate legwear instead of layering them under the final shorts swap
  - the planner proof keeps cargo pants as the best current full-length legwear, marks `leggings` as duplicate current wear, and still lands the cargo-shorts upgrade in heat
  - the direct locker-service proof shows the duplicate leggings being removed and returned to locker stock while the final cargo-shorts swap still lands in the same service pass
  - fresh recheck on this Mac passed after `make -j4 tests`, via `./tests/cata_test "[camp][locker]"` with logs in `build_logs/package3_leggings_build_20260408.log` and `build_logs/package3_leggings_tests_20260408.log`
- A seventh narrow Package 3 lower-body classification slice is now closed in deterministic proof:
  - jumpsuit-like outer one-piece suits no longer fall into vest logic just because they are marked `OUTER`; the locker classifier now keeps those full-body suit items in the pants lane with the existing one-piece/jumpsuit behavior instead of pretending the lower-body slot is empty
  - the new deterministic coverage in `tests/faction_camp_test.cpp` proves both the direct `suit` classification result and the no-vest/no-empty-pants planning result on the current logic
  - fresh recheck on this Mac passed after `make -j4 tests`, via `./tests/cata_test "[camp][locker]"` with logs in `build_logs/package3_suit_build_20260408.log` and `build_logs/package3_suit_tests_20260408.log`
- An eighth narrow Package 3 one-piece acceptance-bar slice is now closed in deterministic proof:
  - lower-body-only upgrades no longer strip torso coverage from a current one-piece suit unless the same locker pass also supplies a torso replacement, so the locker path stops "upgrading" suits into half-dressed nonsense
  - the new deterministic coverage in `tests/faction_camp_test.cpp` proves both sides of that guard on the current logic: a lower-body-only shorts candidate leaves the suit in place, while a shorts + t-shirt locker packet can still split the suit cleanly in one service pass
  - fresh recheck on this Mac passed after `make -j4 tests`, via `./tests/cata_test "[camp][locker]"` with logs in `build_logs/package3_onepiece_guard_build_20260408.log` and `build_logs/package3_onepiece_guard_tests_20260408.log`
- A ninth narrow Package 3 skintight one-piece slice is now closed in deterministic proof:
  - skintight full-body one-piece suits like `union_suit` and `wetsuit` no longer hide in underwear; the locker planner now keeps them in the pants lane instead of pretending the lower-body slot is empty while shorts pile on top
  - the new deterministic coverage in `tests/faction_camp_test.cpp` proves both the direct classification/planning result and the direct locker-service guard on the current logic: a shorts-only locker candidate leaves the current wetsuit in place instead of layering shorts over it
  - fresh recheck on this Mac passed after `make -j4 tests`, via `./tests/cata_test "[camp][locker]"` with logs in `build_logs/package3_skintight_onepiece_build_20260408.log`, `build_logs/package3_skintight_onepiece_tests_20260408.log`, and `build_logs/package3_skintight_onepiece_diffcheck_20260408.log`
- Patrol sanity on the current McWilliams save is already checked: the serialized patrol tiles currently resolve to **2 clusters** under 4-way connectivity, so that note no longer belongs in the active mystery pile.
- The right current discipline is:
  - one package at a time
  - revalidate before widening
  - do not let Andi or the repo drift into a broad opportunistic locker rewrite

### Existing baseline that should not be mistaken for the active package

- Patrol Zone v1 remains checkpointed, the current McWilliams patrol-cluster check is a sanity confirmation, not a reopened patrol lane.
- The harness swap to `McWilliams` / `Zoraida Vick` for ordinary chat/ambient probing is real, but it does **not** by itself prove the basecamp toolcall-routing bug is fixed.
- The debug-intake pass and the Andi work-package draft are useful planning evidence, but neither counts as code/test closeout on their own.

### Meaning

- The active question is not "how do we fix all locker/basecamp weirdness at once".
- The active question is "what is the next isolated package that honestly improves the system without wrecking the working loop".
- If a proposed step starts blending harness polish, wrong-snapshot routing, locker outfit hardening, control-surface design, and inventory policy into one blob, split it before coding.

---

## Pending probes

### Active queue

1. **Package 3** on the current McWilliams / fresh-save locker path:
   - use the now-closed Package 2 routing probe as a baseline and do not quietly reopen routing while continuing locker hardening
   - keep the landed better-condition bag slice, jumpsuit-not-shoes slice, cap -> helmet proof, the hot-weather lower-body cleanup proof, the duplicate-shorts-vs-jeans cleanup proof, the leggings-underlayer cleanup proof, the outer-suit classification proof, the one-piece torso-strip guard proof, and the skintight one-piece no-shorts-overlayer proof closed while choosing the next isolated ugly locker conflict
   - the next missing evidence class is current-path locker behavior for the next visible lower-body oddity beyond those now-proven hot-weather cleanup, duplicate-shorts-vs-jeans, leggings-underlayer, outer-suit-classification, one-piece torso-strip-guard, and skintight one-piece no-shorts-overlayer paths, not more ceremonial basecamp reruns
2. keep the helper narrow:
   - do not widen Package 3 into locker policy/control-surface or carried-item support yet
   - do not treat raw freeform craft phrasing as a routing regression unless the exact `show_board` -> `job=1` assigned-camp probe breaks too

Still true:
- ordinary chat / ambient harness footing should stay on the captured `McWilliams` / `Zoraida Vick` save, not drift back to the older default fixture
- Package 3 stays next on purpose, do not bury the wrong-snapshot question under locker feature creep
- `sustain_npc` remains only a helper idea for later probes if some future live packet honestly needs it

### Anti-hallucination rule for this packet

Do not treat any of these as success by themselves:
- swapping the ordinary harness fixture to `McWilliams`
- writing a clean work-package document
- observing plausible-looking NPC gear motion on-screen once
- describing a nice locker inventory/outfit policy in prose
- partially fixing one package while quietly widening two others behind it

If the packet sounds cleaner than the active package boundary or evidence underneath it, stop and trim it.

**Hackathon-reserved — do not touch before the event:**
1. **chat interface over dialogue branches**
   - current `chat.nearby_npc_basic` evidence is harness-only scaffolding, not feature implementation
2. **ambient-trigger reaction lane / tiny ambient-trigger NPC model**
   - current `ambient.weird_item_reaction` evidence is harness-only scaffolding/observability, not feature implementation

**Later discussion topics, not current code lanes:**
1. **smart zone manager**
2. any broader locker/inventory redesign beyond the active package order in the auxiliary doc

### Active-lane handoff block

- **active lane:** controlled locker / basecamp follow-through packet
- **active slice:** Package 3, locker outfit engine hardening
- **next slice:** Package 4, locker zone policy + control-surface cleanup
- **last closed lane:** Package 2, basecamp toolcall routing fix, is now landed on the McWilliams assigned-camp probe path; Patrol Zone v1 remains checkpointed
- **current blocker shape:** none on Package 2 anymore. The honest next work is now Package 3 locker hardening, kept narrow against the newly closed routing baseline
- **Josef ask:** none right now beyond keeping the packet narrow and one-package-at-a-time

### Non-blocking Josef notes

- The current debug pass already did the hard work of separating real bugs from design requests and future ideas.
- Keep using that packetized structure. If future notes start piling up again, collect first, then reduce to packages again before handing anything to Andi.

---

## Reusable commands

Use these when they are actually the missing evidence, not as ritual.

### Fresh full test rebuild on this Mac
- `make -j4 tests`

### Fresh tiles relink on this Mac
- `make -j4 TILES=1 cataclysm-tiles`

### Patrol-focused deterministic check
- once patrol tests exist, run the narrowest patrol-specific `cata_test` filter or named case instead of the whole suite

### Startup/load smoke for later live patrol proof
- `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'`

## Local build caveat

On this Mac, treat top-level `make -j4 tests` as the reliable path for a fresh `cata_test`.
Avoid treating `make -C tests cata_test` as authoritative here; it has been a repeated source of toolchain/stale-build nonsense.
Also: if you actually need a fresh tiles binary, use `make -j4 TILES=1 cataclysm-tiles`; plain `make cataclysm-tiles` is not an honest rebuild path here.
