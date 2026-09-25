# Publication candidate review

**Status:** all local preparation completed; waiting on explicit owner approval. No source commit, branch, tag, or release was pushed or published remotely. No live Catapult source file was changed. No WSL GUI/game was operated during this rebuild.

## Reviewable candidate source commit

- Repository: `https://github.com/josihosi/Lacapult-Doobdab`
- Base release: [catapult-dabubu-python-venv-fix-2026-06-05](https://github.com/josihosi/Lacapult-Doobdab/releases/tag/catapult-dabubu-python-venv-fix-2026-06-05). Its lightweight tag resolves directly to `366aee6bd675be9b45afbd2ab222808faceedd9d`; release metadata reports that same target commit. The old Linux asset SHA-256 is `b293a8fe35f160639d5e5d59dfe65166a380d8c048067bfd4356b59912034114` and lacks the Gemma/installer source changes.
- Candidate commit: `495ac43ef62f36685a941840405a4a8e25a1f03f`; parent `366aee6bd675be9b45afbd2ab222808faceedd9d`; tree `0fc6b9150aff981938e359a9fbe671ab4613bde0`.
- The exact original five-file feature patch is unchanged and verified byte-for-byte within this commit. Its SHA-256 is `3fba85a5e80497b44c84e1ae4f9207cf15440732ba90c3b61cf3d786b5e446d9`.
- The commit contains those five source files plus five Godot 3.6.3 export normalizations: four `.import` defaults and removal of two explicit window defaults. A headless settings query confirmed omitted defaults resolve to `resizable=True` and `borderless=False`. A subsequent full export left the candidate checkout clean.
- This is an isolated, local review commit authored as `Codex isolated release candidate <codex-candidate@localhost>`, not an owner-authored commit. It was not pushed. Commit patch and a Git bundle containing the candidate commit with base commit as prerequisite are preserved as `source-commit.patch` (SHA-256 `5441b971f4e2c17ea6fed1e1d97a428b3e7fe517bfb1f210f3fca8d8669d33e2`) and `source-commit.bundle` (SHA-256 `286726d12c6b1f5d60d91f2cc7b890c636708f964ef5ce86e60efce1ce82061e`). Bundle verification passed.

## Rebuild and focused verification

Godot 3.6.3 (`a3378686f`) export used the portable binary SHA-256 `b8603384613cb69d1fdc7047e476edd7e26ea61e44fd82a851ab07620dfa1528` and verified templates with hashes bound in `manifest.json`. All Mac, Linux, and Windows app exports and packages returned success; export presets were restored. A first probe without templates failed before producing app packages; copying the already verified templates into the isolated build HOME resolved it, and the complete rebuild succeeded.

- Godot workflow smoke passed on the candidate commit. It covered exact Gemma E2B/E4B choices, setup sequencing, confirmation gates, proof-only behavior, options persistence and runner dry-run without real Ollama/API requests, installation or model pulls.
- C-AOL backend contract proof passed against the local C-AOL workspace read-only; sandbox option writes remained in candidate `.proof-cache`. PATH excluded the installed Mac Ollama command, so the test did not query the Mac Ollama service.
- Godot 3.6.3 loaded the Mac package's exact exported PCK headlessly and verified embedded `res://utils/7za` (3,751,032 bytes, SHA-256 `52a55958c075e427e0fc30c814bc97edce717312a3c97fde3db543c5344735cf`) and its license. The PCK also contains the exact Gemma E2B/E4B tags, installer step, and official Linux installer URL. The three platform PCK export hashes are identical. Mac ZIP has no outer 7za sidecar; the executable and 7zip runtime are embedded in its PCK. The final manifest corrects the export helper's unconditional Mac `contains_7zip_sidecar=true` to `false` and records the embedded-resource verification. The headless inspection script and output are preserved as `godot-pck-inspection.gd` and `pck-runtime-resource-check.log`.
- Independent archive checks passed ZIP CRCs, ELF/MZ signatures, member counts, executable bits, utility/license presence, manifest checksums and SHA256SUMS matching. See `verify-package-shapes.py` and `package-shape-check.log`.

## Release candidate assets

Files are in this directory. `manifest.json` binds the package records to candidate commit `495ac43ef62f36685a941840405a4a8e25a1f03f`. `SHA256SUMS.txt` covers the three package archives.

| Asset | Bytes | SHA-256 | Shape |
|---|---:|---|---|
| `Catapult-Dabubu-macos-unsigned.zip` | 90,603,649 | `c47b8e2aadf3646d0d98a80a348d04ec7e5ba295ee371febd5d2866a1fd04c1f` | ZIP, 17 entries; app, Info.plist, executable and PCK; PCK runtime verified |
| `Catapult-Dabubu-linux-unsigned.tar.gz` | 37,579,544 | `f5a4c73f9e4a4b5f85ed99bfe56fe7e8553a73f52d3574b438008e0c91143bab` | tar.gz, 5 entries; executable ELF app, executable 7za and license |
| `Catapult-Dabubu-windows-unsigned.zip` | 68,745,801 | `b26cbb43f7f16ef0d4ff3227b263c3d2614c1f2e4f2652f15005249b01dba794` | ZIP, 3 entries; root MZ executable, 7za.exe and license |
| `manifest.json` | 13,276 | `ceeb319c95aa6e38f6e4627c67f208418e858f23bd72e9041553f1dbcfa05d8e` | Source revision, engine/templates, export outcomes and independent package checks |
| `SHA256SUMS.txt` | 308 | `8777d954db45d7d9b806860e0c8e602dfe2f033f591370c84433d6527da5a2e8` | Matching SHA-256 entries for the three package archives |

## Release tooling and limits

Root `README.md` requires builds from the final commit, GitHub upload, download-back verification, and platform smoke checks. `tools/prove_dabubu_export_packaging.py` builds local unsigned packages and manifests; it does not sign or publish. No active `.github/workflows` release workflow exists. The inherited signing workflow remains under `.github/workflows-disabled/`; its README says it is parked. Existing published packages are unsigned.

The proposed release tag `catapult-dabubu-gemma4-ollama-installer-2026-09-24` is absent from the remote release and tag-ref endpoints. Suggested push branch `codex/catapult-dabubu-gemma4-2026-09-24` is also absent (read-only 404). This prep does not satisfy the downloaded-published Linux asset requirement: WSL Gemma installation, model response, and game continuity are still pending publication and a later authorized WSL run.

## Exact approval action

Authorize pushing this exact local commit unchanged to `josihosi/Lacapult-Doobdab` on branch `codex/catapult-dabubu-gemma4-2026-09-24`, then creating tag `catapult-dabubu-gemma4-ollama-installer-2026-09-24` at commit `495ac43ef62f36685a941840405a4a8e25a1f03f` and a GitHub draft prerelease with the three archives, `manifest.json`, and `SHA256SUMS.txt`. Because signing/notarization is unavailable, approval must explicitly accept unsigned macOS, Linux, and Windows packages. After publication, download the Linux asset back and continue the WSL clean-install proof. No approval action has been taken here.
