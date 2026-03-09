# Agents
Follow all instructions in this file.
Read Plan.md and keep it up to date. Whenever something is finished, tick it off, or remove it completely.
When you added a new complicated mechanic, add a brief explanation to TechnicalTome.md.

### Code style
- Code style is enforced by `astyle`.
- See `doc/c++/CODE_STYLE.md` for details.
- Quick astyle overview (C++):
  - K&R-style braces: `if( ... ) {` and `} else {`
  - 4-space indentation, no tabs
  - Space inside control parens: `if( ... )`, `for( ... )`
  - Pointer/reference style: `type *name`, `type &name`
  - One statement per line; consistent spacing after commas and around operators
- Tell Josef to use and apply astyle 3.1 for `.astylerc`; newer versions reject options like `indent-preprocessor` and `add-brackets` (WSL/MSYS2 can supply 3.1).
- Ensure changes stay Linux-compatible (paths, APIs, and build steps).
- When editing README.md, please tell Josef to review every time. He does not like your edits (though he tells you to do them) in README and always wants to edit them afterwards.

### How to edit with diffs
When editing a file, do not delete and rewrite bystander lines for diff context. Only add them as neutral context, without -/+, to the diff.

### Shell commands in this repo
- The default shell is PowerShell; avoid here-docs and complex quoting in `-Command`.
- Prefer one-line commands with explicit quoting, or use WSL for bash-specific scripts.
- When a command needs lots of quoting or newlines, create a temporary script file and run it.

### Automation and script portability
- CI workflows run many steps under `bash` (including Windows via MSYS2/Git-Bash).
- Prefer POSIX-compatible shell commands for scripts used by build or tooling.
- Avoid PowerShell- or cmd-only assumptions unless a workflow explicitly uses them.
- When adding new automation, keep paths and commands portable across Linux and Windows.
- Do not disable CI workflows; fix the root cause instead.

### Porting orchestrator usage
- Use `tools/porting/orchestrate_ports.ps1` for branch recreation, upstream merges, and build checks.
- Start it from `master` only; it errors out on other branches by design.
- Treat `master` as the source of truth for shared auxiliary files: `README.md`, `Plan.md`, `TechnicalTome.md`, `Agents.md`, `just_build.cmd`, `just_build_linux.cmd`, `build_and_run.cmd`, `build_and_run_linux.cmd`, and `tools/porting/orchestrate_ports.ps1`. Port branches should sync these from `master` instead of carrying branch-local edits.
- Do not switch the orchestrator default `AolSourceRef` to `port/cdda-master` until `port/cdda-master` has passed real in-game AOL smoke tests. Until then, keep the default on `master` and pass `-AolSourceRef port/cdda-master` only deliberately.
- First-run safety check:
  - `.\tools\porting\orchestrate_ports.ps1 -DryRun -AllowDirty`
- Typical run with Codex merge/build fixing:
  - `.\tools\porting\orchestrate_ports.ps1 -RunCodex`
- Scope a single target when testing:
  - `.\tools\porting\orchestrate_ports.ps1 -Targets cdda-0.I -RunCodex`
- Review logs in `tools/porting/logs/<timestamp>/` after each run.
- When monitoring long-running orchestrator, build, or rebuild runs, prefer roughly a 5-minute polling cadence unless a new error appears or the branch/log state changes materially.
- When a target already has the newer AOL queue/executor pipeline, record obsolete prototype commits in `tools/porting/patchsets/<target>-ignore.txt` on `master` instead of re-resolving them every run.
- `port/cdda-0.I` now has source-level AOL parity on its branch. Future `0.I` patchset runs should ignore prototype-era commits that try to reintroduce the old `guard_area` / `follow_player` / `use_gun` / `use_melee` model once the branch already has the newer `follow_close` / `follow_far` / `equip_*` / `panic_*` queue-executor pipeline.
- `port/cdda-0.H` now has source-level AOL parity on its branch too. Future `0.H` reruns need the target overlay commits in `tools/porting/patchsets/cdda-0.H.txt`, because the branch-local follow-up fixes are not part of the shared common queue.

## Porting build gotchas (2026-02-24)
- Always run long builds with redirected logs, then parse first hard error:
  - `.\just_build.cmd --unclean *> build_logs\<name>.log`
  - `rg -n "error:|make: \*\*\*|fatal error|undefined reference" build_logs\<name>.log`
- If errors look inconsistent after branch switching, clear stale PCH and rebuild:
  - `Remove-Item pch\main-pch.hpp.gch,pch\main-pch.hpp.d -Force -ErrorAction SilentlyContinue`
- If manual builds fail with `Cannot convert existing thin library zstd.a`, remove stale archives before rebuild:
  - `Remove-Item zstd.a,zstd_test.a -Force -ErrorAction SilentlyContinue`
  - (Helper scripts now do this automatically.)
- If Linux/manual builds fail with `Cannot convert existing thin library cataclysm.a`, remove stale archives and tests PCH:
  - `Remove-Item cataclysm.a,pch\tests-pch.hpp.gch,pch\tests-pch.hpp.d -Force -ErrorAction SilentlyContinue`
  - (Helper scripts now do this automatically.)
- If Windows linking fails with `undefined reference to 'BCryptGenRandom'` from `cata_allocator.o` after upstream snmalloc changes, ensure the Windows link flags include `-lbcrypt` in `Makefile`.
- On `port/ctlg-master`, `make clean` may report `clean-tests` failures while the actual build still succeeds. Treat that as cleanup noise unless the final build command exits non-zero.
- When checking cleanliness across `port/*` branches after switching between CDDA and CTLG targets, verify `data/mods/TEST_DATA/` is not left as an untracked carryover on CTLG branches.
- Executable naming differs by branch target:
  - CDDA targets: run `cataclysm-tiles.exe` on Windows and `cataclysm-tiles` on Linux.
  - CTLG target: run `cataclysm-tlg-tiles.exe` on Windows and `cataclysm-tlg-tiles` on Linux.
- CTLG font config schema differs from newer CDDA-style `config/fonts.json`:
  - CTLG expects legacy string arrays (`typeface`, `map_typeface`, `overmap_typeface`) compatible with `data/fontdata.json`.
  - Copying modern hinted-object font config (`{ "path": "...", "hinting": "..." }`) into CTLG profile `fonts.json` causes startup JSON parse failure.
- If CTLG reports `Invalid mod No_Rail_Stations requested`, treat it as stale/renamed mod id and map it to `railroads`.
- CTLG railroads JSON may fail strict loading on `data/mods/railroads/mapgen/railroad/railroad_station.json` due to `lootable` field in vending machine entries; remove/patch unsupported keys when this appears.
- CTLG-specific merge traps seen during porting:
  - `npc::evaluate_weapon` requires 3 args (`item&, bool can_use_gun, bool use_silent`).
  - If `npcmove.cpp` defines `execute_llm_intent_action(...)`, `npc.h` must declare it.
  - LLM sleepiness snapshot code may need branch-safe fallback if `sleepiness` APIs diverge.
- `port/cdda-0.I` replay trap: early AOL commits `fd9cd69de9`, `b39b2b82a9`, `0691a1e92a`, `c1f0d74965`, `103d591616`, `020d81df9e`, `ae713a9cc8`, `3560374527`, `b1e341b62d`, and `8d6746a32e` are obsolete-on-target once `0.I` already has the newer queue/executor implementation. Keep the branch-safe `evaluate_weapon( item &, bool, bool )` helpers and newer executor path; do not backslide to the prototype action vocabulary.
- `port/cdda-0.H` replay/build trap: after executor parity is restored, Windows builds can still fail with `undefined reference` from `npcmove.o` if the branch is missing `npc::evaluate_best_gun`, `npc::evaluate_best_silent_gun`, and `npc::evaluate_best_melee` definitions. Keep the older branch-safe implementations in `src/npcmove.cpp` and replay them from `tools/porting/patchsets/cdda-0.H.txt` rather than re-porting newer bubble/item-handle code.

## Tests and in-game verification
- Use unit tests where applicable.
- For gameplay changes, use the in-game debug menu to reproduce conditions.
- Validate builds with `just_build.cmd > debug.txt 2>&1` and `just_build_linux.cmd > debug.txt 2>&1` when touching code or build scripts.
- Fast compile check (changed files only):
  - Windows/MSYS2: `make -j8 RELEASE=1 MSYS2=1 DYNAMIC_LINKING=1 SDL=1 TILES=1 SOUND=1 LOCALIZE=0 LINTJSON=0 ASTYLE=0 TESTS=0 objwin/tiles/llm_intent.o objwin/tiles/npc.o objwin/tiles/npcmove.o`
  - Linux/WSL: `make -j8 TILES=1 SOUND=1 RELEASE=1 LOCALIZE=1 LANGUAGES=all LINTJSON=0 ASTYLE=0 TESTS=0 obj/tiles/llm_intent.o obj/tiles/npc.o obj/tiles/npcmove.o`
- For LLM runner changes, run `tools/llm_runner/runner.py --self-test` from your venv.
- After certain changes, you can give me tasks to test new features in-game

## Build (MSYS2 UCRT64)
- Josef needs to use the MSYS2 UCRT64 shell for Windows builds.
- Command:
  `make -j$((\`nproc\`+0)) CCACHE=1 RELEASE=1 MSYS2=1 DYNAMIC_LINKING=1 SDL=1 TILES=1 SOUND=1 LOCALIZE=1 LANGUAGES=all LINTJSON=0 ASTYLE=0 TESTS=0`
- Expected warnings about unterminated character constants may appear; they do not impact compilation.
- For a repeatable Windows bindist + DLL bundling + zip, run:
  `bash tools/release/msys2_bindist.sh`
- Optional versioned zip name:
  `bash tools/release/msys2_bindist.sh --0.1.0`
  or `bash tools/release/msys2_bindist.sh --version=0.1.0`

## Build (Linux/WSL)
- In WSL2 Josef needs to run:
  `make -j$(nproc) TILES=1 SOUND=1 RELEASE=1 LOCALIZE=1 LANGUAGES=all LINTJSON=0 TESTS=0`
For the full, canonical guidance, see `doc/CONTRIBUTING.md`.

read README.md and Plan.md.
