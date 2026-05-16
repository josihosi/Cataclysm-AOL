# GitHub Actions CI recovery + checkpoint packet v0

## Classification

CLOSED / CHECKPOINTED.

This packet was the C-AOL execution target after Josef asked to move back from Lacapult/Doobdab rhythm and get the failing GitHub Actions under control.  It closed on `c5ff712e01` with green General `24944793868`, Windows `24944793884`, CodeQL `24944793877`, IWYU `24944793878`, and Clang-tidy `24944793865` Actions.

## Summary

Recover the current `dev` branch GitHub Actions from the observed red state, then leave a small durable checkpoint rule so future Andi work links each meaningful code change to the relevant local/CI-equivalent gate before it is treated as reviewable.  This is not “make tests green by turning them off”, bitte sehr; it is diagnose the actual red checks, fix code-caused failures, and make the next red build harder to smuggle past as vibes.

## Observed failure footing

Schani inspected the latest relevant Actions before packaging this item:

- `General build matrix` on `dev` run `24931588361` failed on `4432bb9a8c Document macOS dylib portability gate`.
- Earlier same-family `General build matrix` run `24931574601` failed on `2ff3b32b0a Fail macOS releases with unbundled local dylibs`.
- `Cataclysm Windows build` run `24931574609` failed on `2ff3b32b0a`.
- `CodeQL` run `24932236370` failed on `master` at `86f786bee5`; its C++ extraction shows the same missing-declaration family in `src/basecamp.cpp`.
- `Clang-tidy 18` and `IWYU` around the same commits were green, so this is not “everything is broken equally”; the current red surface is build/CodeQL/Windows/package-flow shaped.

Decisive log examples from the failed runs:

- `tests/faction_camp_test.cpp:6118` and neighbors use C++20 designated initializers and omit `source_utterance`; CI builds as C++17 with warnings as errors.
- `src/basecamp.cpp` has many `-Werror=missing-declarations` failures for camp-locker helper functions.
- `src/bandit_playback.cpp` reports a missing declaration for `bandit_playback::visibility_reads(...)`.
- The Windows build compiles deep into MSVC/test build, then exits `1`; treat it as a real investigation target, not as proof of a source compile error until the failing step is isolated.
- macOS packaging is now expected to fail on unbundled package-manager dylib paths if a release app still links `/opt/local/...`; distinguish an intentional portability guardrail from accidental CI breakage.

## Scope

Andi should:

1. Pull the current `dev` branch and re-inspect the latest Actions state with `gh run list` / `gh run view --log-failed` before editing.
2. Fix current source/test failures that are clearly code-caused:
   - replace C++20 designated initializers in C++17 tests with explicit C++17 construction including `source_utterance`
   - give internal helpers proper internal linkage / declarations instead of tripping `-Wmissing-declarations`
   - keep the fixes narrow and reviewable
3. Investigate the Windows failure far enough to classify it honestly as source, workflow/package, timeout/resource, or external runner noise; fix only the bounded cause found.
4. Distinguish intentional macOS portability-guard failure from accidental red CI, and adjust workflow/docs only if the guard is making the wrong check red.
5. Rerun the narrow local gates that match the failure class, then push a clean/reviewable checkpoint and monitor the matching GitHub Actions.
6. Leave one small CI checkpoint/linking safeguard in repo process docs or helper tooling so future Andi handoffs name:
   - changed file class
   - local gate run
   - relevant Actions workflow/run link when available
   - any remaining red check and its classification

## Non-goals

- No new game-feature behavior in this packet.
- No broad CI rewrite, runner migration, or build-matrix redesign unless a failing log proves it is the smallest honest fix.
- No disabling warnings/tests/Actions to manufacture green.
- No GitHub release, tag, signing/notarization, upstream contact, or public release work.
- No reopening Lacapult work inside this packet.

## Success state

This packet is done:

- [x] Current `dev` Actions are no longer red for code-caused C-AOL failures, or every remaining red check is explicitly classified with a bounded non-code cause and next owner.
- [x] The C++17/warnings-as-errors failures are fixed: no designated-initializer/missing-field failures in `tests/faction_camp_test.cpp`, no missing-declaration family in `src/basecamp.cpp` / `src/bandit_playback.cpp`.
- [x] Windows build failure is either green or reduced to a named, evidence-backed workflow/runner/package blocker.
- [x] CodeQL is green or its remaining failure is classified as upload/config/external rather than silently sharing the same source compile failure.
- [x] The repo contains a lightweight CI checkpoint/linking rule so future reviewable Andi commits do not say “tested” without naming the relevant gate.
- [x] `Plan.md`, `TODO.md`, `SUCCESS.md`, and `TESTING.md` match the final state.

## Testing and evidence expectations

Minimum local evidence before claiming a fix:

- `git diff --check`
- the narrow build/test that reproduces or prevents the C++17 and missing-declaration failures
- relevant filtered test runs for edited test/source areas
- `gh run list` / `gh run view` evidence for the post-push Actions state

If a CI gate cannot be reproduced locally on Josef's Mac, record the exact workflow/job/run URL and the decisive failing lines instead of pretending local smoke is equivalent.
