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

---

## Current relevant evidence

### Bandit concept formalization follow-through

Current honest state:
- the active lane is still doc/spec only, not code, and its canonical work packet lives at `doc/bandit-concept-formalization-followthrough-2026-04-19.md`
- that packet still carries the 3-package / 31-micro-item structure, but Package 1, micro-item 1 is now answered with a 0-5 starter structural-bounty table for open street, field, forest, cabin, house, farm, city structure, and camp footprint
- the table freezes the intended site-class ordering explicitly: `open street/field < forest < cabin < house < farm/city structure < camp footprint`
- Package 1, micro-item 2 is now answered too: structural bounty only depletes after a site-contacting exploitation pass that actually extracts, strips, or meaningfully denies site-bound value; scouting, visitation, and failed approach do not count
- Package 1, micro-item 3 is now answered too: depleted structural bounty stays down until the site itself gains new site-bound value again through real resupply, rebuilding, or reoccupation; fresh traffic/signals may justify renewed interest, but only as moving bounty
- Package 1, micro-item 4 is now answered too: moving bounty source families are now named explicitly for direct humans/NPC groups, repeated route traffic, caravans/haul convoys, basecamp routine, smoke, ordinary visible light, defensive scanning light, and sound/contact clues
- Package 1, micro-item 5 is now answered too: moving bounty now stays source-shaped, with explicit actor/group, route/corridor, and site-centered activity attachments instead of blurring back into structural terrain value
- Package 1, micro-item 6 is now answered too: moving bounty cleanup now happens by evidence-driven refresh, overwrite, clear, replacement, or coexistence on the current carrier instead of passive timer-shave language
- adjacent parked substrate wording was tightened too: the scoring/concept docs now say moving bounty is rewritten or cleared by changed evidence or negative recheck, while cheap cleanup only prunes weak unsupported clutter
- Package 1, micro-item 7 is now answered too: threat marks now explicitly come from direct humans, organized defenders, fortification signs, searchlights/watch routines, combat-contact cues, failed probes, recent losses, and zombie/monster pressure, while ordinary smoke/light/traffic stays bounty-first unless paired with harder danger evidence
- Package 1, micro-item 8 is now answered too: threat now rewrites source-by-source through real recheck only, with explicit raise / confirm / lower cases for harder evidence, repeated corroboration, bad outcomes, and close contradictory observation
- adjacent parked substrate wording was tightened again too: the scoring/concept/mark-model docs now say threat stays sticky without passive decay and only downgrades through source-specific close recheck or successful passage
- Package 1, micro-item 9 is now answered too: threat and bounty now stay as separate concurrent reads, so the same region/carrier may remain attractive and dangerous at once unless a source-specific recheck actually disproves one side
- Package 1, micro-item 10 is now answered too: outings may collect opportunistically on the current corridor or immediate intercept envelope, but not by broad side-sweep harvesting, road-as-loot-tile logic, or free mission rewrites
- adjacent parked substrate wording was tightened once: the synthesis/scoring docs now talk about intercept-worthy corridor value and bounded on-path skimming rather than vague repeated route "farming"
- Package 1, micro-item 11 is now answered too: one outing converts at most 0-3 haul steps into stockpile, route-side skims stay inside that same cap, and rich regions still need repeat exploitation instead of one magic vacuum pass
- Package 1, micro-item 12 is now answered too: camp stockpile drains only by one daily housekeeping pass plus explicit dispatch/return mission costs, with food/ammo/med pressure driven by those named costs instead of hidden decay
- Package 1, micro-item 13 is now answered too: pure forest now cashes out as thin food/fuel/basic-material value, usually 0-1 haul, while richer hidden sites in the woods should score as those site classes rather than as magical forest bounty
- Package 2, micro-item 14 is now answered too: daily travel is now frozen as a job-shaped 1-6 OMT/day starter envelope, with local forage skims at the bottom, raids/reinforce packets near the top, and cadence spending explicitly deferred instead of hidden inside the table
- Package 2, micro-item 15 is now answered too: cadence now spends only elapsed-time-earned travel credit from that daily budget, carrying fractional progress forward instead of rounding each wake into a fresh free OMT hop
- Package 2, micro-item 16 is now answered too: target desirability now falls by the round-trip share of the outing's daily travel budget, with starter discount bands that make the same destination cheaper for higher-commitment jobs and keep mediocre far targets from tying with nearer ones by accident
- adjacent parked substrate wording was tightened again: the broad concept doc now points at the landed round-trip-share distance burden instead of treating it as still-undecided future law
- Package 2, micro-item 17 is now answered too: each outing now carries a calm-condition elapsed-time return clock with starter job-shaped leash values, fresh sightings do not refresh that leash for free, and the default flips toward home once only plain return time remains
- Package 2, micro-item 18 is now answered too: cargo, wounds, and panic now collapse into a bounded additive burden table that only shrinks useful travel and remaining return clock after the calm-condition movement law is already set
- adjacent parked substrate wording was tightened again where it still implied cargo/wounds/panic burden was undecided future law
- Package 2, micro-item 19 is now answered too: `hold / chill` is the explicit score-`0` no-target fallback baseline, so camps stay home when no outward job beats that bar instead of inheriting random-wander behavior
- adjacent parked substrate wording was tightened again where it still implied no-target fallback was undecided future law
- Package 2, micro-item 20 is now answered too: unreachable targets now fail closed at dispatch time, write a soft route-blocked / unreachable-from-here damper, and fall back to other reachable jobs or `hold / chill` instead of implicit wander/teleport behavior
- adjacent parked substrate wording was tightened again where it still implied no-path fallback was undecided future law
- Package 2, micro-item 21 is now answered too: already-dispatched outings now have one bounded mid-route rewrite ladder, where fresh same-envelope evidence may only continue, probe, shadow, divert, or abort/reroute while spending the same remaining travel and return leash
- adjacent parked substrate wording was tightened again where it still treated mission leads as free-form target rewrites instead of bounded same-outing switches
- Package 3, micro-item 22 is now answered too: candidate generation is now frozen as a separate board-building pass, with `hold / chill` always present and outward jobs emitted only from deduped compatible lead envelopes whose hard preconditions already hold
- adjacent parked substrate wording was tightened again where it still blurred board generation together with later scoring/veto/no-path filtering
- Package 3, micro-item 23 is now answered too: job comparison now freezes as positive pull from lead bounty, lead confidence, job-lead fit, mild need alignment, soft temperament bias, and job-type bias, shaped by the already-landed distance multiplier, then reduced by soft threat and active-pressure penalties before later desperation/veto passes
- adjacent parked substrate wording was tightened again where it still hand-waved one muddy `job_score` instead of the now-landed pre-veto comparison shape
- Package 3, micro-item 24 is now answered too: only `low` and `critical` shortage bands may add a capped reward-profile-matched rescue bonus after pre-veto scoring, and only for mediocre real leads near `hold / chill` rather than stale emptiness or deep-negative suicide jobs
- adjacent parked substrate wording was tightened again where it still treated desperation as an unfrozen future blob instead of the now-landed bounded need-pressure rescue
- Package 3, micro-item 25 is now answered too: threat gating now freezes as `discount_only`, `soft_veto`, or `hard_veto`, with soft-veto preserving only capped marginal info/pressure jobs and hard-veto blocking fresh catastrophic recent-loss / failed-probe style reads outright
- adjacent parked substrate wording was tightened again where it still treated threat veto as an unfrozen future blob instead of the now-landed post-score danger ladder
- Package 3, micro-item 26 is now answered too: local handoff now freezes as a deterministic chooser from winning job, lead carrier, contact certainty, danger posture, and return pressure into bounded `scout` / `probe` / `harvest` / `ambush` / `raid` / `shadow` / `withdrawal` entry modes
- the next honest active slice is Package 3, micro-item 27, `Bubble-to-overmap return-state packet`
- the immediate evidence bar is still documentation consistency, not compile or harness proof: the next patch should name which exact fields must come back from bubble play, without quietly solving save/load persistence or the later starter-number/scenario packet in the same pass

### Recently closed, do not casually reopen

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

- the missing evidence for the active lane is not another locker rerun or compile ritual
- the score-shape, need-pressure, threat-veto, and entry-mode laws are no longer open either; the next honest move is the bubble return-state packet, not another handoff chooser rerun
- the now-closed helper and locker/basecamp packets stay closed unless a fresh contradiction appears

---

## Pending probes

- land Package 3, micro-item 27, `Bubble-to-overmap return-state packet`, as one bounded doc/spec slice
- reread only the adjacent parked bandit substrate needed to keep that return-state field list separate from later Package 3 save/load persistence work

---

## Reusable commands

Use these when they are actually the missing evidence, not as ritual.

### Narrow locker deterministic pass after a code slice lands
- `make -j4 tests`
- `./tests/cata_test "[camp][locker]"`

### Current bandit follow-through slice
- docs-only change -> no compile
- validate by rereading the touched doc/spec packet and its immediately adjacent substrate instead of inventing ceremonial runtime checks

### Fresh tiles rebuild only if a later combat-policy handoff really needs live proof
- `make -j4 TILES=1 cataclysm-tiles`

## Local build caveat

On this Mac, treat top-level `make -j4 tests` as the reliable path for a fresh `cata_test`.
Avoid treating `make -C tests cata_test` as authoritative here; it has been a repeated source of toolchain/stale-build nonsense.
Also: if you actually need a fresh tiles binary, use `make -j4 TILES=1 cataclysm-tiles`; plain `make cataclysm-tiles` is not an honest rebuild path here.
