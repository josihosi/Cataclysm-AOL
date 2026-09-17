# Opt-in LLM diagnostics

In the game's LLM options, enable **LLM runner debug logging** (`DEBUG_LLM_INTENT_LOG`)
for a diagnostic run. It defaults to false. Turn it off afterward. The option controls
prompt/response logs, native intent/action-event logs, and the runner's `--log-file`
argument on both Windows and POSIX. A change to the option reconfigures the runner on
its next request. Existing logs are retained; turning logging off stops new writes.

For the standalone Python runner, explicitly supplying `--log-file PATH` opts into
diagnostics. Without it, no diagnostic log is opened. A harness debug run additionally
uses its run-owned `llm.action-status.events.jsonl`; ordinary gameplay has no such sink.
No empty-item-queue message is emitted every idle NPC turn.

Runner JSON records identify `process_instance`, `runner_pid`, `request_id`, timestamps,
and prompt/response hashes. A `run_id` is included only when the two harness run bindings
agree. Native debug events include the bound run, game turn and `wall_time_ms`. Primary
parse/actor eligibility, secondary inventory enqueue, and native intent execution events
show the links between model output and gameplay. Native intent execution records the
request, action enum, resulting panic/calm counters, wielded item and position; the enum
is defined by `llm_intent_action` in `src/npc.h`.

Use the actual primary response to choose the next observation. `look_inventory` is a
valid primary action: its item-specific wear/wield/activate/drop choice arrives through
a secondary request. Wait for that response and a native applying turn before inspecting
the consequence. A runner response proves generation, not native application. A single
pause issued while generation is pending can finish before the response arrives.

Correlate by bound run and runner process, not `req_0` alone. Preserve mixed outcomes and
preconditions: an instruction to wait is not an idle-only test, an already calm actor
cannot establish panic clearance, and a generic bow preference may fall back to another
weapon. Diagnose the missing stage before repeating a whole test campaign.
