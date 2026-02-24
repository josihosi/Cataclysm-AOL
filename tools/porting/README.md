# Porting Orchestrator

This folder contains automation for recurring AOL port refreshes across:
- CDDA master
- CDDA 0.H
- CDDA 0.I
- CTLG master

## Script
- `tools/porting/orchestrate_ports.ps1`
- `tools/porting/simulate_patchset.ps1`

## What It Does
- Requires current branch to be `master` (hard fail otherwise).
- Optionally requires clean working tree (default hard fail if dirty).
- Ensures `upstream-ctlg` exists and tracks only `master` with no tags.
- Fetches remotes (`origin`, `upstream`, `upstream-ctlg`) with `--prune`.
- Creates a dated backup branch from `master`.
- Recreates `port/*` branches from upstream targets.
- Replays AOL commits via patchset cherry-picks onto each `port/*` branch.
  - Base queue: `tools/porting/patchsets/common.txt`
  - Optional target queues: `tools/porting/patchsets/<target>.txt`
  - Optional ad-hoc source: `-PatchsetCommitRange` (+ optional `-PatchsetPathFilter`)
- Applies conflict threshold gate (`-MaxConflictFiles`, default `100`) to stop obviously manual-heavy targets.
- Runs build checks (`just_build.cmd --unclean`, `just_build_linux.cmd --unclean`).
- Optionally invokes `codex exec` to fix cherry-pick/build failures.
- Writes per-target logs plus a summary under `tools/porting/logs/<timestamp>/`.

## Usage

Dry run:
```powershell
.\tools\porting\orchestrate_ports.ps1 -DryRun
```

All targets, no Codex:
```powershell
.\tools\porting\orchestrate_ports.ps1
```

All targets, Codex auto-fix enabled:
```powershell
.\tools\porting\orchestrate_ports.ps1 -RunCodex
```

Subset of targets:
```powershell
.\tools\porting\orchestrate_ports.ps1 -Targets cdda-master,ctlg-master
```

Allow dirty tree (not recommended):
```powershell
.\tools\porting\orchestrate_ports.ps1 -AllowDirty
```

Skip build checks:
```powershell
.\tools\porting\orchestrate_ports.ps1 -SkipBuild
```

Run orchestrator using ad-hoc commit range (instead of patchset files):
```powershell
.\tools\porting\orchestrate_ports.ps1 -PatchsetCommitRange upstream/master..master -PatchsetPathFilter src/llm_intent.cpp,src/npc.cpp,src/npc.h,src/npcmove.cpp,tools/llm_runner
```

Tighten conflict threshold (manual handoff earlier):
```powershell
.\tools\porting\orchestrate_ports.ps1 -MaxConflictFiles 40
```

Patchset dry-run simulation using curated patchset files:
```powershell
.\tools\porting\simulate_patchset.ps1 -Fetch
```

Patchset dry-run simulation from a commit range (quick estimator):
```powershell
.\tools\porting\simulate_patchset.ps1 -Fetch -CommitRange upstream/master..master -PathFilter src/llm_intent.cpp,src/npc.cpp,src/npc.h,src/npcmove.cpp,tools/llm_runner
```

Patchset files live in:
```text
tools/porting/patchsets/
```

## Preflight Expectations
- You are on `master`.
- `dev` may differ from `master` while development continues.
- Before release cycle, merge `dev -> master`.
- You are logged in for Git remotes and (if used) Codex CLI.

## Manual Release Packaging (after orchestration)
Run per prepared `port/*` branch:

Windows (MSYS2 UCRT64 path):
```powershell
just_build.cmd --unclean
bash tools/release/msys2_bindist.sh
```

Linux (WSL2 path):
```powershell
just_build_linux.cmd --unclean
```

Then publish artifacts for each target:
- `cdda-master` (Windows + Linux)
- `cdda-0.H` (Windows + Linux)
- `cdda-0.I` (Windows + Linux)
- `ctlg-master` (Windows + Linux)
