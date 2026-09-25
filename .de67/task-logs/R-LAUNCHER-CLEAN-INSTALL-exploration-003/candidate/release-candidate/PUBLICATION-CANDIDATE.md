# Publication candidate review

**Status:** prepared for review, not publishable as-is. No GitHub tag or release was created, no source was committed, and the live owner source tree is unchanged.

## Provenance verified

- Source repository: `https://github.com/josihosi/Lacapult-Doobdab`
- Existing release: [catapult-dabubu-python-venv-fix-2026-06-05](https://github.com/josihosi/Lacapult-Doobdab/releases/tag/catapult-dabubu-python-venv-fix-2026-06-05), title `Catapult-Dabubu Python venv setup fix 2026-06-05`, published 2026-06-05, prerelease.
- Remote lightweight tag resolves directly to commit `366aee6bd675be9b45afbd2ab222808faceedd9d`; GitHub release `target_commitish` and the local candidate checkout HEAD agree. Commit subject: `Finish Dabubu canon cleanup`; committed 2026-06-05.
- The five-file patch is based on that commit and matches the staged source diff byte-for-byte. Patch path: `../launcher-five-file-diff.patch`; SHA-256 `3fba85a5e80497b44c84e1ae4f9207cf15440732ba90c3b61cf3d786b5e446d9`.
- Candidate export used official Godot 3.6.3 (`a3378686f`), executable SHA-256 `b8603384613cb69d1fdc7047e476edd7e26ea61e44fd82a851ab07620dfa1528`; three platform templates were present. Export/package results and artifact hashes are in `../manifest.json` (SHA-256 `7c9dfcd91a68e8b389fb008c5a0194cd3b440c4afe1f83f10e57a7c031798122`).
- Existing release path is manual: root `README.md` says build from the final commit, upload to GitHub, download the published assets back, and run platform smoke checks. `tools/prove_dabubu_export_packaging.py` creates local unsigned packages and a hash manifest; it explicitly does not publish or sign. No active `.github/workflows` release workflow exists. Inherited signing workflow is under `.github/workflows-disabled/` and its README says it is parked.

The old release's published asset digests match the preserved earlier boundary: Linux SHA-256 `b293a8fe35f160639d5e5d59dfe65166a380d8c048067bfd4356b59912034114`; macOS `67a80638cac9e60e293a25ca0d3739794820ac837b0d3eb27a2a9032db1b5733`; Windows `b8eba3b2a02b4a018b940387e9629fbea4d0620f19598a3bf64b89bff1458f8a`. The proposed tag `catapult-dabubu-gemma4-ollama-installer-2026-09-24` was absent from the remote release and tag-ref endpoints (read-only HTTP 404) and from the local tag list at review time.

## Hash-bound candidate assets

Files are in `../` (the candidate output directory):

| Asset | Bytes | SHA-256 | Shape |
|---|---:|---|---|
| `Catapult-Dabubu-macos-unsigned.zip` | 90,603,649 | `be333113608e5a4149bc43e77548ab1011891239dd498c1e5142feea9bed89ca` | ZIP, 17 entries; app root and 7zip sidecar |
| `Catapult-Dabubu-linux-unsigned.tar.gz` | 37,579,575 | `5c023ef70c4fe6ff02bcef9434f38162c92ad996e5e15fa07514d7cd71b88d07` | tar.gz, 5 entries; executable Linux app and `7za`; license sidecar; embedded PCK |
| `Catapult-Dabubu-windows-unsigned.zip` | 68,745,801 | `ccb4658eba8a1b21d2bd555d854f461f7ae3a0ba7bde477e436237b20e03d439` | ZIP, 3 entries; root executable and 7zip sidecar |
| `manifest.json` | 9,363 | `7c9dfcd91a68e8b389fb008c5a0194cd3b440c4afe1f83f10e57a7c031798122` | Three app exports and three package exports report success; presets restored |
| `SHA256SUMS.txt` | 308 | `b2f40d4f0fe279a43e8d2aaedbc9a20ec1b4da187a9a8aca0bdfde6b5677a01c` | Contains matching hashes for all three packages |

Suggested uploaded assets follow the old release layout: the three archives, `manifest.json`, and `SHA256SUMS.txt`. Draft text is in `DRAFT-RELEASE-BODY.md`; exact proposed tag/title/state are in `RELEASE-TAG.txt`.

## Remaining release blockers

1. The package README requires builds from the final source commit. The current five-file patch is uncommitted, and the manifest has no source commit field. The packages were exported from a temporary checkout whose export run also changed four `.import` sidecars and removed two project settings in `project.godot`. The live source is untouched, but these archives are not yet proven to be exports from a clean final commit. Commit/review the patch, normalize or account for export-generated metadata, rebuild from that exact clean commit, then replace the draft asset hashes.
2. Current packages are unsigned. This matches the previous release's unsigned package convention, but signing/notarization is not provided by the active tooling. Owner must accept unsigned artifacts or arrange signing before publication.
3. Publication is still required before downloaded-published-package proof can begin. The successful WSLg launch used the local unsigned Linux archive and does not satisfy that proof requirement.

## Exact final approval action

After blocker 1 is cleared and the new hashes replace this draft: authorize publishing a **draft prerelease** in `josihosi/Lacapult-Doobdab` under `catapult-dabubu-gemma4-ollama-installer-2026-09-24`, targeting the reviewed final source commit, with the three rebuilt archives plus `manifest.json` and `SHA256SUMS.txt`. If the intended artifacts remain unsigned, the approval must explicitly accept unsigned macOS, Linux, and Windows packages. Then download the published Linux asset and continue the WSL clean-install proof. No approval or publication action has been taken here.
