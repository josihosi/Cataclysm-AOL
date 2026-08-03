# C-AOL Hostile Camp Overmap Ecology and Stalking

## Durable implementation ledger

Status: **ACTIVE - Phase 1 authoritative persistent model**

Active phase: **Phase 1**

First deterministic execution row: **Give every return/report/resource/cargo/member component a stable idempotency key.**

Latest resume packet (behavior checkpoint `258247d26c`, 2026-08-02): `dev` on the isolated Mac
worktree; production `port/cdda-master` remains `660057ff728bdf77531f607b1bd42a175f027a5f` and
untouched. Phase 1 is active at component idempotency. Final reports now persist bandit-shakedown
or cannibal-night-raid policy and a canonical 64-entry acted watermark keyed by target ID, OMT,
and policy. Same-key stale/exact tuples cannot reopen assessment; different target/policy keys
advance independently; policy drift, explicit unknown fields, and revision overflow fail closed.
The final redirected Mac build exits `0`; policy tests pass 3/42, full live-world 99/5,046,
handoff 9/203, and save compatibility 2/24. Test binary SHA-256 is
`12030ac296c498bc03b87f27949107039a036c4331e79a261114c9d4646e5e87`
(80,057,064 bytes). Manifest SHA-256 is
`fe377b62d12766fae0fca3fa07c03278d33f5d5963b9eb83fff4645e2b6304b0` under
`phase1-20260802/report-policy`; no current blocker.

Production target: `port/cdda-master`

This ledger is the cross-off contract for the **bandit/cannibal** overmap-AI work. When work is authorized and the canonical checkout is synchronized, the first documentation checkpoint must copy this ledger into a tracked subordinate design/implementation document and make `Plan.md`/`SUCCESS.md` point to it. The tracked copy then becomes canonical across machines; this CodexBulk copy remains an immutable launch snapshot. `Plan.md` remains the roadmap authority.

When work is authorized, the primary Mac Codex session owns the canonical tracked ledger and may check an item only after inspecting the implementation and its evidence. Contract checkboxes describe acceptance criteria; they are not the execution queue. Execution starts at Phase 0 and proceeds from the first unchecked row inside the active phase.

Do not interpret a checked coding task as a completed phase. Each phase has an explicit exit gate.

## Desired player experience

A quiet evac shelter should not be found because hostile camps know the avatar's coordinates. Bandit and cannibal camps should send believable two-person scout parties into the world, learn only what those parties can perceive, return information home, and then make a faction-specific decision. A discovered scout party should withdraw coherently instead of dancing outside a window. Bandits should later attempt a robbery; cannibals should wait for darkness and attempt a lethal raid when their camp can afford it.

The system is not done if any of these remain true:

- hostile camps retarget the player's current OMT without evidence;
- routine scouts are singletons;
- party members take unrelated overmap routes or one member triggers contact alone;
- scouts appear somewhere they could not have physically reached;
- covert scouts are displayed as openly hostile or emit `gets angry` before committing;
- being burned causes tile oscillation instead of a persistent exit plan;
- intelligence reaches home without a returning survivor;
- two camps can consume the same finite ground bounty;
- old reports immediately retrigger pressure after a payment or failed raid;
- abstract and local simulation both advance the same party;
- CPU cost, memory use, or save size grows without a proven bound.

## Scope boundary and release prerequisites

This package changes bandit and cannibal camps only. Writhing-stalker AI and zombie-rider AI/progression are a separate design discussion and require their own promoted ledger before code changes. They may later reuse a generic, reviewed abstract/local egress primitive; they must not inherit camp dossiers, physical reports, bounty, or faction operation state by accident. Flesh-raptor behavior is not reopened.

The ordinary-play release fixture may still stage a flesh raptor, zombie rider, writhing stalker, bandit camp, and cannibal camp so Josef can notice all candidate content while roaming. That fixture is observation footing, not permission to redesign the three non-camp creature families and not proof of natural world generation.

The natural-generation audit below is required before Phase 1. Roaming-save, excluded-family, and
release-harness rows are Phase-10 qualification or held context; they do not block deterministic
model work.

- [x] Prove natural overmap-special creation/registration for both bandit and cannibal camps, including a new world with no fixture injection. If one faction has no natural camp source, add that as an explicit prerequisite rather than claiming its scout ecology is natural.
- [ ] Prove the fresh release fixture starts at 10:00 inside the completed evac shelter with no hostile in the initial reality bubble or at the windows, no stale active scout/operation, bandits about 10 OMT south, cannibals about 10-12 OMT east-northeast, flesh raptor about 4 OMT west, zombie rider about 5 OMT north, and one early writhing-stalker observation row about 3 diagonal OMT southeast.
- [ ] Preserve the source fixture as immutable; every handoff installs a fresh derived copy and sends no gameplay input after the normal map appears.
- [ ] Prove both intended camp NPCs are genuinely assigned through game-owned camp state and receive their normal game schedule after load. Do not hand-author a patrol schedule. If passive schedule advancement still leaves a broken assignment, repair that regression separately before using the save as camp-strength evidence.
- [ ] Treat loot autosort as closed/no-change: its empty destination zones explained the earlier `all items have been sorted` result. Do not reopen sorting without a new reproduction containing non-empty matching destination zones.
- [ ] Validate flesh-raptor availability on the exact candidate without changing its closed behavior packet.
- [ ] Move zombie-rider age/evolution timing and existing-versus-new-area behavior into the separate rider discussion. Any later age rule must use the configured game calendar/evolution machinery and comparison monsters such as zombie brutes, never a hard-coded assumption that two game years equal 730 days.
- [ ] Keep writhing-stalker early-game frequency/strength and burned-OMT evacuation in the separate stalker discussion; this ledger only records that the release observation fixture can place one outside the start bubble.
- [ ] The release harness selects `UltimateCataclysm` (Ultica), provisions `CATA_API_KEY` automatically from `OPENAI_API_KEY` or the platform secure store, never logs the secret, and selects a platform-valid API-runner Python path. Prove the API runner self-test and child-process inheritance on Windows and Mac before their respective harness runs.
- [ ] Record the current upstream assessment: candidate contains upstream through `8d4959bee4`; fetched `7cf1d08ae8` is 104 commits newer; the dry merge has only `src/npcmove.cpp` and Bombastic Perks `closetland.json` conflicts; no reviewed change is release-blocking. Refresh this read-only assessment before implementation, but do not merge upstream as part of this package.

## Non-negotiable behavior contract

The party-size, honest-knowledge, burned-information, faction-outcome, performance-proof, and scope decisions come from Josef. Unless the decision log says otherwise, numerical weights, TTLs, power margins, caps, and cooldowns below are **provisional v1 engineering defaults** to ratify in Phase 0 against live code/baselines. The implementation may change one only through an explicit ledger decision with before/after evidence; it may not silently drift.

### Routine party size

- [x] Living camp size 0-1 dispatches no routine scouting party.
- [x] Living camp size 2 dispatches both members when it scouts; the camp is genuinely empty while they are away.
- [x] Living camp size 3-4 normally dispatches two and retains the remainder.
- [x] Living camp size 5+ dispatches two in v1.
- [x] A three-person routine party is deferred policy work, not a v1 fallback. If later promoted, it requires a named logistical reason and at least two ready members remaining at home.
- [ ] Unknown or overwhelming danger causes delay, reroute, or abort; it never increases the v1 routine party above two.
- [x] A camp with fewer than two ready/capable members waits instead of sending a singleton.
- [x] The zero-home-reserve exception for a two-person camp applies only to routine scouting, never to a shakedown or lethal raid.
- [x] Small roadside/micro-site encounters may retain separate one-off policies; the two-person minimum applies to camp-backed routine scouting.

### Shared machinery, faction-specific outcome

- [ ] Bandit and cannibal camps share the same bounty, route, perception, memory, scouting, stalking, withdrawal, report, and assessment machinery.
- [ ] Bandits value renewable stored goods, negotiate a shakedown, and become openly hostile only at commitment/refusal/attack.
- [ ] Cannibals value people, do not offer a payment interaction, wait for real darkness, and attack all loaded camp defenders rather than only the avatar.
- [ ] Autonomous inter-camp wars are not part of the first production slice.
- [ ] Another active hostile party is perceived as danger through ordinary evidence, not resolved through magical cross-camp coordination.

### Knowledge and causality

- [ ] World truth is never exposed through a camp confidence field or direct `player_character` lookup.
- [ ] Every learned fact has source, observer, observation location, receiver location, timestamp, strength, uncertainty, and expiry/retention policy.
- [ ] A camp dossier is a timestamped belief snapshot, not a live pointer to its target.
- [ ] Mobile threats become uncertain with age; permanent terrain knowledge does not vanish like a sound.
- [ ] Smoke, light, significant sound, actual proximity, and bounded OMT visibility can create leads.
- [ ] Quiet targets outside legitimate observation remain unknown even if the global scheduler knows they exist.
- [ ] A report changes camp knowledge only after a survivor physically returns.
- [ ] If all scouts die, the camp learns only that the party is overdue/missing after a grace period.

## Execution model

### Primary session

- Root coordinator: **GPT-5.6-sol, xhigh reasoning, normal/default service speed**.
- One manually started long-running goal session owns architecture, integration, ledger truth, commits, and phase gates.
- The root session must independently re-audit live code and current Git state before editing. This ledger is a contract and starting hypothesis, not permission to ignore drift.
- The preparation run has Josef's limited authorization to push the exact reviewed `port/cdda-master` synchronization commit so Windows and Mac can converge. That does not authorize a release, tag, PR, upstream merge, force-push, branch deletion, or later implementation-branch publication. The future implementation run must re-establish its Git mutation boundary before any push.
- `port/cdda-master` is the production/playtest target, not the default editing branch. Follow the repo branch contract: implement and checkpoint on `dev`; when an authorized development cycle is green, merge `dev` into `master`; then audit/dry-run and update `port/cdda-master` through `tools/porting/orchestrate_ports.ps1` from `master`. Never casually edit or merge `dev`/`master` over the current production candidate.
- The root owns phase selection, architecture decisions, persistent-schema/version changes, integration diffs, test interpretation, performance-budget decisions, checkpoint commits, ledger truth, and the final engineering-complete claim. Subagent output is evidence to inspect, not authority to cross off a row.
- The root must preserve a resumable state packet after every checkpoint: branch/HEAD, active phase and first unchecked execution row, dirty paths, running processes, last successful commands/artifacts, open blocker, and next permitted action.

### Prompt/task contract

This contract applies the official [GPT-5.6 prompting best practices](https://developers.openai.com/api/docs/guides/latest-model?model=gpt-5.6#prompting-best-practices), reviewed during preparation on 2026-08-01: keep prompts lean and outcome-focused, state each constraint once, pass only relevant context, use direct semantic tools for judgment-heavy work, and bound programmatic orchestration explicitly. Every delegated work item uses this envelope:

```text
Goal: one observable outcome.
Context: exact branch/commit, active phase, relevant live symbols and prior evidence.
Constraints: scope/non-goals, ownership boundary, permission boundary, platform/style rules.
Evidence: exact commands/artifacts the worker must inspect or produce.
Success: objective checks that permit the item to be accepted.
Output: result schema below; no unrelated edits.
```

Every worker returns:

```text
status: complete | partial | blocked
scope: inspected and changed paths/symbols
result: behavior/state transition implemented or finding reached
evidence: exact commands, exit codes, counts, timings, artifact paths
git: branch, before/after HEAD, dirty paths, commit if any
risks: regressions, migration/performance caveats, unproven claims
next: smallest permitted follow-up and ledger row
```

Use direct tool calls for semantic code judgment, approval-sensitive actions, integration, and final validation. Programmatic/parallel tool orchestration is allowed only for bounded predictable reads/tests with explicit concurrency, retry, and stop rules. Do not let accumulated transcripts replace the ledger/resume packet.

### Subagent routing

- GPT-5.6-sol xhigh: persistence/migration, abstract-to-local identity, movement ownership, covert hostility integration, assessment/escalation, cross-cutting review.
- GPT-5.6-sol medium: bounded code archaeology, table tests, harness scenarios, benchmark fixtures, serialization-size tooling, documentation alignment, isolated compile-error fixes.
- Medium agents may propose architecture but may not silently decide a cross-cutting contract.
- At most two subagents should edit concurrently, and only in demonstrably non-overlapping areas/worktrees.
- Only one expensive build/profile run at a time. Do not create another `cc1plus` storm to gain a few minutes.
- All Mac implementation sessions inherit the already configured `approval_policy="never"` and `sandbox_mode="danger-full-access"`; do not launch a child with a weaker profile. Full access does not broaden the authorized product/Git scope.

### Apple/TCC/password blocker protocol

If macOS requires Josef to approve TCC, unlock Keychain, enter a password, or interact with another Apple-owned prompt, pause only the Apple-dependent action. Continue unrelated deterministic work whenever an unblocked roadmap row exists. Mac secure-store/API qualification is a release-harness/Phase-10 gate and may not pause bandit/cannibal engineering.

1. Stop repeated retries for the affected action; preserve work and record the exact prompt/window, command, phase/row, and safe running processes.
2. Send one concise secret-free OpenClaw Discord message only when the Apple action blocks the next active roadmap row. Do not send another message for the already-recorded Keychain denial.
3. If another deterministic row is available, record the Apple-dependent gap and continue the same long-running goal. Do not use cron, a watcher, or repeated Discord messages.
4. The guarded Keychain attempt at `d12edba150` consumes the current retry budget. While Josef is unavailable, retain the existing shell export and make no further write/read/API attempts. Final secure-store qualification remains unchecked until Josef explicitly reopens that later release gate.

The relay test was successfully delivered during preparation. Future messages are blocker-only, not progress spam.

### State-driven loop (no cron)

Repeat until every engineering-success gate is checked:

1. Re-read this ledger and the repo's `AGENTS.md`, `Plan.md`, `SUCCESS.md`, `TODO.md`, `TESTING.md`, and `COMMIT_POLICY.md`.
2. Verify branch, HEAD, dirty state, active processes, and the first unchecked row inside Phase 0 or the currently active phase.
3. Select one small vertical slice and define its exact acceptance evidence.
4. Delegate at most two bounded, non-overlapping subtasks.
5. Root inspects every diff and reconciles it against the behavior contract.
6. Run the narrow functional test and the phase-appropriate performance microbenchmark.
7. Create a checkpoint commit at a real state boundary under `COMMIT_POLICY.md`.
8. Check a row only when its commit, test command/result, and artifact or measurement evidence are recorded in that phase's evidence block.
9. Update this ledger and repo ledgers only when the truth materially changed.
10. Continue to the next first unchecked row inside the active phase.

If the long-running session stops, resume it manually from the ledger. Do not install a polling loop, heartbeat, cron task, or launchd watcher.

## Phase 0 - start gate and reproducible baselines

Preparation receipts are not Phase 0 credit; re-verify every preflight condition at launch.

Phase-0 closure follows Josef's 2026-08-02 pragmatic decision: this packet is a useful regression
baseline, not a publication-grade statistical campaign. Three paired normal runs and the bounded
500-site stress row are sufficient; wide intervals and lower-fidelity phase RSS are retained
honestly. Forward-schema soak, allocation, loaded-pair, and platform proof remain requirements of
their implementing phases and Phases 9-10 rather than reasons to delay Phase 1.

### Repository and process preflight

- [x] Confirm all relevant transfers and their data verification are complete.
- [x] Identify the canonical Mac checkout after the transfer; do not assume the older checkout or bulk copy is canonical.
- [x] Compare exact Mac and Windows `port/cdda-master` commit IDs, and record the current `dev`/`master`/candidate relationship before selecting a worktree.
- [x] Reconcile only through an explicit, reviewed sync direction.
- [x] Confirm `git status --short --branch` is clean on both machines.
- [x] Confirm no unrelated build/Codex process owns the checkout.
- [x] Confirm the dormant implementation goal has not already been started elsewhere; there must be one root owner, not competing xhigh sessions.
- [x] Read all repo instructions and current roadmap ledgers.
- [x] Confirm the active roadmap still authorizes this mechanic. If `Plan.md` names another active target, pause before each implementation phase; do not let this dormant ledger overrule it without Josef's explicit promotion.
- [x] Keep the Windows source checkout and its `.userdata` read-only during Mac implementation unless Josef explicitly authorizes a reviewed sync/validation step; never bulk-copy a Mac tree or save over the Windows playtest tree.
- [x] Create/use an isolated Mac `dev` worktree for implementation. If `dev` is too far behind the current candidate to support the mechanic safely, stop and present the exact commits/API delta and a non-destructive reconciliation proposal; do not solve it by editing `port/cdda-master` directly or resetting branches.
- [x] Record compiler, build flags, Mac model/OS, commit ID, season length, world seed, and test binary identity.
- [x] Keep upstream CDDA unmerged unless Josef separately authorizes an upstream refresh.
- [x] Verify the limited preparation push/sync authorization against the exact commits being moved; release publication and future feature-branch pushes remain outside that authorization.

### Release/harness preflight

The unchecked roaming-save/API-platform rows in this subsection are deferred release-harness
qualification for Phase 10. They are not execution-order blockers for the deterministic Mac-only
Phase-0 benchmark rows below and must not trigger Windows work or another Keychain retry in this
run.

- [x] Run `python -m unittest tools.openclaw_harness.test_fixture_contract` on the synchronized exact HEAD and record the count.
- [ ] Dry-resolve `manual.release_candidate_roaming_mcw` on Windows and Mac; prove the Windows and macOS `LLM_INTENT_PYTHON` values resolve to existing native environments and Linux is not assigned either foreign absolute path.
- [x] Record the bounded Mac secure-store attempt without exposing the credential: writer tests pass, one guarded native write returned `OSStatus -25308`, the existing shell export remains intact, and final clean-environment retrieval/API proof is deferred to the Phase-10 release-harness gate without pausing deterministic work.
- [ ] Prove the derived save's evacuation-shelter footprint/time, two repaired camp assignments, zero old active scouts, five staged observation families, exact distance bands, and no initial-bubble hostile.
- [ ] Advance the save through an ordinary game-owned camp schedule window and inspect whether the two assigned NPCs leave idle bench state when their schedule requires it. If not, split a camp-assignment/patrol repair before relying on them as defenders.

### Baseline functional evidence

- [x] Run the existing narrow `bandit_live_world`, pursuit-handoff, playback, and relevant harness tests before changing code.
- [x] Record existing expected failures/caveats rather than normalizing them away.
- [x] Attempt to capture the known radar/dancing/covert-hostility failure. If current HEAD does not reproduce it, record a code-path/fixture baseline as `not reproduced`; do not resurrect or fake the bug.

### Baseline performance and save evidence

- [x] Land the deterministic benchmark driver and scoped hostile-maintenance timers/counters as an isolated, behavior-neutral instrumentation commit.
- [x] Apply the identical benchmark-only commit to both the pre-change baseline and final comparison worktrees.
- [x] Preserve an exact pre-change baseline binary/buildable worktree at the same compiler and flags as the final comparison.
- [x] Split fixtures into (a) legacy-equivalent 0/1/10/50/100-camp scenarios runnable on both builds and (b) forward-schema capacity/churn scenarios added as each new state becomes real and judged against absolute/scaling budgets.
- [x] For the legacy-equivalent packet, measure idle camps, existing lead saturation, current structural maintenance, current dispatch, current return/writeback, and current save round trip.
- [x] Do not pretend paired outings, evidence expiry, ownership handoff, or new report state exists in the old build; add those forward fixtures at the implementing phase.
- [x] Restore a pristine fixture snapshot with recorded content hash and identical RNG/calendar state before every warmup and measured run.
- [x] Randomize paired A/B or B/A execution order across three run pairs under the explicit pragmatic decision; report run-level totals with dispersion/provisional 95% confidence intervals and record wide intervals rather than adding runs.
- [x] Collect 10,000 per-update latency observations inside each normal run and report p50/p95/p99/max separately from run-level totals; the deterministic 500-site stress row uses its declared 250-update fairness schedule.
- [x] Record scoped inclusive/self hostile-maintenance time, update-call count, cache hits/misses, route/pathfinding calls, serialized component cardinalities, and bytes where those legacy components exist.
- [x] Measure retained memory through bounded runner RSS plus current-process phase-boundary RSS. Allocation/live-heap attribution is deferred to Phase 9 unless a real phase regression approaches the retained-memory budgets.
- [x] Measure serialized `bandit_live_world` JSON bytes independently of whole-save compression and decompose current legacy camps/members/leads. Add sortie, operation, resource, dossier, report, observation, route, and debug decomposition only as each component becomes real.
- [x] Measure whole-save directory bytes, save wall time, and load wall time from the same pristine deterministic worlds.
- [x] Ratify the versioned long-soak requirements and terminal-cardinality gates; implement their exact schedules with the forward schema and run them at Phase 9 rather than fabricating absent state in Phase 0.
- [x] Ratify both sustained-new-OMT/depleted-resource exploration and fixed-world plateau soaks as forward-schema/Phase-9 gates.
- [x] Require a comparable focused packet at every phase checkpoint so a regression can be bisected before final profiling.
- [x] Store raw machine-readable results and a short environment/fixture manifest outside Git history; link them from `TESTING.md`.

### Provisional performance budgets

These budgets are ratified provisional v1 gates. A checked row means its numerical policy was
reviewed before implementation; forward behavior still has to prove the gate in its implementing
phase. Budgets may be tightened. They may be relaxed only with an evidence-backed explanation and
Josef's product decision.

- [x] Absolute scoped-maintenance p95 ceilings are `1/2/10/50/100 us` for 0/1/10/50/100 camps; a near-zero baseline is governed by these ceilings rather than percentages alone.
- [x] Zero/one-camp idle child-process CPU is no more than 2% relative where meaningful and stays below its absolute per-call ceiling; wall-clock uncertainty alone does not fail a near-zero case.
- [x] A normal 10-camp world adds no more than 5% to the final paired 24-hour macro simulation total and no more than 10% to scoped hostile-maintenance p95. The Phase-0 synthetic 10-camp CPU ratio CI is `0.998..1.005`.
- [x] No normal hostile-AI maintenance update creates a player-visible stall over 20 ms on the Mac Mini reference build.
- [x] After subtracting the zero-camp floor, 50-camp scoped work costs no more than 6x the 10-camp work and 100-camp work no more than 12x; both also remain below their absolute ceilings.
- [x] Stress-only single updates remain below 100 ms and are clearly labeled as non-normal conditions.
- [x] A synchronized cadence-avalanche fixture must remain within the 20 ms normal/100 ms stress spike budgets when dispatch, expiry, report, and prune state becomes real.
- [x] Loaded-bubble incremental caps are ratified: one visible pair p95 <=2 ms and p99 <=5 ms per avatar turn; four visible pairs p95 <=8 ms and p99/max-normal <=20 ms; blocked-exit stress <100 ms with no unbounded replan loop.
- [x] Persisted capacities are ratified: one typed scout-sortie slot and one typed follow-on-operation slot per camp, never both active simultaneously; at most 64 target/resource dossiers per camp; at most 16 retained observations in the live sortie or current report revision; no historical report archive beyond the current report plus acted-on revision summary; at most 256 cached high-level route steps per active operation; and at most 16 persisted failed exits.
- [x] Reference-aware pruning pins state required by an active operation and never breaks a live ID/revision/route foreign key.
- [x] Saturated camp-owned hostile-AI state remains at or below 64 KiB incremental uncompressed JSON per full camp unless reviewed evidence justifies more.
- [x] World-global resource state is budgeted separately at no more than 32 incremental bytes per harvested OMT plus bounded container overhead.
- [x] Normal 10-camp total hostile-AI state stays below 1 MiB and the 100-camp stress state below 10 MiB under the ratified fixture, including its specified resource count.
- [x] After retention caps are saturated, a second configured game year grows hostile-AI serialized state by no more than 5% without newly discovered camps/resources.
- [x] Sustained exploration grows only at the ratified compact bytes-per-new-resource slope; revisiting depleted OMTs does not recreate resource records or bounty.
- [x] Scanning 100,000 empty/default or merely unharvested OMTs creates zero world-global resource/depletion entries and at most 4 KiB fixed scan metadata; target leads remain inside the bounded per-camp dossier budget.
- [x] Normal-world save/load regression is no more than both 10% and 100 ms. This conservative provisional rule replaces 5%/50 ms because the three-pair Phase-0 packet is intentionally regression-grade rather than publication-grade.
- [x] Incremental retained-memory caps are 16 MiB for the normal 10-camp steady-state fixture and 128 MiB for the 100-camp stress fixture; after warmup, 1,000 fixed-cardinality materialize/dematerialize cycles leave at most 1 MiB unexplained net retained growth and no supported positive RSS/live-heap slope.
- [x] No unbounded vectors, completed-outing archives, route caches, observations, debug strings, or report history survive the implementing-phase/Phase-9 soaks.

### Phase 0 exit

- [x] Baseline functional and performance packets are reproducible and archived.
- [x] Provisional budgets are accepted or explicitly revised before behavior implementation.
- [x] Checkout and process preflight is green.
- [x] Checkpoint/ledger commit created if repo roadmap truth changed.

Evidence:

- Baseline commit: pre-change candidate `660057ff728bdf77531f607b1bd42a175f027a5f`.
  Behavior-neutral instrumentation is `22ca8759f239c3196a158c026cb64f6aeca2ae80` on `dev` and
  `b7e9a6a1f6138e3b2546157b9aa97887172e8bbd` on the preserved baseline worktree. Both diffs have
  stable patch ID `de11c834a4e0075a8695a8b7b4d5bdca698cfa48`.
- Repository/process preflight: Mac candidate and `origin/port/cdda-master` both
  `660057ff728bdf77531f607b1bd42a175f027a5f`; authorized transfer receipt records the
  same Windows candidate. Old `dev`/`master` preserved at
  `backup/dev-pre-hostile-ecology-20260802` (`56aa060678d2`) and
  `backup/master-pre-hostile-ecology-20260802` (`86f786bee563`); isolated `dev` was
  fast-forwarded to the candidate. Production worktree remained clean. Process audit found no
  transfer/build/orchestrator and no competing goal. Mac-only implementation boundary remains in
  force.
- Launch envelope: session metadata recorded GPT-5.6-sol/xhigh, `approval_policy=never`, and
  `sandbox_mode=danger-full-access`; `/Users/josefhorvath/.codex/config.toml` was normalized from
  `service_tier="priority"` to `service_tier="default"` before the goal and child work began.
- Roadmap/instructions: Josef explicitly promoted the package on 2026-08-02; `Agents.md`,
  `README.md`, `Plan.md`, `SUCCESS.md`, `TODO.md`, `TESTING.md`, `COMMIT_POLICY.md`, relevant
  `TechnicalTome.md` mechanics, and the immutable CodexBulk launch package were reviewed.
- Upstream read-only refresh: candidate includes `8d4959bee4`; fetched `upstream/master` remains
  `7cf1d08ae8`, 104 commits newer. `git merge-tree` conflict markers remain limited to
  `src/npcmove.cpp` and `data/mods/BombasticPerks/perkdata/closetland.json`. No merge was made.
- Functional command/result: `/usr/bin/python3 -m unittest
  tools.openclaw_harness.test_fixture_contract` on `660057ff` ran 57 tests with the expected one
  failure and one error: foreign Windows path instantiation and Linux acceptance of the real Mac
  venv. This is a recorded Phase-0 baseline defect, not a green result.
- Harness repair command/result: the reviewed native-store/path-classification patch at the next
  checkpoint passes `/usr/bin/python3 -m unittest tools.openclaw_harness.test_fixture_contract`
  with 60 tests and passes `py_compile` plus `git diff --check`. It rejects a Mac absolute or
  tilde-expanded venv on mocked Linux, avoids host-incompatible `pathlib` construction, and writes
  through Security.framework without putting the secret in a subprocess or error text.
- Deferred secure-store gap: one guarded real write from the existing interactive-shell
  `CATA_API_KEY` reached Security.framework and failed secret-free with `OSStatus -25308`
  (`interaction not allowed`). The existing shell export remains intact. This does not block the
  deterministic ecology phases; make no further Keychain attempt or blocker relay while Josef is
  unavailable, and leave final clean-environment retrieval/API proof for the Phase-10 gate.
- Environment/fixture/binary identity: Mac mini `Mac16,10`, Apple M4 (10 cores), 16 GiB,
  arm64 macOS 26.3.1 build `25D771280a`; Apple clang 17.0.0, GNU Make 3.81, Command Line
  Tools SDK 26.2. The successful command was
  `PATH=/opt/homebrew/opt/gettext/bin:/opt/homebrew/bin:/usr/bin:/bin /usr/bin/make -j4 tests
  LINTJSON=0 ASTYLE=0` at `54d2c76c0b`; it exited `0` in 57.78 seconds after the installed
  Homebrew gettext path was made explicit. `tests/cata_test` is arm64 Mach-O, 79,036,488 bytes,
  SHA-256 `4491718735452fa868644d9609f4fcfeffb13fb300b118378f2742c587699525`.
  Legacy functional fixture `release_candidate_roaming_v0_2026-08-01` resolves through the
  McWilliams source chain to seed `830204929`, 91-day seasons, non-eternal seasons, and transformed
  turn `5220000`. Full machine-readable receipt:
  `/Users/josefhorvath/codexbulk/C-AOL-hostile-ecology-artifacts/phase0-20260802/environment-fixture-manifest-54d2c76c0b.json`.
- Build audit: `macos-tests-build-c66364dbd1.log` is preserved as incomplete (no error, link,
  marker, or binary). `macos-tests-build-54d2c76c0b.log` records the immediate missing-`msgfmt`
  environment failure with exit `2`. `macos-tests-build-54d2c76c0b-gettext-path.log` records the
  only completed replacement build and explicit `CAOL_BUILD_EXIT=0` marker.
- Pre-change functional packet:
  `/Users/josefhorvath/codexbulk/C-AOL-hostile-ecology-artifacts/phase0-20260802/functional-baseline-manifest-ffbf32166c.json`.
  Against the behavior-identical binary recorded above, `bandit_live_world*` passed 43 cases/1,002
  assertions, `[bandit][handoff]` passed 3/41, `[bandit][playback]` passed 37/1,028, and
  `/usr/bin/python3 -m unittest tools.openclaw_harness.test_fixture_contract` passed 60 tests. Raw
  logs, exact commands, exit codes, and timings are named in the manifest.
- Known baseline failures/caveats: source at `ffbf32166c` admits `direct_player_range`, constructs
  exact `player@x,y,z` identity from the avatar OMT, and passes the avatar OMT directly into
  `plan_site_dispatch`. It solves one separate overmap route per selected member. This is source
  proof of radar and independent-party routing risk; visible dancing was not reproduced.
  Covert-hostility failure was also not reproduced; the current local gate only explicitly sets
  `NPCATT_KILL` for `combat_forward`, but no operation-scoped loaded-NPC disposition assertion
  exists, so it remains unqualified rather than credited green. Structural scan fairness is a
  source-proven prefix-starvation risk and currently excludes cannibal profiles. Playback timing is
  semantic-only and receives no performance credit.
- Natural creation/registration checkpoint: `dc094e8bf1b3d50a603ba1fdcdcb5ccfb997f66c`
  extracts the existing scan behavior without policy change. The slow integration test freezes
  internal worldgen seed `830204914`, generates a fresh adjacent overmap through ordinary
  `overmap_buffer.get`, and proves both natural default specials, canonical 8-tile footprints,
  abstract headcounts 6/5, idempotent registration, real 14-member mapgen reconciliation, and
  duplicate-free JSON round trip. It passed 34 assertions even with CLI seed `12345`; the full
  `[bandit][live_world]` tag passed 68 cases/1,459 assertions. The final structured autoreview
  returned clean with no accepted/actionable findings. Raw logs are
  `natural-worldgen-registration-internal-seed.log` and
  `live-world-regression-natural-cleanup.log` under the Phase-0 artifact root.
- Benchmark instrumentation evidence: the Python driver suite passes 77 tests; the visible
  histogram validation case passes 1 case/13 assertions; the full `[bandit][live_world]` tag
  passes 68 cases/1,459 assertions; `git diff --check` passes; and final xhigh autoreview artifact
  `autoreview-benchmark-instrumentation-checkpoint-clean.txt` reports no actionable finding. Both
  exact commits were built sequentially with
  `PATH=/opt/homebrew/opt/gettext/bin:/opt/homebrew/bin:/usr/bin:/bin /usr/bin/make -j4 tests
  LINTJSON=0 ASTYLE=0`. Dev log `macos-tests-build-instrumentation-dev-22ca8759f2.log` ends
  `CAOL_BUILD_EXIT=0`; binary size/SHA-256 are 79,150,952 bytes and
  `858ddd88ec9c8cf77639392620a136cc112caa5e34f46c39aea0cb1c828918e0`. Baseline log
  `macos-tests-build-instrumentation-baseline-b7e9a6a1f6.log` ends `CAOL_BUILD_EXIT=0`; binary
  size/SHA-256 are 79,112,952 bytes and
  `eb14211166ca4021c88933f88f3103b00cdd061a1283ee7fc63a058b4b66b146`.
- Rejected comparison smoke: one committed AB pair completed two valid children, then the runner
  correctly rejected the packet with exit `2`; independent `validate --verify-files` also exited
  `2`. Raw artifact SHA-256 is
  `0c5d1431c120ae0f8913e98543da64c7cf1bd968915ef1dc6320fca3e396249c`. The baseline child
  populated 3,418 ignored FlatBuffer entries under `data/cache` after initial identity capture,
  changing the manifest from 8,156 to 11,574 files. Both Git worktrees remained clean. This stays
  red/non-credit historical evidence; the following checkpoint repaired the contract and proved
  it from fresh cold roots instead of crediting an incidental now-warm rerun.
- Fixture/input checkpoint: `c2d7921d9f` on `dev` and `7e6d11091d` on the preserved baseline have
  identical stable patch ID `c8b72321516ccf34ce160121d4da4ab2d44aee42`. The Python runner
  suite passes 94 tests; direct lead-saturation, histogram, and full live-world gates pass; final
  xhigh closeout review is clean. Exact sequential builds exited `0` with dev binary
  `d113a5480473f6e70f637aab2f030ba38bb4cf346fd0906ec8c766ad4051fa61` and baseline binary
  `708cfeb2fc763f9083809dd182f1f480d5677f02746c826acb6bd13798017f88`.
- Accepted cold-cache integration smoke: raw artifact
  `paired-smoke-c2d7921d9f.raw.json` has SHA-256
  `62b57c1e88778576e1c1f248c637f5427543152268cdfdc923e0dc77c985c444`; runner and independent
  file-verifying validation exited `0` with two valid runs. Both roots started from the identical
  8,156-file non-cache/source manifest, populated 3,418 recorded cache files during the
  sacrificial warmup, then held immutable warmed identities through measurement. Fixture SHA,
  seed, calendar turn `5220000`, 91-day season, initial/terminal state, and timing/fairness replay
  reset all matched. The associated 2,000-bootstrap summary is accepted at SHA-256
  `de65f54b01e9d6593f392ce17f2271f8f8e1ce9a56c5420f73ad46fe493aa18d`. This smoke receives
  reproducibility/integration credit only; the pragmatic official packet below supplies Phase-0
  engineering-baseline credit.
- Pragmatic instrumentation checkpoint: `fee1e44d38` on `dev` and identical-patch cherry-pick
  `2a3e7efb17` on the baseline have stable patch ID `bf8a5649...`. Exact sequential builds exited
  `0` in 41/36 seconds. Dev binary is 79,191,992 bytes at SHA-256 `6aada731...`; baseline is
  79,153,992 bytes at `75854084...`. Exact `fee1e44d38` passes 110 Python tests, 3 focused C++
  cases/1,775 assertions, `py_compile`, raw/summary validation, and `git diff --check`.
- Official pragmatic packet: one cold-cache 25-case matrix, three pairs, 150 measured children,
  seed `830204929`, pair orders AB/AB/BA, and identical 8,156-file/102,972,091-byte source-data
  manifests completed in 2,279 seconds with zero failures, zero fixture-restore failures, and zero
  cross-variant terminal-equivalence failures. Raw SHA-256 is `7332059a...`; the independently
  validated 1,000-bootstrap summary is `9736b3af...`. Structural p95 maxima at 0/1/10/50/100 are
  `0.209/0.671/3.167/13.951/27.391 us`; normal max is `53.667 us`, stress-500 max `36.167 us`,
  and structural-10 CPU ratio CI `0.998..1.005`. Structural phase-RSS deltas are below `0.4 MiB`
  at 10 camps and `0.3 MiB` at 100/500; serialize-100 stays below `1.8 MiB`.
- Save/growth evidence: all 18 whole-save runs completed real `game::save` plus menu-level
  `game::load`; maximum save is `472 ms`, maximum load `7.084 s`, and maximum directory growth
  `1,480,374` bytes. Baseline/dev directory growth is identical at every scale. Current legacy
  structural terminal JSON is 79,532 bytes at 10 sites, 805,185 at 100, and 2,626,240 at 500.
- Fairness caveat: 0/1/10/50/100 have zero never-serviced eligible camps over 10,000 updates, but
  the 500-site/250-update legacy stress row services 125/250 and starves 125 with maximum wait 250.
  The accepted forward gate is zero starved and maximum 32 hourly waits at 500 sites. Phase 3 must
  replace the prefix restart and include cannibals; Phase 0 does not misclassify this as green
  behavior.
- Performance artifact manifest:
  `/Users/josefhorvath/codexbulk/C-AOL-hostile-ecology-artifacts/phase0-20260802/phase0-pragmatic-evidence-manifest-fee1e44d38.json`
  (SHA-256 `ff410e9b...`). Core/paired/fairness TSVs and the full raw/summary sit beside it.
- Save-growth artifact manifest: the same manifest plus
  `official-pragmatic-phase0-whole-save-metrics.tsv` (SHA-256 `0ab9e17e...`).
- Known caveats: this first preflight turn began while the local Codex config still named priority
  service; that already-started request could not be retroactively changed. All later local
  launches inherit `default`. The repaired writer is unit-tested but headless secure-store access
  remains unqualified because macOS denied non-interactive access; no repeated write is permitted
  during this deterministic lane. The API venv has `any_llm` but lacks `flatbuffers`, so fixture
  contracts currently use `/usr/bin/python3` while later API self-tests use the native venv.

## Phase 1 - one authoritative persistent model

Primary anchors: `bandit_live_world::site_record`, `camp_intelligence_map`, existing active-group fields, `bandit_pursuit_handoff`, savegame member `bandit_live_world`.

- [x] Independently audit the existing serialized schema and migration behavior.
- [x] Define a durable `scout_sortie` with stable ID, generation, camp ID, selected member IDs, leader, shared route, waypoint, target/resource lead revision, observations, cargo, casualties, timestamps, and idempotency keys.
- [x] End a scout sortie only after every member is returned, confirmed dead, or declared missing after its fixed grace. A first-survivor report may update a provisional dossier, but no follow-on operation can reserve the slot until the sortie closes. Do not carry scout member reservations into a later response.
- [x] Define scout phases: assembling, outbound, searching, observing, harvesting, burned-withdrawal, returning-exposed, returning-report, returning-home, and lost. `returning-exposed` is the bounded fallback after coherent burn-origin evacuation when no concealed rally exists; it can only advance toward home and cannot re-enter observation.
- [x] Define separate bounded camp decision state: idle, report-awaiting-assessment, preparing-follow-on, and cooldown/abandoned.
- [x] Define a new `hostile_operation` for a shakedown or raid with a new operation ID/generation, fresh member reservations, `source_report_revision`, route/rally state, and phases assembling, outbound, rallying, waiting-night, approaching, committed-contact, returning-home, and lost.
- [x] Give both scout sorties and hostile operations a serialized `simulation_owner` (`abstract` or `local`), handoff generation/epoch, and `last_advanced_turn`; any transient handoff state must commit or roll back before a save is accepted.
- [x] Remove or migrate competing scalar state rather than maintaining two authorities indefinitely.
- [x] Define a world-global finite resource record keyed by OMT.
- [x] Add one bounded generic camp supply stock: integer `supply_units`, where one unit is one member-day. Cap at `min(256, 14 * max(1, living_total))`; consume `living_total` units per real 24 game hours with deterministic bounded catch-up; clamp roster-change overflow. Legacy sites seed at seven member-days per living member so migration does not create an instant starvation dispatch.
- [x] Keep per-camp resource knowledge as an estimate with timestamp/confidence.
- [x] Enforce the ratified numerical caps and deterministic, reference-aware pruning for leads, observations, route cache, reports, and completed state; active operations pin every referenced ID/revision until termination.
- [x] When the 16-observation cap is full, deterministically compact/replace lower-value stale evidence before burn, casualty, contradiction, hard-danger, or target-revision facts. Define progress as a new deduplicated fact that changes certainty, bounds, route state, or alert; polling and duplicate strength do not reset `last_progress_at`.
- [x] Version reports and record the last report revision acted upon per target/faction policy.
- [x] Give return packets, report delivery, resource depletion, cargo credit, and member return stable operation/idempotency keys.
- [x] Persist monotonic per-camp sortie/operation generations plus component application watermarks and per-member resolution bits. Once a completed record is pruned, packets at or below its watermark remain no-ops; world-resource claims use the resource's monotonic revision. Do not retain an unbounded tombstone list.
- [x] Validate a complete packet before mutating any member, roster count, cargo, resource, or dossier state; replay after save/load is a no-op rather than a duplicate credit.
- [x] Add minimal structured transition events now: operation ID/generation, simulation owner, previous/new phase, reason, and turn. Keep them bounded/on-demand rather than a persisted prose log.
- [x] Add old-save migration and round-trip tests, including missing/new fields and legacy active groups.
- [x] Save/load every active phase without duplicate members, operations, transitions, reports, depletion, or cargo.
- [x] Add a brief durable mechanic description to `TechnicalTome.md` once the model is real.
- [x] Run serialization byte benchmarks at empty, normal, and saturated state.

### Phase 1 exit

- [x] One state owner exists for each camp, outing, resource, and dossier.
- [x] Round trips and legacy migration pass.
- [x] Malformed packets are atomic/no-op.
- [x] Saturated serialized size remains within the agreed Phase 0 budget.
- [x] Behavior + tests form a reviewable checkpoint commit.

Evidence:

- Commits: `673a900067` makes malformed return application and world deserialization atomic;
  `4995a3c64e` adds schema-v2 typed outing identity, generation/key/watermark persistence,
  legacy migration/repair, and routes runtime consumers off the old group-id scalar;
  `e4b75e15a3` adds the complete bounded scout envelope, independent report/cargo receipts,
  casualty/clock/phase persistence, strict component watermarks, and transactional return checks;
  `42e5bad3cd` persists per-member resolution and applies provisional first-survivor receipts while
  retaining the active reservation through final return or fixed-grace loss; `31354b71c3` rejects
  a missing declaration before the persisted grace deadline and accepts it at the boundary;
  `7acc011951` centralizes expected-phase transitions, enforces irreversible homeward phases, and
  safely repairs unknown phases or malformed scout/job pairs; `687d7bcecb` adds a five-state
  report-pinned camp decision owner, safe migration, and stale-dispatch exclusion; `67cd68e416`
  adds the schema-v5 fresh hostile-operation owner, report/route/rally pins, canonical identity and
  receipt keys, one-way identity-CAS phases, safe legacy withdrawal migration, and save-time
  consistency repair; `833599e5e4` adds one schema-v4 activity/generation/owner/epoch/time cursor,
  exact compare-and-swap owner transfer, strictly newer state advances, atomic same-minute
  start/contact handoff, safe legacy migration, and fail-closed current-schema repair;
  `432c0f9da7` adds the schema-v4 OMT-keyed finite-resource authority, exact revision claims,
  persistent depletion, transactional malformed-load rejection, and harvested-only pre-v4 migration;
  `37498066ba` adds schema-v6 bounded member-day supply, O(1) catch-up, roster-cap enforcement,
  exact casualty-time reconciliation, and seven-member-day legacy/new-camp seeding;
  `1aa9851902` makes physical resource-estimate updates camp-private, timestamp/confidence-bearing,
  stale-safe, and independent from world truth or another camp's belief; `ddd1afe480` adds stable
  dossier ID/revision ownership, schema-v7 migration, deterministic 64-lead pruning, stale-plan
  rejection, terminal/no-op revision safety, bounded strings/marks, and compact default-omitting JSON;
  `9be3e8c044` adds semantic 16-fact observation compaction and cursor-atomic batches;
  `258247d26c` adds schema-v8 faction-scoped report policy, canonical 64-key acted watermarks,
  overflow-safe report revision allocation, policy-drift rejection, and absent-only legacy migration.
- Tests: latest strict redirected Mac build exit `0`; `[hostile_operation]` 3 cases/243
  assertions, `[bandit][live_world]` 86/2,714, `[bandit][handoff]` 9/202, and 2 overmap-global
  save compatibility cases/16 assertions pass. Exact-source autoreview is clean at 0.99. Binary
  SHA-256 is `d6e8a9f0fe1570437cfbabda375aa10cdb0b6452bf45416fe387dca8db0bef26`.
- Serialized sizes: empty world 87 bytes; normal four-member camp with active scout 4,558 bytes;
  cap-saturated route plus active/current observations 28,534 bytes. Saturated output is
  byte-stable across round trip and below the 64 KiB per-full-camp provisional gate.
- Resource persistence adds 29 bytes per harvested OMT over the measured 500-to-1,000 record
  interval, below the ratified 32-byte incremental gate. Competing stale claims and exact depleted
  replays are byte-identical no-ops; a malformed pre-v4/current-field hybrid is rejected before commit.
- Supply tests cover cap edges, schema-v5 migration, incomplete-v6 fail-closed behavior, fractional
  round trip, daily/large-jump equivalence, backward-time no-op, zero-living stability, roster shrink,
  and 730-day bounded catch-up. Saturated camp/scout JSON remains 29,730 bytes below 64 KiB.
- Private-estimate tests cover global-claim isolation, cross-camp isolation, stale/invalid byte-level
  no-op, later physical depletion, and round-trip persistence using the existing bounded lead fields.
- Intelligence tests cover forward/reverse canonical 64-lead saturation, deterministic duplicate
  resolution including delimiter-shaped strings, oldest active-reference retention, positive legacy
  revision migration, stale and terminal plan/update rejection, duplicate-signal byte identity,
  bounded strings/marks, and writer/load normalization. The final redirected build
  `build_logs/macos-tests-build-phase1-reference-pruning-final.log` exits `0`; focused intelligence
  passes 3/48, full live-world 95/4,931, handoff 9/203, and overmap save compatibility 2/24.
  Empty/normal/full saturated JSON is 87/5,842/48,070 bytes and the full form is byte-stable below
  64 KiB. Test binary is 79,898,584 bytes with SHA-256
  `f435a54a682e7bfc061e7973e271cecd47997c15cc690cbfd55ab1df869214d7`.
  One xhigh AutoReview accepted five concrete defects (legacy revision downgrade, no-op revision
  churn, terminal revision reuse, delimiter collision, and unbounded strings); all were fixed in the
  single permitted review/fix pass. Artifacts are under the external Phase-1 reference-pruning root.
- Report-policy tests cover exact/stale replay, an older-generation report for a different target,
  a newer revision for the original target, the same target under a different faction policy,
  canonical 64-entry retention, legacy migration, explicit-unknown fail-closed behavior, profile
  drift, and atomic `INT_MAX` report exhaustion. The final build exits `0`; `[report_policy]`
  passes 3/42, `[bandit][live_world]` 99/5,046, handoff 9/203, and overmap save compatibility
  2/24 at seed `830204929`. Binary SHA-256 is
  `12030ac296c498bc03b87f27949107039a036c4331e79a261114c9d4646e5e87`.
  Exact logs and manifest are under the external `phase1-20260802/report-policy` artifact root.
- Migration/replay fixtures cover legacy and transitional active state, contact-anchored clocks,
  malformed reservation release, all scout-phase round trips, partial casualty persistence,
  exact casualty/job agreement, report/cargo receipt before slot close, universal watermark
  ordering, provisional split arrival, fixed-grace terminal loss, and byte-for-byte stale replay
  no-op. The 10x10 phase matrix adds stale expected-phase/time, wrong kind/job, unknown future
  phase, burn re-entry, exposed-return, and malformed reservation repair coverage. This checkpoint
  also covers the 5x5 decision graph, report/generation/time rejection, provisional/scavenge/all-loss
  controls, cooldown watermark retention, legacy final-report migration, and fail-closed repair.
  It does not claim detailed Phase-5 burn perception/egress or a physical post-close return after
  a missing declaration.
- Component-idempotency checkpoint `f12180de5f` adds canonical return/report/cargo/resource and
  per-member receipt keys, validates complete packets before mutation, persists bounded component
  watermarks plus one last-resource receipt, and binds new resource depletion to the exact issued
  camp operation. Current schema rejects forged or terminal receipts atomically; exact replay
  remains a no-op after save/load or operation closure. Final Mac build exits `0`; at seed
  `830204929`, resource passes 4/2,117, full live-world 99/5,099, handoff 10/247, playback
  37/1,028, and overmap save 2/24. Empty/normal/saturated state is 87/6,020/48,265 bytes,
  byte-stable below 64 KiB. Final xhigh AutoReview is clean at 0.98. Exact logs and hashes are in
  the external `phase1-20260802/component-keys/MANIFEST.md` packet.
- Transition checkpoint `16649b77b0` adds opt-in, non-persisted events with exact activity and
  generation, committed final owner, prior/new phase, reason, and minute. The probe retains at most
  64 events, rejects any copied string above 256 bytes, and accounts every rejected/full-buffer
  event in one aggregate drop count. Final Mac build exits `0`; transition events pass 3/79, full
  live-world 102/5,178, and overmap save 2/24 at seed `830204929`. The external
  `phase1-20260802/transition-events/MANIFEST.md` records commands, hashes, the accepted review
  fixes, and why a request for categorized counters was outside the bounded contract.
- All-phase checkpoint `e408c9c450` drives one real hostile operation through every active phase
  plus `lost` and round-trips the full world twice at each point. Loads synthesize zero transition
  events and preserve the sole operation/generation/owner, report and component pins, unique member
  reservations/resolutions, and member states. The exact case passes 1/833 and full live-world
  passes 103/6,011 at seed `830204929`; logs and hashes are in the external
  `phase1-20260802/all-phase-roundtrip/MANIFEST.md` packet.

## Phase 2 - roster authority, paired dispatch, and reservations

Primary anchors: `count_live_members`, abstract `headcount`, lazy materialization, `plan_site_dispatch`, `choose_camp_map_dispatch`, `plan_structural_bounty_outing`, `scout_sortie_should_return_home`.

- [x] Persist and validate distinct roster counts/sets for living total, physically present at site, away, reserved, and ready; never overload one `headcount` with all meanings.
- [x] Materialize exactly the selected party plus required local reserve, never fixed faction counts unrelated to the plan.
- [x] Implement shared policy plumbing but separate v1 `routine_scout_policy` (exact pair or no outing) from threat-derived `response_party_policy` for shakedowns and raids.
- [x] V1 `routine_scout_policy` always returns exactly two or no outing. Keep any future trio hook disabled and separately test that high danger/reward cannot activate it.
- [x] Use fresh response-party selection after a scout report; two-standard scouting must not leak into combat-force sizing.
- [x] Implement the exact routine size matrix in the behavior contract.
- [x] Select a scout/observer and escort using actual readiness and capability; do not always drain the strongest defenders.
- [x] Reserve all selected member IDs and the relevant camp mission slot atomically under the owning operation ID and generation.
- [x] Release a reservation only when operation ID and generation still match; stale abort/load cleanup must not release a newer operation's members.
- [x] Release matching reservations on every success, abort, death, migration, origin loss, and load-failure path.
- [x] Generalize singleton-only timeout/return logic to groups.
- [x] Preserve at most one active external operation per camp for the first production version. The scout-sortie slot must close after all member resolutions or timeout and its final/provisional report revision must be accepted before a fresh follow-on operation can become active; no scouting/raid overlap or in-place type mutation.
- [ ] Table-test populations 0-10 for both factions, wounds, sleep, incapacity, missing members, and active reservations.
- [x] Test camp size 2 becoming empty without losing site ownership or creating a phantom home defender.
- [x] Define origin-loss behavior while a pair is away: camp attacked/captured/deleted/invalidated, recall if legitimately signaled, orphaned return, return failure, mission-slot cleanup, and final site ownership.
- [ ] Test concurrent dispatch attempts and stale cleanup after a newer operation generation exists.

### Phase 2 exit

- [ ] Every v1 camp-backed routine outing is exactly a pair; no trio/singleton fallback is active.
- [ ] No NPC can be reserved or dispatched twice.
- [ ] No singleton fallback bypasses readiness/reserve rules.
- [ ] Dispatch selection benchmark remains bounded and approximately linear.
- [ ] Behavior + tests form a checkpoint commit.

Evidence:

- Commit: `563499e3fe` persists `living_total`, derives and validates the canonical roster view, rejects
  malformed current schema transactionally, repairs bounded legacy state, preserves empty-camp
  ownership while members are away, and reconciles abstract spawn-tile authority on materialization.
- Roster authority: 2 cases / 132 assertions; full live-world: 105 / 6,154; overmap save regression:
  2 / 24; save-size: 1 / 10; handoff: 10 / 251. Final Mac build and every filter exit `0`.
- Artifact: `/Users/josefhorvath/codexbulk/C-AOL-hostile-ecology-artifacts/phase2-20260803/roster-authority/MANIFEST.md`.
- Commit: `c846be1632` adds exact bandit/cannibal routine-pair and materialization policy, keeps response
  sizing and micro-site singleton policy separate, preserves a concrete reserve at population 3+,
  applies dispatch transactionally, generalizes return timing, and rejects undersized response
  requests plus overwhelming-danger stale dispatch.
- Matrix test result: `[routine_policy]` 2 / 183 and full live-world 107 / 6,347; handoff 10 / 251;
  playback 37 / 1,028; overmap save regression 2 / 24; save-size 1 / 10. Final build and every
  credited filter exit `0`; final AutoReview is clean.
- Artifact: `/Users/josefhorvath/codexbulk/C-AOL-hostile-ecology-artifacts/phase2-20260803/routine-pair/MANIFEST.md`.
- Commit: `5fbefa452e` removes caller-supplied hostile member IDs, selects from the current ready
  post-report roster, recomputes pinned lead threat/reward sizing for both factions, validates the
  lead ID/revision/target/OMT, and rejects roster or dossier drift atomically at apply.
- Fresh response result: hostile-plan 1 / 82 and final full live-world 107 / 6,359; handoff 10 / 251;
  playback 37 / 1,028; overmap save regression 2 / 24; save-size 1 / 10. Final build and every
  credited filter exit `0`.
- Artifact: `/Users/josefhorvath/codexbulk/C-AOL-hostile-ecology-artifacts/phase2-20260803/fresh-response/MANIFEST.md`
  (SHA-256 `94c0c9327f43854ef206edaae978823fedf98a4129b9859a19fe02772e1c7dd1`).
- Commit: `f049104375` selects a stable strongest observer plus the lightest return-safe escort,
  refreshes presence/death/HP/sleep/incapacity from live NPC state, rejects role/readiness drift at
  apply, and repairs legacy members without template IDs to the profile's generic scout.
- Capability result: routine policy 3 / 227, migration 1 / 87, save round trip 1 / 32, multi-site
  1 / 67, and final full live-world 108 / 6,408. Final authoritative Mac builds and every credited
  filter exit `0`; binary SHA-256 is
  `02b3e3c4bd398a0a7287578a7b49da57adcd8d3f7e7cd33b048e8ba007e89471`.
- Artifact: `/Users/josefhorvath/codexbulk/C-AOL-hostile-ecology-artifacts/phase2-20260803/capability-pair/MANIFEST.md`
  (SHA-256 `a2784be680ceb76e0b08c044ced8d753fcdad4212b24466800ee83a74d938808`).
- Commit: `f65e6bd28a` pins structural plan activity ID/generation, shares the camp mission-slot gate,
  and atomically commits the exact selected IDs, owner envelope, and generation advance. Competing
  and stale plans reject byte-identically before and after another operation resolves; the active
  reservation round-trips unchanged.
- Reservation result: focused 1 / 45, structural bounty 24 / 458, and full live-world 109 / 6,453.
  Final build and every filter exit `0`; structured xhigh review is clean at 0.96. Binary SHA-256
  is `626844eafaa6f20a02ac754e3884ffc2c4c75e70d8386c8cc35683de0deff2b5`.
- Artifact: `/Users/josefhorvath/codexbulk/C-AOL-hostile-ecology-artifacts/phase2-20260803/reservation-ownership/MANIFEST.md`
  (SHA-256 `37cc3ba65e385534a0bb31e0c22214203f030b1573ae5d068cbf67ea75cbd595`).
- Commit: `61017301a4` makes structural release compare the exact current activity ID/generation,
  release only unresolved away members on a candidate copy, preserve resolved casualties, clear
  only the matching slot, validate the roster, and return the exact committed member count.
- Matching-release result: focused 1 / 70, structural bounty 24 / 483, and full live-world
  109 / 6,478. Final build and every filter exit `0`. The first structured review's returned-count
  finding was accepted; the same-engine final review is clean at 0.96. Binary SHA-256 is
  `eeeb6524e134f09b0b000646e4fade314163771eaae30734a4edcf12e8af6b16`.
- Artifact: `/Users/josefhorvath/codexbulk/C-AOL-hostile-ecology-artifacts/phase2-20260803/matching-release/MANIFEST.md`
  (SHA-256 `82c06375b3370175f327377748eaae4bce34377e8b549f76b76ba8f1536dd63f`).
- Commit `084b7c0747` centralizes exact release for ordinary, structural, and hostile holders,
  preserves resolved casualties, closes matching mission slots, and preflights paid shakedown
  cleanup before goods transfer. Commit `f29808d80b` adds schema-11 origin disposition and
  non-dispatchable orphans, physical-signal-only recall, complete exact return resolution, terminal
  no-reactivation, v10 migration, and transactional current-load rejection.
- Complete-release result: origin 1 / 82, prior release paths 1 / 93, migration 5 / 275,
  current load failure 1 / 3, and full live-world 111 / 6,737. Final build and every filter exit
  `0`; the single root review's pre-commit event finding was fixed. Binary SHA-256 is
  `6cb7a8727eef742a9666e2771accc6b71f6b7d64dcfe11557bc5dec07bcd7691`.
- Artifact: `/Users/josefhorvath/codexbulk/C-AOL-hostile-ecology-artifacts/phase2-20260803/origin-loss/MANIFEST.md`
  (SHA-256 `595881ac38952fd262de1ed0a4fe6f23379996c09c38e18f13f1112878a62212`).
- Dispatch benchmark:

## Phase 3 - shared real scouting ecology and finite bounty

### Exact v1 dispatch contract

Use deterministic integer permille values `0..1000`. The routine scheduler hard-gates on: bandit/cannibal camp profile; at least two living and two ready/capable members; no active sortie/follow-on operation/unresolved return packet; `now >= next_routine_dispatch_eligible_at`; atomic reservation of exactly two members plus the mission slot; and at least one candidate surviving danger/route gates.

For each eligible camp, compute need `N`, knowledge gap `K` (fraction of eight frontier sectors unseen or older than seven days), best cheap legitimate target `T` (the highest cheap candidate score below), and cadence `C` (0 through 24 hours after resolution, linearly rising to 1000 at 72 hours). Derive supply days as integer/fixed-point `supply_units / max(1, living_total)`: at least 7 days -> pressure 0/`N=0`; 3 to <7 -> 1/333; 1 to <3 -> 2/667; below 1 -> 3/1000.

```text
dispatch_drive D = (350*N + 250*K + 200*T + 200*C) / 1000
```

Dispatch only when `D >= 500` and the best routed target score is at least 300. The first outing becomes force-due at a stable `hash(camp_id)` offset from 6-18 hours after activation; 72 hours since the last resolved routine outing also becomes force-due. Force-due bypasses `D`, never readiness, knowledge, danger, route, reservation, or global scheduler limits.

Candidate generation is bounded to at most 22 deduplicated records: six remembered ground-bounty leads, four due human-camp dossiers, four legitimate current home-perceived signals, and eight least-recent frontier spokes. Each frontier candidate is one radial 3-5-waypoint route that samples its sector at both 4-6 and 7-9 OMT before returning, within the 18-OMT complete-route cap. A persisted stable sector cursor makes one frontier route force-due if none resolved in 72 hours; it may be delayed only by readiness/danger/route/global budgets, not forever by higher-reward known targets. Thus all eight sectors are sampled to the outer ring within eight resolved frontier outings and, absent hard blocks, within 24 days. Remembered/signaled goals may be within 12 OMT. This permits 10-12 OMT camps/targets to meet through 3-OMT visibility without beelining. Hidden loot/residents/mobile threats are never inspected. Static passability may prevent an impossible route but does not become camp knowledge.

Normalize remembered estimates/signals to `0..1000`. Convert stalking certainty to dossier confidence with integer `confidence = floor(certainty * 1000 / 95)`. Freshness is linear `clamp((expires_at-now) * 1000 / (expires_at-observed_at), 0, 1000)`; permanent facts use 1000. Signal base strength is fixed by class/quality: significant sound 600, smoke/light 700, direct visual quality 1/2/3 = 300/700/1000; `G = floor(base_strength * freshness / 1000)`. For each candidate compute weighted confidence/freshness `W`, expected reward `R`, information value `I = 1000 - W`, legitimate signal `G`, novelty `V` (never visited = 1000, otherwise linear 0 -> 1000 across seven days), route quality `Q`, faction terrain fit `F`, known risk `X`, and repetition penalty `P` (last routine target 1000, previous target 500, otherwise 0). Use `neutral_prior=333` for unknown ground/frontier salvage and `0` for human-camp opportunity until a legitimate signal/observation exists. Terrain fit table: bandits rate road/shelter/building/town edge 1000, field/forest edge/rural 500, deep forest/swamp 250, impassable 0; cannibals rate forest edge/rural/shelter 1000, deep forest/field/town edge 500, roads/buildings 250, impassable 0.

Faction opportunity is exact: bandits normalize a legitimately reported bounty estimate of 0/1/2/3+ to 0/333/667/1000; cannibals use 0 when no human presence was legitimately confirmed, otherwise `min(1000, 600 + 100 * (defenders_low - 1))`. Risk is independent, so additional observed humans increase both prey value and danger rather than cancelling either invisibly.

Bandit `bounty_estimate` is coarse outward observation, never remote storage inspection. Set it to 0 with no eligible occupied-human-camp evidence or after the clear-empty rule. A legitimately confirmed occupied camp gives 1. Add one step, to at most 2, after one distinct outward wealth cue is observed: a vehicle belonging to the site, powered/workshop infrastructure, constructed storage/fortification activity, or cargo hauling/storage interaction. Raise to 3 only after either two different cue classes in distinct 30-minute buckets or one quality-3 observation of repeated/bulk cargo handling. Deduplicate cue class/source/bucket, retain its provenance and timestamp, and lower only through newer contradictory/empty evidence. Visible defender equipment feeds danger, not bounty. Light, smoke, camp size, theoretical zone capacity, hidden items, and exact item value never raise this field by themselves. Exact reachable stored value remains contact-only in Phase 7.

```text
W = confidence * freshness / 1000
R = (W * remembered_estimate + (1000 - W) * neutral_prior) / 1000
S = clamp((300*R + 250*I + 150*G + 100*V + 100*Q + 100*F
           - 450*X - 150*P) / 1000, 0, 1000)
```

Cheap scoring is pathfinding-free: set `Q` from clamped Chebyshev OMT distance against the 18-OMT cap plus known static impassability only, compute `T` from that cheap `S`, and rank candidates. Only the top two may consume the global full-route budget; replace `Q` with `clamp(1000 - full_route_cost * 1000 / 18)` and recompute final `S`. Acquire at final `S >= 300`; retain the already selected target down to `S >= 150`. The active outing never retargets because the avatar moved or a new global fact appeared. Bandits project stored-goods opportunity; cannibals project legitimately observed human opportunity. The machinery and weights remain shared.

Risk policy uses the single Phase 6 `danger_high` authority: every legitimate route/terrain/mobile/alert estimate is normalized onto `0..200`, then `X = clamp(5 * danger_high, 0, 1000)`. Static terrain priors on the `X` scale are impassable 1000, swamp/dense urban 600, forest/building/town 400, road/field 200, and unknown traversable 300. A candidate is also hard-rejected when known `danger_low >= 2 * selected_scout_party_power`, even if absolute `X` is lower for a weak pair. `X >= 750` forbids the candidate; `500 <= X < 750` requires a route whose every segment is below 500. After departure, newly perceived `X >= 750` or the relative danger gate permits at most two bounded detour attempts, but every successful detour becomes a homeward withdrawal and the sortie cannot resume its mission. Only newly perceived danger below both gates may be bypassed and followed by a return to the committed route. Unknown-strength observed danger uses `X=600`; unexplored terrain uses only its static prior. Examine at most the top two candidates and two route alternatives each.

Unknown abstract danger is not radar and scouts are not invulnerable. One `advance_outing_segment` owner checks a bounded adapter for the party's current OMT plus only the OMTs on its committed forward corridor that the ordinary OMT observer says are currently visible under weather, light, terrain, elevation, and optics (never beyond three route OMTs). This permits a clear-day/binocular pre-entry horde sighting and detour; a dark, screened, off-corridor, or otherwise out-of-LOS threat remains unknown until legitimate proximity/overlap. The adapter consumes live overmap-group/entity records already present on those permitted OMTs and camp-remembered threats; it may not scan arbitrary surrounding hordes or future route tiles and then invent visibility afterward.

At first legitimate pre-entry observation or overlap, normalize through the same existing monster/overmap threat evaluator chosen and recorded in Phase 0 and attempt at most two passable adjacent detours. For actual overlap, persist one bounded active encounter episode `(outing_id, encounter_episode, stable_threat_id_set, overlap_omt, outcome_applied)`. An hourly tick, save/load, or owner-generation handoff cannot create another outcome while that episode remains active. Clear the active episode only after the party has left the overlap OMT and the same threat set is absent from the party's current OMT for one complete segment advancement; a later re-encounter increments `encounter_episode`. Retain only the current episode plus a monotonic last-applied episode watermark.

If the party is in/near the active reality bubble, materialize once and ordinary local escape/combat owns the result. Otherwise, when no detour exists, use this provisional v1 non-victory resolver: `danger_high < party_power` -> both survive, the lower-power/stable-ID member becomes wounded/unready, and the pair withdraws; `party_power <= danger_high < 2*party_power` -> the lower-power/stable-ID member becomes missing and the survivor becomes wounded/unready and returns; `danger_high >= 2*party_power` -> both become missing. The abstract resolver never deletes/damages the threat or fabricates corpses/loot. Persist the member outcome and episode watermark before advancing; replay, save/load, and owner handoff are idempotent.

Each in-game hour, a persisted round-robin cursor considers at most 16 camps, performs at most eight full route solves, and starts at most two outings globally. Compute `overdue_bonus = floor(250 * clamp(overdue_minutes, 0, 4320) / 4320)` and sort contenders by `D + overdue_bonus` with stable camp-ID tie-break. Exhausted global budget is not recorded as camp failure. At 100 due camps, every camp is reconsidered within seven hourly passes.

Camp-wide routine-dispatch cooldown (`next_routine_dispatch_eligible_at`) plus deterministic 0-6 hour camp jitter: useful return 24h; empty/stale return 18h; no candidate/route 12h; burned/danger withdrawal 24h; casualty/missing party 72h. Consecutive no-route/no-candidate failures back off 12h -> 24h -> 48h; a useful return resets the streak. This clock controls whether the camp may send any routine pair. Phase 5's `next_target_stalk_eligible_at` independently excludes a specific target; a same-target dispatch requires both clocks, while a genuinely different eligible target only requires the camp-wide clock.

Evidence defaults: significant sound 3h; smoke/light 6h; mobile threat 24h; human defenses/opportunity 3 days; failed route 72h; positive ground estimate 14 days; static terrain permanent; confirmed exhausted ground permanent. These are real durations, not fractions of a presumed 365-day year.

- [ ] Enable the same routine scan/outing machinery for bandit and cannibal camps.
- [ ] Replace the bandit-only singleton abstract timer with a persistent paired outing.
- [ ] Choose bounded frontier sectors and 3-5 waypoint routes using camp-local least-recently-observed memory.
- [ ] Prefer plausible roads, forest edges, rural sites, shelters, and town outskirts without treating terrain labels as confirmed safety.
- [ ] Give the party one shared high-level route and one movement owner.
- [ ] Implement the abstract/local ownership transaction: freeze the current owner; increment handoff generation; snapshot position/direction/phase/route/egress/HP/cargo/deaths; atomically spawn or bind the complete surviving pair at one plausible entry edge; then activate the new owner.
- [ ] On dematerialization, snapshot all local changes and stable NPC IDs before reactivating abstract advancement.
- [ ] Record `last_advanced_turn` and reject a second advance in the same owner generation.
- [ ] Roll back a partial spawn/bind failure without losing members or leaving both owners active; save/load of a committed handoff is idempotent.
- [ ] Use a leader/follower cohesion radius, rendezvous timeout, deterministic leader re-election, bounded reroutes, and abort-return.
- [ ] Define group arrival as the required surviving members assembled at staging, not first-member arrival.
- [ ] Ground bounty is finite and atomically depleted at the world resource.
- [ ] Wire the bounded current/legitimately-visible-forward-corridor abstract-threat adapter and one-shot detour/attrition resolver; the ordinary OMT observer may expose at most three committed route OMTs, an unseen horde cannot reroute scouts, an overlapped horde cannot be ignored, and abstract scouts never damage the horde without local simulation.
- [ ] Normalize existing ground bounty 1/2/3 to 333/667/1000; a surviving pair atomically takes at most one unit per survivor, so two camps contesting bounty 3 resolve as 2 then 1 rather than duplicating it.
- [ ] Ordinary ground bounty does not replenish; human-camp opportunity may recover through new observed activity/storage.
- [ ] Treat initial abstract bounty as ambient salvage unless actual map-item mutation is implemented; never claim visible player-loaded items vanished when no physical mutation occurred.
- [ ] Return abstract cargo to camp stockpile and derive scarcity pressure from real bounded fields rather than a permanent zero/test-only input.
- [ ] Each actually harvested ground-bounty unit adds two `supply_units`; paid shakedown loot adds `max(1, floor(surrendered_trade_value / 1000))` units, both clamped to the stock cap. No invisible replenishment occurs merely because time passed.
- [ ] If two camps reach one depleted OMT, only the first successful claim consumes it; the second reports an empty/stale lead.
- [ ] Keep the existing global start budget or replace it with an equally explicit bounded scheduler.
- [ ] Implement and persist the exact hourly 16-camp/eight-route/two-start scheduler cursor, including eventual-fairness and save/load tests.
- [ ] During this phase, prove route/resource ecology only. Legacy player radar may not supply credit for target discovery.
- [ ] Instrument the origin of every target lead/write (`legacy_radar`, new observer, signal, returned report) so Phase 4 can enforce a single-writer cutover.
- [ ] Run a live/harness pair-materialization proof now: plausible entry location, stable IDs, shared route/cohesion, and no duplicate abstract advance.
- [ ] Test loaded/unloaded boundary transitions while outbound, harvesting, regrouping, withdrawing, and returning.
- [ ] Test repeated load/unload thrash, partial pair spawn, one member dying during handoff, and save/load in both transition directions.
- [ ] Boundary-test `D=499/500`, `S=299/300`, retained `149/150`, and risk `749/750`; prove force-due does not bypass hard gates.
- [ ] Test supply pressure at just below/at 1, 3, and 7 member-days; daily and large-jump consumption; roster/cap changes; legacy seeding; returned bounty replenishment; and no unbounded catch-up loop.
- [ ] Test outward frontier coverage and legitimate discovery at target separations 10 and 12 OMT; every sector reaches 7-9 OMT by the eighth resolved frontier outing/24-day bound; cheap ranking performs zero full route solves and the global budget solves at most two candidates per considered camp.
- [ ] Test a bounty-3 resource where the first pair arrives with one survivor and atomically takes one unit, then a full second pair takes exactly the remaining two; replay/save-load cannot change the total.
- [ ] Boundary-test bandit bounty cues: occupied-only 0->1; one valid cue 1->2; a duplicate cue/source/bucket is a no-op; two distinct cue classes in distinct buckets or one quality-3 bulk-handling observation reach 3; light/smoke/hidden storage/exact item value alone do not raise it; newer clear-empty evidence returns it to 0 without erasing audit provenance.
- [ ] Boundary-test abstract danger: visible pre-entry versus dark/screened/off-corridor controls; two-detour cap; `danger_high` just below/equal to party power and just below/equal to twice party power; continuous overlap across hours/save/load/owner handoff applies one outcome; leaving then genuinely re-encountering creates exactly one new episode; local materialization and abstract resolution never both own one episode.
- [ ] Prove quiet targets inside the old radar radius create no candidate, decoy signals lead only to their uncertainty area, movement of the avatar does not drag an outing, and hidden camp recovery creates no new pressure without a returned post-event report.

### Phase 3 exit

- [ ] Both factions naturally send coherent paired outings without player proximity.
- [ ] Parties follow one plausible route and cannot double-resolve abstract/local work.
- [ ] Pair materialization/cohesion has live artifact proof in this phase rather than being deferred to Phase 8.
- [ ] Finite resource depletion is globally atomic and camp beliefs remain independent.
- [ ] CPU, allocation, and save-size microbenchmarks stay within the phase budgets.
- [ ] Behavior + tests form a checkpoint commit.

Evidence:

- Commit:
- Natural outing harness:
- Resource concurrency test:
- Reality-boundary test:
- Performance delta:

## Phase 4 - bounded perception, evidence aging, and removal of radar

Primary anchors: current direct-player dispatch lookup, signal adapters, `Character::overmap_los`, smoke/light/sound systems.

- [ ] Define a compact observation record: stable evidence/source ID, sense, observer ID, source/receiver OMT, time and 30-minute bucket, strength, visual quality, distinct defender IDs plus simultaneity window, normalized observed power/equipment detail, target revision, uncertainty radius, expiry, and shared status.
- [ ] Use legitimate OMT visibility with clear-day baseline around 3 OMT, intermediate visibility around 2, and unlit night around 1; let weather, light, terrain, elevation, and optics modify the real calculation.
- [ ] Use separate acquire/retain thresholds and last-known-location age so visibility does not flicker every update.
- [ ] Convert smoke and light into bounded evidence rather than exact avatar coordinates.
- [ ] Add only significant sounds (gunfire, alarms, explosions) initially; preserve uncertainty and age.
- [ ] Record actual local zombie/horde observations only when legitimately visible.
- [ ] Treat terrain danger as a prior and observed mobile danger as timestamped evidence.
- [ ] Introduce a temporary, test-visible single-writer cutover: observer/signal discovery may write while legacy radar is disabled; a legacy-only control may run separately, but both paths may never write the same live target revision.
- [ ] Prove autonomous observer/signal discovery for both factions, then remove exact `direct_player_range` targeting and active-player-OMT matching rather than leaving a permanent dual path.
- [ ] A quiet evac shelter inside the former radar radius remains undiscovered without a route, signal, or legitimate line of sight.
- [ ] Moving the player does not drag a stationary camp lead to the new avatar OMT.
- [ ] False/decoy smoke, light, and sound can produce stale/empty investigations rather than magical correction.
- [ ] Scouts share observations only while within party communication range; a dead scout's private unshared evidence does not return home.
- [ ] Add debug rendering/logging of last-known position, evidence provenance, age, and expiry.
- [ ] Run live/harness proof in this phase for quiet-evac no-radar, day/dusk/night/weather/optics, smoke/light/sound, target relocation, and decoy/empty leads.

### Phase 4 exit

- [ ] No live dispatcher reads precise player location without an observation boundary.
- [ ] Lead-origin instrumentation shows no dual write or legacy-radar contribution in the production path.
- [ ] Day/dusk/night/weather/signal tests prove bounded discovery.
- [ ] Stale and contradictory evidence remains honest.
- [ ] Evidence aging/pruning benchmark and two-year save soak remain bounded.
- [ ] Behavior + tests form a checkpoint commit.

Evidence:

- Commit:
- No-radar control:
- Visibility matrix:
- Evidence/save benchmark:

## Phase 5 - stalking, being burned, covert disposition, and evacuation

### Exact v1 stalking state machine

Use real game durations and deterministic integer updates: 30-minute evidence buckets; minimum normal observation 120 minutes; no-progress timeout 120 minutes; maximum observation 480 minutes; normal assessment-ready certainty 70 with release below 60; burned assessment-ready certainty 60 with release below 50; at most 16 observations per sortie/current report. Persist `observation_started_at`, `last_progress_at`, irreversible `burned_at`, `burn_origin_omt`, certainty `0..95`, readiness latch/threshold class, capped strong-visual-window count, defender/danger bounds, target alert `0..100`, pinned target revision, next eligibility, and exit reason.

Deduplicate evidence by `(sense, evidence_kind, source_id, 30-minute bucket)` and process stable keys, so two distinct defenders/facts in one bucket remain representable while true replay is idempotent. Confirmed presence adds 10 once; quality-3 clear/optical visual adds 20; quality-2 partial/dusk adds 15; poor quality adds 5 and never counts as a strong window; equipment/readiness detail adds 10 once; distinct legitimate light/sound/activity classes add 5 each capped at 10; contradictions subtract 15 each capped at 30; one burned close-contact snapshot adds 30. Cap visual contribution at 60 and total at 95. Polling the same evidence cannot improve certainty.

First ingest every legitimately perceived event for the current bucket in stable order, including a one-shot burn snapshot/alert latch, without changing phase more than once. Remote changes to the camp dossier never reach scouts in the field. Then evaluate exactly one exit transition per maintenance call in this order:

1. hard abort for invalid origin, impossible route, incapable/understrength party, legitimate recall, or immediate survival hazard;
2. overwhelming legitimately observed danger (`danger_low >= 2 * scout_party_power`) -> withdraw, informationally successful only if certainty is at least 50;
3. first burn latch -> capture the pair's current OMT as immutable `burn_origin_omt`, immediately enter `burned_withdrawal`, and classify the report assessment-ready at certainty 60+, otherwise partial;
4. a target revision change legitimately observed by the scouts -> return stale/contradictory evidence, never silently retarget;
5. normal success after at least 120 minutes, three quality-2/3 strong windows, certainty 70+, and `defenders_high - defenders_low <= 2`;
6. false/empty lead after three clear empty windows spanning at least two hours;
7. no progress for two hours -> travel to one persisted alternate watch point; start the second 120-minute no-progress window on arrival, then abort if still no progress;
8. eight-hour expiry -> inconclusive partial report.

A burn is legitimate mutual visibility/reaction, never a planner choice. Being burned improves the report while increasing approach risk and ending the observation phase. Attack, robbery contact, or exchanged fire sets alert to 100 and ends stalking. Report certainty is unpenalized to 12h, -10 at 12-24h, -20 at 24-48h, and unusable for attack authorization at 48h. Normal reports latch assessment-ready at 70 and release below 60; burned close-contact reports latch at 60 and release below 50. Authorization consumes the readiness latch plus freshness/hard gates, not a contradictory second `>=70` check.

Same-target cooldowns (`next_target_stalk_eligible_at`): successful normal 48h; successful/partial burned 48h; inconclusive 12h; danger abort 24h; route failure 6h; empty/false lead 72h. Only a genuinely newer lead revision with at least 20 new evidence strength may bypass it. Target alert decays by 10 per 12h without new contact, anchored/reset by the latest contact.

Primary anchors: `live_bandit_make_gate_input`, ordinary standoff helper, local sight-avoid evaluator, `npc::move`, `npc::set_attitude`, faction hostility.

- [ ] Define and test the watch-ring metric explicitly (including diagonals) from the nearest OMT in the actual target camp footprint/perimeter, not an avatar position or arbitrary anchor.
- [ ] Select a reachable/concealed watch OMT on the distance-3 ring so the chosen approach line contains two intervening empty OMTs when geography permits.
- [ ] If an exact ring position is impossible, choose a justified farther watch point or abandon; never collapse to the target window.
- [ ] Keep pair cohesion and plausible observer/cover slots within the watch OMT.
- [ ] During searching, observing, burned-withdrawal, returning-exposed, and target-report return until every survivor is outside the target's ordinary acquire/visibility range, apply an actor-specific covert non-combat relationship toward the target camp.
- [ ] Suppress red hostility, generic kill-on-sight targeting, and `gets angry` only for that narrow covert relationship.
- [ ] Player attack, shakedown refusal, or committed raid immediately ends covert status.
- [ ] Detect exposure/burning through legitimate visibility/reaction rules.
- [ ] Process burning before generic NPC hostility/movement in one atomic party update: capture the observer-specific perception snapshot; detect exposure once; append close-contact evidence/alert once; leave `observing`; persist egress+rally; enter `burned_withdrawal`; then permit local NPC targeting/attitude/movement.
- [ ] Being burned ends observation/stealth but retains the narrow covert non-combat disposition during exfil. Open hostility still begins only at committed contact, refusal, or player attack.
- [ ] The planner cannot intentionally choose to be burned for the evidence bonus.
- [ ] Replace adjacent corrective movement with a persistent egress/rally route off `burn_origin_omt` and away from the target footprint; the burn origin is normally the distance-3 watch OMT, not the target OMT.
- [ ] Score reachable exits against observed threats and concealment; use hard/soft danger avoidance.
- [ ] Immediate survival hazards can override the squad task; trapped scouts defend or fight through the least dangerous exit.
- [ ] Bound retries and remember failed egress tiles/routes to prevent oscillation.
- [ ] Define egress completion: every surviving member is off `burn_origin_omt` and the target footprint, reassembled at a persisted rally where ordinary current perception gives no mutual visibility/acquire condition, and barred from re-entry until a new camp-authored operation/report revision. If open terrain preserves visibility, extend the committed route by at most one OMT on each of three bounded attempts; after that, transition to `returning-exposed` and continue home without re-entering or oscillating rather than waiting forever for a perfect hidden tile.
- [ ] Persist failed exits plus minimum route commitment/hysteresis so bubble transitions and visibility changes cannot reset the party to `observing`.
- [ ] Run live/harness proof in this phase for visible burn -> exactly one evidence/alert transition -> coherent off-OMT egress -> rally, with no anger and no dance.
- [ ] Cover save/load immediately before and after burn; burn on the last local tick before unload; abstract continuation of the same egress; repeated load/unload; only one loaded member; member death during handoff; leader death; slow/injured follower; no legal exit; soft-danger escape; defender asleep/blind/behind cover; and darkness/weather changing mid-watch.
- [ ] Boundary-test normal success at exactly 120 minutes versus 119; same-bucket replay; poor-night polling; three empty windows; one alternate-watch attempt; eight-hour expiry; target revision replacement; 69/70/60 assessment hysteresis; every cooldown; and large-time-jump equivalence to stepwise maintenance.
- [ ] Test simultaneous burn+danger, burn+survival hazard, locally observed revision-change+success, and empty+expiry: all perceived evidence is ingested once, exit priority is deterministic, and only one phase transition occurs.
- [ ] Test clear-empty semantics across the whole visible target footprint: consecutive distinct windows, occlusion/partial visibility, intermittent presence reset, and save/load while travelling to the one alternate watch point.
- [ ] Test exact 12h/24h/48h report aging and 12h alert decay/reset under stepwise and large-time-jump advancement.

### Phase 5 exit

- [ ] Visible covert scouts do not appear openly hostile or announce anger.
- [ ] A burned party gains evidence, coherently evacuates its `burn_origin_omt`, and does not dance.
- [ ] Reload or owner handoff cannot reset `burned_withdrawal` to `observing` or add duplicate burn evidence.
- [ ] Slow/injured follower, leader death, blocked bridge, no-exit, and save/load-mid-egress tests pass.
- [ ] Local movement profiling shows no per-turn pathfinding/replan storm.
- [ ] Behavior + tests form a checkpoint commit.

Evidence:

- Commit:
- Burn/egress harness:
- Hostility presentation proof:
- Movement trace:

## Phase 6 - physical reports, assessment, and escalation math

- [ ] Keep observations on the outing until a survivor reaches home.
- [ ] Merge only shared/surviving evidence into a new immutable report revision.
- [ ] Preserve observation timestamps and ranges; do not collapse `two seen` into `exactly two now`.
- [ ] Derive visible defender power from existing character weapon/armor/health/readiness evaluation where possible.
- [ ] Represent unobserved defenders/equipment as uncertainty, not zero and not omniscient truth.
- [ ] Track certainty, defender-count bounds, power bounds, bounty estimate, route danger, target alert, report age, and losses.
- [ ] Burned contact improves certainty while increasing target-alert/approach risk.
- [ ] Normalize legitimately observed defender power through the existing combat evaluator to integer `1..10` per visible defender; normalize simultaneous static hazards and route danger separately to `0..20`. For each legitimate perception snapshot inside one 30-minute evidence bucket, record the distinct simultaneously visible defender IDs, `count_w`, `power_w`, and `static_hazard_w`. Across retained windows set `defenders_low = max(count_w)`, `danger_low = clamp(max(power_w + static_hazard_w), 0, 140)`, and `observed_unit_power = clamp(max(ceil(power_w / max(1, count_w))), 3, 10)`, with stable timestamp/source tie-breaks. If a report has legitimate signals/cues but no visible-defender window, use `defenders_low = 0`, `danger_low = clamp(max legitimately observed static_hazard_w, default 0)`, and `observed_unit_power = 3`; never manufacture a defender count or power observation. Never sum identities or power across different windows. If any one snapshot contains more than 12 defenders, mark the target hard-unsafe and retain that true simultaneous lower bound rather than truncating it.
- [ ] Otherwise add uncertainty slots by certainty (`<40:4`, `<60:3`, `<80:2`, else `1`), set `defenders_high = min(12, defenders_low + unknown_slots)`, and set `danger_high = clamp(danger_low + unknown_slots * observed_unit_power + route_danger_high + ceil(target_alert / 20), 0, 200)`. Phase 3 risk is exactly `X = 5 * danger_high` clamped to 1000.
- [ ] Compute follow-on party power against a pessimistic target estimate and faction-specific safety margin.
- [ ] Reserve a real home defense for follow-on operations: `home_reserve = max(1, ceil(living_total / 3))` must be selected from ready, physically present, unreserved capable defenders actually left at home. Response capacity is the remaining ready/present/unreserved members, with a minimum response party of two and a v1 cap of six. Select the smallest capable party that clears the power gate rather than always emptying the camp.
- [ ] V1 authorization gates: assessment-readiness latch set under its normal/burned threshold, unexpired/unacted target revision, normalized faction opportunity at least 600, and `100 * selected_response_power >= 125 * danger_high` for a bandit shakedown or `>= 150 * danger_high` for a cannibal lethal raid. Response member power uses the same integer `1..10` combat normalization. These are centralized engineering defaults subject to the explicit Phase 0 review, not balance values attributed to Josef.
- [ ] Hard reserve/readiness requirements gate action; they are not score penalties that a valuable target can override.
- [ ] If the camp cannot afford the response, hold, rescout after expiry, or abandon.
- [ ] An overdue total loss produces only missing-route knowledge, never the dead party's dossier.
- [ ] One survivor can return a provisional partial/shared report and leave the other member unresolved. The sortie and external-operation slot remain active, and no response operation may launch yet.
- [ ] Persist `expected_return_at = departure + outbound_route_eta + phase_budget + return_route_eta + 2-hour cohesion buffer`, using the same route-cost/speed estimator as movement and the phase's bounded 8-hour observation or configured harvest budget. Set unresolved-member deadline to `expected_return_at + 24 hours`. At the deadline, atomically mark unresolved members missing/away, release only matching reservations, finalize the available report, close the sortie slot, and allow assessment. Large time jumps produce the same one-shot result.
- [ ] Define split-arrival semantics: the first survivor can atomically credit only that survivor/cargo/shared evidence; an on-time later survivor completes the sortie and may finalize a newer report revision. A survivor returning after the deadline/newer operation may credit only its still-unapplied member/cargo and create a newer dossier revision for future decisions; it cannot mutate a response operation already pinned to an older report or duplicate earlier credit.
- [ ] Replay delivery after saving between resource depletion, cargo credit, member return, and report merge is idempotent.
- [ ] Age each fact from its observation timestamp, not report creation or home delivery; late return never refreshes old intelligence. Record creation/delivery timestamps separately for audit.
- [ ] Cache expensive assessment/path inputs and invalidate on relevant new reports, casualties, equipment/readiness changes, target revision, or route threat.
- [ ] Record the complete assessment equation and normalized input table in code/tests/debug output; bounty never overrides the hard readiness/reserve gate or overwhelming `danger_low`.
- [ ] Test exact `100*power == 125*danger_high` / one below and `==150*danger_high` / one below, plus away/sleeping/wounded/incapable/reserved home defenders and a legitimately observed defender count over 12.
- [ ] Test first return -> save/load -> on-time second return; deadline -> late return; old-generation packet after a newer operation; replay after pruning/watermark; and newer home dossier revision while scouts remain pinned without remote knowledge.
- [ ] Test two distinct same-bucket visual facts both survive while an exact duplicate is a no-op.
- [ ] Test alternating defender windows: three weak defenders together then one strong defender, one strong then three weak, identities changing across patrol shifts, equal-count tie-breaks, save/load, and a signal/cue-only report with no visible defender window. `defenders_low`, `danger_low`, and `observed_unit_power` use per-window maxima or the explicit `0`/observed-static-hazard/`3` empty-window defaults without ever summing identities or power across time.

### Phase 6 exit

- [ ] Every escalation names the report revision and utility inputs that justified it.
- [ ] No-return and partial-return tests pass.
- [ ] Assessment is stable under tiny score changes and does not alternate each tick.
- [ ] Assessment/caching benchmark stays inside the agreed budget.
- [ ] Behavior + tests form a checkpoint commit.

Evidence:

- Commit:
- Report tests:
- Decision trace:
- Benchmark:

## Phase 7 - faction outcomes and aftermath

### Bandits

- [ ] A favorable returned report can create a later assembled shakedown party.
- [ ] Exact reachable stored-goods value is calculated only at real contact, not through binoculars.
- [ ] At contact, value actual eligible items in loaded, reachable player-camp storage zones/containers using the existing trade/value rules. Do not demand a percentage of theoretical zone capacity, inaccessible unloaded storage, worn gear, or items the party cannot physically take.
- [ ] Preserve the existing intended demand exactly: if reachable eligible stored value is positive, base demand is `ceil(35 * reachable_value / 100)`. Apply `shakedown_demand_modifier_percent`: 140% exactly when `shakedown_reopen_available && !shakedown_reopen_used`; otherwise `max(50, 100 - 25 * max(shakedown_caution, shakedown_bandit_losses))` when either field is positive; otherwise 100%. Ceil the modified value, then clamp to `1..reachable_value`. If eligible reachable storage is zero, offer no impossible Pay branch and record empty opportunity; do not demand one unit from nothing. Boundary-test every modifier field/state, one-time reopen consumption, rounding, clamp, and zero storage.
- [ ] Payment records structured time, report revision, observed opportunity, amount taken, and next eligibility.
- [ ] Replace permanent summary-string immunity with cooldown plus genuinely newer evidence of recovered bounty.
- [ ] Refusal/attack transitions the party to genuine hostility once, without duplicate messages.
- [ ] Bandits prefer a renewable surviving target when policy says robbery, not extermination.

### Cannibals

- [ ] A favorable returned report creates a later assembled raid party, not an immediate scout conversion.
- [ ] The raid waits at the staging ring for actual darkness.
- [ ] Define actual darkness as `is_night(now)` plus maximum ambient light along the loaded approach/contact tiles at or below `LIGHT_AMBIENT_DIM`; a lit camp can therefore delay/defeat stealth even at midnight.
- [ ] Set `staging_deadline = min(next_dawn, arrival + 8 hours)`. Once darkness is present, permit at most two hours for ordinary target-footprint/resident realization and safe approach. If the target never loads/realizes, light stays too bright, or dawn arrives before commitment, abort/exfil; never wait indefinitely or launch a daytime magic attack.
- [ ] Committed combat targets all loaded camp defenders through ordinary hostile combat AI.
- [ ] Before commitment, require the target camp footprint/contact area to be loaded and its legitimately assigned residents to be realized through ordinary camp/NPC loading. Hold/abort if that cannot be established; never teleport an offscreen resident into combat or resolve a silent offscreen death.
- [ ] No payment UI is available.
- [ ] An empty target does not retarget to the avatar's current location.
- [ ] No unloaded/offscreen resident is silently killed.

### Aftermath

- [ ] Casualties, wounds, cargo, morale, target-alert state, and route threat return through the persistent packet.
- [ ] Consider a deferred casualty-site record so offscreen deaths can later materialize corpses/dropped cargo at the real encounter OMT.
- [ ] If casualty residue is deferred beyond v1, document the exact limitation rather than faking physical loot.
- [ ] A second hostile party observing an active encounter treats it as danger and holds/withdraws; no invisible inter-camp battle simulation in v1.
- [ ] Human-camp bounty renews only through a returned report revision whose relevant observation occurred after the last acted event, with effective report certainty at least 60 on the `0..95` stalking scale and opportunity at least 600/1000. This renews the dossier; a follow-on still requires its normal-70 or burned-60 assessment-readiness latch plus every Phase 6 hard gate. Retain an already pending pressure decision down to 450/1000 for hysteresis.
- [ ] Time alone never retriggers pressure. Minimum outcome cooldowns are bandit paid/success 3 days, bandit repelled/loss 7 days, cannibal success 5 days, and cannibal repelled/loss 10 days; a pre-event observation delivered afterward is ineligible.
- [ ] Add a faction-readability inspection and blind screenshot/nameplate test. Bandit and cannibal scouts must be distinguishable at ordinary encounter distance through at least two of silhouette/loadout, naming/speech, or behavior. If the current data fails, make the smallest data-only loadout/presentation repair; do not fork the shared AI merely for cosmetics.

### Phase 7 exit

- [ ] Full natural bandit loop passes: scout, discover, stalk, return/report, assess, later shakedown, pay/fight, aftermath, bounded repeat.
- [ ] Full natural cannibal loop passes: scout, discover, stalk, return/report, assess, later night wait, all-defender attack/abort, aftermath.
- [ ] Old-report/payment race and dawn/empty-target controls pass.
- [ ] Storage-zone reachability, resident realization, faction-readability, 599/600 opportunity, 449/450 pending hysteresis, and every outcome cooldown boundary pass.
- [ ] Test a full 16-record evidence buffer followed by burn/hard-danger/casualty; priority evidence survives deterministic compaction.
- [ ] Test a cannibal operation whose target never loads before dawn, a permanently lit target, darkness exactly at `LIGHT_AMBIENT_DIM` versus just above it, the two-hour realization deadline, and the eight-hour/dawn staging cap.
- [ ] Performance and save deltas remain within budget.
- [ ] Behavior + tests form one or more bounded checkpoint commits.

Evidence:

- Commits:
- Bandit lifecycle proof:
- Cannibal lifecycle proof:
- Aftermath/save proof:

## Phase 8 - debugability and deterministic proof suite

Phase 8 aggregates and soaks the live proofs already required at Phases 3-5. It must not be the first time pair materialization, no-radar discovery, or burned evacuation is exercised in the actual game path.

- [ ] Add a compact debug surface showing camp ID, outing ID, phase, members, leader, route, waypoint, cohesion, target lead/revision, evidence, last-known target, utility inputs, last transition reason, and next eligible transition.
- [ ] Ensure debug data is generated on demand or cheaply; no release-mode per-turn string accumulation.
- [ ] Extend the existing playback packet with the new lifecycle states, but do not confuse semantic `benchmark_100/500` checks with real CPU benchmarks.
- [ ] Add natural ecology harness scenarios rather than relying only on manually injected high-confidence/high-bounty fixtures.
- [ ] Test party-size matrix and readiness for both factions.
- [ ] Test quiet target/no-radar, smoke, light, sound, day/dusk/night, weather, obstacles, and optics.
- [ ] Test known horde detour, stale horde memory, danger ring with one exit, and unseen threat.
- [ ] Test cohesion, early-arrival non-contact, regroup, leader death, blocked route, and abort.
- [ ] Test burned assessment/withdrawal/no-dance/no-anger.
- [ ] Test two deaths/no report and one survivor/partial report.
- [ ] Test finite resource concurrency and stale private beliefs.
- [ ] Test abstract/local transition and save/load during every phase.
- [ ] Test old report after payment, target relocation, decoy signals, dawn, empty target, and overlapping hostile parties.
- [ ] Remove or clearly mark temporary instrumentation that is not part of durable debugging.

### Phase 8 exit

- [ ] One deterministic command runs the narrow unit/integration packet.
- [ ] One harness packet proves each visible behavior with local artifacts, not merely OCR or log-string matches.
- [ ] Debug output makes every dispatch/transition explainable from saved evidence.
- [ ] Tests and debug tooling form reviewable commits.

Evidence:

- Commits:
- Unit packet:
- Harness artifact manifest:
- Remaining yellow/red proof:

## Phase 9 - detailed performance, memory, and save-bloat qualification

This phase is a release gate. Passing functional tests does not waive it.

### Reproducibility controls

- [ ] Stop transfers, indexing-heavy work, concurrent builds, and other Codex compiles during measurements.
- [ ] Use a release-equivalent build, fixed seed, fixed season configuration, identical compiler/flags, and the exact intended binary.
- [ ] Restore and hash a pristine fixture before every warmup and measured run; warmup may not mutate the subsequent measured world.
- [ ] Run at least ten randomized paired A/B or B/A repetitions for macro totals; retain all raw pairs and report dispersion/95% confidence intervals.
- [ ] Gather at least 10,000 scoped update observations where feasible for p50/p95/p99/max; do not calculate tail percentiles from ten run totals.
- [ ] Compare the identical benchmark-only instrumentation patch on the preserved pre-change baseline and final feature build on the same Mac.
- [ ] Keep the Mac plugged in/fixed power mode and record thermal/power/background-load state.
- [ ] Predeclare run rejection: wrong fixture hash/RNG state, wrong process/binary, another transfer/build/profile starts, target reports an error, or recorded thermal/background-load threshold is crossed. Do not discard a merely slow valid sample.

### CPU timing matrix

- [ ] Microbenchmark party selection/reservations for 1/10/50/100 camps.
- [ ] Microbenchmark evidence insertion, merge, aging, and prune at empty/normal/capacity state.
- [ ] Microbenchmark resource lookup/depletion with competing camps.
- [ ] Microbenchmark route creation, cached reuse, threat invalidation, soft detour, and failed-route abort.
- [ ] Microbenchmark report validation/merge and assessment scoring.
- [ ] Benchmark the complete hourly maintenance loop with idle camps and maximum permitted new outings.
- [ ] Run near/far distribution fixtures at 0/1/10/50/100 camps. Distant, unloaded, and irrelevant camps must not trigger per-avatar-turn global perception/pathfinding scans; report work by active versus dormant camp.
- [ ] Benchmark a synchronized worst-case hourly burst where dispatches, evidence expiry, report returns, pruning, and resource claims become due together.
- [ ] Benchmark overdue catch-up after save/load, long sleep/wait, and 30-day/one-year/two-year calendar advances. Work must be bounded or amortized so a backlog cannot create one unbounded main-thread stall, while the terminal state remains equivalent to stepwise advancement.
- [ ] Benchmark abstract/local materialization and dematerialization of paired parties.
- [ ] Benchmark open terrain and threat-dense/route-invalidating terrain separately so pathfinding cost is not hidden by an easy map.
- [ ] Benchmark loaded local AI for 0/1/4 visible pairs in observing, burned withdrawal, blocked-exit, and repeated bubble-crossing states; report avatar-turn p50/p95/p99/max, NPC move calls, path requests, route invalidations, replans, and allocations.
- [ ] Assert the loaded-bubble budgets and a bounded per-pair replan/path-request rate; a correct abstract scheduler does not excuse an on-screen NPC movement storm.
- [ ] Benchmark a 24-hour wait simulation near no hostile camps, near one camp, and in a 10-camp stress region.
- [ ] Report scoped hostile-maintenance inclusive/self time separately from whole-simulation macro time so unrelated CDDA work cannot hide a regression.
- [ ] Report run-level paired totals/intervals and per-update p50/p95/p99/max, call count, cache-hit rate, and pathfinding-call count.

### Instruments attribution

- [ ] Confirm available templates with `xcrun xctrace list templates`.
- [ ] Record a Time Profiler trace around the deterministic stress driver or exact game PID.
- [ ] Collect untraced release timing first. Never compare traced timing directly against an untraced baseline; reproduce a regression separately for sampling attribution.
- [ ] Verify the process path with `ps` before trusting an attached trace.
- [ ] Export the time-profile table with `xcrun xctrace export` and aggregate app frames outside the terminal.
- [ ] Identify CPU percentage and call counts for hostile maintenance, pathfinding, perception, JSON, allocation, and NPC realization.
- [ ] Capture Allocations/Leaks plus periodic RSS/live-heap samples during fixed-cardinality churn and repeated materialize/dematerialize cycles; `/usr/bin/time -l` peak RSS alone is insufficient.
- [ ] Keep `.trace`/large XML artifacts outside Git; retain a small manifest and summarized top stacks.

### Save and memory matrix

- [ ] Measure raw serialized hostile-AI bytes at 0/1/10/50/100 camps and 0/normal/max retained state.
- [ ] Report count and bytes separately for camps, scout sorties, hostile operations, resources/depleted OMTs, dossiers, reports, observations, routes, failed exits, and debug state.
- [ ] Measure both uncompressed component bytes and actual compressed whole-world on-disk bytes; identify the incremental master-save contribution rather than relying on compression to hide redundant state.
- [ ] Measure save and load wall time for empty, normal, stress, and long-soak states.
- [ ] Define a versioned soak schedule with exact event turns/counts for scouting, burn, payment, failed raid, casualty, target relocation, discovery, prune, save, and load; assert expected terminal cardinalities before interpreting bytes.
- [ ] Run that schedule through 30 days, one configured year, and two configured years in both fixed-world plateau and sustained-new-OMT exploration variants.
- [ ] Run a scan-only exploration soak over at least 100,000 empty/default or unharvested OMTs and assert zero default global resource records, bounded per-camp dossiers, and the approved fixed metadata ceiling.
- [ ] Plot state bytes versus elapsed time and camps; demonstrate a plateau after caps/expiry.
- [ ] Demonstrate that doubling camps is approximately linear and that old completed outings/reports are pruned or compacted.
- [ ] Verify round-trip correctness at every measured point; a small save that loses state is a failure.
- [ ] Verify fixed-cardinality CPU and retained-memory curves plateau after warmup; positive slopes require attribution even when final peak remains under a cap.

### Regression investigation

- [ ] If any budget fails, obtain a trace before optimizing.
- [ ] Attribute the regression to a specific call path/data structure.
- [ ] Fix the cause rather than weakening cadence/behavior invisibly.
- [ ] Rerun the full matrix after optimization.
- [ ] If a justified budget change remains necessary, present exact before/after tables and player-facing tradeoff to Josef.

### Phase 9 exit

- [ ] All agreed performance/save budgets pass, or Josef explicitly accepts a documented exception.
- [ ] Raw results, environment manifest, plots/tables, trace summary, and commands are archived.
- [ ] No significant unexplained CPU spike, allocation growth, or save-growth slope remains.
- [ ] `TESTING.md` records the latest representative evidence and pending human probes without becoming a log graveyard.
- [ ] Any optimization/profiling changes form bounded checkpoint commits.

Evidence:

- Baseline vs final table:
- Raw artifact manifest:
- Time Profiler summary:
- Memory/allocations summary:
- Save-growth table:
- Accepted exceptions:

## Phase 10 - platform and packaged-build engineering qualification

- [ ] Mac native build and narrow/full relevant test packets pass.
- [ ] Linux/WSL changed-file compile and relevant tests pass.
- [ ] Windows changed-file compile and relevant tests pass.
- [ ] Through explicitly authorized isolated validation paths, run a lighter paired runtime/save packet on Windows and Linux: 0 versus 10 camps over the representative 24-hour macro, 0 versus one loaded pair, raw hostile-state bytes, whole-save bytes, and save/load wall time. Compare before/after on the same platform; do not compare absolute Mac/Windows/Linux timings as if hardware were identical.
- [ ] Broader Windows/Linux builds run only when integration risk warrants them, with redirected logs and first-error parsing.
- [ ] Locally produced release-equivalent Mac, Windows, and Linux packages include the required data/mod content and standard Ultica tileset route, and launch without depending on a development checkout.
- [ ] The Windows and Mac manual-harness routes inherit a working API key from their platform secure stores without a per-run shell export, use a native runner environment, and never print the key. Linux package validation proves the default/native runner path is not overwritten by a Windows or Mac absolute path; release users still supply their own provider credential.
- [ ] Statically validate Catapult-Dabubu's artifact naming/URL/platform-selection contract against the produced manifest; actual download of the newly published GitHub artifact belongs to the separately authorized release-acceptance phase.
- [ ] Fresh playtest world starts in an evac shelter; no stale hostile NPC from an old save is present.
- [ ] The observation fixture contains the five intended release families at the recorded bands, outside the initial reality bubble and not directly at the starting windows; only bandit/cannibal lifecycle behavior is an engineering claim of this package.
- [ ] A separate new-world harness row proves naturally generated bandit and cannibal camp registration and autonomous paired scouting without fixture-injected target confidence/bounty.
- [ ] Agent-side Mac harness/playback exercises the natural discovery, stalking, withdrawal, report, assessment, shakedown, and night-raid lifecycle from the packaged-equivalent content path.
- [ ] Review all diffs, checkpoint commits, repo ledgers, and dirty state on both machines.
- [ ] Produce a concise `ready for Josef` handoff with build identities, fixture/save location, expected observations, known caveats, and performance artifact manifest.

### Engineering success state

- [ ] The complete natural lifecycle works for both factions.
- [ ] Scouts feel like a coherent pair with bounded knowledge and credible survival behavior.
- [ ] Bandits rob; cannibals wait for night and attack the camp population.
- [ ] No player-coordinate radar, window dancing, false anger, magic reports, duplicate resources, or offscreen avatar substitution remains.
- [ ] Performance, memory, save-size, save-time, and load-time evidence is detailed and within the accepted budgets.
- [ ] Mac has the full performance matrix; Windows and Linux have representative runtime/save packets; all three have package and Ultica engineering proof.
- [ ] All checkpoint commits are reviewable, and both repos end clean at the intended commit.

When every engineering item above is checked, the long-running implementation goal may be completed as **engineering-complete and ready for Josef**. That status is not a release decision.

## Phase 11 - Josef product/release acceptance (external, nonblocking for the engineering goal)

- [ ] Josef completes an ordinary extended Windows play session, not only targeted debug scenarios.
- [ ] Collect timestamped debug notes for discovery, stalking, withdrawal, shakedown/night raid, general feel, and any apparent turn-time hitch.
- [ ] Compare any human-observed hitch against the saved performance traces/instrumentation and open a concrete follow-up if needed.
- [ ] Record Josef's product judgment on scout liveliness, distance, danger, faction distinction, cadence, and whether the feature feels intrusive.
- [ ] After Josef separately authorizes GitHub mutation/publication, upload or publish the intended platform artifacts and prove Catapult-Dabubu downloads and launches those GitHub-built artifacts rather than a development checkout.
- [ ] Do not push, publish, or declare release-ready until Josef gives the explicit release decision.

## Decision log

Record only material contract changes.

| Date | Decision | Evidence/reason | Approved by |
|---|---|---|---|
| 2026-08-01 | V1 camp-backed routine scouts are exactly two: camp size 2 sends both and all larger eligible camps send two. A named trio policy is deferred rather than being an automatic danger fallback. | Josef's `two should be standard` decision, narrowed to a deterministic first production slice. | Josef |
| 2026-08-01 | Being burned accelerates assessment but forces withdrawal and raises target-alert risk. | Close contact provides better information but ends stealth. | Josef |
| 2026-08-01 | Both factions share scouting/ecology; bandits rob and cannibals wait for darkness and kill. | Avoid duplicate AI stacks while preserving faction identity. | Josef |
| 2026-08-01 | Detailed CPU/memory/save-bloat proof is a release gate. | Overmap AI must remain bounded over long worlds. | Josef |
| 2026-08-01 | Writhing-stalker and zombie-rider AI/progression are separate design discussions. | Avoid scope-merging individual predator logic into faction camp ecology. | Josef |
| 2026-08-01 | No cron/watcher loop; one xhigh goal owns state and delegates bounded work. Apple/TCC/password blockers produce one secret-free OpenClaw Discord relay and a safe pause. | Durable orchestration without unattended approval loops. | Josef |
| 2026-08-02 | Close Phase 0 with one regression-grade matrix: three paired normal runs, one bounded 500-site stress category, phase RSS, real whole-save, conservative provisional budgets, and honest caveats. Defer publication-grade intervals, allocation attribution, and long forward-schema soaks to their implementing phases/Phase 9. | Phase 0 had consumed enough time; working deterministic ecology is the priority. Official packet `7332059a...`, manifest `ff410e9b...`. | Josef |

## Source/review anchors

Local symbols to re-audit before edits:

- `bandit_live_world::site_record`
- `bandit_live_world::camp_intelligence_map`
- `bandit_live_world::choose_camp_map_dispatch`
- `bandit_live_world::plan_structural_bounty_outing`
- `bandit_live_world::plan_site_dispatch`
- `bandit_live_world::apply_return_packet`
- `bandit_live_world::scout_sortie_should_return_home`
- `bandit_pursuit_handoff`
- `live_bandit_make_gate_input`
- local sight-avoid integration in `do_turn`
- `overmap_path_params::for_npc`
- `npc::move`, `npc::set_attitude`, and faction hostility evaluation
- savegame member `bandit_live_world`
- existing playback `bandit_overmap_benchmark_suite_packet_v0` (semantic, not a CPU benchmark)

External primary references worth retaining:

- [Unreal AI Perception documentation](https://dev.epicgames.com/documentation/unreal-engine/ai-perception-in-unreal-engine?lang=en-US): stimulus age/expiry, strength, stimulus location, receiver location, and sensed/lost state support explicit evidence rather than live target truth.
- [Project Zomboid's official `Zuckerverse` metaworld notes](https://projectzomboid.com/blog/news/2022/03/the-zuckerverse/): simplified event-driven offscreen actors, meta-to-real transitions, and a stated 500-NPC stress test support a cheap abstract layer plus explicit materialization proof.
- [Jeff Orkin's F.E.A.R. GDC paper](https://www.madwomb.com/tutorials/gamedesign/prototyping/gdc2006_JeffOrkin_AI_FEAR.pdf): working memory, replanning after failed paths, and separation between squad-level and individual decisions support bounded camp plans plus actor survival overrides.
- [Wesnoth's official RCA AI documentation](https://wiki.wesnoth.org/RCA_AI) and [AI configuration reference](https://wiki.wesnoth.org/AiWML): threat-weighted retreat, reachability, exposure/caution, and avoid regions are useful comparators for soft/hard danger scoring.
- [OpenRA's source repository](https://github.com/OpenRA/OpenRA) is an optional squad-management archaeology target; inspect the current bot code directly before borrowing a rule. No OpenRA behavior is part of this contract merely because it sounds similar.
