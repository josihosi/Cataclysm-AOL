# Shared harness performance alarms — implementation result

The current owner correction to repair shared harness execution and add performance alarms before native signal, camp, or travelling-hostile acceptance was applied for the implementation slice. Native `game::do_turn` now publishes run-bound start, end, and excluded-phase facts. The existing ProcessPerformance owner and `play_cli` pending-operation poller derive bounded measurements, explicit baseline/configuration status, spike, sustained, incomplete-turn, stalled-progress, recovery, and uncertainty results without submitting another native action.

Review found and the worker corrected malformed native JSON, insufficient epoch precision, input/save time leaking into simulation age, a blocking activity prompt outside the input phase, loss of alarms across internal polls, absence of no-start stall detection, and missing wall/CPU/throughput metrics. The final diff uses the existing semantic journal, performance owner, pending request, and session state instead of adding another service or durable truth owner. Obsolete batch-wall-time fields that claimed per-turn meaning were removed after repository search found no remaining consumer.

Verification on the final diff:

- `PYTHONPATH=tools/openclaw_harness /opt/homebrew/opt/python@3.14/bin/python3.14 -m unittest process_performance_test play_cli_test semantic_state_test r009_semantic_channel_compaction_test semantic_broker_test proof_classification_unit_test`: 257 tests passed.
- `make -j2 src/do_turn.o`: passed; the object was current on the coordinator rerun.
- `git diff --check`: passed.
- Repository search found no remaining `avg_turn_ms`, `per_turn_ms`, or `max_batch_turn_ms` consumer outside retained evidence/history.

No game, bridge, session, provider call, or native journey was launched. The result proves the implementation and controlled instrumentation contracts, not live alarm behavior or the later signal/camp and travelling-hostile outcomes. The next boundary is a source-bound pending native wait with a provenance-bearing scenario configuration and comparable baseline, followed by real completion and cleanup.
