# Andi handoff: Bandit live-wiring audit + visible-light horde bridge correction v0

## Classification and position

- classification: ACTIVE / GREENLIT NOW; implementation checkpoint, live proof open
- authoritative canon: `Plan.md`
- active queue: `TODO.md`
- success ledger: `SUCCESS.md`
- validation policy: `TESTING.md`
- packet doc: `doc/bandit-live-wiring-audit-and-light-horde-bridge-correction-v0-2026-04-26.md`
- previous packet caveat: `doc/bandit-live-signal-site-bootstrap-correction-v0-2026-04-26.md`
- repo: `/Users/josefhorvath/Schanigarten/Cataclysm-AOL` on `dev`

## Item title

Bandit live-wiring audit + visible-light horde bridge correction v0

## Why this is active

Schani/Josef stopped the smoke/fire site-refresh playtest loop for `Bandit live signal + site bootstrap correction v0` after it crossed the review threshold. That packet remains a partial implementation checkpoint with raw-field reader proof and caveats, not a completed player-fire product proof.

The next greenlit stack item is now active: fix or honestly defer the missing live visible-fire/light-to-horde bridge so deterministic bandit proof packets stop implying live horde behavior they do not yet have.

## Previous packet stop condition to preserve

Do not run more blind smoke/fire site-refresh probes for `bandit-live-signal-smoke-source-site-refresh-proof` unless Josef explicitly reopens it.

Preserve the exact reviewed blocker: `.userdata/dev-harness/harness_runs/20260427_013136/` produced `signal_packet=yes` but `matched_sites=0 refreshed_sites=0 rejected_by_signal_range=1`, then decayed to no-signal. Raw saved fields remain map-field reader proof only; synthetic smoke would be smoke-source/live-signal proof only; player-fire proof still requires the real brazier/wood/lighter/player-action chain.

Also preserve the process caveat: this smoke/fire loop crossed the Frau Knackal attempt-3 consultation rule unclearly before landing in Josef review.

## Current implementation checkpoint

- Source bridge implemented: qualifying live light observations now carry `horde_signal_power` and call `overmap_buffer.signal_hordes(...)` from `signal_live_hordes_from_light_observations(...)`.
- Threshold helper added: `bandit_mark_generation::horde_signal_power_from_light_projection(...)` rejects daylight, screened ordinary light, non-viable projections, and weak range projections; exposed night/searchlight-like cases get clamped power `8..60`.
- Deterministic coverage added: `bandit_mark_generation_visible_light_horde_bridge_stays_bounded`.
- Reviewer-readable live-path log added: `bandit_live_world horde light signal:` records packet id, kind, source OMT, signal power, range cap, and weather when the bridge actually signals.
- Live gameplay closure is still open: no accepted live/harness proof yet shows a real visible light/fire source producing that horde-light log and reaching the horde path.

## Non-goals

- no Lacapult work
- no release publication, signing, notarization, or repo-role surgery
- no mutation of Josef's real saves/userdata
- no broad horde rewrite unless the bridge truly requires it
- no reopening the smoke/fire site-refresh proof loop by drift

## Success state

- [ ] Docs/canon clearly distinguish deterministic proof/playback behavior from live game behavior for bandit light, smoke, horde-pressure, and handoff claims.
- [ ] The live visible-light-to-horde bridge is implemented in source and deterministically tested, but still needs live/harness proof before gameplay closure.
- [x] If implemented, the bridge calls the real horde signal path through bounded thresholds and reviewer-readable reports.
- [ ] At least one deterministic test proves bridge thresholds and one live/harness proof shows a real light/fire source can affect a real horde signal path. _(Partial: deterministic threshold test passes; live/harness proof open.)_
- [ ] Existing bandit test claims are audited enough that no closed packet says “game does X” when only an authored proof packet does X.

## Testing / evidence so far

- `git diff --check` passed after the bridge source/test/doc edits.
- `make -j4 obj/bandit_mark_generation.o obj/do_turn.o tests LINTJSON=0 ASTYLE=0` rebuilt touched objects and `tests/cata_test`; log `.userdata/andi-horde-bridge-build-tests.log`.
- `./tests/cata_test "bandit_mark_generation_visible_light_horde_bridge_stays_bounded"` passed: 7 assertions in 1 test case; log `.userdata/andi-horde-bridge-test.log`.
- `./tests/cata_test "*bandit_mark_generation*"` passed: 214 assertions in 16 test cases; log `.userdata/andi-horde-bridge-bandit-mark-generation.log`.
- `make -j4 cataclysm LINTJSON=0 ASTYLE=0` linked the game binary; log `.userdata/andi-horde-bridge-cata-build.log`.

Gameplay claim still requires a live/harness proof packet with source, command/scenario, artifact path, and the real horde signal path evidence:

```text
visible fire/light -> overmap_buffer.signal_hordes(...) -> overmap::signal_hordes(...) -> horde_map::signal_entities(...)
```

Do not use the parked smoke/fire site-refresh loop as the proof by inertia. The next proof should be deliberately shaped as a light/horde-bridge probe, not another blind smoke/fire rerun. Na bravo, we are not feeding the same slot machine again.
