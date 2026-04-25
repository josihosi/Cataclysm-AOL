# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active target: `GitHub Actions CI recovery + checkpoint packet v0`.

- Re-inspect the latest failing `josihosi/Cataclysm-AOL` Actions directly with `gh run list` / `gh run view --log-failed`; do not work from vague red-badge memory.
- Monitor the current `b7d709d58c` / current recovery Actions runs: `Cataclysm Windows build` run `24936570455` is still in progress from the source/style checkpoint, and queued `General build matrix` run `24937345201` covers the docs checkpoint.
- If the current Windows run stays red, classify it from fresh logs before widening beyond the already-fixed C++17 test-construction source failure.
- If the current General matrix stays red, separate remaining source/JSON/style failures from the intentional macOS dylib portability guardrail.
- If the current source-shaped runs turn green, update `Plan.md` / `SUCCESS.md` / `TESTING.md` and move toward the queued normal-download release packet only after remaining red checks are honestly classified.
- Use the full-history `/Users/josefhorvath/Schanigarten/Cataclysm-AOL` worktree and `josihosi/Cataclysm-AOL` as project truth; `josihosi/C-AOL-mirror` is green-dot-only and must not become planning/release truth.
