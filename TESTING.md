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

### Basecamp carried-item dump lane (active)

Current honest state:
- still believed true from earlier work: locker outfitting core exists, locker maintenance rhythm exists, and earlier deterministic/runtime proof for those non-carried-item slices still stands unless new evidence disproves it
- locker ranged-readiness support already has honest ammo / magazine grounding on the curated locker side
- Package 5 code is now landed in the tree:
  - ordinary carried misc junk is removed during locker dressing instead of being preserved follower-style
  - the deliberately kept carried lane is explicitly limited to `bandages`, `ammo`, and `magazines`
  - dumped carried junk is sent to a non-locker cleanup/drop tile instead of being folded back into curated locker stock
- fresh deterministic proof on current HEAD passed:
  - `./tests/cata_test "camp_locker_service_dumps_carried_junk_outside_curated_locker_stock"`
  - log: `build_logs/package5_carried_dump_test_20260418.log`
  - result: `All tests passed (22 assertions in 1 test case)`
- fresh broader locker regression packet also passed:
  - `./tests/cata_test "[camp][locker]"`
  - log: `build_logs/package5_locker_suite_20260418.log`
  - result: `All tests passed (955 assertions in 51 test cases)`
- fresh tiles rebuild for live proof succeeded after clearing the stale tiles PCH:
  - `make -j4 TILES=1 cataclysm-tiles`
  - log: `build_logs/package5_tiles_build_20260418.log`
- fresh live locker proof now exists on rebuilt current HEAD `6952308b74` at `.userdata/dev-harness/harness_runs/20260418_231138/` via `python3 tools/openclaw_harness/startup_harness.py probe locker.weather_wait`
  - screen: runtime-compatible harness launch reached gameplay and completed the locker wait path; this packet did not surface direct on-screen locker text, so the proof class here is mainly artifact/log rather than visible UI text
  - tests: `not_run` by the harness packet itself; the deterministic evidence above is the paired test packet
  - artifacts/logs: `probe.artifacts.log` captured the real locker service dumping ordinary carried junk for Ricky Broughton (`small plastic bag`, `dried lentils`, `bullpup shotgun`, `stone axe`) while the post-service locker state only showed the pants-lane items, not the dumped junk
- not yet safe to claim the whole Package 5 acceptance bar is closed:
  - the current live fixture/probe still honestly proves the junk-dump path and non-polluting locker target on the real locker route
  - the missing live proof is now the kept `bandages` / `ammo` / `magazines` lane on that same real path
- the active correction packet on 2026-04-19 proved the actor question, then forced a probe-shape audit, and that audit materially changed the blocker:
  - corrected scenario still is `tools/openclaw_harness/scenarios/locker.package5_robbie_e2e_verified_mcw.json`
  - first honest finding: the modified harness helper itself was wrong. `tools/openclaw_harness/startup_harness.py` had been leaving the multidrop menu open, and the filtered drop path was selecting items wrong, so the old post-drop screenshots were not honest ground checks
  - repaired narrow bandage proof now closes honestly:
    - inventory probe: `.userdata/dev-harness/harness_runs/20260419_035025/`
      - screen: the wish helper stocks inventory for query `bandage`, but as `adhesive bandage`
    - ground probe: `.userdata/dev-harness/harness_runs/20260419_040122/`
      - screen: the corrected filtered drop path now logs `You drop your adhesive bandage on the floor.` and the pickup menu shows `adhesive bandage` on the intended live tile
  - repaired narrow magazine proof now closes honestly too:
    - inventory probe: `.userdata/dev-harness/harness_runs/20260419_035431/`
      - screen: the filtered drop menu shows `Glock 9x19mm 15-round magazine (15/15 9x19mm JHP)` really exists in inventory after the wish spawn
    - ground probe: `.userdata/dev-harness/harness_runs/20260419_040243/`
      - screen: the corrected filtered drop path now logs `You drop your Glock 9x19mm 15-round magazine on the floor.` and the pickup menu shows that same Glock magazine on the intended live tile
  - the older loose-`9x19mm JHP` query mismatch is still useful context, but it is no longer the current blocker:
    - wish-query probe: `.userdata/dev-harness/harness_runs/20260419_031430/`
      - screen: the top wish-menu result for query `9x19mm JHP` is `1000 9x19mm JHP ammo can`, while loose `9x19mm JHP` is the second filtered result
    - explicit-second-entry ground probe: `.userdata/dev-harness/harness_runs/20260419_040812/`
      - screen: even after forcing the second filtered wish-menu entry, the pickup seam on the live tile still resolves as `small ammo can`, not loose `9x19mm JHP`
    - current correction packet avoids that seam by seeding `9x19mm JHP, reloaded`
  - fresh same-actor rerun: `.userdata/dev-harness/harness_runs/20260419_051921/`
    - screen, post-drop verification seam: the pickup menu on the intended live tile now honestly shows `9x19mm JHP, reloaded (1)`, `adhesive bandage`, `Glock 9x19mm 15-round magazine (15/15 9x19mm JHP)`, and `small plastic bag`
    - screen, spoken-order seam: `ask_robbie_to_pick_up_seeded_items.after.png` still sits in `Enter a sentence to say`, with the overlong exact-name pickup line never visibly submitted
    - screen, later seams: `verify_robbie_picked_up_seeded_packet.after.png`, `assign_robbie_to_camp_dialog.after.png`, and `wait_for_first_locker_service_window.after.png` stay in the same prompt-stall state, so they are not honest pickup/assignment/service proof
    - artifacts/logs: the rerun produced `verdict: inconclusive_no_new_artifacts`, with no matching `camp locker:` lines
  - fresh corrected rerun with the shorter pickup phrasing: `.userdata/dev-harness/harness_runs/20260419_052751/`
    - screen, spoken-order seam: `ask_robbie_to_pick_up_seeded_items.after.png` now shows `You say "Robbie, pick up the bandage, the reloaded 9mm ammo, the Glock magazine, and the small plastic bag on the ground."`
    - screen, later dialog seam: the broader follow-up packet no longer stalls in the say prompt, but its generic continuation still lands in the wrong end-state instead of returning cleanly to gameplay
    - artifacts/logs: `probe.artifacts.log` records `avatar::talk_to` topic churn (`TALK_MISSION_INQUIRE`, `TALK_MISSION_SUCCESS_LIE`, `TALK_FRIEND`) and the rerun ends `verdict: inconclusive_no_artifact_match`
  - fresh dialog drill-down on that same corrected packet: `.userdata/dev-harness/harness_runs/20260419_055401/`
    - screen: the post-pickup reply chain `b -> a -> b -> d -> n -> a` reaches Robbie's camp-specific `What about the camp?` submenu and then `I want to assign you to work at this camp.`
    - screen: this proves the camp branch is real on the corrected packet, but only on this narrow drill-down path so far
    - tests: `not_run` by this live drill-down, because the missing evidence class here was UI path shape
    - artifacts/logs: the useful fact from this probe is the on-screen branch shape; no locker artifact interpretation is honest yet because the flow still did not return to gameplay and wait for service
  - fresh packaged e2e rerun after inlining the assignment presses: `.userdata/dev-harness/harness_runs/20260419_061324/`
    - screen, pickup seam: the corrected packet still gets the short spoken order on-screen and Robbie still picks up the seeded items
    - screen, assignment seam: the full packet does **not** yet reproduce the narrowed camp-submenu path reliably; on this rerun the follow-up dialog drifts back into Robbie's general follower menu instead
    - screen, concrete wrong-path evidence:
      - `d` selects `I'd like to know a bit more about your abilities.`
      - the later `a` opens Robbie's character sheet, not camp work assignment
      - the later `c` opens `Select a destination`
    - tests: `not_run` by this live rerun, because the missing evidence class was still UI path shape
    - artifacts/logs: still no matching `camp locker:` lines, so there is still no honest kept-lane locker readout from the corrected packet
  - fresh pickup-targeting re-audit at `.userdata/dev-harness/harness_runs/20260419_063750/` did reveal a real upstream problem:
    - screen: after the short `say` order and a 50-turn wait, the message log shows both nearby followers responding, not a Robbie-only packet
    - screen: `You say "Robbie, pick up the bandage, the reloaded 9mm ammo, the Glock magazine, and the small plastic bag on the ground."`
    - screen: `Katharina Leach says: "Got it, Zoraida Robbie, bandage and ammo go to you; I'll grab the Glock mag and keep watch."`
    - screen: `Robbie Knox says: "On it, Zoraida-I'll secure the supplies and keep watch while you grab the bandage and ammo."`
  - fresh whisper-targeted pickup probe at `.userdata/dev-harness/harness_runs/20260419_064258/` confirmed the fallback was no better:
    - screen: `You whisper "Robbie, pick up the bandage, the reloaded 9mm ammo, the Glock magazine, and the small plastic bag on the ground."` is visibly submitted on-screen
    - screen: after 50 turns, there is still no follower response or pickup-completion line in the visible log, so this targeted whisper path is currently a no-op for the packet
    - tests: `not_run` by this live probe, because the missing evidence class was targeted live pickup behavior
    - artifacts/logs: no new locker artifact interpretation is honest here because the packet never reaches confirmed pickup
  - fresh post-fix rerun at `.userdata/dev-harness/harness_runs/20260419_073418/` on rebuilt head `6fcc64b56a-dirty`
    - code: `src/npctalk.cpp` now filters allied hearers by direct addressed name before ambient routing steals the utterance, so named `say` orders stop fanning out to nearby followers
    - screen: the same short `say` order is visibly submitted on-screen and Robbie is now the only follower who answers
    - screen: visible pickup lines now show Robbie taking `adhesive bandage`, `9x19mm JHP, reloaded`, and `Glock 9x19mm 15-round magazine`
    - artifacts/logs: this rerun exposed the real remaining upstream lie: the model answer named all four requested items, but `look_around` was still hard-capped to three selections in both the prompt text and `parse_look_around_response()`
    - tests: narrow compile/build only for this code-path repair
      - `make -j8 TILES=1 SOUND=1 RELEASE=1 LOCALIZE=1 LANGUAGES=all LINTJSON=0 ASTYLE=0 TESTS=0 obj/tiles/npctalk.o`
      - `make -j4 TILES=1 SOUND=1 RELEASE=1 LOCALIZE=1 LANGUAGES=all LINTJSON=0 ASTYLE=0 TESTS=0 cataclysm-tiles`
  - bounded checkpoint for the targeting repair is now committed as `377c1f0695` (`Fix direct-addressed NPC pickup targeting`)
  - exactly one materially changed bag-only probe then answered Frau Knackal's narrow question on that checkpointed footing: `.userdata/dev-harness/harness_runs/20260419_082431/`
    - screen: `You say "Robbie, pick up the small plastic bag on the ground."`, Robbie answers, `Robbie Knox picks up a small plastic bag.`, and the follow-up `g` check says `There is nothing to pick up nearby.`
    - artifacts/logs: `config/llm_intent.log` records `look_around selected Robbie Knox (req_1): item_6:a: small plastic bag` followed by `look_around pickup Robbie Knox (small plastic bag): picked up 1 item(s)`
    - meaning: the bag is not being picked up silently on this footing. The remaining issue on the original four-item packet is upstream selection / queueing on the combined request, not unreadable visible pickup feedback
  - fresh four-item-cap repair validation at `.userdata/dev-harness/harness_runs/20260419_090351/`
    - code/prompt: `src/llm_intent.cpp`, `data/llm_prompts/look_around_prompt.txt`, and the live-debug fixture prompt now lift the `look_around` selection cap from three to four
    - tests: the paired narrow proof remains `make -j4 tests` plus `./tests/cata_test "[llm_intent]"`, which passed after the limit/test adjustment (`All tests passed (69 assertions in 10 test cases)`); the same repaired tiles build was then used for this live rerun
    - screen: `You say "Robbie, pick up the bandage, the reloaded 9mm ammo, the Glock magazine, and the small plastic bag on the ground."` is visibly submitted, Robbie is the only follower who answers, and the visible log shows Robbie picking up `adhesive bandage`, `9x19mm JHP, reloaded`, `Glock 9x19mm 15-round magazine`, and `small plastic bag`
    - artifacts/logs: `config/llm_intent.log` now records `look_around response Robbie Knox (req_1): item_8, item_6, item_7, item_9`, the matching four-item selection list, and pickup lines for `9x19mm JHP, reloaded`, `Glock 9x19mm 15-round magazine`, and `small plastic bag`
    - remaining mismatch: the bandage clearly enters the selected queue and is visibly picked up on-screen, but current `action_status` logging still falls through to `pickup.item_missing` for that first item instead of mirroring the visible pickup cleanly in `config/llm_intent.log`
    - meaning: the old three-item queue cap is no longer the active blocker. The four-item pickup-completion seam is now honest enough to reopen downstream camp-assignment work, with a smaller bandage observability quirk still noted
- the current slice should stay on the `McWilliams` / `Zoraida Vick` footing and the real locker/live path, not wander back into Package 4 surface cleanup or unrelated future lanes

### Meaning

- the active delivery target is still Package 5 from `doc/locker-basecamp-followthrough-work-packages-2026-04-07.md`
- the exact kept-item seed packet is now honestly visible on the live tile in the corrected packet
- the shortened pickup order is still honestly visible on-screen on the live footing, and the repaired `say` path is now the only honest delivery seam for this packet
- the mixed-actor pickup-targeting concern is closed on the named `say` seam:
  - the latest live rerun shows only Robbie answering the addressed order and only Robbie pickup lines in the visible log
  - the older whisper no-op is now just a failed side path, not the current delivery seam
- the upstream four-item pickup-completion blocker is now closed enough to move on:
  - the bag-only probe proved the small plastic bag itself was never the silent failure
  - the repaired four-item-cap rerun now shows the whole requested seed being picked up on-screen on the corrected named-`say` path
  - the remaining mismatch on this seam is observability for the bandage log line, not selection / queueing
- the active blocker therefore moves back downstream to post-pickup camp assignment and return to locker service
- do not interpret any later locker-service artifact as kept-lane proof until Robbie is shown both completing that corrected four-item pickup packet and re-entering camp service on the same path
- keep Smart Zone Manager, board routing, Package 4 surface cleanup, hackathon features, and future threat design closed or out of scope while this slice is active

### Smart Zone Manager v1

Current honest state:
- the one-off Smart Zone Manager v1 code remains real in the tree, not aux-doc theater
- the Basecamp inventory-zone prompt path still exists in both honest UI seams:
  - initial `CAMP_STORAGE` placement in Zone Manager
  - later position/stretch edits of that same Basecamp zone
- the deterministic proof still covers the active honest packet in `tests/clzones_test.cpp`, including:
  - expected stamped layout on a representative indoor basecamp fixture
  - outdoor rotten placement beyond a simple wall ring
  - too-small-zone failure
  - non-destructive refusal when a required bed tile is already occupied by a non-basecamp zone
  - repeatability on the same layout
- fresh current-head rebuild on 2026-04-09 succeeded:
  - `make -j4 tests` -> `build_logs/freeze_prep_tests_20260409.log`
  - `make -j4 TILES=1 cataclysm-tiles` -> `build_logs/freeze_prep_tiles_20260409.log`
- fresh deterministic recheck on rebuilt current HEAD passed:
  - `./tests/cata_test "[zones][smart_zone][basecamp]"`
  - log: `build_logs/freeze_prep_smart_zone_tests_20260409.log`
  - result: `All tests passed (2825 assertions in 4 test cases)`
- the too-small-zone failure path is therefore **implemented and presently re-verified**; it is not a hand-wavy TODO
- fresh proportional live proof now exists on rebuilt current HEAD `fce15a6c5a` at `.userdata/dev-harness/harness_runs/20260409_140439/`:
  - prepared-save seam: reinstall the McWilliams fixture into `dev-harness`, move existing `*zones*.json` out of the save, then run `smart_zone.live_probe_mcw_prepped`
  - prompt screenshot: `.userdata/dev-harness/harness_runs/20260409_140439/confirm_zone_end.after.png`
  - saved/reopened layout screenshot: `.userdata/dev-harness/harness_runs/20260409_140439/reopen_zones_manager_for_layout_capture.after.png`
  - screen evidence: the Smart Zone Manager prompt fired on the real UI path, and the saved/reopened Zone Manager list still showed `Basecamp: Storage` plus the expected stamped smart-zone entries
  - artifacts/logs: the harness verdict is `inconclusive_no_new_artifacts`, which is expected here because this proof class is screen/UI evidence rather than `llm_intent.log` output

### Meaning

- Smart Zone Manager v1 still survives a fresh rebuild + live rerun on current HEAD
- the lane should stay closed while Package 5 is active unless code/runtime evidence breaks it

### Assigned-camp board/log leak recheck

Current honest state:
- the preserved-baseline assigned-camp board/log fix still holds on rebuilt current HEAD
- fresh deterministic recheck passed:
  - `./tests/cata_test "[camp][basecamp_ai]"`
  - log: `build_logs/freeze_prep_basecamp_ai_tests_20260409.log`
  - result: `All tests passed (332 assertions in 1 test case)`
- fresh live assigned-camp probe now exists on rebuilt current HEAD `fce15a6c5a` at `.userdata/dev-harness/harness_runs/20260409_140655/` via `basecamp.package2_assign_camp_toolcall_probe_mcw`
- screen evidence:
  - screenshot: `.userdata/dev-harness/harness_runs/20260409_140655/wait_for_job_followup_reply.after.png`
  - visible in-game replies stay organic on the player-facing message log
  - the old raw leak shape does **not** appear on-screen: no visible `board=show_board`, `details=show_job=1`, or `next=job=1`
  - the player's own typed structured probe text `job=1` still appears as the player utterance, which is expected for this harnessed internal-path check
- artifacts/logs:
  - artifact log: `.userdata/dev-harness/harness_runs/20260409_140655/probe.artifacts.log`
  - the internal structured path still records `board=show_board`, `details=show_job=1`, `approval=waiting_player`, and `next=job=1`, which is the intended observability split
- known weird-but-non-blocking remainder:
  - the same artifact packet still records a nearby Robbie Knox response after the Katharina exchange
  - that is recipient/chatter noise on the live probe, not the old raw player-log leak returning

### Meaning

- the assigned-camp board/log leak fix still survives a fresh rebuilt-current-head live check
- current truth is the intended split: organic player-facing message log, structured internal artifact log
- keep this out of the active queue while Package 5 is running unless later code or runtime evidence breaks one of those claims

---

## Pending probes

- rerun the corrected Robbie end-to-end packet now that the upstream four-item pickup seam is honest again, and confirm the post-pickup camp-assignment path on the repaired footing
- if that path still drifts, instrument or replace the assignment continuation rather than reopening pickup-targeting or pickup-queue speculation
- only after Robbie reaches pickup, camp assignment, and locker service on the same corrected seed packet should the locker artifact/log be read as kept-vs-dump evidence for Package 5
- keep the bandage pickup observability mismatch noted, but do not treat it as the active blocker unless it prevents interpreting the downstream packet

---

## Reusable commands

Use these when they are actually the missing evidence, not as ritual.

### Fresh full test rebuild on this Mac
- `make -j4 tests`

### Fresh tiles relink on this Mac
- `make -j4 TILES=1 cataclysm-tiles`

### Smart-zone deterministic check
- `./tests/cata_test "[zones][smart_zone][basecamp]"`

### Basecamp routing deterministic check
- `./tests/cata_test "[camp][basecamp_ai]"`

### Startup/load smoke for later live proof
- `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'`

### Current smart-zone live probe
- `python3 tools/openclaw_harness/startup_harness.py install-fixture mcwilliams_live_debug_2026-04-07 --profile dev-harness --fixture-profile live-debug --replace`
- move the installed McWilliams `*zones*.json` files out of `.userdata/dev-harness/save/McWilliams/`
- `python3 tools/openclaw_harness/startup_harness.py probe smart_zone.live_probe_mcw_prepped`

### Current assigned-camp board/log recheck
- `python3 tools/openclaw_harness/startup_harness.py probe basecamp.package2_assign_camp_toolcall_probe_mcw`

### Existing locker live baseline
- `python3 tools/openclaw_harness/startup_harness.py probe locker.weather_wait`

### Current corrected same-actor probe
- `python3 tools/openclaw_harness/startup_harness.py probe locker.package5_robbie_e2e_verified_mcw`

### Current narrow kept-item seam probes
- `python3 tools/openclaw_harness/startup_harness.py probe locker.package5_seed_ammo_ground_probe_mcw`
- `python3 tools/openclaw_harness/startup_harness.py probe locker.package5_seed_bandage_ground_probe_mcw`
- `python3 tools/openclaw_harness/startup_harness.py probe locker.package5_seed_mag_ground_probe_mcw`
- `python3 tools/openclaw_harness/startup_harness.py probe locker.package5_spawn_bandage_inventory_probe_mcw`
- `python3 tools/openclaw_harness/startup_harness.py probe locker.package5_spawn_mag_inventory_probe_mcw`
- `python3 tools/openclaw_harness/startup_harness.py probe locker.package5_spawn_query_probe_mcw`

## Local build caveat

On this Mac, treat top-level `make -j4 tests` as the reliable path for a fresh `cata_test`.
Avoid treating `make -C tests cata_test` as authoritative here; it has been a repeated source of toolchain/stale-build nonsense.
Also: if you actually need a fresh tiles binary, use `make -j4 TILES=1 cataclysm-tiles`; plain `make cataclysm-tiles` is not an honest rebuild path here.
