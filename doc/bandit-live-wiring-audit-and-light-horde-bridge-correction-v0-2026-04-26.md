# Bandit live-wiring audit + light/horde bridge correction v0 (2026-04-26)

Status: parked / packaged correction.

## Normalized contract

**Title:** Bandit live-wiring audit + visible-light horde bridge correction v0

**Request kind:** Josef correction / parked implementation package

**Summary:** Josef caught a real gap: the bandit proof packets implemented light and shared horde-pressure behavior on deterministic proof/playback seams, but the live game does not currently wire visible fire/light into the zombie horde attraction path. This package preserves that distinction, adds a first audit of deterministic-only bandit tests, and defines the bounded correction needed before anyone can claim “roof/basecamp light attracts overmap hordes” as live game behavior.

## Scope

1. Add an explicit live bridge from meaningful visible fire/light exposure to the real overmap horde signaling system, if product direction still wants that behavior.
2. Reuse existing physical footing where possible: local light truth, directional/exposure buckets, weather/concealment reduction, and the existing `overmap_buffer.signal_hordes(...)` / `horde_map::signal_entities(...)` horde interest path.
3. Keep the bridge bounded: meaningful exposed night light / large elevated fire / searchlight-like cases, not every candle in every basement.
4. Add deterministic tests for the bridge math and thresholds, plus at least one live or harness proof showing a real light/fire source in a real game path can create the intended horde signal.
5. Audit bandit proof/test claims and downgrade or relabel any deterministic-only proof that reads like live gameplay implementation.
6. Repair docs/canon where wording currently implies more live wiring than exists.

## Non-goals

- No full zombie tactical simulation.
- No generic global light simulator.
- No claim that current proof/playback scenarios already move real hordes by sight.
- No broad rewrite of `horde_map`, `overmap`, or live bandit dispatch unless the bridge truly requires it.
- No reopening the whole bandit overmap concept chain; this is a correction packet for evidence honesty and one missing bridge.

## First sweep: deterministic-only surfaces that need honest labels

This sweep is not a full static proof of the whole codebase. It is the first targeted bandit-family audit prompted by the light/horde gap.

### `tests/bandit_mark_generation_test.cpp`

Status: useful deterministic adapter tests, but not live game proof.

These tests prove synthetic packet adapters such as smoke, light, weather, human-route, moving-memory, and repeated-site signal generation. They do **not** prove that live map light/smoke/fire automatically produces those packets in a running game. Claims based on these tests must say “adapter/proof seam” unless paired with live packet extraction or harness proof.

Highest-risk examples:
- `bandit_mark_generation_light_adapter_keeps_night_leaks_long_range_but_bounded`
- `bandit_mark_generation_portal_storm_light_keeps_bright_exposed_leaks_legible_but_bounded`
- `bandit_mark_generation_elevated_exposed_light_outruns_hidden_ground_light_without_global_sight`
- `bandit_mark_generation_vertical_smoke_stays_nearby_visible_without_gaining_extra_range`

### `tests/bandit_playback_test.cpp`

Status: useful deterministic scenario/playback proofs, but not live game proof.

These tests prove evaluator behavior across authored/generated scenario frames. They do **not** by themselves prove the running game generates those frames from real light, smoke, fire, hordes, or player/NPC activity.

Highest-risk examples:
- `bandit_playback_generated_directional_light_corridor_shares_horde_pressure`
- `bandit_playback_long_range_directional_light_proof_packet_stays_honest`
- `bandit_playback_elevated_light_and_z_level_visibility_packet_covers_the_active_contract`
- `bandit_playback_overmap_benchmark_suite_packet_covers_the_active_contract`
- `bandit_playback_overmap_local_handoff_interaction_packet_keeps_posture_rewrite_and_carryback_readable`

The horde-pressure cases currently mean “the abstract proof packet carries shared horde-pressure fields/notes,” not “real hordes see a fire and path toward it.”

### `tests/bandit_dry_run_test.cpp`

Status: evaluator unit tests.

These are not empty, but they only prove decision scoring/fallback behavior. They should not be cited as live map behavior unless a live-world or harness path feeds real leads into the same evaluator and reaches a visible outcome.

### `tests/bandit_pursuit_handoff_test.cpp`

Status: handoff packet unit tests with partial live consumers.

`bandit_pursuit_handoff` is referenced by `src/bandit_live_world.cpp` and `src/do_turn.cpp`, so it is not vapor. Still, its unit tests only prove packet construction/application rules; live claims need paired evidence through the live owner/dispatch/contact/writeback path.

### `tests/bandit_live_world_test.cpp`

Status: closest to live wiring, because `bandit_live_world` is used by `src/do_turn.cpp`, `src/mapgen.cpp`, `src/savegame.cpp`, and `src/overmapbuffer.cpp`.

Many closed packets already have matching harness/live evidence for dispatch, contact, shakedown, pay/fight, and writeback. Some profile/adopter tests, such as cannibal profile separation, are intentionally deterministic substrate proof unless later paired with live encounter proof. That is acceptable only if docs say so plainly.

## Known live bridge absence

Current live horde attraction still appears sound/JSON-effect driven:

- `src/sounds.cpp` clusters sound and calls `overmap_buffer.signal_hordes(...)` for sufficiently strong signals.
- JSON dialogue/effect path `signal_hordes` also calls `overmap_buffer.signal_hordes(...)`.
- `overmapbuffer::signal_hordes(...)` reaches `overmap::signal_hordes(...)` and then `horde_map::signal_entities(...)`.

No equivalent live path was found for:

```text
visible fire/light -> overmap_buffer.signal_hordes(...) -> horde_map
```

The deterministic light/horde packets therefore must not be described as live roof-fire horde attraction until this bridge exists and is proven.

## Success state

- [ ] Docs/canon clearly distinguish deterministic proof/playback behavior from live game behavior for bandit light, smoke, horde-pressure, and handoff claims.
- [ ] The live visible-light-to-horde bridge is either implemented and proven, or explicitly rejected/deferred with wording that no longer implies it exists.
- [ ] If implemented, the bridge calls the real horde signal path through bounded thresholds and reviewer-readable reports.
- [ ] At least one deterministic test proves bridge thresholds and one live/harness proof shows a real light/fire source can affect a real horde signal path.
- [ ] Existing bandit test claims are audited enough that no closed packet says “game does X” when only an authored proof packet does X.

## Testing expectations

- Deterministic unit tests are required but insufficient for live behavior claims.
- Any success claim about gameplay must name its evidence class: unit/evaluator, playback/proof packet, live source hook, harness/startup, screen, save inspection, or artifact/log.
- A packet may close as deterministic-only only if its contract says it is deterministic-only. Otherwise, live wiring or live proof is part of the bar.
