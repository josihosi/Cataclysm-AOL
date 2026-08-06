# Plan

Canonical roadmap for Cataclysm-AOL.

This file answers one question: **what should the agent advance next?**
It is not a changelog, not a graveyard of crossed-off substeps, and not a place to preserve every historical detour.
Rewrite it as reality changes.

## File roles

- **Plan.md** - canonical roadmap and current delivery target
- **SUCCESS.md** - success-state ledger / crossed-off exit criteria for roadmap items
- **TODO.md** - short execution queue for the current target only
- **TESTING.md** - current validation policy, latest relevant evidence, and pending probes
- **TechnicalTome.md** - durable mechanic notes, not daily state tracking
- **doc/work-ledger.md** - compact receipt book for asks, state transitions, evidence links, and owners
- **COMMIT_POLICY.md** - checkpoint rules to prevent repo soup

If these files disagree, **Plan.md wins** and the other files should be repaired.

## Working rules for agents

- Do **not** mechanically grab the first unchecked-looking thing from some list.
- Follow the current delivery target below and move it to its **next real state**.
- Josef being unavailable for playtesting is **not** a blocker by itself.
- When a target is waiting on Josef, move to the next best unblocked target.
- If no good unblocked target remains, send Josef a short parked-options note so he can greenlight the next lane; do not just keep revalidating the old packet.
- During the current debug-proof finish stack, failed agent-side proof does **not** close or park implemented code. After the attempt budget, move implemented-but-unproven items to Josef's playtest package and continue the next greenlit debug note.
- Prefer batching human-only asks where practical. One useful packet with two real product questions beats two tiny pings.
- Keep these files lean. Remove finished fluff from `TODO.md` and `TESTING.md` instead of piling up crossed-off archaeology.
- Each real roadmap item needs an explicit success state in `SUCCESS.md` (or an equally explicit inline auxiliary) so completion is visible instead of guessed.
- Cross off reached success-state items; only remove the whole roadmap item from `Plan.md` once its success state is fully crossed off / done.
- Prefer agent-side playtesting first. Josef should be used for product judgment, feel, priority calls, or genuinely human-only checks.
- Validation should match risk:
  - docs-only change -> no compile
  - small local code change -> narrow compile/test
  - broad or risky code change, or a Josef handoff -> broader rebuild / startup harness as needed
- Follow `COMMIT_POLICY.md`. Do not let the repo turn into one giant dirty blob.

---

## Current status

**Production priority (2026-08-01): `port/cdda-master` is the only current production-candidate branch.** `master`, `dev`, and every other `port/*` branch are behind this line. Do not use them for production playtesting or merge them over the candidate unless the roadmap explicitly promotes a replacement.

The Windows laptop is Josef's playtest machine and the canonical integration checkout. The Mac Mini is the background build, repository-sync, and evidence lane. Both machines should resolve to the same clean candidate commit before handoff. Josef has authorized advancing `origin/port/cdda-master` only as needed to synchronize the reviewed preparation commit; that is not release/tag/publication authorization.

The candidate contains `upstream/master` through `8d4959bee4` (2026-07-18). The latest fetched upstream tip is `7cf1d08ae8` (2026-08-01), 104 commits ahead. A read-only merge simulation finds two conflicts: `src/npcmove.cpp` and Bombastic Perks `closetland.json`. The upstream batch contains useful incremental content and NPC fixes, but no release-blocking C-AOL feature; do not interrupt the current feel playtest with that refresh.

`doc/work-ledger.md` is now the compact receipt book for meaningful asks, state changes, evidence links, owners, supersessions, held lanes, and red/non-credit proof. Use it before trimming active docs.

Detailed contracts, closure evidence, and older checkpoint history belong in `doc/*.md`, `doc/work-ledger.md`, `SUCCESS.md`, and git history. Keep this file short enough that the active stack is visible without archaeology.

---

## Current execution posture

### Active target - CAOL-HOSTILE-CAMP-OVERMAP-ECOLOGY-v0

**Status:** ACTIVE / PHASE-4 LIVE HOLD / PHASE-5 EGRESS RETRY MEMORY NEXT

Josef explicitly promoted the bandit/cannibal hostile-camp implementation on 2026-08-02. The
canonical contract and cross-off evidence ledger is
`doc/hostile-camp-overmap-ecology-implementation-ledger-v0.md`.

Goal: naturally generated bandit and cannibal camps send coherent two-person scout parties, learn
through bounded honest perception, return physical reports, withdraw coherently when burned, and
produce faction-specific bandit shakedowns or cannibal night raids with durable identity,
persistence, bounded performance, and bounded save growth.

Execution posture: all implementation, builds, harness runs, profiling, save-growth work, diffs,
and orchestration stay on the Mac Mini. Work is checkpointed on the isolated `dev` worktree; the
clean `port/cdda-master` checkout at `660057ff728bdf77531f607b1bd42a175f027a5f` remains the
release/playtest target and is not edited directly. Integration remains `dev` -> `master` -> the
port orchestrator, with no upstream merge, push, publication, tag, or release in this lane.

Boundary: bandits and cannibals only. Writhing-stalker AI, zombie-rider AI/progression, and
flesh-raptor behavior remain closed. Honest perception, paired scouting, physical reports,
coherent withdrawal, faction outcomes, persistence, performance, save-bloat, and cross-platform
requirements may not be weakened for convenience.

Phase 0 is complete under Josef's pragmatic engineering-baseline decision. Instrumentation commit
`fee1e44d38` and identical baseline patch `2a3e7efb17` produced one accepted 25-case, three-pair,
150-run Mac packet with CPU, phase RSS, scheduler wait/fairness, serialization, whole-save, and
save-growth evidence. Raw and summary SHA-256 values are `7332059a...` and `9736b3af...`; the
ratified provisional budgets and caveats are recorded in the canonical ledger and external
manifest. The legacy 500-site scan starves 125 of 250 eligible camps in 250 updates; that is an
explicit Phase-3 repair target, not hidden or normalized away.

The Phase-1 authority stack now includes checkpoints `673a900067`, `4995a3c64e`,
`e4b75e15a3`, `42e5bad3cd`, `31354b71c3`, `7acc011951`, `687d7bcecb`,
`67cd68e416`, and `833599e5e4`. The typed active outing is the only runtime owner of a
bounded scout's members, leader, route, target revision, observations, cargo, casualties, phases,
clocks, simulation owner, and independent return/report/cargo receipts. It migrates legacy scalar
saves, releases malformed reservations, preserves split casualties, and rejects contradictory,
stale, or replayed returns before mutation. The latest checkpoint also lets the first returned
survivor deliver one provisional dossier/cargo receipt while the typed reservation remains active;
later on-time return or fixed-grace loss finalizes exactly once, and a new dispatch stays blocked.
The deadline guard rejects an early missing declaration atomically and accepts it only at or after
the persisted boundary.
One expected-phase transition authority now makes burn/return phases irreversible, normalizes an
unknown future saved phase to safe `lost`, and prevents homeward scouts from re-entering the target
gate. Missing legacy phase data still migrates as `assembling`; legacy scavenge keeps its non-report
return owner.
Final physical scout reports now open a separate persisted assessment owner. Provisional reports
and all-loss returns cannot do so; cooldown/idle retain the acted-report watermark, and only a
newer report can revive abandoned pressure. Existing routine dispatch is blocked while assessment,
preparation, or cooldown is active.
Fresh shakedowns and raids now use a separate schema-v5 hostile-operation owner with a new
generation, fresh party, exact report pin, route/rally, canonical receipt keys, and one-way
identity-checked phases. Fresh save repair rejects malformed report, route, key, phase, size, and
home-reserve state; legacy hostile outings remain withdrawal-only.
Empty/normal/cap-saturated JSON is 87/4,558/28,534 bytes; the saturated state is byte-stable after
reload and remains below 64 KiB.

Scout and hostile state changes now require the same serialized activity/generation/owner/epoch/
last-advanced cursor. Ambiguous dual owners, stale epochs, duplicate advances, and inconsistent
save parity fail closed; exact compare-and-swap owner transfers are atomic.

Reference-aware intelligence pruning is checkpointed at `ddd1afe480`: every retained dossier has
a stable ID/revision, active scout/report/decision/hostile owners carry that exact reference,
legacy positive revisions migrate without downgrade, identical evidence is revision-stable, stale
plans reject atomically, and deterministic 64-lead/8-mark/string caps keep the full saturated camp
at 48,070 bytes. Semantic observation compaction is checkpointed at `9be3e8c044`: the 16-fact
working set protects burn/casualty/contradiction/hard-danger/target-revision facts, deduplicates by
stable fact and semantic state, and advances progress only for retained certainty/bounds/route/
alert change. Report policy is checkpointed at `258247d26c`: final reports pin an explicit
bandit-shakedown or cannibal-night-raid policy, and each camp retains at most 64 canonical acted
watermarks by target ID/OMT/policy. Same-key stale/exact tuples, policy drift, explicit unknown
fields, and revision exhaustion fail closed; distinct target/policy keys advance independently.
Component idempotency is checkpointed at `f12180de5f`: canonical return/report/cargo/resource and
per-member receipts validate before mutation, persist through schema migration, and remain bounded
after operation closure. New resource claims require the exact issued camp operation; forged or
terminal receipts fail atomically. Empty/normal/saturated state is 87/6,020/48,265 bytes.
Phase 1 is complete through transition checkpoint `16649b77b0` and all-phase persistence checkpoint
`e408c9c450`. Bounded opt-in events retain exact operation/generation, final owner, phase change,
reason, and minute without entering saves. A real hostile operation now round-trips at every active
phase plus `lost` without synthetic transitions or duplicate/mutated operation, report, receipt,
reservation, or member state.

Phase-2 roster authority is checkpointed at `563499e3fe`, exact routine pairs at `c846be1632`,
fresh post-report response selection at `5fbefa452e`, and capability-aware pairs at `f049104375`.
Bandit/cannibal camp routines now dispatch
exactly two or wait, materialize only that pair plus one required concrete reserve, and preserve the
two-person empty-camp case. Hostile response callers cannot supply member IDs: a pinned current
dossier recomputes threat/reward sizing from the current ready roster, and apply rejects roster or
dossier drift atomically. Routine selection uses live readiness and stable observer/return-safe
escort capability without draining the strongest defenders. Structural reservations are pinned at
`f65e6bd28a`; competing/stale plans cannot steal a newer generation or occupied mission slot.
Generation-matched structural release is checkpointed at `61017301a4`; it cannot clear a newer
same-ID generation, resurrect resolved casualties, or overcount returns. Shared release paths are
checkpointed at `084b7c0747`, and terminal origin handling at `f29808d80b`: success/payment,
abort, death, legacy migration, origin loss, and current load-failure paths preserve exact
activity/generation ownership. Schema 11 persists terminal origin and non-dispatchable survivors;
only a real physical signal recalls a party, and a terminal origin cannot reactivate from later
spawn claims. Phase 2 closes with population/readiness matrix checkpoint `b9fcddaa7b` and
cross-camp stable-identity checkpoint `a8252313b7`: both factions are covered at populations
0-10, same-camp generation races stay atomic, and no NPC ID can be owned by two camps. The
accepted Phase-0 structural probe plus bounded current roster passes supplies the pragmatic
approximately-linear selection gate; the obsolete synthetic dispatch/return fixture is recorded
as non-credit drift rather than repaired into a new benchmark subsystem.
Phase-3 shared routine ecology is green: bandit and cannibal camps now traverse the same structural
scan, exact-pair reservation, abstract advance, and return path, while small hostile sites remain
excluded. Exact persistent-pair ownership is checkpointed at `0247de602e`: current structural
saves require the canonical camp operation and exactly two reserved stable members, preserve that
pair through load, reject forged singleton packets transactionally, and reject duplicate-minute
advancement without a threat read or state change. The bounded shared structural route is
checkpointed at `e537ea7b49`: one exact pair owns a schema-6 radial route, advances through fixed
approach/target/home clocks, and survives save/load without danger or harvest teleporting its
members. Camp-local frontier coverage is checkpointed at `0576113190`: intelligence schema 3
persists one cursor and eight last-resolved timestamps, all eight bounded radius-4/radius-9 routes
cost 18, danger leaves a sector unresolved without starving the other seven, and only physical
home return advances memory. The persisted global fairness envelope is checkpointed at
`83c40e3bc3`: schemas 5/12 resume the hourly eligible-camp cursor, enforce 16 considerations and
two starts, order saturated contenders by real wait age, persist honest no-candidate backoff, and
prove 100/500-site fairness without charging retired/micro sites. The bounded 100-site artifact
services every dispatch-eligible camp within six passes. Fair terrain discovery, exact faction fit,
physical terrain checks, and bounded cheap/final score diagnostics are checkpointed at
`cb53cbafdb`: the accepted 100-site packet services terrain and dispatch 100/100. Routed dispatch
is checkpointed at `cab98bc55c`: exact drive/force-due and acquire/retain/risk boundaries feed a
cheap-top-two route consumer with eight global solves and two starts. Abstract/local ownership is
green through dematerialization checkpoint `f83b6bb116` and local cohesion checkpoint
`71bde93d48`: distinct same-OMT staging tiles, physical-death-only leader re-election, six-tile
cohesion, ten-minute incomplete-assembly timeout, two failed paths, and final live survivor
recheck prevent first-member completion or wedged local ownership. Bounded abstract threat is
checkpointed at `d5e76a447f`: an ordinary remote observer reads only current plus at most three
visible committed route OMTs under remote weather/light, bounded concrete threat work, and no
population-only hostility inference. Structural schema 8 persists one overlap episode, two-detour
withdrawal cap, exact `<P`/`[P,2P)`/`>=2P` attrition, local-owner exclusion, 72-hour wound/casualty
recovery, and replay-safe clearing/re-entry without ever damaging the abstract threat. Final
focused/live-world/save gates pass 5/284, 137/34,527, and 3/34; the 100-site artifact remains under
the provisional Phase-0 CPU/memory/save gates and services 100/100 within six passes.
The current-schema live pair packet is archived as an honest non-credit attempt under external
`phase3-20260803/pair-handoff/MANIFEST.md`. Two meaningfully different direct fixtures were tried:
the north route exposed stale-contact interference, while the corrected east route completed a
real six-hour wait without emitting a structural handoff. The full log identified the concrete
cause: the legacy player-pressure dispatcher matched the structural lead by avatar proximity,
created its own `#dispatch` pair, and blocked structural maintenance as unresolved outside pressure;
inherited spawn-tile headcount also reconciled the intended five-member fixture back to fourteen.
The strict startup classifier was not weakened and no third fixture loop is permitted by Josef's
pragmatic attempt budget. Phase 3's exact-pair ownership, shared route, dematerialization, cohesion,
finite bounty, abstract threat, fairness, and gross CPU/RSS/save gates are deterministically green;
its missing live credit is an explicit limitation carried into the later natural-lifecycle packet.
Phase 4 lead-origin/single-writer footing is checkpointed at `d801058e79`: every current producer
persists an exact origin, cross-origin rewrites fail before mutation, and the legacy player-pressure
consumer cannot steal structural/frontier/terrain leads. Focused origin/camp-map/local-handoff,
full live-world, save-compatibility, save-size, and 63 harness tests are green; exact artifacts live
under external `phase4-20260803/lead-origin/MANIFEST.md`. The compact physical-observation envelope
is checkpointed at `600685c1c2`: schema-1 facts pin exact observer/source/receiver/bucket/revision
provenance, strict batches reject atomically, `(fact,bucket)` retention stays 16/64 bounded, and only
shared evidence crosses final/provisional physical report return. Bandit/cannibal roundtrip,
malformed/replay/cap/migration, structural-positive/hostile-negative, and dead-before-share proof
are green; saturated state is 51,244 bytes. Exact artifacts live under external
`phase4-20260803/typed-observation/MANIFEST.md`. Production observer/report wiring is checkpointed at
`e7c3da73e7`: the bounded
structural threat read writes strict typed evidence without changing camp intelligence, forward
pair evidence is shared only by the unresolved pair, and hard mobile danger reaches the camp only
when an eligible living observer physically returns. Dead-before-share, local-owner exclusion,
save/replay, below-gate route progress, and both factions are green; exact evidence is under external
`phase4-20260803/observer-writer/MANIFEST.md`. The production visibility envelope is checkpointed at
`1738cf5ca2`: real NPC sight maps through existing CDDA thresholds to clear-day 3 OMT,
intermediate 2, and unlit-night 1 before actual weather, elevation, optics, and terrain cost;
candidate inspection remains current plus three committed route OMTs. The real forest-cost control,
full live-world regression, and clean structured review are archived under external
`phase4-20260803/visibility-envelope/MANIFEST.md`. Acquire/retain hysteresis is checkpointed at
`b7a2333f7f`: normal acquisition keeps the ordinary terrain-cost budget, while the exact same
persisted route OMT and stable threat-ID set may retain through one extra see-cost point for at most
60 minutes. Target-revision drift, moved/replaced threats, expiry, and age 61 all fail closed; the
request-only derivation adds no save bytes. Both-faction save boundaries and full live-world
regression are green under external `phase4-20260803/acquire-retain/MANIFEST.md`. Bounded smoke and
light evidence is checkpointed at `190fab0de5`: the physical source adapter recomputes range and
terrain visibility from the scout, records no player/exact-map-square/defender truth, and commits at
most one uncertain six-hour fact per sense through the typed writer. Both factions, save roundtrip,
atomic malformed rejection, combined signal/visual cursor ownership, and full live-world regression
are green under external `phase4-20260803/smoke-light/MANIFEST.md`. The legacy camp-facing signal
writer remains an explicit later cutover defect. Significant sound evidence is checkpointed at
`1541b351fa`: only explicitly tagged gunfire, alarms, and explosions enter a bounded coarse-OMT
queue and the actual structural observer's hearing, regional weather, route, and three-hour aging
boundaries. Ambient noise and exact source/player identity stay out; focused 4/442 and full
live-world 157/35,638 are green under external
`phase4-20260803/significant-sound/MANIFEST.md`. Honest local-zombie evidence is checkpointed at
`8828bcdbfd`: one shared 64-monster snapshot, exact active-pair NPC LOS and hostility, route-OMT
binding, and ordinary-zombie/rider filtering produce one private 24-hour typed fact without
abstract horde population, avatar sight, exact map squares, or lead mutation. Focused 4/389 and
full live-world 161/36,027 are green under external
`phase4-20260803/local-zombie/MANIFEST.md`. Tracker-order false negatives and snapshot-not-identity
IDs remain explicit bounded caveats. The temporary single-writer cutover is checkpointed at
`dda62833fc`; autonomous discovery and radar removal are checkpointed at `f28450a2a6`, with retired
legacy harness contracts separated at `641ea0884b`. Scheduler-owned maintenance now materializes
only a bounded dispatch finalist, both factions carry typed signal facts home into transactional
`returned_report` leads, and the exact-avatar dispatcher, active-player-OMT matcher, camp-facing
signal writer, comparison mode, and `player@...` signal envelope are deleted. Focused 2/1,698,
scheduler 9/27,248, full live-world 161/37,686, handoff/save 12/331, save-size 1/10, and 63 harness
contract tests are green under external `phase4-20260804/autonomous-discovery/MANIFEST.md`. The
500-site seam creates only two materialization attempts/six members/two starts and none on replay,
cooldown, or no-candidate paths. Natural NPC insertion remains compile-path rather than live proof;
the quiet former-radar control is green at `f80c33996b` for both factions with 1/80 focused and
3/1,778 adjacent autonomous assertions. It deliberately isolates the deleted proximity path with
zero terrain-scan budget; legitimate static terrain priors remain allowed. Avatar relocation and
single-writer stability are green at `531f626c6c`: a returned lead stays byte-identical at the old
OMT while the real avatar moves between two former-radar positions. Focused 1/113, combined no-radar
2/193, adjacent autonomous 4/1,891, and full live-world 163/37,879 are green. Decoy/empty signal
honesty is green at `0e8c531d95`: returned smoke/light/sound leads are bounded routine candidates,
expired clues cannot dispatch, and a production scheduler lane admits at most eight earliest-expiry
signals inside the existing 16-considered/8-route/2-start budgets without skipping the normal
cursor. Arrival with no matching typed evidence records an empty investigation without clearing the
routine no-candidate streak; matching private/shared evidence prevents false emptiness and refreshes
the camp lead only after physical return. Generic terrain matching and the exported site planner
reject these signal-only leads. Focused 2/619, scheduler 11/27,867, full live-world 165/38,502,
handoff/save 12/331, and save-size 1/10 are green. Local communication and dead-scout evidence
control are green at `429385ec26`: a materialized pair shares observer-private typed facts only
inside its exact six-tile cohesion boundary, stale/malformed promotion is atomic, and a private
fact dies with a separated observer while a previously shared fact can return through the living
partner. Structural death writeback now belongs to the shared cleanup boundary; true off-route
death tiles persist without occupying living resume geometry, and same-ID lead replacement at a
different OMT cannot inherit the report. Focused 3/316, combined 15/1,453, full live-world
168/38,818, handoff/save 12/331, and save-size 1/10 are green under external
`phase4-20260804/local-communication-control/MANIFEST.md`. Bounded evidence diagnostics are green
at `4cbd85c57e`: the hourly production path renders last-known OMT, writer/source/observer
provenance, signed age, and effective expiry for bounded rotating site/lead/observation windows.
Returned sound and smoke/light leads use their real three-hour/six-hour decision horizons; unsafe
tokens cannot inject lines, and completed site sweeps advance independent inner windows so capped
entries cannot starve. The pure renderer is byte-read-only and adds no persisted debug state.
Focused 1/34, adjacent Phase-4 8/739, full live-world 169/38,852, handoff/save 12/331, and save-size
1/10 are green after clean read-only review under external
`phase4-20260804/evidence-debug/MANIFEST.md`. The first live no-radar slice is green at
`5cfcf94e90`: a clean five-member bandit camp starts with zero evidence while the real player is six
OMT away, then remains idle across three live hours. The saved result contains exactly three
bounded terrain priors, all `structural_routine`, with no player/radar/observer/signal/
returned-report writer, active target, dispatch, or handoff. Harness contracts pass 71/71 and the
corrected run is 9/9 green under external `phase4-20260804/quiet-live-no-radar/MANIFEST.md`. This
field-footing proof does not claim exact evac-shelter terrain, cannibal parity, or the remaining
visibility/signal matrix. The autonomous exact-pair handoff prerequisite is green at behavior
checkpoint `69fc2a6ceb` and final probe checkpoint `bfabeed571`: corrected run `20260804_123313`
dispatches two members from the full deterministic road lead, binds epoch-1 local ownership, and
saves the same pair with a shared route, distinct staging, valid owner cursor, and no same-run
dematerialization. The feature ledger is 11/11 green, focused C++ coverage passes 332 assertions,
and harness contracts pass 78/78 under external
`phase4-20260804/autonomous-pair-handoff/MANIFEST.md`. Ownership preflight is atomic across the
site. Ordinary hostility can move an assembled member and require later rendezvous reacquisition;
the live visibility matrix is green at runtime checkpoint `98707f2da0` and final contract checkpoint
`8afe569474`. Five 9/9 feature-path runs prove clear twilight/day/cloudy-neutral night road budgets
of 2/3/1, clear optical forest acquisition at 6, and fog-penalized optical forest non-acquisition at
3. The forest pair defers only site-local terrain discovery beyond the experiment, so it proves
production dispatch/route/weather/optic/visibility behavior without claiming competing-target
preference. Evidence is archived under external
`phase4-20260804/structural-visibility-matrix/MANIFEST.md`. Physical home return is checkpointed at
`92aadee446d9`: once an assembled structural pair enters a homeward-only phase, staging releases
motor ownership, concrete survivors keep travelling under local ownership across load/unload, an
early arrival waits inside its source footprint, and only a complete pair or recorded casualty plus
survivor at camp can commit abstract return. Time-based completion cannot outrun concrete NPC
location. The exact Mac compile is green, focused handoff coverage passes 1/440, the broader
structural tag passes 51/6,574, and final closeout review is clean. Harness run `20260804_214456`
remains honest pre-fix red/inconclusive evidence; it is not recredited or retried by ritual.

Danger-scored physical egress is checkpointed at `0658697276e2`. A burned pair evaluates at most
eight outward adjacent OMTs through pair-owned target-excluding routes, prefers a safe pool over
hard danger, and then minimizes legitimate route-wide danger, concealment loss, cost, and stable
coordinates. Actor-visible fields/traps may force the least-dangerous non-inward step before combat;
all generated local/overmap routes keep the target inner ring excluded. The chosen egress survives
dematerialization/rematerialization, post-rally casualties keep the same owner cleanup, and terminal
no-route or six-hour persistent immobility closes through physical report/cargo/casualty writeback
plus the canonical structural return receipt. Mac build, focused tests, release tiles link, and the
fresh closeout review are green. Bounded failed-egress memory is checkpointed at `0dc85f5bc9`:
three cursor-revisioned attempts remember endpoints and route footprints across save/load and owner
handoff; exhausted survivors receive preflighted home routes whose exclusions remain active until
the outing closes. Only failed home routing may use stranded-return closure. Final Mac focused
tests, release tiles link, and clean autoreview are green. No astyle 3.1, live GUI, Linux, or Windows
runtime is claimed. Full bounded egress completion is checkpointed at `b9f29496bd23`: every
survivor must reach the persisted rally and receive an explicit clear ordinary-vision acquire read;
unknown in-bounds actors hold, visible rallies extend strictly outward for at most three attempts,
and exhaustion installs real home routes without re-entry. Concrete routes are nondecreasing in
target distance from each survivor's physical start, legacy schema-10 chains load without gaining
new retries, and endpoint ownership releases cleanly for camp return/dematerialization. Fixed-seed
focused tests, Mac test build, release tiles link after a targeted generated-PCH refresh, and final
autoreview are green. The persisted route-commitment/hysteresis row is green at `42bfc59dcd`:
one cross-layer contract proves unknown visibility is byte-inert, legitimate contact updates retain
the burned phase and exact retry commitment, local-to-abstract-to-local handoff preserves it, and
clear completion plus later contact can advance but never regress to observation. No new persistence
field was needed. The rebuilt Mac test target, fixed-seed burn 2/391, one-way phase 1/187, and diff
check pass. The credited observer-backed live row remains open after two non-credit startup-only
runs (`20260806_100318`, `20260806_100839`) exhausted its attempt cap: both stopped before gameplay
on legacy fields emitted by the current-schema roster transform. Producer footing is checkpointed at
`5f4d091250`; schema-aware population and canonical-outing repairs are `685307349e` and
`38a130e146`. Do not run a third probe now. Per the debug-proof finish rule, retain that prepared
playtest footing and advance to the next deterministic boundary slice: preserve burned physical
egress across one real abstract-resume maintenance tick and subsequent rematerialization. That
boundary is green at `2b721e3f2e`: burned, returning-exposed, and returning-report snapshots retain
the physical egress owner, while ordinary returning-home canonical receipts still hand back to
abstract travel. The cross-layer test round-trips immediately before/after burn, dematerializes on
the last local tick, runs real structural maintenance, reloads, and rematerializes with the same
egress/retry state and one burn fact. Mac build, burn 2/426, handoff 1/651, diff check, and final
autoreview pass. Next: isolate the partial-bubble/only-one-loaded-member boundary without weakening
the exact-pair owner. That boundary is green as a test-only contract at `86c8c2fb3b`: one absent
production-shaped member read rejects byte-inert without inferred visibility, casualty, movement,
route, retry, or burn state; the same pair burns exactly once after the complete read becomes
available. The rebuilt exact case passes 449 assertions and `[covert_burn]` passes 2/453. Next:
repeat the burned local/abstract/load cycle and prove the same owner survives more than one bubble
transition. Repeated cycling is green at `6217823710`: after the real cohesion step, a second
dematerialization, abstract-maintenance tick, reload, and rematerialization preserve the exact
egress/retry owner and one burn fact. Mac exact burn passes 1/474 and the tag passes 2/478. Next:
cover authoritative member and leader death during a burned handoff, reusing normal casualty
writeback rather than erasing the outing.

Josef's comfort-first ecology observer/editor directive is now the canonical prerequisite before
the remaining Phase-4 live matrix. The ratified roadmap and success-state packet is
`doc/ecology-observer-editor-roadmap-and-success-state-v0-2026-08-05.md`. The first row is one
`DEBUG_CLAIRVOYANCE`-gated, side-effect-free camp/dispatch view shared by overmap UI and compact
harness artifacts; horde/stalker adapters, watches/incident capture, and the first authoritative
casualty intervention follow in that order. Smoke/light/sound is the first live row that must use
the observer and preserve a screenshot/snapshot pair. The prepared O0 handoff is green: one derived
fixture preserves the inherited `DEBUG_CLOAK`, idempotently adds only `DEBUG_CLAIRVOYANCE`, reports
before/after/already-present state, and exposes one launch-only manual command. The system-Python
fixture contract passes 125 tests and the dry-run is plan-only; overlay/save-neutrality claims remain
open for the shared-view/UI rows. The O1 shared camp/dispatch query is also green at the current
checkpoint: it reads only the authoritative camp-style/cannibal world owner, includes genuinely
unmaterialized camps, removes terminal/survivor-less dispatches, distinguishes abstract/local and
loaded state, and retains deterministic candidates in `O(N log 2048)` work before the 256-marker
cap. Selected-only route/reason/deadline/member detail, z-level/co-location, exact save round-trip,
and gate-closed byte neutrality pass 5 focused cases / 61 assertions. AutoReview found and closed
five concrete adapter/cap defects; the final rerun is clean. O2 code is now green at checkpoint
`a7b844d100`: the normal overmap renders the same projection in curses and tiles, with stable
markers, legend, category/faction/loaded filters, cursor details, selection, and pin/follow under
the single `DEBUG_CLAIRVOYANCE` gate. Process-local controls never enter the save; the selected
monitor uses a bounded authority-index lookup, while a deliberate Trace-tab export uses the exact
active filters and deterministic compact JSON. Fast travel and closed/specialized overmaps do no
observer query work. Exact Mac tiles compilation passes, `[ecology_debug]` passes 20/176,
`[debug_console]` passes 9/1,116, keybindings parse, and final AutoReview is clean. O2 live proof is
paired in run `20260805_055304`: the current binary visibly selects natural abstract bandit camp
`BC-E75C82` at `(140,51,0)`, and the exact 1,893-byte JSON reports the same ID/state with 1
considered/1 emitted and no truncation. The launch-only detached process exited without diagnostics,
so the pair comes from one direct launch of its exact reported command. Two meaningfully different
O0 save-byte probes are preserved in the same run: the first was invalidated by first-save rewrite
and debug-console contamination; a matched gate-off/gate-on overmap navigation still produced
serializer/lazy-overmap variance after normalizing the intentional mutation. The row remains open
without another canonicalizer retry and is deferred to O4's existing performance/save-growth
harness. O3a is checkpointed at `a4e4cb5c22`: snapshot schema 2 now has one opt-in, bounded,
fail-closed mobile-owner contract with distinct horde/stalker symbols and type-specific selected
details. Exact Mac link plus `[ecology_debug]` 23/232 and `[debug_console]` 9/1,116 are green; one
review/fix pass closed absent-source and canonical-ID collision defects. Neither current C-AOL nor
fetched upstream `7f6b236556` has a durable horde/group/monster identity that survives movement and
concrete/abstract transfer, so production mobile adapters remain disabled instead of inventing
position IDs. O4a is checkpointed at `759e0851bd`: one immutable selected token feeds a 128-record
transition-only ring through the existing monitor and Step/Play controller, with exact provenance,
pause/capture disposition, stale control revision, and fail-closed anomaly handling. Exact Mac
delta 3/334, capture 9/34, console 9/1,116, and full ecology 26/566 are green after one review/fix
pass. Live preflight `20260805_074635` selected natural camp `BC-E75C82` but stopped before watch
arming because the automation bridge focused SDL helper window `22148` rather than render window
`22114`; permissions were green, so this is a non-credit focus-routing harness limitation. O4b
incident capture is checkpointed at `541932daa5`: one action revalidates the exact live owner,
serializes the selected projection plus retained deltas and bounded intervention ledger, and stages
an exact-byte JSON/existing-game screenshot pair into the harness run directory with JSON published
last. Incident 5/71, console 9/1,116, harness 126/126, current Mac tiles runtime, and one combined
review/fix pass are green. E1 code is checkpointed at `1081f6f6a0`: `I` on the selected overmap
entity opens one inspector, while mutation is limited to a loaded structural local-handoff member
whose entity/cursor/NPC/location/HP guard still matches after confirmation. Wound/heal use the
concrete NPC; kill reuses `npc::die` plus normal cleanup/casualty writeback; the bounded process-local
receipt feeds same-turn overlay, monitor trace, and incident provenance. Exact Mac release tests
pass intervention 2/60, all ecology 33/697, and console 9/1,116; the release tiles rebuild/link is
green. The helper-based review was unavailable, so one bounded manual review/fix pass closed delayed
natural-provenance mixing and missing failed-attempt receipts. At that checkpoint, live field use,
the casualty outcome matrix, and O0 neutrality proof were still open. Field attempt
`20260805_091051` selected the natural camp and proved permissions/keyboard console entry, but the
console's second global-debug toggle
again blocked watch arm. Adapter checkpoint `15e01c1e64` now exposes only ecology snapshot/watch /
incident plus Step/Play under `DEBUG_CLAIRVOYANCE`, with `A/P/./R` shortcuts and no global-debug
side effect; exact Mac console 10/1,120 and release tiles link are green. The attempt receipt is
non-credit. Field bridge checkpoint `22004574a1` adds `I` to queue the same selected-overmap editor
outside the ImGui frame and return to the same armed watch. Its captured natural-producer fixture
starts from a real schema-8 two-member local structural handoff, adds only `DEBUG_CLAIRVOYANCE`,
and preserves the source run's `yellow_step_local_proof_incomplete` whole-probe caveat. Exact Mac
tiles/non-tiles compile, tiles link, console 10/1,120, intervention 2/60, all ecology 33/697,
harness 126/126, byte-identical fixture hash, dry-run plan, and fresh no-findings review are green.
O3 adapters resume only after an authoritative identity seam exists. The field gate is now green
on exact runtime `648a509cc9`,
run `20260805_101713`: selected local dispatch `BD-374153`, armed the existing watch, stepped one
turn, confirmed authoritative kill of NPC 4, retained NPC 5 alive, captured natural `appeared` then
`debug_intervention` `hp_changed`, published the exact 4,099-byte incident JSON/PNG pair, and
immediately redrew the selected overmap marker with debug provenance. Field attempts `095314` and
`100813` remain honest non-credit evidence that isolated an inverted SDL3_image `IMG_SavePNG`
success check; fix `648a509cc9` normalizes SDL2/SDL3 contracts, while `f997bbd368` keeps capture/file
I/O outside the active ImGui callback. Query evidence is 2 considered/2 shown/26 us, trace is 2,047
bytes with no truncation, permissions are green, and the exact screenshot/payload hashes live in
`ecology_field_gate_receipt.json`. Bounded watch/run-until behavior is checkpointed at
`13cbeeb072`: exactly six typed predicates, capture/pause/fail, a default six-hour deadline,
one-action existing-controller play, terminal/fatal latching, typed evidence deltas, and schema-2
incident watch state. Exact Mac watch 6/63, incident 6/80, ecology 40/792, console 10/1,120,
tiles/non-tiles compile, tiles link, and the bounded review/fix/recheck are green. O4 performance /
save neutrality is checkpointed at `117857f551`: the shared gate returns zero candidates,
callbacks, and measured query work across 1,000 disabled calls; 1,000 enabled 100-camp marker
queries max at 88 us on this Mac; real save/menu-load runs keep their normalized authoritative
ecology bytes identical and stay inside the ratified Phase-0 save/load/growth envelope. Final raw
and summary artifacts are SHA-256 `49836c50...` and `a04c0e52...`. The isolated save worlds have
different RNG seeds, so this is not a claim of paired whole-directory byte identity or foreign-
platform timing. E1 outcome reconciliation is checkpointed at `1e6a0924e7`: the shared structural
owner now proves both factions across one-dead/one-survivor, both-confirmed-dead, and wounded-pair
returns through save/reload, roster cleanup, wound retention, and 72-78 hour casualty cooldown.
Confirmed physical deaths close promptly; merely missing members retain their deadline. Exact Mac
local-handoff 1/646, structural-bounty 51/6,772, intervention 2/60, tiles/non-tiles compile, and
fresh review are green. This is owner reconciliation proof; the earlier field run supplies GUI /
`npc::die` evidence. Observer-backed smoke/light/sound scenario support is checkpointed at
`927251fc70`, but neither bounded live attempt earns the row. Run `20260805_121516` completed the
three-fact and physical-return path far enough to log the atomic three-lead return, but `O` opened
the profile's Mutations menu instead of the overmap and the intentionally capped diagnostic hid
two exact lead rows before the authoritative save audits. The contract now uses the real `m`
binding and leaves exact smoke/light/sound persistence to the existing post-save owner audits.
Run `20260805_122335` then completed its exact five-minute wait, but the interruption handler
mistook ordinary wilderness "being watched" flavor for a modal prompt, sent Space, created a real
Unknown-command popup, and safely aborted before signal setup. Per the two-attempt cap, do not tune
OCR or rerun this row now. It remains open and non-credit; target relocation was the next
unblocked named row and used the corrected observer path.
Observer-backed target relocation is green at scenario checkpoints `029363748c` / `9029d4e1a4`
and run `20260805_124207`. The two game-authored incident pairs move only the player from
`(135,51,0)` to `(135,63,0)` while natural dispatch `BD-374153` keeps its canonical ID,
`(137,51,0)` position, `(136,51,0)` destination, route, generation, phase, member IDs `4/5`, and
full health. Loaded presentation honestly changes true to false; both ecology intervention ledgers
remain empty. Final save/writeback retains the exact generation-1 camp-owned road target and pair.
The generic probe remains `yellow_step_local_proof_incomplete` because it does not promote
unparsed key/capture steps; the bounded coordinator receipt credits only the paired incidents and
green saved-owner audit. The first attempt `20260805_123855` is non-credit and isolated the missing
watch arm plus noisy save-prompt OCR. Claim limits: explicit debug teleport, a captured producer
with yellow whole-probe history, one bandit pair, Mac only. Live decoy/empty lead control followed.
The decoy scenario support is now checkpointed through `5cffecb404`. Its first live run
`20260805_125925` proved the untouched legacy base only initialized its quiet routine clock; the
second, `20260805_130217`, failed closed because scheduler state cannot be inserted into a pre-v12
site. Neither earns gameplay credit. The replacement fixture is a captured schema-12 world from
credited quiet run `20260804_103631`; its derived transform explicitly removes three natural
terrain priors, adds one zero-bounty/zero-threat returned smoke clue, stages only eligible routine
clocks, and adds `DEBUG_CLAIRVOYANCE`. Exact 2/2 contracts, the full 130/130 fixture suite, dry-run,
and an isolated install are green. The live decoy row remains open under its two-attempt cap; do
not rerun it now. Evidence aging/pruning is checkpointed at `ed47145504`: exact three-hour sound,
six-hour smoke/light, and 30-day unreferenced terminal pruning share one bounded 64-lead scan,
while active references are protected. The accepted six-case Mac packet applies one 730-day jump,
shrinks 6,400 leads to 2,400 at 100 camps in 608 us, and round-trips the aged 10-camp owner through
the real save/load path without changing its authoritative bytes. This specialized saturated burst
is credited against the 20 ms cadence-avalanche gate, not the tighter legacy maintenance
microbenchmark ceilings. A fresh read-only exit review found no implementation defect and closes
the no-player-dispatcher-read and single-writer rows. Phase 4 remains open because the capped
smoke/light/sound and decoy attempts are non-credit; retain them for Josef's later disposable
playtest packet without rerunning them now. Phase 5 watch distance now has an explicit same-z
Chebyshev metric from the nearest actual target-footprint OMT, including diagonal and multi-cell
coverage, without avatar or arbitrary-anchor input. A pure exact-ring selector now requires caller-
verified reachability, concealment, a clear two-OMT approach, and nonnegative route cost, then picks
by route cost and stable OMT order. If exact distance 3 is unavailable, a bounded selector now
chooses distance 4 before 5, then route cost/stable OMT, or returns a typed abandon outcome; closer
and more remote candidates never qualify. These began as evaluator footing only. A direct route-read
attempt was backed out cleanly after apply-time replay proved that ephemeral watch metadata cannot
satisfy the existing canonical route/save contract. Structural schema 9 is checkpointed at
`dfb19de3aa`: the existing outing owner now persists a canonical 64-OMT target footprint and one
immutable exact/fallback watch selection from at most 256 candidates, migrates schema 8 to an
unselected singleton footprint, and rejects stale/conflicting/malformed replay atomically. Focused
Mac persistence 1/71, full structural 51/6,780, handoff 5/237, and final AutoReview are green.
Production adapter checkpoint `ef962e6e88` now derives the full verified basecamp footprint or a
singleton structural target, reads real OMT concealment/approach terrain, searches NPC routes around
the target footprint, and commits exact/fallback selection through schema 9 or abandons. Work is
bounded to 256 terrain reads per routed contender and eight watch path solves per maintenance pass;
source-aware shortlisting avoids one-sided coordinate sampling. Exact Mac object/test link,
adapter 1/50, and structural 51/6,780 are green after review found and fixed five concrete geography
and budget defects. Physical watch travel is checkpointed at `d4e6579aed`: schema 10 replaces the
target-facing radial route with one symmetric home/approach/watch/approach/home route from the same
authoritative path solve, keeps schema-9 saves on their old route, and holds the pair observing at
the selected watch without consuming the remote lead. Exact Mac compile/link, adapter 1/72,
structural 51/6,780, persistence 1/71, and handoff 14/343 are green. Exact and fallback selection
now share this route consumer. Watch staging checkpoint `62e26812d6` reuses the local-handoff owner
to choose distinct target-facing observer/cover tiles inside the watch OMT, bounds both living entry
and staging pairs by the existing six-tile cohesion radius, and preserves the slots through reload
and assembly without changing homeward staging. Exact Mac build/link, adapter 1/91, handoff 1/651,
watch geometry 1/15, structural 51/6,785, and local-zombie 4/389 are green. Covert-disposition
checkpoint `c23e817132` derives a neutral relationship only for each exact living schema-10 scout
against the exact player-camp target, keeps unrelated faction hostility intact, and clears only after
the whole surviving pair is routing home/already home, outside the footprint, and outside ordinary
loaded reciprocal visibility. Player/allied attacks, successful hostile spell effects, actual blast
propagation, and avatar-attributed manual turrets exit through authoritative hostility; selection,
jams, failed spells, and automatic synthetic turrets do not. Exact Mac build/link plus covert 2/97,
NPC-AI 3/69, magic 11/189, turret 1/2,328, and local-handoff 1/651 are green; final autoreview is
clean. Burn checkpoint `339aa54c4d` now requires reciprocal ordinary sight with debug clairvoyance
excluded, preflights both routes, commits one typed burn fact plus outing/handoff transition before
generic movement, and routes the whole surviving party toward the persisted egress/rally. Dead and
deadline-missing members reconcile through the same structural owner before the survivor gate, so
loss cannot wedge exfil. Exact Mac build/link plus burn 2/68, covert 2/97, NPC-AI 3/69,
local-handoff 1/651, and structural 51/6,785 are green; final autoreview is clean. No astyle, live
GUI, Linux, or Windows proof is claimed. Next is reachable exit scoring against observed threats and
concealment; the current persisted return approach is deliberately not claimed as that scorer.
The compound human-camp opportunity-recovery
row remains sequenced after typed observations and later physical-report/outcome semantics. Terrain
labels remain static priors, not perception of loot, residents, or mobile danger.
Private per-camp resource estimates are checkpointed at `1aa9851902`; physical estimate
updates are timestamped/confidence-bearing and neither global claims nor another camp mutate them.
Bounded supply remains checkpointed at `37498066ba`, and world-global resources at `432c0f9da7`.
Finite physical arrival is checkpointed at `dfddc712d4`: full pairs contest a three-unit OMT as
`2+1`, a physically recorded casualty produces `1+2`, each harvested unit carries two supply units,
and home credit/resource depletion/private belief/save-load/replay share existing exact receipts.
Post-close
physical arrival by a member already declared missing is not yet claimed; that later bounded
receipt belongs with detailed outcome semantics. The repaired Mac
Keychain/API path remains a Phase-10 release-harness gate; do not retry or pause deterministic work
for `OSStatus -25308`.

The prior five-family Windows free-play handoff remains useful held observation footing. It is not
the active implementation target and does not authorize work on the three excluded creature
families.

### Next decision - CAOL-CDDA-UPSTREAM-REFRESH-2026-08-v0

After hostile-camp engineering qualification and Josef's later product pass, decide whether to
merge `7cf1d08ae8` or a newer stable upstream tip. Current estimate remains
mechanically small-to-moderate and behaviorally moderate; no upstream merge belongs to the active
ecology implementation.

---

## Recent closed lane — CAOL-CI-RED-TRIAGE-v0

**Status:** CLOSED / CHECKPOINTED GREEN / ACTIONS VERIFIED

Contract: `doc/ci-red-triage-packet-v0-2026-05-06.md`.

Initial red evidence was run `25371458600` on `5043f2c32c` (`General build matrix`, title `Retitle Andi handoff for checkpointed camp smoke proof`) with failures across GCC/Clang/Linux/macOS/CMake jobs. Seed clusters included `faction_camp_test` patrol-alarm/current-target failures, `debug_menu_test` missing entry, `flesh_raptor_test` sight setup failure, `item_test` density for `zombie_rider_bone_bow`, `uncraft_test` yield drift, and `zombie_rider_test` mature-gate/direct-entry failures.

Repair stack from `29cb5bbb97` through `cb21294168` fixed the branch-caused CI failures with narrow code/data/test changes: zombie rider test/data fixes, CI-sensitive camp/flesh-raptor/debug-menu test stabilization, NPC zone-sort ASan completion, and the layered-bedroom terrain item allowance.

Verification: `General build matrix` run `25462728843` on `cb21294168` completed success across all matrix jobs, and `Cataclysm Windows build` run `25462728845` completed success.

---

## Recent checkpointed lane — CAOL-DEFENDED-CAMP-SIGHT-SMOKE-HARDENING-v0

**Status:** CHECKPOINTED GREEN / AGENT-SIDE PROOF COMPLETE / AWAITING FRAU REVIEW

Schani promoted the next bounded Andi target from the deferred rows of `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`: prove and harden defended-camp sight/smoke behavior for bandit watchers and compatible cannibal stalking profiles.

Contract: `doc/defended-camp-sight-smoke-hardening-packet-v0-2026-05-05.md`.

Imagination source: `doc/josef-live-debug-batch-imagination-source-2026-05-03.md`.

Parent matrix: `doc/josef-live-debug-batch-test-matrix-v0-2026-05-03.md`.

Scope: when player or camp NPCs can currently see a hostile watcher, the watcher should break LoS, back off, reroute through cover/darkness, escalate/report, or record a concrete blocker instead of visible garden-gnome stalking. Smoke on the watcher tile or sightline should make the lead obscured/uncertain and cause reposition/wait/probe/escalation/blocker behavior, not same-tile smoke camping. Bandits keep bandit outcomes; cannibals keep cannibal outcomes and never gain shakedown UI by drift.

Boundary: one hardening packet. Do not reopen completed shakedown, light adapter, patrol, sorting, debug-spawn, locker, Monsterbone, or full cannibal raid/contact rows unless Schani/Josef promotes them separately.

Recommended next action: Augerl/Frau should review the checkpointed smoke/sighted-watcher proof; if accepted, Schani should close/retitle this packet and promote the next specific greenlit lane. No routine Andi reprobe is pending unless review finds a concrete evidence gap.

---

## Recent checkpointed lane — CAOL-JOSEF-LIVE-DEBUG-BATCH-v0

**Status:** CHECKPOINTED GREEN DEBUG STACK / AWAITING SCHANI-FRAU BOUNDARY REVIEW / NO ACTIVE ANDI PROBE

Josef's 2026-05-02 + 2026-05-03 live-debug batch has reached a clean agent-side checkpoint. All eleven ordered slices have implementation/proof receipts, and the remaining deeper playfeel rows are future promotional hardening rather than current Andi blockers.

Imagination source: `doc/josef-live-debug-batch-imagination-source-2026-05-03.md`.

Contract: `doc/josef-live-debug-batch-packet-v0-2026-05-03.md`.

Handoff packet: `doc/josef-live-debug-batch-handoff-v0-2026-05-03.md`.

Test matrix: `doc/josef-live-debug-batch-test-matrix-v0-2026-05-03.md`.

Proof receipts:
- Slice 1 shakedown Pay/Fight + actual NPC trade UI/payment pool: `doc/shakedown-pay-fight-npc-trade-ui-proof-v0-2026-05-03.md`.
- Slice 2 defended-camp current-pass standoff/hot-loot checkpoint: `doc/defended-camp-scout-standoff-hot-loot-proof-v0-2026-05-03.md`; Josef's live read was good enough to continue. Deeper sight/smoke playfeel remains future hardening if promoted.
- Slice 3 multi-z roof/tower dispatch fallback: `doc/multi-z-roof-dispatch-fallback-proof-v0-2026-05-03.md`.
- Slice 4 hostile-camp toll/escalation checkpoint: `doc/hostile-camp-toll-escalation-proof-v0-2026-05-03.md`.
- Slice 5 all-light-source live adapter: `doc/all-light-source-live-adapter-proof-v0-2026-05-04.md`.
- Slice 6 camp patrol aggression/alarm/report hygiene: `doc/camp-patrol-aggression-alarm-report-hygiene-proof-v0-2026-05-04.md`.
- Slice 7 writhing-stalker distance/sight/threat-drop rhythm: `doc/writhing-stalker-distance-sight-threat-drop-live-proof-v0-2026-05-04.md`.
- Slice 8 NPC sorting debounce: `doc/npc-sorting-failure-debounce-proof-v0-2026-05-04.md`.
- Slice 9 debug horde/stalker/rider spawn options: `doc/debug-spawn-overmap-threat-options-proof-v0-2026-05-04.md`.
- Slice 10 locker/basecamp equipment consistency: `doc/locker-basecamp-equipment-consistency-proof-v0-2026-05-04.md`.
- Slice 11 cannibal Monsterbone spear: `doc/monsterbone-spear-proof-v0-2026-05-04.md`.

Boundary: debug correction stack only. Do not reopen unrelated closed predator/rider/locker/save-pack lanes, release packaging, full diplomacy, full vertical assault AI, broad sorting redesign, tile-perfect overmap light engine, or deferred sight/smoke/live-cannibal playfeel rows without Schani/Josef promotion.

Recommended next action: Schani/Frau boundary review should either close/retitle this lane, promote one specific deferred proof/playfeel row, or hand Andi the next greenlit packet. Until then there is no sensible unblocked Andi execution target in repo canon.

---

## Recent closed lane — CAOL-HARNESS-PORTAL-STORM-WARNING-LIGHT-v0

**Status:** CHECKPOINTED GREEN V0 / CLOSED / FRAU-ACCEPTED / HARNESS-HARDENING FOLLOW-UP

The visions sampler is relay-ready and waiting on Schani/Josef taste relay rather than more agent-side proof. Josef's harness portal-storm warning-light ask is now implemented/proven as a harness-hardening checkpoint.

Contract: `doc/harness-portal-storm-warning-light-packet-v0-2026-05-02.md`.
Proof: `doc/harness-portal-storm-warning-light-proof-v0-2026-05-02.md`.

Result: probe/handoff reports and repeatability summaries now surface a report-level `portal_storm_warning`; unallowed portal storms add a yellow contamination row to the step ledger and block silent green feature proof; explicitly allowed portal-storm scenarios stay green while retaining visible warning text; required portal-storm scenarios fail red if the required weather is missing/unknown.

Evidence: `python3 tools/openclaw_harness/proof_classification_unit_test.py` -> `Ran 13 tests ... OK`; `python3 -m py_compile tools/openclaw_harness/startup_harness.py tools/openclaw_harness/proof_classification_unit_test.py`; `git diff --check`.

Frau review: accepted green v0 from commits `74ef657057` / `8ea5546107`. Do not pull future runs back into portal-storm proof by ritual.

Boundary: harness-hardening only. Do not solve portal-storm gameplay, redesign weather, rerun old packets by ritual, or reopen closed bandit/visions/camp-locker lanes by drift.

---

## Recent closed lane — CAOL-ZOMBIE-RIDER-CLOSE-PRESSURE-NO-ATTACK-v0

**Status:** CLOSED / CHECKPOINTED GREEN V0 / BUGFIX + PRODUCT-FEEL FOLLOW-UP

Proof: `doc/zombie-rider-close-pressure-no-attack-proof-v0-2026-05-02.md`.

Result: the close-pressure no-attack seam is fixed. Root cause was the missing `aggro_character` bridge between `decision=bow_pressure reason=line_of_fire` planning and the monster gun actor's avatar-target gate. Current code marks the rider character-aggro before ready bow handoff and chooses named irregular bunny-hop/reposition pressure when too close, cooling down, blocked, or out of ammo.

Evidence: focused `[zombie_rider]` tests are green after the tainted-arrow follow-up (`207 assertions in 17 test cases`); `./just_build_macos.sh` relinked `cataclysm-tiles` for the original close-pressure checkpoint; fresh staged-but-live row `zombie_rider.live_open_field_pressure_mcw` -> `.userdata/dev-harness/harness_runs/20260502_050055/` remains behavior-shape footing for bow-pressure aggro bridge, ammo decrement, and close `too_close_bunny_hop` reposition. The current source lookup now checks `zombie_rider_tainted_bone_arrow` ammo instead of the pre-follow-up `arrow_wood` id. Caveat: staged-but-live McWilliams proof, not natural random discovery/full siege proof.

---

## Recent closed lane — CAOL-CAMP-LOCKER-ZONE-PLAYTESTS-v0

**Status:** CLOSED / CHECKPOINTED GREEN V0

Josef explicitly asked for locker zone playtests after the camp locker API-reduction lane closed. The bounded agent pass is complete and intentionally did not reopen open-ended locker ontology archaeology.

Proof/readout: `doc/camp-locker-zone-playtest-proof-v0-2026-05-02.md`.

Imagination source: `doc/camp-locker-zone-playtests-imagination-source-2026-05-02.md`.

Contract: `doc/camp-locker-zone-playtest-packet-v0-2026-05-02.md`.

Handoff packet: `doc/camp-locker-zone-playtest-handoff-v0-2026-05-02.md`.

Result: current-build harness feature proof is green after repairing the scenario evidence contract. `locker.zone_manager_save_probe_mcw` (`.userdata/dev-harness/harness_runs/20260502_041828/`) now has a green step-local ledger, matched Zone Manager UI-trace rows for `Basecamp: Locker` and created/reopened `Probe Locker` as `type="CAMP_LOCKER"`, and a saved-zone metadata audit for the persistent `Basecamp: Locker` zone. `locker.weather_wait` (`.userdata/dev-harness/harness_runs/20260502_041300/`) has a green step-local ledger and same-run `camp locker:` queued/plan/after/serviced artifacts proving real service from `locker_tiles=1` stock. `[camp][locker]` deterministic tests prove `NO_NPC_PICKUP`, off-zone, and policy-disabled boundary/exclusion guards. The Robbie e2e row remains blocked/no-credit, but it is not needed for this bounded v0 closure.

Boundary preserved: no API-reduction reopen, no broad locker/basecamp redesign, no Smart Zone/basecamp preset work, and no bandit/rider/stalker/raptor drift.

---

## Recent closed lane — CAOL-WRITHING-STALKER-PATTERN-TESTS-v0

**Status:** CLOSED / CHECKPOINTED GREEN V0

Schani promoted the queued writhing-stalker behavior-pattern follow-up into the next active lane after Josef clarified the desired rhythm: fair dread, not hidden-cheat goblin nonsense; repeated attacks with breathing room while healthy; withdrawal once badly injured.

Contract: `doc/writhing-stalker-behavior-pattern-minimap-packet-v0-2026-04-30.md`.

Imagination source: `doc/writhing-stalker-behavior-pattern-imagination-source-of-truth-2026-04-30.md`.

Proof: `doc/writhing-stalker-behavior-pattern-proof-v0-2026-04-30.md`.

Result: `tests/writhing_stalker_test.cpp` now has a compact deterministic behavior-pattern helper and trace rows covering no-magic/no-evidence, weak evidence decay, cover-route preference, light/focus counterplay, zombie-pressure guardrails, vulnerable-player strike windows, cooldown anti-spam, healthy repeated strikes, badly-injured retreat, and jitter/stuckness smell checks. No gameplay tuning/source behavior changed.

Evidence: `make -j4 tests/writhing_stalker_test.o tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[writhing_stalker][ai]"` passed (`97 assertions in 8 test cases`); `./tests/cata_test "[writhing_stalker]"` passed (`129 assertions in 10 test cases`); trace extraction for `writhing_stalker_pattern_helper_traces_repeated_strikes_then_injured_retreat` passed and shows `shadow -> strike -> cooldown -> cooldown -> shadow -> strike -> withdraw` with retreat at `hp=50`.

Boundary: deterministic pattern proof is closed for v0. The live seam was not rerun because this slice changed tests only; existing live `monster::plan()` receipts remain the unchanged v0 game-path evidence. Do **not** reopen roof-horde, old writhing-stalker v0 closure, Smart Zone, old fire proof lanes, the multi-camp signal gauntlet, or this pattern packet without explicit Schani/Josef promotion.

---

## Recently closed lane — CAOL-WRITHING-STALKER-LIVE-FUN-SCENARIOS-v0

**Status:** CLOSED / CHECKPOINTED GREEN V0 LIVE FUN-SCENARIO PACKET

Josef explicitly greenlit live writhing-stalker fun scenarios on 2026-04-30: “YESSSS DO IT”. This lane follows the closed deterministic `CAOL-WRITHING-STALKER-PATTERN-TESTS-v0` and asks whether the stalker creates fair dread in live-shaped scenes instead of only passing tidy evaluator traces.

Contract: `doc/writhing-stalker-live-fun-scenarios-packet-v0-2026-04-30.md`.

Imagination source: `doc/writhing-stalker-live-fun-scenarios-imagination-source-of-truth-2026-04-30.md`.

Prior deterministic proof: `doc/writhing-stalker-behavior-pattern-proof-v0-2026-04-30.md`.

Closure proof: `doc/writhing-stalker-live-fun-scenario-proof-v0-2026-04-30.md`.

Green v0 result: staged-but-live McWilliams scenarios prove campfire/light counterplay, alley predator shadow routing, zombie distraction without magic, door/light escape, wounded-predator retreat, repeated strike/cooldown rhythm, and no omniscient target acquisition. Credited runs: `writhing_stalker.live_campfire_counterplay_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233129/`; `writhing_stalker.live_alley_predator_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233156/`; `writhing_stalker.live_zombie_distraction_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233521/`; `writhing_stalker.live_zombie_distraction_no_magic_guard_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233335/`; `writhing_stalker.live_door_light_escape_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233405/`; `writhing_stalker.live_wounded_predator_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233434/`.

Boundary: do **not** reopen monster flavor/stat/spawn footing, deterministic pattern closure, roof-horde, Smart Zone, old fire proof lanes, the multi-camp gauntlet, or these live fun rows without explicit Schani/Josef promotion. Remaining stricter fully-natural/manual discovery is future-only, not a blocker for v0.

Prior closed active lane: `CAOL-WRITHING-STALKER-PATTERN-TESTS-v0` — see `doc/writhing-stalker-behavior-pattern-proof-v0-2026-04-30.md`, `doc/work-ledger.md`, `SUCCESS.md`, and `TESTING.md`.

---

## Last closed active lane — CAOL-ZOMBIE-RIDER-0.3-v0

**Status:** CLOSED / CHECKPOINTED GREEN V0 INITIAL DEV

Josef greenlit preparing release `0.3` zombie rider for Andi, including playtests and a small map-AI funness benchmarking suite. The v0 initial-dev packet is now closed green.

Contract: `doc/zombie-rider-0.3-initial-dev-packet-v0-2026-04-30.md`.

Imagination source: `doc/zombie-rider-0.3-imagination-source-of-truth-2026-04-30.md`.

Raw intake / exact flavor text: `doc/zombie-rider-raw-intake-2026-04-30.md`.

Map-AI / funness benchmark suite: `doc/zombie-rider-map-ai-funness-benchmark-suite-v0-2026-04-30.md`.

Closure readout: `doc/zombie-rider-0.3-closure-readout-v0-2026-05-01.md`.

Result: endpoint zombie rider initial dev is green for v0: exact flavor text preserved, mature-world `GROUP_ZOMBIE` gate at `730 days`, large-body `SMALL_PASSAGE` counterplay, live-consumed local bow/withdraw/cover behavior, bounded overmap light interest and memory decay, capped rider convergence/band pressure, and five staged-but-live funness rows covering open-field terror, wounded disengagement, cover/LOS escape, lit-camp band circle-harass, and matched no-camp-light control.

Evidence: monster/evolution footing `d50715f00e`; local combat AI `4343dbdad1`; overmap light attraction `d2ffbd54c3`; convergence/band checkpoint `ce05ef44ec`; live rows `zombie_rider.live_open_field_pressure_mcw` -> `.userdata/dev-harness/harness_runs/20260501_013055/`, `zombie_rider.live_wounded_disengagement_mcw` -> `.userdata/dev-harness/harness_runs/20260501_014613/`, `zombie_rider.live_cover_escape_mcw` -> `.userdata/dev-harness/harness_runs/20260501_021656/`, `zombie_rider.live_camp_light_band_mcw` -> `.userdata/dev-harness/harness_runs/20260501_030432/`, and `zombie_rider.live_no_camp_light_control_mcw` -> `.userdata/dev-harness/harness_runs/20260501_032016/`.

Boundary: staged-but-live v0 does not claim natural random discovery, full siege/navigation behavior, year-one availability, unavoidable camp deletion, infinite rider accumulation, or broader release packaging. Do not reopen this lane, writhing-stalker lanes, bandit/horde proof lanes, Smart Zone, old fire proof lanes, release packaging, or held parked concepts by drift.

---

## Recently closed lane — CAOL-FLESH-RAPTOR-CIRCLING-SKIRMISHER-v0

**Status:** CLOSED / CHECKPOINTED GREEN V0 / PREDATOR VARIETY PACKAGE

Josef greenlit changing predator behavior variety after the zombie-rider lane, starting with flesh raptors as visible circling skirmishers instead of one-note stab-and-flee annoyance.

Contract: `doc/flesh-raptor-circling-skirmisher-packet-v0-2026-05-01.md`.

Imagination source: `doc/predator-behavior-variety-imagination-source-of-truth-2026-05-01.md`.

Result: flesh raptors now use a raptor-only live `monster::plan()` seam for readable 4–6 tile orbit/flank pressure, under-occupied arc preference, bounded swoop cadence, corridor/blocked-terrain fallback, and jitter guardrails without globally rewriting every `HIT_AND_RUN` monster or increasing equipment-destruction tedium.

Evidence: focused `[flesh_raptor]` tests cover variant footing, orbit/flank scoring, under-occupied arc preference, fallback, cadence/hysteresis, live plan consumption for `mon_spawn_raptor`, and non-raptor `HIT_AND_RUN` preservation. Credited staged-but-live rows are `flesh_raptor.live_open_field_skirmisher_mcw` -> `.userdata/dev-harness/harness_runs/20260501_052709/`, `flesh_raptor.live_crowded_arc_skirmisher_mcw` -> `.userdata/dev-harness/harness_runs/20260501_053414/`, `flesh_raptor.live_blocked_corridor_skirmisher_mcw` -> `.userdata/dev-harness/harness_runs/20260501_054807/`, and `flesh_raptor.live_equipment_frustration_comparison_mcw` -> `.userdata/dev-harness/harness_runs/20260501_062300/`. The final comparison row proves current orbit/swoop/melee debug metrics plus screenshot/OCR player-facing `flesh-raptor` / `impales` / `cut` / `bleeding` evidence from message history.

Closure review: Frau accepted v0 for agent-side close with staged-but-live caveats. Josef taste/playtest is optional/future, not a blocker; if Josef later says the raptor is still annoying, that is a taste/tuning follow-up, not proof that v0 failed.

Boundary: staged-but-live McWilliams rows are not natural random discovery. Equipment-damage tuning was not changed; equipment damage remains an observational frustration metric. Do not reopen this lane, remove every attack-and-retreat enemy, redesign eigenspectres, create a global pack-AI rewrite, or tune raptors by simply raising damage/equipment destruction unless Schani/Josef promote a follow-up.

---

## Recently closed lane — CAOL-WRITHING-STALKER-ZOMBIE-SHADOW-PREDATOR-v0

**Status:** CLOSED / CHECKPOINTED GREEN V0 / PREDATOR VARIETY PACKAGE

Josef greenlit changing both flesh raptors and the writhing stalker away from overused stab-and-flee behavior, with deterministic little-map tests, playtests, and fun metrics. Flesh raptor circling is closed green v0, and the stalker zombie-shadow predator shift is now closed green v0 for the scoped local-evidence package.

Shared imagination source: `doc/predator-behavior-variety-imagination-source-of-truth-2026-05-01.md`.

Contract: `doc/writhing-stalker-zombie-shadow-predator-packet-v0-2026-05-01.md`.

Result: the stalker now has explicit confidence components for evidence/interest, zombie pressure, quiet-side/cutoff opportunity, and counterpressure from light/focus/open exposure. The live shadow destination consumes the quiet-side cutoff scorer, so ordinary zombies pressing one side can make the stalker choose a shadow/cutoff tile where the zombies are not, without granting omniscient target acquisition.

Evidence: focused `[writhing_stalker]` tests prove pressure-side scoring, ambiguous split-pressure restraint, retreat-route cutoff bias, pressure gating, overmap-interest helper admission, light/focus suppression, and preservation of old no-omniscience/cooldown/injured-retreat constraints. `writhing_stalker.live_quiet_side_zombie_pressure_mcw` -> `.userdata/dev-harness/harness_runs/20260501_071548/` logs the local-evidence-only path from east/front zombie pressure (`pressure_x=3`) to a west/quiet-side cutoff tile (`quiet_x=-1`, `chosen_rel_x=-1`, `chosen_rel_y=-4`) with `overmap_interest=no`; proof note: `doc/writhing-stalker-zombie-shadow-live-quiet-side-proof-v0-2026-05-01.md`. Later sampler audit found an `ERROR GAME` wall-location backtrace in this row's `probe.artifacts.log`, so it is dirty/caveated for Josef-facing optical footing unless rerun clean. `writhing_stalker.live_escape_side_zombie_retreat_mcw` -> `.userdata/dev-harness/harness_runs/20260501_071940/` proves the cleaner local-evidence-only retreat row from north/front zombie pressure (`pressure_y=-3`) to a south/escape-side cutoff tile (`quiet_y=1`, `chosen_rel_y=4`) with `overmap_interest=no`; proof note: `doc/writhing-stalker-zombie-shadow-live-escape-side-proof-v0-2026-05-01.md`.

Closure caveat: v0 proves local-evidence zombie-shadow behavior through staged-but-live McWilliams rows. Live overmap-interest wiring/logging remains unclaimed unless a later packet promotes it; Josef taste/playtest is optional future feedback, not a v0 blocker.

Boundary: do not remove all attack-and-retreat enemies, do not fold eigenspectres into this package, do not make zombie proximity a magical target buff, and do not give the stalker omniscient backstab magic. The anti-redundancy refactor must preserve this approved zombie-shadow behavior shape.

---

## Closed recent lane — CAOL-WRITHING-STALKER-BEHAVIOR-SEAM-REDUCTION-v0

**Status:** CLOSED / CHECKPOINTED GREEN V0 / ANTI-REDUNDANCY PACKAGE

Josef greenlit targeted anti-redundancy packages after Schani's `master...dev` redundancy audit. The first cleanup package was the writhing-stalker behavior seam reduction, preserving the closed green zombie-shadow predator behavior.

Imagination source: `doc/anti-redundancy-packaging-imagination-source-of-truth-2026-05-01.md`.

Contract: `doc/writhing-stalker-behavior-seam-reduction-packet-v0-2026-05-01.md`.

Closure checkpoint: seam inventory is recorded in the contract doc, the live planner seam-consumption test proves `monster::plan()` reaches quiet-side cutoff behavior, and `src/monmove.cpp` now routes writhing stalker / zombie rider / flesh raptor target-response planning through a single named `targeted_live_plan_adapter` dispatch before the generic hostile/flee destination fallback. No behavior-tree or special-attack seam honestly owns this destination-planning response today; product-specific stalker judgment remains explicit in the writhing-stalker evaluator and quiet-side scorer.

Evidence: `git diff --check`; `make -j4 tests/writhing_stalker_test.o tests src/writhing_stalker_ai.o LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[writhing_stalker]"` -> `All tests passed (192 assertions in 15 test cases)`; adapter spillover guard `./tests/cata_test "[zombie_rider],[flesh_raptor]"` -> `All tests passed (231 assertions in 21 test cases)`.

Boundary preserved: this was cleanup/refactor, not tuning. Closed writhing-stalker v0, zombie-shadow proof rows, bandit/horde/camp claims, flesh raptors, and eigenspectres remain closed unless explicitly promoted.

---

## Closed recent lane — CAOL-CAMP-LOCKER-EQUIPMENT-API-REDUCTION-v0

**Status:** CLOSED / CHECKPOINTED GREEN V0 / ANTI-REDUNDANCY PACKAGE

Shared imagination source: `doc/anti-redundancy-packaging-imagination-source-of-truth-2026-05-01.md`.

Contract: `doc/camp-locker-equipment-api-reduction-packet-v0-2026-05-01.md`.

Closure checkpoint: the camp locker API-reduction package is closed at the current scoped v0 boundary. The implementation now keeps camp policy explicit while deferring the audited item, wearability, body coverage/layer, reload/ammo, scoring, carried-cleanup, live-state, medical-readiness, and zone-boundary truths to existing engine APIs where an honest equivalent exists. The package deliberately stops here: remaining camp-specific choices such as enabled slots, bulletproof/weather-sensitive preference, kept-supply policy, and camp-storage boundaries are policy, not accidental duplicate ontology.

Evidence: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[camp][locker]"` -> green at closure. Last code checkpoint was `6a0f003dfe Reduce camp locker armor resistance scoring`.

Boundary preserved: cleanup/refactor only. This does not redesign basecamp missions, Smart Zones, general NPC equipment selection, outfit tuning, or any bandit/rider/stalker feature. Further camp-locker work requires a newly promoted follow-up with a concrete seam, not more open-ended ontology archaeology.

---


## Closed recent lane — CAOL-BANDIT-SIGNAL-ADAPTER-REDUCTION-v0

**Status:** CLOSED / CHECKPOINTED GREEN V0 / ANTI-REDUNDANCY PACKAGE

Shared imagination source: `doc/anti-redundancy-packaging-imagination-source-of-truth-2026-05-01.md`.

Contract: `doc/bandit-signal-adapter-reduction-packet-v0-2026-05-01.md`.

Closure proof: `doc/bandit-signal-adapter-reduction-proof-v0-2026-05-02.md`.

Closure checkpoint: the live `fd_fire` / `fd_smoke` observation path now builds a `bandit_mark_generation::local_field_signal_reading` and routes through `adapt_local_field_signal_reading()` before producing smoke/light projections. Basic field/time/weather/exposure mapping is now adapter-shaped; bandit mark scoring, site memory, live dispatch, and horde tuning remain custom/unchanged.

Evidence: `git diff --check`; `make -j4 tests/bandit_mark_generation_test.o src/bandit_mark_generation.o obj/do_turn.o LINTJSON=0 ASTYLE=0`; standalone adapter probe linked against `src/bandit_mark_generation.o` / `obj/bandit_dry_run.o`; `make -j1 tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[bandit][marks]"` -> `All tests passed (236 assertions in 18 test cases)`.

Boundary preserved: cleanup/refactor only. This does not redesign bandit live-world dispatch, roster state, structural bounty, camp-map memory, signal tuning/ranges, horde behavior, or a generic overmap event bus. Existing roof-fire/live-signal expectations remain classified as preserved behavior, not newly tuned behavior.

---

## Recent closed lane — CAOL-WRITHING-STALKER-THREAT-DISTRACTION-HANDOFF-v0

**Status:** CLOSED / CHECKPOINTED GREEN V0 / FRAU-ACCEPTED

Josef live-tested the writhing stalker after the earlier hit-fade and zombie-shadow packets and reported that it is still not satisfactory. The bad contrast is now explicit: with three NPC allies / high visible threat it does not retreat into stalking distance, and at night outside it can stand near a house/window without attacking or making a legible move.

Imagination source: `doc/writhing-stalker-threat-distraction-handoff-imagination-source-2026-05-02.md`.

Contract: `doc/writhing-stalker-threat-distraction-handoff-packet-v0-2026-05-02.md`.

Handoff packet: `doc/writhing-stalker-threat-distraction-handoff-handoff-v0-2026-05-02.md`.

Deterministic checkpoint: `doc/writhing-stalker-threat-distraction-deterministic-checkpoint-v0-2026-05-03.md`.

Live/staged proof: `doc/writhing-stalker-threat-distraction-live-staged-proof-v0-2026-05-03.md`.

Raw live-watch note: `/Users/josefhorvath/.openclaw/workspace/runtime/caol-live-watch-20260502/writhing-stalker-live-watch-notes-2026-05-02.md`.

Goal: add/refine stalker overmap threat/opportunity evaluation, reality-bubble anti-loiter behavior, and overmap/bubble handoff memory so high-threat daylight/three-NPC situations retreat into stalking mode about `3` OMTs back, night/outside reachable-player situations attack or reposition instead of garden-gnome loitering, and zombie/distraction entering the player/NPC tile enables dark-square approach/strike without omniscience.

Current checkpoint: deterministic evaluator/live-plan seam coverage is green for threat retreat, stalking-distance intent, dark reachable anti-loiter, zombie-distraction/no-omniscience, handoff/writeback, and existing stalker guarantees. Current-build live/staged proof is green for all three remaining rows: high-threat/allies retreat/stalk (`writhing_stalker.live_high_threat_allied_light_retreat_stalk_mcw` -> `.userdata/dev-harness/harness_runs/20260503_021310/`), zombie/distraction shadow-then-strike (`writhing_stalker.live_zombie_distraction_mcw` -> `.userdata/dev-harness/harness_runs/20260503_031247/`), and night/outside reachable bad-loiter anti-gnome strike (`writhing_stalker.live_anti_gnome_bad_loiter_mcw` -> `.userdata/dev-harness/harness_runs/20260503_025712/`). Frau accepted this as closure-ready agent-side staged/live feature-path evidence; door opening did not land and remains out of scope unless separately promoted.

Door-opening line: allowed only as a narrow optional escalation if needed — unlocked/simple doors, slow/noisy/interruptible, darkness/distraction/commitment gated, and suppressed under high threat. Do not turn the stalker into a burglar or locked-door solver.

Frau review note: safe claims are deterministic seam coverage plus current-build staged/live behavior rows; do not claim natural random discovery, full natural retreat pathing, broad house navigation, door opening, burglar/locked-door solving, or general ecosystem behavior.

Boundary: closed v0 packet. Do not reopen all stalker v0 work, flesh raptors, zombie riders, bandits, the save-pack card, or natural random discovery by drift.

---

## Handoff boundary — CAOL-JOSEF-PLAYTEST-SAVE-PACK-v0

**Status:** WAITING FOR NEXT GREENLIGHT / JOSEF HANDOFF CARD READY / OPTIONAL BANDIT CONTRAST READY / NOT CURRENT ANDI LANE

Josef greenlit turning the relay-ready visions sampler into a concrete playtest save pack. The product now is not another proof receipt: it is a small labelled set of current-build saves or handoff sessions Josef can actually load and play.

Imagination source: `doc/caol-josef-playtest-save-pack-imagination-source-2026-05-02.md`.

Contract: `doc/caol-josef-playtest-save-pack-packet-v0-2026-05-02.md`.

Handoff packet: `doc/caol-josef-playtest-save-pack-handoff-v0-2026-05-02.md`.

Working playtest card: `doc/caol-josef-playtest-save-pack-card-v0-2026-05-02.md`.

Promoted from: `CAOL-VISIONS-PLAYTEST-SAMPLER-v0` and its relay card `doc/caol-visions-josef-playtest-card-v0-2026-05-01.md`.

Goal: prepare labelled playable entries for Basecamp AI / camp locker usefulness, bandit pressure / shakedown / basecamp contact, cannibal camp pressure, flesh raptor skirmishing, zombie rider predator/counterplay, and writhing stalker hit-fade / zombie-shadow behavior. Each entry needs a short “what to try” card, current-build load/start-state evidence, portal-storm warning status, proof footing, and plain staged-vs-natural caveats. Include Josef's thematic contrast pass for stalker and bandit behavior: no-fire/no-signal, fire/smoke/light signal, and high-threat/resistant setup should produce visibly different reads or be marked caveated/blocked. Before threat-contrast rows are credited, audit camp NPC membership: unassigned current-save NPCs are not camp threat evidence; low-threat rows may remove/kill/despawn extras, while high-threat rows must spawn or repair NPCs as properly camp-assigned members.

Current card checkpoint: `doc/caol-josef-playtest-save-pack-card-v0-2026-05-02.md` now lists six ready staged rows (camp locker weather/service, bandit first-demand contact, cannibal night pressure, flesh raptor crowded-arc skirmisher, zombie rider cover/wounded contrast, writhing stalker hit-fade/light/zombie-side pressure) plus explicit caveated/omitted rows for bandit contrast/camp-threat and camp/NPC assignment. Post-card no-signal repair `bandit.live_world_nearby_camp_no_signal_control_mcw` -> `.userdata/dev-harness/harness_runs/20260502_134959/` proves a cleaned low-threat/no-loose-NPC no-signal control, but it does not claim camp-threat membership and does not expand the first Josef card by itself. Smoke/fire signal is repaired through the materially different guarded `bandit.mixed_signal_coexistence_mcw` path -> `.userdata/dev-harness/harness_runs/20260502_155058/`, with green wait/step ledgers, portal clear, signal scout dispatch from camp `@151,39,0`, separate structural scavenge from camp `@160,39,0`, and manual saved-overmap active-member cross-references for members `4` and `9`; direct `bandit.player_lit_fire_signal_wait_mcw` attempt `20260502_154828/` remains blocked at startup UI and is not credited. Flesh raptor current footing was repaired by relaxing the stale exact crowded-arc tile expectation and rerunning `flesh_raptor.live_crowded_arc_skirmisher_mcw` -> `.userdata/dev-harness/harness_runs/20260502_141246/` green/portal-clear. Camp-pressure assignment is repaired as auxiliary proof: `bandit.active_outside_dogpile_block_live` -> `.userdata/dev-harness/harness_runs/20260502_144842/` is green/portal-clear and requires `active_member_ids=[4,5]` with `active_members_all_found_in_saved_overmap=true`. Pure high-threat hold is repaired as auxiliary proof: `bandit.high_threat_low_reward_holds` -> `.userdata/dev-harness/harness_runs/20260502_145429/` is green/portal-clear with green guarded wait and the same-run risk/reward hold artifact. After the later zombie-rider tainted-arrow/rider behavior commits, the rider cover/wounded card rows were refreshed on current `dev` with `.userdata/dev-harness/harness_runs/20260502_232133/` and `20260502_232214/`, both green and portal-clear after `./just_build_macos.sh > /tmp/caol-savepack-post-rider-build-20260502.log 2>&1` exited `0`. Andi's v0 save-pack prep is now at a state boundary: Schani can relay the six-entry card as-is and may include the optional staged bandit contrast card; no ready rows or old blocked bandit rows need ritual reruns.

Boundary: save-pack prep and product-taste handoff only. Do not implement new gameplay unless a hard blocker prevents a save from loading or being playable. Do not reopen closed v0 lanes by drift, do not create release packaging, and do not make Josef inspect logs as the primary playtest activity.

---

## Closed recent lane — bandit scenic shakedown chat openings

**Status:** CHECKPOINTED / CLOSED / SUPERSEDED

Josef asked on 2026-05-01 for the bandit shakedown to use a normal chat window, become more scenic, and have a selection of bandit openings.

Contract: `doc/bandit-scenic-shakedown-chat-window-openings-packet-v0-2026-05-01.md`.

Proof: `doc/bandit-scenic-shakedown-chat-window-openings-proof-v0-2026-05-02.md`.

Result: the live shakedown UI now uses the normal `dialogue_window` surface instead of the bare `uilist`, with contextual opening IDs/summaries/barks for basecamp pressure, warning from cover, weakness read, roadblock toll, and reopened higher-demand. Its staged proof is still useful for the scenic-dialogue path and cannibal/no-shakedown separation, but its `Pay / Fight / Refuse` response contract and pay-branch behavior are now explicitly superseded by `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`: Josef's stranded 2026-05-02/03 notes require visible Pay/Fight only and a trade/debt-style payment surface.

Boundary: scenic-dialogue proof only. Do not use this closed proof to claim the final shakedown response/payment contract; that correction now belongs to `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`.

---

## Recent closed detail — harness portal-storm warning light

**Status:** CHECKPOINTED GREEN V0 / HARNESS-HARDENING FOLLOW-UP

Josef reported on 2026-05-02 that portal storms sometimes seem to break harness runs and asked for a “flashing light”.

Contract: `doc/harness-portal-storm-warning-light-packet-v0-2026-05-02.md`.
Proof: `doc/harness-portal-storm-warning-light-proof-v0-2026-05-02.md`.

Result: report-level `portal_storm_warning` now lands in probe/handoff reports and repeatability summaries; unallowed portal storms yellow the step ledger as `yellow_step_portal_storm_contamination`; opt-in portal-storm scenarios remain visible but green when explicitly allowed.

Boundary: harness-hardening only. Do not solve portal-storm gameplay, redesign weather, rerun old packets by ritual, or reopen closed bandit/visions/camp-locker lanes by drift.

---

## Recent closed detail — zombie rider close-pressure no-attack fix

**Status:** CLOSED / CHECKPOINTED GREEN V0 / BUGFIX + PRODUCT-FEEL FOLLOW-UP

Josef live-watched `zombie_rider.live_open_field_pressure_mcw` on 2026-05-02 and then quit the game so Andi could get the package. The watched handoff seed is `.userdata/dev-harness/harness_runs/20260502_015857/`, but it is yellow/inconclusive due to runtime-version mismatch, so use it as a bug seed and player-observed taste note rather than closure proof.

Imagination source: `doc/zombie-rider-close-pressure-no-attack-imagination-source-2026-05-02.md`.

Contract: `doc/zombie-rider-close-pressure-no-attack-packet-v0-2026-05-02.md`.

Handoff packet: `doc/zombie-rider-close-pressure-no-attack-handoff-v0-2026-05-02.md`.

Goal/result: fixed the zombie rider close-pressure/no-attack seam. The important smell was `decision=bow_pressure`, `reason=line_of_fire`, and `line_of_fire=yes` at distance around `4-5`, while Josef saw no actual attack. The plan seam did not force `aggro_character`, and `gun_actor` refuses avatar shots while `!z.aggro_character`; current code marks aggro before ready bow handoff, and close/indoor pressure chooses irregular bunny-hop/reposition destinations instead of polite loitering.

Evidence: `doc/zombie-rider-close-pressure-no-attack-proof-v0-2026-05-02.md`; live row `.userdata/dev-harness/harness_runs/20260502_050055/`.

Boundary: bugfix/product-feel follow-up only. Do not reopen all `CAOL-ZOMBIE-RIDER-0.3-v0`, and do not break wounded disengagement, cover/LOS counterplay, camp-light banding, or no-light controls.

---

## Closed receipt — CAOL-WRITHING-STALKER-HIT-FADE-RETREAT-DISTANCE-v0

**Status:** CLOSED / CHECKPOINTED GREEN v0 / PRODUCT-TUNING + BEHAVIOR FIX FOLLOW-UP

Proof/readout: `doc/writhing-stalker-hit-fade-retreat-distance-proof-v0-2026-05-02.md`.

Josef live-watched the writhing stalker on 2026-05-02 and greenlit a focused follow-up for its attack/retreat rhythm. The watched handoff seed `.userdata/dev-harness/harness_runs/20260502_015032/` remains yellow/debug footing because of runtime-version mismatch.

Closed behavior: the stalker no longer uses generic `HIT_AND_RUN`; it owns a bounded burst/fade cadence. Favorable vulnerable pressure can permit a short 2-4 hit burst, while light/focus/allied support/injury push earlier/farther caution. Post-burst live planning targets about `8+` tiles of retreat when pathing allows.

Credited evidence: original flesh-raptor `HIT_AND_RUN` source reference; deterministic `[writhing_stalker]` suite green (`206 assertions in 17 test cases`); current-build `cataclysm-tiles` build green; staged-but-live McWilliams feature proof `.userdata/dev-harness/harness_runs/20260502_113738/` with `feature_proof=true`, `verdict=artifacts_matched`, `decision=strike`, `decision=withdraw`, `decision=cooling_off`, `burst=0/2`, `burst=1/2`, `burst=2/2`, `retreat_distance=8`, and `cooldown=yes`.

Caveat: staged-but-live feature proof, not natural random discovery or final human taste for exact scariness/forgiveness.

---

## Closed backlog receipt — targeted anti-redundancy packages

**Status:** CLOSED / CHECKPOINTED GREEN V0 SET / NEEDS FRESH PROMOTION FOR MORE

These were queued cleanup/refactor contracts with targeted tests; they are not current `TODO.md` work.

Shared imagination source: `doc/anti-redundancy-packaging-imagination-source-of-truth-2026-05-01.md`.

All originally queued v0 anti-redundancy packages in this block are now closed/checkpointed or need a freshly promoted concrete seam before further work.

Boundary: these packages do not reopen closed writhing-stalker, bandit, horde, Smart Zone, or camp claims by drift. The larger bandit live-world roster/mission-state reduction remains future audit territory, not part of this targeted package set.

---

## Held / parked lanes that must not disappear

**Status:** PARKED / HELD VISIBILITY ONLY

These are visible here only because they affect future selection. Do not treat them as active without explicit Schani/Josef promotion.

- `CAOL-GITHUB-RELEASE-v0` — **HELD / NOT PROMOTED**. Normal GitHub-download release work remains held until Schani/Josef explicitly promote it. Contract: `doc/github-normal-download-release-packet-v0-2026-04-25.md`.
- `CAOL-BANDIT-OVERMAP-AI-CONCEPT-v0` — **PARKED / COHERENT SUBSTRATE**. Parent concept: `doc/bandit-overmap-ai-concept-2026-04-19.md`; bounded promoted slices need their own IDs/contracts.
- `CAOL-AOL-SOCIAL-THREAT-BANK-v0` — **PARKED / FAR BACK**. Anchor: `doc/arsenic-old-lace-social-threat-and-agentic-world-concept-bank-2026-04-22.md`. Writhing stalker has now been promoted out of this bank into its own active packet; the rest remains parked.

---

## Recent closed receipts

Use `doc/work-ledger.md` and the linked aux docs for exact evidence, caveats, and supersessions. Do not reopen these by drift.

- `CAOL-ROOF-HORDE-NICE-FIRE-v0` — closed/checkpointed green v0; focused nice roof-fire horde playtest using source player-created roof-fire chain `.userdata/dev-harness/harness_runs/20260429_172847/`, named scenario `bandit.roof_fire_horde_nice_roof_fire_mcw`, and green run `.userdata/dev-harness/harness_runs/20260430_191556/`. Proof reaches live roof-fire horde signal plus saved horde retarget/destination, `last_processed=5267242`, and `moves=8400`; caveats: `tracking_intensity=0` and horde-specific timing `not instrumented`. Proof doc `doc/roof-fire-horde-nice-roof-fire-proof-v0-2026-04-30.md`.
- `CAOL-WRITHING-STALKER-v0` — closed/checkpointed green v0; first playable rare singleton zombie-adjacent predator with deterministic interest/latch/approach/opportunity/withdraw substrate, live monster-plan seam, exposed-retreat proof, shadow/strike/cooldown proof, no-omniscience negative control, and mixed-hostile metrics/tuning readout. Caveat preserved: horde presence/setup is proven in `.userdata/dev-harness/harness_runs/20260430_181748/`, but direct horde movement/retarget cost is not instrumented and is future-only unless explicitly promoted.
- `CAOL-BANDIT-STRUCT-BOUNTY-v0` — closed/checkpointed green v0; closure readout `doc/bandit-structural-bounty-phase-7-closure-readout-2026-04-30.md`; green run `.userdata/dev-harness/harness_runs/20260430_115157/`; non-credit run `.userdata/dev-harness/harness_runs/20260430_114106/`.
- `CAOL-SZM-LIVE-LABEL-v0` — closed green live coordinate-label proof; run `.userdata/smart-zone-audit-live-20260429e/harness_runs/20260429_225644/`.
- `CAOL-CANNIBAL-NIGHT-RAID-v0` — closed cannibal camp night-raid behavior checkpoint; contract `doc/cannibal-camp-night-raid-behavior-packet-v0-2026-04-28.md`.
- `CAOL-CANNIBAL-CONFIDENCE-PUSH-v0` — closed confidence-uplift proof; matrix `doc/cannibal-camp-confidence-push-live-playtest-matrix-v0-2026-04-29.md`.
- `CAOL-BANDIT-CAMP-MAP-RISK-REWARD-v0` — closed scoped live/product checkpoint; matrix `doc/bandit-live-product-playtest-matrix-v0-2026-04-28.md`.
- `CAOL-HARNESS-PROOF-FREEZE-v0` — closed/checkpointed process rule; proof-freeze matrix `doc/c-aol-harness-proof-freeze-matrix-v0-2026-04-28.md`.
- `CAOL-REAL-FIRE-SIGNAL-v0` and `CAOL-ROOF-HORDE-SPLIT-v0` — closed actual-playtest proof bundles for real-fire signal response and split-run roof-fire horde response.
- `CAOL-BANDIT-LIVE-SIGNAL-SITE-BOOTSTRAP-v0` — superseded partial lane; real player-fire signal response is closed by `CAOL-REAL-FIRE-SIGNAL-v0`, while remaining range/decay/scoring questions are future-only unless explicitly promoted.

---

## Documentation discipline

If the structure starts bloating again, apply this rule:
- `Plan.md` should be readable in a minute
- `TODO.md` should show only the current execution queue
- `TESTING.md` should show only current policy, latest relevant evidence, and pending probes
- `COMMIT_POLICY.md` should stop the dirty tree from becoming a lifestyle

If a sentence exists only to remember that something used to be true, it probably belongs in git history, not here.
