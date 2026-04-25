# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active target: `GitHub Actions CI recovery + checkpoint packet v0`.

- Re-inspect the latest failing `josihosi/Cataclysm-AOL` Actions directly with `gh run list` / `gh run view --log-failed`; do not work from vague red-badge memory.
- Current post-source checkpoint Actions: `IWYU` `24938191284` green and `Clang-tidy 18` `24938191292` green on `ca8eb0e3be`; `CodeQL` `24938191289` red is JSON/style for `data/json/mapgen_palettes/cannibal_camp.json`; Windows run `24939016251` on `471d4ef8e6` is green; the earlier Windows run `24938305397` was cancelled by a newer same-workflow request, not a source failure.
- Push/monitor the bounded TEST_DATA overmap coverage whitelist fix for fresh General run `24942284589`, which red-lined on rare `overmap_terrain_coverage` misses for `urban_dd_[1-6]` and `riverside_dwelling1(_roof)?`.
- If General stays red, classify the next failing job from fresh logs before widening beyond overmap TEST_DATA coverage scope.
- If the current General matrix stays red, separate remaining source/JSON/style/test-data failures from the intentional macOS dylib portability guardrail.
- If the current source-shaped runs turn green, update `Plan.md` / `SUCCESS.md` / `TESTING.md` and move toward the queued normal-download release packet only after remaining red checks are honestly classified.
- Use the full-history `/Users/josefhorvath/Schanigarten/Cataclysm-AOL` worktree and `josihosi/Cataclysm-AOL` as project truth; `josihosi/C-AOL-mirror` is green-dot-only and must not become planning/release truth.
