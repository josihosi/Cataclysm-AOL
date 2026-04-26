# Andi handoff: Test-vs-game implementation audit report packet v0

## Classification and position

- classification: ACTIVE / GREENLIT NOW
- authoritative canon: `Plan.md`
- packet doc: `doc/test-vs-game-implementation-audit-report-packet-v0-2026-04-26.md`
- repo: `/Users/josefhorvath/Schanigarten/Cataclysm-AOL` on `dev`

## Item title

Test-vs-game implementation audit report packet v0

## Why this is first

Josef greenlit the C-AOL debug-note correction stack and explicitly asked us to remove hollow code. The first job is to check which tests actually reach game implementation and which tests only prove a deterministic seam, authored playback packet, or harness shape. Do this before implementing more bandit/basecamp corrections, or we risk building a nicer-painted cardboard door.

## Implementation scope

1. Read `Plan.md`, `TODO.md`, `SUCCESS.md`, `TESTING.md`, and the packet doc before editing.
2. Produce `doc/test-vs-game-implementation-audit-report-2026-04-26.md` or an explicitly equivalent report.
3. Prioritize current high-risk test lanes:
   - `tests/bandit_mark_generation_test.cpp`
   - `tests/bandit_playback_test.cpp`
   - `tests/bandit_live_world_test.cpp`
   - relevant `tests/faction_camp_test.cpp`
   - relevant `tests/clzones_test.cpp`
4. For each audited pass condition, record:
   - test file / scenario
   - pass condition
   - source seam under test
   - live producer, if any
   - live consumer, if any
   - evidence class
   - verdict: live-wired, deterministic-only, playback-only, harness-only, stale/ambiguous, or hollow/misleading
5. Explicitly trace smoke, light, weather, horde attraction, site bootstrap, dispatch, local handoff, and scout behavior through live producers/consumers.
6. Assign each hollow/missing bridge to one of the greenlit stack items or name a new follow-up if it does not fit.
7. Update `TESTING.md` compactly with the audit result and the next implementation target.

## Greenlit stack after the audit

1. `Bandit live signal + site bootstrap correction v0`
2. `Bandit live-wiring audit + visible-light horde bridge correction v0`
3. `Bandit local sight-avoid + scout return cadence packet v0`
4. `Smart Zone Manager v1 Josef playtest corrections`
5. `Basecamp medical consumable readiness v0`
6. `Basecamp locker armor ranking + blocker removal packet v0`
7. `Basecamp job spam debounce + locker/patrol exceptions packet v0`

## Non-goals

- no release publishing
- no Lacapult work
- no signing/notarization/repo-role surgery
- no mutation of Josef's real saves/userdata
- no implementation of every queued package inside this audit packet
- no deleting deterministic tests just because they are deterministic; relabel them honestly

## Success bar

- A concrete report exists and maps high-risk tests to live implementation paths or missing bridges.
- Bandit AI / camp tests are covered first and reviewer-cleanly.
- Smoke/light/weather/horde/site-bootstrap/dispatch/local-handoff/scout paths are classified by evidence class.
- Hollow or missing bridges are assigned to the correct greenlit package.
- `TESTING.md` names the audit outcome and next implementation target.

## Testing / evidence expectations

Docs/audit lane, so no compile is required unless you change code. Use code search/call-path inspection as evidence. If you do run proof commands, record them in the report, but do not turn the audit into an accidental implementation branch.

## Cautions

A passing test is not product truth unless the live game consumes the tested seam. Name the bridge. If the bridge is missing, say missing. No theater, bitte.
