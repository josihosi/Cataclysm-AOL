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

### Controlled locker / basecamp follow-through packet

Current honest state:
- The active lane is now a controlled follow-through packet, not a broad locker/basecamp reopen blob.
- Ordinary chat / ambient harness footing now points at the captured `McWilliams` / `Zoraida Vick` save instead of the older Sandy Creek default.
- The current McWilliams debug pass has already been reduced into five explicit packages in:
  - `doc/locker-basecamp-followthrough-work-packages-2026-04-07.md`
- Those packages are:
  1. harness zone-manager save-path polish
  2. basecamp toolcall routing fix
  3. locker outfit engine hardening
  4. locker zone policy + control-surface cleanup
  5. basecamp carried-item support + dump lane
- **Package 1** is now landed on real screenshots/artifacts instead of guesswork:
  - the zone name must be entered at creation time, not later through a separate edit-name handshake
  - the closeout path back to gameplay is a single `Esc` to open the save prompt, then uppercase `Y`
  - reopening Zone Manager after returning to gameplay still shows the custom `Probe Locker` entry on the current McWilliams path
  - latest screenshot packet: `.userdata/dev-harness/harness_runs/20260408_005852/`
- **Package 2** has its first deterministic reduction landed in code:
  - camp-request routing no longer keys off bare `assigned_camp`
  - the current gate now accepts explicit `FACTION_CAMP` role-id workers and stationed camp guards/patrol guards, while still rejecting `GUARD_ALLY` hold/follower state
  - deterministic coverage for that discriminator lives in `tests/faction_camp_test.cpp`
  - current deterministic recheck passed on this Mac via `./tests/cata_test "[camp][basecamp_ai]"`
- The current McWilliams live probe path needed one honest harness fix before it could be trusted again:
  - startup on the McWilliams save was still declaring success while the window sat on the lingering load screen
  - the harness now sends the same post-load `Enter` continuation key that cleared the real window into gameplay on this path
  - fresh live-debug startup proof now reaches gameplay at `.userdata/live-debug/harness_runs/20260408_020648/success.png`
- With that live path repaired, the ordinary-hearer repro is no longer showing the old wrong-snapshot/toolcall behavior on the current save:
  - manual live captures after startup are recorded in `.userdata/live-debug/harness_runs/20260408_020648/`
  - nearby hearers `Katharina Leach` / `Robbie Knox` answered in ordinary spoken bark form for freeform `show me the board` and `craft 1 bandage`
  - those captures did **not** show structured board/toolcall payload text such as `board=show_board`, `planner_move=`, `overmap:`, `details=`, or `next=`
- Stale-binary suspicion on the same live path was real and is now cleared:
  - an initial Package 2 talk-menu probe came back `inconclusive_version_mismatch`, so the current tiles binary was rebuilt from repo HEAD
  - fresh rebuild log: `build_logs/package2_live_rebuild_20260408_retry1.log`
- The old "no nearby camp-duty hearer" read was too optimistic about the problem shape:
  - the current live save already gives us nearby hearers `Katharina Leach` and `Robbie Knox`
  - a fresh literal `show_board` probe at `.userdata/dev-harness/harness_runs/20260408_033437/` still sent both hearers through the ordinary nearby-hearer LLM prompt path
  - that run did **not** emit a deterministic camp board reply artifact such as `camp board reply`, `board=show_board`, or `planner_move=`
  - the current live gap is therefore beyond the first routing gate: the next seam is the runtime hearer-state/grouping split, not missing access to any nearby hearer at all
- Patrol sanity on the current McWilliams save is already checked: the serialized patrol tiles currently resolve to **2 clusters** under 4-way connectivity, so that note no longer belongs in the active mystery pile.
- The right current discipline is:
  - one package at a time
  - revalidate before widening
  - do not let Andi or the repo drift into a broad opportunistic locker rewrite

### Existing baseline that should not be mistaken for the active package

- Patrol Zone v1 remains checkpointed, the current McWilliams patrol-cluster check is a sanity confirmation, not a reopened patrol lane.
- The harness swap to `McWilliams` / `Zoraida Vick` for ordinary chat/ambient probing is real, but it does **not** by itself prove the basecamp toolcall-routing bug is fixed.
- The debug-intake pass and the Andi work-package draft are useful planning evidence, but neither counts as code/test closeout on their own.

### Meaning

- The active question is not "how do we fix all locker/basecamp weirdness at once".
- The active question is "what is the next isolated package that honestly improves the system without wrecking the working loop".
- If a proposed step starts blending harness polish, wrong-snapshot routing, locker outfit hardening, control-surface design, and inventory policy into one blob, split it before coding.

---

## Pending probes

### Active queue

1. **Package 2** on the current McWilliams harness path:
   - ordinary nearby-hearer live recheck is still in hand: the current freeform-hearer repro no longer shows the old wrong-snapshot/toolcall lane for ordinary hearers on the live save
   - the current literal `show_board` live probe is also in hand, and it still leaves both nearby hearers on the ordinary LLM path instead of emitting a camp board reply
   - the next step is to inspect the runtime hearer-state/grouping seam that still keeps `Robbie Knox` out of the basecamp-aware route on this path
2. if that runtime-state audit still fails or exposes a new seam, isolate that next discriminator or payload seam without widening into locker work or inventing fake staging too early

Still true:
- ordinary chat / ambient harness footing should stay on the captured `McWilliams` / `Zoraida Vick` save, not drift back to the older default fixture
- Package 3 stays next on purpose, do not bury the wrong-snapshot question under locker feature creep
- `sustain_npc` remains only a helper idea for later probes if some future live packet honestly needs it

### Anti-hallucination rule for this packet

Do not treat any of these as success by themselves:
- swapping the ordinary harness fixture to `McWilliams`
- writing a clean work-package document
- observing plausible-looking NPC gear motion on-screen once
- describing a nice locker inventory/outfit policy in prose
- partially fixing one package while quietly widening two others behind it

If the packet sounds cleaner than the active package boundary or evidence underneath it, stop and trim it.

**Hackathon-reserved — do not touch before the event:**
1. **chat interface over dialogue branches**
   - current `chat.nearby_npc_basic` evidence is harness-only scaffolding, not feature implementation
2. **ambient-trigger reaction lane / tiny ambient-trigger NPC model**
   - current `ambient.weird_item_reaction` evidence is harness-only scaffolding/observability, not feature implementation

**Later discussion topics, not current code lanes:**
1. **smart zone manager**
2. any broader locker/inventory redesign beyond the active package order in the auxiliary doc

### Active-lane handoff block

- **active lane:** controlled locker / basecamp follow-through packet
- **active slice:** Package 2, basecamp toolcall routing fix
- **next slice:** Package 3, locker outfit engine hardening
- **last closed lane:** Package 1, harness zone-manager save-path polish, is now landed on the McWilliams harness path; Patrol Zone v1 remains checkpointed
- **Josef ask:** none right now beyond keeping the packet narrow and one-package-at-a-time

### Non-blocking Josef notes

- The current debug pass already did the hard work of separating real bugs from design requests and future ideas.
- Keep using that packetized structure. If future notes start piling up again, collect first, then reduce to packages again before handing anything to Andi.

---

## Reusable commands

Use these when they are actually the missing evidence, not as ritual.

### Fresh full test rebuild on this Mac
- `make -j4 tests`

### Fresh tiles relink on this Mac
- `make -j4 TILES=1 cataclysm-tiles`

### Patrol-focused deterministic check
- once patrol tests exist, run the narrowest patrol-specific `cata_test` filter or named case instead of the whole suite

### Startup/load smoke for later live patrol proof
- `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'`

## Local build caveat

On this Mac, treat top-level `make -j4 tests` as the reliable path for a fresh `cata_test`.
Avoid treating `make -C tests cata_test` as authoritative here; it has been a repeated source of toolchain/stale-build nonsense.
Also: if you actually need a fresh tiles binary, use `make -j4 TILES=1 cataclysm-tiles`; plain `make cataclysm-tiles` is not an honest rebuild path here.
