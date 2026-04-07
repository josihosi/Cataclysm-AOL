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
- Package 1 is now partly reduced on real screenshots instead of guesswork:
  - repeated `locker.zone_manager_save_probe_mcw` runs proved the closeout path back to gameplay is `Esc` -> save prompt -> uppercase `Y`
  - the old early-name-entry story was wrong; typing before or during zone-type selection is not the right surface
  - the remaining honest blocker is narrower now: the harness still leaves the default `Basecamp: Locker` name unchanged because the edit-name handshake is not yet landing on the actual rename field
  - latest screenshot packet: `.userdata/dev-harness/harness_runs/20260407_234435/`
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

1. **Package 1** on the current McWilliams harness path:
   - finish the edit-name handshake so the intended custom zone name actually lands
   - keep the proven closeout path (`Esc` -> save prompt -> uppercase `Y`) intact while fixing rename timing/control
   - confirm the renamed zone persists after returning to gameplay
2. once Package 1 is landed or honestly blocked, move to **Package 2** (`basecamp toolcall routing fix`) as the next isolated slice

Still true:
- ordinary chat / ambient harness footing should stay on the captured `McWilliams` / `Zoraida Vick` save, not drift back to the older default fixture
- Package 2 stays next on purpose, do not bury the wrong-snapshot bug under locker feature creep
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
- **active slice:** Package 1, harness zone-manager save-path polish
- **next slice:** Package 2, basecamp toolcall routing fix
- **last closed lane:** Patrol Zone v1 remains checkpointed, McWilliams patrol clustering now matches the expected 2-cluster save layout
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
