# Run 3 crash and cleanup reconciliation

Checked 2026-09-24 03:57:38 CEST, before any new game launch.

## Crash evidence

- Retained owned game PID 32384 had exact birth `Thu Sep 24 03:33:06 2026`; coordinator-provided sample is preserved at `run3-pid32384-sample.txt`.
- The live native crash alert was observed in `run3-crash-alert-see.png` / `run3-crash-alert-window.png`: `SIGSEGV: Segmentation fault`, version `4d378c8e7c-dirty+SDL3`, and named run-profile crash log path.
- The explicit alert was dismissed through the observed Peekaboo button `OK` (snapshot `1790214687310-4913`, element `elem_4`) because it was a crash report, not an unsaved-state decision. It closed the game window; the game then exited.
- Crash log is copied to `run3-crash.log`; original is `.userdata/r032-camp-locker-transfer-exp003-20260924/config/crash.log`. SHA-256: `d676e349a2588e7e1c5868d711300f1aabb580e5643d18a6e3ac85a569076599`.
- Crash log stack: `SIGSEGV` in `zone_manager_ui::display_zone_manager()::$_4::operator()(ui_adaptor&)`, called by `ui_adaptor::redraw_invalidated()` inside `query_popup::query_once()` / `query_yn()`, called from `maybe_offer_basecamp_smart_zoning` at `src/zone_manager_ui.cpp:159`, invoked after `mgr.add` at line 843–845.

## Broker / runner / game disposition

- Run `c3866db0b48e27320c386103dd2cb43f126075f258d4ba2f3e410d1e38afba73`; binding `4f9adc7d0787faf27ffbc1f51caf5164f1adf84bc31a170be54074b9452013a1`.
- Game PID 32384 expected `Thu Sep 24 03:33:06 2026`; bridge PID 32342 and runner PID 32344 expected `Thu Sep 24 03:33:05 2026`.
- After crash-dialog acknowledgement, `play quit` returned `Playtest ended.` and created a `safe_to_cleanup` marker. At 03:57:38, `ps -p 32384,32344,32342` returned no rows. The probe report and broker marker both record cleanup `already_exited`, observed generation `alive:false`, and `native_exit_credit:false` (correct: this was a crash, not graceful game exit).
- `probe.report.json` SHA-256 `d3149ddc51b9622248436ccaa7eeb01fba813ae0d23f13158674fdd01adb41e6`; `cockpit.bridge.safe_to_cleanup.json` SHA-256 `262f49d5259a80954c380d94f4f3d48b755a74746ae96ba67700ff3573dceacb`.
- PID 90668 remained alive with its original `Wed Sep 23 21:32:37 2026` birth and `.userdata/dev-harness/McWilliams` world. It remains coordinator-owned and was not operated.

No fresh game was launched before preserving these bindings and hashes.
