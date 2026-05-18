# Bandit + Basecamp first-pass encounter/readability packet v0 (2026-04-22)

Status: CHECKPOINTED / DONE FOR NOW.

## Outcome verdict

This packet is now closed.

The setup-existence question was already closed, and this packet answered the next honest live product question on top of that helper seam:

- the encounter is **mechanically real and immediately dangerous** on current McWilliams/Basecamp footing
- the encounter is **not first-pass readable enough on screen** to feel clean yet
- the main problem is not "can a hostile bandit exist here" anymore; it is that the current restage mostly reads through the right-panel log instead of through strong spatial staging on the playfield

So the right next step is the already-greenlit **Bandit + Basecamp playtest kit packet v0**: repeatability, screen-first artifact polish, cleanup, and better fast-reload playtest footing.

## What the live run actually showed

Fresh current-build proof lives under `.userdata/dev-harness/harness_runs/20260422_144921/`.

The bounded live helper for this packet is now:
- `tools/openclaw_harness/scenarios/bandit.basecamp_first_pass_readability_mcw.json`

That helper reuses the closed packaging seam and captures:
- immediate full/local/right-panel state after spawn
- short-horizon full/local/right-panel state after eight turns
- OCR companions for the right-panel captures

### Immediate read

Immediate on-screen readability is weak.

The spawn mostly lands as:
- one more purple nearby-NPC name (`Heath Griffith`)
- old movement/saving clutter still sitting in the right-panel log
- no especially strong first-pass spatial cue explaining why this new person is the threat

So the first read still feels closer to "debug-spawn puppet show" than to a naturally legible hostile encounter.

### After eight turns

By eight turns, the scene is unmistakably hostile and dangerous.

The same live packet records:
- `Heath Griffith, Bandit gets angry!`
- bandit taunts
- safe-mode survivor ping
- gunfire / impact lines
- `Katharina Leach dies!`
- `Robbie Knox dies!`

That is real pressure, not fake theater.
But it is still communicated mainly through the right-panel log rather than through a clean on-map encounter read.

## Readability split

### Real encounter/readability truth

- A hostile bandit can be intentionally restaged on real Basecamp footing and produce immediate lethal pressure.
- Basecamp context does matter: the scene reads as "camp under attack," not as an empty sandbox duel.
- The first-pass encounter is alive enough to justify more follow-through.

### What still reads as awkward helper/debug baggage

- The immediate hostility read is too dependent on UI text instead of strong spatial staging.
- The first visible clue is subtle enough that it can get buried under unrelated right-panel clutter.
- The strongest proof of danger is currently the later kill-log burst, not a reviewer-clean first glance at the playfield.
- The generic probe artifact verdict (`inconclusive_no_new_artifacts`) is still much less useful than the screen/OCR companions.

## Concrete blockers handed forward

This packet did not find a fresh mechanics contradiction.
It found a **playtest-surface/readability blocker**:

- first-pass hostility is readable too late and too text-log-first
- the artifact surface still needs screen-first packaging
- repeated use still needs less operator clutter and better cleanup
- later human playtesting wants faster reload / clearer scenario footing than the current ad-hoc seam

## Validation

- Fresh rebuild used before live proof: `make -B -j4 TILES=1 cataclysm-tiles`
- Fresh live probe used for the packet: `python3 tools/openclaw_harness/startup_harness.py probe bandit.basecamp_first_pass_readability_mcw`
- No deterministic tests were added or rerun because this packet stayed bounded to live probe / artifact work only

## Position in sequence

This packet is closed enough to stop rerunning it ceremonially.

The baton now passes to:
- `doc/bandit-basecamp-playtest-kit-packet-v0-2026-04-22.md`

That follow-through should improve the live surface rather than reopening the same first-pass readability question from scratch.
