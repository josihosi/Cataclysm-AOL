# R-LAUNCHER-CLEAN-INSTALL exploration-003 candidate review

Preparation only. No package was published and the live Catapult source tree was not edited.

## Source and build inputs

- Temporary checkout: `/tmp/r-launcher-linux-exploration-003/candidate-checkout`
- Five-file source patch: `launcher-five-file-diff.patch`, SHA-256 `3fba85a5e80497b44c84e1ae4f9207cf15440732ba90c3b61cf3d786b5e446d9`
- Existing source base: `366aee6bd675be9b45afbd2ab222808faceedd9d`
- Godot: official portable 3.6.3, `/tmp/catapult-gemma-godot-3.6.3/engine/Godot.app/Contents/MacOS/Godot`, SHA-256 `b8603384613cb69d1fdc7047e476edd7e26ea61e44fd82a851ab07620dfa1528`
- Export manifest: `candidate/manifest.json`, SHA-256 `7c9dfcd91a68e8b389fb008c5a0194cd3b440c4afe1f83f10e57a7c031798122`. All three app exports and pack exports returned 0; all required templates were found; temporary export presets were restored.

Godot 3.6.3 updated four `.import` sidecars by adding `process/normal_map_invert_y=false` and removed `window/size/resizable=true` and `window/size/borderless=false` from `project.godot` in the temporary checkout during export. These export-generated edits are outside the five-file patch and did not touch the live source tree. The packaged outputs are unsigned. This is a candidate build result, not a published release.

## Preserved package artifacts

All are in `candidate/`; checksums were recomputed after preservation and match `candidate/SHA256SUMS.txt` (SHA-256 `b2f40d4f0fe279a43e8d2aaedbc9a20ec1b4da187a9a8aca0bdfde6b5677a01c`).

| Platform | Artifact | Bytes | SHA-256 | Shape |
|---|---|---:|---|---|
| macOS | `candidate/Catapult-Dabubu-macos-unsigned.zip` | 90,603,649 | `be333113608e5a4149bc43e77548ab1011891239dd498c1e5142feea9bed89ca` | ZIP, 17 members, expected `.app` root present, 7zip sidecar present, unsigned local package ready |
| Linux | `candidate/Catapult-Dabubu-linux-unsigned.tar.gz` | 37,579,575 | `5c023ef70c4fe6ff02bcef9434f38162c92ad996e5e15fa07514d7cd71b88d07` | tar.gz, 5 members, executable `linux/Catapult-Dabubu.x86_64` and `linux/utils/7za`, 7zip license sidecar, embedded PCK |
| Windows | `candidate/Catapult-Dabubu-windows-unsigned.zip` | 68,745,801 | `ccb4658eba8a1b21d2bd555d854f461f7ae3a0ba7bde477e436237b20e03d439` | ZIP, 3 members, root executable present, 7zip sidecar present, unsigned local package ready |

## Boundary and rejected cleanup

This does not establish WSL installation or gameplay. The exact WSL distro is `Ubuntu`; the previously stopped-before-probe distro had no `ollama` command, and auto-stopped after that read-only absence check. It was not installed or launched during this candidate work. The old published Linux release remains tag `catapult-dabubu-python-venv-fix-2026-06-05`, source `366aee6bd675be9b45afbd2ab222808faceedd9d`, asset SHA-256 `b293a8fe35f160639d5e5d59dfe65166a380d8c048067bfd4356b59912034114`; it lacks the current Gemma/installer behavior. The prepared candidate is not a download from a published release.

An automatic review rejected this cleanup command before execution: `rm -rf /tmp/r-launcher-linux-exploration-003/candidate-checkout/.proof-cache /tmp/r-launcher-linux-exploration-003/candidate-checkout/.import /tmp/r-launcher-linux-exploration-003/launcher-data-3.6.3 /tmp/r-launcher-linux-exploration-003/isolated-home-3.6.3`. Exact gate reason: `rm -f style commands are not permitted. Use a safer approach`. No target was deleted; cleanup was not retried.
