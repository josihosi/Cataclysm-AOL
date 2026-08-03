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

Fair terrain discovery and faction fit/scoring are green at `cb53cbafdb`. Final terrain,
structural, scheduler, frontier, full live-world, fairness, overmap-save, and save-size gates pass
5/4,486, 33/5,297, 4/25,610, 5/245, 124/33,705, 1/3,007, 2/24, and 1/10 at seed
`830204929`. The 80,926,936-byte binary SHA-256 is
`2fa89eb409a66b91f49ef1442b5540241ed0cca415e0a05bc85c66aecac29fd3`.
The accepted 100-site/32-hour artifact records 38,911 ns p95, 60,791 ns max, 212,992-byte timing
RSS delta, 66,112 serialized bytes of growth, all 100 camps terrain-scanned with spread one, and
all 100 routine camps serviced within six passes. Exact evidence and review caveats are under
external `phase3-20260803/terrain-fit/MANIFEST.md`. Current validation target is exact dispatch
drive/force-due boundaries plus the real top-two/global-eight route budget; the terrain packet's
zero full-route solves are an intentional next-row boundary, not complete scheduler credit.

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
