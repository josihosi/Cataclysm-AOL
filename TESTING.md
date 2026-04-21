# TESTING

Current validation policy and current evidence only.

This file is not a trophy wall.
Remove stale or completed fluff instead of stacking crossed-off test history forever.

## Validation policy

Use the **smallest evidence that honestly matches the change**.

- **Docs-only change** -> no compile
- **Small local code change** -> narrow build and the narrowest relevant test target
- **Broad or risky code change** -> broader rebuild and relevant filtered tests
- **Before a Josef handoff** or after suspicious stale-binary behavior -> rebuild the real targets and rerun the real smoke path

### Agent vs Josef

Schani should do agent-side playtesting first whenever the harness or direct in-game probing can answer the question.
Josef should be asked only for:
- product judgment
- tone/feel
- human-priority choices
- genuinely human-only interaction

Josef being unavailable is **not** a blocker by itself.
Josef self-testing is **not** a plan blocker and does not belong in the active success state as a gate.
If Josef-specific checks are useful, write them down as non-blocking notes so he can run them later from his own list.
If another good agent-side lane exists, keep moving there.
If several human-only judgments are likely soon, batch them instead of sending tiny separate asks.

### Anti-churn rule

Do not keep rerunning the same startup or test packet when it is no longer the missing evidence class.
If startup/load is already green, and the missing proof is live behavior, then the next probe must target live behavior.
If a target is merely waiting on Josef, do not keep revalidating it unless the code changed.

### Bandit overmap-proof rule

For the remaining bandit AI proof packets, single-turn deterministic checks are **not** enough by themselves.
The honest bar now includes real overmap-side multi-turn scenario proof, up to `500` turns where needed, with explicit per-scenario goals and tuning metrics.

---

## Current relevant evidence

### Latest closed lane — Bandit weather concealment refinement packet v0

Current honest state:
- `src/bandit_mark_generation.{h,cpp}` now carries a bounded weather refinement on the current smoke/light seam: windy smoke loses source precision and confidence instead of taking only a token range haircut, while portal-storm weather is explicit for both smoke and light
- reviewer-readable smoke/light summaries now expose the weather read directly, including fuzzed, reduced, blocked, displaced/corridor-ish, and preserved bright-light cases instead of hiding the interpretation in debugger soup
- `src/bandit_playback.cpp` now carries explicit windy-smoke and portal-storm exposed-light scenarios on the current generated-mark seam, and the broader playback reference suite count is updated to include them instead of pretending the suite froze in amber
- deterministic coverage in `tests/bandit_mark_generation_test.cpp` and `tests/bandit_playback_test.cpp` now proves the bounded weather distinctions honestly: windy smoke stays scoutable but fuzzier, portal-storm smoke is harder to localize, exposed bright portal-storm light can stay legible while sheltered ordinary light stays bounded, and rain remains an explicit reducer
- narrow deterministic validation passed via `make -j4 tests`, `./tests/cata_test "[bandit][marks]"`, `./tests/cata_test "[bandit][playback]"`, and `./tests/cata_test "[bandit]"`

### Latest closed lane — Bandit overmap/local pressure rewrite packet v0

Current honest state:
- `src/bandit_playback.{h,cpp}` now carries the named `generated_local_loss_rewrites_corridor_to_withdrawal` and `generated_local_loss_reroutes_to_safer_detour` multi-turn scenarios on the current playback seam
- the same file now also exposes `run_overmap_local_pressure_rewrite_proof_packet()` plus `render_overmap_local_pressure_rewrite_proof_packet( const proof_packet_result &result )` so the reviewer-readable packet is first-class output instead of debugger folklore
- deterministic coverage in `tests/bandit_playback_test.cpp` now proves the key bounded distinctions honestly: one stale corridor posture collapses to `hold / chill` once local loss makes the tile too hot, one parallel intercept case reroutes onto a safer detour instead of homing forever on stale pressure, and the long horizon stays off the burned route instead of regrowing it by habit
- narrow deterministic validation passed via `make -j4 tests`, `./tests/cata_test "[bandit][playback]"`, and `./tests/cata_test "[bandit]"`

### Latest closed lane — Bandit long-range directional light proof packet v0

Current honest state:
- `src/bandit_playback.{h,cpp}` now carries three named directional-light multi-turn scenarios on the generated-mark / playback seam: hidden-side inert, visible-side actionable, and shared corridor pressure
- the same file now also exposes `run_long_range_directional_light_proof_packet()` plus `render_long_range_directional_light_proof_packet( const proof_packet_result &result )` so the reviewer-readable packet is first-class output rather than debugger folklore
- deterministic coverage in `tests/bandit_playback_test.cpp` now proves the key bounded distinctions honestly: hidden-side leakage stays non-actionable, visible-side leakage becomes actionable under the same broader footing, and the matching corridor case still carries shared zombie-pressure consequences instead of becoming private bandit-only theater
- the playback reference suite now includes those directional-light scenarios directly, so the broader bandit playback suite keeps the packet visible instead of burying it in one bespoke spot check
- narrow deterministic validation passed via `make -j4 tests`, `./tests/cata_test "[bandit][playback]"`, and `./tests/cata_test "[bandit]"`

### Latest closed lane — Bandit bounded scout/explore seam v0

Current honest state:
- `src/bandit_dry_run.{h,cpp}` now carries one explicit bounded scout/explore packet on the current evaluator seam via `camp_input`, for map uncovering only instead of accidental random wandering
- unreachable leads still fail closed as `no_path`, and the evaluator seam does not auto-mint explore just because a route is blocked
- deterministic coverage in `tests/bandit_dry_run_test.cpp` proves the key bounded distinctions honestly: explicit explore can beat `hold / chill`, blocked routes do not create explore without the explicit greenlight, and strong real reachable leads still stay ahead when they score better
- `src/bandit_playback.cpp` now carries the named `bounded_explore_frontier_tripwire` scenario packet with explicit goals and tuning metrics, and `tests/bandit_playback_test.cpp` proves the same rule on the scenario seam
- reviewer-readable dry-run output now says plainly that the outing is explicit map uncovering and not accidental random wandering
- narrow deterministic validation passed via `make -j4 tests`, `./tests/cata_test "[bandit][dry_run]"`, and `./tests/cata_test "[bandit][playback]"`

### Latest closed lane — Bandit moving-bounty memory seam v0

Current honest state:
- `src/bandit_mark_generation.{h,cpp}` now carries a bounded moving-memory packet for moving-carrier and corridor marks only, with leash, opportunity/threat bands, and reviewer-readable transition state instead of a broad memory architecture pass
- structural/site marks stay on cheap site footing and do not gain chase memory
- deterministic coverage in `tests/bandit_mark_generation_test.cpp` now proves the key bounded distinctions honestly: one moving lead persists for a short honest window after raw signal cooling, one structural/site lead does not gain chase memory, and one stale moving lead drops reviewer-cleanly on leash expiry instead of being chased forever
- reviewer-readable output now explains whether a moving lead was refreshed, narrowed, or dropped instead of hiding the memory state in debugger soup
- `tests/bandit_playback_test.cpp` now keeps the corridor playback packet aligned with the bounded memory window instead of pretending the mark cools all the way to quiet immediately
- narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`

### Latest closed lane — Bandit scoring refinement seam v0

Current honest state:
- `src/bandit_dry_run.{h,cpp}` now carries a bounded danger-collapse packet on the current evaluator seam, forwarding existing monster-pressure and target-coherence footing into `effective_threat_penalty` plus a small opportunism bonus instead of inventing a fresh threat ontology
- `src/bandit_mark_generation.cpp` now forwards existing `monster_pressure_add` and `target_coherence_subtract` into emitted leads so current marks actually drive the refined score choice instead of burying that footing inside one undifferentiated threat number
- deterministic coverage in `tests/bandit_dry_run_test.cpp` and `tests/bandit_mark_generation_test.cpp` now proves the key bounded distinctions honestly: one clearly strong target still gets deferred, one distracted target becomes materially more attractive, one neutral case stays sane, and reviewer-readable output exposes raw threat, collapsed threat, and the bounded opportunism window
- the slice stayed bounded: no new visibility signal family, no broad heatmap/memory rewrite, no tactical zombie simulation, no coalition strategy layer, and no fresh world-sim expansion
- narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`

### Recently closed lane — Bandit concealment seam v0

Current honest state:
- `src/bandit_mark_generation.{h,cpp}` now carries a bounded concealment reduction on the current light seam, applying daylight, weather, containment/terrain, and side-leakage modifiers on top of existing local light truth instead of inventing a second visibility machine
- deterministic coverage in `tests/bandit_mark_generation_test.cpp` and `tests/bandit_playback_test.cpp` now proves the key bounded distinctions honestly: daylight suppression, weather penalty, containment, side-dependent leakage/suppression, and reviewer-readable reduced/blocked/allowed verdicts
- the reviewer-readable report path now exposes the concealment summary directly in generated light-mark output instead of hiding it in debugger soup
- the slice stayed bounded and cheap: no broad all-signals concealment rewrite, no fog-sound law, no global smoke/world simulation, no tactical stealth doctrine, and no pursuit/handoff expansion, so no extra perf probe was needed
- narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`

### Recently closed lane — Bandit first 500-turn playback proof v0

Current honest state:
- the current abstract bandit seams now have a first-class long-horizon proof path: `src/bandit_playback.{h,cpp}` adds `proof_packet_result`, `run_first_500_turn_playback_proof()`, and `render_first_500_turn_playback_proof( const proof_packet_result &result )`
- the proof packet stays bounded on the named `smoke_only_distant_clue`, `city_edge_moving_hordes`, and `generated_repeated_site_reinforcement_stays_bounded` scenarios, with explicit `tick 500` framing so reviewer-readable winner drift and generated-mark state can be inspected without debugger soup
- the repeated-site case now cools honestly on the idle horizon instead of remaining immortal scout pressure, and the proof packet still does not unlock magical settlement truth, free extraction, or broader world-sim claims
- narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`

### Recently closed lane — Bandit repeated site activity reinforcement seam v0

Current honest state:
- the bounded repeated-site seam now lands in code: `src/bandit_mark_generation.{h,cpp}` carries a `repeated_site_reinforcement_packet`, and `src/bandit_playback.cpp` now feeds that packet through the existing generated-mark seam with the named `generated_repeated_site_reinforcement_stays_bounded` scenario
- deterministic coverage in `tests/bandit_mark_generation_test.cpp` and `tests/bandit_playback_test.cpp` now proves the key bounded distinctions honestly: mixed repeated signals reinforce one site mark cleanly, weak repetition does not fake durable settlement truth, self-corroboration stays bounded, and strengthened site interest still keeps extraction jobs blocked
- reviewer-readable playback / mark reports now expose the reinforcement packet and resulting mark/lead path instead of hiding the bridge in debugger soup
- the product rules stay preserved: repeated ordinary site signals raise revisit interest first, not free settlement truth, free loot truth, or a magical raid warrant
- the slice stayed bounded: no smoke/light/human-route rewrite, no broad concealment implementation, no settlement-signature mythology, and no 500-turn proof theater
- narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`

### Recently closed lane — Bandit human / route visibility mark seam v0

Current honest state:
- the bounded human / route seam now lands in code: `src/bandit_mark_generation.{h,cpp}` adds deterministic human/route packets plus a bounded adapter, and `src/bandit_playback.{h,cpp}` now feeds those packets through the existing generated-mark seam
- deterministic coverage in `tests/bandit_mark_generation_test.cpp` and `tests/bandit_playback_test.cpp` now proves the key bounded distinctions honestly: direct sightings stay mobile, same-camp routine traffic stays suppressed, shared/external route activity can reinforce corridors, and only site-correlated traffic yields bounded site clues with extraction still blocked
- the product rules stay preserved: direct human sightings are strong bounty clues, route activity only hardens when it plausibly belongs to somebody else or a shared corridor, and the camp's own routine traffic does not self-poison into hostile-contact truth
- the slice stayed bounded: no smoke/light rewrite, no broad visibility/concealment implementation, no settlement-signature mythology, no full traffic simulator, and no 500-turn proof theater
- narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`

### Recently closed, do not casually reopen

- Bandit smoke visibility mark seam v0 is now honestly checkpointed:
  - the bounded smoke seam now lands in code: `src/bandit_mark_generation.{h,cpp}` adds deterministic smoke packets plus a bounded smoke adapter, and `src/bandit_playback.{h,cpp}` now feeds those packets through the existing generated-mark seam
  - deterministic coverage in `tests/bandit_mark_generation_test.cpp` and `tests/bandit_playback_test.cpp` now proves the coarse long-range rule honestly: sustained clear-weather smoke can stay several OMT legible with a hard cap, while weak fogged smoke does not fake long-range truth
  - reviewer-readable playback / mark reports now expose the smoke packet projection and resulting mark/lead path instead of hiding the bridge in debugger soup
  - the slice stayed bounded: no light/searchlight adapter, no broad visibility/concealment implementation, no global offscreen smoke sim, and no 500-turn proof theater
  - narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`

- Locker lag-threshold probe v0 is now honestly checkpointed:
  - the threshold packet now stays on the real `CAMP_LOCKER` service path with locker `DebugLog` noise suppressed during timing runs, so measurement output is readable instead of log-spew soup
  - the high-clutter fixture is now honest: `tests/faction_camp_test.cpp` spreads top-level clutter across real multi-tile locker zones, which avoids lying about `5000 / 10000 / 20000` items on one tile past `MAX_ITEM_IN_SQUARE`
  - the packet now covers top-level clutter at `1000 / 2000 / 5000 / 10000 / 20000` plus worker-count sweeps at `1 / 5 / 10` on `5000` clutter
  - current result: no clear knee was found within the tested bound, with median service time staying roughly linear from about `210 us` at `1000` clutter to about `4152 us` at `20000`, and about `1.0 ms` per worker at `5000` clutter across `1 / 5 / 10`
  - narrow deterministic validation passed via `make -j4 tests`, `./tests/cata_test "[camp][locker]~[threshold]"`, and `./tests/cata_test "[camp][locker][threshold]"`

- Bandit overmap-to-bubble pursuit handoff seam v0 is now honestly checkpointed:
  - the authoritative contract lives at `doc/bandit-overmap-to-bubble-pursuit-handoff-seam-v0-2026-04-20.md`
  - `src/bandit_pursuit_handoff.{h,cpp}` now provides the bounded overmap-to-bubble handoff, building an explicit `entry_payload`, explicit `return_packet`, bounded `scout` / `probe` / `shadow` / `withdrawal` chooser, and abstract-state writeback through `apply_return_packet()`
  - deterministic coverage in `tests/bandit_pursuit_handoff_test.cpp` now proves the bounded scout entry packet, explicit return consequences, moving-carrier shadow routing, and reviewer-readable report output
  - narrow deterministic validation passed via `make -j4 tests`, `./tests/cata_test "[bandit][handoff]"`, and `./tests/cata_test "[bandit]"`

- Locker clutter / perf guardrail probe v0 is now honestly checkpointed:
  - the authoritative contract lives at `doc/locker-clutter-perf-guardrail-probe-v0-2026-04-20.md`
  - `src/basecamp.{h,cpp}` now exposes a direct locker-service probe through `camp_locker_service_probe`, `basecamp::measure_camp_locker_service( npc & )`, and `render_camp_locker_service_probe()` instead of fake-path speculation
  - deterministic coverage in `tests/faction_camp_test.cpp` now exercises the real `CAMP_LOCKER` service path across top-level clutter sweeps at `50 / 100 / 200 / 500 / 1000`, worker-count sweeps at `1 / 5 / 10`, the first junk-heavy / locker-candidate-heavy / ammo-magazine-container-heavy stock-shape comparison, and the nested-content question for loaded magazines and ordinary filled bags
  - the current verdict is `fine for now`: service cost scales with top-level locker items and worker passes, while loaded magazines and ordinary filled bags still behave like one top-level locker item on this path instead of triggering an obvious nested-cost cliff
  - narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[camp][locker]"`

- Bandit mark-generation + heatmap seam v0 is now honestly checkpointed:
  - the authoritative contract lives at `doc/bandit-mark-generation-heatmap-seam-v0-2026-04-20.md`
  - `src/bandit_mark_generation.{h,cpp}` now provides a bounded writer-side mark ledger with deterministic signal ingestion, refresh, selective cooling, sticky confirmed threat, generated lead emission, and a reviewer-readable mark/heat report instead of hand-authored lead theater
  - `src/bandit_playback.{h,cpp}` now bridges that ledger into the playback/evaluator footing, carries generated marks/leads through checkpoints, and exposes generated mark state in playback reports
  - deterministic coverage in `tests/bandit_mark_generation_test.cpp` and `tests/bandit_playback_test.cpp` now proves mark creation, refresh, selective cooling, sticky confirmed threat, and the clean bridge into the evaluator/playback seam
  - narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`

- Bandit perf + persistence budget probe v0 is now honestly checkpointed:
  - `bandit_dry_run::evaluation_metrics` now makes lead filtering, candidate generation, score/path checks, invalidations, and winner-comparison churn visible instead of hand-waved away inside the evaluator
  - `src/bandit_playback.{h,cpp}` now provides `measure_scenario_budget()`, `measure_reference_suite_budget()`, `estimate_v0_persistence_budget()`, and `render_budget_report()` so the named scenarios can answer runtime, churn, save-budget, and cheap-enough-vs-suspicious questions reviewer-cleanly
  - the first bounded persistence sample lands at about `512` payload bytes before serializer overhead and still reads cheap enough for the abstract v0 shape, with duplicated tactical truth remaining the main obvious future bloat risk
  - narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`
- Bandit scenario fixture + playback suite v0 is now honestly checkpointed:
  - `src/bandit_playback.{h,cpp}` now provides fourteen stable named deterministic scenarios on top of the bounded dry-run evaluator, including seven generated-mark writer-side cases, instead of pretending a broader world simulator already exists
  - `run_scenario()` replays those cases at `tick 0`, `tick 5`, `tick 20`, and `tick 100`, while `render_report()` gives a reviewer-readable checkpoint summary for drift questions
  - narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`
- Bandit evaluator dry-run seam v0 is now honestly checkpointed:
  - `src/bandit_dry_run.{h,cpp}` provides the bounded abstract evaluator, candidate board, visible scoring ladder, threat gating, and downstream `no_path` rejection without smuggling in playback or persistence architecture
  - `render_report()` provides the first inspectable explanation surface instead of ghost-hunting through debugger soup
  - narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit][dry_run]"`
- Plan status summary command is now honestly checkpointed:
  - `tools/plan_status_summary.py` provides the deliberately small read-only command seam for current canon
  - thin/contradictory handling is covered by the built-in self-test plus the live thin-canon warning on the current `Hackathon feature lanes` heading
  - validation stayed proportional: command-level proof only, no build or harness ritual
- Bandit concept formalization follow-through is now honestly checkpointed:
  - Package 3, micro-item 31 (`Invariants and non-goals packet`) is now landed, so the full 3-package / 31-micro-item packet finally has explicit red-line invariants/non-goals on top of its starter numbers and worked scenarios
  - the follow-through now closes as docs-only control-law cleanup inside the parked bandit chain, not as implementation greenlight
  - validation stayed honest for the slice: docs-only change, so no compile or harness ritual was needed
- Plan/Aux pipeline helper is now honestly checkpointed:
  - `tools/plan_aux_pipeline_helper.py` still keeps canon patching bounded to known headings, but `emit` can now also produce downstream `andi.handoff.md` output from the same validated classified contract
  - narrow validation passed via `python3 -m py_compile tools/plan_aux_pipeline_helper.py`, `schema`, `show`, `emit`, emitted handoff review, and `apply` on a temp repo copy
- Combat-oriented locker policy is now honestly checkpointed:
  - the final closure audit found one real remaining deterministic gap, namely end-to-end service proof for the newly explicit common combat-support slots
  - that gap is now closed by `camp_locker_service_equips_common_combat_support_slots`, which proves real locker service equips gloves, dust mask, tool belt, and holster from `CAMP_LOCKER` stock
  - focused deterministic validation passed on the current tree via `make -j4 tests`, `./tests/cata_test "camp_locker_service_equips_common_combat_support_slots"`, and `./tests/cata_test "[camp][locker]"`
  - the filtered locker suite still covers the previously suspicious safety seams this packet could have broken, including great-helm classification, ballistic body-armor comparisons, holster-vs-pants cleanup behavior, weird-garment preservation, weather-sensitive outerwear/legwear handling, and full-body suit protection logic
- Organic bulletin-board speech polish is now reclosed on both deterministic and live footing:
  - `make -j4 tests` plus `./tests/cata_test "[camp][basecamp_ai]"` passed for the widened organic status parsing and cleaned spoken bark
  - live run `.userdata/dev-harness/harness_runs/20260419_154244/` used the real camp-assignment seam, asked `what needs making`, and showed `Board's got 1 live and 1 old - 1 x bandages.` with no visible request-id glue
  - Robbie's same-packet follower crosstalk stays separate routing noise, not a reason to reopen the closed machine-speech seam
- Locker surface/control is also reclosed on both deterministic and live footing:
  - deterministic proof still covers locker policy persistence and sorting skip behavior on `CAMP_LOCKER`
  - live run `.userdata/dev-harness/harness_runs/20260419_141422/` created `Basecamp: Locker`, renamed it to `Probe Locker`, used the single-`Esc` -> save-prompt -> `Y` closeout, and reopened Zone Manager with `Probe Locker` still present
  - no type-mismatch popup or related stderr/debug failure surfaced on that live packet

### Meaning

- there is currently no active greenlit bandit lane in repo canon
- the bounded scout/explore seam is now checkpointed closed with dry-run plus playback/reference proof
- do not reopen the scoring seam, concealment seam, smoke bridge, light bridge, human / route bridge, repeated-site reinforcement seam, first 500-turn proof, locker threshold packet, or earlier locker shape packet unless new evidence says the answer was dishonest or incomplete
- the bandit pursuit handoff seam is now checkpointed closed with deterministic proof and reviewer-readable packet output
- the writer-side bandit mark-generation seam is now checkpointed closed too
- the next honest bandit step is a fresh bounded greenlight from the parked chain, not more ceremonial reruns of the closed weather packet

---

## Pending probes

- Active proof obligation now: `Bandit overmap benchmark suite packet v0` at `doc/bandit-overmap-benchmark-suite-packet-v0-2026-04-21.md`.
- Build one complete named overmap scenario family on the current playback/proof seam instead of leaving the benchmark burden split across isolated narrow packets.
- Every required scenario must carry an easy-to-read `100`-turn benchmark packet.
- Scenarios that honestly need the longer horizon must also carry `500`-turn carry-through proof instead of pretending `100` turns proves everything.
- The suite must explicitly include the empty-frontier case where nothing useful is nearby and the camp should venture out through bounded scout/explore behavior and increase frontier visibility instead of sitting forever.
- Benchmark misses should be treated as routing-logic failures to fix, not as excuses to blur the benchmark until it passes.
- `Bandit z-level visibility proof packet v0` stays ungreenlit until this suite exists and has started surfacing the next real problems.

---

## Reusable commands

Use these when they are actually the missing evidence, not as ritual.

### Narrow locker deterministic pass after a code slice lands
- `make -j4 tests`
- `./tests/cata_test "[camp][locker]"`

### Plan status summary command
- `python3 tools/plan_status_summary.py --self-test`
- `python3 tools/plan_status_summary.py /plan`
- `python3 tools/plan_status_summary.py /plan active`
- `python3 tools/plan_status_summary.py /plan greenlit`
- `python3 tools/plan_status_summary.py /plan parked`

### Bandit dry-run evaluator seam foundation
- `make -j4 tests`
- `./tests/cata_test "[bandit][dry_run]"`

### Fresh tiles rebuild only if a later combat-policy handoff really needs live proof
- `make -j4 TILES=1 cataclysm-tiles`

## Local build caveat

On this Mac, treat top-level `make -j4 tests` as the reliable path for a fresh `cata_test`.
Avoid treating `make -C tests cata_test` as authoritative here; it has been a repeated source of toolchain/stale-build nonsense.
Also: if you actually need a fresh tiles binary, use `make -j4 TILES=1 cataclysm-tiles`; plain `make cataclysm-tiles` is not an honest rebuild path here.
