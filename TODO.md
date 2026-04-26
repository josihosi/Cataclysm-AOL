# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active target: `Bandit live signal + site bootstrap correction v0`.

- Work from `/Users/josefhorvath/Schanigarten/Cataclysm-AOL` on `dev`; `josihosi/Cataclysm-AOL` is project/release truth and `josihosi/C-AOL-mirror` is green-dot-only.
- Use `doc/test-vs-game-implementation-audit-report-2026-04-26.md` as the immediate footing: smoke/light/weather marks, generated mark ledgers, human-route sightings, and playback scout tuning are not live until the producer/consumer bridge exists.
- Implement the first correction package from the report: `doc/bandit-live-signal-site-bootstrap-correction-v0-2026-04-26.md`.
- Continue from the current source footing: abstract hostile-site ownership registers from existing loaded overmap specials before NPC materialization; concrete spawn claims reconcile to the same ledger; the live dispatch master candidate gate uses the `40 OMT` system envelope instead of `distance <= 10`; live `fd_fire` / `fd_smoke` fields near the player create bounded smoke signal candidates via the real `overmap_npc_move()` path; and selected abstract candidates now lazily materialize only a minimum scout-dispatch roster instead of eagerly spawning every camp member.
- Next: prove or tighten the real path with one harness/live smoke-or-fire source and one no-signal control, then decide whether this slice needs light-source intake now or whether light remains the next bounded hook.
- Extend live dispatch/logging from this minimal signal candidate footing toward the remaining reviewer-readable reasons: cadence skip, hold/chill, signal decay/refresh, and any remaining abstract-to-concrete dispatch/materialization limits.
- Preserve deterministic mark/playback tests as adapter regressions, but do not claim full gameplay closure until source hook + live/harness/log evidence reaches the real path.
- Do not publish releases, touch Lacapult, mutate Josef's real saves/userdata, or start lower-priority follow-ups before this package has a bounded implementation shape.

Keep this file focused on the active lane only, ja, otherwise it turns into a junk drawer with headings.
