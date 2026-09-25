# Windows clean-install exploration 002 — recoverable boundary

Observed 2026-09-24 on host `Josi_Hosi` as `josi_hosi\\josef`.

## Published package provenance

- Release: [catapult-dabubu-python-venv-fix-2026-06-05](https://github.com/josihosi/Lacapult-Doobdab/releases/tag/catapult-dabubu-python-venv-fix-2026-06-05)
- Launcher source commit: `366aee6bd675be9b45afbd2ab222808faceedd9d`.
- Windows archive: [Catapult-Dabubu-windows-unsigned.zip](https://github.com/josihosi/Lacapult-Doobdab/releases/download/catapult-dabubu-python-venv-fix-2026-06-05/Catapult-Dabubu-windows-unsigned.zip)
- Windows SHA-256: `b8eba3b2a02b4a018b940387e9629fbea4d0620f19598a3bf64b89bff1458f8a`; Windows-computed digest matched GitHub release asset digest. ZIP size: 66,754,537 bytes.
- Download: `C:\\Users\\josef\\Downloads\\Catapult-Dabubu-clean-install-002\\Catapult-Dabubu-windows-unsigned.zip`
- Extracted payload: `app\\Catapult-Dabubu.exe`, `app\\utils\\7za.exe`, and `app\\utils\\7-ZIP_LICENSE`.

## Release feature boundary

Read-only inspection of the exact published source commit's `scripts/BackendConfigManager.gd` shows `get_ollama_model_choices()` returns only `mistral:v0.3` and `nemotron-9b-dumber:latest`. Its `scripts/BackendSetupUI.gd` builds only Mistral v0.3 and Nemotron 9B options. The install confirmation explicitly describes this as a proof build that records intent and does not run package managers or pull models.

A read-only ASCII scan of the downloaded EXE found the existing Mistral, Nemotron, Ollama and BackendSetupUI markers, but neither `gemma4:e2b` nor `gemma4:e4b`. The app-created `dabubu_settings.json` contains `backend_ollama_model`, but no Gemma choices. These findings align with the source commit and establish that this published package cannot provide the required Gemma downloads or executable launcher-led runtime installation. No local rebuild was used.

Smallest release route: carry the current Gemma/setup implementation into the actual Catapult-Dabubu source repository, make sure the Windows setup path performs installation/pull rather than proof-only recording, run the repository's existing Windows packaging/export path, and publish a new versioned Windows asset with its manifest and SHA-256. Preserve this historical tag and artifact unchanged. External publication remains a separate decision.

## Safe Windows preflight and package-only attempt

- Exact C-AOL workspace: `C:\\Users\\josef\\dev\\Cataclysm-AOL`, `dev@4d378c8e7ccce140afc9a656f869ea9d06490916`; it was inspected read-only. Its preexisting dirty work was not modified.
- Before install, no Catapult user/profile install dirs were present in the checked Roaming/LocalAppData paths. The generic `%APPDATA%\\Godot` directory exists and was left untouched.
- Ollama was absent from PATH/command lookup; no matching process or service, `%USERPROFILE%\\.ollama`, `%LOCALAPPDATA%\\Ollama`, or `%USERPROFILE%\\AppData\\Local\\Ollama` store was found. No uninstall was needed.
- Environment variable names `OPENAI_API_KEY` were present at User and Process scopes. Values were never read or printed. This is a potential authorized credential source, but no API call was made because the published package/UI route was not usable.
- Launching the extracted EXE by SSH started PID 22716 at the package path. At 5 seconds it had an empty main-window title and HWND 0. A later OS process lookup confirmed PID 22716 absent; no matching Ollama process remained. No matching Windows Application crash entry appeared in the inspected 20-minute window.
- The launch created only package-adjacent `app\\dabubu_settings.json` (1,949 bytes; created/written `2026-09-24T00:30:37.7683218Z`). No Catapult AppData directory was created. The downloaded package directory is retained at the path above; no owner settings/data were removed or restored because none were modified.

## Interactive handoff boundary

The one-shot helper `/Users/josefhorvath/bin/send-to-windows-codex --help` unexpectedly invoked the Windows endpoint rather than showing help. It failed before Codex startup with:

```text
codex.cmd : Error loading config.toml: invalid type: map, expected a boolean
...
in `features`
```

Windows run record: `C:\\Users\\josef\\codex-remote-runs\\20260924T002928Z-27316\\output.txt`, status `FAILED`, exit code `1`. The Windows owner config `C:\\Users\\josef\\.codex\\config.toml` was not changed.

The wrapper advertised local record directory `/Users/josef/codex-remote-runs/windows-handoffs/20260924T002928Z-25024`, but that directory is absent in this worker filesystem; the invocation transcript was available inline only. The Windows-side output/status/exit-code record is readable and preserved. Do not treat the process launch as native UI proof or take over Josef's active desktop.

## Open boundary

No launcher-led game release/Python/Ollama installation, Gemma pulls or responses, API inference, native graphics/input, save, quit/relaunch, or persistence journey was performed. Windows proof needs both (1) a newly published package containing the current implementation and actual setup behavior and (2) an approved, verified Windows agent/UI route. This remains recoverable work; no coordinator terminal transition is made by this worker.
