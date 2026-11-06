# Long-running natural-play soak

<!-- caol-catalogue-family: {"family_id":"natural-play-endurance","title":"Natural-play endurance","purpose":"Observe crashes, dispatch and performance across varied journeys and reloads.","kind":"maintained","starting_save":"Starting save not yet defined"} -->

Owner-selected final batch under `R-REL-RELEASE-PLAYTESTS-S001`. Run after the planned
mechanism/regression playtests. This is a broad crash, stuck-dispatch and performance-spike test, not another
full assertion suite or proof of unobserved faction behavior.

## Five worlds

Use five distinct recorded world seeds and separate save/profile/session directories on one
coherent immutable product. Keep natural terrain, roads, obstacles and ordinary monsters.
Choose the following route emphases where the generated terrain supports them; do not spend
hours hunting perfect seeds. A world may cover several emphases, but the batch should include:

1. **Open countryside:** fields, roads and long sightlines; settlements spread along a long trip.
2. **Forest and rivers:** bends, dense woodland, bridges or other actual detours.
3. **Town outskirts:** buildings, doors, road junctions and transitions between town and wilderness.
4. **Crowded overlap:** several faction homes and ordinary monsters near the same travel corridor.
5. **Sparse distance:** widely separated homes, long absences and later returns to old ground.

Each world should contain bandit and cannibal homes, Mi-go towers, writhing stalkers, zombie
hordes and ordinary monsters. Use naturally generated sites when suitable; supplement missing
representative entities/sites through supported setup on plausible natural terrain. Do not place
everything in a single arena. Distribute homes around the starting route at roughly 3–8, 15–25
and 35–50 OMT; vary faction/distance pairings across seeds. Record actual positions and seed,
not a claim that every generated map naturally supplied that layout. Leave NPC capabilities,
perception, mission decisions, casualties and movement under the normal production simulation.

Enable and verify avatar invisibility and Debug Life Support (`DEBUG_LS`) individually using
[setup controls](setup-and-interactions.md#mutation-setup-for-suite-runs). Life support removes
needs noise, not incoming damage. Choose practical shelter/routes and supported avatar protection
when necessary; keep debug light off. This is not survival, balance or avatar-detection proof.

## Repeatable journey: at least seven actual game days per seed

Use this itinerary as a route shape, adapting coordinates and travel duration to each map.
Time counts from the first setup-complete World. Movement and native waits must actually earn
at least seven days; never edit the date or credit requested-but-interrupted waits.

| Period | Journey |
| --- | --- |
| Day 1 | Travel through the near sites and a terrain boundary. Establish a small real fire or light source in a suitable exposed location near the route, with actual fuel/power. Observe any resulting ordinary activity. Save, quit to menu, exit the game and reload the same world. |
| Day 2 | Travel roughly 15–25 OMT along roads and off-road stretches. Leave operating parties behind. Spend a substantial quiet interval at a different shelter, including a night; let the world act without constant intervention. |
| Day 3 | Return by another approach through previously loaded ground. Inspect a few previously seen scouts/parties. If an outing is active, use it for the second full save/exit/reload cycle; otherwise record that opportunity absent and reload during ordinary travel. Continue moving/waiting after loading. |
| Day 4 | Take a longer excursion, crossing both local loading and the production faction-activation boundary. Use roughly 45–60 OMT separation from an earlier home where practical, and check actual living-member distances rather than assuming distance from home alone freezes a party. Spend about a day away through real waits. |
| Day 5 | Return to the earlier corridor and revisit its homes/source sites. Let any surviving source burn out or deliberately extinguish it; establish a later ordinary source elsewhere. Judge only actual observations/responses. Perform the third full save/exit/reload cycle after changing region. |
| Day 6 | Take a different leg through the seed's distinctive terrain, including nighttime travel or waiting. Cross previously visited ground again; allow normal encounters, retreats, returns and weather rather than forcing outcomes. |
| Day 7 and finish | Revisit old actors/homes after the long absence. Finish enough real travel/waiting to reach seven elapsed days. Perform the fourth full save/exit/reload cycle, then continue a meaningful travel/wait segment and check actor continuity before final save and native exit. |

Travel may consume more than the nominal day; keep the ordering and seven-day minimum rather
than resetting time or forcing a timetable. Where accessible terrain permits, cover at least
three separated regions and more than one outbound/return route. A second week repeats a different
circuit with fewer interventions; use it when useful for an unresolved long-period behavior, not
as a mandatory replay of all five worlds. Five seven-day worlds mean at least 35 actual world-days.

A reload cycle means **save → native quit → verified game/helper exit → supported same-save
launch → World → actual continued play**. Reopening a menu or reading a save is not a cycle.
Where naturally available, distribute cycles across outbound/watch/return activity and quiet
travel; lack of a particular mission phase is not permission to fabricate one. Do not reset
actors, reload an earlier save to undo consequences, or erase a crash with a fresh world.

## Interruptions and lightweight observations

Keep safe mode off, checking after entry/reload or a blocked move. The explicit CLI `ignore`
wait policy continues through supported danger and damage interruptions; invisibility and
Debug Life Support do not make the avatar immune to damage. Before long waits, check actual
health, effects and surroundings, choose practical shelter and care, and use supported avatar
protection when warranted. Use ignore for understood routine interruptions, not for a freezing
warning: stop the wait, get dry and warm in suitable shelter, and check health and effects before
resuming. Treat pain or falling HP as a reason to stop and investigate; when the cause or safety
is uncertain, use short waits and recheck before a long absence. Preserve actual consequences.
Choose currently advertised native wait durations and batch them toward the next day or useful
journey boundary; a one-day requested wait is not automatically a one-day
completed wait. Collect pending CLI requests instead of submitting them again. Resolve a known
harmless prompt through its advertised Ignore control. Unknown dialogs, debug errors, crashes
and lost input ownership require inspection; never blanket-dismiss them as monsters.
See [live operation](live-operation.md). Invisibility is not a substitute for these controls.

At each region departure/revisit and reload, take a compact sample of the same few relevant
scouts/parties: identity, position, current purpose/phase and actual game time. Include both
human factions and Mi-go activity when present. Look for persistent nonprogress despite an
available route/eligible activity, impossible oscillation, duplicate or lost actors, broken
return/home settlement, and a simulation that stops responding. A guard holding position,
ordinary rest/watch, completed mission, dead actor or legitimately inactive distant party is
not a stuck scout. Check local conditions and the production activation predicate before
calling a stall. Follow a suspicious actor across another useful interval or revisit; no
per-turn transcript or exhaustive actor census is required.

### Performance spikes and stalls — week and month

Performance is a primary outcome alongside crashes and stuck dispatches in both durations.
Use existing supported game-turn/wait timings and performance alarms for a compact ordinary
throughput reference early in the run and at useful travel/reload milestones. Compare actual
simulated time against wall time for comparable actions; interrupted waits, rendering, loading
and external/model waits must not be mislabelled as overmap simulation cost. No invented fixed
threshold or continuous profiling is required.

When a pronounced slowdown appears, note game time, action/elapsed time, nearby creature
load and relevant party/dispatch activity. Make a bounded useful comparison after leaving the
dense local bubble or continuing through sparse terrain, keeping product and world continuity.
If useful, compare a similar earlier quiet interval or save/reentry, explicitly noting changed
conditions. Dense local creatures can explain work, but density alone does not excuse a stall.
Look for cost that persists or grows with accumulated overmap AI/dispatch bookkeeping despite
sparse local activity. Attribution requires evidence. Report uncertainty honestly while investigating, but keep the
performance finding open until its cause is supported. Missing tooling/evidence is a concrete
blocker to solve or escalate, not a completed unknown-cause result; never fabricate an explanation. Retrieve
bounded existing timing/profile detail and involve the source owner only when it can distinguish
causes or guide a demonstrated repair. Do not add per-turn dumps, an always-on profiler, a new
benchmark campaign or extra worlds. Record compact before/after timings, conditions, recovery
and first anomaly in the same per-seed/month result; manage log growth as below.

At the end report one compact row per seed: product/seed/run, actual elapsed days, completed
reload cycles, route/terrain coverage, activity actually observed, first crash/stuck-dispatch/performance anomaly, and cleanup state. For a defect retain the first divergence, actor/phase/time
and concise reproduction steps in the existing task context; keep a necessary current save
only while diagnosis/continuation needs it. A blocked or crashed world remains incomplete
coverage, even if the other four finish. Investigate useful failures; do not restart healthy
worlds merely because one failed. Remove finished run residue after normal closeout.


## Final single-world month crash soak

After the five shorter runs, choose **one** scenario/world and continue it to at least
**30 actual elapsed game days from its setup-complete start**. Its already-earned 7–14 days
count toward that total; report the starting day and additional days honestly. This is one
month-long follow-on, not five month-long tests or a fresh replay of the first week.

Sol selects the starting world before its useful save is cleaned up. Give a one-sentence
reason grounded in the shorter runs: preferably the world with the richest sustained mix of
active factions, travel/loading boundaries and terrain; a world showing suspicious long-term
accumulation can be more useful than the busiest one. Do not select a known crash that prevents
continuation and simply wait on it—diagnose that failure first. Keep the selected same-world
save as current working input; clean up the other completed worlds normally.

One direct Luna continues a loose loop of travel to another region, a long native wait,
return by another route, then periodic full save/exit/reload and resumed play. Include quiet
multi-day absences and revisits. Keep invisibility/life support and supported ignore handling;
leave NPC simulation real. The main question is whether prolonged ordinary simulation and
reentry crash, hang, stop making progress or develop pronounced performance degradation.
Apply the shared performance observations above, including sparse-versus-dense comparisons. Do not turn this into another detailed faction
assertion campaign or require daily reports.

**Keep output small:** use existing supported quiet/default logging and compact observations;
no per-turn actor dumps, repeated full snapshots, videos or copied save archives. Keep one
current continuation save and the data the live harness actually needs. Check free space and
log growth at natural save/reload boundaries; correct excessive output at its producer through
the owning worker rather than deleting files under a live writer. A noisy harness needs a
supported reduction, not suppressed crash/error reporting or invented logging switches.

Record only the selected seed/product and selection reason, starting/final actual day,
brief save/reentry milestones, and the result. On a crash/hang or performance spike, preserve the first error or timing anomaly,
last known game time/action and a concise reproduction handle while diagnosis needs them.
Normal progress needs no growing narrative. Thirty days with no observed crash is a bounded
soak result, not proof of bug-free AI. Finish with native exit, process verification and removal
of generated residue once no continuation or diagnosis needs it.

## Parallel execution

Five independent Luna players may run these worlds concurrently when measured CPU, memory,
storage and input responsiveness support it. One direct Luna owns each game's input and
closeout; Sol oversees the batch. Share the immutable executable/data read-only, never a
mutable save/profile, and coordinate any source/build replacement with the live runtime owner.
Start with available capacity, stagger setup/reloads if useful, and reduce concurrency if
contention impairs the test. Five seeds are the coverage requirement, not a five-process quota.
No Windows use or new host admission is implied.

## Disk writes and diagnostic growth — week and month

Alongside the existing performance milestones, capture compact process disk-write deltas when supported and diagnostic/log file growth over useful advancing-time intervals. Use existing observation surfaces, not a per-turn recorder. Separate logical logger bytes from physical disk I/O, and legitimate save/exploration growth from repetitive diagnostics. Check whether output grows with active work or merely accumulated history. If excessive output appears, identify its producer and compare a bounded targeted diagnostic reduction with equivalent simulation and timing; preserve required error/crash evidence. Missing physical-I/O counters are an explicit measurement limit, not proof of no writes. Retain concise interval totals, cause and corrective result in the existing task result. Both five-seed week runs and the single month continuation carry this check; counts/durations/reloads remain unchanged.
