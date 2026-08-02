# Hostile-camp overmap ecology resume packet

Updated: 2026-08-02

## Repository state

- Active worktree: `/Users/josefhorvath/Schanigarten/Cataclysm-AOL-hostile-ecology-dev`
- Active branch: `dev`
- Phase-0 instrumentation commit: `fee1e44d38b6fc69846b3931a947c00ba72ec3a8`
- Preserved baseline instrumentation commit: `2a3e7efb17919a26347aae238083fcf23d6be6e1`
- Launch base: `660057ff728bdf77531f607b1bd42a175f027a5f`
- Untouched release/playtest worktree: `/Users/josefhorvath/Schanigarten/Cataclysm-AOL`
- Untouched release/playtest branch: `port/cdda-master` at exact launch base
- Prior branch tips: `backup/dev-pre-hostile-ecology-20260802` and `backup/master-pre-hostile-ecology-20260802`
- The checkpoint containing this file is the authoritative resume commit; resolve it with `git log -1 --format=%H` after checkout.

No push, publication, tag, release, upstream merge, Windows mutation, or production-candidate edit is authorized.

## Active execution state

- Goal: complete the engineering success state in `doc/hostile-camp-overmap-ecology-implementation-ledger-v0.md`.
- Active phase: Phase 1 - one authoritative persistent model.
- First unchecked deterministic execution row: add the bounded generic camp supply stock with
  deterministic catch-up, cap enforcement, and legacy seeding.
- Scope: bandits and cannibals only. Writhing-stalker AI, zombie-rider AI/progression, and flesh-raptor behavior are excluded.
- Non-blocking release-harness gap: the guarded Security.framework write returned `OSStatus -25308` (`interaction not allowed`). The existing shell export remains intact; make no more Keychain attempts while Josef is unavailable.
- Current engineering state: the path classifier and writer defects are repaired and pass 60 contract tests. Final clean-environment secure-store/API qualification is deferred to the later release gate and may not pause deterministic camp-AI work.

## Launch evidence

- The clean production checkout and `origin/port/cdda-master` both resolved to `660057ff728bdf77531f607b1bd42a175f027a5f`.
- No relevant transfer, build, port-orchestrator, or separate implementation process was active.
- Launch configuration reported GPT-5.6-sol, xhigh reasoning, approval `never`, and unrestricted sandboxing. Persistent service tier was normalized to default.
- `/usr/bin/python3 -m unittest tools.openclaw_harness.test_fixture_contract` ran 57 tests and reproduced one failure plus one error: a mocked Windows `WindowsPath` construction on macOS and mocked Linux acceptance of the real Mac API venv.
- A release-candidate dry run with both API-key variables removed exited successfully and selected the Mac API venv; it did not make an API call.
- A read-only upstream merge-tree audit was saved at `/tmp/caol-hostile-camp-upstream-merge-tree-20260802.txt`; upstream remains unmerged.
- The repaired harness passes 60/60 fixture-contract tests plus `py_compile` and `git diff --check`.
- One guarded real Keychain write was attempted through Security.framework. It emitted no secret,
  returned `OSStatus -25308`, and wrote no item. Do not retry until Josef explicitly confirms the
  Mac login Keychain is unlocked/approved from a native local session.
- Phase-0 identity is complete. The machine-readable manifest is
  `/Users/josefhorvath/codexbulk/C-AOL-hostile-ecology-artifacts/phase0-20260802/environment-fixture-manifest-54d2c76c0b.json`.
  The completed top-level test build log is `macos-tests-build-54d2c76c0b-gettext-path.log` in the
  same artifact root and contains `CAOL_BUILD_EXIT=0`. The arm64 test binary SHA-256 is
  `4491718735452fa868644d9609f4fcfeffb13fb300b118378f2742c587699525`.
- The pre-change functional packet is archived at
  `/Users/josefhorvath/codexbulk/C-AOL-hostile-ecology-artifacts/phase0-20260802/functional-baseline-manifest-ffbf32166c.json`.
  Existing live-world, handoff, playback, and harness suites are green; exact-player radar and
  independent per-member routing are source-proven caveats, while visible dancing and covert
  hostility were not reproduced.
- Natural camp qualification is checkpointed at `dc094e8bf1b3d50a603ba1fdcdcb5ccfb997f66c`.
  Fixed internal seed `830204914` proves fresh default-world bandit/cannibal placement, canonical
  registration, real 14-member mapgen reconciliation, idempotence, and JSON round trip. Final
  autoreview is clean. The current source-built test binary is 79,081,592 bytes with SHA-256
  `c424ea8e251f4319123f90fe1264cfdfdfd268ac058f914bf01f6487944f70c8`.
- Benchmark instrumentation is checkpointed at
  `22ca8759f239c3196a158c026cb64f6aeca2ae80` on `dev` and identical-patch cherry-pick
  `b7e9a6a1f6138e3b2546157b9aa97887172e8bbd` on the preserved baseline; stable patch ID is
  `de11c834a4e0075a8695a8b7b4d5bdca698cfa48`. Exact sequential builds with the same command/flags
  exited `0`. Dev binary: 79,150,952 bytes, SHA-256
  `858ddd88ec9c8cf77639392620a136cc112caa5e34f46c39aea0cb1c828918e0`; baseline binary:
  79,112,952 bytes, SHA-256
  `eb14211166ca4021c88933f88f3103b00cdd061a1283ee7fc63a058b4b66b146`. Driver tests pass 77;
  histogram test passes 1 case/13 assertions; `[bandit][live_world]` passes 68 cases/1,459
  assertions; final xhigh autoreview is clean.
- The first committed one-pair A/B smoke is deliberately red/non-credit. Runner and independent
  validator both exited `2`; raw SHA-256 is
  `0c5d1431c120ae0f8913e98543da64c7cf1bd968915ef1dc6320fca3e396249c`. Test startup generated
  3,418 ignored `data/cache` FlatBuffers in the cold baseline after identity capture. Both
  worktrees remained Git-clean. It remains red/non-credit history; the following checkpoint
  repaired the contract with fresh cold roots.
- Fixture/input hardening is checkpointed at `c2d7921d9f` on `dev` and identical-patch
  `7e6d11091d` on the preserved baseline; stable patch ID is
  `c8b72321516ccf34ce160121d4da4ab2d44aee42`. The Python runner suite passes 94 tests and the
  direct lead-saturation, histogram, and full live-world C++ gates pass. Final xhigh closeout
  review is clean. Same-command sequential builds exited `0`; dev binary is 79,151,528 bytes,
  SHA-256 `d113a5480473f6e70f637aab2f030ba38bb4cf346fd0906ec8c766ad4051fa61`, and baseline binary is
  79,113,576 bytes, SHA-256
  `708cfeb2fc763f9083809dd182f1f480d5677f02746c826acb6bd13798017f88`.
- The first accepted cold-cache paired integration smoke completed in 49 seconds with two valid
  measured runs. Raw SHA-256 is
  `62b57c1e88778576e1c1f248c637f5427543152268cdfdc923e0dc77c985c444`; independent
  `validate --verify-files` exited `0`. Both variants began with the identical 8,156-file source
  manifest and recorded their 11,574-file warmed trees before measurement. Fixed seed/calendar,
  fixture hash, replay reset, and equivalent terminal state all validate. The accepted summary
  SHA-256 is `de65f54b01e9d6593f392ce17f2271f8f8e1ce9a56c5420f73ad46fe493aa18d`.
- Phase 0 is complete under Josef's pragmatic engineering-baseline decision. Final instrumentation
  is `fee1e44d38` on `dev` and identical-patch `2a3e7efb17` on the baseline, stable patch ID
  `bf8a5649...`. Exact builds exited `0` in 41/36 seconds; binaries are SHA-256 `6aada731...` and
  `75854084...`. Exact `dev` passes 110 Python tests, 3 C++ cases/1,775 assertions, `py_compile`,
  validation, and `git diff --check`.
- The official cold-cache matrix completed in 2,279 seconds with 25 cases, three pairs, 150 valid
  runs, and zero failures. Raw SHA-256 is `7332059a...`; independently validated summary SHA-256
  is `9736b3af...`; external manifest SHA-256 is `ff410e9b...` at
  `/Users/josefhorvath/codexbulk/C-AOL-hostile-ecology-artifacts/phase0-20260802/phase0-pragmatic-evidence-manifest-fee1e44d38.json`.
  Provisional CPU/memory/size/save budgets are ratified in the canonical ledger. The known legacy
  500-site result starves 125/250 eligible camps after 250 updates; Phase 3 must reach zero starved
  and <=32 hourly waits.
- Phase-1 audit/atomicity checkpoint `673a900067` fixes prevalidation-before-mutation for return
  packets and transactionally parses the world save before replacing live state. Model checkpoint
  `4995a3c64e` adds schema-v2 active-outing identity, per-camp generation and return watermark,
  owner/epoch/last-advance/key persistence, legacy active-group migration/repair, and stale replay
  rejection. Its redirected Mac build exits `0`; focused evidence passes 72 live-world cases,
  6 handoff cases, the patrol/shakedown consumer, and both overmap-global save cases.
- Phase-1 scout checkpoint `e4b75e15a3` makes the nested outing the sole runtime scout owner and
  persists bounded members/leader, route, target revision, phases, observations, cargo,
  casualties, clocks, owner/handoff state, and independent return/report/cargo receipts. Strict
  Mac build and focused tests pass: live-world 78 cases/1,965 assertions, handoff 8/145, patrol
  1/12, save compatibility 2/16. Empty/normal/saturated JSON is 87/4,139/28,115 bytes and the
  saturated form is byte-stable after reload below 64 KiB. Final AutoReview's six concrete
  persistence findings were accepted and repaired in the checkpoint.
- Phase-1 split-return checkpoint `42e5bad3cd` persists per-member resolutions, accepts a
  first-survivor provisional report/cargo receipt without freeing the mission slot, and finalizes
  on later on-time return or fixed-grace terminal loss without duplicate credit. Its strict Mac
  build exits `0`; live-world passes 80/2,085, handoff 8/145, patrol 1/12, save compatibility 2/16,
  and empty/normal/saturated JSON is 87/4,190/28,166 bytes. Physical return after a member was
  already declared missing remains a later outcome-receipt concern and is not claimed here.
- Deadline checkpoint `31354b71c3` atomically rejects premature missing declarations and accepts
  them only at or after the persisted fixed-grace deadline.
- Phase-transition checkpoint `7acc011951` centralizes expected-phase/time validation, makes
  burned/exposed/report/home/lost progression one-way, skips the target gate during homeward-only
  phases, safely loads unknown future phases as `lost`, and preserves legacy scavenge return
  compatibility. The final strict build exits `0`; exact phase coverage is 1/144 and live-world is
  81/2,230. Binary SHA-256 is `cae011df1eea90a9a28c4375699cc325fea60365b03ee2bfe7bc014cad4a8a20`.
- Camp-decision checkpoint `687d7bcecb` separates final-report assessment from scouts and pins the
  report revision/generation/identity through awaiting, preparing, cooldown, abandoned, and idle
  watermark states. Provisional, scavenge, all-loss, stale-plan, replay, and malformed-save controls
  pass. Live-world is 82/2,318, handoff 8/148, and save compatibility 2/16; binary SHA-256 is
  `7bbd3f0a24a5cdc0f012bdf27b6dd9660d25bc3ae560fa06c3f79e501645c38c`.
- Hostile-operation checkpoint `67cd68e416` persists fresh shakedown/raid identity, report and
  route/rally pins, reservations, canonical receipt keys, one-way phases, safe migration, and
  consistency repair.
- Shared simulation-cursor checkpoint `833599e5e4` requires one serialized activity/generation/
  owner/epoch/time cursor for scout and hostile mutations, transfers ownership by exact atomic
  compare-and-swap, rejects duplicate/stale advances, repairs legacy parity, and fails closed on
  ambiguous current-schema ownership. The redirected build exits `0`; live-world is 86/2,714,
  handoff 9/202, hostile-operation 3/243, save compatibility 2/16, and final exact-source
  autoreview is clean at 0.99. Binary SHA-256 is `d6e8a9f0fe1570437cfbabda375aa10cdb0b6452bf45416fe387dca8db0bef26`.
- Finite-resource checkpoint `432c0f9da7` makes touched OMTs the sole compact depletion authority,
  applies claims by exact monotonic revision, keeps depleted tombstones, and migrates only truly
  harvested legacy leads. Its redirected build exits `0`; resource-focused tests pass 3/2,057,
  live-world passes 89/4,771, and save compatibility passes 2/20. The measured resource save slope
  is 29 bytes per harvested OMT. AutoReview found one schema-3 hybrid resurrection defect; the
  accepted fix rejects that malformed packet transactionally and the one permitted rerun is clean
  at 0.97. Binary SHA-256 is `8129cf98478e32fe0fc82477f5dae07b882033f812bebbd6031be36de7df99ff`.

## Resume procedure

1. Confirm `git status --short`, `git log -1 --format=%H`, and `git worktree list` before editing.
2. Read `Plan.md`, `SUCCESS.md`, `TODO.md`, `TESTING.md`, and the canonical implementation ledger.
3. Resume bounded camp supply on top of `432c0f9da7`. Do not reopen Phase-0
   statistics unless a later real implementation measurement approaches or exceeds a ratified budget.
4. Do not retry Keychain or send another blocker message during this resume. Retain the shell export and leave the later release-harness secure-store/API row unchecked.
5. Reuse the current `432c0f9da7`-source test binary where valid; run one redirected build at a
   time after implementation invalidates it.
6. Complete operation, resource, and dossier owners with legacy/missing-field,
   phase round-trip, malformed-packet atomicity, replay, pruning, and serialized-size evidence.

Build state at this checkpoint: no build, test, review, benchmark, or profile is running. The
Phase-1 simulation-cursor build and focused tests completed with explicit exit `0`; the abandoned early log
remains classified incomplete. The baseline and production candidate remain untouched. Do not
start another build until the next Phase-1 source change invalidates the current test binary.

Keychain/TCC/password interaction is not a whole-goal blocker for this deterministic package. A
future Apple prompt may pause only the later release-harness action that requires it; ordinary
technical failures and deferred API qualification must not stop the active ecology roadmap.
