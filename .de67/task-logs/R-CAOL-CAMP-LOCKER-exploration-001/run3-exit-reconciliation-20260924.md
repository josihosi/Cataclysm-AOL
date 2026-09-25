# Run 3 process exit reconciliation

Checked 2026-09-24 after coordinator reported all three run-3 PIDs absent.

- Run: `c3866db0b48e27320c386103dd2cb43f126075f258d4ba2f3e410d1e38afba73`
- Binding: `4f9adc7d0787faf27ffbc1f51caf5164f1adf84bc31a170be54074b9452013a1`
- Exact prior births: game PID 32384 at Thu Sep 24 03:33:06 2026; bridge PID 32342 and runner PID 32344 at Thu Sep 24 03:33:05 2026.
- `status.json`: `safe_to_cleanup`, terminalization accepted, `child_exit_code: 0` (bridge child/runner), cleanup `already_exited`, `native_exit_credit: false`; process-generation observation `alive:false`.
- `game-process-exit.json`: game exit code `-6`, observed at `2026-09-24T01:51:34.554+00:00`, consistent with crash/SIGABRT path already captured in run3 crash reconciliation.
- Rechecked with `ps -p 32384,32344,32342 -o pid=,ppid=,lstart=,stat=,command=`; no rows returned. Their absence confirms OS exit after the bridge's recorded cleanup observation.
- Source records: `.userdata/openclaw_harness/bridge-sessions/selected-ffa1d60af4c54da9a700f75ab722a2fb/{status.json,game-process.json,game-process-exit.json,game-process.generations.jsonl,cockpit.bridge.safe_to_cleanup.json}` and `.userdata/r032-camp-locker-transfer-exp003-20260924/harness_runs/20260924_033305_7e4f354919de45b8a9f850ee63d19402/probe.report.json` (SHA256 `d3149ddc51b9622248436ccaa7eeb01fba813ae0d23f13158674fdd01adb41e6`).
- Interpretation: this run ended abnormally with no graceful native-exit credit; it is closed at OS/process and broker level, not gameplay evidence. No signal was sent to those PIDs during reconciliation.
- Separate protected process: PID 90668 remains alive at its original birth Wed Sep 23 21:32:37 2026 under `.userdata/dev-harness/McWilliams`, owned by coordinator and untouched.
