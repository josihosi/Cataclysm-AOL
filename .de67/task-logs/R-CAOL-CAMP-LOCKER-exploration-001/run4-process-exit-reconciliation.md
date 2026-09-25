# Run 4 process exit reconciliation

- Run `5a740fcaeaa49d27790568b1bba8d485cd8a3814c789fd748645e937dcbc6769`; binding `2cf23159899d5381211668a1e000a906240dc20c10962044149770f4e648b818`; isolated profile `r032-camp-locker-transfer-exp004-20260924`.
- Game PID 39695 birth `Thu Sep 24 04:15:10 2026`; bridge PID 39658 and runner PID 39660 birth `Thu Sep 24 04:15:08 2026`.
- After the inconclusive witness was sealed, `play finish` / `play collect` reported `Playtest ended.` Registry-bound broker status records `safe_to_cleanup`, terminalization accepted, bridge child exit code 0, cleanup `already_exited`, and `native_exit_credit:false`; `game-process-exit.json` records PID 39695 exit code 0 at `2026-09-24T02:50:26.171+00:00`.
- Recheck `ps -p 39695,39660,39658,90668 -o pid=,ppid=,lstart=,stat=,command=` showed only PID 90668; the exact run-4 game, runner, and bridge are absent. This confirms OS exit; the native exit credit remains false because the run was sealed with `run.finish` before a native save/quit segment.
- The run's retained diagnostic report is `.userdata/r032-camp-locker-transfer-exp004-20260924/harness_runs/20260924_041509_3ac095ef98dc48bfb8ebdf3c0641abcb/probe.report.json` SHA256 `e0f383a9d89ec46f10f74c180dd173f1f5bd48cbd33e0a633315af0b210676ae`; its bridge record is `.userdata/openclaw_harness/bridge-sessions/selected-6afb3d17280f4929adaab4ff89ab07f5/`.
- PID 90668 remains untouched and coordinator-owned.
