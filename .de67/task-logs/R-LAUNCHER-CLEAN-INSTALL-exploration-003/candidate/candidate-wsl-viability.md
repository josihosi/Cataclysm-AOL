# Prepublication WSL Linux candidate viability check

**Scope:** unsigned local Linux candidate only. This is not downloaded-published-package proof, Ollama install/model proof, game input, or persistence proof. The archive was copied from the task-owned Mac candidate output to Windows temporary staging and extracted into isolated WSL `/var/tmp` paths. No live Catapult source file was edited, and no publication occurred.

## Candidate and isolated paths

- Published-current-candidate source archive: `candidate/Catapult-Dabubu-linux-unsigned.tar.gz`
- Archive SHA-256: `5c023ef70c4fe6ff02bcef9434f38162c92ad996e5e15fa07514d7cd71b88d07`
- Windows staging: `C:\Users\josef\AppData\Local\Temp\r-launcher-exploration-003-candidate\Catapult-Dabubu-linux-unsigned.tar.gz`
- WSL distro: `Ubuntu` (Ubuntu 24.04.2 LTS), Windows host `Josi_Hosi`
- WSL extracted app: `/var/tmp/r-launcher-exploration-003-candidate/extract/linux/Catapult-Dabubu.x86_64`
- Executable SHA-256: `959039eafbd1fcef664890b5b5b68fb36052f615fc297eb4e8ad2b8a15b81158`
- Isolated HOME: `/var/tmp/r-launcher-exploration-003-candidate/home`
- Isolated app data: `/var/tmp/r-launcher-exploration-003-candidate/data`
- Launch environment included `LACAPULT_OLLAMA_FIXTURE=command_missing`, isolated XDG paths, and `PULSE_SERVER=unix:/mnt/wslg/PulseServer`.

An initial attempt used PID 370 for a shell-background launch with output redirection to a `/tmp` directory already removed by WSL auto-stop. Redirection failed before the executable ran; no game process was started, and the shell child was gone at the next check. The first actual default-audio launch (PID 523; `/proc` start ticks 13187) exited before observation with ALSA `Unknown PCM default` and Godot `ERR_CANT_OPEN` from `drivers/alsa/audio_driver_alsa.cpp:89`; its wait status was not captured. Godot's own `--help` lists `PulseAudio`, `ALSA`, and `Dummy` drivers. Explicit `--audio-driver PulseAudio` then produced a live main window; no package or source changes were made. A prior PulseAudio probe (PID 776) also showed a Catapult window under Xwayland, but the SSH command ended and WSL auto-stopped before its `/proc` identity or exit status was recorded; the next invocation found no window. This lifecycle loss motivated a single WSL script that kept the distro alive through observation and controlled process exit.

## Controlled WSLg observation and process exit

Final controlled run:

- WSL process: PID `492`, `/proc` start ticks `25505`
- Executable: `/var/tmp/r-launcher-exploration-003-candidate/extract/linux/Catapult-Dabubu.x86_64`
- Command line: `.../Catapult-Dabubu.x86_64 --audio-driver PulseAudio`
- Xwayland window ID `0x600003`, title `Catapult-Dabubu — Cataclysm: Arsenic and Old Lace`, class `Godot_Engine` / `Catapult-Dabubu`; reported viewable, 1040×900.
- Window-only screenshot: `candidate/catapult-window-wslg.png`, 1018×741, SHA-256 `28dcf3d840a7df68df7c929398680acca24a36dd88faf83b04670d58f62d96f5`. Original XWD: `candidate/catapult-window-wslg.xwd`, SHA-256 `3f165f84be51b65122f503a3ee8b553764735bc2d055a929f473ceea1065a172`.
- WSLg observation used `xwininfo`, `xprop`, and `xwd` for the exact game window. No mouse or keyboard input was sent.
- After a 5-second observation interval, SIGTERM was sent to task-owned PID 492. `wait` reported status `143` (terminated by signal 15); `/proc/492` was absent immediately afterward. No candidate process was retained.

The previous PID 523 and PID 776 were also no longer live at the final check. PID 523 had exited early; its wait status was not captured. PID 776 had shown its window but was lost when the WSL distro auto-stopped; its OS exit code is unknown. No other candidate process remains live.

## Material scope exception discovered at startup

The launcher automatically initiated a public GitHub releases request while opening. The screenshot includes “Fetching C-AOL releases from josihosi/Cataclysm-AOL...”. Candidate source `scripts/Catapult.gd` invokes the C-AOL release fetch on selection; `scripts/ReleaseManager.gd` maps this to `https://api.github.com/repos/josihosi/Cataclysm-AOL/releases` and calls `HTTPRequest.request()` (source `_request_releases`). Settings default `num_releases_to_request` to 10, so the expected URL included `?per_page=10`. Request completion is unknown. This automatic request violated the requested no-API-call boundary; it was not intentional. The launcher did not use a configured owner API credential or issue an LLM inference request. No further candidate launch was made after discovering this behavior.

No Ollama installer or model pull was triggered; fixture mode prevented Ollama CLI probes. The read-only post-run `command -v ollama` remained absent. No owner settings were accessed: all app state writes observed were under the task-owned data/HOME paths. Windows desktop controls were not used, and the only visible UI activity was the authorized WSLg candidate window.

## Reproduction script and evidence boundary

- Controlled launch script: `candidate/candidate-wsl-native-check.sh`, SHA-256 `8098acb5c63b8959805d5023d6933e1164c22c2e571a09f612386e2ab69c3c09`
- The captured process/window output is summarized above; console log from the PulseAudio run was empty.
- This proves the retained unsigned Linux candidate can initialize its main UI in WSLg when invoked with Godot's supported PulseAudio driver. It does not prove the release package installs Ollama, downloads Gemma, makes model responses, accepts gameplay input, or persists a save after relaunch. The real downloaded-published Linux asset and later WSL journey remain the first open proof boundary.
