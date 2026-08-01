# C-AOL Windows free-play release-candidate card (2026-08-01)

Branch: `port/cdda-master` only. The other branches are behind this production candidate.

Launch from the repo root:

```powershell
python tools\openclaw_harness\startup_harness.py handoff manual.release_candidate_roaming_mcw --launch-only --compact-stdout
```

The handoff selects the standard `UltimateCataclysm` tileset, starts you inside the completed evacuation shelter at 10:00, and then stops sending gameplay input. Play normally. The staged world has a zombie rider north, a flesh raptor west, cannibal pressure east-northeast, and bandit pressure south. They begin outside the starting reality bubble; there is no required order and no target checklist.

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

Claim boundary: this is deliberate nearby roaming setup, not proof that ordinary world generation naturally places all four feature families around every start.
