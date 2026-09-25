# Frozen agent backlog — ignore during DE-67 runs

This is a human maintenance note with no DE-67 runtime authority. Routers, coordinators, workers,
reviewers, and watchers must not read, execute, summarize, mutate, or copy it into project `.de67/`
state. Only the repository owner or an agent directly tasked with maintaining this note may change it.

## Pending

### Playtest token consumption — human maintenance queue (2026-09-18)

Owner: Josef; explicitly promote individual items before implementation. This is a
parked maintenance queue, not a new worker assignment or permission to change live runs.
Implement in the order below. Harness paths are relative to
`/Volumes/CodexBulk/Schanigarten/workspaces/Cataclysm-AOL-hostile-ecology-dev`;
Telescope paths are relative to this de67 repository.

Evidence: September 16 trace of Terra worker `01a0a87a-17d2-74e0-ae34-d072dc3043aa`.
Counts below are command occurrences, not proven redundant work or token savings.
The broader audit is `docs/token-audit-2026-09-16.md`.

- [ ] **1. Keep pending-response waits inside one tool execution.**
  Target: `tools/openclaw_harness/play_cli.py`, `PlayerClient.collect` and
  `--wait-seconds` (currently defaults to one second), plus the playtest worker's
  invocation guidance. Observed 245 collect commands; 70 corresponding outputs
  still contained pending state. First use the existing wait option within the
  task's remaining deadline; keep shell launch and necessary process polling in
  one `functions.exec`. Only add runtime glue if that route cannot do the job.
  Return on response, terminal process/bridge failure, or deadline; never resubmit
  the action. Check delayed success, process death and deadline expiry. Compare
  model turns spent waiting on the same delayed-response workload before/after.

- [ ] **2. Remove redundant action-to-look round trips.**
  Target: playtest worker invocation guidance and `play_cli.py` response collection
  around `gameplay_display.display`; `gameplay_display.py` already emits deltas.
  Observed 508 look commands. An explicit `game.observe` forces a display refresh.
  Inspect consecutive action-to-look pairs and reuse the action's returned frame
  when it provides valid current authority and the needed facts. Keep look for
  missing/stale frames, reentry and real refresh needs. Do not blindly batch moves.
  Check prompt transitions, rejected actions, reentry and stale-frame recovery;
  compare observation calls and displayed bytes on the same action sequence.

- [ ] **3. Make filtered, projected evidence the default investigation example.**
  Target: `play_cli.py:PlayerClient.evidence`, `evidence_events.py:query` and worker
  examples. Use exact run/request/actor/event filters and `--select` before paging
  raw records. For rejection diagnosis project identity, time, error, accepted,
  rejection_reason and outcome fields; recover larger details through source handles.
  Do not include whole nested receipts when individual fields suffice. In the live
  sample, 12 original response files were 410,922 bytes; all 16 projected event rows
  were 28,117 bytes, without model selection. These are bytes, not model tokens.
  Check that the projected answer preserves the same acceptance/rejection meaning,
  identities and verifiable citations; absent fields must remain explicitly unknown.

- [ ] **4. Keep Jev out of deterministic lookups.**
  Target: `integrations/jev_telescope/README.md`, discovery guidance in
  `de-67-3/scripts/instruction_context.py`, and any future harness routing glue.
  Route known request IDs, explicit failure flags and exact rejection strings to
  existing deterministic filters. Reserve Telescope for ambiguous semantic evidence
  selection and contradictory explanations. Trial: 12 submitted candidates occupied
  19,685 bytes; Jev selected six failure rows occupying 10,684 bytes, using 15,498
  input plus 819 output provider tokens, with no fallback. That is NOT evidence of
  net savings. Compare exact-filter and Jev routes on the same frozen evidence;
  count provider tokens, agent fresh tokens and follow-up retrievals together.

- [ ] **5. Improve the adapter's candidate pool before increasing its limits.**
  Target: `integrations/jev_telescope/harness_adapter.py:search` and
  `test_harness_adapter.py`. The trial snapshot contained 16 rows but only the first
  12 were submitted; truncation was correctly reported. Build a question-specific
  shortlist after exact identity filters; deduplicate equivalent evidence before
  provider submission, preserving distinct observations, time and contradictions.
  Use small explicit field projections. Do not silently drop unmatched rows or
  impose an arbitrary product cap. Test a relevant late row, an early irrelevant
  prefix, duplicates, absent matches and counterevidence. Measure coverage and
  total cost; fewer selected rows alone is not success.

- [ ] **6. Carry a small continuation note across context resets.**
  Target: playtest worker continuation/recovery guidance and its existing checkpoint
  surface; identify that surface before adding storage. Observed 26 controls commands
  (~168K output characters); the earlier audit counted 40 context resets for this
  worker. Retain current session/binding, pending request ID, evidence handles,
  unresolved question and next decision. Reuse static controls guidance, but verify
  current advertised actions and evidence freshness. Test resumption with a pending
  request and with a changed session generation: neither may replay input or reuse
  stale authority. Compare repeated controls/source reads across a context reset.

Completion evidence for each promoted item: comparable completed playtest outcomes,
correct citations/contradictions and native authority, fresh agent tokens plus Jev
tokens, tool/model-turn counts, follow-up reads and elapsed time. Keep comparisons
local and read-only where possible. Do not claim improvement from lower hourly burn
caused by doing less useful work. No game build is needed for this backlog entry.

- Add a low-maintenance, read-only de67 dashboard for the repository owner.

  Purpose and boundary:

  - Host a small home-network website on the machine that owns the long-lived de67 workspaces.
  - Show the authoritative `.de67/FS.md`, active and blocked `.de67/work-ledger.md` items, and a
    compact projection of SQLite clock, deadline-generation, restart, supervisor, and optional
    sidecar state.
  - Do not use a coordinator, worker, observer, or language-model call to render or refresh it.
  - Do not edit de67 state from the site, create a second source of truth, add required ledger
    fields, add coordinator rituals, or make dashboard health a de67 blocker.
  - Keep public hosting, remote code execution, a large frontend framework, and automatic user
    deployment outside the first version.

  Recommended architecture:

  - Use a standalone read-only process that reads the FS and ledger directly and queries SQLite
    through a strictly read-only connection and transaction.
  - Build an immutable in-memory projection and atomically swap it only after a refresh attempt has
    produced a usable result. Keep the last good projection for each source independently.
  - Do not require an on-disk generated snapshot in the first version. A persisted cache may be
    considered later, but it must stay explicitly disposable and non-authoritative.
  - Serve small server-rendered HTML. A manual-refresh page should have effectively zero idle work;
    optional browser refresh may be configured explicitly without becoming de67 policy.
  - Run under process supervision independent of the de67 supervisor. Removing, breaking, or
    restarting the dashboard must have no effect on de67.

  Source and rendering rules:

  - Read each Markdown file as one byte snapshot. Decode tolerantly, record source identity and
    observation time, and detect when the file changes during observation.
  - Render headings, paragraphs, lists, checkboxes, code fences, and basic tables conservatively.
    Unknown or incomplete Markdown falls back to escaped text; never pass raw source HTML through.
  - Escape every value from Markdown, SQLite, paths, findings, restart reasons, and error messages.
    Use a content-security policy that disallows scripts, frames, remote assets, and inline event
    handlers.
  - Query SQLite with read-only mode, `PRAGMA query_only`, and one consistent read transaction. Do
    not instantiate the operational deadline harness merely to render state because its open path
    may initialize or migrate schema.
  - Present the immutable original claim clock, appended deadline generations, active task clock,
    restart generation, and supervisor/process observation as different facts. Do not flatten them
    into one ambiguous deadline.
  - Show each panel's observed time, source hash or identity, freshness, and partial/stale/error
    state. Markdown and SQLite cannot share one transaction, so the page must not imply perfect
    simultaneity.

  Failure behavior:

  - A malformed or partially written FS or ledger leaves its last good panel visible with a clear
    stale/error notice while other panels continue updating.
  - A locked, missing, replaced, newer-schema, or temporarily unavailable database leaves the last
    good clock panel visible and never waits behind or writes to the operational owner.
  - A missing workspace produces a healthy unavailable page. If no last-good view exists after a
    service restart, show an empty unavailable state rather than failing the service.
  - Refresh requests share one refresh operation instead of multiplying filesystem and database
    reads during active churn.
  - Display exceptions, service crashes, restart loops, and complete dashboard absence never stop,
    restart, mutate, or block de67.

  Home-network access:

  - Bind to loopback by default. Home-network exposure is an explicit owner configuration.
  - Prefer an authenticated home-network or Tailscale HTTPS endpoint in front of the loopback
    service. Direct unauthenticated LAN binding exposes specifications, paths, findings, and blocker
    text to every device on that network and should not be the default.
  - Keep authentication and TLS concerns outside the de67 runtime and outside the authoritative
    workspace state.

  Smallest honest vertical slice:

  - One standard-library service, one configured workspace, and one server-rendered overview.
  - FS status and claim navigation; active and blocked ledger items; current task and deadline
    generation; compact restart/supervisor state; per-source freshness and errors.
  - Immutable in-memory last-good projection, loopback-only initial service, no workspace writes,
    and no OpenClaw dependency.
  - Focused controls for partial files, invalid UTF-8, incomplete Markdown, unknown fields, HTML and
    script payloads, source changes during reads, SQLite locks, missing/replaced/newer databases,
    parser exceptions, workspace disappearance, service restart, and concurrent refreshes.
  - Prove that dashboard reads do not initialize, migrate, or modify SQLite and that de67 continues
    when the dashboard is absent or repeatedly broken.

  Packaging direction:

  - Develop and validate the dashboard in `de67-lab`, then promote it deliberately into release
    `de67` as an optional integration.
  - The dashboard and OpenClaw adapter may share a top-level `integrations/` home and packaging
    conventions, but they remain separate install units and processes. The dashboard is passive;
    the OpenClaw adapter receives external owner authority and therefore has a different trust and
    failure boundary.
- Add an owner-contact route for blockers that require a material user decision. Preserve unattended
  progress on other unblocked work, and do not treat ordinary worker findings as reasons to contact
  the owner.
- Revisit the C-AOL light-and-zombie package: player light, Writhing Stalker, and Zombie Rider.

  Source-bound audit:

  - Read-only inspection of the current Mac hostile-ecology `dev` working-tree bytes at
    `c6616640a768625ce2673de56f336ae387ee0fc9` on 2026-09-03. Recheck references after rebases.
  - Keep this as a deferred light-and-zombie lane. It has no DE-67 runtime authority and does not make the
    current helper tests or debug-spawn scenarios production gameplay proof.

  Verdict:

  - Both concepts have useful local policy pieces, but neither currently proves the full physical
    light -> abstract travel -> reality-bubble handoff -> characteristic local behavior trajectory.
  - Treat movement ownership and multi-action behavior as the first problems. Tuning numbers before
    those are true would polish behavior that can contradict its own decisions.

  Shared player-light requirement:

  - The player's actual illuminated position must publish a shared physical light point on the
    overmap. This includes active flashlights and other lights carried or worn by the player, which
    the current ground-item/field/terrain/vehicle scan omits (`src/do_turn.cpp:6553-6633`).
  - Illumination cast onto the player by another physical source must use the same representation.
    Derive intensity, exposure, and range from the actual light state rather than treating possession
    of a named item as sufficient.
  - Every intended overmap entity must be able to observe the same point through common physical
    range and occlusion rules. The point carries light facts, never player identity, faction knowledge,
    or a guaranteed target; actor-specific policy decides the response.
  - Movement must update or coalesce the point, switching the light off must expire it honestly, and
    unload/save/load must not leave permanent breadcrumb lights or duplicate a moving player source.
  - Prove held-versus-dropped parity for the same active light, externally illuminated player tiles,
    movement across OMT boundaries, contained versus exposed buildings, light-off decay, and multiple
    actor consumers from one source record.

  Writhing Stalker findings:

  - An unloaded stalker can receive the generic `ZOMBIE`/`FERAL` plus `HEARS` horde signal, but the
    dedicated planner runs only after generic loaded target acquisition. There is no dedicated
    abstract stalker-light state (`src/horde_map.cpp:125-239`, `src/monmove.cpp:1648-1689,1997-2001`).
  - The creature has no actual pounce. A `strike` increments the burst count during planning and sets
    a destination; it does not require a landed attack. From three tiles away it can spend the default
    burst merely approaching and then withdraw (`src/monmove.cpp:1096-1128`,
    `src/writhing_stalker_ai.cpp:448-469`).
  - `hold`, `interested`, and `ignore` unset the destination, after which generic scent or wander
    movement can immediately advance anyway (`src/monmove.cpp:1138-1145,2325-2407`).
  - The live latch is rebuilt without stored age and advanced with zero elapsed minutes; the field
    named cooldown-minutes decreases per planner invocation. Shadow and retreat validate endpoints,
    not complete routes (`src/writhing_stalker_ai.cpp:725-789`, `src/monmove.cpp:825-989`).

  Zombie Rider findings:

  - Dedicated light selection scans loaded `g->all_monsters()` only. Abstract riders use the generic
    horde signal without the dedicated cap, so the nominal 36-OMT query and two-rider cap do not govern
    one actor population (`src/do_turn.cpp:7234-7351`, `src/zombie_rider_overmap_ai.h:14-19`).
  - `band_formed` currently means that two riders were selected for one light, not that they met.
    There is no durable band identity, and loaded IDs are derived from changing positions
    (`src/zombie_rider_overmap_ai.cpp:151-218`, `src/do_turn.cpp:7242-7258`).
  - Open ground fails the current direct-attack opening heuristic, while nearby impassable clutter can
    satisfy it. The default result for two healthy riders in open ground is circle/harass rather than
    aggressive attack (`src/do_turn.cpp:7129-7152`, `src/zombie_rider_overmap_ai.cpp:232-268`).
  - Bow HIT_AND_RUN writes a four-turn retreat, but the next custom plan can replace it with an inward
    pressure destination. Light intent lasts 90-300 turns while scans occur every 300 turns, allowing
    periodic policy snapping (`src/mattack_actors.cpp:1549-1561`, `src/monmove.cpp:1403-1645`,
    `src/zombie_rider_overmap_ai.cpp:62-149`).

  Later acceptance checks:

  - Light: start from a real illuminated player, create one source-bound overmap point for every
    intended consumer, move and extinguish it, and preserve exact source lifetime and physical
    visibility through OMT movement, bubble transitions, and save/load without identity leakage.
  - Stalker: begin abstract, answer a physical light only at the intended short range, survive both
    bubble-entry routes, shadow through route-valid darkness, land a real distraction/lone-target
    strike, commit to egress, and preserve/expire intent honestly through unload and save/load.
  - Rider: begin abstract, enforce one responder cap across representations, require physical meeting
    before durable band formation, attack aggressively in open ground, preserve the complete bow
    retreat/reload rhythm, distinguish a real defended obstacle or breach, and survive light-off plus
    unload/save/load without stale or duplicate ownership.
  - Exercise both handoffs: the entity walks into the loaded map, and the player loads the entity's
    submap. The latter currently has an explicit destination-synchronization TODO
    (`src/overmap.cpp:1606-1624`, `src/overmapbuffer.cpp:2219-2271`).
