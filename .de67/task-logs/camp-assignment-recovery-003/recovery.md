# Camp assignment retained-process recovery

## Disposition

PID 90668 remains live and is explicitly handed to the coordinator for disposition. No native save/quit or OS exit was verified. No new game was launched and no registry state was changed. This recovery worker will send no further game input. No launcher-worker GUI clearance was sent.

## Identity and original binding

- PID 90668; birth `Wed Sep 23 21:32:37 2026`; PPID 1; executable `/Volumes/CodexBulk/Schanigarten/workspaces/Cataclysm-AOL/cataclysm-tiles`; args `--userdir .userdata/dev-harness/ --world McWilliams`.
- Harness session: `.userdata/openclaw_harness/bridge-sessions/selected-f76d72ee54a74b7cb4bde4b28fdac3af`.
- Run: `87364de72a12822f97a9a16ec2776d911687ee3b62ab512b0261c52174981122`.
- Binding: `50b95ea2011f6b171dabf3cfae1f6963d05ecbf4c14923aef4ce51e5c7be515d`.
- Scenario in `bridge.manifest.json`: R-029 rolling-save/reload continuity. It cannot establish the fresh camp assignment requirement.
- The session is `safe_to_cleanup`/terminalized; `play-client.json` has `pending_request: null` and all game operation availability false. Terminal cleanup verified PID 90677 exited; it does not cover PID 90668. PID 90668 remains present in `game-process.generations.jsonl`.
- Existing full transcript: `.userdata/openclaw_harness/bridge-sessions/selected-f76d72ee54a74b7cb4bde4b28fdac3af/playtest.txt`.

## Ownership and control checks

- Native helper `/root/camp_recovery_luna` is interrupted (`collaboration.list_agents`); old supervisor PID 1202 is absent. No current helper or harness bridge holds game action authority.
- Fresh process observation still matches the PID and birth above. Peekaboo `list windows --pid 90668` returned window 42355 titled `Cataclysm: Dark Days Ahead - 4d378c8e7c-dirty+SDL3`.
- Peekaboo bridge status: remote GUI host, socket `/Users/josefhorvath/Library/Application Support/Peekaboo/bridge.sock`, build 3.9.1, handshake succeeds; Accessibility, Screen Recording and Event Synthesizing are granted.
- `peekaboo see --pid 90668 --window-id 42355` timed out after 20 seconds. `peekaboo image --pid 90668 --window-id 42355 --mode window` succeeded; captured screen at `pid-90668-window.png` shows the live world view.
- Background `escape` and `type "S"` were delivered to PID 90668 but did not visibly change the captured screen.
- `peekaboo type "S" --pid 90668 --window-id 42355 --foreground` remained pending; its CLI was interrupted (exit 130) without a verified game action.
- `peekaboo app list --json` identified `cataclysm-tiles`, bundle `unknown`, PID 90668. `peekaboo app switch --to cataclysm-tiles --verify` failed with `App switch verification failed: frontmost is ChatGPT`.
- No native save receipt or exit evidence was produced. This is a control/activation blocker with granted permissions, not proof of game failure or process exit.

## Coordinator handoff

DE67 mailbox messages to `coordinator`:

- Status message `13b6faec933d432d83d4a98dba2f87c4` (pending): terminal harness binding, active PID discrepancy, and control attempt.
- Explicit handoff message `cc1df9b29022418286b3d06f065158ad` (pending): releases this worker's exclusive GUI/input ownership of PID 90668 to the coordinator with complete identity and blocker details.

First remaining behavioral boundary is native assignment to an eligible follower at the fresh camp's actual lookup OMT, followed by same-character save/reload continuity and ordinary work. No credit is claimed for this retained R-029 session.
