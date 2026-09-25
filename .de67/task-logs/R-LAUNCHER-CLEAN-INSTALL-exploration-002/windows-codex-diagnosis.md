# Windows Codex startup diagnosis

Observed 2026-09-24 on `Josi_Hosi` as `josi_hosi\\josef`. This is a diagnosis artifact only; the broader Windows clean-install task remains live.

## Root cause: malformed owner config table

The preserved owner config is `C:\\Users\\josef\\.codex\\config.toml`. The read-only excerpt at lines 59–69 is:

- line 59: `[features]`
- line 60: `context_management.experimental_mode = true`
- following feature flags are scalar booleans.

Because the dotted key appears inside the `[features]` table, TOML makes `features.context_management` a nested table/map. The parser reports exactly `invalid type: map, expected a boolean in features`. Current official config guidance defines `[features]` entries as boolean feature toggles. The correct durable edit is an owner/mutator decision; this worker did not change the file.

Config remained at last-write UTC `2026-09-22T21:35:58.9340135Z`; final SHA-256 observed: `BF71A81776146CAD572AC9F73D706037BBCBE2AB7A500FC4FA1598E82F78C102`.

## Endpoint and existing failure record

- Mac helper: `/Users/josefhorvath/bin/send-to-windows-codex`.
- Windows endpoint: `C:\\Users\\josef\\bin\\codex-remote-run.ps1`.
- Endpoint resolves `%APPDATA%\\npm\\codex.cmd` and invokes `codex exec -c features.unified_exec=false --skip-git-repo-check --sandbox <mode> -C <cwd> -`. Its default sandbox is `danger-full-access`.
- The earlier helper call with `--help` treated that text as the agent prompt; the helper has no help option. Windows run record `C:\\Users\\josef\\codex-remote-runs\\20260924T002928Z-27316` contains prompt `--help`, status `FAILED`, exit `1`, and the config parse error at endpoint line 56.
- The helper advertised local record directory `/Users/josef/codex-remote-runs/windows-handoffs/20260924T002928Z-25024`, but that path was absent from the worker filesystem. The remote record remains intact.

A direct read-only Codex probe without the endpoint override still failed on the owner config, confirming the parse error is in the inherited owner configuration and not caused by `features.unified_exec=false`.

## Isolated tests and outcome

1. The installed CLI was `codex-cli 0.144.4`. With a temporary clean `CODEX_HOME` and a temporary copy of the existing `auth.json`, that CLI passed config parsing/auth but rejected `gpt-6-sol` with HTTP 400: unsupported for ChatGPT-account Codex.
2. I installed the officially released Codex CLI `0.156.1` in a task-only npm prefix, leaving the global npm install untouched. The temporary home used a minimal valid config, `history.persistence = "none"`, and the same temporary auth copy. Both isolated paths were removed afterward.
3. With that runtime/config, the exact Windows endpoint completed a read-only `Get-Location` command under its default sandbox. Record: `C:\\Users\\josef\\codex-remote-runs\\20260924T004958Z-22808`; status `DONE`, exit `0`. Output shows the command succeeded in 78ms and returned only the task-owned path `C:\\Users\\josef\\codex-remote-runs\\config-diagnosis-002\\cwd`.
4. With `--sandbox read-only`, both the endpoint run and a run with one-off `features.unified_exec=true` started on 0.156.1 but Windows execution policy rejected PowerShell process creation before command execution. Their Codex run records say `DONE/0`, but their outputs explicitly say the command was blocked; they are not accepted as command success. The task-scoped output folder `C:\\Users\\josef\\codex-remote-runs\\unified-exec-true-diagnosis-002` retains that transcript.
5. Current official changelog lists CLI 0.156.1 on 2026-09-23 and says its model catalog includes GPT-6 Sol and GPT-6 Luna. It documents `npm install -g @openai/codex@0.156.1`; no global upgrade was performed.

## Supported route and limits

A read-only command probe works without changing the owner config by setting a process-local `CODEX_HOME` to an existing task-owned directory containing a minimal valid `config.toml` plus an isolated auth copy, and process-local `APPDATA` to an isolated npm prefix with Codex 0.156.1. Invoke the canonical `codex-remote-run.ps1` endpoint with a task-owned cwd and a narrowly specified read-only prompt. The successful transcript proves this route. Official Codex documentation states `CODEX_HOME` selects the config/auth/state root and must already exist, and documents one-run config overrides.

For the ordinary helper to work against the owner's standard profile, the owner/mutator must correct the nested `context_management.experimental_mode` line in `[features]`; the global CLI should also be updated to a GPT-6-capable release. The read-only sandbox is not currently a working command route on this Windows host: the execution policy rejected even `Get-Location`. The only accepted probe used the endpoint's existing default `danger-full-access` mode, while the prompt limited it to that one read-only command.

## Cleanup and preservation

The isolated npm install, npm cache and temporary auth/config home were removed. Final checks found no processes using either temporary path; endpoint process IDs `38652`, `3872`, and `22808` were absent. The owner config, global Codex installation and active desktop were left unchanged. No game/package GUI was opened.
