# Andi handoff — CAOL-ZOMBIE-RIDER-CLOSE-PRESSURE-NO-ATTACK-v0

Classification: GREENLIT / QUEUED after active camp-locker unless Schani/Josef explicitly promotes it.

Read first:
- Contract: `doc/zombie-rider-close-pressure-no-attack-packet-v0-2026-05-02.md`
- Imagination source: `doc/zombie-rider-close-pressure-no-attack-imagination-source-2026-05-02.md`
- Runtime watch notes: `/Users/josefhorvath/.openclaw/workspace/runtime/caol-live-watch-20260502/zombie-rider-live-watch-notes-2026-05-02.md`

Core bug smell: player observed rider not attacking at all while trace reportedly showed repeated `decision=bow_pressure`, `reason=line_of_fire`, `line_of_fire=yes`, distance around `4-5`. Name the decision-to-action break before tuning aggression.

Scope:
- reproduce/minimize close or indoor no-attack;
- identify action-layer cause;
- fix visible attack pressure or named refusal;
- add irregular bunny-hop/circle repositioning when shot is blocked/too close;
- include corrected rider description text.

Non-goals:
- do not reopen all zombie rider v0;
- do not break wounded retreat, cover/LOS, camp-light banding, no-light controls;
- no perfect orbit, no instant-kill tuning, no wall-suicide.

Success bar: deterministic bridge/reposition tests + fresh clean live/handoff proof with screenshots/artifacts, plus existing rider guarantees still green.
