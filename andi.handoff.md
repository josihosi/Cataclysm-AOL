# Andi handoff: GitHub Actions CI recovery + checkpoint packet v0

## Classification and position

- classification: ACTIVE / GREENLIT NOW
- authoritative canon: `Plan.md`
- packet doc: `doc/github-actions-ci-recovery-checkpoint-packet-v0-2026-04-25.md`

## Item title

GitHub Actions CI recovery + checkpoint packet v0

## Implementation scope

1. Re-inspect the latest failing `josihosi/Cataclysm-AOL` Actions directly with `gh run list` and `gh run view --log-failed` before editing.
2. Fix observed code-caused failures first:
   - C++17/warnings-as-errors failures in `tests/faction_camp_test.cpp` from designated initializers and missing `source_utterance`
   - missing-declaration/internal-linkage failures in `src/basecamp.cpp`
   - missing-declaration/linkage shape for `bandit_playback::visibility_reads(...)`
3. Classify the Windows build failure from run `24931574609` as source, workflow/package, timeout/resource, or external runner noise before widening the fix.
4. Keep the macOS dylib portability guard honest: intentional guard failure is not accidental CI breakage, but accidental red workflows still need repair.
5. Add one lightweight CI checkpoint/linking safeguard so future Andi checkpoints name changed file class, local gate, Actions link, and remaining red-check classification.

## Non-goals

- no new game-feature behavior
- no broad CI rewrite unless a log proves it is the smallest honest fix
- no disabling tests/warnings/Actions to fake green
- no GitHub release/tag/signing/upstream contact
- no Lacapult work in this packet

## Success bar

- Current `dev` Actions are green for code-caused failures, or remaining red checks are explicitly classified with bounded non-code cause and next owner.
- C++17/missing-declaration failures are fixed.
- Windows failure is green or evidence-classified.
- CodeQL is green or evidence-classified as upload/config/external rather than sharing source failure.
- The CI checkpoint/linking rule exists in repo process docs/tooling.
- Plan/TODO/SUCCESS/TESTING match the final state.

## Testing / evidence expectations

Minimum before claiming closure:

- `git diff --check`
- narrow local build/test that covers the C++17 and missing-declaration fixes
- relevant filtered tests for touched areas
- post-push `gh run list` / `gh run view` evidence for Actions state

If a CI gate cannot be reproduced locally on Josef's Mac, cite the exact workflow/job/run URL and decisive failing lines.

## Cautions

Do not treat this as “fix all tests, somehow.”  The first job is to collapse the current red wall into named causes and remove the obvious code-caused ones.  A smaller honest green path beats another grand heroic fog machine.
