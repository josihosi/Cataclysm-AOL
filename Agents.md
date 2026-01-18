# Agents

## Contribution Rules (Short)

### Licensing and authorship
- Cataclysm: Dark Days Ahead is released under CC BY-SA 3.0.
- Your contributions must be authored by you, or properly attributed when
  porting from another fork. Preserve original authorship when porting commits.

### Pull request summary
- Every PR must include a one-line `Summary` section for the changelog.
- Format:
  ```markdown
  #### Summary
  Category "description"
  ```
- Categories: Features, Content, Interface, Mods, Balance, Bugfixes,
  Performance, Infrastructure, Build, I18N.
- Use `#### Summary` `None` for minor tweaks that should not appear.

### Workflow basics
- Keep `master` clean; create a new branch per feature/fix.
- Do not merge your topic branches into `master`; update `master` from upstream.

### Code style
- Code style is enforced by `astyle`.
- See `doc/c++/CODE_STYLE.md` for details.

### Automation and script portability
- CI workflows run many steps under `bash` (including Windows via MSYS2/Git-Bash).
- Prefer POSIX-compatible shell commands for scripts used by build or tooling.
- Avoid PowerShell- or cmd-only assumptions unless a workflow explicitly uses them.
- When adding new automation, keep paths and commands portable across Linux and Windows.

### Translations
- Translations are handled via Transifex.
- See the translation project page referenced in `doc/CONTRIBUTING.md`.

### Tests and in-game verification
- Use unit tests where applicable.
- For gameplay changes, use the in-game debug menu to reproduce conditions.

For the full, canonical guidance, see `doc/CONTRIBUTING.md`.
