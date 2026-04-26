# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active target: `GitHub normal-download release packet v0`.

- Inspect current `josihosi/Cataclysm-AOL` releases, tags, and latest `dev` Actions directly before publishing; do not infer release state from memory.
- Use `c5ff712e01` as the current CI-green source checkpoint unless a newer committed checkpoint appears: General `24944793868`, Windows `24944793884`, CodeQL `24944793877`, IWYU `24944793878`, and Clang-tidy `24944793865` are green.
- Decide the next release identifier from existing tags/releases; if the version choice is not obvious, stop and ask Schani/Josef before publishing.
- Build or collect intended downloadable assets through the repo's established release path, then verify the asset table matches the release notes.
- Ship macOS only if the dylib portability guard passes; otherwise withhold or mark macOS blocked instead of publishing another `/opt/local/...` dud.
- After publish, record the final release URL, tag, source commit, relevant Actions links, assets/checksums if available, and any caveats in `TESTING.md` / release notes.
- Use the full-history `/Users/josefhorvath/Schanigarten/Cataclysm-AOL` worktree and `josihosi/Cataclysm-AOL` as project truth; `josihosi/C-AOL-mirror` is green-dot-only and must not become planning/release truth.
