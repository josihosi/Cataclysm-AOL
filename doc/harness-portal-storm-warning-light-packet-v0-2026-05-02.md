# CAOL-HARNESS-PORTAL-STORM-WARNING-LIGHT-v0 (2026-05-02)

Status: GREENLIT / QUEUED / HARNESS-HARDENING FOLLOW-UP

Source: Josef Discord, 2026-05-02: “It seems portal storms sometimes break the harness! Need a flashing light for that.”

## Intent

Make portal-storm contamination impossible to miss in C-AOL harness runs.

If a probe/handoff/repeatability run starts during, enters, or saves with portal-storm weather, the report should flash a big warning instead of leaving Schani/Andi to discover later that the scenario went sideways because reality put on its stupid carnival hat.

## Product shape

Add a prominent harness warning/light for portal storm state:

- report-level field, e.g. `portal_storm_warning.status = active|observed|not_observed|unknown`;
- short human-readable summary in `probe.report.json`, `handoff.report.json`, and repeatability summaries;
- step-ledger warning/yellow row when portal storms are observed in a scenario that did not explicitly allow them;
- optional scenario flag to allow or require portal storm weather for tests that intentionally exercise it;
- clear final summary text such as `⚠ PORTAL STORM ACTIVE / HARNESS RESULT MAY BE CONTAMINATED`.

“Flashing light” means reviewer-visible signal, not literally a new graphics system unless implementation finds a cheap way to surface it in HTML/report output.

## Evidence target

- A deterministic/unit-style harness test or fixture proves portal storm weather is detected from saved dimension weather metadata (`WEATHER_PORTAL_STORM` / portal-storm equivalent).
- A negative control proves ordinary weather does not raise the warning.
- A scenario/report proof shows the warning lands in the run report/summary and changes the ledger status when portal storms are not allowed.
- If a scenario opts into portal storms, the warning is present but classified as expected/allowed rather than contaminating.

## Success state

- [ ] Harness reads the relevant saved/current weather state and recognizes portal storm weather explicitly.
- [ ] `probe`, `handoff`, and `repeatability` report surfaces include a visible portal-storm warning/light when observed.
- [ ] Step ledger or report status makes unallowed portal storm contamination yellow/red instead of silent green.
- [ ] Scenarios can explicitly allow or require portal storms without making every intentional portal-storm test fail.
- [ ] Negative-control proof shows normal weather does not trigger the warning.
- [ ] Documentation tells Andi/Schani how to interpret the warning: rerun under controlled weather unless the scenario intentionally tests portal storms.

## Non-goals

- Do not solve portal-storm gameplay behavior here.
- Do not redesign weather simulation.
- Do not rerun old proof packets solely because a future warning exists.
- Do not interrupt the active camp-locker cleanup lane unless Schani/Josef explicitly promote this harness-hardening item.
