# Hostile-camp overmap ecology resume packet

Updated: 2026-08-03

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
- Active phase: Phase 4 - bounded perception, evidence aging, and removal of radar.
- Current deterministic execution row: persist explicit target-lead origin and prevent the legacy
  player-pressure dispatcher from consuming structural/frontier/terrain-owned leads.
- Scope: bandits and cannibals only. Writhing-stalker AI, zombie-rider AI/progression, and flesh-raptor behavior are excluded.
- Non-blocking release-harness gap: the guarded Security.framework write returned `OSStatus -25308` (`interaction not allowed`). The existing shell export remains intact; make no more Keychain attempts while Josef is unavailable.
- Current engineering state: Phases 0-2 are complete. Phase 3 has shared routine parity, exact
  persistent pairs, bounded shared routes, camp-local frontier memory through `0576113190`, and
  the persisted global fairness envelope through `83c40e3bc3`. Schemas 5/12, save/replay,
  eligible-only 16-camp rotation, two-start allocation, 100/500 fairness, saturated wait ordering,
  and no-candidate backoff are green. Fair terrain discovery, exact faction fit, physical terrain
  checks, and bounded score diagnostics are checkpointed at `cb53cbafdb`. Exact dispatch drive,
  force-due, acquire/retain/risk gates, camp ordering, and the top-two/global-eight route consumer
  are checkpointed at `cab98bc55c`.
- Exact-pair handoff, dematerialization, cohesion, finite bounty, and bounded abstract threat are
  checkpointed through `d5e76a447f`; the nested current-schema harness audit is checkpointed at
  `beafbbbd86`, and the fixture correction at `7f4cac4ae0`. The two-attempt live packet is honest
  non-credit evidence under external `phase3-20260803/pair-handoff/MANIFEST.md`; its full log proves
  a legacy player-pressure dispatcher/structural mission-slot collision plus spawn-tile fixture
  bootstrap drift. Do not start a third fixture loop or weaken the startup classifier. Phase 4
  opens with the concrete lead-origin/single-writer repair.
- Scheduler evidence is archived at
  `/Users/josefhorvath/codexbulk/C-AOL-hostile-ecology-artifacts/phase3-20260803/global-scheduler/MANIFEST.md`.
  The final test binary is 80,790,008 bytes at SHA-256
  `bba9f8d75c0212f410c44f25209356f07d46c78ab61b7240d24fb0b35425f41c`; final structured
  review is clean at 0.93. Its 100-site benchmark proves all dispatch-eligible camps within six
  passes. Terrain evidence is archived at external
  `phase3-20260803/terrain-fit/MANIFEST.md`: the 80,926,936-byte binary SHA-256 is
  `2fa89eb409a66b91f49ef1442b5540241ed0cca415e0a05bc85c66aecac29fd3`, the accepted bounded
  packet reaches terrain and dispatch service 100/100. Routed-dispatch evidence is archived at
  external `phase3-20260803/routed-dispatch/MANIFEST.md`; the focused 16-camp fixture proves eight
  route callbacks and two starts, and the bounded 100-site child remains within the Phase-0
  provisional budgets.
  Final clean-environment secure-store/API qualification is deferred to the later release gate and
  may not pause deterministic camp-AI work.

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
- Bounded-supply checkpoint `37498066ba` adds one capped member-day stock with O(1) minute catch-up,
  sub-day remainder, immediate roster-shrink cap, exact casualty-time reconciliation, and
  seven-member-day legacy/new-camp seed. The final redirected build exits `0`; supply-focused tests
  pass 2/77, live-world passes 91/4,848, save compatibility passes 2/24, and saturated JSON is
  29,730 bytes. Binary SHA-256 is
  `6ee20c0fd4c8472e91323713d8a3b640e1742d9fd553435d654ba7808768831b`.
- Private-estimate checkpoint `1aa9851902` reuses each camp's persisted structural bounty,
  confidence, and observation timestamp as its belief authority. Global claims mutate no camp;
  a strict physical update mutates only the reporting camp and rejects stale/invalid estimates
  byte-identically. The redirected build exits `0`; private knowledge passes 1/33, all resource
  tests 4/2,090, and live-world 92/4,881. Binary SHA-256 is
  `bd754c017c410066c0ded6b8dc0e3886faaa031afffd648c0af0876e02b9b3c9`.
- Reference-pruning checkpoint `ddd1afe480` adds stable dossier IDs/revisions to scout, report,
  decision, hostile-operation, and structural-plan owners; canonicalizes duplicate/oversized saves
  to 64 dossiers and eight recent marks; preserves positive legacy revisions; keeps identical
  evidence byte-stable; and rejects stale or terminal-revision mutations before state changes.
  Final build `build_logs/macos-tests-build-phase1-reference-pruning-final.log` exits `0`.
  Intelligence is 3/48, live-world 95/4,931, handoff 9/203, save compatibility 2/24, and the
  64-dossier/256-route/16-observation saturated state is byte-stable at 48,070 bytes. Binary is
  79,898,584 bytes, SHA-256
  `f435a54a682e7bfc061e7973e271cecd47997c15cc690cbfd55ab1df869214d7`.
- Semantic-observation checkpoint `9be3e8c044` canonicalizes the 16-fact working set, protects
  safety evidence, and advances progress only for retained semantic changes under the shared
  simulation cursor. Its final build and observation/scout/live-world/handoff/save gates are green;
  artifacts are under `phase1-20260802/observation-progress`.
- Report-policy checkpoint `258247d26c` persists `bandit_shakedown` or
  `cannibal_night_raid`, keeps at most 64 canonical acted watermarks by target ID/OMT/policy,
  permits independent target/policy progress, and rejects stale/exact same-key tuples, profile
  drift, explicit unknown fields, and revision exhaustion without mutation. Final build exits `0`;
  policy passes 3/42, live-world 99/5,046, handoff 9/203, and save compatibility 2/24 at seed
  `830204929`. Binary SHA-256 is
  `12030ac296c498bc03b87f27949107039a036c4331e79a261114c9d4646e5e87`;
  manifest SHA-256
  `fe377b62d12766fae0fca3fa07c03278d33f5d5963b9eb83fff4645e2b6304b0` is under
  `phase1-20260802/report-policy`.
- Transition checkpoint `16649b77b0` adds bounded, opt-in, non-persisted committed phase events.
  Its final build and transition/live-world/save filters exit `0`; exact evidence is under
  `phase1-20260802/transition-events`.
- All-active-phase checkpoint `e408c9c450` round-trips a real hostile operation through every phase
  plus `lost` without duplicate state or synthetic events. The exact case passes 1/833 and full
  live-world 103/6,011; exact evidence is under `phase1-20260802/all-phase-roundtrip`.
- Phase-2 roster-authority checkpoint `563499e3fe` replaces overloaded headcount with strict
  `living_total` plus a derived physical/away/reserved/ready view, validates current schema
  transactionally, repairs bounded legacy state, preserves ownership when a two-person camp is
  empty because both members are away, and keeps abstract spawn-tile authority byte-stable through
  materialization and reload. The final build and roster/live-world/save/save-size/handoff filters
  exit `0`; exact evidence is under `phase2-20260803/roster-authority/MANIFEST.md`.
- Phase-2 routine-pair checkpoint `c846be1632` makes every camp-backed routine outing exactly two
  or none, materializes only the pair plus one concrete reserve, preserves micro-site singleton and
  threat-derived response policy, generalizes return timing, and rejects stale overwhelming danger
  plus undersized combat requests. Final build and routine/live-world/handoff/playback/save filters
  exit `0`; exact evidence is under `phase2-20260803/routine-pair/MANIFEST.md`.
- Phase-2 fresh-response checkpoint `5fbefa452e` removes caller-supplied hostile member IDs,
  recomputes pinned threat/reward sizing from the ready post-report roster, and rejects stale lead
  or readiness drift atomically. Final build and live-world/handoff/playback/save filters exit `0`;
  exact evidence is under `phase2-20260803/fresh-response/MANIFEST.md`.
- Phase-2 capability-pair checkpoint `f049104375` selects a stable strongest observer and lightest
  return-safe escort, refreshes live readiness, rejects role drift atomically, and repairs legacy
  empty member templates. Final authoritative Mac builds and routine/migration/save/multi-site/full
  live-world filters exit `0`; exact evidence is under `phase2-20260803/capability-pair/MANIFEST.md`.
- Phase-2 reservation checkpoint `f65e6bd28a` pins structural activity ID/generation and the camp
  mission slot, rejects stale/competing plans byte-identically, and round-trips the exact member
  owner envelope. Build and focused/structural/full live-world filters exit `0`; structured review
  is clean; exact evidence is under `phase2-20260803/reservation-ownership/MANIFEST.md`.
- Phase-2 matching-release checkpoint `61017301a4` rejects stale cleanup byte-identically, including
  a newer same-ID generation, releases only unresolved members on a candidate copy, preserves
  casualties, and returns the exact committed count. Build and focused/structural/full live-world
  filters exit `0`; final review is clean; exact evidence is under
  `phase2-20260803/matching-release/MANIFEST.md`.
- Phase-3 frontier checkpoint `0576113190` persists one cursor plus eight last-resolved timestamps,
  plans deterministic four-waypoint radius-4/radius-9/home routes at cost 18, resolves memory only
  after physical home return, skips dangerous sectors without starving the other seven, and keeps
  synthetic probes out of generic targeting. The final build exits `0`; frontier/structural/
  live-world/handoff/overmap-save/save-size gates pass 4/172, 28/766, 116/7,800, 10/275, 2/24,
  and 1/10. Final structured review is clean; exact evidence is under
  `phase3-20260803/frontier-sectors/MANIFEST.md`.

## Resume procedure

1. Confirm `git status --short`, `git log -1 --format=%H`, and `git worktree list` before editing.
2. Read `Plan.md`, `SUCCESS.md`, `TODO.md`, `TESTING.md`, and the canonical implementation ledger.
3. Resume Phase 3 with exact drive/force-due thresholds and bounded route consumption. Do not reopen
   Phase-0 statistics unless a later real implementation measurement approaches or exceeds a
   ratified budget.
4. Do not retry Keychain or send another blocker message during this resume. Retain the shell export and leave the later release-harness secure-store/API row unchecked.
5. Reuse the current `cb53cbafdb`-source test binary where valid; run one redirected build at a time
   after implementation invalidates it.
6. Phase 1 is closed; keep its transition and all-phase manifests as the persistence baseline.

Build state at this checkpoint: no build, test, review, benchmark, or profile is running. The final
terrain build and focused/full/save tests completed with exit `0`; the accidental direct
`tests/Makefile` invocation is recorded as non-credit harness misuse in the artifact manifest.
The baseline and production candidate remain untouched. Do not start another build until routed
dispatch source changes invalidate the current test binary.

Keychain/TCC/password interaction is not a whole-goal blocker for this deterministic package. A
future Apple prompt may pause only the later release-harness action that requires it; ordinary
technical failures and deferred API qualification must not stop the active ecology roadmap.
