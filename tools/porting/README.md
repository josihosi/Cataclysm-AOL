# Porting Orchestrator

This folder contains automation for recurring AOL port refreshes across:
- CDDA master
- CDDA 0.H
- CTLG master

## Script
- `tools/porting/orchestrate_ports.ps1`

## What It Does
- Requires current branch to be `master` (hard fail otherwise).
- Optionally requires clean working tree (default hard fail if dirty).
- Ensures `upstream-ctlg` exists and tracks only `master` with no tags.
- Fetches remotes (`origin`, `upstream`, `upstream-ctlg`) with `--prune`.
- Creates a dated backup branch from `master`.
- Recreates `port/*` branches from upstream targets.
- Merges `master` into each `port/*` branch.
- Runs build checks (`just_build.cmd --unclean`, `just_build_linux.cmd --unclean`).
- Optionally invokes `codex exec` to fix merge/build failures.
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
- `ctlg-master` (Windows + Linux)
