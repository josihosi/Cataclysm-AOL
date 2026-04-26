# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active target: `Test-vs-game implementation audit report packet v0`.

- Work from `/Users/josefhorvath/Schanigarten/Cataclysm-AOL` on `dev`; `josihosi/Cataclysm-AOL` is project/release truth and `josihosi/C-AOL-mirror` is green-dot-only.
- Read the canonical contract: `doc/test-vs-game-implementation-audit-report-packet-v0-2026-04-26.md`.
- Audit current C-AOL tests, especially bandit AI / camp lanes, against the real game paths they claim to prove.
- Produce `doc/test-vs-game-implementation-audit-report-2026-04-26.md` or an explicitly equivalent report.
- For each high-risk pass condition, name the test file/scenario, pass condition, source seam, live producer, live consumer, evidence class, and verdict: live-wired / deterministic-only / playback-only / harness-only / stale-ambiguous / hollow-misleading.
- Explicitly trace smoke, light, weather, horde attraction, site bootstrap, dispatch, local handoff, and scout behavior through live producers/consumers.
- Assign each hollow/missing bridge to the greenlit stack item that owns it.
- Update `TESTING.md` with a compact result and the next implementation target after the audit.
- Do not publish releases, touch Lacapult, mutate Josef's real saves/userdata, or implement every queued package inside this audit packet.

After this active packet closes, take the next item from `Plan.md`'s greenlit implementation stack. Keep this file focused on the active lane only, ja, otherwise it turns into a junk drawer with headings.
