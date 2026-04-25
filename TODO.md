# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active target: `GitHub Actions CI recovery + checkpoint packet v0`.

- Re-inspect the latest failing `josihosi/Cataclysm-AOL` Actions directly with `gh run list` / `gh run view --log-failed`; do not work from vague red-badge memory.
- Current post-source checkpoint Actions: `IWYU` `24938191284` green and `Clang-tidy 18` `24938191292` green on `ca8eb0e3be`; `CodeQL` `24938191289` red is JSON/style for `data/json/mapgen_palettes/cannibal_camp.json`; fresh `General build matrix` `24938305401` on `4733c7a3e1` has `Basic Build and Test` red on JSON/style for `data/json/npcs/cannibals/classes.json`; `Cataclysm Windows build` `24938305397` is still in progress from fresh `gh run list`.
- Push the bounded JSON/style fix for `data/json/npcs/cannibals/classes.json`, then inspect the fresh CodeQL/Windows/General runs directly.
- If the current Windows run stays red, classify it from fresh logs before widening beyond the already-fixed C++17 test-construction/source-warning failures.
- If the current General matrix stays red, separate remaining source/JSON/style failures from the intentional macOS dylib portability guardrail.
- If the current source-shaped runs turn green, update `Plan.md` / `SUCCESS.md` / `TESTING.md` and move toward the queued normal-download release packet only after remaining red checks are honestly classified.
- Use the full-history `/Users/josefhorvath/Schanigarten/Cataclysm-AOL` worktree and `josihosi/Cataclysm-AOL` as project truth; `josihosi/C-AOL-mirror` is green-dot-only and must not become planning/release truth.
