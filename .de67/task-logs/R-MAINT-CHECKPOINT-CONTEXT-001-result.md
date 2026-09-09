# R-MAINT-CHECKPOINT-CONTEXT-001 result

The installed DE67 `work_context.py` now emits an optional `checkpoint_worker_template` only when an explicitly queried task has one current live worker ownership binding.

The emitted argv uses the installed Python and `deadline_harness.py`, the resolved absolute state path, the requested lineage and task, and the actual worker ID. It deliberately omits `--kind` and `--evidence`, which remain caller-supplied values. The template is absent for unknown, unclaimed, terminal, and released tasks; executing a retained template after ownership changes remains rejected by `deadline_harness.py`'s existing ownership check.

Verification:

- `test_work_context.py`: 8 tests passed.
- `test_deadline_harness.py`: 94 tests passed.
- `python3 -m py_compile scripts/work_context.py`: passed.
- Live query for `R-MAINT-CHECKPOINT-CONTEXT-001` emitted the exact current worker binding `01a08737-18eb-7e90-89de-aa878869b7ae` with `optional: true`.

No game, harness session, or gameplay source was changed or launched.
