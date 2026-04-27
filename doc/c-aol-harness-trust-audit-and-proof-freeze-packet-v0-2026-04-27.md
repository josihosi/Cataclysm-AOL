# C-AOL harness trust audit + proof-freeze packet v0 (2026-04-27)

Status: ACTIVE / GREENLIT PROCESS AUDIT.

## Why this exists

Josef watched the current harness work and identified the real product risk: the harness can load the game, close it, and still let Andi believe a feature was proven. That is not a small missing assertion. It means the harness/procedure can fail at the keystroke or setup layer while the report still sounds green.

Smoke attraction currently gets a narrow live credit from Josef. The rest of the recent harness-driven claims must not inherit trust from that one working behavior. The audit exists to make the harness itself observable enough that future feature proof is not folklore with screenshots sprinkled on top.

This is deliberately a large packet. It should be executed by Andi with Frau Knackal reviewing the proof matrix and false-pass traps where possible.

## Classification and placement

- Classification: active / greenlit process package.
- It became the next active lane after `C-AOL debug-proof finish stack v0` reached its completed Schani-review boundary.
- It does block future confidence in harness-based closure claims: a run that only proves startup/load must be labeled startup/load proof only.
- When this packet becomes active, it should be treated as infrastructure repair, not as a side errand tucked under a feature fix.

## Scope

### 1. Inventory the whole harness surface

Build a reviewer-readable inventory of the harness skill and implementation surfaces before changing behavior. At minimum inspect and classify:

- `/Users/josefhorvath/.openclaw/workspace/skills/c-aol-harness/SKILL.md`
- `tools/openclaw_harness/startup_harness.py`
- `tools/openclaw_harness/profiles/*.json`
- any scenario registry, control lookup, report writer, screenshot helper, save/profile copier, zzip/map/save transform, and live GUI driving helper used by current C-AOL proof runs
- existing named scenarios used for Smart Zone, bandit smoke/fire/light, bandit local-contact/standoff, shakedown, basecamp locker/zone checks, and startup/load checks

The inventory must say what each primitive claims to do, what observable artifact proves it, and what failure mode would otherwise be invisible.

### 2. Freeze one save/profile policy instead of save roulette

Use the same canonical save/profile for the audit whenever possible. If a feature family genuinely needs a different fixture, the report must justify the split and record the provenance.

Required save policy:

- name the canonical audit profile/save and why it is safe to mutate or copy;
- record source commit, profile file, world name, character/save id, and copied-run directory for every proof run;
- prefer copied disposable run state over mutating Josef's real playtest state;
- preserve a small set of stable fixture saves rather than silently generating a new mystery save for every proof;
- if the current save cannot support a primitive, mark the primitive blocked-by-fixture instead of pretending the feature failed or passed;
- never use the contaminated old McWilliams/bandit macro as generic closure proof unless the packet explicitly says that exact contamination is part of the scenario.

### 3. Prove every step, not just the final screen

For each harness primitive or scenario, build a step ledger. Every row must contain:

1. precondition / expected starting screen or save state;
2. exact command, keystroke, menu action, save transform, or script primitive;
3. expected immediate state change;
4. screenshot checkpoint or exact game metadata/log field proving the state change;
5. failure detection rule;
6. artifact path;
7. whether the next step is allowed to run after this proof.

This applies to every little step. Loading into the game is one step. Opening debug/spawn menus is another. Typing an item id is another. Confirming the spawn happened is another. Placing a brazier, placing wood, lighting it, creating `fd_fire`, creating `fd_smoke`, opening Zone Manager, generating Smart Zones, inspecting the zone layout, waiting several hours, saving, reloading, and exiting are all separate proof points.

No scenario may claim feature proof if one of its pre-feature primitives silently failed.

### 3a. Expand information extraction before rerunning the blocked UI seam

The current blocked primitive is `blocked_untrusted_brazier_deploy_selector`: the fixture inventory contains the required brazier/wood/lighter items, but the live GUI path has not yet proven that the `brazier` row was selected, that activation entered `Deploy where?`, or that the direction key was consumed by that prompt. Do not keep trying key variants.

Before rerunning this part of the audit, add a small observation primitive that can prove the UI/menu state. Preferred exploration order:

- harness-gated selector/menu trace for inventory use, returned item type/id/name/invlet or selection method, prompt text, and direction-choice consumption;
- checked GUI text extraction or OCR regions for menu header, filter prompt, item list/selected row, and bottom prompt;
- richer saved-player item metadata for activatable/deploy target details such as `deploy_furn` -> `f_brazier`;
- terminal/PTY ASCII scraping only as separately labeled recipe discovery, not as SDL GUI closure proof.

Current observation result: run `.userdata/dev-harness/harness_runs/20260427_202434/` proves the `Use item` selector opens and saved-item metadata records `brazier` as `deploy_furn -> f_brazier`, but the filter state `brazier` still does not prove a highlighted/returned brazier row. Follow-up run `.userdata/dev-harness/harness_runs/20260427_203847/` adds selector-entry metadata and proves the live `Use item` list contains only `smart_phone` before filtering and zero entries after filter `brazier`. The next guard is still `filtered_brazier_selected`, but the immediate blocker is now the fixture-to-live inventory availability gap: saved `player.inv` prerequisites are not live selector-row proof. Repair/prove that gap before any further confirm/right attempt.

The deploy row only turns green when it proves the relevant menu/selector state and saved east-tile `f_brazier`; fuel/lighter/fire proof remains blocked behind that row.

### 4. Audit high-risk primitives first

The audit should prioritize primitives that already caused or masked false confidence:

- game launch, focus, title/load flow, world selection, and character load readiness;
- screen capture naming, timing, and image provenance;
- menu navigation and keystroke delivery, including stuck focus and wrong-screen detection;
- debug spawn paths for items, furniture, terrain, fields, NPCs, monsters, and weather/time manipulation;
- exact target-tile proof after spawn/transform actions;
- zzip/save/map transforms and version-39 save-shape assumptions;
- same-save copy/restore logic and `.mm1` / `world/` readiness;
- report generation, success/failure classification, and artifact summaries;
- Smart Zone generation and visible layout inspection;
- brazier/wood/lighter/fire/smoke setup and live field verification;
- wait/sleep/turn advancement, survival needs, safe mode, interruption handling, and game clock metadata;
- NPC/bandit positioning, local contact, active-member ids, route/path state, return-home clocks, and writeback fields.

### 5. Turn false passes into explicit red states

The harness must know when a setup step failed. If it spawns something and the game did not actually create it, the report should say so immediately. If it pressed a key but remained on the wrong screen, the report should say wrong screen. If a metadata field is missing, the report should say missing metadata, not success by omission.

The frozen workflow must include red/yellow/green classifications:

- green: step proved by screenshot and/or exact metadata;
- yellow: step ran but only proves setup, seam, or fixture behavior;
- red: expected state change absent, contradictory, or unobserved;
- blocked: fixture/tooling prevented the step from being honestly tested.

### 6. Update and freeze the operator workflow

After the audit, update the harness-facing docs/skill surfaces so later agents do not re-derive the ritual:

- update `skills/c-aol-harness/SKILL.md` if the proof workflow or scenario rules change;
- update repo harness docs such as `tools/openclaw_harness/CONTROL_LOOKUP.md` or scenario docs if the audit adds/replaces named procedures;
- write the frozen proof contract in repo docs so future Plan/TODO/SUCCESS/TESTING claims can cite it;
- keep the workflow strict enough that Andi cannot accidentally treat load-and-close as feature proof again.

## Non-goals

- Do not use this packet to redesign Smart Zones, bandit AI, Basecamp jobs, or combat behavior by stealth.
- Do not mutate Josef's personal playtest state as the audit substrate.
- Do not build a giant opaque macro that produces prettier lies.
- Do not replace live/product proof with deterministic tests when the claim is explicitly about screen-visible behavior.
- Do not demand screenshots for purely deterministic unit tests where exact assertions are already the stronger proof; use screenshots for GUI/live harness claims and metadata for game-state claims.
- Do not silently split into many new saves/profiles just because the same-save discipline is inconvenient.

## Success state

This packet is good enough when:

- [ ] A full harness-surface inventory exists and names each current primitive/scenario, its proof artifact, and its known blind spots.
- [ ] One canonical disposable audit save/profile is selected for the majority of checks, with explicit provenance and justified exceptions for any other fixture.
- [ ] Each audited primitive has a step ledger with precondition, action/keystroke, expected state, screenshot or metadata proof, failure rule, artifact path, and pass/yellow/red/blocked verdict.
- [ ] The audit covers at least launch/load readiness, focus/keystrokes, screenshot capture, debug spawn paths, target-tile metadata checks, save/zzip transforms, Smart Zone UI/layout inspection, fire/smoke setup, wait/time passage, NPC/bandit positioning, and report writing.
- [ ] At least one known previously fragile chain is walked end-to-end with proof at every step: e.g. load game -> prepare target tile -> spawn/place brazier/wood/lighter or equivalent -> verify object/field metadata -> perform action -> verify result -> wait/save/report.
- [ ] False-pass behavior is demonstrated or guarded against: wrong screen, failed spawn, missing target field, missing save metadata, stale binary/profile, and load-only runs all produce explicit non-green verdicts.
- [ ] The frozen workflow states that load-and-close is startup proof only, and no feature package may close from it.
- [ ] The relevant harness skill/docs are updated so another agent can run and audit the workflow without chat archaeology.
- [ ] Frau Knackal or an equivalent review pass checks the proof matrix for contradictions, hidden fixture bias, and claims that outrun their evidence.

## Validation expectations

The final report should contain:

- one compact proof matrix listing every audited primitive and scenario;
- named screenshot checkpoints for GUI/live steps, with enough labels to understand the sequence without opening a hundred random PNGs;
- exact metadata/log fields for game-state claims, especially spawns, map fields, zone geometry, NPC ids, clocks, and save/writeback state;
- a same-save/provenance section explaining which save/profile was used and why any exception was necessary;
- a red/yellow/green/blocked summary;
- a list of harness primitives that remain untrusted and therefore cannot support closure claims yet;
- a short frozen workflow section to copy into `skills/c-aol-harness/SKILL.md` / harness docs if not already patched.

## Handoff packet for Andi

When activated, Andi should start with the audit, not with another feature retry. Recommended order:

1. inventory skill/docs/scripts/scenarios;
2. choose canonical disposable same-save/profile policy;
3. build the step-ledger template and artifact naming convention;
4. audit launch/load/focus/screenshot primitives first;
5. audit item/furniture/field spawn and target-tile metadata primitives;
6. audit one Smart Zone UI/layout path and one fire/smoke setup path;
7. audit wait/save/reload/report-writing primitives;
8. update skill/docs and ask Frau Knackal to review the matrix before closing.

If Andi finds that a primitive fails, the correct output is a red primitive and a small repair or follow-up, not a fake green feature proof. Das ist kein Hexenwerk; it is just not allowed to lie politely anymore.
