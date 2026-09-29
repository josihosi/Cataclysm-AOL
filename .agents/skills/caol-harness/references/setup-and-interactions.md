# Setup, speech and item interactions

Scenario setup removes irrelevant friction; it does not prove gameplay. Use the mutation policy
below for suite runs. Record applied changes and keep their evidence scope explicit. For item
activation/placement, use the current native prompt and [setup interaction](setup-interaction.md).

## Mutation setup for suite runs

**Default for long faction, roof and away/return journeys: avatar Debug Life Support (`DEBUG_LS`)
on; ordinary vision; no invisibility or combat boosts.** Apply it to the player only, never to the
NPCs whose sleep, stamina, injuries and combat are being tested. It removes needs-maintenance noise,
not the raid's ability to injure or kill. A dedicated food/water/sleep-survival arm instead leaves it
off and plans ordinary supplies. Short CLI comparisons retain identical declared setup in both arms.

### Get and verify Debug Life Support

1. Inspect the current avatar mutation state once. If `DEBUG_LS` is already active, keep it active.
2. If missing, open the currently advertised `world.debug_menu`, choose **Player → Mutate**
   through its returned menu entries, search/select **Debug Life Support**, and enable that
   individual trait. Resolve current action/target IDs rather than copying old menu numbers.
   Do **not** use Game → Toggle debug mutations: that bundle includes unrelated traits such as
   debug light and can change the very signal or visibility being tested.
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

### Vision and other mutations by arm

- Ordinary smoke, roof, rural, bandit payment/Fight, patrol, light-only and away/return: `DEBUG_LS`
  as above; no added clairvoyance, night/map vision, cloak, invisibility or debug light. Use selected
  actor logs to inspect off-screen behavior. Keep normal combat capability unless the assigned
  fixture explicitly records another setup.
- The named clairvoyance raid variant: `DEBUG_LS` plus `DEBUG_CLAIRVOYANCE` and
  `DEBUG_CLAIRVOYANCE_PLUS`, enabling only missing traits. Verify after returning to World.
  No cloak. This is viewer coverage, not ordinary-view discovery/stalking proof.
- Exposed stalking, unseen escape and pursuit knowledge (6h–6j): ordinary vision is the main
  proof. A separate viewer copy may diagnose position/path, but do not use hidden information to
  steer the ordinary run or count debug sight as reciprocal exposure. Current do_turn.cpp burn
  checks use `sees_without_clairvoyance` for both participants; that protects this specific gate,
  not every visibility-dependent system or the worker's choices.
- Cloak/invisibility, debug light, invulnerability/healing, stamina/speed boosts, temperature
  immunity and mind control are not defaults. Each changes a relevant detection, light, survival,
  movement or combat variable. Use only in an explicitly identified diagnostic/setup branch,
  with the ordinary arm retained. Never grant the blanket debug mutation bundle for convenience.

For comparisons, keep setup fixed or label the mutation difference as the tested variable. Keep
existing viewer evidence; do not silently remove traits from its saved continuation and call it
the same setup. Mutation verification is a start/change check, not a per-wait ritual.

Choose waiting behavior from the test goal; see [Wait and observe](../GAME-MANUAL.md#wait-and-observe)
for the existing `safe`, `stop` and `ignore` modes and their actual behavior. Damage or death during
an intended combat test can be the result being tested, not a defect. Use `safe` when you need to
inspect the first harm and `ignore` when deliberately observing combat through its outcome.
No cloak or extra approval is required merely because fictional danger is expected. Exact-identity
creature zapping remains a zero-credit diagnostic/setup intervention; it cannot prove natural
route, ecology, combat, lifecycle, qualification or certification behavior.

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
