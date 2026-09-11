# R-037 task003 context finding

## Finding

The task packet did not require a nonexistent or incorrect observation. It explicitly assigned a
run-bound semantic item-action-status repair and required the source-defined block/skip or a
transfer contradiction (packet, `Outcomes` 2 and `Boundaries`; registry assignment `406b8213bb7d4e5682e351ff903354bc`).
The repository path also has the expected terminal consumer: `src/npcmove.cpp:2209` emits
`pickup.item_missing`, with the surrounding pickup precheck/zone-forbidden path at
`src/npcmove.cpp:2130-2210` and `5340-5392`. Thus the requested terminal status was a valid
proof target, subject to the selected target reaching the ordinary NPC consumer.

## Earliest preventable boundary

The worker's exact turns show successive fixture/readiness failures before the final missing
status: it first treated the empty stream as expected asynchronous delay (worker event log
`406b8213bb7d4e5682e351ff903354bc.jsonl`, worker message at line 1959), then found that seven
application turns never scheduled the NPC (line 2099), repaired the NPC move budget, found the
NPC unloaded by the model's `move=0,0 hold_position` (line 2278), and finally found unrelated
pickup AI consuming herbal tea during the second-stage request (line 2399). The worker repaired
each observed fixture/ordering issue and re-ran with the guard (lines 2478, 2886, and 3184).

After those repairs, the final run reached the primary E4B `look_around` response and the sole
bandage selection `item_1:a`, but still produced no native terminal status before the deadline
(worker lines 3431 and 3509; receipt `first_divergence` and artifact
`.userdata/harness-npc-llm-coverage/harness_runs/20260911_024415_c3d5ba08d4b54246bb5654f9417bf18b/llm.action-status.events.jsonl`).
The stream therefore establishes an instrumentation/proof gap at the secondary-response to
current-NPC queue/ordinary-move/consumer boundary, rather than proving that the source logger is
absent. `src/llm_intent.cpp`'s secondary response path (around 3659-3710) only installs item
targets after `g->find_npc(context.npc_id)` and `is_player_ally`; `src/npc.cpp:3192` clears the
look-around pending state. The final artifact does not show whether that lookup, queue install,
or subsequent NPC turn occurred.

## Assessment and uncertainty

The first preventable cause supported by the worker trace is missing fixture/dispatch readiness
at the item-selection-to-NPC-consumer boundary, discovered incrementally by the worker; it is
not a packet demand for an impossible event. The packet did authorize repository-owned fixture,
scenario, and semantic-observation repairs, so “separately authorized observation/dispatch
repair” in the receipt's `first_open_boundary` is imprecise: task003 already supplied that
authority. This does not identify which of `g->find_npc`/ally gating, target queue installation,
NPC scheduling, or status correlation failed in the final run. The receipt's no-transfer result
remains valid but cannot substitute for the missing run-bound terminal disposition.
