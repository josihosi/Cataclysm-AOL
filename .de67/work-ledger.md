# Current work ledger

## Parked for Josef

- **In-world radio conversations — locked.** [Radio play WEC](radio-play/WEC.md) and [frozen radio FS](radio-play/FS.md) are ready for Josef. The design notes are in [Human todo](human-todo.md). The coordinator continues the current testing program.

- **NPC AI Lab — locked by Josef.** Only Josef’s explicit instruction to open the lab authorizes importing the [secondary FS](npc-ai-lab/FS.md) into the delivery roadmap. Until then, continue the current work. Leave the lab outside task selection, assignment, implementation and playtests. Completing the current work does not open it.

## Phase 2 preparation — seeded overmap AI campaign, 2026-11-22

The sole incoming WEC was moved exactly to `.de67/WEC.md` (SHA-256
`9e15d3739f7073cd842adcecca3f42f0c25924801411b7e3f9f8d0a5b8820220`).
Canonical `.de67/FS.md` contains eight new unaccepted campaign claims and 79 retained historical
claim/slice identities. All 69 old checked claim lines and 24 delivery-status receipt lines are
preserved with their original scope in the verified Mac archive. SQLite remains authoritative,
including invalidations. No prior acceptance gives new campaign credit. The ledger was consumed
and had no remaining checked entries before this preparation.

Source baseline: `dev`, `876a6c48e226b27db90c9d443f1b6d13fa6c698d`; upstream `origin/dev`.
The archive's difference from the prior active FS was exactly the six withdrawal lines in that
commit; reversing just those changes reproduced the archive hash. Both supplied archives remain
outside this checkpoint. Existing unrelated dirty product/test/harness/policy files are preserved.
Existing soak executable `cataclysm-v2-soak-20261122` was observed at PIDs 43628 and 3291;
this phase neither sends input nor manages those owners' processes or saves.

### Active context audit

- Host `/Users/josefhorvath/.codex/AGENTS.md` and project `AGENTS.md` supply local workspace,
  ownership, process-diagnostic, cleanup, proof and upstream rules. `Agents.md` resolves to the
  same case-insensitive root file. Retain them. The Phase-2 owner received both; no delivery
  coordinator, worker or reviewer context was launched by this phase. Capability-only probes
  are recorded separately below and do not establish delivery-context assembly.
- The existing FS was a historical product/acceptance source with superseded execution prose.
  Preserve its stable identities and scoped receipts in the new canonical FS, with the exact
  verified archive supplying full historical scope. The old WEC is not new intent. Current
  live/paused owners retain their existing assignment and archived contract; this phase grants
  no authority to resume or replace them.
- `work-ledger.md` was an empty consumed queue. `test-and-task-guidelines.md` describes itself as
  a legacy differential fixture and names `policy_kernel.py` plus compiled policy as lifecycle
  authority. Its useful ownership/proof facts are retained; it is not Phase-2 dispatch authority.
- `.de67/phase3-policy.d67`, `.de67/phase3-policy.json` and `.de67/phase3-contracts.json` are
  existing dirty compiled policy/contract inputs. The JSON names clock/ledger/FS-driven lifecycle
  decisions; the binary policy is not prose. Retain their owner and bytes. Do not rewrite them
  to fit the newly read portable skill.
- `.de67/coordinator-guidance.md` is existing delivery guidance with current GPT-6.2 Sol selection
  and dated release planning. `.de67/owner-context.md` explicitly pauses delivery and retains
  GPT-6.2 Sol/Astra migration and active-work custody. Neither commissions this campaign. Their
  historical FS subsection references resolve through the preserved slice and verified archive;
  they are not silently promoted into current WEC requirements. No context file was neutralized
  or moved. Any runtime compatibility conflict must be resolved by its owner, not a phase reset.
- `.de67/mutation-suggestions.md` is a pre-existing dirty deferred queue. It is not new owner
  intent for this phase and remains unchanged/uncommitted here. The mutator guidance applies
  to acting mutators; this specification pass performs no mutation review or recovery.
- `.agents/skills/caol-harness/SKILL.md` and its directly relevant live/setup/search/manual routes
  govern later native ownership and evidence. This phase inspected their scope and actual harness
  entrypoints without running a scenario. `doc/overmap-ai-field-guide.md` supplies maintained code
  navigation, not new product intent; the FS links it and requires changed caller answers to be
  reconciled with later evidence. No current product behavior or guide answer was changed.
- The published setup helper preserves the existing packed-policy runtime: `phase3-policy.d67`,
  `phase3-policy.json`, `phase3-contracts.json`, `test-and-task-guidelines.md`, `work-ledger.md`
  and `mutation-suggestions.md`. Its reported file list differs from the skill prose's older
  four-markdown-file list. Setup found every required installed runtime file and copied none.
  The absent `orchestrator-guidelines.md` was not manufactured as a competing policy surface.
  Existing policy files and the unrelated dirty mutation queue remain outside this checkpoint.

The native project default already reads `[agents] default_subagent_model = "gpt-6-luna"` with
no pinned effort. Existing project configuration is preserved. Freeze, capability proof and
workspace-setup/checkpoint results are recorded below. No Phase 3 or campaign launch.


### Resumed Phase 2 validation and preparation — complete

- Same phase owner and same frozen contract. The incoming WEC, both supplied archives and FS
  remain byte-identical to the first pass. Frozen FS SHA-256:
  `8437b36549122a4557e5985f232c9ff564ad836f563b088bfb10f3ab684dce1d`.
  All 24 recorded production/test/harness/guide hashes still match. The original inspected
  `876a6c48e226b27db90c9d443f1b6d13fa6c698d` baseline remains exact for those bytes.
- On resumption HEAD was `352ea0e64535c8a2ec76035c2f9a452fbcb44f6e`; another operation then
  advanced it to `da6deac939d878c488503ead4a4e3cf05a5274df`. Both intervening commits change
  only `.de67/human-todo.md`. They remain untouched and supply no new campaign owner intent.
  Root/host guidance, the relevant changed method documents and current project ownership,
  configuration and model direction were rechecked. No product source drift requires refreeze.
- Installed DE67 release is clean commit `a89cfaa8666e0f9613beda5f3c1e717ff57e92c7`.
  The user reports its 37 workspace-setup tests and three packaging foundation tests passed;
  those settled tests were not repeated. The actual managed CLI executable
  `/Users/josefhorvath/.codex/packages/app-server-daemon/current/bin/codex` reports 0.171.0.
  Native probe session metadata independently records that version. The separate generic
  shell `codex` wrapper resolves to 0.167.0; it was not used for these probes or altered here.
- Fresh capability-only parent `/root/phase2_updated_capabilities` completed three sequential
  `fork_turns="none"` nonce probes. Luna omitted the model and requested low; Sol explicitly
  requested `gpt-6.2-sol`/high; Terra requested `gpt-6.1-terra`/medium. Exact replies and native
  `turn_context` model/effort metadata confirmed all three:
  - `01a204a4-8813-7153-a069-4a8e280a22a5`: Luna/low,
    `CAOL-P2-RESUME-LUNA-LOW-297d8a31`.
  - `01a204a4-9bb9-7e10-b1ec-bc7510686f09`: Sol/high,
    `CAOL-P2-RESUME-SOL-HIGH-bd704922`.
  - `01a204a4-b84f-7e43-8121-7cad618ea52f`: Terra/medium,
    `CAOL-P2-RESUME-TERRA-MEDIUM-0cf7b856`.
  These three probe contexts received the host and project AGENTS text. They prove capability,
  not delivery-role assembly. No Phase-3 coordinator, product worker or reviewer was launched.
  The earlier successful Luna/low and legacy Sol/high evidence remains valid at its original
  scope; current required Sol proof is the new GPT-6.2 probe. The Luna default required no edit.
- The previous roster blocker is resolved by the published helper. The first resumed invocation
  used a different `python3` from the existing managed hook and was rejected as a modified hook.
  Inspection showed the same helper and guarded push command, bound to
  `/opt/homebrew/opt/python@3.14/bin/python3.14`. Re-running setup with that existing interpreter
  (Python 3.14.7) succeeded; no hook rewrite, guard bypass or policy edit was needed.
- Successful invocation: `workspace_setup.py setup --workspace . --target origin dev` with
  `--worker-capability gpt-6-luna low`, `--worker-capability gpt-6.2-sol high`,
  `--worker-capability gpt-6.1-terra medium` and
  `--guidance-source /Users/josefhorvath/.codex/AGENTS.md`.
  The helper passed its disposable durable-acceptance projection, retained lineage
  `semantic-surface-cockpit`, preserved all six existing runtime files, retained prior model
  capabilities, and recorded the audited host guidance hash
  `e7b6d904d0a5e78f15637b51a82248496d7137e72d075d5d846ce10cff693d69`.
  `.de67/state/` remains ignored. The sole automatic target is `origin/dev`.
  Setup reported successful backlog push at `da6deac939d878c488503ead4a4e3cf05a5274df`.
- Durable acceptance remains 122 rows, including two invalidations; its full row digest
  matches before and after preparation. All prior capability pairs and unrelated dirty bytes remain intact. Historical acceptance supplies no credit to the eight new claims.
  FS marker/receipt/source checks and scoped `git diff --check` validate this documentation handoff.
  No game tests or campaign were run, and no behavioral pass is claimed.
- Scoped checkpoint files: `.de67/WEC.md`, `.de67/FS.md`, `.de67/work-ledger.md` only.
  Configuration, ignore rules and runtime files needed no tracked edit. Archives remain at their
  exact supplied Mac paths outside the checkpoint. The guarded post-commit hook owns publication;
  verify its receipt and the actual remote SHA after committing, without a second routine push.
  Git records the final checkpoint identity. No unresolved preparation blocker remains.

### Unaccepted campaign obligations

These are the eight frozen FS claims, with no task launch or inherited acceptance:

- [ ] R-CAMPAIGN-REPLAY — Independent setup/game seeds, resumable attempts and honest reached coverage.
- [ ] R-CAMPAIGN-FAILURES — Detect, fresh-process replay and group the first broken invariant.
- [ ] R-CAMPAIGN-HUMAN-TRAVEL — Split crossings, native local execution, return and the next decision.
- [ ] R-CAMPAIGN-BANDIT-ECONOMY — Finite resources, physical payment custody and once-only home credit.
- [ ] R-CAMPAIGN-CANNIBAL-ECONOMY — Supply pressure, member availability and provisional/final returns.
- [ ] R-CAMPAIGN-MIGO-RESPONSE — Real perception, resident identity and individual retreat across handoffs.
- [ ] R-CAMPAIGN-MIGO-ECONOMY — Population and matched loaded, active-unloaded and frozen brood time.
- [ ] R-CAMPAIGN-COVERAGE-LOAD — Measured pilot, connected expansion, long cycles and genuine mixed work.
