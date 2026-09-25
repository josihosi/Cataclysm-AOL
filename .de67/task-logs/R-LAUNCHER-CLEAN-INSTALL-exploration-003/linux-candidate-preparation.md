# Linux package candidate preparation — exploration-003 continuation

## Staged source candidate

A clean local clone at `/tmp/r-launcher-linux-exploration-003/candidate-checkout` starts from `366aee6bd675be9b45afbd2ab222808faceedd9d` and has exactly these five modified files, copied from the Mac launcher source tree without editing that tree:

- `scripts/BackendConfigManager.gd` — SHA-256 `0cf504078623536667e21aa04d3ea8ab48c659e2a7ab5976b41cd47fe2a07f0e`
- `scripts/BackendSetupUI.gd` — SHA-256 `245c7a973b31688a855cfeb11abb47337be6c8b0ccfc61695b2a559dc5fda8b1`
- `scripts/path_helper.gd` — SHA-256 `834f475664ae4b6ecd83c8b97a284c72353a9b6a2295acc66431b5a3398b3e4d`
- `tools/godot_ollama_workflow_smoke.gd` — SHA-256 `9bccf88aba69e33f6ed77bf8b9e33c4abd4e05b6a32ea6fa37a0dda1273fff0c`
- `tools/prove_caol_backend_contract.py` — SHA-256 `0f8ee23840dded64e7a003668632e026ca60dff7e930983a87f317513f23c802`

The exact binary patch is retained at `launcher-five-file-diff.patch`, SHA-256 `3fba85a5e80497b44c84e1ae4f9207cf15440732ba90c3b61cf3d786b5e446d9`. The candidate checkout's combined `git diff --binary HEAD` digest matches this patch digest. No other source files were modified in the isolated checkout.

## Focused check

`python3 tools/prove_caol_backend_contract.py` passed in the temporary checkout. It verified the backend option contract and generated sandbox option fixtures for both exact Gemma tags. It made no API call, secret readout, model pull, OpenVINO install, or real user-config mutation. Its read-only local Ollama status probe observed the Mac's existing `/opt/homebrew/bin/ollama` command/server as present/running; it did not start or stop that owner service and this is not WSL evidence.

The Godot UI workflow smoke was attempted with `/opt/homebrew/bin/godot --path /tmp/r-launcher-linux-exploration-003/candidate-checkout --no-window --script tools/godot_ollama_workflow_smoke.gd`, with `HOME`, `XDG_CONFIG_HOME`, and `XDG_DATA_HOME` set beneath `/tmp/r-launcher-linux-exploration-003/isolated-home`. It timed out at 40 seconds without output. This does not establish that the script reached its first assertion.

## Engine/template binding and build obstruction

- Installed engine: `/Applications/Godot.app/Contents/MacOS/Godot`, app version `3.6.2`, binary SHA-256 `dbda86787cab9e7284d5e1fe67d9b22c475b6b9d25c214d7c16e788836fbcbb3`.
- Template version: `3.6.2.stable`. Needed release templates were available in the existing installation and copied into isolated HOME; hashes of the originals: `osx.zip` `27e1cf52e033c8b8649f215637180f2642a6961b30c4c4d8c3a7a159ba823553`, `linux_x11_64_release` `f65aad4ba949c18e211de94cacbf6da92c251dc11a6d48ff29217b886acb0d11`, `windows_64_release.exe` `fd1b18ccb23dc4ed4347bc4f2412eb3dae52a0b4104a153951e7cb6011cdc244`.
- `/opt/homebrew/bin/godot --headless --version` produced no output and exceeded the 20-second bound. The existing export route `python3 tools/prove_dabubu_export_packaging.py` was then run in the candidate checkout with isolated HOME and timed out at 20 seconds without output. Its first operation is `godot --version`; therefore it did not reach template inspection, temporary export-preset creation, pack/app export, or packaging. Full attempt record: `linux-package-export-attempt.txt`, SHA-256 `ab4aba9e69540db6cb1b61227a9484ae913583932b41bac10fc89f06795a8e3e`.
- No `export_presets.cfg` was left behind, and `.proof-cache/dabubu-export` was not created. The package workflow restores any prior preset in `finally`; in this run it never reached the preset write.
- No Linux candidate package was produced, so there is no candidate package checksum. The old published Linux package remains the separate historical asset with SHA-256 `b293a8fe35f160639d5e5d59dfe65166a380d8c048067bfd4356b59912034114`; it is not this candidate.

## Process/data reconciliation and next route

Godot invocations started only by this preparation were terminated after their respective bounds; a follow-up process inventory found no remaining Godot/export process. The existing Mac Ollama service was observed but left untouched. WSL's prior stopped-state/absence evidence remains in `wsl-linux-boundary.md`; this preparation did not start WSL or change it. The Mac launcher checkout and its five-file diff were left unchanged. No GUI, Windows desktop, or live game/harness session was opened.

First build blocker: the installed Godot 3.6.2 executable does not return even its version probe in this noninteractive route, and the current project packaging tool blocks at that probe before export. A bounded, owner-authorized route that makes this engine invocation usable (or another compatible Godot 3.6.2 engine/export environment) is needed before producing a Linux artifact. After a successful build, inspect the Linux package manifest and checksum in temporary output, then review before any publication. This is package preparation only; it cannot count as published-download, WSL installation, model-response, or WSL gameplay proof.

## Continuation: portable Godot 3.6.3 candidate export (current result)

The initial 3.6.2 noninteractive stall remains preserved above as a failed route. Following the coordinator's concrete tool route, the candidate export completed with the official portable 3.6.3 engine and matching 3.6.3 templates. This continuation supersedes the earlier interim “no candidate package” and “first build blocker” statements; it does not supersede the old published-package boundary or WSL proof ceiling.

The 3.6.2 sample record at `godot-3.6.2-dyld-sample.txt` (SHA-256 `8c22062f954a57e6e8effa7906829cfc5c7a81d5b620dff7ad644e222c348777`) captured the stalled arm64 process with 112 KiB footprint and all samples at `_dyld_start`; this was an engine launch/loader route failure, not an export/source defect.

- Engine: `/tmp/catapult-gemma-godot-3.6.3/engine/Godot.app/Contents/MacOS/Godot`, app version 3.6.3, CLI output `3.6.3.stable.official.a3378686f`, binary SHA-256 `b8603384613cb69d1fdc7047e476edd7e26ea61e44fd82a851ab07620dfa1528`.
- Templates: `3.6.3.stable`, copied from `/tmp/catapult-clean-install-home/Library/Application Support/Godot/templates/3.6.3.stable/` into this task's separate `/tmp/r-launcher-linux-exploration-003/isolated-home-3.6.3/`. Hashes (source and task-local copy match): `version.txt` `46c7a15dd4086b2ca7ec8e0dcd8bb2533af8a47ae1f18dd3b39fcff2abd65ed6`; `osx.zip` `d75ca256d1d930b4a8cdee5cce44194f8bb5a198eba60fb30bf63c3d60036851`; `linux_x11_64_release` `c35f24c2f83731a03c6439748e4ad83746105c89dbe7f63b1a6bcb220e5291f5`; `windows_64_release.exe` `0a581b1fcdd52db0dcceebe3896e92c403e66a2f8fff864fe581de2cb1c7f146`.
- Exact package command was the repository's `python3 tools/prove_dabubu_export_packaging.py`, with `GODOT_BIN` set to the 3.6.3 executable and `HOME`, `XDG_CONFIG_HOME`, `XDG_DATA_HOME`, and `CATAPULT_DABUBU_DATA_DIR` pointing beneath the isolated temp roots. The manifest reports 3.6.3, all required app templates ready, macOS/Linux/Windows app exports and unsigned packages each returned 0, and `temporary_export_presets_restored: true`.
- Linux app export: `.proof-cache/dabubu-export/app/Catapult-Dabubu.x86_64`, 66,772,896 bytes, SHA-256 `959039eafbd1fcef664890b5b5b68fb36052f615fc297eb4e8ad2b8a15b81158`; executable bit and embedded PCK confirmed.
- Reviewable unsigned Linux package copied into `candidate/Catapult-Dabubu-linux-unsigned.tar.gz`, 37,579,575 bytes, SHA-256 `5c023ef70c4fe6ff02bcef9434f38162c92ad996e5e15fa07514d7cd71b88d07`. The archive has five entries, executable `linux/Catapult-Dabubu.x86_64` and `linux/utils/7za`, the 7-Zip license, and passes the existing ready-unsigned-local-package shape check. `strings` inspection of the packaged executable found `gemma4:e2b` and `https://ollama.com/install.sh`, binding it to the staged source diff.
- Companion evidence copied under `candidate/`: `manifest.json` (full platform/template/export/package manifest), `SHA256SUMS.txt`, `Linux-App.log`, and `Linux-Pack.log`. The package checksum also matches the Linux line in that checksum file. The source patch remains `launcher-five-file-diff.patch`.

The focused Godot workflow smoke initially exited at its own guard because `CATAPULT_DABUBU_DATA_DIR` was unset, and the clean source checkout had not yet imported translated resources. No source edit was needed: the export imported project resources, then the smoke was rerun with that variable set to `/tmp/r-launcher-linux-exploration-003/launcher-data-3.6.3`. `godot_3.6.3-ollama-smoke.log` records exit 0 and “Ollama workflow UI smoke passed,” including both exact Gemma tags, save/reload, fixture readiness, inert proof-only install, and failure-state paths. The test data and HOME remained under `/tmp`.

This is a local unsigned preparation candidate only. No published asset was changed or downloaded, and no WSL install/model/game proof was attempted. The older published release and its SHA-256 remain preserved in `wsl-linux-boundary.md`. The old 3.6.2 probe remains in `linux-package-export-attempt.txt`. No Godot/export process remains; the Mac launcher source checkout, WSL state, Windows desktop, and existing Mac Ollama service were left unchanged.

The candidate checkout and generated export output remain in `/tmp/r-launcher-linux-exploration-003/` for source and manifest review. The automatic review gate rejected a requested `rm -rf` cleanup command, so I retained those isolated task artifacts rather than retrying cleanup through another mechanism.
