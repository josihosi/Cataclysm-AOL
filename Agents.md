# Agents
Follow all instructions in this file.

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

## Tests and in-game verification
- Use unit tests where applicable.
- For gameplay changes, use the in-game debug menu to reproduce conditions.
- Validate builds with `just_build.cmd > debug.txt 2>&1` and `just_build_linux.cmd > debug.txt 2>&1` when touching code or build scripts.
- For LLM runner changes, run `tools/llm_runner/runner.py --self-test` from your venv.

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