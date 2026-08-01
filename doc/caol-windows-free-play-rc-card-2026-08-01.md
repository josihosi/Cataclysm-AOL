# C-AOL Windows free-play release-candidate card (2026-08-01)

Branch: `port/cdda-master` only. The other branches are behind this production candidate.

Launch from the repo root:

```powershell
python tools\openclaw_harness\startup_harness.py handoff manual.release_candidate_roaming_mcw --launch-only --compact-stdout
```

The handoff selects the standard `UltimateCataclysm` (Ultica) tileset, starts you safely inside the completed evacuation shelter at 10:00, and then stops sending gameplay input. Play normally. The stale scout from the source test fixture is removed; the fixture and a fresh post-install saved-state audit stage a zombie rider north, a flesh raptor west, a writhing stalker southeast, cannibal pressure east-northeast, and bandit pressure south. The intended threats begin outside the starting reality bubble; there is no required order and no target checklist. Final exact-commit normal-map load remains a handoff gate rather than an already completed claim.

For this test profile, the harness provisions `CATA_API_KEY` from Windows Credential Manager (or an existing `OPENAI_API_KEY`) only into the game child process and selects the known API venv. You should not need to set the variable manually for each run; the key is not printed into the handoff report. If the key or exact runner is unavailable, the harness stops before launching a game with broken LLM support.

For a useful debug note, send one short block:

```text
Place/time:
What had just happened:
What I saw:
What felt good, fake, confusing, unfair, or inert:
Severity: note / annoying / blocking
Saved after it: yes / no
```

If an encounter is memorable or broken, save before moving far away. That gives the agent-side audit a much better chance of reconstructing the exact actor state and preceding decisions.

Claim boundary: this is deliberate nearby roaming setup, not proof that ordinary world generation naturally places all five feature families around every start. Writhing-stalker and zombie-rider AI/progression are separate design discussions; this card only makes them available to notice during ordinary play.
