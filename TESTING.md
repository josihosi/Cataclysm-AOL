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

### Locker Zone V1 surface/control reopen

Current honest state:
- Locker Zone V1 is reopened from checkpoint because fresh-save manual testing on 2026-04-07 contradicted the old surface/control close-out.
- Fresh manual evidence from Josef's live `McWilliams` / `Zoraida Vick` session on build `4e3b63650d-dirty` reported three locker-surface problems worth treating as real until disproved:
  - creating `CAMP_LOCKER` threw `Type mismatch in diag_value: requested string, got double`
  - ordinary sorting should protect locker tiles, but the code had no explicit `CAMP_LOCKER` source-tile skip
  - a visible `S` overlay leaked after basecamp/zone interaction and needs triage
- Locker-related code is unchanged between `4e3b63650d` and current `HEAD`, so this is not a docs-only build mismatch excuse.
- Current agent-side progress on the smallest slice:
  - `sort_skip_item` now explicitly refuses to sort items off `CAMP_LOCKER` source tiles
  - deterministic regression coverage exists as `zone_sorting_leaves_camp_locker_tiles_alone` in `tests/clzones_test.cpp`
  - the reported `diag_value` crash was reduced to one concrete locker path in current code: locker downtime skip logging was reading numeric `camp_locker_last_service_turn` with `diag_value::str()`
  - that locker skip path now formats the stored turn with `to_string( false )`, and deterministic regression coverage exists as `camp_locker_skip_probe_handles_numeric_last_service_turn` in `tests/faction_camp_test.cpp`
  - a fresh harness recheck of the real Zone Manager locker-creation path on `basecamp_dev_manual_2026-04-02` now succeeds on current code (`locker.create_zone_probe`) with no visible popup and no stuck `S` after closing the zone manager
  - a second live harness probe on that same fixture (`locker.display_toggle_probe`) shows that explicitly toggling zone display on a `CAMP_LOCKER` zone leaves visible storage-style `S` markers after closing the manager, which reduces the overlay report toward latched zone-visibility state rather than an automatic locker-creation leak
- Latest validation evidence for this reopen:
  - `git diff --check` passed
  - `make -j4 tests` passed
  - `./tests/cata_test "zone_sorting_leaves_camp_locker_tiles_alone"` passed
  - `./tests/cata_test "camp_locker_skip_probe_handles_numeric_last_service_turn"` passed
  - `python3 tools/openclaw_harness/startup_harness.py probe locker.create_zone_probe` passed on `profile=dev`, `fixture=basecamp_dev_manual_2026-04-02`
  - `python3 tools/openclaw_harness/startup_harness.py probe locker.display_toggle_probe` passed on `profile=dev`, `fixture=basecamp_dev_manual_2026-04-02`; **screen:** after explicit display toggle, closing Zone Manager leaves visible `S` markers over the locker tiles, **artifacts/logs:** no dedicated artifact contract, so the packaged report honestly stayed `inconclusive_no_artifact_match`

What counts next:
- compare the clean harness result against Josef's reported `McWilliams` live-session failure and reduce whether the remaining mismatch is save/path-specific
- determine whether the reported visible `S` on `McWilliams` came from that same latched zone-visibility state or from a separate rendering/state bug

### Patrol Zone v1 baseline

Current honest state:
- Patrol Zone v1 remains checkpointed / done for now.
- No new patrol probe is queued unless later code or runtime evidence reopens that lane.
- The current live packets and deterministic proof still stand as the last closed lane, not the active one.

### Existing baseline that should not be mistaken for locker proof

- Earlier locker V1/V2 runtime proof still supports the non-surface outfitting / maintenance spine, but it does **not** cover the newly reported fresh-save locker surface regressions.
- The repo already has one honest packaged proof format that separates **screen** / **tests** / **artifacts** (for example `locker.weather_wait` at `.userdata/dev-harness/harness_runs/20260406_125056/probe.report.json`). Treat that as a packaging template only, not as proof that the current locker surface is healthy.
- Current harness scenarios `chat.nearby_npc_basic` and `ambient.weird_item_reaction` remain hackathon-reserved scaffolding. They are not part of this locker-surface reopen.

### Meaning

- The repo is not parked anymore.
- The active question is locker surface/control honesty on the fresh-save path, not patrol close-out.
- If the locker story starts sounding cleaner than the current code/tests/live evidence, stop and audit.

---

## Pending probes

### Active queue

1. compare the clean harness locker-creation recheck against Josef's reported `McWilliams` live-session failure
2. triage whether the reported visible `S` on `McWilliams` came from that same latched zone-visibility state or a separate bug on that save/path

Still true:
1. the live `McWilliams` save should become the preferred harness/manual baseline only after Josef closes the session and the save can be snapshotted safely
2. keep the helper idea `sustain_npc` available only if a future live probe genuinely needs it

### Anti-hallucination rule for this lane

Do not treat any of these as success by themselves:
- only proving the locker planner/service internals while the locker UI surface still misbehaves on the fresh-save path
- only writing docs/spec text
- only adding one narrow code guard without rerunning the relevant regression test once the build allows it
- only explaining away the `S` overlay without determining whether it is just a mode toggle or a real render/state bug

If the story starts sounding cleaner than the evidence, stop and audit.

**Hackathon-reserved — do not touch before the event:**
1. **chat interface over dialogue branches**
   - current `chat.nearby_npc_basic` evidence is harness-only scaffolding, not feature implementation
2. **ambient-trigger reaction lane / tiny ambient-trigger NPC model**
   - current `ambient.weird_item_reaction` evidence is harness-only scaffolding/observability, not feature implementation

**Parked discussion topics, not current code lanes:**
1. reopen **Locker Zone V3** for one deliberately narrow next judgment slice after the surface/control reopen is closed again
2. **smart zone manager** after locker surface truth is restored

### Active-lane handoff block

- **active lane:** Locker Zone V1 surface/control reopen
- **active slice:** locker sort protection first, then zone-creation crash audit, then `S` overlay triage
- **last closed lane:** Patrol Zone v1 still stands as the last closed lane with deterministic + live proof
- **Josef ask:** none right now; keep working agent-side until a real try-now packet or a genuinely blocked choice appears

### Non-blocking Josef notes

- No new Josef ask yet for the locker-surface reopen.
- Once the fresh-save `McWilliams` session is safely closed, snapshot it cleanly for the harness instead of copying a moving live save.

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
