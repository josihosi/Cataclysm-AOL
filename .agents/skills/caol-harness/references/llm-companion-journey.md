# E4B companion journey — continuous native playtest

Owner-selected protocol, 2026-10-05. Current execution and host readiness belong in the assignment/results. This is the configured-LLM arm of the [product suite](playtest-suite.md#coverage-repeat-starts-and-current-results), not a separate test framework or dispatch. Sol oversight assigns one direct GPT-6 Luna game owner, who travels with the same allied NPC and returns issues to that Sol without spawning another player or chaperone. Sol diagnoses without acquiring input. No coordinator message or new game is implied by this document.

## Scenario intent and setup latitude

This is the model-mediated category of the [suite](playtest-suite.md#scenario-intent-and-closed-categories).
The trip supplies connected context for commands, changed orders, inventory, danger and memory;
incidental avatar walking is not a separate acceptance test. Preserve actual NPC follow/move,
scavenge and danger responses and the prepared-start rules below. General teleport/setup advice
does not authorize changing protected prompts, injecting responses or bypassing the tested
command. Natural surprises are valuable inputs to the evaluation; neither perfect obedience nor
a prescribed collection of random encounters is required. Finish the selected command journey
or identify a concrete failure/blocker, rather than ending at an uneventful partial trip.

## Outcome and protected instructions

Play a coherent supply trip: prepare at shelter, travel, scavenge, face danger, regroup and return. Try every supported command route, meaningful context variations and supported chains while keeping the NPC a believable zombie-apocalypse survivor. Josef wants roughly 60–90% practical reliability with personality, not robotic obedience. This is a qualitative target plus descriptive results, not a statistical guarantee, failure quota, upper cap on good performance, or permission to inject mistakes. Technical failures are not character flavour.

Apply only the instruction changes explicitly selected by Josef for the assigned run; otherwise keep his system/templates, defaults, snapshot representation, response grammar and character instructions unchanged. Do not alter model/sampling settings to improve results, add evaluation coaching to the NPC, substitute synthetic model replies, or rewrite its dialogue. Use ordinary in-world speech. Preserve the loaded configuration identity before and after, without reproducing prompt bodies or secrets in reports. Evaluate the owner-selected configured model; record its actual provider, model, endpoint and configuration identity, with a digest where available rather than assuming aliases match.

Model understanding and character problems are report-only. The owner permits runner repairs for demonstrated runner issues. Separate diagnosis from that repair: preserve the first failing request/output, identify the runner cause, use the existing implementation owner and validate the relevant failure before a safe continuation. Do not replace code beneath a live process. NPC motor, gameplay policy, prompt, grammar or snapshot changes are not authorized by this protocol. An unresolved issue there is reported, not quietly fixed. A repair/reload creates an explicit new evidence segment; it cannot erase the first attempt.

## Prepared start and actual readiness

Reuse the existing [R037 command scenario](../../../../tools/openclaw_harness/scenarios/r037.npc_llm_command_coverage_v001_mcw.json) and [fixture manifest](../../../../tools/openclaw_harness/fixtures/saves/live-debug/r037_npc_llm_command_sandbox_v001/manifest.json) as preparation seeds. The fixture derives from `harness_basecamp_no_giuseppe_2026-09-05`; its directory is a transformation recipe, not proof of a complete retained save. Resolve and retain the actual source bytes through the existing registry before launch. Use a disposable isolated copy; leave standardized originals unchanged.

The recipe supplies allied NPC 2, revolver/ammo, machete, bow/arrows, shirt, lighter and rock; unowned/foreign-owned bandages and a NO_NPC_PICKUP zone. It also supplies a long-lived initial panic effect and clears nearby monsters. Identify the live actor/items rather than blindly reusing these historical IDs. Start by acknowledging and testing the existing panic, not claiming a calm baseline. Record fixture intervention as setup with zero model/gameplay credit.

The historical scenario requires macOS/Tiles and has Mac Python/model option paths. Bind preparation to the execution host selected by the current owner contract: its supported native build/transport, actual E4B runner and model, compatible save bytes and isolated writable roots through existing harness machinery. A different host is not a prerequisite. Terminal mode is suitable only where speech, observation, inventory and interruption controls are supported; required visual proof retains its supported UI lane. Historical paths and profiles do not establish readiness on another host.

Choose an ordinary reachable scavenging route and retreat shelter from the actual map. The safe fixture alone cannot supply combat coverage: find a real encounter outside its cleared area, or use a documented compatible prepared derivative with a safe departure and later danger. Do not splice separately launched safe/combat fixtures into a claimed continuous journey. Missing terrain, equipment or threat is an explicit preparation/coverage gap, not permission to mutate live state. Avoid hostile NPC targets that would introduce a separate faction campaign; ordinary zombies suffice.

The start brief contains only current host/repo/source/build, selected scenario and byte-backed save, run/session/binding, game/runner process identities, NPC identity, loaded model/instruction identity, current pending state, route and evidence/performance handles. Reuse accepted preparation; don't rediscover historical directories. Shared model services remain owned by their existing operator; this arm may use them but does not stop/reconfigure them for other games.

## Native control and batching route

Use [selection and launch](selection-and-launch.md), [live operation](live-operation.md), and [speech handling](setup-and-interactions.md#speech-and-npc-replies). Existing CLI entry points, with SESSION replaced by the selected live session, are:

```sh
python3 tools/openclaw_harness/play_cli.py --session SESSION look
python3 tools/openclaw_harness/play_cli.py --session SESSION controls
python3 tools/openclaw_harness/play_cli.py --session SESSION act ACTION --target STABLE_ID
python3 tools/openclaw_harness/play_cli.py --session SESSION collect
python3 tools/openclaw_harness/play_cli.py --session SESSION inspect SELECTOR
python3 tools/openclaw_harness/play_cli.py --session SESSION messages --contains TEXT
```

ACTION, STABLE_ID and SELECTOR come from the actual returned controls/facts. Omit target when the action has none. Speech/menu text submission uses the currently advertised prompt schema; do not invent a universal speech command or reuse old letters/frame IDs. Internal command tokens below are coverage labels, not strings to inject as model output.

For each normal request batch: observe the relevant before-state → open ordinary speech and submit the in-world request → collect any pending harness request → correlate runner request/response → resolve any modal input → advance an applying native World turn through advertised controls → inspect actual NPC effect. A response from the runner is not proof the NPC acted. Read-only look and wall-clock sleep do not advance game time. Serial hearers can require applying one reply before the next request starts; keep the intended companion as the identified recipient.

Batch safe unchanged travel and suitable waits using current advertised macros. Do not merge across a reply/application boundary, new threat, inventory mutation, prompt, or the timing window being tested. The special pending-response tests intentionally advance ordinary gameplay before the model reply; keep their chronology. Never resubmit merely because inference is pending. Inspect whether a second deliberate order is accepted separately.

Keep actual actions and results in the existing run report. Promote a missing reusable command segment only under the [manual maintenance rule](../GAME-MANUAL.md#add-prune-and-correct-notes), with live bindings parameterized and meaningful interruption boundaries. Do not transcribe each successful segment into the protocol or create a separate macro system.

## Journey and command coverage

Examples are natural-language suggestions, not exact responses the NPC must produce. Adapt to its established voice and what is physically present. Each stage has a purpose; the player may reorder safe cases to fit the story, and should not stop at the first unexpected answer.

| Stage | Requests and variations | Native evidence / meaningful end |
| --- | --- | --- |
| 1. Shelter, orientation and panic | Talk about the supply trip; hear its response without demanding an action (`idle`). Address the fixture's actual panic: “Take a breath. We're safe here.” (`panic_off`). Establish usable equipment and a safe retreat. | Actual initial panic/effect versus changed state, reply tone, selected action and native application. Saying “calm down” to an already calm NPC does not prove clearing. Record whether personality is established by the existing game or merely inferred from dialogue. |
| 2. Prepare equipment | Ask for gun, melee and bow in turn (`equip_gun`, `equip_melee`, `equip_bow`). Ask to check supplies (`look_inventory`), put on clothing (`wear`), wield a particular item (`wield`), use an appropriate usable item (`act`), and put down a expendable item (`drop`). Restore sensible travel equipment. | Actual wielded/worn/active item, charges/ammo, quantity and location. Inspect weapon fallback independently. Missing/unusable-item variations are legitimate opportunities, not fabricated successful actions. Activation needs an effect, not just an “activation_attempted” status. |
| 3. Leave shelter together | “Stay close”; “Give me a little space” (`follow_close`, `follow_far`). Ask it to wait, move away and return (`wait_here`). Ask it to hold a defensible point (`hold_position`), then release with follow. Ask it to move to a visible nearby spot and wait/hold (`move=dx,dy` terminal forms). Include one actual obstruction or inaccessible spot if available. | Positions/path and distance over native turns; distinction between retained hold and wait, legitimate safety override, clear/released old order. Unreachable terrain need not be magically traversed. |
| 4. Scavenge | “See anything useful?” (`look_around`), then request a real visible item. Use available unowned versus foreign-owned/zone-forbidden items. Return the dropped supply to practical use. | Actual model-selected target, ownership/zone context, pickup inventory delta or attributable refusal. A forbidden candidate sitting nearby is not rejection coverage if never selected. Avoid repetitive rephrasing until the model picks the desired negative case. |
| 5. Combine and revise | Equip + follow; move + hold; calm + equip; later equip + explicit visible target. Vary phrasing and reverse requested order where meaningful. Revisit follow after hold, and an inventory request after a different primary order. Deliberately issue one second order while a first is actually pending. | Full parsed chain and per-action outcome; partial chains stay partial. Record requested, parsed and executed order separately. The executor has fixed inventory categories; don't assume arbitrary utterance ordering is promised. Check whether the second order is visibly refused, queued, applied or silently lost. |
| 6. Danger on the road | Approach a manageable ordinary zombie encounter with a retreat path. Test an explicit visible `attack=target`, changed target/visibility where naturally possible, retreat/regroup, and `panic_on` then `panic_off` when safe enough to observe a real change. Include the pending-inference danger case below. | Exact target identity, attempt/miss/hit, ammo and wounds; survival overrides, response delay and actual movement. No guaranteed hit/obedience. Danger-induced panic returning differs from a failed calm command. Do not intentionally sacrifice the companion just to tick a box. |
| 7. Return and continuity | Regroup, retrieve useful supplies, return to shelter. Reissue earlier requests in the post-danger context, discuss what happened, and save/reload at a useful quiet checkpoint after recording current state. | Same NPC, wounds/items/current order, no duplicated response or stale action replay. Check conversational continuity actually provided by the product; do not assume unlimited persistent memory. Record history reset or loss instead of repairing the prompt. Finish with usable saved recovery state and honest remaining gaps. |

Use a well-stocked, varied inventory and nearby supplies for a connected sequence of tactical drops, pickups, equipment changes and useful refusals. Record setup changes against the comparison fixture. Include ordinary fit, full pockets with empty hands, full pockets with occupied hands, overweight, partial stack fit and a container with contents. Do not include an unreachable-item intake case. Verify the actual native store/wear/wield alternatives rather than assuming a setup label guarantees refusal or automatic partial intake. For each request, separate selected item/quantity, actual destination and conservation from model dialogue, native refusal speech, debug status, silence and false completion. Use supported behavior; these cases do not authorize new fallback rules, direct give or prompt coaching.

This covers the current `allowed_actions` catalogue: wait_here, hold_position, follow_close, follow_far, equip_gun, equip_melee, equip_bow, panic_on, panic_off, look_around, look_inventory, idle; additional attack/move forms and secondary wear/wield/act/drop. Verify against the chosen build once. Do not require unsupported arbitrary chains or an exhaustive cross-product.

## Code-grounded interleavings inside the journey

These are risks to investigate, not predeclared bugs. Capture before/request/pending/change/response/application chronology, including game turns and wall time where relevant.

- **Danger during inference:** `src/llm_intent.cpp` uses a worker thread, while `src/npcmove.cpp::npc::move` explicitly pauses an NPC with response_pending/look_around_request_pending after some danger assessment and before later action selection. During an actual outstanding request, advance normal play so an existing threat becomes relevant. Observe NPC movement, attacks, flee/hazard reaction and damage before/after completion. Contrast ordinary nonpending behavior when the same journey supplies a comparable opportunity; do not claim a controlled comparison if geometry changed. If inference completes too quickly to overlap danger, mark the timing case unexercised. Do not insert artificial latency into the model run to manufacture a finding.
- **An order overtakes another:** pending-primary handling can skip a request for an already pending NPC. Record the first and second utterance, whether each actually gets a request, any visible feedback, and eventual order. Later secondary inventory completion can also matter after a new order. A missing log field alone does not prove a dropped request.
- **Inventory changes in flight:** inventory selection captures item pointers in `look_inventory_context` before asynchronous completion. If ordinary interaction permits, move/consume/transfer the relevant item while that response is pending, or exercise a supported chain affecting it twice. Check the correct current item/owner and stale-selection handling. Do not directly mutate saves or force a dangling pointer. If the timing/opportunity is absent, report the gap; retained responses/source tests may diagnose separately.
- **Attempt versus result:** inventory activation currently records an attempt in its action completion path. Try a usable item and a naturally unavailable/empty/inapplicable counterpart if present. Inspect physical effect and charges; distinguish no-op, legitimate refusal and false success. Similarly, acknowledgment of follow/attack is never execution proof.

Source anchors: [NPC motor](../../../../src/npcmove.cpp), [LLM request/response and inventory execution](../../../../src/llm_intent.cpp), [existing LLM controls](../../../../tests/llm_intent_test.cpp), [runner diagnostics](../../../../tools/llm_runner/DEBUGGING.md). These anchors explain test choices; the player need not reread entire sources before starting.

## Character, reliability and variation

Use natural contextual requests, including polite/direct/urgent phrasing, an ordinary correction and a few conversational exchanges about supplies, danger and the return. Vary eligible case order within safe stages; record the chosen order and a seed if a randomizer is used. Keep command meaning interpretable. No adversarial prompt injection, out-of-world test talk, forced catchphrases, or demands to recite internal tokens.

For character proof, first verify each NPC's actual sent personality summary/tone, example expression, profession, personality/opinion/state and supplied conversation or memory. Distinguish missing fields from a full biography or persistent memory; do not assume either is supplied. Compare representative actual replies during preparation, danger, intake/refusal and return against those inputs. Give short quotes and reasons for fitting character, mechanical/repetitive speech, unexplained personality changes, invented knowledge or false completion. Report how many delivered replies were reviewed per NPC and any unjudgeable cases; character judgments are qualitative, separate from action success and technical failures. Concision alone is not robotic, a coherent refusal may fit the character, and repeating its example catchphrase is not required. Do not invent a persona or rewrite model dialogue.

Report practical first-attempt compliance as successes / eligible understandable requests, with partial chains and sensible refusals shown separately. Also show all attempted requests and excluded/unobservable reasons so the denominator is inspectable. A coherent in-character refusal can be good roleplay while not fulfilling the request. Technical delivery/parse/execution reliability is a separate count, not diluted by the owner's 60–90% companion preference. A small contextual sample cannot estimate a universal success probability; do not tune or intentionally degrade performance to land in that band.

## Performance, interruption and recovery

Keep existing per-run performance collection active where supported. Retain actual telemetry/configuration and missing hooks honestly. At request boundaries capture available inference queue/start/completion/application times; distinguish cold-model startup, queue delay, inference, and waiting for a native turn. Existing configured game-performance alarms still apply during aggressive batches. Record host/model contention if another run shares inference resources; do not call that an isolated benchmark.

On a timeout, inspect the owned runner/request and game state through existing controls/logs before retrying. On runner failure retain stderr/exit identity and pending request state, then use supported recovery. No fixed additional request/response deadline is invented here. A crash, NPC death, lost input authority or unusable save may bound the journey; report what was already tested and what remains. Continued travel must not hide the first divergence. Save/reload after recovery is a declared continuation, not a fresh clean success.

## One report, coverage table and cleanup

Use this arm's existing task/run artifact directory and one report with links into original logs/journal/save. Do not create a directory or copied prompt/log bundle per trial. A compact table contains:

`sequence | scene + relevant prior order | actual utterance | run/runner/request/NPC | response + parsed chain | native consequence | outcome | character note | original evidence handle`

Request IDs such as req_0 are not globally unique: keep run and runner identity. Summarize command-route coverage (observed, failed, unexercised), first-attempt reliability/counts, chain/order sensitivity, character examples, latency/performance findings, technical defects versus model behaviour, and unavailable prerequisites. Include exact successful batch controls with live bindings parameterized. Preserve original negative outcomes and follow-up attempts independently.

At closeout save when possible, retain byte-backed continuation and necessary original evidence, finish through existing witness/finish controls, and verify owned game/helper exit with process identity. Leave shared model service and other workers' games alone. Remove disposable scratch/copies once accepted evidence and recovery bytes are retained under existing cleanup rules. Do not call the arm complete merely because every command was spoken: each coverage row needs an observed disposition or explicit remaining gap. No claim that this journey excludes every bug.
