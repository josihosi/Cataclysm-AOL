# Plan

Canonical roadmap for Cataclysm-AOL.

This file answers one question: **what should the agent advance next?**
It is not a changelog, not a graveyard of crossed-off substeps, and not a place to preserve every historical detour.
Rewrite it as reality changes.

## File roles

- **Plan.md** — canonical roadmap and current delivery target
- **SUCCESS.md** — success-state ledger / crossed-off exit criteria for roadmap items
- **TODO.md** — short execution queue for the current target only
- **TESTING.md** — current validation policy, latest relevant evidence, and pending probes
- **TechnicalTome.md** — durable mechanic notes, not daily state tracking
- **COMMIT_POLICY.md** — checkpoint rules to prevent repo soup

If these files disagree, **Plan.md wins** and the other files should be repaired.

## Working rules for agents

- Do **not** mechanically grab the first unchecked-looking thing from some list.
- Follow the current delivery target below and move it to its **next real state**.
- Josef being unavailable for playtesting is **not** a blocker by itself.
- When a target is waiting on Josef, move to the next best unblocked target.
- If no good unblocked target remains, send Josef a short parked-options note so he can greenlight the next lane; do not just keep revalidating the old packet.
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

## 1. Current delivery target — Hackathon runway: stabilization + harness

**Status:** GREENLIT / ACTIVE

Josef now has a real external deadline: OpenAI hackathon in roughly two weeks, with only about five active human-testing days before holiday.
That changes the top job.

Current job:
- make Cataclysm-AOL stable enough that later hackathon work can stand on it without swamp logic
- improve the harness enough that Schani can help meaningfully with playtesting instead of relying on fragile ritual probes
- keep current locker work honest and narrow while doing that, rather than continuing V3 nuance for its own sake

Working priority inside this lane:
1. **stabilization tail on the current locker work**
   - preserve trusted V1/V2 baseline
   - keep V3 honest: finish the current narrow question or demote it back to the honest state
2. **first real harness uplift**
   - establish one reliable live probe path with explicit screen/tests/artifacts boundaries
   - make that path reusable instead of re-invented each run
3. **prepare the runway for the distinguished hackathon features**
   - chat-style interface in place of dialogue-branch soup
   - tiny ambient-trigger model for weird-event NPC reactions

### Immediate next move
- the current locker state is still honest enough to build on: the hot-side Ricky packet is recorded, and the `antarvasa` outcome remains the current one-item-per-slot pants policy rather than a hidden blocker
- the first concrete harness uplift still stands in `tools/openclaw_harness/startup_harness.py`:
  - `probe locker.weather_wait`
  - dedicated `dev-harness` profile + `basecamp_dev_manual_2026-04-02` fixture sourced from `dev`
  - explicit **screen** / **tests** / **artifacts** report split
  - startup screenshots audit the running window title against repo HEAD so stale binaries stop masquerading as current proof
  - direct window-image capture now records `window_id` / `window_index` for startup and probe screenshots, so blocked runs and per-step captures no longer disappear into Peekaboo element-detection timeouts; any remaining version mismatch is now a genuine binary-freshness call rather than screenshot-tool fog
- the chat extension is now in its first actually-proven packaged state instead of the old fake blocker packet:
  - profile loading still merges `tools/openclaw_harness/profiles/master.json` into non-`master` profiles, so shared startup policy reaches `dev-harness`
  - probe contracts can script key/text steps and choose artifact logs instead of only “advance turns + grep debug.log”
  - `chat.nearby_npc_basic` still installs the captured `dev` profile snapshot before the save fixture, so `dev-harness` inherits the saved chat/keybinding state the probe expects
  - the harness now reads LLM artifacts from the repo-level `config/llm_intent.log`, which is where current game code writes prompt/response and ambient logs
  - startup screen audits now distinguish raw repo-HEAD drift from runtime-relevant drift, so docs/harness-only commits no longer falsely invalidate a runtime-compatible captured build
  - the latest packaged chat run at `.userdata/dev-harness/harness_runs/20260406_092352/probe.report.json` now lands `artifacts_matched` with recipient + prompt/response proof for `Bruna Priest`
- keep following `doc/harness-first-slice-plan-2026-04-06.md`, but extend the landed pattern instead of restarting from zero
- next concrete steps:
  - move `ambient.weird_item_reaction` from “runnable but vague” to real reaction proof; the latest packaged run at `.userdata/dev-harness/harness_runs/20260406_092532/probe.report.json` now tails the correct `llm_intent.log` path and honestly returns `inconclusive_no_new_artifacts`, so the remaining gap is the live trigger/staging, not harness bookkeeping
  - keep extending the landed scenario-setup helper wave so the harness can keep moving on unblocked live-probe work
    - `debug_spawn_follower_npc` is already landed on `}`, `s`, `f`
    - `debug_spawn_item` / `debug_spawn_monster` now cover the current wish paths `}`, `s`, `w` and `}`, `s`, `m`
    - `drop_item(...)` now covers the normal inventory drop path (`d`) via either raw slot selection or filtered visible item text
    - `ambient.weird_item_reaction` is runnable on the shipped `basecamp_dev_manual_2026-04-02` fixture; do not keep pretending it is helper-blocked
    - any future assign-NPC helper work is now for alternate restaging / stronger probe variants, not for making the current ambient contract runnable
  - keep `locker.weather_wait` explicitly blocked until a locker-capable fixture/restaging path exists; the shipped `basecamp_dev_manual_2026-04-02` save does not contain a `CAMP_LOCKER` zone, so more no-artifact reruns are just theater
- once one more unblocked probe/helper slice is in place, batch a compact Josef-facing testing packet instead of sending piecemeal asks

---

## 2. Checkpointed — Locker Zone V2

**Status:** CHECKPOINTED / DONE FOR NOW

V2 is now considered done for now because the bundled V2 task set in `SUCCESS.md` is checked:
- managed ranged loadouts can pull up to two compatible magazines from locker supply
- selected compatible magazines can be topped off from locker-zone ammo and the supported weapon reloaded from that supply
- deterministic coverage exists for the V2 contract
- proportional runtime proof is recorded on the current binary

If later code work or runtime evidence shows any one of those bundled claims is false or incomplete, reopen V2 immediately.

---

## 3. Checkpointed — Locker Zone V1

**Status:** CHECKPOINTED / DONE FOR NOW

V1 is only considered done because the bundled V1 task set in `SUCCESS.md` is fully checked.
That bundled close-out is meant to stop false completion:
- locker surface/control exists as a real zone-manager + camp-policy feature
- locker outfitting core exists as real planner/service behavior
- locker maintenance rhythm exists as real dirty/queue/reservation behavior
- V1 has deterministic + proportional runtime proof recorded

If later code work shows any one of those bundled claims is false or incomplete, reopen V1 immediately.

---

## 4. Checkpointed — post-Locker-V1 Basecamp follow-through

**Status:** CHECKPOINTED / DONE FOR NOW

This queue reached its exit criteria for now:
- the board/job log packet is legible enough to compare against the deterministic router proof
- the deterministic board packaging is cleaner/upstream-friendlier
- the richer structured treatment now follows the board-emitted `next=` tokens instead of dropping straight back to spoken bark
- the testing/docs packet describes the closed state instead of an open queue

Keep this closed unless Josef explicitly reopens Basecamp prompt follow-through or a later change breaks the structured board/job lane again.

---

## 5. Checkpointed — LLM-side board snapshot path

**Status:** CHECKPOINTED

This slice reached its exit criteria for now:
- routing proof exists on the actual camp request router, not only on helper builders
- the richer structured/internal `show_board` lane is covered with deterministic evidence
- the short spoken board bark stayed separate
- the testing/docs packet can now describe current truth instead of an open routing question

Keep this out of the active queue unless later code changes break the route again or a new greenlit slice explicitly extends it.

---

## 6. Queued after stabilization/harness

These are not the immediate base-stability job, but they are the likely distinguished hackathon runways once the repo/harness footing is good enough:

1. **Chat interface over in-game dialogue branches**
   - replace dialogue-branch soup with a cleaner chat-style interaction surface
2. **Tiny ambient-trigger NPC model**
   - introduce a small RoBERTa-like model that notices weird situations and triggers ambient NPC reactions

Do not start pretending these are cheap side errands.
They need stable footing first.

---

## 7. Documentation discipline

If the structure starts bloating again, apply this rule:
- `Plan.md` should be readable in a minute
- `TODO.md` should show only the current execution queue
- `TESTING.md` should show only current policy, latest relevant evidence, and pending probes
- `COMMIT_POLICY.md` should stop the dirty tree from becoming a lifestyle

If a sentence exists only to remember that something used to be true, it probably belongs in git history, not here.
