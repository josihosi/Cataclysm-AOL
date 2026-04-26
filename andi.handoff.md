# Andi handoff: GitHub normal-download release packet v0

## Classification and position

- classification: ACTIVE / GREENLIT NOW
- authoritative canon: `Plan.md`
- packet doc: `doc/github-normal-download-release-packet-v0-2026-04-25.md`

## Item title

GitHub normal-download release packet v0

## Implementation scope

1. Inspect current `josihosi/Cataclysm-AOL` releases, tags, and latest `dev` Actions directly before publishing.
2. Use the current CI-green source checkpoint unless a newer committed checkpoint appears: `c5ff712e01` has green General `24944793868`, Windows `24944793884`, CodeQL `24944793877`, IWYU `24944793878`, and Clang-tidy `24944793865`.
3. Decide the next release identifier from existing tags/releases; if the version choice is not obvious from repo state, stop and ask Schani/Josef before publishing.
4. Build or collect the intended downloadable assets through the repo's established release path.
5. Verify asset shape before publishing:
   - Windows asset exists and is downloadable
   - Linux asset status is known and recorded
   - macOS is either portable under the dylib guard or explicitly withheld/marked blocked
6. Publish the GitHub release only when release notes and asset table match reality.
7. Record the release URL, tag, source commit, relevant Actions links, assets/checksums if available, and any known caveats in `TESTING.md` / release notes.

## Non-goals

- no hiding red CI by disabling workflows or deleting evidence
- no shipping macOS if the dylib portability guard fails
- no signing, notarization, upstream contact, broader distribution/promotion, or Lacapult work unless Josef separately asks
- no repo-role surgery; `/Users/josefhorvath/Schanigarten/Cataclysm-AOL` and `josihosi/Cataclysm-AOL` are project truth

## Success bar

- A new public GitHub release exists on `josihosi/Cataclysm-AOL` with a deliberate tag/version and clear release notes.
- The release assets match the stated platform support instead of implying broken platforms work.
- The release source commit and relevant Actions state are linked from canon/testing notes.
- Josef has a normal GitHub Releases URL he can download from.
- Any withheld/broken platform is plainly marked with the evidence-backed blocker.

## Testing / evidence expectations

Minimum before/after publish:

- `gh release list` / tag inspection proves the next release identifier will not collide with existing releases.
- Release asset inspection proves the downloadable files exist and match the notes.
- For macOS, the dylib portability guard must pass for a shipped macOS app, or macOS must be withheld/blocked in notes.
- After publish, `gh release view <tag>` or equivalent proves the final release URL/assets.

## Cautions

This is a real public GitHub release lane, so do not treat version choice, asset claims, or macOS support as vibes.  If the tag/version or platform asset story is ambiguous, stop and ask instead of publishing a haunted souvenir.
