# AOL Porting Context

## Objective
Port AOL LLM functionality from this repository's `master` branch onto fresh
upstream targets:
- `upstream/master`
- `upstream/0.H-branch`
- `upstream/0.I-branch`
- `upstream-ctlg/master`

The orchestration model is:
1. Develop on `dev`.
2. Merge `dev` into `master`.
3. Recreate `port/*` from fresh upstream tips.
4. Replay AOL patchset commits (curated queue from `master`) via cherry-pick.

## Preserve These Behaviors
- Player sentence shout triggers asynchronous LLM intent processing for allied NPCs.
- Snapshot construction and prompt/response flow in `src/llm_intent.cpp`.
- Intent action handling in NPC logic (`follow`, `wait`, equip, panic, attack, item lookups).
- Runner integration and scripts in `tools/llm_runner/`.
- LLM-related options, logs, and bindist inclusion behavior.

## Important Files (high signal)
- `src/llm_intent.cpp`
- `src/llm_intent.h`
- `src/npc.cpp`
- `src/npc.h`
- `src/npcmove.cpp`
- `src/npctalk.cpp`
- `tools/llm_runner/*`
- `just_build.cmd`
- `just_build_linux.cmd`

## Constraints
- Keep Linux compatibility.
- Do not remove AOL features during conflict resolution.
- Keep changes focused to porting/compatibility fixes.
- Respect existing code style and run checks.

## Validation Commands
- Windows:
  - `just_build.cmd --unclean > debug.txt 2>&1`
- Linux:
  - `just_build_linux.cmd --unclean > debug.txt 2>&1`

For formatting-sensitive failures, use `astyle 3.1` with repo `.astylerc`.
