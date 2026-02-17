# Local LLM Intent Layer (Follower NPCs) Plan
# Plan summary
We are integrating a local LLM into Cataclysm to let allied NPC followers react
to player shouts. The game sends an NPC-centric snapshot, the LLM replies with a
single-line speech + action list, and the NPC applies those actions for a few
turns using existing AI behaviors. The system is async (no frame stalls), guard
railed for safety, and optimized for fast prompt iteration.

## Next: AI Implementation (In-Game Intent Actions)
Goal: expand LLM actions to cover combat/movement behaviors.

### Intent Whitelist
- guard_area: assign guard mission at current location / hold position.
- follow_close: follow at close distance.
- follow_far: follow at farther distance.
- idle: no-op (speech only).
- equip_gun: allow guns + wield best gun.
- equip_melee: disallow guns + wield best melee (or unwield gun).
- equip_bow: allow guns + dissallow loud weapons.
- attack=<target> to attack a certain target.

#To-Do:
### Reliability / Flow
- Buffer multi-step intents: when look_inventory/look_around trigger a second toolcall, preserve the first toolcall’s actions instead of overwriting them.
x Snapshot follower mode: include current follow distance/state (follow-close, follow-afar, guard/hold) so the LLM stops spamming repeat follow actions and maps to the right in-game mode.
x Conversation memory: add the last 1–2 utterance/response pairs to the snapshot (brief, sanitized) for tone/continuity.
x Overheard ally memory: nearby allies now receive short `npc_name/speech/actions` entries and include them in snapshot context.
- Malformed-output safety: retry once with a short "fix format" prompt and strict timeout before dropping.

### Porting & Releases
- Goal: keep LLM features in sync across three upstream targets and ship 6 binaries per refresh cycle.

#### Target matrix (first-class)
- CDDA master -> upstream `CleverRaven/Cataclysm-DDA:master` -> Windows + Linux release.
- CDDA 0.H -> upstream `CleverRaven/Cataclysm-DDA:0.H-branch` -> Windows + Linux release.
- CTLG master -> upstream `Cataclysm-TLG/Cataclysm-TLG:master` -> Windows + Linux release.

#### Branching model
- Develop LLM features on `dev` (working branch).
- Before every port/release cycle, merge `dev` -> `master` first.
- Treat `master` as the only port source branch (release staging baseline).
- Recreate integration branches from fresh upstream tips on every run:
  - `port/cdda-master`
  - `port/cdda-0.H`
  - `port/ctlg-master`
- Then apply AOL changes from `master` onto each `port/*` branch.
- Before every refresh run, create a dated backup of current `master`:
  - `backup/master-pre-port-YYYYMMDD-HHMM`

#### Automation scope (scripted)
- Add a single orchestrator script (PowerShell) to:
  - fetch all remotes (`origin`, `upstream` for CDDA, and `upstream-ctlg` for CTLG).
  - create backup branch of current `master`.
  - recreate each `port/*` branch from its mapped upstream target.
  - apply AOL layer from `master` onto each `port/*`.
  - detect merge conflicts and stop with clear next actions.
  - call Codex with a per-target merge-fix prompt template (conflict context + goals).
  - run post-fix validation commands and save logs.
  - print final human checklist for release packaging.
- Store prompt templates and run metadata under `tools/porting/` so periodic refresh is repeatable.

#### Codex merge-fix loop (per target branch)
- Step 1: recreate `port/*` from target upstream tip, then merge `master` into `port/*`.
- Step 2: if conflicts/build breaks happen, invoke Codex with:
  - what branch is being ported
  - which upstream commit range was merged
  - "preserve all AOL LLM behavior parity" requirement
  - required build checks (Windows + Linux)
- Step 3: Codex applies fixes, reruns build checks, summarizes changes and residual risk.

#### Build/release responsibilities
- Keep release packaging manual for now (required local environments differ: MSYS2 UCRT64 and WSL2).
- Automation should prepare branches and instructions; Josef performs final release builds and publishing.
- Manual release pass per branch:
  - Windows build/bindist command
  - Linux build/bindist command
  - artifact naming check (`target + platform + date/version`)

#### Release cadence
- Weekly or biweekly refresh for `cdda-master` and `ctlg-master`.
- On-demand refresh for `cdda-0.H` (or when upstream receives relevant fixes).

#### Nice operator output (must-have)
- Script prints:
  - updated branches and upstream SHAs
  - whether Codex auto-fix was needed
  - build check status per branch/platform
  - exact next commands Josef should run for packaging and tagging

#### Future (later)
- Integrate distribution into CDDA-Game-Launcher "kitty" installer (https://github.com/Fris0uman/CDDA-Game-Launcher) so players can install/update this fork.

#Later to-Do, not now:
- Throw grenades
- Move instructions
- LLM Finetuning
Finetuning/Distilling would increase speed and accuracy. Is that legal?

				  
 ### Complete NPC Dialogue/Interaction Overhaul??
lol
#### Technical: how dialogue options are built (current architecture)
- Dialogue data is loaded at startup from `type: "talk_topic"` JSON across `data/json/npcs/**` (including `data/json/npcs/Backgrounds/*.json`) into the `json_talk_topics` map in `src/npctalk.cpp` via `load_talk_topic()`.
- A conversation starts in `avatar::talk_to()` (`src/npctalk.cpp`), which creates a `dialogue` with two `talker`s and pulls an initial topic stack from `talker_npc::get_topics()` (`src/talker_npc.cpp`).
- `talker_npc::get_topics()` picks the NPC's `dialogue_chatbin` topics (first_topic, talk_friend, talk_leader, etc. in `src/dialogue_chatbin.h`), then falls back to `npc::pick_talk_topic()` (`src/npctalk.cpp`) which selects a stranger/friend topic based on personality/opinion.
- For each topic, `dialogue::dynamic_line()` calls `json_talk_topic::get_dynamic_line()` and `dialogue::gen_responses()` calls `json_talk_topic::gen_responses()` to build the visible player responses from JSON, filtering by dialogue conditions and attaching effects/trials.
- The response's effect sets the next topic; the loop continues until `TALK_DONE`, with `TALK_NONE` popping the stack. This is the central hook if we ever want to intercept or rewrite dialogue selection globally.
