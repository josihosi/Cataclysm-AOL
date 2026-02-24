# C-AOL Release Roadmap

## Objective
Ship tested Windows and Linux releases for:
- `port/cdda-master`
- `port/cdda-0.H`
- `port/cdda-0.I`
- `port/ctlg-master`

## Status
- `Done`: orchestrator replay flow is stable and `rerere` is enabled (`rerere.enabled=true`, `rerere.autoupdate=true`).
- `In progress`: release validation and packaging parity across all `port/*` branches.

## Execution Roadmap

### 1) Port refresh
- Ensure `master` contains the latest release-ready AOL changes.
- Re-apply the curated patch queue onto each `port/*` branch (orchestrator/cherry-pick flow).
- Keep per-target fixups branch-local only when unavoidable.

### 2) Build matrix (logs redirected)
- For each target branch run:
  - `just_build.cmd --unclean > build_logs/<target>-win.log 2>&1`
  - `just_build_linux.cmd --unclean > build_logs/<target>-linux.log 2>&1`
- Use the helper defaults for matrix speed:
  - Linux dependency bootstrap is opt-in (`--install-deps`).
  - Background summary generation is opt-in (`--with-summary`).
  - Linux build-validation runs disable `ASTYLE` checks.

### 3) Smoke tests
- For each target branch run:
  - `build_and_run.cmd --unclean`
  - `build_and_run_linux.cmd --unclean`
- Validate at least:
  - Game reaches main menu.
  - Save/world load works.
  - NPC shout/speech loop (`C` + `b`) still behaves correctly.

### 4) Packaging audit
- Required in release artifacts for every target:
  - `tools/llm_runner/**`
  - `data/json/npcs/Backgrounds/Summaries_short/**`
  - `README.md`
  - `Plan.md`
  - `TechnicalTome.md`
  - `Agents.md`
- Ensure CAOL branding is present in README/title assets, with a short compatibility disclaimer on port builds.

### 5) GitHub release
- Produce 8 artifacts total (4 targets x 2 platforms).
- Publish with commit hashes and short per-target notes.
- Include known limitations for any target with behavior diffs.

## Definition Of Done
- All four `port/*` branches complete Windows and Linux build validation.
- Smoke tests are completed on both platforms.
- Packaging contents are parity-checked across targets.
- GitHub release artifacts are uploaded with clear branch/commit provenance.
