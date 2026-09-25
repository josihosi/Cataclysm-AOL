# R-LAUNCHER-CLEAN-INSTALL-exploration-003 — WSL Linux boundary

Observed 2026-09-24 from Mac mini workspace `/Volumes/CodexBulk/Schanigarten/workspaces/Cataclysm-AOL`, using the authorized `windows-codex` one-shot SSH route. Windows identity was confirmed as `Josi_Hosi`; exact distro was `Ubuntu` (WSL 2), Ubuntu 24.04.2 LTS, x86_64. No Windows desktop UI was opened.

## Published Linux package provenance

- GitHub release: `josihosi/Lacapult-Doobdab`, tag `catapult-dabubu-python-venv-fix-2026-06-05`.
- Release API target/source: `366aee6bd675be9b45afbd2ab222808faceedd9d`.
- Linux asset: `Catapult-Dabubu-linux-unsigned.tar.gz`, 37,565,193 bytes; URL: `https://github.com/josihosi/Lacapult-Doobdab/releases/download/catapult-dabubu-python-venv-fix-2026-06-05/Catapult-Dabubu-linux-unsigned.tar.gz`.
- GitHub asset SHA-256 and locally downloaded SHA-256 both equal `b293a8fe35f160639d5e5d59dfe65166a380d8c048067bfd4356b59912034114`.
- Temporary download/extraction only: `/tmp/r-launcher-linux-exploration-003/`. Archive contains `linux/Catapult-Dabubu.x86_64` (stripped x86-64 ELF) and `linux/utils/{7za,7-ZIP_LICENSE}`. No archive install or launch was performed.

## Published binary versus current required source

The exact release source was cloned to `/tmp/r-launcher-linux-exploration-003/source` at the tag commit. Its `scripts/BackendConfigManager.gd` defines only Mistral/Nemotron choices, returns `manual_required` on Linux when Ollama is absent, and its installer action is proof-only. `scripts/BackendSetupUI.gd` says confirmation records intent and does not run package managers or pull models. ASCII strings in the actual Linux executable corroborate those behaviors and include no `gemma4:e2b` or `gemma4:e4b` marker. The executable does include `mistral:v0.3`, Nemotron markers, the proof-only/setup intent messages, and manual Linux install state.

The local current launcher checkout is `/Volumes/CodexBulk/Schanigarten/workspaces/Catapult Dabubu`, still at base HEAD `366aee6bd675be9b45afbd2ab222808faceedd9d` with five modified files. Its uncommitted `scripts/BackendConfigManager.gd` adds exact Gemma tags (`get_ollama_model_choices`, lines 186–198) and a Linux installer step using the official `https://ollama.com/install.sh` flow (`_ollama_installer_step`, lines 429–442), with install → CLI recheck → start → endpoint recheck → exact model pull sequencing (setup plan around lines 260–310). Source SHA-256 values: BackendConfigManager `0cf504078623536667e21aa04d3ea8ab48c659e2a7ab5976b41cd47fe2a07f0e`; BackendSetupUI `245c7a973b31688a855cfeb11abb47337be6c8b0ccfc61695b2a559dc5fda8b1`; `godot_ollama_workflow_smoke.gd` `9bccf88aba69e33f6ed77bf8b9e33c4abd4e05b6a32ea6fa37a0dda1273fff0c`. These are source-side only and are not in the published asset. The official Ollama Linux download page currently documents `curl -fsSL https://ollama.com/install.sh | sh`.

## WSL read-only preflight and display boundary

Before our check, coordinator preflight observed distro `Ubuntu` stopped and `command -v ollama` empty. Our read-only check independently found Ubuntu 24.04.2 LTS, `josef`, x86_64; no `ollama` command in PATH; no `/home/josef/.ollama` store; `ollama.service` inactive. `wsl --list --verbose` then showed Ubuntu Running because the read-only WSL command had started it. WSLg exposed `DISPLAY=:0`, `WAYLAND_DISPLAY=wayland-0`, `/mnt/wslg` and `/mnt/wslg/runtime-dir`; X11 `/tmp/.X11-unix/X0` and Wayland `wayland-0` socket existence checks both succeeded. This establishes a possible WSLg display route, not a rendered app or verified input session. WSL later auto-stopped; final `wsl --list --verbose` showed Ubuntu and docker-desktop Stopped. No persistent install, model, game, or other task-owned process remains.

Since the published package cannot perform the required Linux installer path or offer Gemma, no Ollama install/start, model download/response, API inference, package game installation, custom graphics, input, save/quit/relaunch, or persistence was attempted. Current local source is not a published/downloaded candidate and was not substituted. No Windows Ollama host result counts as WSL evidence.

## Mac owner-data incident reconciliation

The earlier exploration-001 Godot run used the real HOME once; these are current observed values, with no pre-run snapshot, so prior contents cannot be reconstructed. Nothing was deleted or restored. Paths, size, mtime UTC, SHA-256:

- `~/Library/Application Support/Godot/projects/Catapult Dabubu-7a45775a2c469809730bf5857f1f635e/project_metadata.cfg` — 177 bytes, `2026-09-24T01:26:52Z`, `520b969ab8c83832277b514a009bb7b4946ba052b3ea4cf711a12275e1b13e1c`.
- `~/Library/Application Support/Godot/projects/Catapult Dabubu-7a45775a2c469809730bf5857f1f635e/filesystem_cache6` — 56,794 bytes, `2026-09-24T01:26:53Z`, `83c0407e617a565234a8cd0879deca228c0acbc7be8e22a5038840d1b1cb34b6`.
- `~/Library/Application Support/Godot/editor_settings-3.tres` — 7,649 bytes, `2026-09-24T01:26:53Z`, `e5c3cdc0f17dea17329038aab8b9368a16b0e5d6b39210416fc2c42b973fa3cd`.
- `~/Library/Application Support/Godot/app_userdata/Catapult-Dabubu/logs/godot2026-09-24T01.26.02.log` — 1,355 bytes, `2026-09-24T01:26:02Z`, `ae3651766793a9a81174f977dbe36f93273e7040b46608cd3a6832bb0470ea97`.
- `~/Library/Application Support/Godot/app_userdata/Catapult-Dabubu/logs/godot2026-09-24T01.26.20.log` — 1,355 bytes, `2026-09-24T01:26:20Z`, same SHA-256 `ae3651766793a9a81174f977dbe36f93273e7040b46608cd3a6832bb0470ea97`.
- `~/Library/Application Support/Godot/app_userdata/Catapult-Dabubu/logs/godot.log` — 1,355 bytes, `2026-09-24T01:26:21Z`, same SHA-256 `ae3651766793a9a81174f977dbe36f93273e7040b46608cd3a6832bb0470ea97`.

Subsequent Godot runs are isolated under `/tmp`. C-AOL and launcher source diffs were not edited by this exploration.

## First open boundary / next route

A new versioned Linux asset must be packaged from the current launcher implementation, published with manifest and checksum, and downloaded/verified in a fresh WSL run. Then perform launcher-led Linux Ollama install/start, exact pulls and real Gemma responses; only proceed to the WSLg game release/graphics/input/save/relaunch journey once the downloaded game and app are current. The WSLg sockets exist, but native Linux app rendering/input has not been demonstrated. API inference requires an authorized secret source without exposing the value. This worker made no changes to coordinator records or claim status.
