# Camp patrol aggression/alarm/report hygiene proof v0 — 2026-05-04

Lane: `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`, Slice 6.

## Claim

Camp patrol workers now respond to visible hostiles instead of passively obeying non-engagement rules:

- patrol-assigned camp NPCs target hostile bandit NPCs and zombies on sight;
- neutral/no-faction NPCs do not trigger a false hostile target;
- active bandit shakedown/toll/parley contact raises patrol readiness but is watched rather than converted into shoot-on-sight combat;
- a hostile sighting raises a temporary camp patrol alarm;
- while alarmed, all patrol-priority workers enter the current patrol roster independent of the normal day/night split;
- routine patrol assignment / inactive-route chatter no longer appears as visible player messages, while debug/artifact lines remain available.

## Source / behavior checkpoint

Changed source classes:

- `src/npcmove.cpp`: patrol-assigned camp NPCs with an active patrol zone bypass passive/self-defense-only engagement restrictions for visible non-parley hostiles, raise `basecamp::raise_patrol_alarm`, watch active shakedown/toll/parley members without selecting them as combat targets, and suppress routine visible patrol route/inactive reports.
- `src/npctalk.cpp`: patrol-duty camp workers reuse the existing `NPC_MISSION_GUARD_PATROL` noise-investigation primitive even if their default follower rules say `ignore_noise`; noise by itself only queues investigation, while normal hostile/LoS combat still decides attacks.
- `src/basecamp.cpp` / `src/basecamp.h`: camp patrol alarm state is tracked, invalidates the shift cache, and expands the current shift roster to all patrol-enabled camp workers while active.
- `tools/openclaw_harness/startup_harness.py`: patrol cache artifact parser accepts both old cache lines and new `alarm=` cache lines, and explains alarm-expanded rosters in patrol summaries.
- `tests/faction_camp_test.cpp`: focused Slice 6 coverage for alarm roster behavior, hostile-bandit vs neutral targeting, active shakedown contact non-escalation until `fight_unresolved`, zombie targeting, and visible-report hygiene.

## Evidence

Local gate run after current edits:

```text
git diff --check
python3 -m py_compile tools/openclaw_harness/startup_harness.py
make -j4 obj/basecamp.o obj/npcmove.o obj/npctalk.o tests/faction_camp_test.o tests LINTJSON=0 ASTYLE=0
./tests/cata_test "[camp][patrol]" --reporter compact
```

Result:

```text
Passed all 12 test cases with 222 assertions.
```

Representative current-build log lines from the focused patrol run:

```text
camp patrol: cache camp=Patrol Camp shift=day alarm=false workers=2 roster=1 active=1 reserve=0 clusters=0=[(10,10,0)]
camp patrol: alarm camp=Patrol Camp spotter=5 until=Year 1, Mar 21 9:07:58 AM
camp patrol: cache camp=Patrol Camp shift=day alarm=true workers=2 roster=2 active=1 reserve=1 clusters=0=[(10,10,0)]
camp patrol: alarm camp=faction_camp spotter=8 until=Year 1, Mar 21 8:27:58 AM
camp patrol: alarm camp=faction_camp spotter=11 until=Year 1, Mar 21 8:27:58 AM
camp patrol: alarm camp=faction_camp spotter=13 until=Year 1, Mar 21 8:27:58 AM
```

Harness parser compatibility check:

```text
PYTHONPATH=tools/openclaw_harness python3 - <<'PY'
from startup_harness import summarize_patrol_artifacts
old_line = 'camp patrol: cache camp=Patrol Camp shift=day workers=2 roster=1 active=1 reserve=0 clusters=0=[(10,10,0)]'
new_line = 'camp patrol: cache camp=Patrol Camp shift=day alarm=true workers=2 roster=2 active=1 reserve=1 clusters=0=[(10,10,0)]'
old = summarize_patrol_artifacts('old', [old_line])['cache']
new = summarize_patrol_artifacts('new', [new_line])['cache']
assert old['alarm'] == 'unknown', old
assert new['alarm'] == 'true', new
print('patrol parser compatibility ok:', old['alarm'], new['alarm'])
PY
```

Result:

```text
patrol parser compatibility ok: unknown true
```

## Boundary

This is a focused deterministic/source-path checkpoint for camp patrol aggression, alarm roster, patrol-duty sound investigation, active shakedown-contact non-escalation, and report hygiene. It preserves the Slice 1 Pay/Fight negotiation path by keeping patrol readiness separate from choosing the shakedown `fight` branch. It does not claim a broader military command simulation, hostile classification by vibe, or full live overmap combat-playfeel coverage.
