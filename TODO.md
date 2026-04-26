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
- Continue from the current source footing: abstract hostile-site ownership registers from existing loaded overmap specials before NPC materialization; concrete spawn claims reconcile to the same ledger; the live dispatch master candidate gate uses the `40 OMT` system envelope instead of `distance <= 10`; live `fd_fire` / `fd_smoke` fields near the player create bounded smoke signal candidates via the real `overmap_npc_move()` path; live fire also feeds the ordinary-light adapter with current time/weather/exposure detail; selected abstract candidates lazily materialize only a minimum scout-dispatch roster; and repaired run `.userdata/dev-harness/harness_runs/20260427_014408/` now proves an initial matched/refreshed smoke/fire signal before the `|` several-hour wait, followed by decay/no-signal rather than escalation.
- Next: tighten the remaining active packet gaps without widening into the next bridge: clean live proof for the light branch under a threshold-surviving condition, separated signal decay/refresh, and reviewer-readable hold/chill reasons.
- Preserve deterministic mark/playback tests as adapter regressions, but do not claim full packet closure until the remaining light/detail/decay/hold-chill evidence is either implemented and proven or explicitly narrowed out of this packet.
- Do not publish releases, touch Lacapult, mutate Josef's real saves/userdata, or start lower-priority follow-ups before this package has a bounded implementation shape.

Keep this file focused on the active lane only, ja, otherwise it turns into a junk drawer with headings.
