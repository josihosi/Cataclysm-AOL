# Bandit journeys — verified payment and remaining branches

Extend the existing [suite](playtest-suite.md); keep assignments and verdicts in the work ledger. The clean R051 smoke-led payment journey is verified at the bound build below. Other branches retain their own unproved outcomes; the R051/R052 site and travel queries below have been exercised.

## Reuse the common procedure

Use the [smoke recipe](smoke-raid-replay.md#start) for bound start/current-state checks, ordinary ignition, explicit `safe` waits, saving without movement, and actor/evidence retrieval. Reuse its controls and query shapes, **not cannibal night gates, actor IDs, operation IDs or assumed timing**. Resolve menus and IDs live. Skip completed setup; read returned observations instead of adding routine look/save calls.

Before long waits or travel, use the shared [mutation setup](setup-and-interactions.md#mutation-setup-for-suite-runs): avatar `DEBUG_LS` on, ordinary vision and normal combat; no blanket debug bundle.

Select a compatible registered bandit start. The existing suite identifies prepared seeds and their setup limits; do not duplicate that catalogue here. Preserve an independent pre-signal start when testing discovery. A pre-choice copy can share the already-proved journey for payment/fighting, but cannot independently prove fresh discovery.

Coordinator gives the playing Luna one current checkpoint plus delta: selected build/save/run,
sole input owner, pending control, completed proof, intended branch and first open boundary.
Include the relevant detailed verified action/query segment and its compatible start; do not
replace the roof/fire/travel/payment procedure with a phase label. Parent and optional retrieval
helper use those same handles and return facts to the sole player; helpers do not acquire a second
input owner. Enable the existing harness decision trace for the relevant bandits and camp defenders. Save at useful branch/recovery points; collect related proof together at meaningful changes, not at a mandatory list of pitstops.

Use the shared [decision points](smoke-raid-replay.md#decision-points-for-efficient-repeats) and [validated counted-action limits](smoke-raid-replay.md#counted-actions-after-the-r043-pilot); the bandit-specific inspection boundary is the actual shakedown choice and resulting transfer or fight. Do not borrow cannibal night timing.

## Ready-report resident payment → home stash

**Before the first repaired run:** load the selected byte-preserved8820 checkpoint with valid report8700 already ready; record local gold setup/drop, leave two OMTs by ordinary travel, let the actual resident receive the natural demand, Pay explicitly, and verify payer debit plus matching persistent items at bandit home1291490. No ignition/scouting replay is needed for this compatible start. Overpayment is accepted. The [current source handoff](../../../../build_logs/first-smoke-067/payment-home-stash/result.md) selects the immutable build/save/scenario; root supplies the current start delta.

The following ordered segments borrow successful R13 controls. Current menu targets, item UIDs, letters and prompts come from the returned frame. The new Trade row/batch and stash-query controls are source-proven; repaired native destination adoption remains pending. Stop a segment on danger, rejection, unknown outcome, prompt or changed owner. Collect pending requests; a completed ACTIVITY WAIT receipt needs `play look`, not repeated collect. Long useful interruptible waits replace quiet short polls. These are supported command segments, not an arbitrary batch-file API.

| Segment | Concrete controls and endpoint |
| --- | --- |
| Local setup before contact | `play act world.debug_menu`; choose current **Spawning** then **Spawn an item** with `play act menu.choose --target CURRENT_TARGET`; `play act menu.filter --param 'text=American Buffalo coin'`; choose current result; `play act prompt.submit --param text=1000`; `play act menu.cancel`. Recorded setup, not payment proof. |
| Reach funding tile and Drop | From3155,3447,1: `play act world.move.southwest` four times, south twice, southwest, west, then `play act world.level_down` at3149,3454,1; ground east twice/north seven reaches3151,3447,0. Do not take the blocked fifth southwest. `play act world.drop`; coin filter as above; `play act inventory.toggle --target CURRENT_UID` for each advertised group; `play inspect surface.facts --select selected_items`; `play act inventory.commit`. Allow ACT_DROP to complete, then `play look` and verify actual local ground/cargo ownership/accessibility. |
| Leave two OMTs | `play act world.overmap`; `play act overmap.move_cursor --target east` twice; `play act overmap.choose_destination --target 'omt:(133,143,0)'`; actual travel confirmation `play yes`. Collect pending travel, look after completed ACTIVITY WAIT; verify the actual OMT endpoint. |
| Natural demand | `play wait 3h safe` or the currently advertised longer useful duration, stopping at contact/danger. Read speaker/receiver; never batch through unresolved Pay/Fight. |
| Explicit payment | `play act shakedown.pay`; `play act trade.switch_pane` only if current pane is wrong; coin filter; `play inspect --view trade --limit 8`. Select current letters with `play trade LETTERS`, or null-letter rows with `play act inventory.toggle --target CURRENT_UID`. `play inspect surface.facts --select payment_destination,active_actor_id,trader_actor_id,demand_amount,balance,player_offer_value,selected_items`; explicit `play act inventory.commit` then current `play yes`. No replay after unknown receipt; F1 is only highlighted-group Auto Balance. |
| Destination and closeout | `play logs` gives the current profile debug source; run its existing `log-query` argv with `--contains shakedown_stash` to retain the deposited count/home/first tile. Save at the proof boundary; run `python3 build_logs/first-smoke-067/payment-home-stash/stash_items_query.py --world CURRENT_SAVED_WORLD --home 129 149 0 --type coin_gold`. Match actual saved stash count/owner/location with payer removal and no collector copy. At World: `play act world.save_quit`, current YES; `play act main_menu.quit`, current YES, collect pending closeout, then the supported journal/finish witness. Confirm owned OS exit separately. |

The stash scans safe storage tiles **within the bound home anchor OMT**, nearest its centre first; exact first tile comes from the receipt, not an assumed centre. A missing saved home quad is unavailable, not zero items. The saved query retains archive/dictionary hashes and original JSON pointers; its temporary decoder copy never edits the live or source save. Generic Trade still transfers to its trader. Old coin custody/home proofs below retain their original destination behavior.

R13 successful command source: [playtest.txt](../../../../build_logs/first-smoke-067/camp-npc-adjacent-omt-r051-20261004/revision-13/bridge-sessions/selected-c47972e0927741a8809d619dc89b4d11/playtest.txt), spawn665–766, Drop6726/7800–7818, departure14277–14354, Pay21811/filter25602/toggle29681/commit36826/YES36837. Update this segment after the useful repaired run with tested stash queries and actual branch proof; planned or borrowed controls are not that proof.

## Verified roof-smoke and gold-payment segment

R051 run `1fe37273`, binding `dbbb6715`, executable `40bfb2be` / source `ec6008b5`,
proved natural smoke-led scouting, an exact-three-OMT lookout, same-ID return/report,
a fresh demand, payment once and both survivors at home. Use the
[original payment record](../../../../build_logs/first-smoke-067/wait-trade-overlay/r051-resume-gold-20261004/revision-2/payment-selection-boundary-8241.md)
for exact native rows, controls, saved turn and source handles. This is one successful branch,
not a full suite pass or proof of every activity interruption.

1. Use a compatible clean prepared bandit save. Add gold coins **before contact**, preferably
   downstairs before ignition, using the current authorized setup controls. Verify the carried
   coins; this run carried 300. That number is recorded setup, not a fixed sufficient amount
   for every demand. Keep ordinary vision, Safe Off and active `DEBUG_LS`.
2. Follow [Josef's proven ladder, roof ignition and maintenance procedure](../GAME-MANUAL.md#josefs-two-brazier-base-save).
   Stay safely near the fire during useful waits; verify actual fuel/refuelling and smoke.
   Descending can prevent nearby automatic refuelling. Follow the actual same actor IDs
   through discovery, watch, return/report and demand; do not assume cannibal timing.
3. In the current native Trade, highlight the gold-coin group. The observed **F1 Auto Balance**
   selected 167 Buffalo coins against a $761.52 demand, offering $764.86 with $3.34 excess.
   Inspect the actual offer/balance. Commit once and accept the current confirmation once.
   Nested item contents **Close** is not basket commit. Refresh after an accepted receipt
   with a missing successor; collect pending input and never replay an already accepted action.
4. Follow the same surviving bandits home and save at a meaningful completed boundary.
   Compare full item trees, including worn-container pockets, from the **final native save**;
   a startup snapshot or top-level `inv=[]` is not a custody check. At turn5248280 the player
   retained133 coins and Mason (#4) carried167 in worn pockets; Mason and Billye (#5) were alive
   at home OMT `[129,149,0]`. Site value76152 is cents ($761.52), distinct from carried items.
   Existing item copies regenerate UIDs: this proves counted physical conservation, not an
   individual old-to-new UID mapping. [Exact final-save extraction](../../../../build_logs/first-smoke-067/wait-trade-overlay/custody-diagnosis/custody.json).

Keep the insufficient-payment/Fight branch separate. The owner accepted the observed
[underfunded escalation and defender flight](../../../../build_logs/first-smoke-067/insufficient-payment-protocol-candidate.md);
Ernest attacked the player and Tilda/Casey fled. That branch does not prove player death or
reciprocal defender attacks. Preserve completed segments when continuing either branch.

## Branches to work through

| Branch | Actions and intended outcome | Native evidence to collect |
| --- | --- | --- |
| **Smoke → peaceful robbery** | Ignite if needed; follow discovery/scout/report to shakedown. Preserve pre-choice state, then use the actual peaceful payment control. Let bandits take payment and leave. Continue to an eligible later visit, retaining base knowledge. | Actual offered/demanded pool, item/value transfer and any debt before/after; same-ID departure and later return; defenders do not attack peaceful collectors. Use displayed transaction values, not an assumed percentage of all base items. |
| **Debt settlement** | At a genuine debt/payment state, use the supported settlement interaction. Record gold spawning as setup if needed. This can extend the peaceful branch when that branch actually leaves debt. | Debt creation, payment and remaining balance; no duplicate charge or invented settlement. No debt in the preceding branch is not debt-system proof. |
| **Refuse → armed fight** | Use an independent pre-choice copy and the live refusal/Fight control. Inspect first harm; deliberately continue combat with the recipe's explicit `ignore` wait when needed to reach the requested outcome. | Each capable attacker and both defenders: target/action and physical progress; actual shots, damage and attributed player/NPC casualties. A label, first hit or checkpoint does not finish the requested fight. Explain a concrete stall rather than waiting unchanged. |
| **Admission → extinguish → routine outings** | After natural roster admission, extinguish through live controls and verify signal-off. Follow natural outings to different real POIs, return/report and any actual collection. | Same-ID physical departure/watch or visit/return; observations, cargo and memory. An empty watch is a valid branch, not haul proof. Known base location may persist; a moved player's exact location needs a sight/report basis. |
| **Away → loot → return** | Branch a compatible start; leave through ordinary doors and westward travel, spend time away during the selected faction phase, collect real loot, then return. Inspect both braziers before relighting. | Faction progress while away, actor/roster continuity, fire state and knowledge of base versus current player position. Record visibility-changing traits separately. |

### Separated pair: actual return from the retained scattered save

This separate owner-authorized arm used the exact retained R051 save at turn5274553/min8709,
copied through the selected saved-world continuation to a disposable profile on immutable
812730. The original 61-file profile still matches
[`save-manifest-before.json`](../../../../build_logs/first-smoke-060/save-manifest-before.json)
byte-for-byte. The saved start is bug-created separation; it does not establish threat-induced
flight. Reobserve actual IDs and owner state on load rather than treating this actor layout as a
general start.

The successful ordinary wait sequence in run
`c3a028ca42f7ca015bce96a89446334e4583a6904d6e233a3bd1965424176929` was:

```sh
python3 tools/openclaw_harness/play_cli.py --session "$CAOL_PLAY_SESSION" wait 30m safe
# Only when the current native alarm-clock prompt appears:
python3 tools/openclaw_harness/play_cli.py --session "$CAOL_PLAY_SESSION" act world.wait
python3 tools/openclaw_harness/play_cli.py --session "$CAOL_PLAY_SESSION" act menu.choose --target wait-mode:wait-a-while
python3 tools/openclaw_harness/play_cli.py --session "$CAOL_PLAY_SESSION" wait 1h safe
python3 tools/openclaw_harness/play_cli.py --session "$CAOL_PLAY_SESSION" wait 30m safe
python3 tools/openclaw_harness/play_cli.py --session "$CAOL_PLAY_SESSION" act world.quicksave
```

Fresh native frames after the accepted waits showed ID4 already home at OMT `[129,149,0]`;
ID5 moved from `[108,142,0]` to `[114,140,0]` with a homeward goal/path16, then `[126,146,0]`
with path4. The final quicksaved `overmaps/o.0.0.zzip` placed both actual bodies at home OMT
`[129,149,0]`, alive: ID4 map-square `[3117,3581,0]`, ID5 `[3096,3594,0]`. The matching
`dimension_data.gsav` site row recorded both IDs `at_home`, `applied_return_generation=1`,
`physical structural return receipt committed`, and no active outing. The live site row had
disappeared at the final World observation; that alone was not used as arrival proof. Exact
run, native-frame, save and cleanup handles are in
[`native-return.md`](../../../../build_logs/first-smoke-067/scattered-party-return-812730/native-return.md).
Only ID5 moved during this arm; ID4 was already physically home. Neither actor's saved-history
separation is evidence of a natural or threat-caused flee.

For the owner-expanded day-away, exposed-watch and reality-bubble variations, use [away/return journeys 6a–6o](away-return-journeys.md). Shared dual-faction runs retain separate cannibal and bandit outcomes.

Run smoke first. Reuse these branches for combined light/smoke only after observing both channels independently. Light-only remains dependent on the electrical/lamp baseline; a smoking brazier does not substitute for it.

## Capture once, improve from use

Use operation/actor/turn filters in the existing [evidence tools](evidence-and-diagnostics.md), actual saved NPC positions and native transaction/damage records. Retain returned source handles; test selectors against this run before adding them as bandit examples. Missing compact rows are not proof of inactivity. Keep the suite's existing performance checks and debug recovery guidance.

R051/R052 validated this narrow travel query (substitute the bound log/run and current site/member IDs):

```sh
python3 tools/openclaw_harness/cockpit_file_bridge.py log-query --path <transition.events.jsonl> \
  --run-id <run> --where 'site_id="overmap_special:bandit_camp@129,149,0"' \
  --where 'actor_ids=[5]' --where 'transition="scout_travel_prepare"' \
  --select sequence --select actor_ids --select site_id --select reason
```

For the retained R052 continuation, `build_logs/first-smoke-052/saved_outing_query.py`
accepts `--dimension PATH --site-id ID`; its tested commands are in that task's `luna-handoff.md`.
It reads **saved** outing state, not current actor positions. Pair it with the save's actual turn;
a no-op quicksave does not refresh the bytes. Keep distinct sites separate. When actor positions
are needed, name their coordinate units and preserve z (only map-square x/y divide by 24 for OMT).
The current compact site/actor projections retain source labels and full retrieval handles;
missing fields are unknown, not inactivity.

Return the achieved outcome or concrete divergence, tested action/query corrections, evidence handles and save/input ownership. Add successful reusable segments to the existing recipe; leave unobserved outcomes unvalidated. A gameplay failure does not invalidate working ignition, waiting, saving or proof collection. No extra protocol review gate or separate protocol database.


## Continue and batch on the current controls

Keep the detailed verified ignition, travel and payment segments above. Reuse a segment when its starting state matches; do not rediscover unchanged controls or repeat completed setup. A prompt, danger interruption, unknown surface or changed input owner ends that batch. Observe the new state before choosing the next segment. Current advertisements supply targets; historical target IDs are evidence, not reusable controls.

### Keep a useful session through a short diagnosis

Saving preserves recovery; it does not require quitting. The same sole Luna input owner may save and park its owned game while the coordinator or source owner answers a narrow read-only question. Keep its purpose, exact PID/birth identities, pending input and current state in the existing checkpoint plus delta. Resume the same player with the relevant proven segment when the answer permits ordinary progress. A normal movement or decision boundary that has not happened yet does not require another source test, build, launch or unchanged binding audit. Investigate a real evidence conflict.

Close safely when conflicting source/build use or rebind requires it, exclusive quiet review is due, recovery requires closure, or the session has no further useful purpose. Preserve actual save and process-exit evidence. Keep prompts and danger as batch boundaries; do not invent input while the session is parked. No fixed parking timeout or extra monitoring loop is needed.

### Selected saved continuation

Carry one selected scenario/build/checkpoint handoff with its existing saved_world_snapshot and profile fields. The launch adapters reuse those fields; explicit overrides remain possible. Identical installed save bytes need no rewrite. Different populated bytes require a fresh disposable profile or explicit replacement. A startup snapshot describes the initial scene, not the mutable final save. After World, check actual turn, Safe mode, active Life Support, visibility mutations and pending input.

If the declaration opts into post_relaunch, its terminal_save_step_label names the **initial**
step whose saved world will be continued, not a relaunch/reentry step. Initial and reentry labels
can remain distinct. The existing declaration lint now reports this exact field/rule and the
available initial labels before selection; runtime retains the same check. Correct the reference
in the declaration rather than renaming the initial save to a relaunch-only label. This applies
to an actual saved-world process boundary; saving or ordinary progress does not require relaunch.

For the camp-resident receiver arm, use the downstairs brazier [3159,3449,0] from [3158,3449,0] when compatible signal setup is still needed. Check whether the fire is already burning before ignition. First obtain a valid completed scouting assessment; then travel two OMTs away by ordinary movement. A compatible checkpoint with that assessment already completed avoids repeating setup. Continued smoke is not required after the valid assessment; preserve its original evidence and age. Preserve the roof protocol for roof arms; a partial zone projection does not prove upstairs zones are absent. Shared fire/travel steps do not prove resident contact, payment or return.

### Partial pickup

Start in the current Pickup selector with an advertised item/group UID. Existing inventory.increase_quantity and inventory.decrease_quantity adjust one unit. inventory.set_quantity with the current target opens the native count prompt; submit through its current prompt.submit text control. Inspect selected_items and available bounds, then inventory.commit. Canceling the count prompt preserves the basket; canceling Pickup moves nothing. Inventory.select confirms the whole basket in this selector.

After accepted count submission, a preceding parent descriptor may briefly show the old count. Obtain a current observation rather than replay the accepted action. These controls have renderer/native source tests; new live use must still be recorded. Do not use group quantity as proof of individual-item splitting.

### Compact evidence and scout ownership

Use the existing inspect items/actors views with contains, offset and limit, or narrow --select for known fields. Item views include worn pockets and recursive containers. Missing fields are unavailable, not empty. Preserve JSON pointers, response hashes and full retrieval handles. Label source-role final/startup/observation/unknown from actual provenance, not filename guesses.

For evidence clocks, select time_roles with the exact source handle. Report delivery is provenance; service time is available only when explicitly recorded. Filter native surface_descriptor rows for actual observation time. Unknown clocks are not reconstructed from neighboring frames or movement cadence. Keep a selected immutable log prefix separate from the appended full log, and pre-save captured bytes separate from final saved bytes; a descriptive witness is not a raw save. Use [the time projection result](../../../../build_logs/first-smoke-067/evidence-time-projection/result.md) for the existing narrow query and original report/denial example.

For scouts, expand structural_outing_owner.assigned_members. selected_object_source identifies the stored overmap object; game_find_npc_aliases_overmap_buffer is not an independent active lookup. tracker_objects exposes distinct active copies and agreement. body_update_turn is a body/effect clock, not movement age. Filter existing opt-in scout_motor_service and scout_watch_arrival_service rows by actual operation/site/generation and actor IDs, retaining sequence, run, turn, minutes, outcome and reason. No matching row cannot prove no motion.

The repaired motor retains a validated live destination until the pair consumer commits. Old already-cleared goals cannot be reconstructed as completion evidence. On immutable902be, R8 proved same-ID physical watch arrival and R9 proved home8060/report8100.
R11 proved actual8160 assessment refusal for that empty report; it did not prove a ready assessment.
Keep those accepted stages and their [exact retained findings](../../../../build_logs/first-smoke-067/finite-knowledge-boundary/result.md)
when selecting the next branch. A valid completed assessment precedes two-OMT departure;
continued smoke is unnecessary afterward. New encounter receiver/payment/return proof remains
separate from these accepted watch/home/report stages.

Source controls and exact commands: [tooling iteration 3](../../../../build_logs/first-smoke-067/tooling-iteration-3/result.md), [detailed guidance proposal](../../../../build_logs/first-smoke-067/tooling-iteration-3/guidance-merge-proposal.md), [watch-order result](../../../../build_logs/first-smoke-067/scout-watch-order-repair/result.md). Keep observed improvements separate from unmeasured speed savings.
