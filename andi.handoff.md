# Andi handoff: Bandit local sight-avoid + scout return cadence packet v0

## Classification and position

- classification: ACTIVE / GREENLIT NOW
- authoritative canon: `Plan.md`
- active queue: `TODO.md`
- success ledger: `SUCCESS.md`
- validation policy: `TESTING.md`
- packet doc: `doc/bandit-local-sight-avoid-and-scout-return-cadence-packet-v0-2026-04-26.md`
- just-closed prior packet: `doc/bandit-live-wiring-audit-and-light-horde-bridge-correction-v0-2026-04-26.md`
- previous parked caveat: `doc/bandit-live-signal-site-bootstrap-correction-v0-2026-04-26.md`
- repo: `/Users/josefhorvath/Schanigarten/Cataclysm-AOL` on `dev`

## Item title

Bandit local sight-avoid + scout return cadence packet v0

## Why this is active

`Bandit live-wiring audit + visible-light horde bridge correction v0` now has its bounded live/harness bridge proof and moved downstream. The next greenlit stack item is the local-stalking credibility slice Josef identified from nearby-camp playtesting: exposed stalking/hold-off bandits should try to break sight locally, and scouts should have a finite sortie/return/writeback cadence instead of lingering forever.

## Prior packet closure to preserve

Do not reopen the light/horde bridge by drift. The closure proof is `.userdata/dev-harness/harness_runs/20260427_031951/` via:

```sh
python3 tools/openclaw_harness/startup_harness.py probe bandit.live_world_nearby_camp_light_horde_mcw
```

Evidence class: copied-save loaded-map visible fire/light bridge proof. The fixture sets night (`5234852 -> 5266800`) and places a safe raw `fd_fire` at `offset_ms=[10,0,0]`; `probe.artifacts.log` records `light_time=night`, `light_packets=1`, and `bandit_live_world horde light signal: ... horde_signal_power=22 range_cap_omt=11`. This proves the live light observation reaches `overmap_buffer.signal_hordes(...)`; it is **not** full player-lit brazier/wood/lighter product proof, and it is not the parked smoke/fire site-refresh loop.

## Previous smoke/fire stop condition to preserve

Do not run more blind smoke/fire site-refresh probes for `bandit-live-signal-smoke-source-site-refresh-proof` unless Josef explicitly reopens it.

Preserve the reviewed blocker: `.userdata/dev-harness/harness_runs/20260427_013136/` produced `signal_packet=yes` but `matched_sites=0 refreshed_sites=0 rejected_by_signal_range=1`, then decayed to no-signal. Raw saved fields remain map-field reader proof only; synthetic smoke would be smoke-source/live-signal proof only; player-fire proof still requires the real brazier/wood/lighter/player-action chain.

## Active implementation target

Read `doc/bandit-local-sight-avoid-and-scout-return-cadence-packet-v0-2026-04-26.md` first, then inspect the current live owner/contact seams in:

- `src/do_turn.cpp`
- `src/bandit_live_world.cpp`
- `src/bandit_live_world.h`
- `tests/bandit_live_world_test.cpp`
- relevant nearby-camp harness scenarios/fixtures (`bandit.live_world_nearby_camp_mcw` or equivalent)

Desired slice:

- current/recent exposure can make stalking/hold-off bandits attempt a bounded local reposition toward cover or broken line of sight
- no teleporting, no perfect future sight-cone omniscience, no broad local combat AI rewrite
- scout outings get a finite sortie window and can return home/write back after watching long enough
- returned scouts can refresh owned-site memory/pressure for later re-evaluation, but no automatic larger spawn cheat
- reviewer-readable output should distinguish `still stalking`, `repositioning because exposed`, `returning home`, and later re-dispatch/escalation decisions

## Current checkpoint - 2026-04-27

Deterministic/source slice is now ready as a reviewable behavior checkpoint:

- `src/bandit_live_world.cpp` / `.h`: bounded adjacent sight-avoid choice, active sortie start/contact clocks, scout timeout decision, active job typing, active sortie serialization/deserialization, return-packet job typing and cleanup
- `src/do_turn.cpp`: live local gate now feeds player LOS plus nearby allied/basecamp observer LOS into sight-avoid, notes sortie start/contact, routes an expired single scout home, observes already-returning scouts as `returning_home`, and logs scout return pressure refresh
- `tests/bandit_live_world_test.cpp`: covers current/recent exposure, no teleport/no perfect omniscience, finite scout expiry, return-home/writeback, and serialization of the new active sortie fields

Validated locally:

```sh
git diff --check
make -j4 tests LINTJSON=0 ASTYLE=0
./tests/cata_test "[bandit][live_world]"
# all 524 assertions in 22 test cases passed
make -j4 cataclysm LINTJSON=0 ASTYLE=0
make -j4 cataclysm-tiles TILES=1 LINTJSON=0 ASTYLE=0
```

Live/harness proof is still open and must not be overclaimed. Current-build attempts:

- `.userdata/dev-harness/harness_runs/20260427_040319/` (`bandit.live_world_nearby_camp_mcw`) -> `inconclusive_no_new_artifacts`
- `.userdata/dev-harness/harness_runs/20260427_041554/` (`bandit.extortion_at_camp_standoff_mcw`) -> reached gameplay screenshots but hung during the long standoff advance; killed manually

Next Andi action: change the live proof shape before rerunning. Prefer a shorter or fixture-assisted nearby owned-site probe that starts with an active scout/local-contact state and advances only enough to see either bounded reposition or return-home/writeback.

## Validation plan

Remaining live bar:

- one live/harness proof on nearby owned-site footing showing a scout either scooches out of exposure or returns after the sortie window

## Non-goals

- no Lacapult work
- no release publication, signing, notarization, or repo-role surgery
- no mutation of Josef's real saves/userdata
- no broad local AI rewrite unless the active slice truly requires it
- no reopening the parked smoke/fire site-refresh proof loop by drift
