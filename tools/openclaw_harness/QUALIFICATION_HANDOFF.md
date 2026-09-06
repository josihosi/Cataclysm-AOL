# Harness handoff for the separate DE67 package

The harness implementation and representative qualification fixtures are checkpointed on `dev`.
The comprehensive playtest package is a separate owner discussion followed by the appropriate DE67
steps; this checkpoint neither launches that package nor certifies all CAOL features.

The working source is the Mac mini checkout at
`/Volumes/CodexBulk/Schanigarten/workspaces/Cataclysm-AOL-hostile-ecology-dev`.
Windows remains behind and must not overwrite that checkout. Preserve the existing DE67 state.
The tested source checkpoint is `54d6c00dfe` on `dev`; later documentation commits do not add
new gameplay. The source-bound build receipt under `.userdata/openclaw_harness/source_bindings/`
is the authority for a future launch, not a copied executable hash or an old registry token.

## Use and evidence

Start with [the harness skill](../../.agents/skills/caol-harness/SKILL.md), then
[the capability map](QUALIFICATION.md). The latter separates native implementation tests,
fresh Luna usability observations, and remaining product questions. Exact local trial receipts
are under `.userdata/openclaw_harness/bridge-sessions/`; review, build, resource and failed-attempt
artifacts are under `build_logs/20260905-harness-ergonomics/`. These are retained local evidence,
not files included in this Git checkpoint.

The `harness.*_mcw` scenario definitions and `factory-*` charters are reusable disposable
qualification scenes. Query them through the registry and use its freshly bound launch authority.
They seed their runtime config from the existing profile snapshot. The prepared revolver fixture
is a declared transform of the prepared camp, with manufactured equipment receiving no gameplay
credit. The combat scene uses local Gemma 4 E4B; the focused NPC scene uses E2B; ordinary camp,
persistence and ecology scenes disable the model. Installed local model tags are prerequisites.

Failed input, transport, observation or cancellation leaves the game running. Refresh uncertain
ownership and choose from the current native surface. The player's explicit quit ends the whole
scenario; a declared save/reload continuation belongs to `finish`, and the persistent client
collects the new session generation. No automatic time/RSS kill guard should be restarted.

## Qualification boundary and next work

The capability map records representative native Luna play: fire and inventory handling,
actor-bound NPC inspection/rules/orders, trade and restored item/zone state, melee and firearm
damage/reload/unload/retreat, E2B outage/recovery and E4B replies, useful native duration waits,
pending cancellation and continued play, exact ecology log retrieval, actual death and explicit
cleanup, and ordinary equipment wearing/takeoff. All qualification games have ended.

Important distinctions survive handoff. The restored Glock is on a bench, not in the avatar's
pockets. A guard command/state and one pause at the post are proved; prolonged guard behavior is
not. Death can be forced by a declared debug intervention, which gives no natural-death credit.
Follower takeover and watching were available and deliberately declined; their successor is
truthfully unsupported MESSAGE_LOG with no actions. Explicit finish still closes the owned game.
The wearing witness reversed two observations; its player acknowledged the corrected native
takeoff/put-on/worn-location sequence, retained separately without rewriting the sealed witness.
The cannibal candidate record and separate bandit no-signal reads do not prove a routing defect
or a natural cannibal response. Mechanical witness validation does not settle those judgments.

For the next package discussion, select and freeze the CAOL product trajectories against the
existing DFS and current source/upstream delta: living NPC intent/context and follow/camp routing,
camp establishment and mission outcomes, Locker/Patrol/Food/Storage use, bandit natural
stimulus/scouting/demand/payment/refusal/return, cannibal discovery/day hold/night departure/approach
through dawn, signal positive/negative controls and world boundaries, persistence, flesh raptors,
and integrated performance. Stalkers and zombie riders remain excluded pending the owner decision.
This handoff does not start those DE67 steps or certify those product features.

The performance client supplies true interval CPU, RSS, action latency and explicit baseline
comparison. Retained finite samples and a controlled parser-memory comparison exist. A matched
ordinary-action or with/without-visible-bandit CPU benchmark was not completed; choose comparable
workloads in the product package. Player-model token consumption is unavailable. Do not substitute
ps's decayed CPU percentage for interval CPU or infer a universal resource threshold from these runs.

## Last useful evidence handles

All following filenames are under `build_logs/20260905-harness-ergonomics/` on the Mac unless an
explicit session path is given. These are retained local evidence; they are not uploaded by Git.

- `combat16-shot-evidence.json`, `combat16-submitted-witness.json`, `combat16-ended-costs.json`:
  actual melee/firearm outcomes and finite costs.
- `e4b-live-runner-evidence.json`, `e4b-loaded-resource-snapshot.json`: actual companion replies,
  request correlation, latency and separate model-memory observation.
- `persistence18-item-guard-evidence.json`, `persistence18-log-discovery-evidence.json`,
  `persistence18-finish-evidence.json`: restored bench item, bounded guard result, native log access
  and final cleanup without an extra reentry.
- `cannibal15-cancellation-terminal-evidence.json`: pending pause cancellation after 283 native
  actions and continued use, with the earlier terminal-owner defect retained honestly.
- `ecology19-final-evidence.json`, `ecology19-ended-costs.json`, `ecology19-process-costs.json`:
  fresh duration waits, exact faction records, current post-death ownership and final cleanup.
- `wear20-final-evidence.json`, `wear20-ended-costs.json`: actual wearing/takeoff, corrected
  chronology, finite costs and exact process cleanup.
- `refresh_compare.py`, `refresh_compare.result.json`: same retained 43.8 MB trace, identical
  output, measured peak parsing allocations 116.164 to 4.195 MiB. Scan time still grows with history.

Use `controls` for current-run log discovery, then exact selectors/record handles for details.
After cleanup, the live descriptor is released and current-owner controls can report unavailable;
retained response handles and previously discovered exact log paths remain readable.

## Local validation and platform limits

Changed classes are C++ native owners/tests, Python harness/tests, the player skill and disposable
scenario documentation. Retained native Mac checks include 1,379 semantic assertions in 60 cases,
180 trade assertions, 114 ground-equipment assertions, 56 terminal-owner assertions and narrower
owner regressions. These groups overlap; they are not an added unique total. Relevant changed
native owners/tests compile through narrow Linux translation units. The Linux GCC route demotes
an existing npc.h changes-meaning diagnostic for that isolated check.

The latest client suite passes 30 Mac tests. Eleven selected shared Python cases pass on Windows
and WSL, followed by both final producer-path cases after the last correction. Exact selectors,
review disposition and platform limits are in `final-player-portable-validation.json`.
The scoped `autoreview --mode local` result is clean in `final-player-log-review-relaunch.json`.
The source-bound Mac game is built and exercised. No complete native Windows or Linux game
runtime is claimed. Full FIFO subprocess tests are a POSIX route; earlier incompatible Windows
fixture failures are retained separately rather than called green.

Use astyle 3.1 with `.astylerc` for C++ changes. Hosted CI is separate from local proof. The code
checkpoint's General build matrix remained pending at the final code-status check on 2026-09-06:
https://github.com/josihosi/Cataclysm-AOL/actions/runs/34000763245 . The prior native checkpoint's
Clang-tidy-plugin run failed on installed LLVM 23 lit rejecting ShTest(True), after plugin
compilation passed; this is a retained workflow/tooling failure:
https://github.com/josihosi/Cataclysm-AOL/actions/runs/33998069154 . Check the current pushed
commit's runs before claiming hosted CI is green.
