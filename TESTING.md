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

Current debug-stack attempt rule for the same item/blocker:
- current hostile-ecology live rows stop after two meaningfully different attempts; isolate the
  gameplay/fixture defect or record the harness limitation, then continue another Phase-4 row
- attempts 1-2 may be Andi solo retries, including multiple focused harness attempts in one cron run when each attempt changes setup, instrumentation, or evidence class
- before attempt 3, consult Frau Knackal
- attempts 3-4 are the final changed retries after consultation
- after attempt 4, if code is implemented but proof still fails, add a concise implemented-but-unproven packet to Josef's playtest package and move to the next greenlit debug note; do not close it and do not park it as dead

### Test-to-game wiring rule

A test is not allowed to impersonate implementation. Before claiming gameplay behavior, identify the live code path that consumes the tested seam and name the evidence class that proves it: unit/evaluator, playback/proof packet, live source hook, harness/startup, screen, save inspection, or artifact/log. Deterministic-only packets may close only as deterministic-only packets; if the contract says the game does something, the proof must reach the game path or the claim stays open.

### Promotion / closure hygiene

Before promoting, closing, or handing off a lane, confirm that `TESTING.md` pending probes still match the active `Plan.md` lane. If the pending-probe text points at an older slice, fix it before Andi uses it as execution truth.

### Bandit overmap-proof rule

For the remaining bandit AI proof packets, single-turn deterministic checks are **not** enough by themselves.
The honest bar now includes real overmap-side multi-turn scenario proof, up to `500` turns where needed, with explicit per-scenario goals and tuning metrics.

## Current validation target - CAOL-HOSTILE-CAMP-OVERMAP-ECOLOGY-v0

Source reference:
- untouched release/playtest target: `port/cdda-master` at `660057ff728bdf77531f607b1bd42a175f027a5f`
- isolated editing branch/worktree: `dev` at `Cataclysm-AOL-hostile-ecology-dev`
- included upstream base: `8d4959bee4`
- reviewed-but-unmerged upstream tip: `7cf1d08ae8`; no upstream merge is authorized in this lane

Phase-0 evidence (complete 2026-08-02):
- The production checkout and `origin/port/cdda-master` independently resolve to exact `660057ff728bdf77531f607b1bd42a175f027a5f`; no relevant transfer, build, or orchestrator process was active at kickoff.
- `dev` was a strict ancestor of the candidate. Backup refs preserve the prior `dev` and `master` tips; `dev` was advanced non-destructively to the candidate before the isolated worktree was created.
- The launch envelope reports GPT-5.6-sol, xhigh reasoning, `approval_policy = "never"`, and `sandbox_mode = "danger-full-access"`; the persistent service tier is now normal/default.
- `/usr/bin/python3 -m unittest tools.openclaw_harness.test_fixture_contract` first reproduced 57 tests with one failure and one error: mocked Windows resolution constructed `WindowsPath` on macOS, and mocked Linux accepted the existing Mac API venv. The reviewed repair now passes 60 tests, including native Keychain add/update seams and tilde-expanded foreign-root rejection.
- A clean-environment release-scenario dry run succeeds and selects the Mac API venv, but the Keychain does not yet supply `CATA_API_KEY`. The existing shell value is retained only until a real secure-store API call succeeds.
- The repaired Keychain writer uses Security.framework without a secret-bearing subprocess. Its one guarded real write returned secret-free `OSStatus -25308` (`interaction not allowed`). This is now a deferred release-harness gap, not a deterministic Phase-0 blocker; retain the existing shell export and make no more Keychain retries while Josef is unavailable.
- Exact identity is archived at `/Users/josefhorvath/codexbulk/C-AOL-hostile-ecology-artifacts/phase0-20260802/environment-fixture-manifest-54d2c76c0b.json`: Mac mini `Mac16,10`, Apple M4/10 cores/16 GiB, arm64 macOS 26.3.1 (`25D771280a`), Apple clang 17.0.0, GNU Make 3.81, SDK 26.2, McWilliams seed `830204929`, and 91-day seasons.
- The abandoned build log ends after compilation with no error, linker, completion marker, or binary and remains classified incomplete. The first replacement attempt failed immediately because installed Homebrew gettext was not on `PATH`. With `/opt/homebrew/opt/gettext/bin` explicit, top-level `make -j4 tests LINTJSON=0 ASTYLE=0` exited `0` in 57.78 seconds. The resulting 79,036,488-byte arm64 `tests/cata_test` has SHA-256 `4491718735452fa868644d9609f4fcfeffb13fb300b118378f2742c587699525`.
- The archived pre-change functional packet is
  `/Users/josefhorvath/codexbulk/C-AOL-hostile-ecology-artifacts/phase0-20260802/functional-baseline-manifest-ffbf32166c.json`.
  `bandit_live_world*` passed 43 cases/1,002 assertions, `[bandit][handoff]` passed 3/41,
  `[bandit][playback]` passed 37/1,028, and the repaired harness contract passed 60 tests. The
  manifest records exact commands, exit codes, timings, logs, and caveats.
- The source baseline proves exact-avatar radar (`direct_player_range` and `player@x,y,z`) and
  per-member independent overmap route solves. Visible dancing and covert-hostility failure were
  not reproduced and were not fabricated; covert disposition remains unqualified until its later
  operation-scoped integration row. Current structural scanning also has a source-proven
  prefix-starvation risk and skips cannibal profiles.
- Behavior-neutral checkpoint `dc094e8bf1` extracts the unchanged existing-only bootstrap scan into
  a shared callable seam. The fixed internal seed `830204914` test uses ordinary
  `overmap_buffer.get` default generation, not a custom batch or forced placement: both real
  2x2+roof specials register once with 8-tile footprints and abstract headcounts 6/5, real mapgen
  reconciles both to 14 members, and JSON round-trip remains duplicate-free. The test passed under
  a deliberately different CLI seed; the full `[bandit][live_world]` tag passed 68 cases/1,459
  assertions. Final autoreview reported no accepted/actionable findings.
- Behavior-neutral benchmark checkpoint `22ca8759f239c3196a158c026cb64f6aeca2ae80` is on `dev`;
  the preserved `660057ff` comparison worktree carries cherry-pick
  `b7e9a6a1f6138e3b2546157b9aa97887172e8bbd`. Both diffs have stable patch ID
  `de11c834a4e0075a8695a8b7b4d5bdca698cfa48`. The driver has bounded timing histograms,
  deterministic seeded AB/BA ordering, immutable binary/matrix/data identities, isolated child
  users/environments, and fail-closed packet validation. The Python suite passes 77 tests; the
  visible histogram case passes 1 case/13 assertions; `[bandit][live_world]` passes 68 cases/1,459
  assertions; final xhigh autoreview reports no actionable finding.
- Both committed comparison binaries were built one at a time with
  `PATH=/opt/homebrew/opt/gettext/bin:/opt/homebrew/bin:/usr/bin:/bin /usr/bin/make -j4 tests
  LINTJSON=0 ASTYLE=0`. Dev build log `macos-tests-build-instrumentation-dev-22ca8759f2.log`
  records exit `0`; its 79,150,952-byte binary SHA-256 is
  `858ddd88ec9c8cf77639392620a136cc112caa5e34f46c39aea0cb1c828918e0`. Baseline build log
  `macos-tests-build-instrumentation-baseline-b7e9a6a1f6.log` records exit `0`; its
  79,112,952-byte binary SHA-256 is
  `eb14211166ca4021c88933f88f3103b00cdd061a1283ee7fc63a058b4b66b146`.
- The first committed one-pair A/B smoke is red/non-credit by design. Runner exit `2` and
  independent validator exit `2` rejected raw artifact SHA-256
  `0c5d1431c120ae0f8913e98543da64c7cf1bd968915ef1dc6320fca3e396249c` because the baseline
  data-root hash changed during execution. The child created 3,418 ignored FlatBuffer files under
  `data/cache` after initial identity capture (8,156 -> 11,574 files). Both worktrees stayed
  Git-clean. It remains red/non-credit historical evidence; the next checkpoint repaired the
  warmup/input contract and proved it from a newly cold start rather than crediting a now-warm
  rerun.
- Fixture/input hardening is checkpointed at `c2d7921d9f` on `dev` and identical-patch cherry-pick
  `7e6d11091d` on the preserved baseline; stable patch ID is
  `c8b72321516ccf34ce160121d4da4ab2d44aee42`. The split legacy matrices cover idle,
  structural, serialization, representative dispatch/return, 500-site fairness stress, and
  genuine 0/1/10/50/100-site existing-lead saturation. Calendar turn `5220000`, 91-day spring,
  the shared seed, fixture hashes, replay resets, output/stream caps, and source/warmed tree
  identities are fail-closed. The Python suite passes 94 tests, direct lead saturation passes
  1 case/31 assertions, histogram validation passes 1/13, and `[bandit][live_world]` passes
  68/1,459. Three xhigh review rounds were resolved; final closeout reports no actionable finding.
- Exact sequential builds used the same gettext-qualified `make -j4 tests` command. Dev build
  `macos-tests-build-dev-c2d7921d9f.log` exited `0` in 49 seconds; its 79,151,528-byte binary is
  SHA-256 `d113a5480473f6e70f637aab2f030ba38bb4cf346fd0906ec8c766ad4051fa61`.
  Baseline build `macos-tests-build-baseline-7e6d11091d.log` exited `0` in 53 seconds; its
  79,113,576-byte binary is SHA-256
  `708cfeb2fc763f9083809dd182f1f480d5677f02746c826acb6bd13798017f88`.
- The first accepted cold-cache paired smoke is
  `/Users/josefhorvath/codexbulk/C-AOL-hostile-ecology-artifacts/phase0-20260802/paired-smoke-c2d7921d9f.raw.json`,
  SHA-256 `62b57c1e88778576e1c1f248c637f5427543152268cdfdc923e0dc77c985c444`.
  Runner and independent `validate --verify-files` exited `0`; two measured runs are valid.
  Both variants began at the identical 8,156-file/102,972,091-byte non-cache manifest, then each
  recorded an accepted warmup and a stable 11,574-file warmed tree. Initial/terminal fixture SHA
  remained `80f5a9c...`, fixed calendar/RNG state matched, and terminal replay equivalence passed.
  The 2,000-bootstrap summary is accepted at SHA-256 `de65f54b...`. This is integration proof;
  the following pragmatic official packet supplies Phase-0 engineering-baseline credit.
- Final benchmark instrumentation is checkpointed at `fee1e44d38` on `dev` and identical-patch
  cherry-pick `2a3e7efb17` on the preserved baseline; stable patch ID is `bf8a5649...`. Exact
  sequential builds exited `0` in 41/36 seconds. Dev binary SHA-256 is `6aada731...`; baseline is
  `75854084...`. The exact commit passes 110 Python tests, 3 focused C++ cases/1,775 assertions,
  `py_compile`, packet validation, and `git diff --check`.
- The bounded official matrix completed in 2,279 seconds: 25 cases, three paired repetitions,
  150 valid measured runs, zero failures, identical 8,156-file source-data manifests, zero fixture
  restore failures, and zero cross-variant terminal-equivalence failures. Raw SHA-256 is
  `7332059ac8f3d0578c2a4907b8be2e17d7941524bcd17f434fcb03951e109d32`; the independently
  validated 1,000-bootstrap summary is `9736b3afcb714474962e19e4dfc7fa4e3ba7deff131007584e626156e113257b`.
  Full manifest: `/Users/josefhorvath/codexbulk/C-AOL-hostile-ecology-artifacts/phase0-20260802/phase0-pragmatic-evidence-manifest-fee1e44d38.json`
  (SHA-256 `ff410e9b...`).
- Observed structural p95 maxima at 0/1/10/50/100 sites are
  `0.209/0.671/3.167/13.951/27.391 us`; normal structural max is `53.667 us` and 500-site
  stress max is `36.167 us`. The 10-site paired wall ratio CI is `0.978..1.057`; exact child CPU
  is much tighter at `0.998..1.005`. Structural phase-RSS deltas stay below `0.4 MiB` at 10 camps
  and `0.3 MiB` at 100/500; serialize-100 is below `1.8 MiB`.
- All 18 whole-save children completed real `game::save` + menu-level `game::load`: maximum save
  `472 ms`, maximum load `7.084 s`, maximum directory growth `1,480,374 bytes`; baseline/dev
  directory growth is identical for each scale. Current legacy serialized terminals are
  `79,532` bytes at structural-10, `805,185` at structural-100, and `2,626,240` at stress-500.
- Fairness caveat is explicit: normal structural scales have zero never-serviced eligible camps,
  but the deterministic 500-site legacy packet services only 125/250 and leaves 125 unserved
  after 250 updates. The accepted future gate is zero starved and at most 32 hourly waits at
  500 sites; Phase 3 owns the round-robin repair.
- Ratified provisional gates: maintenance p95 ceilings `1/2/10/50/100 us` at 0/1/10/50/100;
  20 ms normal and 100 ms stress single-update maxima; post-floor scaling `6x/12x`; 16/128 MiB
  normal/stress retained memory; 64 KiB per full camp, 1 MiB per 10 camps, 10 MiB per 100 camps;
  and save/load regression no more than both 10% and 100 ms. Forward-schema caps, long soaks,
  allocation attribution, loaded pairs, and platform packets remain their implementing-phase or
  Phase-9/10 gates rather than reasons to extend Phase 0.

Current Phase-1 evidence target:
- The independent audit mapped the single top-level save seam but found competing outing,
  roster, intelligence, and resource authorities. It also found two concrete transactional bugs:
  malformed returns mutated before survivor validation, and world deserialization cleared live
  state before nested JSON succeeded.
- `673a900067` fixes both atomicity defects. `4995a3c64e` adds world/site schema v2, one typed
  persisted active-outing identity, monotonic per-camp generation and return watermark,
  abstract/local owner plus handoff epoch, legacy active-group migration/normalization, and
  generation/key replay rejection across save/load. The old `active_group_id` is read only as a
  legacy field and is no longer serialized or consulted by runtime consumers.
- Redirected Mac test build `macos-tests-build-phase1-identity-673a900067.log` exited `0`; binary
  SHA-256 is `503542ce...`. Focused gates pass 72 live-world cases/1,536 assertions, 6 handoff
  cases/99 assertions, 1 patrol/shakedown case/12 assertions, and 2 overmap-global save cases/16
  assertions.
- `e4b75e15a3` expands that identity into the bounded schema-v3 scout owner: members/leader,
  route/waypoint, target revision, all declared phases, observations, cargo, casualties, clocks,
  owner/handoff state, and independent return/report/cargo keys and watermarks. Legacy scalar and
  transitional nested saves migrate through one validated path; malformed reservations release;
  return validation is pre-mutation and exact for generation, job, member/casualty status, and
  component watermarks.
- Exact strict build command was
  `PATH=/opt/homebrew/bin:/opt/homebrew/sbin:/usr/bin:/bin:/usr/sbin:/sbin make -j8 TESTS=1 RELEASE=0 LOCALIZE=1 LANGUAGES=all LINTJSON=0 ASTYLE=0`;
  `build_logs/macos-tests-build-phase1-scout-autoreview-fixes-233f7662d4.log` exited `0`.
  The 79,376,488-byte test binary is SHA-256
  `12970845d758c7b4d19cc62c8dfdd90b71f68ab45387e4962cb4ba14acb2f49c`.
- Final focused evidence with seed `830204929`: `[bandit][live_world]` passes 78 cases/1,965
  assertions; `[bandit][handoff]` passes 8/145; the exact patrol/shakedown consumer passes 1/12;
  `[savegame][overmap][regression]` passes 2/16. The size case passes 1/10 and records 87 bytes
  empty, 4,139 normal, and 28,115 saturated; saturated round-trip output is byte-stable and below
  the provisional 64 KiB full-camp cap.
- The final xhigh AutoReview fix pass accepted six concrete defects and repaired all six: return
  phase ordering, malformed nested reservation cleanup, contact-anchored migrated deadlines,
  universal component watermark ordering, exact casualty agreement, and authoritative job
  matching. No further review loop was run.
- `42e5bad3cd` adds persisted per-member resolution bits and a transactional split-return owner.
  A first survivor can deliver a provisional report and cargo once while the active slot remains
  reserved; an on-time second return or fixed-grace death/missing resolution finalizes once.
  Duplicate, unknown, contradictory, stale, and post-load replay packets are atomic no-ops.
- `31354b71c3` closes the one root-review defect: a missing declaration is rejected before the
  persisted fixed-grace deadline and accepted exactly at the boundary.
- Redirected strict build
  `build_logs/macos-tests-build-phase1-split-return-deadline-fix-42e5bad3cd.log` exited
  `0`. The 79,443,816-byte binary is SHA-256
  `8fb6e4e6c70492451f1d13b59e278d047c00a9301df95c3afbe2f041f4dc431d`.
  The two new cases pass 1/66 and 1/52; `[bandit][live_world]` passes 80/2,085;
  `[bandit][handoff]` passes 8/145; patrol passes 1/12; save compatibility passes 2/16.
  Empty/normal/saturated state is 87/4,190/28,166 bytes and remains byte-stable below 64 KiB.
- `7acc011951` adds one expected-phase, monotonic scout transition authority and exhaustive 10x10
  policy coverage. Burned/exposed/reporting/home/lost phases bypass the target gate and can move
  only homeward; stale/wrong-kind/wrong-job transitions are atomic rejects; unknown future saved
  phases normalize to `lost`, while missing legacy phase data remains `assembling`. Malformed
  scout-kind/raid-job reservations close on load, and legacy scavenge retains its non-report return.
- Final redirected build `build_logs/macos-tests-build-phase1-scout-transitions-fixtures-final-48d6b708a1.log`
  exited `0`. The 79,479,080-byte binary is SHA-256
  `cae011df1eea90a9a28c4375699cc325fea60365b03ee2bfe7bc014cad4a8a20`.
  The exact phase case passes 1/144; `[bandit][live_world]` passes 81/2,230. The adjacent handoff
  gate remains 8/145 and overmap save compatibility remains 2/16.
- `687d7bcecb` adds the five-state camp decision owner and pins final scout report revision,
  generation, identity, target, transition time, eligibility, and bounded reason. Provisional and
  scavenge reports remain inert; all-loss stays idle; stale dispatch plans cannot overlap pending
  assessment; cooldown-to-idle preserves the watermark; unknown/malformed saves fail closed.
- The first broad attempt compiled successfully but its final link selected unavailable
  `-lncursesw` (`build_logs/macos-phase1-camp-decision-tests.log`, exit `2`). The established Mac
  `CXXFLAGS=-D_DARWIN_C_SOURCE LDFLAGS=-lncurses` envelope then linked with exit `0`; final source
  and fixture builds are `build_logs/macos-phase1-camp-decision-dispatch-guard.log` and
  `build_logs/macos-phase1-camp-decision-replay-fixture.log`, both exit `0`.
- Exact camp-decision coverage passes 1/74; `[bandit][live_world]` passes 82/2,318;
  `[bandit][handoff]` passes 8/148; save compatibility passes 2/16. The 79,533,976-byte binary is
  SHA-256 `7bbd3f0a24a5cdc0f012bdf27b6dd9660d25bc3ae560fa06c3f79e501645c38c`.
  Empty/normal/saturated JSON is 87/4,558/28,534 bytes and saturated round-trip is byte-stable.
- Still required: hostile-operation, resource, supply, dossier, pruning, and
  component-idempotency owners. Detailed burn perception/egress remains Phase 5 behavior work.

Deferred release-harness evidence, not a Phase-0 engineering blocker:
- non-interactive Mac Keychain retrieval and a real API runner self-test from a process with neither API key in its starting environment, without printing the credential;
- removal of the old shell export only after that secure route is proven.

Claim boundary: Phase 0 establishes trustworthy infrastructure and a comparison baseline. It does
not prove the new ecology. Fixture staging is not natural world-generation proof, semantic turn
playback is not a CPU benchmark, and startup/load screenshots are not feature-path evidence.

## Held validation receipt - CAOL-WINDOWS-FREE-PLAY-RC-v0

Source reference:
- production branch: `port/cdda-master`
- candidate before this playtest packet: `a35c2b932a`
- included upstream base: `8d4959bee4`
- latest reviewed-but-unmerged upstream tip: `7cf1d08ae8`

Latest Windows candidate evidence (2026-08-01):
- A clean `just_build.cmd` SDL3 tiles-and-sound build completed with exit `0`; `cataclysm-tiles.exe` and the Windows `zzip.exe` helper were produced. The runtime reports SDL compile/link/runtime `3.4.0`.
- `python -m unittest tools.openclaw_harness.test_fixture_contract` passes all `57` fixture/handoff contract tests, including Windows `zzip.exe`, explicit cannibal profile cloning, post-snapshot `UltimateCataclysm` selection, five-family fixture staging, child-only API-key inheritance, provider-aware fallback, bounded secure macOS Keychain stdin, missing-key launch refusal, exact game-runner resolution, foreign-path rejection, and platform-specific runner overrides.
- The combined fixture/proof-classification/release-asset Python packet passes `243` tests.
- The release scenario dry-resolves on Windows with the known Windows API venv and no gameplay input after load. A real clean-environment API runner self-test passes after the harness retrieves `CATA_API_KEY` from Windows Credential Manager without logging the secret. The harness no longer mutates its own process environment, and an enabled API launch fails before process replacement when either the key or exact runner is unavailable.
- Prior direct saved-state audits return `required_state_present` for the 10:00 start, four at-home bandits, three at-home cannibals, the north zombie rider, and the west flesh raptor. A fresh disposable fixture installation now also returns `required_state_present` with exact counts of one north zombie rider, one west flesh raptor, and one southeast writhing stalker; the isolated audit profile was removed afterward. The exact committed packet still needs its final normal-map load.
- Manual handoff run `.userdata/dev-harness/harness_runs/20260801_193053/` stayed responsive with no hard startup error; direct Windows window capture showed the ordinary map, no immediate combat, and the `UltimateCataclysm` tileset.
- Catapult Dabubu's current Playtest tab already invokes packaged `manual.*` scenarios through `handoff --launch-only`; no launcher edit is needed for this scenario or option override.
- The GitHub release workflow still defines Windows, Linux, and macOS assets and verifies each package before publishing. That workflow is deliberately not triggered until after Josef's feel pass.

Required evidence:
- scenario and fixture manifests parse and remain discoverable through `startup_harness.py list-scenarios`;
- saved-state audits prove the flesh raptor, zombie rider, and writhing stalker are staged outside the initial reality bubble and prove nearby bandit/cannibal site or pressure state;
- Windows and Mac secure-store API resolution, native runner selection, and real self-tests pass without printing the key or requiring a per-run shell export;
- the exact Windows tiles binary builds from the current candidate and reaches the normal map through the manual handoff scenario;
- the handoff does not advance a scripted combat window after load;
- Windows and Mac Mini Git heads match and both working trees are clean before Josef starts;
- Josef receives a short note format for observations, reproduction clues, and severity without a target-by-target checklist.

Claim boundary:
- this is deliberate staged roaming footing, not natural world-generation discovery proof;
- the writhing-stalker row is observation footing only; its AI and zombie-rider AI/progression are being discussed separately from the dormant bandit/cannibal ecology plan;
- load and saved-state audits prove availability, not final behavior quality;
- Josef's free play is the product-feel pass and is not replaced by old targeted harness receipts;
- the unmerged upstream batch is assessed separately and remains held until after the feel pass.

## Recent historical receipts

### Closed validation receipt — CAOL-CI-RED-TRIAGE-v0

Contract: `doc/ci-red-triage-packet-v0-2026-05-06.md`.

Result: `dev` branch-health CI is green again at code head `cb21294168` (`Allow items under layered bedroom terrain`).

Initial red evidence: run `25371458600` on `5043f2c32c`, workflow `General build matrix`, failed GCC/Clang/Linux/macOS/CMake jobs. First failing clusters seen in `/tmp/caol-ci-25371458600/failed.log`: `faction_camp_test` current-target/patrol-alarm assertions, `debug_menu_test` missing entry, `flesh_raptor_test` sight assertion, `item_test` density for `zombie_rider_bone_bow`, `uncraft_test` yield drift, and `zombie_rider_test` mature-gate/direct-entry assertions.

Repair stack: `29cb5bbb97` (`Fix zombie rider CI triage failures`), `b9430f0e23`, `793c283c6b`, `53ddfefe69`, `a3538aefb9`, `1f6d022030`, and `cb21294168` repaired the branch-caused failures across zombie rider data/tests, camp/flesh-raptor/debug-menu CI-sensitive tests, NPC zone-sort ASan completion, and layered bedroom terrain item placement.

Final Actions evidence on `cb21294168`:
- `General build matrix` run `25462728843` completed success, including Android x64, oldest-supported Clang, GCC 9/LTO, Clang 18 ASan, macOS universal, GCC 9 CMake, and GCC 14 jobs.
- `Cataclysm Windows build` run `25462728845` completed success.

Claim boundary: this closes the red-CI repair lane only. It does not reopen or extend the defended-camp sight/smoke proof or any closed product lane.

### Checkpointed validation target — CAOL-DEFENDED-CAMP-SIGHT-SMOKE-HARDENING-v0

Contract: `doc/defended-camp-sight-smoke-hardening-packet-v0-2026-05-05.md`.

Parent matrix: `doc/josef-live-debug-batch-test-matrix-v0-2026-05-03.md`.

Current proof target: defended-camp sight/smoke hardening for bandit watchers and compatible cannibal stalking profiles.

Required evidence shape:
- deterministic or source-path gates for current LoS, recent LoS, cover/no-cover fallback, and smoke-obscured lead handling;
- staged/live bandit defended-camp row where a currently sighted watcher breaks LoS, backs off, reroutes, escalates/reports, or logs a concrete blocker and does not keep hot-doorstep pickup behavior;
- staged/live bandit smoke-out row where smoke on the watcher tile or sightline changes lead/posture and avoids same-tile smoke camping;
- cannibal staged/live or source-path proof that consumes the same sight/smoke discipline while preserving cannibal outcome split: no shakedown, cautious stalk/withdraw under bad odds, attack/contact only when justified;
- feature-path proof must have no stale-binary/runtime-version mismatch.

Boundary: staged/live rows may close this bounded hardening packet if they reach the real dispatch/local gate/profile path and claim only staged/live behavior. Do not claim natural random discovery, full vertical assault, full cannibal raid/contact, or tile-perfect smoke physics.

Latest green checkpoint, 2026-05-05: agent-side packet proof is complete pending review. Gates: `make -j4 obj/bandit_live_world.o obj/do_turn.o tests/bandit_live_world_test.o tests LINTJSON=0 ASTYLE=0`; `./tests/cata_test "bandit_live_world*" --reporter compact` -> 43 cases / 1002 assertions; `make -j4 TILES=1 cataclysm-tiles LINTJSON=0 ASTYLE=0`; `git diff --check`; scenario/fixture JSON validation. Live row 1: `python3 tools/openclaw_harness/startup_harness.py probe bandit.scout_stalker_sight_avoid_live --compact-stdout` -> `.userdata/dev-harness/harness_runs/20260505_102525/`, evidence class `feature-path`, `feature_proof=true`, 10/10 step-local rows green, same-run scout/current-exposure/local-contact hold-off/no-shakedown/no-combat plus bounded adjacent sight-avoid reposition for `reason=repositioning because exposed`. Live row 2: `python3 tools/openclaw_harness/startup_harness.py probe bandit.scout_stalker_smoked_watcher_live --compact-stdout` -> `.userdata/dev-harness/harness_runs/20260505_103517/`, evidence class `feature-path`, `feature_proof=true`, 11/11 step-local rows green, same-run smoke-on-watcher/sightline `local_gate` hold-off/no-shakedown/no-combat plus bounded adjacent `sight_avoid: smoke-obscured -> repositioned` for `reason=repositioning because smoke obscures lead`. Claim boundary: staged/live feature-path current-sight and smoke-out rows plus source-path cannibal split only; no natural random discovery, full vertical assault, full cannibal raid/contact, or tile-perfect smoke physics.

### Closed validation receipt - CAOL-WRITHING-STALKER-THREAT-DISTRACTION-HANDOFF-v0

`CAOL-WRITHING-STALKER-THREAT-DISTRACTION-HANDOFF-v0` is closed/checkpointed green v0 after Frau review. Contract: `doc/writhing-stalker-threat-distraction-handoff-packet-v0-2026-05-02.md`; imagination source: `doc/writhing-stalker-threat-distraction-handoff-imagination-source-2026-05-02.md`; handoff: `doc/writhing-stalker-threat-distraction-handoff-handoff-v0-2026-05-02.md`; proof: `doc/writhing-stalker-threat-distraction-live-staged-proof-v0-2026-05-03.md`.

Credited current-build staged/live rows: high-threat/allies retreat/stalk `.userdata/dev-harness/harness_runs/20260503_021310/`; zombie/distraction clean shadow-then-strike `.userdata/dev-harness/harness_runs/20260503_031247/`; night/outside anti-gnome strike `.userdata/dev-harness/harness_runs/20260503_025712/`. Gate at closure: `git diff --check`; `python3 -m py_compile tools/openclaw_harness/startup_harness.py`; scenario/fixture JSON validation; `make -j4 tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[writhing_stalker]" --reporter compact` -> 23 cases / 264 assertions; spillover guard `./tests/cata_test "[zombie_rider],[flesh_raptor]" --reporter compact` -> 24 cases / 268 assertions.

Claim boundary: staged/live feature-path proof plus deterministic seam coverage only. No natural random discovery, full natural retreat pathing, broad house navigation, door opening, burglar/locked-door solving, or general ecosystem claim.

### Handoff-boundary validation receipt - CAOL-JOSEF-PLAYTEST-SAVE-PACK-v0

`CAOL-JOSEF-PLAYTEST-SAVE-PACK-v0` is at a handoff boundary, not an active Andi lane. Contract: `doc/caol-josef-playtest-save-pack-packet-v0-2026-05-02.md`; imagination source: `doc/caol-josef-playtest-save-pack-imagination-source-2026-05-02.md`; handoff: `doc/caol-josef-playtest-save-pack-handoff-v0-2026-05-02.md`; working card: `doc/caol-josef-playtest-save-pack-card-v0-2026-05-02.md`.

Card checkpoint: six ready staged rows are listed for Josef (camp locker weather/service, bandit first-demand contact, cannibal night pressure, flesh raptor crowded-arc skirmisher, zombie rider cover/wounded contrast, writhing stalker hit-fade/light/zombie-side pressure). Optional staged bandit contrast footing is ready and caveated in the card/doc ledger. All ready rows record current-build load/start-state footing and portal-storm status; no current validation target calls for rerunning them.

### Archived closed receipts

Detailed closed validation history has been trimmed out of this active testing file. For older closed lanes and non-credit proof, use `SUCCESS.md`, `Plan.md`, `doc/work-ledger.md`, linked `doc/*proof*` / packet files, and git history. This includes closed bandit signal adapter, portal-storm warning-light, zombie rider close-pressure, camp locker zone playtests, flesh raptor, stalker pattern/live/zombie-shadow/hit-fade, multi-camp, roof-horde, Smart Zone, fire, bandit, cannibal, and older basecamp/locker packets.

## Pending probes

The shared simulation-cursor checkpoint `833599e5e4` is green on the Mac. Redirected build
`build_logs/macos-tests-build-phase1-owner-cursor-empty-hostile-fix.log` exits `0`; exact focused
logs pass `[bandit][live_world]` 86 cases / 2,714 assertions, `[bandit][handoff]` 9 / 202,
`[hostile_operation]` 3 / 243, and `[savegame][overmap][regression]` 2 / 16. Final exact-source
autoreview is clean at 0.99. The test binary is 79,698,664 bytes with SHA-256
`d6e8a9f0fe1570437cfbabda375aa10cdb0b6452bf45416fe387dca8db0bef26`.

The world-global finite-resource checkpoint `432c0f9da7` is green on the Mac. Redirected build
`build_logs/macos-tests-build-phase1-finite-resource-hybrid-fix.log` exits `0`; exact focused
logs pass resource claims/migration/size 3 cases / 2,057 assertions, the full
`[bandit][live_world]` tag 89 / 4,771, and overmap-global save compatibility 2 / 20. The compact
harvested-OMT save slope is 29 bytes per record at 500-to-1,000 records, below the 32-byte gate.
AutoReview found and the patch fixed one real malformed schema-3 hybrid that could resurrect a
harvested bounty; the single post-fix rerun is clean at 0.97. The test binary is 79,754,792 bytes
with SHA-256 `8129cf98478e32fe0fc82477f5dae07b882033f812bebbd6031be36de7df99ff`.

The bounded-supply checkpoint `37498066ba` is green on the Mac. The final redirected incremental
build `build_logs/macos-tests-build-phase1-supply-roster-fix.log` exits `0`; supply migration/time
tests pass 2 cases / 77 assertions, full `[bandit][live_world]` passes 91 / 4,848, and overmap-global
save compatibility passes 2 / 24. Coverage includes cap boundaries, abstract-to-materialized seed,
schema-v5 migration, incomplete schema-v6 fail-closed behavior, sub-day remainder round trip,
one-jump versus daily equivalence, backward-time no-op, zero-living stability, exact casualty-time
roster reconciliation, and a 730-day O(1) jump. Saturated scout/camp JSON is 29,730 bytes below
64 KiB. The test binary is 79,788,840 bytes with SHA-256
`6ee20c0fd4c8472e91323713d8a3b640e1742d9fd553435d654ba7808768831b`.

The private resource-estimate checkpoint `1aa9851902` is green on the Mac. Redirected build
`build_logs/macos-tests-build-phase1-resource-estimate.log` exits `0`; private-knowledge coverage
passes 1 case / 33 assertions, all resource tests pass 4 / 2,090, and full live-world passes
92 / 4,881. The test proves a world claim changes no camp belief, one camp's timestamped physical
estimate changes no other camp, stale/invalid updates are byte-identical no-ops, global depletion
still changes no belief, and only a later physical zero estimate marks that camp depleted across
round trip. The test binary is 79,806,184 bytes with SHA-256
`bd754c017c410066c0ded6b8dc0e3886faaa031afffd648c0af0876e02b9b3c9`.

Reference-aware pruning is green at `ddd1afe480`. Final redirected build
`build_logs/macos-tests-build-phase1-reference-pruning-final.log` exits `0`; intelligence passes
3 cases / 48 assertions, full live-world 95 / 4,931, handoff 9 / 203, and overmap-global save
compatibility 2 / 24. Forward/reverse saturation, duplicate tie-breaks, active/legacy reference
retention, stale/terminal revisions, identical-signal byte stability, bounded strings/marks, and
writer/load normalization are covered. Empty/normal/full saturated JSON is 87/5,842/48,070 bytes
and byte-stable below 64 KiB. Final binary is 79,898,584 bytes at SHA-256
`f435a54a682e7bfc061e7973e271cecd47997c15cc690cbfd55ab1df869214d7`.

Semantic observation compaction is green at `9be3e8c044`. The exact build envelope was
`PATH=/opt/homebrew/opt/gettext/bin:/opt/homebrew/opt/ncurses/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin CXXFLAGS=-D_DARWIN_C_SOURCE LDFLAGS=-lncurses make -j8 tests TESTS=1 RELEASE=0 LOCALIZE=1 LANGUAGES=all LINTJSON=0 ASTYLE=0`.
Two non-credit compile attempts exited `2` on a dead helper and then eight explicit aggregate
compatibility fields; both concrete defects were removed. The final incremental invocation exited
`0` in 9.2 seconds. Each test used
`./tests/cata_test '<filter>' --rng-seed 830204929 --reporter compact`; exact filters pass
`[observation]` 1 case / 47
assertions, `[scout_state][save]` 4 / 413, `[bandit][live_world]` 96 / 4,981,
`[bandit][handoff]` 9 / 203, `[savegame][overmap][regression]` 2 / 24, and `[save_size]` 1 / 10.
The one root review found no further correctness defect; `git diff --check` is green. The
79,978,136-byte binary SHA-256 is
`b487d7e72208bcc8ebe0ecd4413da37c3ab079cfc23e92a14b640048bed12445`. Exact logs and their
hashes are archived at
`/Users/josefhorvath/codexbulk/C-AOL-hostile-ecology-artifacts/phase1-20260802/observation-progress/`.

Faction-scoped acted-report policy is green at `258247d26c`. The post-review redirected Mac build
exits `0`. At seed `830204929`, `[report_policy]` passes 3 cases / 42 assertions,
`[bandit][live_world]` 99 / 5,046, `[bandit][handoff]` 9 / 203, and
`[savegame][overmap][regression]` 2 / 24. `git diff --check` passed before checkpoint. The final
80,057,064-byte test binary SHA-256 is
`12030ac296c498bc03b87f27949107039a036c4331e79a261114c9d4646e5e87`. Exact commands, logs,
caveats, and hashes are archived at
`/Users/josefhorvath/codexbulk/C-AOL-hostile-ecology-artifacts/phase1-20260802/report-policy/`.
The one root review found and fixed explicit unknown nested policies being mistaken for absent
legacy fields; the final evidence above is post-fix.

Component idempotency is green at `f12180de5f`. The final redirected Mac build exits `0`; at seed
`830204929`, resource passes 4/2,117, full live-world 99/5,099, handoff 10/247, playback 37/1,028,
and overmap save 2/24. Empty/normal/saturated state is 87/6,020/48,265 bytes and remains byte-stable
below 64 KiB. Canonical component/member receipts, bounded resource replay, exact issued-operation
authorization, terminal generation guards, and atomic malformed current-schema loads are covered.
Final xhigh AutoReview is clean at 0.98; exact commands and hashes are in the external
`phase1-20260802/component-keys/MANIFEST.md` packet.

Phase 1 is green. Transition checkpoint `16649b77b0` passes 3 cases / 79 assertions, full
live-world 102 / 5,178, and overmap save 2 / 24. All-phase checkpoint `e408c9c450` passes its exact
world-level hostile phase case at 1 / 833 and full live-world at 103 / 6,011. Loads emit no
diagnostic transition events and preserve canonical loaded state. Exact commands, caveats, hashes,
and binaries are under external `phase1-20260802/transition-events/` and
`phase1-20260802/all-phase-roundtrip/`.

Phase-2 roster authority is green at `563499e3fe`. The final redirected Mac build exits `0`; roster
coverage passes 2 cases / 132 assertions, full `[bandit][live_world]` passes 105 / 6,154,
`[savegame][overmap][regression]` passes 2 / 24, `[save_size]` passes 1 / 10, and
`[bandit][handoff]` passes 10 / 251. Strict schema-v10 validation, legacy migration/repair,
two-person-empty ownership, disjoint away/reserved/ready authority, abstract tile assignment, and
byte-stable materialization round trip are covered. The final 80,281,432-byte test binary is
SHA-256 `43e235eeb5ce7b3c23e38e796ba7d7dbeb5821ef9a3d19346f478ac8d3761531`;
the clean final AutoReview and exact logs/hashes are archived in external
`phase2-20260803/roster-authority/MANIFEST.md`.

Exact routine pairs are green at `c846be1632`. The final redirected Mac build exits `0`;
`[routine_policy]` passes 2 cases / 183 assertions, full `[bandit][live_world]` passes 107 / 6,347,
`[bandit][handoff]` passes 10 / 251, `[bandit][playback]` passes 37 / 1,028, overmap save regression
passes 2 / 24, and save-size passes 1 / 10 at seed `830204929`. The 80,332,328-byte test binary is
SHA-256 `fa71e8ff0d1172a4eff727976910d1be59e08cd9be236839c5d61e5f2c63adfd`; final AutoReview is clean.
Exact commands, failed non-credit build routes, logs, hashes, and caveats are archived in external
`phase2-20260803/routine-pair/MANIFEST.md`.

Fresh post-report response selection is green at `5fbefa452e`. Hostile callers no longer supply
member IDs; pinned current leads derive bandit toll/cannibal raid sizes from current threat/reward,
and apply rejects stale lead or readiness drift atomically. The final redirected build exits `0`;
the hostile-plan case passes 1 / 82, full `[bandit][live_world]` passes 107 / 6,359,
`[bandit][handoff]` passes 10 / 251, `[bandit][playback]` passes 37 / 1,028, overmap save regression
passes 2 / 24, and save-size passes 1 / 10. The 80,332,760-byte test binary is SHA-256
`a12e450a59806bc5555a6657c7424486dcded8855f7ac67ba6b5edf5894b1fe3`; exact commands, diagnostic
fixture failures, logs, and hashes are archived in external `phase2-20260803/fresh-response/MANIFEST.md`.

Capability-aware routine pairs are green at `f049104375`. Live presence/death/HP/sleep/incapacity
refresh excludes unready members; stable capability selects the strongest observer and lightest
return-safe escort; plan/apply drift rejects atomically; legacy empty templates normalize safely.
The final authoritative build exits `0`; routine policy passes 3 / 227, migration 1 / 87, save
round trip 1 / 32, multi-site 1 / 67, and full `[bandit][live_world]` 108 / 6,408. The
80,371,336-byte binary is SHA-256
`02b3e3c4bd398a0a7287578a7b49da57adcd8d3f7e7cd33b048e8ba007e89471`; resolved command-route and
fixture failures plus final logs are archived in external `phase2-20260803/capability-pair/MANIFEST.md`.

Atomic reservation ownership is green at `f65e6bd28a`. Structural plans pin activity ID/generation
and the idle mission slot; competing/stale plans reject without mutation before and after a prior
generation resolves; exact member reservations persist through reload. The authoritative build
exits `0`; reservation passes 1 / 45, structural bounty 24 / 458, and full live-world 109 / 6,453.
Structured xhigh review reports no findings at 0.96; exact evidence is in external
`phase2-20260803/reservation-ownership/MANIFEST.md`.

Phase 2 is complete through population matrix `b9fcddaa7b` and cross-camp identity guard
`a8252313b7`. Both factions are table-tested at populations 0-10 across readiness/missing/active
states; focused policy passes 3/993. Same-camp generation concurrency passes 1/70, cross-camp
identity passes 1/12, and final full live-world passes 112/7,515. The accepted Phase-0 structural
packet remains the credited scale evidence: p95 `3,167 ns` at 10 sites and `27,391 ns` at 100
sites, with `36,167 ns` at 500-site stress. A current forward runner was not credited after its
two allowed warm-up/binding attempts; a direct child identified obsolete dispatch-return fixture
ownership drift. Exact hashes, failures, and the bounded source/test rationale are in external
`phase2-20260803/phase2-closeout/MANIFEST.md`. Phase-3 validation starts with cannibal/bandit
structural-path parity, the retained small-site negative control, then full live-world regression.

Phase-3 shared routine parity is green. One common eligibility predicate now gates routine policy,
structural scan, and structural outing planning for both camp factions; the small-hostile profile
remains a negative control. The first focused run exposed only a misclassified work-camp test
fixture; after correcting it, the authoritative build exits `0`, structural maintenance passes
24/514, routine policy passes 3/993, and full live-world passes 112/7,546 at seed `830204929`.

Exact persistent structural pairs are green at `0247de602e`. Current-schema structural loads
require the canonical camp activity ID, scout/scavenge job, and exactly two reserved member IDs;
the exact pair round-trips, a forged roster-consistent singleton fails transactionally, and a
same-minute stalking replay performs no threat read and leaves bytes unchanged. Root review found
one test-only classification defect: generic singleton resource operations had masqueraded as
structural sorties, so that helper now uses the already-supported scout/scavenge kind without
weakening production validation. The redirected build exits `0`; at seed `830204929`, resource
passes 4/2,117, structural bounty 24/524, and full live-world 112/7,556. The 80,530,056-byte
binary SHA-256 is `be785a035be1d41d35e66c638975d949685f7120bb6e76fb8a4f02b8b6c028c5`;
exact commands, logs, and hashes are in external `phase3-20260803/persistent-pair/MANIFEST.md`.
The persisted bounded shared route is green at `e537ea7b49`. Final structural/live-world/handoff/
overmap-save/save-size gates pass 24/594, 112/7,626, 10/275, 2/24, and 1/10 at seed `830204929`.
The 80,603,832-byte test binary is SHA-256
`6cc628872f13f3665f8df837b5abecdae5fb1a072682615dd4d491ddce14a724`. A bounded
100-site/1,000-update structural smoke passes 1/31 with 0.182 ms maintenance p95, 0.208 ms max,
245,760-byte replay RSS delta, 525,009-byte serialized growth, and all 50 eligible camps serviced.
Its 234-update legacy wait is retained as evidence for the scheduler row, not claimed as a
fairness pass. Exact logs and JSON are archived in external
`phase3-20260803/shared-route/MANIFEST.md`.

The persisted global scheduler is green at `83c40e3bc3`. Final scheduler/frontier/fairness/
structural/live-world/handoff/overmap-save/save-size gates pass 3/21,220, 5/231, 1/3,007,
28/773, 119/29,155, 10/307, 2/24, and 1/10 at seed `830204929`. The 80,790,008-byte test binary
is SHA-256 `bba9f8d75c0212f410c44f25209356f07d46c78ab61b7240d24fb0b35425f41c`.
The validated 100-site/32-hour child records 42,417 ns p95/max, all 100 routine camps serviced
within six passes, zero fairness-replay RSS delta, and 22,755 serialized bytes of growth. Final
structured review is clean at 0.93 after fixing mixed-site slot use, start monopolies, saturated
wait ties, backoff/reset semantics, and benchmark clock drift. Exact evidence is under external
`phase3-20260803/global-scheduler/MANIFEST.md`.

Routed dispatch is green at `cab98bc55c`. Final routed/structural/scheduler/frontier/full
live-world/fairness/overmap-save/save-size gates pass 4/116, 37/5,413, 8/25,726, 5/245,
128/33,821, 1/3,007, 2/24, and 1/10 at seed `830204929`. The 81,043,688-byte binary SHA-256 is
`587e4e8e3db12f53b140d9feb54fa37b897dd34f462eade5aefa27ef7255dc1e`.
The accepted 100-site/32-hour artifact records 62,463 ns p95, 66,167 ns max, 245,760-byte timing
RSS delta, 66,002 serialized bytes of growth, all 100 camps terrain-scanned with spread one, and
all 100 routine camps serviced within six passes. The focused 16-camp fixture proves exactly eight
route callbacks, at most four final plans, two starts, no false global-budget backoff, and
byte-stable replay. Exact evidence and caveats are under external
`phase3-20260803/routed-dispatch/MANIFEST.md`.

Atomic local-pair handoff is green at `367337c9e4`. Final local-handoff/structural/existing-handoff/
full-live-world/overmap-save/save-size gates pass 1/102, 38/5,515, 10/307, 129/33,923, 2/24,
and 1/10 at seed `830204929`. The 81,129,096-byte binary SHA-256 is
`7e80e64ee92de6020ac50976a2bccee7ffc318a2578b85c488b0900515ff8d12`.
The accepted 100-site/32-hour artifact records 62,463 ns p95, 68,000 ns max, 229,376-byte timing
RSS delta, 67,826 serialized bytes of growth, and 100/100 terrain and scheduler service within six
passes. The focused case directly proves both factions, exact stable identities, owner-last commit,
reverse rollback after an injected second bind failure, save/load, and replay idempotency; the
generic benchmark does not claim real NPC materialization. Exact evidence and caveats are under
external `phase3-20260803/local-handoff/MANIFEST.md`.

Local-pair dematerialization is green at `f83b6bb116`. Final local-handoff/structural/
existing-handoff/full-live-world/overmap-save/save-size gates pass 1/189, 38/5,602, 10/307,
129/34,010, 2/24, and 1/10 at seed `830204929`. The 81,226,776-byte binary SHA-256 is
`2776f83565f8c781239a782e88ce5ff02dbb1be61f8dda15685c05c279a8cfbe`.
The accepted 100-site/32-hour artifact records 60,927 ns p95, 67,042 ns max, 262,144-byte timing
RSS delta, 67,826 serialized bytes of growth, and 100/100 terrain/scheduler service within six
passes. Focused proof covers physical death-before-cleanup, missing-is-not-dead, exact exit/HP/
cargo/death writeback, partial-read and second-quiesce rollback, save/load/replay, same-minute
freeze, and one later abstract advance. Exact evidence is under external
`phase3-20260803/dematerialization/MANIFEST.md`.

Local pair cohesion and assembled arrival are green at `71bde93d48`. Final local-handoff/
structural/existing-handoff/full-live-world/overmap-save/save-size gates pass 1/281, 38/5,694,
10/307, 129/34,102, 2/24, and 1/10 at seed `830204929`. The 81,290,984-byte binary SHA-256 is
`441f01261e6c455d24bcd6d773a6d5bf69b231b686a6b603d65baa81e56afc5b`.
Focused proof covers both factions, distinct staging, first/absent-member non-arrival, replay,
save/load, incomplete-assembly timeout, reunion and fresh timeout, two failed routes, coherent
return, physical leader death/re-election, and final dematerialization position checks. One
accepted P1 review fix closed an unload-time ownership wedge; final AutoReview is clean.
The current 100-site benchmark made two pre-measurement non-credit attempts (fixture hash drift,
then wrong fixture name), so no current timing number is claimed. The prior dematerialization
packet remains the nearest honest gross performance footing. Exact evidence is under external
`phase3-20260803/local-cohesion/MANIFEST.md`.

Finite physical bounty is green at `dfddc712d4`. The final 81,368,552-byte `cata_test` SHA-256 is
`a551b90690cb90eb1177a16c9bff49959fb93fb004749d0c8db1a4a03535aac7`; resource, structural,
save, and full-live-world gates pass 7/2,252, 41/5,833, 21/8,581, and 132/34,243 at seed
`830204929`. Full pairs contest bounty three as `2+1`; a physically recorded casualty produces
`1+2`; only the arriving camp learns the remaining estimate; each unit becomes two persisted
in-transit supply units and credits the bounded home stock once. Depleted arrival and save/replay
cannot duplicate resource or cargo. One broad-pass crash exposed and fixed two older test pointers
retained across site replacement; the final full order is green. Exact commands/hashes are under
external `phase3-20260803/finite-bounty/MANIFEST.md`. No new generic timing claim was added; the
existing 29-byte harvested-OMT slope and prior accepted 100-site footing remain valid.

Bounded abstract threat is green at `d5e76a447f`. Final `cata_test` is 81,582,248 bytes with
SHA-256 `41d850721ddbf5cff075c451103c8a3dd10e0a6857ad8559ce77aa7a1eaafb91`;
focused abstract-threat, full live-world, and combined save gates pass 5/284, 137/34,527, and 3/34
at seed `830204929`. The final accepted 100-site/32-update artifact SHA-256 is `5528ba39...`:
61,951 ns p95, 66,750 ns max, 262,144-byte timed RSS delta, 72,164 serialized growth, and 100/100
eventual scheduler service within six passes. Three structured review rounds found and closed real
arrival, hostility, wound, remote-light, local-owner, cooldown, and bounded-work defects; per the
user's one-review-loop boundary, the root performed final review and reran the complete packet
without another polish review. Exact commands, hashes, caveats, and review outputs are under
external `phase3-20260803/abstract-threat/MANIFEST.md`.

The current-schema harness audit is green for nested `active_outing`/`local_handoff` state, and the
fixture correction is checkpointed at `7f4cac4ae0`. The live packet under external
`phase3-20260803/pair-handoff/MANIFEST.md` is deliberately non-credit: two harness startups were
safe but strict-OCR yellow, the first direct fixture hit an inherited north-side shakedown contact,
and the corrected east fixture completed a visually verified six-hour wait without a structural
dispatch/handoff trace. No third fixture attempt or classifier weakening is permitted in this
slice. The full direct log identifies a concrete integration defect: the legacy player-pressure
dispatcher matched the structural lead by avatar proximity, created `#dispatch` with two members,
and made structural maintenance report `active_outings=0` plus unresolved outside pressure; runtime
spawn-tile reconciliation also restored fourteen living members instead of the fixture's intended
five. Phase-3 deterministic ownership and the accepted `d5e76a447f` 100-site CPU/RSS/save packet
remain the honest gross gate. Phase-4 lead ownership is green at `d801058e79`: origin-focused tests
pass 3/61, camp-map 18/378, exact local handoff 1/281, full live-world 140/34,598, save compatibility
2/24, save-size 1/10, and the harness 63/63. The 81,600,632-byte test binary SHA-256 is
`a29645ec0efee1a577121e6a55c9d80c5328b4f56f65e13ef6d0151c7a0dc971`; saturated live-world
serialization remains 50,572 bytes. External `phase4-20260803/lead-origin/MANIFEST.md` records the
two corrected build attempts, final exit-0 build, exact log hashes, and boundary. Typed physical
observations are green at `600685c1c2`: focused 5/147, legacy compaction 1/47, split return 2/148,
full live-world 145/34,745, save compatibility 2/24, save-size 1/10, and harness 63/63. The final
81,672,728-byte binary SHA-256 is `3a074e7a...`; saturated serialization is 51,244 bytes. External
`phase4-20260803/typed-observation/MANIFEST.md` records the corrected local-owner fixture, both
exit-0 builds, exact hashes, and private-dead/shared-return proof. Production observer/report wiring
is green at `e7c3da73e7`: focused physical observation 2/183, full abstract threat 7/467, full
live-world 147/34,928, save compatibility 2/24, save-size 1/10, and harness 63/63. The final
81,711,064-byte binary SHA-256 is `fb9198ed...`; saturated serialization remains 51,244 bytes.
External `phase4-20260803/observer-writer/MANIFEST.md` records exact commands, hashes, the two
root-found correctness fixes, and the boundary: the day/dusk/night/weather/terrain/elevation/optics
matrix and quiet no-radar controls remain next. Human-camp recovery remains sequenced after typed
observations and physical reports.

Legitimate structural visibility is green at `1738cf5ca2`: focused visibility 1/31, observation
regression 2/183, abstract threat 7/467, and full live-world 148/34,959. The test uses a real PER-8
NPC, real `sunny`/`rainstorm`/`fog` definitions, and real field/forest see costs 0/4; clear daylight
cannot cross the forest screen until elevation or optics supplies enough occlusion budget. The
81,746,904-byte binary SHA-256 is `e41ee72c...`. Structured review is clean with no finding at 0.94
confidence. External `phase4-20260803/visibility-envelope/MANIFEST.md` records both build attempts,
exact commands/hashes, and the physical-contact/blindness non-claim. Acquire/retain hysteresis is
green at `b7a2333f7f`: exact-source focused proof passes 2/78; visibility 1/31, observation 2/183,
abstract threat 7/467, and full live-world 150/35,038 passed before removal of one redundant negative
test assertion with identical product objects. The final 81,792,776-byte binary SHA-256 is
`5a8066f4...`. Both factions retain an exact persisted visual track through one extra terrain-cost
point at ages 0..60, while age 61, changed OMT/IDs/revision, malformed IDs, and expiry fail closed.
No save field was added; the request derives from the existing bounded 16-fact outing record.
External `phase4-20260803/acquire-retain/MANIFEST.md` preserves corrected/non-credit attempts and
the exact command/hash boundary. A natural live horde crossing the one-point edge remains Phase-4
live-exit proof. Bounded smoke/light evidence is green at `190fab0de5`: exact-source focused proof
passes 3/159 and full live-world passes 153/35,196. The final 81,896,504-byte binary SHA-256 is
`f899a62e...`; the exact-source build exited 0 in 62.84 seconds with max RSS 3,515,613,184 bytes.
Both factions record at most one uncertain six-hour fact per sense, round-trip it without a new
save field, reject malformed batches atomically, and batch signal plus visual facts under one
cursor. External `phase4-20260803/smoke-light/MANIFEST.md` preserves exact commands and hashes,
including the first non-credit fixture attempt. The production adapter is compile/source-hook
proof, not yet a natural live smoke/light scout scenario; legacy camp-facing signal/radar writes
remain open for the later cutover. Bounded significant-sound evidence is green at `1541b351fa`:
final sound/queue proof passes 4/442, the adjacent smoke/light gate passes 3/159, and full live-world
passes 157/35,638 at seed `830204929`. The final 82,015,048-byte test binary SHA-256 is
`9470243781f1615631ef9cf745b1fc1a4605d3a9f6d0fd4c95b018c734a01a7a`; its final exact-source
build exited 0 in 51.67 seconds with max RSS 3,510,419,456 bytes. Exact producer tags admit only
gunfire, alarms, and explosions into a 64-entry coarse-OMT queue; real observer hearing, regional
weather, route bounds, an observation-window lower bound, and three-hour expiry gate typed facts.
No new save field is added. Structured review found and closed remote-weather, shotgun-trap, and
flashbang producer defects, then reran clean. External
`phase4-20260803/significant-sound/MANIFEST.md` preserves exact commands, hashes, red environment
attempts, and caveats. Local legitimately visible zombie evidence is green at `8828bcdbfd`:
focused live-adapter/persistence/atomicity proof passes 4/389 and full live-world passes
161/36,027 at seed `830204929`. The final 82,080,072-byte test binary SHA-256 is
`f7cf14b10181b369095248eb3624db9831ecea94432261622ab46c8a9db2ff96`. One shared first-64
loaded-monster snapshot feeds only active exact-pair NPC LOS and ordinary hostile-zombie evidence;
riders, abstract horde population, avatar sight, exact map squares, and lead writes are excluded.
External `phase4-20260803/local-zombie/MANIFEST.md` preserves commands, hashes, review findings,
and the bounded tracker-order/snapshot-ID caveats. The temporary single-writer cutover is green at
`dda62833fc`: focused mode/atomicity proof passes 1/24, adjacent live-signal 4/63, full live-world
162/36,051, and handoff/save 12/331 at seed `830204929`. The 82,097,592-byte test binary SHA-256 is
`b179c23b0c8dc521585d22b2e86ace192ea5e1c2e80da1c2032017ef993aeec4`. Production enables typed
observers while caller and callee gates disable both legacy hostile-camp writers before those
writers read player position; legacy-only comparison cannot revise an observer-origin lead or its
scalar memory. External `phase4-20260804/single-writer/MANIFEST.md` records exact commands, hashes,
and the claim boundary. Autonomous discovery/radar removal is green at `f28450a2a6`, with 18
deleted-path harness scenarios retired separately at `641ea0884b`: focused both-faction return plus
500-site materialization proof passes 2/1,698, scheduler 9/27,248, signal/observation 8/769, full
live-world 161/37,686, handoff/save 12/331, save-size 1/10, and harness contracts 63/63 at seed
`830204929`. The final 82,119,560-byte test binary SHA-256 is
`a49ed545e2027174787dc87336bbc05114ca3c25f3b6c67f7f4478623e5ac997`. Production materializes
only a bounded finalist after replay/cooldown/candidate/route gates; typed facts create camp leads
only at physical return, while exact-avatar targeting/matching and direct camp signal writes are
deleted. Fresh read-only review is clean. Synthetic member records prove the bounded callback seam;
real NPC template/claim insertion remains production compile-path evidence. External
`phase4-20260804/autonomous-discovery/MANIFEST.md` records the commands, hashes, initial rejected
eager-materialization design, and claim boundary. Avatar relocation and decoy/empty investigation
proof remain after the quiet control below.

The quiet former-radar control is green at `f80c33996b`: both faction camps sit six OMT from the
real avatar, inside the deleted ten-OMT range, yet zero-budget maintenance calls none of the
terrain/threat/route/observer/signal/materialization seams and creates no lead, route, report,
decision, hostile operation, or `player@...` state across exact save/load. Focused proof passes
1/80 and the adjacent autonomous packet passes 3/1,778 at seed `830204929`; the 82,157,240-byte
test binary SHA-256 is `7d878652bc131d6696550788e612afe2d37f7e15ec3236b7f48a66bcf64549eb`.
External `phase4-20260804/quiet-radar-control/MANIFEST.md` records commands and the deliberate
boundary: this isolates proximity with zero terrain-scan budget and does not forbid the established
static zero-bounty shelter prior. The relocation control below completes the deterministic no-radar
pair.

Avatar relocation/single-writer stability is green at `531f626c6c`: a returned-report lead is
upserted at the real avatar's old OMT, the avatar moves twelve OMT while the camp remains six OMT
from both positions, and zero-budget production maintenance leaves the lead byte-identical at its
old OMT/origin/revision. The new OMT does not match, every legitimate discovery/materialization seam
stays unused, no second lead or `player@...` memory appears, and save/load is exact. Focused proof
passes 1/113, both no-radar controls 2/193, adjacent autonomous proof 4/1,891, and full live-world
163/37,879 at seed `830204929`; the 82,213,352-byte binary SHA-256 is
`7bfaf8ba7fab325cce0a7078f859714b80da2f0d259a33f424182bcca5fd26e4`. External
`phase4-20260804/avatar-relocation-control/MANIFEST.md` records the commands and boundary.

Decoy/empty signal honesty is green at `0e8c531d95`. Returned smoke/light/sound leads can be planned
only by the bounded routine path; expired clues fail closed, while an urgent prepass admits at most
eight earliest-expiry signals inside the existing 16-considered/8-route/2-start budgets and advances
the normal cursor only for normal slots. Both factions prove honest empty arrival without clearing
the no-candidate streak, and prove that exact matching typed support prevents false emptiness and
refreshes the lead only after physical home return. Generic terrain matching and the exported
site-dispatch planner reject signal-only leads. At seed `830204929`, focused proof passes 2/619,
scheduler 11/27,867, full live-world 165/38,502, handoff/save 12/331, and save-size 1/10. The final
82,349,400-byte test binary SHA-256 is
`590997596c8c27613a0512673dd3151350155beb28645a561c00f16e15531e2e`. Fresh read-only review found
the original reachability, support-matching, alternate-planner, urgent-order, and no-candidate-streak
defects; each is repaired in the checkpoint. External
`phase4-20260804/decoy-signal-control/MANIFEST.md` records commands and the bounded burst caveat:
more than eight simultaneous urgent signals may expire honestly.

Local communication and dead-scout evidence control are green at `429385ec26`. Observer-private
typed facts become shared only when both living materialized scouts are on the owned route and
inside the exact six-tile cohesion boundary; malformed counts and stale replay reject without
mutation. A separated observer's private fact dies with them, while an already-shared local-zombie
hard-danger fact can return only to the same pinned lead identity/OMT. Physical death writeback now
runs inside shared `game::cleanup_dead()` before removal, preserves exact off-route death tiles, and
does not treat dead evidence as living occupancy. At seed `830204929`, focused proof passes 3/316,
combined communication/local-zombie/handoff/abstract-threat proof passes 15/1,453, full live-world
passes 168/38,818, handoff/save passes 12/331, and save-size passes 1/10. The final 82,442,312-byte
test binary SHA-256 is `35dc71c6a5b48a2ea4285ae0a368367fc6402e033077f7af03d236420bf94fd8`.
Fresh read-only review found pinned-lead replacement, cleanup-ownership, off-route-death, and
dead-position occupancy defects; all are repaired and the final pass is clean. External
`phase4-20260804/local-communication-control/MANIFEST.md` records commands, cleanup integration,
artifact cleanup, and the deterministic/live claim boundary.

Bounded evidence provenance diagnostics are green at `4cbd85c57e`. Hourly structural maintenance
now logs a pure capped view of last-known OMT, writer/source/observer provenance, signed age, and
effective expiry. Sites rotate every hour; lead and observation windows rotate once per completed
site sweep, preventing nested phase-lock starvation without persisted debug state. Returned sound
leads use the existing three-hour expiry and smoke/light leads six hours; reachable schema-0 legacy
facts cover the explicit unbounded case. Tokens are capped/sanitized, repeated rendering is
deterministic, and serialized state remains byte-identical. At seed `830204929`, focused
`[phase4_evidence_debug]` passes 1/34, adjacent evidence/communication/local-zombie passes 8/739,
full `[bandit][live_world]` passes 169/38,852, handoff/save passes 12/331, and save-size passes 1/10.
The final 82,500,104-byte test binary SHA-256 is
`62b942e77da342332ae98f6f384a799a09395c9335b7f553d0790a202d4f943a`. Fresh read-only review
found and drove repairs for scheduler-cursor site starvation, false unbounded signal-lead labels,
an unreachable schema-1 unbounded fixture, first-eight entry starvation, and outer/inner phase
locking; its final pass is clean. External `phase4-20260804/evidence-debug/MANIFEST.md` records the
commands and deterministic/live claim boundary. The Phase-4 live/harness matrix follows;
startup/load images alone are not feature-path proof.

The first live no-radar slice is green at `5cfcf94e90`. Runtime
`a9d8ec4ff6+SDL3` (57,670,144 bytes, SHA-256
`221c6d02e7b35f78bb7473cbf41e8942a0fe33f17eb054b998388f1e08b77f5b`) loaded a disposable
five-member bandit camp with the real player six OMT away. The corrected fixture preflight proves
zero spawn-tile heads, zero leads, five ready members, and no outing; three real hours later the
saved camp still has all five members home, no target/outing, and exactly three leads whose complete
origin set is `structural_routine`. The same-run 174-line delta contains no player, legacy-radar,
observer, signal, returned-report, dispatch, or handoff trace. Harness contracts pass 71/71; the
live row is 9/9 green with clean debug guard and no portal storm. Fresh read-only review found and
closed inherited spawn-head fixture contamination and a malformed-lead fail-open edge, then passed
clean. External `phase4-20260804/quiet-live-no-radar/MANIFEST.md` preserves the corrected report,
audits, screenshots, build log, and non-credit structural-pair attempt. This proves only the quiet
bandit/field footing; exact evac terrain, cannibals, day/dusk/night/weather/optics, and
smoke/light/sound remain open.

The autonomous exact-pair handoff prerequisite is green at `bfabeed571` over behavior checkpoint
`69fc2a6ceb`. Matching SDL3 runtime `bfabeed571+SDL3` (59,975,936 bytes, SHA-256
`a353077128b5170307a7b550d9d593e89f998f11c4d1de1cc9abf854071e9e78`) produced credited run
`20260804_123313`: zero initial leads/spawn heads, deterministic autonomous road dispatch, exact
epoch-1 local ownership, incomplete-to-assembled cohesion, the same saved two-member pair with a
shared route and distinct staging, and no same-run dematerialization. All 11 feature steps are green;
the focused C++ handoff test passes 332 assertions and harness contracts pass 78/78. Site-wide
ownership is preflighted before any NPC or site mutation. Ordinary hostility may move an assembled
member, after which maintenance can honestly reacquire rendezvous. External
`phase4-20260804/autonomous-pair-handoff/MANIFEST.md` preserves the report, audits, screenshots,
build logs, and rejected non-credit attempts. This is staging/handoff proof; the completed
day/dusk/night/weather/optics matrix is recorded below.

The Phase-4 live visibility matrix is green at production runtime checkpoint `98707f2da0` and final
contract checkpoint `8afe569474`; runtime-relevant sources are unchanged between them. Credited
runs `20260804_143110`, `20260804_152858`, `20260804_145631`, `20260804_151754`, and
`20260804_152015` are each `feature-path`, 9/9 step-local, and 3/3 wait green. Exact production
traces derive road budgets 2/3/1 for clear twilight/clear day/cloudy-neutral night, acquire the
one-OMT forward road target, derive 6 with real binoculars across the clear forest screen, and apply
fog penalty 1.7 to reduce the same optical forest budget to 3 and block acquisition. Harness
contracts pass 108/108; transform-focused coverage passes 4/4; syntax, JSON, diff, and fresh
read-only closeout review are green. Forest fixtures pre-stage the known target, concrete hostiles,
and optics and defer only that site's near-terrain scan to minute 10861, one minute beyond the
experiment. The row therefore does not claim autonomous target discovery, competing-target forest
preference, cannibal live parity, or every fog/terrain configuration. External
`phase4-20260804/structural-visibility-matrix/MANIFEST.md` preserves credited and rejected runs.
Physical signal return is green at `92aadee446d9`. The exact Mac build of
`obj/bandit_live_world.o`, `obj/do_turn.o`, and `tests` exits 0; the focused transactional handoff
case passes 1/440 and `[bandit][live_world][structural_bounty]` passes 51/6,574. Coverage includes
normal and abort homeward exits, split-owner continued physical travel, camp-only dematerialization,
early-arrival hold, multi-OMT return, save/reload, stale/contradictory cursor rejection, off-camp
deadline blocking, repeated owner handoff, and recorded casualty plus survivor. Final AutoReview is
clean. This is Mac compile/unit evidence only. Run `20260804_214456` remains the honest pre-fix
red/inconclusive live artifact (`red_wait_completion_artifact_timeout`); no further OCR/wait-menu
retry is justified before the observer exists.

E1 code is green at `1081f6f6a0`. The overmap inspector revalidates world/entity/generation/owner /
authority, exact simulation cursor, NPC ID/OMT, loaded/alive state, and HP after confirmation.
Wound/heal operate on the concrete NPC; kill calls `npc::die` plus `game::cleanup_dead` and requires
the existing local-handoff casualty receipt before reporting success. A bounded unsaved receipt
ledger supplies same-turn `debug_intervention` projection provenance, the existing monitor trace,
and O4b incident input; failed/prevented attempts remain labelled receipts, while later natural
changes remain natural. Exact Mac release build/tests pass intervention 2/60, all ecology 33/697,
console 9/1,116, JSON/diff checks, and a full release tiles rebuild/link. The installed AutoReview
launcher was absent; one bounded manual review/fix pass closed delayed-provenance mixing and missing
failed-attempt receipts. The next validation target is the real field dry run plus the bandit /
cannibal casualty outcome matrix. O0 save/knowledge neutrality retries only through O4's existing
performance/save-growth harness.

The smallest field adapter is green at `15e01c1e64`. Under `DEBUG_CLAIRVOYANCE` with global debug
mode off, the console now renders only ecology snapshot/watch/incident and the existing Step/Play
footer; keyboard `A/P/./R` removes the ImGui-click dependency without exposing other debug tabs.
Exact Mac release compile/test passes `[debug_console]` 10/1,120 and the release tiles link. Field
attempt `.userdata/dev-harness/harness_runs/20260805_091051/` selected natural camp `BC-E75C82` and
confirmed Screen Recording, Accessibility, and Event Synthesizing, but is non-credit: its first
half stopped at the now-removed second toggle, and its post-build relaunch entered a case-sensitive
quit modal before adapter proof. `ecology_field_gate_attempt.json` records exact screenshot hashes
and every missing artifact. Do not add more click/OCR retries; start one clean keyboard handoff.

Field bridge behavior is green at `22004574a1`. `I` in the narrow ecology console now queues the
same selected-overmap authoritative editor outside the ImGui frame, suspends console drawing while
its blocking menus run, and returns to the same armed watch; incident capture immediately re-samples
the edit without a fake turn. The paired launch-only fixture stores the byte-identical 81-file save
payload from natural producer run `20260804_121729` (tree SHA-256
`ab06d76de1045497fa2855c8f87e599aee588e0160a5075b81e363c865b7b8ce`): schema-8 structural sortie
generation 1, local handoff epoch 1, living members 4/5, phase observing. It adds only
`DEBUG_CLAIRVOYANCE` and explicitly retains the producer's `yellow_step_local_proof_incomplete`
whole-probe caveat. Exact Mac tiles/non-tiles objects and tiles link pass; console 10/1,120,
intervention 2/60, all ecology 33/697, staged fixture contracts 126/126, JSON/diff checks, and the
launch-only dry-run pass. A fresh read-only review found no actionable defect. This behavior-only
checkpoint remained readiness footing until the credited field run below.

Observer O4 watch/run-until behavior is green at `13cbeeb072`. The shared selected projection now
feeds exactly six typed predicates (phase, evidence, exposure/burn, casualty, return/completion,
and no-progress deadline), capture/pause/fail policy, a default six-hour bounded deadline, and one
`Arm + play` action through the existing controller. Typed evidence identity makes age-only ticks
silent; movement and visible authoritative HP changes still reset no-progress. Completion/death is
latched once, missing identity remains anomalous, and gate/world/control staleness produces a
structured fatal result plus pause. Compact watch-session JSON combines the latched result and
128-row transition ring; incident schema 2 retains that watch state even for a timeout or
status-only casualty with no delta. Exact Mac watch passes 6/63, incident 6/80, all ecology 40/792,
and console 10/1,120. Affected tiles and non-tiles objects compile, the tiles runtime links, diff
checks pass, and the bounded read-only review found seven concrete defects which the targeted
recheck confirms fixed. The private console-session latch has surrounding console/compile evidence,
not a direct unit seam. Foreign-platform runtime remains unclaimed.

Observer O4 save/performance neutrality is green at `117857f551`. The accepted final four-case Mac
packet (`49836c5063215dfc...`; summary `a04c0e525d6b5273...`) runs the shared view 1,000 times closed
with zero candidates/callbacks/measured query work and 1,000 times open against 100 camp markers
with 88 us observed maximum, 100 considered/emitted, and no truncation. Closed/open 10-camp cases
each execute real `game::save` plus menu-level `game::load`, retain exact normalized authoritative
hash `fe1d0b55...`, and grow 865,589 / 1,079,260 bytes with 290/381 ms save and 7.67/7.51 s load,
inside the ratified Phase-0 envelope. The existing exact-cap test covers 2,048/256/128; the field
incident retains 2,047 trace bytes. Focused observer tests pass 9/118; TILES test link and non-TILES
`game`/`game_io` compile pass with `-Werror`; matrix/raw/summary validation and diff checks are green.
A fresh read-only review found no code defect. Limits: save cases use different seeded isolated
worlds, so no paired whole-directory identity is claimed; 88 us covers camp markers without mobile
providers or selected detail; timing is descriptive Mac evidence only. The headless TILES load path
needed null-safe tileset zoom sync, and serialized mismatch diagnostics are now bounded instead of
dumping whole worlds. Use astyle 3.1 if formatting is applied; it is unavailable on this Mac.

E1 casualty outcome reconciliation is green at `1e6a0924e7`. One focused 2x3 matrix sends bandit
and cannibal structural parties through the shared authoritative local-handoff path for one dead /
one healthy survivor, both confirmed dead, and one HP-25 wounded member plus healthy partner. Every
case saves/reloads before terminal advance, validates returned count, clears the exact reservation,
keeps the roster valid, preserves dead or wounded/unready state, and gives casualty outcomes the
72-78 hour cooldown. Two confirmed physical deaths now close on the next structural advance only
when every member is roster-dead, resolved, and casualty-recorded; all-missing/unknown still waits
the persisted deadline. Exact Mac local-handoff passes 1/646, structural-bounty 51/6,772, and
intervention guards 2/60; TILES link and non-TILES owner compile pass with `-Werror`; diff checks
and fresh read-only review are green. The matrix directly stages terminal travel/callbacks and
therefore proves reconciliation/persistence, not GUI confirmation or natural movement; field run
`20260805_101713` separately proves the real `npc::die` plus cleanup path.

The field gate is green at exact runtime `648a509cc9`, run `20260805_101713`. The documented
handoff installed only `DEBUG_CLAIRVOYANCE`; its detached child exited silently, so the exact
reported command ran attached in the same run directory. Overmap selection bound local/loaded
dispatch `BD-374153` in phase `observing`; the console armed its watch, stepped one turn, and kept
that watch alive while `I` confirmed authoritative kill of Giuseppe Bachman (NPC 4, HP 100 -> 0).
Vance Gunderson (NPC 5) remained alive/loaded at HP 100. `R` published
`ecology_incident_5249094.json` + `.png`; the 4,099-byte payload has natural `appeared` followed by
`debug_intervention` `hp_changed`, one retained intervention, 2,047 trace bytes, no truncation, and
exact `648a509cc9+SDL3`/scenario/run identity. The reopened overmap immediately showed debug
provenance; 2 entities were considered/shown in 26 us. Permissions and all artifact hashes are in
`ecology_field_gate_receipt.json`. Runs `20260805_095314` and `20260805_100813` remain non-credit:
they isolated the shared SDL3_image bool-success bug. `f997bbd368` moves incident capture outside
the ImGui callback; `648a509cc9` normalizes SDL2 zero-success versus SDL3 true-success. Exact Mac
tiles/non-tiles compile, tiles links, console 10/1,120, intervention 2/60, all ecology 33/697,
harness 126/126, and fresh read-only reviews are green. O4's credited packet above now supplies the
deferred authoritative-byte and bounded real-save/menu-load evidence; this live run itself still
claims only field behavior.

Observer O4b code is green at behavior checkpoint `541932daa5`. The schema-1 incident bundle
contains run/turn/player/build/scenario identity, the exact selected projection/token, retained
deltas and metadata, an optional 256-byte note, at most 32 ordered intervention receipts, and exact
self-referential payload bytes. Invalid identity/token/delta provenance fails closed. The Trace
action synchronously re-resolves the authority before capture, writes through temporary files,
publishes the existing-game screenshot first and JSON last, and removes partial artifacts on any
failure. Probe and handoff children receive deterministic run/profile/world/scenario environment
metadata without inheriting stale scenario state. Exact Mac tests pass incident 5/71 and console
9/1,116 at seed `240401`; all 126 harness fixture contracts, Python syntax, touched objects/tests
link, current `cataclysm-tiles`, and `git diff --check` are green after one combined review/fix pass.
At that checkpoint no live incident was credited; run `20260805_101713` below supplies it.

Observer O4a code is green at behavior checkpoint `759e0851bd`. The immutable world/ID/generation /
owner/authority token feeds a 128-record FIFO with appeared, moved, phase, visible-authoritative HP,
and anomaly transitions; mutable filters and selection bind a process-local revision; generic
monitor removal/disablement reconciles instead of leaving a false armed state; unchanged fields
advance the comparison baseline; and load/unload visibility is not an HP event. Exact Mac tests at
seed `230059` pass delta 3/334, capture 9/34, console 9/1,116, and full ecology 26/566; touched release
tiles objects, tests link, current `cataclysm-tiles`, and `git diff --check` are green after one
review/fix pass. Run `20260805_074635` is a non-credit field preflight: it records the prepared
handoff, current-binary natural selection of `BC-E75C82`, and green Screen Recording, Accessibility,
and Event Synthesizing, but SDL helper window `22148` retained focus instead of render window `22114`,
so the ImGui watch could not be armed. `ecology_watch_preflight_receipt.json` names every missing
artifact; do not grow more click/OCR heuristics or call this watch proof.

Observer O0 prepared handoff is green at the current checkpoint. Derived fixture
`bandit_phase4_ecology_observer_handoff_v0_2026-08-05` preserves inherited `DEBUG_CLOAK` and its
single new transform requests only `DEBUG_CLAIRVOYANCE`; the transform report records requested,
before/after, added, already-present, and idempotent state. `/usr/bin/python3 -m unittest
tools.openclaw_harness.test_fixture_contract` passes 125 tests, and the launch-only handoff dry-run
exits 0 with plan-only evidence. Homebrew Python 3.14 lacks the fixture suite's `flatbuffers`
dependency and is not credited. This proves disposable prepared footing only: no overlay, ordinary
start, knowledge/save-neutrality, or gameplay behavior claim is closed by the dry-run.

Observer O1 camp/dispatch core is green at the current checkpoint. Exact Mac compilation of
`obj/tiles/ecology_debug_view.o` and the focused test object exits 0; linked
`[ecology_debug][observer][phase4]` passes 5 cases / 61 assertions at seed `830204929`. Coverage
includes gate-closed zero callbacks plus byte-identical owner serialization, genuinely
unmaterialized camps, exclusion of small hostile map extras, abstract/local ownership, unresolved-
survivor-only loaded filtering, selected-only name/HP reads, stable aliases/provenance, co-located
entities, z-levels, save/load, terminal lost/dead/completed removal, exact 2,048/256/128 metadata,
deterministic `O(N log 2048)` retention, truncation, and selected-row forcing. Initial AutoReview
accepted five concrete findings across abstract camps, supported profiles, resolved loaded state,
terminal lost state, and unbounded sorting; all are fixed. Final command
`autoreview --mode local` exits 0 with no accepted/actionable findings. `astyle` is unavailable on
this Mac; Josef should apply astyle 3.1 if a formatting gate later reports drift. This is Mac
compile/unit evidence only and does not claim the O2 overmap gate, JSON artifact, or foreign
platforms.

Observer O2 overmap/JSON code is green at checkpoint `a7b844d100`. The exact Mac release/tiles test
link succeeds; `[ecology_debug]` passes 20 cases / 176 assertions and `[debug_console]` passes 9 /
1,116. `jq empty data/raw/keybindings.json` and `git diff --check` pass. Tests cover distinct stable
legend markers, orthographic/isometric viewport bounds, co-location, selection/pin behavior, exact
cache regions, matching human/JSON category/faction/loaded filters, CDDA/CTLG binary identity,
process-local reopen state, full Trace export, stable selected-only monitor output, deterministic
serialization, exact payload bytes, and an authority-index selected query that fails closed on a
stale index. Normal overmap presentation is the only active UI route; specialized overmaps and fast
travel skip observer queries. Final `autoreview --mode local` reports no accepted/actionable
finding with 0.93 correctness confidence. Live run `20260805_055304` adds the current-build
2,560x2,880 selected-camp screenshot (SHA-256 `65ed44da...`) and exact compact JSON snapshot
(payload 1,893 bytes; artifact SHA-256 `65d87153...`). Both identify natural abstract/unloaded
bandit camp `BC-E75C82` at `(140,51,0)`; JSON reports player `(133,51,0)`, all filters, five at-home
members, 1 considered/1 emitted, no truncation, 7 us query, and 20 us render. The transform report
records `DEBUG_CLAIRVOYANCE` newly added. The launch-only detached process exited with empty logs;
one direct launch of its exact binary/userdir/world command supplied the pair. Save/knowledge
neutrality and Windows/Linux runtime remain uncredited.

Observer O0 save/knowledge neutrality remains uncredited after two bounded live attempts in run
`20260805_055304`. A raw transformed-fixture/pre-save comparison was invalid because the first save
rewrote the fixture layout and the JSON-copy path enabled separate debug-console state. A second
matched comparison loaded the source gate-off fixture and derived gate-on fixture, opened the same
normal overmap, panned seven OMTs, closed, and saved at the same turn without the console. Gate-off
correctly showed no observer and gate-on showed the selected camp, but non-character aggregates,
dimension data, and character JSON still differed after removing `DEBUG_CLAIRVOYANCE` from all
three player trait/mutation locations. Exact hashes and screenshots are in
`ecology_observer_neutrality_attempt.json`. This is classified as serializer/lazy-overmap variance,
not green or a proven source defect. The deterministic gate-closed test still proves zero callbacks,
zero query time, and byte-identical authoritative live-world serialization. Per the two-attempt cap,
retry only through O4's existing save-growth/performance harness.

Observer O3a shared mobile contract is green at checkpoint `a4e4cb5c22`. Snapshot schema 2 accepts
mobile rows only through an installed provider that receives the exact region, selected ID, and
2,048 cap. Horde detail is population/interest/target; stalker detail is HP; absent sources report
disabled; wrong/empty identities and every canonical-ID collision fail closed before selection.
Exact Mac link and `git diff --check` pass; `[ecology_debug]` passes 23 cases / 232 assertions at
seed `2349632433`; the unchanged `[debug_console]` suite passes 9 / 1,116. One AutoReview pass found
two P2 defects (false-enabled absent sources and ambiguous IDs); both were fixed and the targeted
fixture was corrected to use genuinely distinct canonical site IDs. This does not claim live horde
or stalker markers, foreign-platform compilation, or a persisted identity migration. Current and
upstream `7f6b236556` owners expose no identity that survives movement plus concrete/abstract
transfer, so position hashes and creature-tracker temporaries are explicitly rejected.

Observer-backed smoke/light/sound support is checkpointed at `927251fc70`; the exact system-Python
scenario contract and JSON parse pass, the Mac tiles runtime reports that clean commit, and the
two bounded live attempts are non-credit. Run `20260805_121516` reached the natural three-fact
state and physical return with `structural outing returned signal leads=3`, but `O` opened the
profile's Mutations menu, leaving no selected observer or incident. The corrected scenario uses
the actual `m` overmap binding and relies on its exact post-save owner audits rather than requiring
all three leads in one eight-row rotating diagnostic window. Run `20260805_122335` completed its
five-minute wait with exact 300-second clock delta and matched cadence artifact, then the handler
acknowledged ordinary `vague feeling of being watched` flavor as a prompt, sent Space, created an
Unknown-command popup, and safely aborted before signal setup. Permissions were green and OCR was
not the gameplay proof. Per the two-attempt cap, do not retry or expand prompt heuristics; the row
remains open. Observer-backed target relocation followed and is green below.

Observer-backed target relocation is green at scenario checkpoints `029363748c` / `9029d4e1a4`,
credited run `20260805_124207`. The exact Mac tiles runtime reports `9029d4e1a4+SDL3`; the full
fixture contract passes 128/128. Game-authored incidents `5249093` and `5249094` move player OMT
from `(135,51,0)` to `(135,63,0)` while selected natural `BD-374153` retains its canonical ID,
`(137,51,0)` position, `(136,51,0)` destination, four-point route, generation 1, observing phase,
member IDs `4/5`, and 100% HP. Loaded state changes true to false; both intervention lists are
empty and trace metadata is bounded/untruncated. The final saved-owner audit is
`required_state_present` for the exact road target, pair, activity ID, and generation with no
missing fields. The two incident JSON/PNG pairs and pre/post overmap screenshots have verified
sizes/hashes in `ecology_target_relocation_receipt.json`. The generic harness stays
`yellow_step_local_proof_incomplete` because ordinary press/capture steps are not semantically
promoted; it had no abort. Narrow credit comes only from the game-authored payloads plus exact
owner audit. First run `20260805_123855` is non-credit after isolating a missing watch arm and
noisy save-prompt OCR. Claim limits: explicit debug teleport, captured producer with yellow
whole-probe history, one bandit dispatch, Mac only. Live decoy/empty lead control followed.

Live decoy/empty support is checkpointed at `5cffecb404`. Run `20260805_125925` is non-credit:
the legacy quiet fixture had never activated its routine scheduler, so the first hour initialized
eligibility without dispatch. Run `20260805_130217` is also non-credit and failed before gameplay:
the strict loader correctly rejected routine scheduler fields inserted into a pre-v12 site. The
replacement base is the preserved schema-12 world from credited quiet run `20260804_103631`.
Its derived transform removes the captured three terrain priors, adds exactly one returned smoke
clue at `(136,51,0)` with bounty/threat `0` and confidence `3`, stages scheduler clocks at
`8280/3960`, and adds only `DEBUG_CLAIRVOYANCE`. The two exact contracts pass 2/2, the full fixture
suite passes 130/130, JSON/dry-run are green, and an isolated install reports 3 -> 0 -> 1 leads with
the exact scheduler fields and newly added mutation. No third live run is allowed; the row stays
open.

Phase-4 evidence aging/pruning is green at exact checkpoint `ed47145504`. Exact Mac tests pass:
focused aging 1/113, same-day scheduler regression 1/10, benchmark fixture 1/64, broader
`[structural_bounty]` 51/6,780, and the exact-identity `tests` rebuild. The independently validated
six-case serial packet is accepted with zero failures: raw/summary SHA-256 `5eb83b7f...` /
`ca8de252...`. One direct 730-day jump ages/prunes 0/24/240/1,200/2,400 returned signals and
prunes 0/40/400/2,000/4,000 total leads at 0/1/10/50/100 camps, leaving the exact 24-per-camp
harvested/dangerous set. At 100 camps the saturated pass is 608 us, 6,400 -> 2,400 leads, and
3,171,377 -> 1,646,857 authoritative bytes; post-floor 50/100 scaling is 4.96x/10.33x from the
10-camp case. This is below the 20 ms cadence-avalanche gate but is not compared to the tighter
legacy scoped-maintenance ceilings. Non-save timing-replay RSS deltas are 0-0.31 MiB (0.19 MiB at
100); the real-save case's 77.2 MiB load-path rise stays below the 128 MiB stress cap. That row
saves in 364 ms, loads in 6.925 s, grows the disposable whole-save directory by 1,124,837 bytes,
and preserves the exact aged owner serialization after reload. These absolute values are below the
Phase-0 observed maxima; no paired save/load regression or Linux/Windows runtime is claimed.
External manifest: `/Users/josefhorvath/codexbulk/C-AOL-hostile-ecology-artifacts/phase4-20260805/evidence-aging/MANIFEST.md`.
Fresh read-only Phase-4 exit review at `0f7f7bbc63` found no code defect. It closes only the exact
player-dispatcher-read and dual-writer/legacy-radar exit rows. The signal-discovery row remains open
because its live smoke/light/sound attempts are non-credit; stale/contradictory honesty remains open
because the live decoy attempts are non-credit. Deterministic evidence for both remains strong but
is not promoted to live. Phase 4 stays held for Josef's later disposable playtest packet without
another agent retry.

Phase-5 watch-ring metric footing is green on Mac: exact requested object/test link passes and
`[bandit][live_world][watch_ring]` passes 1 case / 12 assertions under coordinator rerun. The pure
helper returns minimum same-z 2D Chebyshev distance across real footprint cells, so straight and
diagonal distance-3 points share one ring; empty/no-same-z returns no value, and order/duplicates do
not matter. Tests also cover footprint interior zero, a multi-cell nearest point that differs from
an arbitrary anchor, mixed z-levels, and the two-intervening-OMT distance interpretation. No route,
selection, persistence, avatar coordinate, or production behavior is changed; Mac compile/unit only.
Phase-5 exact-ring watch selection is green on Mac: exact requested object/test link passes and
`[bandit][live_world][watch_selection]` passes 1 case / 25 assertions under both delegated and
coordinator runs. The pure selector requires caller-verified reachable, concealed, clear-two-OMT,
nonnegative-route-cost candidates at exact nearest-footprint distance three, then selects lowest
route cost and stable z/y/x. Diagonal, multi-cell, order/duplicate, every gate, and empty/wrong-z /
interior/near/far controls pass. It performs no terrain scan, persistence, fallback, or production
mutation; Mac compile/unit only. Next evidence class is an explicit bounded farther-or-abandon
outcome when the exact ring is impossible.

Phase-5 bounded watch fallback is green on Mac. Pure `select_watch_ring_candidate(...)` prioritizes
any qualified exact distance-3 candidate, otherwise permits only distance 4-5 and ranks by distance,
route cost, then stable z/y/x; closer and 6+ candidates reject. Typed outcomes distinguish exact,
fallback, empty footprint, and no-safe-candidate abandonment. Root review caught and fixed one valid
exact result that initially retained the default abandoned outcome. Exact requested link passes;
delegated and coordinator runs pass fallback 1/24 plus adjacent exact selection 1/26, and
`git diff --check` is clean. Pure policy only, with no terrain enumeration, pathfinding, persistence,
live adapter, or production mutation; Mac compile/unit only. Root roadmap audit therefore keeps the
exact/fallback behavior rows open: evaluator tests cannot impersonate a game route. Next evidence
class is a schema-owned watch route. A bounded direct route-read experiment compiled after adding
source-compatible read fields but correctly failed dispatch: apply-time replay reconstructed the
ordinary target route, and current save/load canonicalization would reject an ephemeral watch route.
The experiment and its test were removed with `apply_patch`; the tree returned clean. This is an
ownership finding, not a red product defect. Next proof must persist bounded target-footprint /
selected-watch metadata in the existing outing owner, migrate schema 8, and pass apply/replay/load
before the live geography adapter is allowed to consume the pure selectors.

Phase-5 watch-route ownership is green at `dfb19de3aa`. Structural schema 9 persists a sorted,
deduplicated target footprint capped at 64 OMTs and one immutable exact/fallback watch OMT plus route
cost; apply caps candidates at 256, recomputes the pure selector, compares the exact simulation
cursor, treats exact replay as unchanged, and rejects stale/conflicting/malformed state atomically.
Schema 8 migrates to an unselected singleton target footprint. Exact Mac build/link passes;
`[watch_persistence]` passes 1/71, ring 1/12, exact selection 1/26, fallback 1/24, full
`[structural_bounty]` 51/6,780, and `[handoff]` 5/237. `autoreview --mode local --thinking high`
reports no accepted/actionable finding with 0.90 correctness confidence. `astyle` is unavailable on
this Mac, so no astyle-diff claim is made. The checkpoint performs no live terrain/path query and
does not close either selection row; the next evidence class is the bounded production adapter.

Phase-5 live watch geography is green at `ef962e6e88`. The production reader resolves a verified
basecamp footprint even when the lead targets an expansion, otherwise uses the structural target
singleton; real OMT see cost, explicit Chebyshev approach clearance, and NPC pathfinding with the
whole target footprint excluded feed schema 9. Terrain work is capped at 128/64/64 candidates per
exact/distance-4/distance-5 ring, while one maintenance pass shares eight actual watch path solves;
source-aware shortlisting prevents coordinate-side starvation. Scheduler scoring, exact-pair
materialization, atomic apply, save/load, replay, fallback, abandon, expansion-safe routing, and one
real excluded-OMT detour are covered. Review found five correctness defects across coordinate bias,
footprint crossing/detour, expansion targets, and global solve accounting; all were fixed before
checkpoint. Exact Mac build includes `obj/overmapbuffer.o`, `obj/bandit_live_world.o`, `obj/do_turn.o`,
the focused test object, and `tests`; adapter passes 1/50 and full structural passes 51/6,780 with
fixed seed 1. One earlier focused run printed 48/48 pass before exit-time 139; the immediate
fixed-seed rerun exited 0, and the final 50-assertion detour packet exits 0. `git diff --check` is
clean. `astyle` remains unavailable, and no Linux/Windows runtime is claimed. Physical movement
still follows the ordinary structural target route, so both watch-selection gameplay rows stay open;
the next evidence class is shared-route travel to persisted `selected_watch_omt`.

Phase-5 physical watch travel is green at `d4e6579aed`. Structural schema 10 consumes the selected
exact/fallback watch through the existing outing route owner as a canonical symmetric five-waypoint
route; the pathfinder-owned approach must be adjacent, both the solved path and persisted waypoints
exclude the target footprint, and target identity remains unchanged. Schema-9 saves retain their
legacy target route. Arrival persists the pair at the selected watch in `observing` without falsely
checking, harvesting, or revising the remote lead, and repeated maintenance does not fabricate a
second arrival. Exact Mac compile/link passes; adapter is 1/72, structural 51/6,780, persistence
1/71, and handoff 14/343 at seed 1. `git diff --check` is clean. `astyle` is unavailable; no Linux
or Windows runtime is claimed. The next evidence class is distinct cohesive observer/cover staging
inside the watch OMT through the existing local-handoff owner.

Phase-5 watch pair staging is green at `62e26812d6`. Observing schema-10 materialization derives the
nearest same-z target-footprint OMT deterministically, then reuses the local-handoff tile selector on
that target-facing watch edge; two stable members retain distinct passable staging tiles, the normal
rendezvous/guard-position path, and exact reload identity. Both living entry and staging pairs are
validated inside the existing six-tile cohesion radius at plan and load time, while returning pairs
retain ordinary egress-facing staging. The schema-8-only local-zombie adapter gate was also corrected
to admit later compatible structural schemas. Exact Mac build/link passes; adapter 1/91, local
handoff 1/651, watch geometry 1/15, structural 51/6,785, and local-zombie observation 4/389 pass at
seed 1. `git diff --check` is clean. No GUI/live materialization screenshot or Linux/Windows runtime
is claimed. The next evidence class is the actor-specific covert non-combat relationship.

Phase-5 covert disposition is green at `c23e817132`. The exact schema-10 local structural pair gets
one derived, actor-specific neutral relationship only toward the exact effectively player-owned camp
target; unrelated factions and malformed/duplicate/resolved ownership fail closed. Ordinary loaded
reciprocal visibility plus authoritative off-bubble positions control party-wide return clearance,
including asynchronous home arrivals and atomic outing/local-handoff phase synchronization. Player
and allied attacks exit through `npc::on_attacked`; successful hostile spell effects, actual blast
propagation, and avatar-attributed manual multi-turrets are covered without treating selection,
jams, failed concentration, or automatic synthetic turrets as attacks. Exact Mac build/link passes;
`[covert_disposition]` passes 2/97, `[npc][npc_ai]` 3/69, `[magic][spell]` 11/189,
`[vehicle][gun][magazine]` 1/2,328, and `[local_handoff]` 1/651. The final command
`python3 /Users/josefhorvath/.codex/skills/autoreview/scripts/autoreview --mode local` exits 0 with
no accepted/actionable findings after accepted fixes for turret attribution, failed spells,
asynchronous home arrivals, and local-handoff phase synchronization. `git diff --check` is clean.
`astyle` is unavailable, and no live GUI, Linux, or Windows runtime claim is made. Next evidence is
legitimate exposure/burning processed once before generic hostility/movement with persisted egress
and rally ownership.

Phase-5 physical burn is green at `339aa54c4d`. Reciprocal ordinary visibility explicitly excludes
debug clairvoyance, both egress paths preflight, and one cursor-checked owner transaction records the
typed burn fact plus outing/local-handoff phase before assigning the persisted egress route. A final
review fix batches physical death and deadline-missing writeback before the survivor egress latch.
Redirected Mac test build and tiles link exit 0; `[covert_burn]` passes 2/68,
`[covert_disposition]` 2/97, `[npc][npc_ai]` 3/69, `[local_handoff]` 1/651, and
`[structural_bounty]` 51/6,785. `git diff --check` is clean. Final
`python3 /Users/josefhorvath/.codex/skills/autoreview/scripts/autoreview --mode local` exits 0 with
no accepted/actionable findings after the accepted casualty-wedge repair. `astyle` 3.1 is
unavailable; no live GUI or Linux/Windows runtime is claimed. Next evidence is bounded
threat/concealment-scored egress; the current persisted return approach is only reachable,
target-excluding route footing.

Phase-5 danger-scored egress and immediate survival are green at `0658697276e2`. Redirected
`make -j8 LOCALIZE=1 TESTS=1 tests` and the Mac release-tiles link exit 0. `[covert_burn]` passes
2/162, `[covert_disposition]` 2/98, `[local_handoff]` 1/651, and `[structural_bounty]` 51/6,785.
Coverage includes safe-pool precedence over hard danger, route-wide legitimate soft-danger and
concealment scoring, deterministic caps/ties, no inward candidate or local/OMT path, actor-specific
field/trap survival, no-route/exposed return, exact scored-egress save plus local/abstract/local
handoff, post-rally death reconciliation, physical signal/cargo writeback, canonical return receipt,
and bounded persistent immobility without premature missing closure. The final scoped autoreview
exits 0 with no actionable finding after fixes for rematerialization, writeback/receipt loss,
post-egress casualty cleanup, and immobile owner retention. `git diff --check` is clean. `astyle`
3.1 is unavailable; no live GUI, Linux, or Windows runtime is claimed. Next evidence is failed-egress
route memory and retry hysteresis, not another proof-infrastructure expansion.

The foreign-platform classifier and native writer contract are repaired at `d12edba150` with 60/60
tests. Clean-environment Mac secure-store/API proof remains a later release-harness gate; it must
not trigger another pause, retry, or Discord blocker during deterministic ecology work.

Production-candidate mutation remains held. Closed zombie-rider,
flesh-raptor, writhing-stalker, roof-horde, Smart Zone, fire, and older bandit/camp proof trains
remain historical evidence only and must not be rerun as Phase-0 ritual.

---

## Reusable commands

Use these when they are actually the missing evidence, not as ritual.

### Narrow camp deterministic pass after a code slice lands
- `git diff --check`
- `make -j4 obj/basecamp.o tests/faction_camp_test.o tests LINTJSON=0 ASTYLE=0`
- focused `./tests/cata_test` filters for the touched camp/locker/patrol reporting path

### Plan status summary command
- `python3 tools/plan_status_summary.py --self-test`
- `python3 tools/plan_status_summary.py /plan`
- `python3 tools/plan_status_summary.py /plan active`
- `python3 tools/plan_status_summary.py /plan greenlit`
- `python3 tools/plan_status_summary.py /plan parked`

### Fresh tiles rebuild only if a later handoff really needs live proof
- `make -j4 TILES=1 cataclysm-tiles`

## Local build caveat

On this Mac, treat top-level `make -j4 tests` as the reliable path for a fresh `cata_test`.
Avoid treating `make -C tests cata_test` as authoritative here; it has been a repeated source of toolchain/stale-build nonsense.
Also: if you actually need a fresh tiles binary, use `make -j4 TILES=1 cataclysm-tiles`; plain `make cataclysm-tiles` is not an honest rebuild path here.

Phase-5 bounded failed-egress memory is green at `0dc85f5bc9`. Redirected Mac test build and
release-tiles link exit 0. With `--rng-seed 424242`, `[covert_burn]` passes 2/278,
`[local_handoff]` 1/651, `[structural_bounty]` 51/6,785, `[covert_disposition]` 2/97, and
`[ecology_intervention]` 2/61. Coverage includes the three-attempt cap, same-minute revision CAS,
endpoint/route-footprint persistence, legacy schema-10 migration, local/abstract/local handoff,
strict retry versus authorized lateral replan, terminal home-route preflight, authoritative no-home
abandonment, and failed-footing exclusion across exposed/report/home overmap and local emergency
movement. `git diff --check` and final scoped autoreview are clean. `astyle` 3.1 is unavailable; no
live GUI, Linux, or Windows runtime is claimed. That checkpoint fed the completion slice below.

Phase-5 full bounded egress completion is green at `b9f29496bd23`. With fixed seed 424242,
`[covert_burn]` passes 2/308, `[local_handoff]` 1/651, `[structural_bounty]` 51/6,785,
`[covert_disposition]` 2/98, and `[ecology_intervention]` 2/61. Coverage requires every survivor at
the persisted endpoint and an explicitly evaluated clear ordinary-vision acquire read; unknown
in-bounds actors hold, visible rallies extend outward for no more than three attempts, member paths
are nondecreasing in target distance from their actual starts, legacy schema-10 chains cannot gain
new retries, and endpoint ownership releases for camp return/dematerialization. The Mac test build,
release-tiles link, `git diff --check`, and final scoped autoreview are green. The first release link
attempt exposed only a stale generated tiles PCH configuration; removing that generated PCH and its
dependency file made the second run green. `astyle` 3.1 is unavailable; no live GUI, Linux, or
Windows runtime is claimed. Next is route-commitment/hysteresis proof across bubble and visibility
transitions.

Phase-5 bubble/visibility reset prevention is green at `42bfc59dcd`. No production state was added:
the focused contract proves the existing authoritative phase graph, retry revision, persisted route
memory, and local-handoff snapshot stay locked across unknown visibility, legitimate member-contact
updates, save/load, and local-to-abstract-to-local ownership. Evaluated-clear completion advances to
`returning_report`, and later visibility/contact cannot regress it.
`make -j8 LOCALIZE=1 TESTS=1 tests` exits 0; fixed-seed `[covert_burn]` passes 391 assertions / 2
cases and the exact one-way phase case passes 187 / 1. `git diff --check` is clean. `astyle` 3.1 is
unavailable; this test-only slice
does not claim a fresh tiles link, live GUI behavior, Linux, or Windows runtime. Next is the credited
observer-backed visible-burn/egress/rally live row.

Phase-5 visible-burn producer footing is checkpointed at `5f4d091250`, but runs
`20260806_100318` and `20260806_100839` are startup-only red/non-credit and exhaust the row's
two-attempt cap. Both had green input/capture permissions and stopped before gameplay: the first on
forbidden schema-12 `headcount`, the second on duplicate legacy `active_group_id`. Harness repairs
`685307349e` and `38a130e146` make roster shaping schema-aware, preserve canonical `active_outing`,
reject fabricated current-schema outside ownership, and pass focused 2/2 then 3/3 contracts plus
Python compile/diff checks. Do not claim burn, observer, egress, or rally from either run and do not
launch a third producer probe now. The next focused evidence is one last-local-tick dematerialization,
real abstract maintenance tick, save/load, and rematerialization with the exact burned egress owner.

Phase-5 burned abstract resume is green at `2b721e3f2e`. The rebuilt Mac test target exits 0;
fixed-seed `[covert_burn]` passes 2 cases/426 assertions and `[local_handoff]` passes 1/651. The new
cross-layer case reloads immediately before and after burn, dematerializes on the last local tick,
runs `advance_structural_bounty_outings`, reloads again, and rematerializes with the same egress,
retry revision/routes, valid cursor, and exactly one burn fact. Initial autoreview correctly found
that the shared homeward predicate also covered ordinary `returning_home`; the accepted P1 was fixed
by retaining only burned/exposed/report snapshots, an explicit ordinary-home control was added, and
the required rerun reports no actionable findings. No astyle 3.1, tiles/live GUI, Linux, or Windows
claim. Next evidence is the only-one-loaded-member partial-bubble boundary.

Phase-5 partial-bubble burn atomicity is green at `86c8c2fb3b`. The production-shaped two-record
payload with one stable member ID but `present=false` is rejected byte-for-byte: phase, route,
retry state, casualty state, and the missing member's physical snapshot do not change, and no burn
fact is created. Supplying the later complete read applies once; replay is inert with one burn fact.
The delegated rebuild exits 0, the exact fixed-seed case passes 1/449, `[covert_burn]` passes 2/453,
the root rerun passes 1/449, and `git diff --check` is clean. This is test-only and claims no live or
cross-platform runtime. Next evidence repeats the full burned load/unload cycle.

Phase-5 repeated burned handoff cycling is green at `6217823710`. After the first reload and
rematerialization, the test traverses the real cohesion plan/commit, dematerializes a second time,
runs real abstract structural maintenance, reloads, and rematerializes again. Both cycles retain the
same egress, retry revision/routes, valid owner cursor, and exactly one burn fact. The first attempt
correctly failed because a rematerialized pair must reassemble; adding the canonical cohesion step,
not a flag mutation, made the sequence honest. Mac build and diff check pass; the exact case passes
1/474 and `[covert_burn]` 2/478. Test-only; no live/cross-platform claim. Next: authoritative member
and leader death during a burned handoff.

Phase-5 burned-handoff casualty reconciliation is green at `76eb587645`. Either exact member dies
during dematerialization through normal casualty/resolved-member/roster writeback; if the leader
dies, the plan elects the canonical survivor before commit, both ownership commits synchronize the
leader fields, replay returns unchanged, and the survivor retains the burned egress through
save/load and rematerialization with one burn fact. A deliberately serialized pre-fix dead-leader
shape loads unchanged and repairs on the authoritative abstract-to-local transition. The redirected
Mac rebuild exits 0; fixed-seed exact burn passes 1/532, `[covert_burn]` passes 2/536, and
`[local_handoff]` passes 1/651. AutoReview found and drove fixes for replay idempotency and pre-fix
save compatibility; the final rerun is clean. One non-credit concurrent exact launch raced on the
shared test player-template file, so credited Cata runs were serial. `git diff --check` is clean.
No astyle 3.1, tiles/live GUI, Linux, or Windows claim. Next: slow/injured follower continuity.

Phase-5 slow/injured burned-follower continuity is green at `df81293bca`. The test creates the
injury only through a 45%-HP dematerialization read, then carries `wounded_or_unready`, phase, exact
egress, attempt/revision/routes, and one burn through real abstract maintenance, save/load, and
rematerialization. With the leader at the rally and follower still on the watch OMT, completion is
byte-inert; moving the authoritative follower read to the persisted egress with evaluated-clear
visibility completes once and replay is inert. Delegated Mac rebuild exits 0, exact fixed-seed burn
passes 1/591, `[covert_burn]` passes 2/595, the root exact rerun passes 1/591, and diff check is
clean. This is test-only owner/position proof, not actual loaded-NPC speed profiling. No astyle 3.1,
tiles/live GUI, Linux, or Windows claim. Next: the production no-legal-egress seam.

Phase-5 production no-legal-egress closure is green at `8a7142757a`. Two real unloaded NPCs are
owned by the overmap at naturally impassable depth, so the public production failure adapter reads
their authoritative positions without active-map placement or terrain mutation. It recomputes all
adjacent retry candidates and both camp-return routes, exhausts into canonical stranded-return
closure, marks both living survivors orphaned, releases the exact generation, applies the six-hour
cooldown, clears stale travel goals, and guards both survivors. Replay is byte-inert. The redirected
Mac rebuild exits 0; the exact case and `[live_egress]` each pass 1/50; `[covert_burn]` passes 3/645;
`git diff --check` and final autoreview are clean. An earlier otherwise-green design was rejected
after review found that restoring only terrain IDs could leave passability/predecessor metadata
contaminated; no such mutation remains. No astyle 3.1, tiles/live GUI, Linux, or Windows claim.
Next: production soft-danger escape selection.

Phase-5 production soft-danger escape is green at `3384846ee9`. The test creates two active thug
members, a canonical schema-10 local owner, a real player-owned target camp, ordinary reciprocal
visibility, and actual overmap route binding. The first production burn supplies the terrain-based
baseline. After restoring the identical world/NPC route state, the scout records one unexpired
schema-1 shared visual certainty through `record_active_typed_observations` at that baseline
endpoint. The second production burn selects a different endpoint and neither the persisted route
footprint nor either concrete NPC route contains the danger tile; both calls remain one-shot. The
delegated Mac build exits 0; delegated and root exact pass 1/42; `[live_egress]` passes 2/92;
`[covert_burn]` passes 4/687; diff check and final autoreview are clean. Fixed-fixture coordinates
change `(1,2,0) -> (1,1,0)` and are evidence, not a compass-direction contract. No astyle 3.1,
tiles/live GUI, Linux, or Windows claim. Next: asleep/blind/covered defender optics boundaries.

Phase-5 reciprocal production optics are green at `8f4885582f`. One shared side-effect-free gate
now applies directionally to the avatar, allied defenders, and both scout members before raw
`sees_without_clairvoyance` can supply reciprocal exposure. The exact real active-pair/player-camp
case proves byte-inert rejection for ordinary sleep, blindness, narcosis plus `SEESLEEP`, sleeping
scouts, blind adjacent scouts, and an opaque three-by-three cover screen; a sleeping `SEESLEEP`
defender and restored ordinary reciprocal sight each produce a one-shot burn with real bound egress.
The first review preserved legitimate `SEESLEEP`; the next exposed asymmetric avatar/scout adjacent
sight; the final concrete finding added unconditional narcosis suppression. Redirected Mac rebuild
exits 0; exact passes 1/86; `[live_egress]` passes 3/178; `[covert_burn]` passes 5/773; diff check is
clean. No astyle 3.1, tiles/live GUI, Linux, or Windows claim. Next: darkness/weather changing
mid-watch.

Phase-5 darkness/weather changing mid-watch is green at test checkpoint `7f3f0261fd`, with no
production change. One real active schema-10 pair and player-owned camp keep the same owner while
time advances monotonically from construction noon through clear midnight, the next late-day
clear/fog differential, and following clear noon. Clear midnight and fog five minutes before sunset
both prove reciprocal ordinary visibility false, burn 0, and byte-identical serialized owner state;
clear weather at the identical late-day time proves the visibility differential, and later clear
noon burns once, binds both concrete NPC routes, and replays inertly. The delegated first attempt
constructed the owner at midnight and lacked a valid cursor; full-noon fog remained honestly visible
even at 50 tiles. Neither was credited. Root corrected time to remain monotonic and removed owner
resets between no-op calls. Redirected Mac build exits 0; exact passes 1/36; `[live_egress]` passes
4/214; `[covert_burn]` passes 6/809; diff check is clean. No astyle 3.1, tiles/live GUI, Linux, or
Windows claim. The burned-egress boundary matrix is complete. Next: assessment timing and replay.

Phase-5 structural watch assessment foundation is green at `bf93d6cf77`. Redirected Mac tiles
objects and test-binary link exit 0. Fixed final evidence: `[scout_assessment]` 1 case / 95
assertions, `[structural_bounty]` 51 / 6,785, `[camp_decision]` 3 / 124,
`[watch_persistence]` 1 / 70, `[physical_report]` 1 / 16, `[split_return]` 2 / 148, and
`[covert_burn]` 6 / 812. The assessment case covers exact 119/120, replay, shared-only target
evidence, private-fact exclusion, schema-10 migration from earliest target evidence, camp-side
revision replacement without field retargeting, exact 479/480 expiry, schema round-trip, durable
normal/inconclusive reports, target identity, and dispatch deadline propagation. One different burn
seed failed the unchanged geometry-sensitive NPC sight assertion; the controlled previously green
seed passed, so no optics change is claimed. Two autoreview passes found concrete ownership defects;
field pinning, retained abstract-handoff phase, private evidence, migration, report identity,
bounded expiry, cooldown consumption, and generated user-dir cleanup are closed. The remaining
review finding is the active next row: an honest route-qualified two-hour no-progress alternate
cannot be simulated by changing an OMT or route cursor in place. No astyle 3.1, live GUI, Linux, or
Windows claim.

Phase-5 alternate-watch route footing is green at `3c66be732c`. The Mac tiles object build and
test-binary link exit 0; `[alternate_watch]` passes 1 case / 31 assertions,
`[watch_persistence]` 1 / 70, `[structural_bounty]` 51 / 6,785, and `[scout_assessment]` 1 / 95.
The live geography adapter retains one deterministic non-primary route-qualified candidate, current
schema-10 saves round-trip it, old saves default to none, malformed alternate routes reject without
mutation, and replay is stable. This proves route ownership only: no scout has moved to the
alternate yet. Next evidence must show the two-hour transition consumes this exact route under both
abstract and local owners without teleportation or target-revision drift.

Phase-5 abstract alternate-watch transition is green at `3dd6c28a79`. The Mac tiles test build
exits 0; `[alternate_watch]` passes 1 case / 47 assertions, `[watch_persistence]` 1 / 70,
`[structural_bounty]` 51 / 6,785, and `[scout_assessment]` 1 / 95. Exact minute 119 is inert,
minute 120 swaps only the selected/alternate route owner, reload is stable, second-watch minute 119
is inert, and minute 120 returns an explicit 12-hour-cooldown inconclusive report. The focused
round-trip first caught and removed an invalid test-only forest job mutation; production route
state was consistent. Local ownership and large-jump equivalence remain open. No astyle 3.1, live
GUI, Linux, or Windows claim.

Phase-5 physical local alternate-watch relocation is green at `1fd66d12a9`. Mac tiles
`bandit_live_world`/`do_turn`/test objects and linked tests exit 0. Final evidence:
`[alternate_watch]` 2 cases / 136 assertions, `[local_handoff]` 2 / 740,
`[structural_bounty]` 51 / 6,785, and controlled seed 424242 `[covert_burn]` 6 / 812. The owner
tests cover exact 2h request, pre-departure abort, one persisted pending bit, save/reload, derived
route repair, stale/split rejection, rollback, casualty, split active/inactive exposure, exact
loaded local completion, exact unloaded abstract completion, new-watch entry/staging/exit geometry,
target/revision/evidence preservation, and replay. A different burn seed reproduced the unchanged
geometry-sensitive reciprocal-sight fixture failure; controlled seed 424242 remains green. A final
fresh read-only review found no concrete defect. Actual overmap travel/reload and loaded-arrival
feel remain runtime seams; poor-night/three-window/large-jump equivalence is next. No astyle 3.1,
live GUI, Linux, or Windows claim.

Phase-5 alternate-watch deadline equivalence is green at `3a0a895118`. The redirected Mac tiles
test build exits 0; `[alternate_watch]` passes 2 cases / 151 assertions, `[scout_assessment]`
1 / 95, `[watch_persistence]` 1 / 70, and `[structural_bounty]` 51 / 6,785. Minute polling,
hourly polling, and one 200-to-440 jump produce byte-identical serialized owners. The one-jump
trace contains exactly two transitions anchored to minutes 320 and 440; no late-poll timestamp,
duplicate transition, target rewrite, or knowledge mutation is accepted. Local relocation remains
physical-travel-owned. No astyle 3.1, live GUI, Linux, or Windows claim. Next: 69/70/60 assessment
hysteresis and the normal/burned/inconclusive cooldown boundary table.

Phase-5 assessment-readiness hysteresis is green at `d44b2c20ea`. One shared pure production
contract proves exact normal acquire/retain/release at 70/60/59 and burned acquire/retain/release
at 60/50/49; a real burn lands at certainty 60 and latches. Existing report owners retain exact
48-hour normal, 48-hour burned, and 12-hour inconclusive cooldowns. The redirected Mac tiles
object/test link exits 0; fixed-seed `[scout_assessment]` passes 2/105, `[covert_burn]` 2/649,
`[alternate_watch]` 2/151, and `[structural_bounty]` 51/6,785. `git diff --check` is clean. No
astyle 3.1, live GUI, Linux, Windows, Phase-6 authorization, or not-yet-owned empty/danger outcome
claim. Next: deterministic simultaneous-exit priority and one-transition ingestion.
