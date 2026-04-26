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
- Register bounded hostile-site ownership without relying solely on the player walking into NPC spawn/load range.
- Wire at least one real fire/smoke/light observation path into live bandit signal candidates under named weather/time conditions.
- Make live dispatch accept or reject those candidates with reviewer-readable reasons: site count, signal packet, weather/light modifiers, candidate distance, cap used, cadence skip, range rejection, or hold/chill.
- Preserve deterministic mark/playback tests as adapter regressions, but do not claim gameplay until source hook + live/harness/log evidence reaches the real path.
- Include one no-signal control.
- Do not publish releases, touch Lacapult, mutate Josef's real saves/userdata, or start lower-priority follow-ups before this package has a bounded implementation shape.

Keep this file focused on the active lane only, ja, otherwise it turns into a junk drawer with headings.
