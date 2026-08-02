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

Current Phase-0 evidence (2026-08-02):
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
  Git-clean. The next instrumentation slice must perform and record a sacrificial data-load
  warmup before identity capture, hash the complete warmed tree including cache, and separately
  prove invariant non-cache source data; an incidental now-warm rerun receives no credit.

Required Phase-0 evidence:
- exact compiler, build flags, Mac/OS identity, source commit, season length, deterministic world fixture/seed, and test-binary identity;
- all fixture-contract tests green on macOS, including mocked Windows/Linux foreign-path controls;
- current functional behavior and natural camp registration recorded before gameplay edits;
- behavior-neutral deterministic counters/driver run against preserved pre-change and final implementations;
- zero/10/100/500-site CPU, memory, scheduler-fairness, and save-growth evidence with reviewed, ratified budgets and raw artifacts retained outside Git;
- checkpoint commits and resume packets that name exact commands, artifacts, dirty state, blockers, and next row.

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

The active missing evidence is Phase 0 only:
- land behavior-neutral benchmark/counter instrumentation on `dev` and the preserved pre-change comparison worktree;
- record the zero/1/10/50/100-site normal matrix plus the ledger's 500-site fairness stress packet;
- collect CPU, memory, fairness, serialization, whole-save, and save-growth evidence and ratify budgets.

The foreign-platform classifier and native writer contract are repaired at `d12edba150` with 60/60
tests. Clean-environment Mac secure-store/API proof remains a later release-harness gate; it must
not trigger another pause, retry, or Discord blocker during deterministic ecology work.

Origin, release, and production-candidate mutation remain held. Closed zombie-rider,
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
