# Zombie rider map-AI / funness benchmark suite v0 — 2026-04-30

This suite belongs to `CAOL-ZOMBIE-RIDER-0.3-v0`. It defines the tests and playtests that make the rider fun instead of merely lethal.

## Deterministic / map-AI test suite

### Test A — endpoint evolution gate

Purpose: prove riders are endpoint-tier, not early routine spawns.

Pass:
- no rider spawn/evolution before configured mature-world threshold in normal conditions;
- rider can appear after endpoint/equivalent mutation pressure;
- chosen lineage is documented, e.g. zombie hunter / advanced predator branch / custom endpoint path.

Fail:
- day-80 routine rider leaks;
- endpoint gate is undocumented folklore.

### Test B — overmap light attraction

Purpose: prove strong late-game light can create rider interest.

Pass:
- strong camp/base/fire/vehicle/flood/elevated light creates bounded rider interest within configured range;
- no-light or weak-light negative control does not attract riders;
- output includes reason and score/confidence, not just “target yes”.

Fail:
- light has no map-AI effect;
- any tiny light summons endpoint cavalry from nowhere.

### Test C — light memory decay

Purpose: prove turning lights off matters.

Pass:
- rider interest decays after light disappears;
- stale light does not permanently doom the camp;
- recent bright light may still leave a short-lived investigation window.

Fail:
- light-off has no effect;
- memory vanishes instantly with no consequence, making pressure toothless.

### Test D — rider convergence and band formation

Purpose: prove two riders meeting can form a band without infinite clown cavalry.

Pass:
- two riders that converge/meet can enter a rider-band state;
- band state has a cap, identity, or cooldown preventing unbounded accumulation;
- decision trace distinguishes lone harass from band behavior.

Fail:
- riders never coordinate;
- riders multiply/accumulate forever;
- band formation is invisible and untestable.

### Test E — circle/harass rather than wall-suicide

Purpose: prove camp pressure feels like mounted siege/harassment.

Pass:
- rider/band near defended or lit camp can circle/harass/hold/reposition rather than charge walls blindly;
- if direct attack is chosen, reason explains advantage or breach.

Fail:
- mounted endpoint horror dies on walls like a moth in a pub lamp;
- it becomes a stationary turret outside the camp forever.

### Test F — local shoot/flee cadence

Purpose: prove ranged mounted combat has rhythm.

Pass:
- approach/shoot/reposition/flee loop occurs with cooldown/range/LOS constraints;
- cover, indoors, terrain, smoke/darkness, injury, or line-of-sight break can disrupt it;
- speed feels dangerous but does not erase all counterplay.

Fail:
- instant sniper deletion;
- permanent unbreakable kiting;
- melee-brute behavior with a decorative bow.

## Live / harness playtest rows

### Row 1 — open-field terror

Setup: mature-world rider, player in open terrain, long sightline.

Pass: rider closes or controls distance fast enough that running is obviously bad, shoots/repositions, and remains stable/perf-safe.

Counterplay expectation: player should still be able to seek cover or break conditions if a plausible route exists.

### Row 2 — cover / indoor escape

Setup: rider pressure with nearby building/forest/rubble/door/smoke/darkness.

Pass: cover or indoor transition meaningfully disrupts shooting and/or pursuit.

Fail: rider deletes player through all counterplay.

### Row 3 — camp light attraction

Setup: mature-world camp with strong light versus no-light/low-light control.

Pass: strong light creates bounded rider interest/approach/harass; no-light control does not.

### Row 4 — rider band circling camp

Setup: two riders converge near lit camp.

Pass: riders can form a band and circle/harass/hold with readable band state and caps.

Fail: no banding, infinite banding, or suicidal wall charge.

### Row 5 — wounded rider disengagement

Setup: rider attacks/harasses, then takes meaningful damage.

Pass: rider flees/repositions/disengages rather than dying heroically or snapping into unfair final-shot spam.

## Required report fields

For each test/playtest row:

- command/scenario/test name;
- run id / artifact path;
- start conditions: world age/evolution tier, light state, rider count, distance, terrain/cover, player exposure, rider HP;
- decision trace: interest reason, route/posture, shot count, flee/reposition count, band state, target/camp id if relevant;
- pass/fail/yellow verdict and exact blocker;
- stability/perf: wall-clock, turn/cadence timing if available, warnings/errors/log spam, crash status.

## Funness verdict rubric

Green funness requires all of these:

- scary in open ground;
- readable counterplay works;
- late-game light pressure is strategic, not instant deletion;
- rider bands feel like escalation, not uncontrolled spawning;
- ranged harassment has rhythm and break conditions;
- endpoint timing is respected.
