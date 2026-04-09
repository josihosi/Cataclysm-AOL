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
- A tenth narrow Package 3 indirect suit-alias slice is now closed in deterministic proof:
  - full-body `looks_like: suit` variants like `tux` no longer fall back into vest logic just because they only reach `jumpsuit` through the suit alias; the locker planner now keeps them in the pants lane so lower-body-only upgrades do not quietly strip torso coverage through that alias path
  - the new deterministic coverage in `tests/faction_camp_test.cpp` proves both the direct classification/planning result and the direct locker-service guard on the current logic: a shorts-only locker candidate leaves the current tuxedo in place instead of layering shorts under it or stripping it into half-dressed nonsense
  - fresh recheck on this Mac passed after `make -j4 tests`, via `./tests/cata_test "[camp][locker]"` with logs in `build_logs/package3_indirect_jumpsuit_build_20260408_retry1.log` and `build_logs/package3_indirect_jumpsuit_tests_20260408_retry1.log`
- An eleventh narrow Package 3 short-dress slice is now closed in deterministic proof:
  - ordinary one-piece civilian dresses like `short_dress` now have explicit planning + direct locker-service proof for the same torso-coverage guard as the suit-like cases, so a shorts-only locker candidate leaves the dress in place while a shorts + t-shirt locker packet can still split it cleanly in one service pass
  - the new deterministic coverage in `tests/faction_camp_test.cpp` proves both sides on the current logic: direct classification/planning keeps `short_dress` in the pants lane, shorts-only upgrades do not strip it into half-dressed nonsense, and a paired shirt replacement still lets the split happen deliberately
  - fresh recheck on this Mac passed after `make -j4 tests`, via `./tests/cata_test "[camp][locker]"` with logs in `build_logs/package3_short_dress_build_20260408.log` and `build_logs/package3_short_dress_tests_20260408.log`
- A twelfth narrow Package 3 draped-overgarment slice is now closed in deterministic proof:
  - draped-only lower-body overgarments like `hakama` no longer get misbucketed as real pants-slot conflicts, so hot-weather locker cleanup can keep the draped overgarment on while swapping actual cargo pants for cargo shorts
  - the new deterministic coverage in `tests/faction_camp_test.cpp` proves the current logic at three levels: direct classification leaves `hakama` out of the locker slot model, planning still upgrades the real pants item without inventing a duplicate-current conflict, and direct locker service keeps the `hakama` equipped while returning only the replaced cargo pants to locker stock
  - fresh recheck on this Mac passed after `make -j4 tests`, via `./tests/cata_test "[camp][locker]"` with logs in `build_logs/package3_draped_legwear_build_20260408.log`, `build_logs/package3_draped_legwear_tests_20260408.log`, and `build_logs/package3_draped_legwear_diffcheck_20260408.log`
- A thirteenth narrow Package 3 full-length-dress slice is now closed in deterministic proof:
  - full-length dresses like `long_dress` now have explicit planning + direct locker-service proof for the same torso-coverage guard as the short-dress cases, so a shorts-only locker candidate leaves the dress in place while a shorts + t-shirt locker packet can still split it cleanly in one service pass
  - the new deterministic coverage in `tests/faction_camp_test.cpp` proves both sides on the current logic: direct classification/planning keeps `long_dress` in the pants lane, shorts-only upgrades do not strip it into half-dressed nonsense, and a paired shirt replacement still lets the split happen deliberately
  - fresh recheck on this Mac passed after `make -j4 tests`, via `./tests/cata_test "[camp][locker]"` with logs in `build_logs/package3_long_dress_build_20260408.log` and `build_logs/package3_long_dress_tests_20260408.log`
- A fourteenth narrow Package 3 sleeved-dress upper-body-coverage slice is now closed in deterministic proof:
  - sleeved dresses like `long_dress_sleeved` no longer split into bare-arm vest-plus-shorts nonsense just because a torso-only upper-body replacement exists; the pants-lane guard now keeps the current dress unless the same locker pass still preserves the stripped arm coverage too
  - the new deterministic coverage in `tests/faction_camp_test.cpp` proves both layers on the current logic: classification/planning still keeps `long_dress_sleeved` in the pants lane, blocks the shorts split when the only upper-body replacement is torso-only `vest`, and direct locker service keeps the current sleeved dress equipped while the blocked shorts remain in locker stock
  - fresh recheck on this Mac passed after `make -j4 tests`, via `./tests/cata_test "[camp][locker]"` with logs in `build_logs/package3_sleeved_dress_arm_guard_build_20260408.log`, `build_logs/package3_sleeved_dress_arm_guard_tests_20260408.log`, and `build_logs/package3_sleeved_dress_arm_guard_diffcheck_20260408.log`
- A fifteenth narrow Package 3 sleeved-dress positive-split slice is now closed in deterministic proof:
  - the new sleeved-dress arm-coverage guard is not overblocking the hot-weather path; when the locker also supplies an arm-covering `tshirt`, the same `long_dress_sleeved` packet still splits cleanly into `shorts_cargo` plus shirt instead of freezing the whole upgrade
  - the new deterministic coverage in `tests/faction_camp_test.cpp` proves both layers on the current logic: planning still selects the shorts + shirt packet when that shirt preserves arm coverage, and direct locker service equips the shorts and `tshirt` while returning the replaced sleeved dress to locker stock
  - fresh recheck on this Mac passed after `make -j4 tests`, via `./tests/cata_test "[camp][locker]"` with logs in `build_logs/package3_sleeved_dress_split_build_20260408.log`, `build_logs/package3_sleeved_dress_split_tests_20260408.log`, and `build_logs/package3_sleeved_dress_split_diffcheck_20260408.log`
- A sixteenth narrow Package 3 full-body protective-suit slice is now closed in deterministic proof:
  - head-covering full-body suits like `hazmat_suit` no longer fall into helmet logic just because the classifier sees head coverage first; the locker planner now keeps them in the pants lane so shorts-only hot-weather candidates stop peeling them into ordinary-clothes nonsense
  - the new deterministic coverage in `tests/faction_camp_test.cpp` proves both layers on the current logic: direct classification/planning keeps `hazmat_suit` in the pants lane with no phantom helmet slot, and direct locker service leaves the hazmat suit equipped while the blocked cargo shorts remain in locker stock
  - fresh recheck on this Mac passed after `make -j4 tests`, via `./tests/cata_test "[camp][locker]"` with logs in `build_logs/package3_hazmat_build_20260408.log`, `build_logs/package3_hazmat_tests_20260408.log`, and `build_logs/package3_hazmat_diffcheck_20260408.log`
- A seventeenth narrow Package 3 split-coverage slice is now closed in deterministic proof:
  - footed full-body jumpsuits no longer split into shorts + shirt just because the torso half of the replacement exists; the pants-lane split guard now requires the same locker pass to preserve the built-in footwear too
  - the new deterministic coverage in `tests/faction_camp_test.cpp` proves both sides on the current logic: planning + direct locker service keep `test_jumpsuit_cotton` intact when the locker only offers cargo shorts + `tshirt`, while the same packet still splits cleanly once replacement `boots` are also present
  - the same planning coverage now also anchors the head/face side of the generic extra-coverage guard: `hazmat_suit` no longer plans a shorts + shirt split when that would drop the suit's non-leg coverage
  - fresh recheck on this Mac passed after `make -j4 tests`, via `./tests/cata_test "[camp][locker]"` with logs in `build_logs/package3_split_coverage_build_20260408.log` and `build_logs/package3_split_coverage_tests_20260408.log`
- An eighteenth narrow Package 3 helmet-tradeoff slice is now closed in deterministic proof:
  - full helmets that also cover eyes, like `helmet_plate`, no longer get misbucketed as glasses just because the classifier sees eye coverage first; they now stay in the helmet lane so the locker planner can compare the real helmet candidates instead of silently treating a great helm as eyewear
  - the new deterministic coverage in `tests/faction_camp_test.cpp` proves both planning and direct locker-service parity on the current logic: a worn `helmet_army` stays put when locker stock only offers a melee-skewed `helmet_plate`, while the same lane still upgrades a worn great helm into the ballistic army helmet when that better helmet is what the locker actually holds
  - fresh recheck on this Mac passed after `make -j4 tests`, via `./tests/cata_test "[camp][locker]" "*helmet*"` with logs in `build_logs/package3_ballistic_helmet_parity_build_20260408_retry1.log` and `build_logs/package3_ballistic_helmet_parity_tests_20260408_retry1.log`
- A nineteenth narrow Package 3 body-armor tradeoff slice is now closed in deterministic proof:
  - ballistic torso armor with ablative plate pockets, like `ballistic_vest_esapi`, no longer gets flattened into the ordinary vest lane just because those plate pockets look like storage; locker classification and scoring now ignore ablative pockets when deciding whether an armor item is really utility wear or real body armor
  - the new deterministic coverage in `tests/faction_camp_test.cpp` proves both planning and direct locker-service parity on the current logic: a worn `ballistic_vest_esapi` stays put when locker stock only offers melee-skewed `armor_lc_plate`, while the same lane still upgrades a worn plate harness into the ballistic vest when that better armor is what the locker actually holds
  - fresh recheck on this Mac passed after `make -j4 tests`, via `./tests/cata_test "[camp][locker]"` with logs in `build_logs/package3_ballistic_body_armor_build_20260409_retry1.log`, `build_logs/package3_ballistic_body_armor_tests_20260409_retry1.log`, and `build_logs/package3_ballistic_body_armor_diffcheck_20260409_retry1.log`
- A twentieth narrow Package 3 lower-body support-gear slice is now closed in deterministic proof:
  - belted leg accessories like `holster` no longer get flattened into the pants lane just because they cover the upper leg, so hot-weather locker cleanup can treat them as support gear instead of fake duplicate pants
  - the new deterministic coverage in `tests/faction_camp_test.cpp` proves both planning and direct locker-service parity on the current logic: a worn `holster` stays equipped while the planner still upgrades `pants_cargo` into `shorts_cargo`, and direct locker service returns only the replaced cargo pants to locker stock
  - fresh recheck on this Mac passed after `make -j4 tests`, via `./tests/cata_test "[camp][locker]" "*holster*"` plus a full `./tests/cata_test "[camp][locker]"` suite recheck, with logs in `build_logs/package3_leg_holster_build_20260409.log`, `build_logs/package3_leg_holster_tests_20260409.log`, `build_logs/package3_leg_holster_suite_20260409.log`, and `build_logs/package3_leg_holster_diffcheck_20260409.log`
- A twenty-first narrow Package 3 lower-leg support-armor slice is now closed in deterministic proof:
  - lower-leg armor accessories like `knee_pads` no longer get flattened into the pants lane just because they cover knees or shins, so hot-weather locker cleanup can treat them as support armor instead of fake duplicate pants
  - the new deterministic coverage in `tests/faction_camp_test.cpp` proves both planning and direct locker-service parity on the current logic: a worn `knee_pads` stays equipped while the planner still upgrades `pants_cargo` into `shorts_cargo`, and direct locker service returns only the replaced cargo pants to locker stock
  - fresh recheck on this Mac passed after `make -j4 tests`, via `./tests/cata_test "[camp][locker]"` with logs in `build_logs/package3_knee_pads_build_20260409.log` and `build_logs/package3_knee_pads_tests_20260409.log`
- A twenty-second narrow Package 3 partial-leg support-storage slice is now closed in deterministic proof:
  - partial leg-mounted support gear like the deep concealment `bholster` and `leg_small_bag` no longer gets misbucketed as underwear or pants just because it covers only hips or upper thighs, so hot-weather locker cleanup can keep that support gear equipped while still swapping real pants for `shorts_cargo`
  - the new deterministic coverage in `tests/faction_camp_test.cpp` proves both planning and direct locker-service parity on the current logic: a worn `bholster` / `leg_small_bag` stays out of the underwear and pants slots, the planner still upgrades `pants_cargo` into `shorts_cargo`, and direct locker service returns only the replaced cargo pants to locker stock
  - fresh recheck on this Mac passed after `make -j4 tests`, via `./tests/cata_test "[camp][locker]"` with logs in `build_logs/package3_partial_leg_support_storage_build_20260409.log` and `build_logs/package3_partial_leg_support_storage_tests_20260409.log`
- A twenty-third narrow Package 3 full-leg rigid-support-armor slice is now closed in deterministic proof:
  - hard no-storage full-leg guards like `legguard_hard` no longer get flattened into the pants lane just because they cover both upper and lower legs, so hot-weather locker cleanup can keep those guards equipped while still swapping real pants for `shorts_cargo`
  - the new deterministic coverage in `tests/faction_camp_test.cpp` proves both planning and direct locker-service parity on the current logic: a worn `legguard_hard` stays out of the pants slot, the planner still upgrades `pants_cargo` into `shorts_cargo`, and direct locker service returns only the replaced cargo pants to locker stock
  - fresh recheck on this Mac passed after `make -j4 tests`, then `./tests/cata_test "[camp][locker]"`, with logs in `build_logs/package3_full_leg_greaves_build_20260409.log`, `build_logs/package3_full_leg_greaves_suite_20260409.log`, and `build_logs/package3_full_leg_greaves_diffcheck_20260409.log`
- A twenty-fourth narrow Package 3 full-body body-armor occupancy slice is now closed in deterministic proof:
  - full-body body armor like `armor_lc_plate` no longer leaves the pants slot looking empty just because it is still classified in the body-armor lane for armor tradeoff scoring, so hot-weather locker cleanup stops adding filler `shorts_cargo` underneath it
  - the new deterministic coverage in `tests/faction_camp_test.cpp` proves both planning and direct locker-service parity on the current logic: a worn `armor_lc_plate` suppresses missing-current pants fill in planning, and direct locker service leaves the plate armor alone while the cargo shorts stay in locker stock
  - fresh recheck on this Mac passed after `make -j4 tests`, then `./tests/cata_test "[camp][locker]"`, with logs in `build_logs/package3_full_body_plate_guard_build_20260409.log`, `build_logs/package3_full_body_plate_guard_tests_20260409.log`, and `build_logs/package3_full_body_plate_guard_diffcheck_20260409.log`
- The preserved Package 2 board/log leak guard is now landed in deterministic code/tests:
  - structured `show_board` / `show_job` / board-follow-through replies no longer dump raw `board=` / `status=` / `approval=` / `next=` payload text into the visible in-game message log; that payload stays in the internal camp-reply log packet while the on-screen path reuses the organic board/status bark
  - `tests/faction_camp_test.cpp` now proves that split for structured `show_board`, structured `job=1`, and archived-request board cleanup without reopening ordinary follower behavior
  - fresh recheck on this Mac passed after `make -j4 tests`, via `./tests/cata_test "[camp][basecamp_ai]"` with logs in `build_logs/basecamp_board_log_leak_build_20260408.log` and `build_logs/basecamp_board_log_leak_tests_20260408.log`
- The matching live observability helper is now landed and re-proved on the current binary:
  - the assigned-camp probe scenario now captures cropped OCR-backed screen-text artifacts beside `llm_intent.log`, so the player-facing bark can be compared directly with the internal structured camp-reply packet
  - the first helper rerun honestly exposed stale-binary mismatch, so the real tiles target was rebuilt via `make -j4 TILES=1 cataclysm-tiles` with current log `build_logs/basecamp_board_observability_tiles_rebuild_20260408_retry1.log`
  - latest current-binary proof packet: `.userdata/dev-harness/harness_runs/20260408_233639/`
    - internal path: `probe.artifacts.log` still carries the structured assigned-camp handoff (`camp heard Katharina Leach`, `utterance=show_board`, `board=show_board`, `details=show_job=1`, `next=job=1`)
    - visible path: `wait_for_board_reply.after.screen_text.txt` and `wait_for_job_followup_reply.after.screen_text.txt` now capture OCR text from the on-screen message area, showing organic bark like `check the board.` and `Got it-I'll help you` instead of raw `board=` / `next=` payload text
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
   - keep the landed better-condition bag slice, jumpsuit-not-shoes slice, cap -> helmet proof, the hot-weather lower-body cleanup proof, the duplicate-shorts-vs-jeans cleanup proof, the leggings-underlayer cleanup proof, the outer-suit classification proof, the indirect suit-alias one-piece proof, the one-piece torso-strip guard proof, the skintight one-piece no-shorts-overlayer proof, the short-dress torso-coverage proof, the draped-overgarment overlay proof, the new full-length-dress torso-coverage proof, the new sleeved-dress arm-coverage / positive-split proof, the new head-covering full-body protective-suit proof, the new footed-jumpsuit split-coverage proof, the new full-helmet-vs-glasses / ballistic-helmet tradeoff proof, the new ballistic-body-armor-vs-plate proof, the new full-body plate-armor no-filler-pants proof, the new leg-holster support-gear proof, the new knee-pad lower-leg-support-armor proof, the new partial-leg support-storage proof, and the new full-leg hard-guard support-armor proof closed while choosing the next isolated ugly locker conflict
   - the next missing evidence class is current-path locker behavior for the next visible body-armor or lower-body oddity beyond those now-proven hot-weather cleanup, duplicate-shorts-vs-jeans, leggings-underlayer, outer-suit-classification, indirect suit-alias one-piece guard, one-piece torso-strip-guard, skintight one-piece no-shorts-overlayer, short-dress torso-coverage, draped-overgarment overlay, full-length-dress torso-coverage, sleeved-dress arm-coverage / positive-split, head-covering full-body protective-suit, footed-jumpsuit split-coverage, full-helmet-vs-glasses / ballistic-helmet tradeoff, ballistic-body-armor-vs-plate, full-body plate-armor no-filler-pants, leg-holster support-gear, knee-pad lower-leg-support-armor, partial-leg support-storage, and full-leg hard-guard support-armor paths, not more ceremonial basecamp reruns
2. **Deterministic locker service-parity battery:**
   - expand current Package 3 proof with `doc/locker-service-parity-test-battery-2026-04-08.md` so locker logic is checked across classification, planning, and actual service behavior
   - include at least: full-body protective suits, footed one-pieces, draped one-pieces, positive split counterparts, and the remaining bullet-vs-melee armor tradeoff cases beyond the now-covered helmet and body-armor seams
   - for chosen families, deterministic tests should prove that planning and service stay aligned instead of allowing technically legal but practically stupid locker behavior
3. keep the helper narrow:
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
