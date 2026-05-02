# Andi handoff

Current active lane: `CAOL-ZOMBIE-RIDER-CLOSE-PRESSURE-NO-ATTACK-v0`.

Imagination source: `doc/zombie-rider-close-pressure-no-attack-imagination-source-2026-05-02.md`.

Contract: `doc/zombie-rider-close-pressure-no-attack-packet-v0-2026-05-02.md`.

Handoff packet: `doc/zombie-rider-close-pressure-no-attack-handoff-v0-2026-05-02.md`.

Core bug smell: Josef observed a zombie rider in/near the house not attacking at all while trace reportedly showed repeated `decision=bow_pressure`, `reason=line_of_fire`, and `line_of_fire=yes` around distance `4-5`. Name the decision-to-action break before tuning aggression.

Scope:
- reproduce/minimize close or indoor no-attack, using `.userdata/dev-harness/harness_runs/20260502_015857/` only as yellow seed evidence;
- identify action-layer cause;
- fix visible attack pressure or named refusal;
- add irregular bunny-hop/circle repositioning when shot is blocked/too close;
- include corrected rider description text.

Non-goals: do not reopen all zombie rider v0; do not break wounded retreat, cover/LOS, camp-light banding, or no-light controls; no perfect orbit, instant-kill tuning, wall-suicide, stalker/locker/bandit/raptor drift, or release work.

Success bar: deterministic bridge/reposition tests + fresh clean live/handoff proof with screenshots/artifacts, plus existing rider guarantees still green.
