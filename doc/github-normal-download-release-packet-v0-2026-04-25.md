# GitHub normal-download release packet v0

## Classification

GREENLIT / QUEUED AFTER `GitHub Actions CI recovery + checkpoint packet v0`.

Josef explicitly asked for an actual GitHub release so he can download C-AOL from GitHub the normal way at the first chance he gets.

## Summary

Prepare and publish the next real C-AOL GitHub release only after the current CI recovery lane has made the release footing honest.  The release should be downloadable from `josihosi/Cataclysm-AOL` in the ordinary GitHub Releases flow, with assets and release notes clear enough that Josef can grab it like a normal user instead of spelunking through local builds and archaeology.  Revolutionary concept, apparently.

## Current release footing

Schani checked the current public release list before packaging this queued item:

- latest stable release is `v0.2.0` / `Cataclysm - Arsenic and Old Lace v0.2.0`
- current release list also contains older/pre-release port and experimental builds
- the new release should not be cut while current `dev` Actions remain red for code-caused failures

## Scope

When promoted after CI recovery, Andi should:

1. Inspect current GitHub Releases, tags, and workflow state directly.
2. Decide the next release identifier from repo state and existing tags; if no obvious version/tag follows from canon, stop and ask Schani/Josef before publishing.
3. Ensure the release commit is a clean, reviewable `dev`/release-source checkpoint with CI state understood.
4. Build or collect the intended downloadable assets through the repo's release workflow or established release process.
5. Verify asset shape before publishing:
   - Windows asset exists and is downloadable
   - Linux asset status is known and recorded
   - macOS asset is either portable under the dylib guard or explicitly withheld/marked blocked; do not ship another app that silently depends on `/opt/local/...`
6. Publish the GitHub release only when the release notes and asset table match reality.
7. Record the release URL, tag, assets, checksums if available, and any known caveats in `TESTING.md` / release notes.

## Non-goals

- Do not cut the release before the active CI recovery lane has either gone green or honestly classified remaining red checks.
- Do not hide red CI by disabling workflows or deleting evidence.
- Do not claim macOS portability until the dylib guard passes or the macOS asset is explicitly withheld/blocked.
- Do not sign, notarize, contact upstream, or perform broader distribution/promotion unless Josef separately asks.
- Do not turn this into Lacapult work; this is the C-AOL game release.

## Success state

This queued packet is done only when:

- A new public GitHub release exists on `josihosi/Cataclysm-AOL` with a deliberate tag/version and clear release notes.
- The release assets match the stated platform support instead of implying broken platforms work.
- The release source commit and relevant Actions state are linked from canon/testing notes.
- Josef has a normal GitHub Releases URL he can download from.
- Any withheld/broken platform is plainly marked with the evidence-backed blocker.

## Testing and evidence expectations

Minimum evidence before publish:

- `gh release list` / tag inspection proves the next release identifier will not collide with existing releases.
- CI recovery packet is closed or remaining red checks are explicitly non-code / non-release-blocking.
- Release asset inspection proves the downloadable files exist and match the notes.
- For macOS, the dylib portability guard must pass for a shipped macOS app, or macOS must be withheld/blocked in notes.
- After publish, `gh release view <tag>` or equivalent proves the final release URL/assets.
