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
- First-run safety check:
  - `.\tools\porting\orchestrate_ports.ps1 -DryRun -AllowDirty`
- Typical run with Codex merge/build fixing:
  - `.\tools\porting\orchestrate_ports.ps1 -RunCodex`
- Scope a single target when testing:
  - `.\tools\porting\orchestrate_ports.ps1 -Targets cdda-0.I -RunCodex`
- Review logs in `tools/porting/logs/<timestamp>/` after each run.

## Porting build gotchas (2026-02-24)
- Always run long builds with redirected logs, then parse first hard error:
  - `.\just_build.cmd --unclean *> build_logs\<name>.log`
  - `rg -n "error:|make: \*\*\*|fatal error|undefined reference" build_logs\<name>.log`
- If errors look inconsistent after branch switching, clear stale PCH and rebuild:
  - `Remove-Item pch\main-pch.hpp.gch,pch\main-pch.hpp.d -Force -ErrorAction SilentlyContinue`
- If manual builds fail with `Cannot convert existing thin library zstd.a`, remove stale archives before rebuild:
  - `Remove-Item zstd.a,zstd_test.a -Force -ErrorAction SilentlyContinue`
  - (Helper scripts now do this automatically.)
- On Windows run commands, execute `cataclysm-tiles.exe` explicitly (not `cataclysm-tiles`).
- CTLG-specific merge traps seen during porting:
  - `npc::evaluate_weapon` requires 3 args (`item&, bool can_use_gun, bool use_silent`).
  - If `npcmove.cpp` defines `execute_llm_intent_action(...)`, `npc.h` must declare it.
  - LLM sleepiness snapshot code may need branch-safe fallback if `sleepiness` APIs diverge.

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
