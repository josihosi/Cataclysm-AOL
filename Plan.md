# Local LLM Intent Layer (Follower NPCs) Plan

## Plan summary
We are integrating a local LLM into the open-source game Cataclysm so allied NPC followers can react to player shouts in-character. 
The game sends an NPC-centric snapshot, the model returns a short speech plus whitelisted actions, and NPC AI applies them asynchronously with guardrails and stable behavior parity across release targets.

## To-Do (Highest Priority)

### Release cycle (current focus)
- Merge `dev` into `master` before starting the port/release run.
- Run the orchestrator from `master`:
  - `.\tools\porting\orchestrate_ports.ps1 -RunCodex`
- Review `tools/porting/logs/<timestamp>/summary.txt` and per-target logs.
- Confirm each target branch is ready:
  - `port/cdda-master`
  - `port/cdda-0.H`
  - `port/cdda-0.I`
  - `port/ctlg-master`
- Produce and publish 8 release artifacts (Windows + Linux for each target).

### Reliability / Flow
- Buffer multi-step intents: when `look_inventory`/`look_around` trigger a second toolcall, preserve the first toolcall's actions instead of overwriting them.
- Malformed-output safety: retry once with a short "fix format" prompt and strict timeout before dropping.
- Context-weighted spontaneous call probability (danger/pain/noise/morale/context deltas), while keeping per-NPC async scheduling.

### Guardrails
- Document and lock the spontaneous-call randomness policy (bounds + distribution + per-NPC independence) in `TechnicalTome.md`.
- Add a focused scheduler regression checklist: duplicate-request prevention, disable/re-enable behavior, and multi-NPC fairness.
- Keep these implementation rules in follow-up changes:
  - Every insertion into persistent async state (`pending_*`, queues, per-NPC timers) must define all removal paths: success, parse failure, timeout, option-disable, despawn/unload.
  - Option toggles must be idempotent across enable/disable cycles and safe to flip mid-flight.
  - Prompt/snapshot protocols should prefer explicit structured fields over magic string sentinels.

## Later To-Do (Not Current Release Blockers)
- Integrate distribution into CDDA-Game-Launcher "kitty" installer (https://github.com/Fris0uman/CDDA-Game-Launcher) so players can install/update this fork.
- Throw grenades.
- Move instructions.
- LLM finetuning.
- Initiative/rumor-system expansion for proactive squad coordination (backburner).

