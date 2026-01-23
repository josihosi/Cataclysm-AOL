# Agents

### Code style
- Code style is enforced by `astyle`.
- See `doc/c++/CODE_STYLE.md` for details.
- Quick astyle overview (C++):
  - K&R-style braces: `if( ... ) {` and `} else {`
  - 4-space indentation, no tabs
  - Space inside control parens: `if( ... )`, `for( ... )`
  - Pointer/reference style: `type *name`, `type &name`
  - One statement per line; consistent spacing after commas and around operators
- Run astyle on touched C++ files and fix any formatting regressions before building on Linux.
- Tell Josef to use and apply astyle 3.1 for `.astylerc`; newer versions reject options like `indent-preprocessor` and `add-brackets` (WSL/MSYS2 can supply 3.1).
- Ensure changes stay Linux-compatible (paths, APIs, and build steps).

### Automation and script portability
- CI workflows run many steps under `bash` (including Windows via MSYS2/Git-Bash).
- Prefer POSIX-compatible shell commands for scripts used by build or tooling.
- Avoid PowerShell- or cmd-only assumptions unless a workflow explicitly uses them.
- When adding new automation, keep paths and commands portable across Linux and Windows.
- Do not disable CI workflows; fix the root cause instead.

### Shell commands in this repo
- The default shell is PowerShell; avoid here-docs and complex quoting in `-Command`.
- Prefer one-line commands with explicit quoting, or use WSL for bash-specific scripts.
- When a command needs lots of quoting or newlines, create a temporary script file and run it.

## Tests and in-game verification
- Use unit tests where applicable.
- For gameplay changes, use the in-game debug menu to reproduce conditions.
- Validate builds with `just_build.cmd > debug.txt 2>&1` and `just_build_linux.cmd > debug.txt 2>&1` when touching code or build scripts.
- For LLM runner changes, run `tools/llm_runner/runner.py --self-test` from your venv.

For the full, canonical guidance, see `doc/CONTRIBUTING.md`.
