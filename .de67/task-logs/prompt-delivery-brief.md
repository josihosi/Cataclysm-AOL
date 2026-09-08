# R-MAINT-PROMPT-DELIVERY-001

Repair the specific stale prompt boundary in installed de67 Phase3 `scripts/coordinator_supervisor.py`
and its tests. `run_coordinator` calls its process-loaded `coordinator_prompt`. The external supervisor
survives mutation while the script file is changed, so a fresh Codex child can still receive old role
text. This review supplies its immediate instruction through the existing exact restart reason;
that is recovery, not a repeatable freshness fix.

Make the next owned child receive the current guarded role contract without restarting the parent.
Choose the smallest compatible loading/rendering route. Preserve exact current invocation arrays,
owner stop, claimed restart generation/reason, and exclusive parent launch/acknowledgement. No live
coordinator, extra restart, change to policy/clock/guard, credential/config change or global access
is authorized. Broader method edits stay with the exclusive reviewer.

Use a fake runner: load supervisor, change the on-disk role text, request the next child and inspect
its actual supplied prompt. Prove visible stale/corrupt-input failure and no duplicate child. Preserve
the current complete-contract injection test rather than asserting arbitrary prose wording. Return
source and tests with evidence limits; ordinary Sol adoption is not proved by this isolated test.
