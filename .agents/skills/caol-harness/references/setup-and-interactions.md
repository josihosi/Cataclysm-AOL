# Setup, speech and item interactions

Scenario setup removes irrelevant friction; it does not prove gameplay. Use the mutation policy
below for suite runs. Record applied changes and keep their evidence scope explicit. For item
activation/placement, use the current native prompt and [setup interaction](setup-interaction.md).

## Reach an evac shelter roof

Evac shelters and the playtest saves that start inside them have a ladder or stairs to the roof. If the route finder cannot reach the roof, inspect the existing access and try the door from an adjacent tile. The door can be locked.

1. Find a **Halligan bar**. When debug setup is allowed, spawn **Halligan bar** (`halligan`) from the debug item menu on the disposable playtest save.
2. Pick it up if needed and **wield** it. Open the item's **Activate** or **Use** action, choose **Pry crate, window, door or nails**, and target the adjacent locked door when prompted.
3. Confirm the native result, walk through the opening, then climb the ladder or stairs. Check that the player reaches the roof level. If prying fails and breaking the door is acceptable for the scenario, use **Smash** toward the door with the bar wielded and verify the opening.
4. Record debug spawning and any door damage as player setup when the test concerns an NPC's later entry.

## Mutation setup for suite runs

Josef authorizes practical avatar mutation setup liberally, including invisibility and avatar
teleportation. Choose shelter, fuel, routes and waiting positions that reach the assigned test
condition. Debug Life Support (`DEBUG_LS`) is useful for long journeys; it removes needs noise,
not incoming damage. Preserve the variable actually under test: a needs-survival check keeps
natural needs, and a natural-combat check retains the relevant damage and survival conditions.
Leave tested NPC mechanics intact except for the explicitly selected controlled intervention, such as the panic or casualty variant. Record meaningful setup changes and their evidence scope.

### Incidental danger

When an unrelated creature interrupts the assigned playtest, the original Luna removes that
exact creature using the supported `world.debug_kill_creature` control and continues the same
run. Resolve the current target from the live scene; incidental danger does not warrant closing
and restarting the game. Record the removal as setup with no combat or victory credit.
Preserve tested actors, required encounters and actual prior injuries, deaths and other history.
An encounter that is itself under test remains part of that test, subject to its selected
controlled interventions; this setup rule does not manufacture its outcome.

### Get and verify Debug Life Support

1. Inspect the current avatar mutation state once. If `DEBUG_LS` is already active, keep it active.
2. If missing, open the currently advertised `world.debug_menu`, choose **Player → Mutate**
   through its returned menu entries, search/select **Debug Life Support**, and enable that
   individual trait. Resolve current action/target IDs rather than copying old menu numbers.
   Prefer individual traits so their effects are known. The debug mutation bundle includes
   debug light, which would confound a light or ordinary-visibility measurement.
3. Return to World, verify `DEBUG_LS` active in native mutation state, and pass one advertised
   `world.pause` if needed for setup to apply. Record its turn and current needs/HP. Do not use
   Tab for this setup because it may attack. Retain setup in the next useful checkpoint; no
   extra save/reload ceremony is required after every check.

Life support suppresses needs progression; it is not a promise to erase inherited dehydration,
fatigue, wounds or effects. Inspect a bad starting condition and address it as declared setup or
ordinary maintenance, not a raid failure. Do not normalize the body mid-combat to erase evidence.
Typed thirst chatter is still handled by the explicit safe-wait macro; direct `act wait.5m` does
not run that handler. For byte-exact bug reproduction, preserve inherited traits in the original;
if this setup changes them, label the disposable run as a setup variant rather than byte-exact.

### Stamina setup for future non-stamina playtests

Owner-selected setup: enable avatar **Debug Stamina (`DEBUG_STAMINA`)** individually for
future non-stamina journeys. Use current `world.debug_menu` → Player → Mutate, resolve the
advertised Debug Stamina entry, enable only if missing, then verify native mutation state.
Select this trait individually and leave owner saves unchanged. `Character::mod_stamina` returns
without changing stamina in either direction for this trait, including ordinary recovery;
it also excludes the tired-activity query. The JSON regeneration modifier is +9, but
recovery still calls this skipped mod_stamina path. It does not refill existing low stamina. It does not suppress hunger, thirst, sleep, damage or death. Apply to
the avatar only; do not normalize existing exhaustion or grant combat immunity.

Stamina/prompt regression checks keep natural stamina and omit this trait. The real prompt is
“You struggle to continue.  Keep trying?” with **Continue after a break.**, **Maybe later.**,
and **Finish it.** Use its current advertised choice, then collect the original successor;
never reuse an Overmap close or assume Finish it replenishes stamina. Break starts a stamina
rest activity with auto-resume; Maybe later cancels the current activity. Targeted854 native
travel nevertheless re-presented the tired menu after Maybe later, so verify the actual
route/successor rather than assuming travel stopped. Finish it suppresses this activity's
query while continuing. The harness activity-session correction serves ordinary blocking UI
in both pre-input and post-action drivers; it is not a stamina-specific adapter.

### Vision and other mutations by arm

Use the authorized avatar setup to observe the NPC mechanism without unrelated travel or
survival friction. This is not a global mutation allowlist. Preserve each measurement:

- Actual reciprocal scout exposure still requires native ordinary sight; debug vision is not
  exposure proof. If invisibility prevents the required contact, disable and verify it for that
  segment, then restore useful observation setup when appropriate.
- Light-only and smoke comparisons retain their actual emitter and ambient-light conditions;
  debug light would change those inputs. Viewer traits establish viewer coverage, not ordinary
  detection. Keep the named clairvoyance variant distinct.
- Avatar teleportation can establish departure, return or loading geometry. Verify actual map
  bounds, actor ownership and elapsed time; it does not prove walked travel or NPC movement.
  For the debug **specific overmap** prompt, each horizontal component is
  `major-overmap-region'minor-OMT`, not OMT followed by a local map tile. The absolute OMT
  coordinate is `180 * major + minor`; z is the third integer directly. As a syntax
  example, absolute OMT `(140, 138, 0)` is `0'140,0'138,0`; this is not a safe-landmark
  claim. A bare component supplies the major region with minor zero, so `140,138,0` is
  not that destination. Check the current prompt and verify the resulting coordinates.
  Before choosing an elevated landing, establish its terrain and support in the current
  world; a remembered coordinate or a correct parser value does not establish safe footing.
- Do not bypass the tested NPC behavior by editing its mission or moving it to the expected endpoint. Preserve explicitly selected controlled variants: the [panic branch](rural-roof-replay.md#panic-and-withdrawal-regression) induces flight, the night-alarm arm permits its stated threat setup, and the [X4 procedure](playtest-suite.md#x4-party-survivor-and-casualty-handover)
  separately permits one identified bandit debug kill; [X4b](playtest-suite.md#x4b-rider-ambush-during-bandit-return)
  permits one hostile zombie rider spawn. These controlled interventions do not authorize other
  actor creation, casualties among tested actors or in required encounters, mission edits or
  forced outcomes. Unrelated interruptions follow [incidental danger](#incidental-danger).

### Typed avatar OMT teleport

From the current native World owner, preview an absolute overmap-tile destination before moving:

```sh
python3 tools/openclaw_harness/play_cli.py --session SESSION act world.debug_omt_preview --omt X Y Z
python3 tools/openclaw_harness/play_cli.py --session SESSION act world.debug_omt_teleport --omt X Y Z --preview-request-id PREVIEW_REQUEST_ID
```

Use the `preview_request_id` from the native preview receipt. X and Y are signed absolute OMTs;
Z is the absolute level. The preview identifies the source and native landing map square and
reports existing tile terrain, furniture and floor when available. Unknown is not safe footing;
floor alone does not prove support from below, absence of hazards or NPC safety. The destination,
source position, game turn and available tile facts are checked again before movement; a stale
preview requires a fresh one. The ordinary native placement may create missing terrain only on
the later teleport action. Check the resulting World position and actual situation. This is an
avatar-only setup intervention, with no walked-travel or NPC-behavior credit.

For comparisons, keep setup fixed or label the mutation difference as the tested variable. Keep
existing viewer evidence; do not silently remove traits from its saved continuation and call it
the same setup. Mutation verification is a start/change check, not a per-wait ritual.

Choose waiting behavior from the test goal; see [Wait and observe](../GAME-MANUAL.md#wait-and-observe)
for the existing `safe`, `stop` and `ignore` modes and their actual behavior. Damage or death during
an intended combat test can be the result being tested, not a defect. Use `safe` when you need to
inspect the first harm and `ignore` when deliberately observing combat through its outcome.
No extra approval is required merely because fictional danger is expected. An induced casualty
does not prove natural combat lethality; subsequent native survivor, ownership and persistence
behavior can establish the assigned controlled X4 outcome.

## Speech and NPC replies

Scripted Dialogue choices and free-text speech are different native routes. For speech, submit via
the current native prompt, correlate utterance/hearer and prompt request ID with runner
`llm_request_started` and `llm_response_emitted`, then pass a native World `world.pause` turn and
inspect the applied reply/action and game-time change. Calculation completion and native
application are separate evidence. A fixed sleep or `look` proves neither completion nor a turn.
`controls` exposes this sequence beside the wait macro and exact log paths. `req_N` resets between
game processes: correlate prompt/time and runner process, and exclude `prewarm`. Missing producer
evidence means unobservable completion, not failure. The current free-text route dispatches the
first hearer immediately; later serial hearers can need a turn to apply a prior response and launch
the next. Inspect that dependency when no matching request starts. Resolve any intervening input
owner before choosing a turn; further behavior may require further simulation. Preserve an
inconclusive attempt and name the changed step that makes a rerun useful.

## Item ownership and zones

Trade panes can include nearby ground, vehicle, camp or companion items as well as carried items.
An item listed on one party's side does not prove that actor is carrying it. Follow placement
prompts through their actual outcome and inspect inventory or pickup/location facts before claiming
possession. Item UIDs can change after transfer or process reload; compare item type, count, location
and actor identity. Disabled zones may be absent from visible World zones; inspect the zone manager
to distinguish a disabled zone from a missing one.
