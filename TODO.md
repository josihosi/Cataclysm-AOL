# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active target: `GitHub Actions CI recovery + checkpoint packet v0`.

- Re-inspect the latest failing `josihosi/Cataclysm-AOL` Actions directly with `gh run list` / `gh run view --log-failed`; do not work from vague red-badge memory.
- Fix the observed code-caused CI failures first: C++17 test construction in `tests/faction_camp_test.cpp`, missing-declaration/internal-linkage failures in `src/basecamp.cpp`, and `bandit_playback::visibility_reads(...)` declaration/linkage shape.
- Classify and fix the Windows build failure only after isolating whether it is source, workflow/package, timeout/resource, or external runner noise.
- Distinguish intentional macOS dylib portability-guard failure from accidental CI breakage.
- Add one small CI checkpoint/linking safeguard so future Andi handoffs name changed file class, local gate, relevant Actions run link, and any remaining red-check classification.
- Use the full-history `/Users/josefhorvath/Schanigarten/Cataclysm-AOL` worktree and `josihosi/Cataclysm-AOL` as project truth; `josihosi/C-AOL-mirror` is green-dot-only and must not become planning/release truth.
