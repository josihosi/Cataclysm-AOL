# TODO

_Practical next steps after the v1 action-status pass._

## Right now / next according to Plan.md

### 1. Turn the status layer into repeatable checks
- [ ] Add 1-3 deterministic action-status fixtures beyond the first attack-target-missing example.
- [ ] Cover at least one pickup blocked case (`pickup.no_path` or `pickup.item_missing`).
- [ ] Cover at least one inventory blocked case (`inventory.item_missing` or `inventory.cannot_wield`).
- [ ] Add panic/morale override coverage so pickup and attack do not look mysteriously flaky when panic supersedes intent.
- [ ] Add ordered phase assertions to `action_status_check.py` so fixtures can verify more than just set membership.
- [ ] Document the intended stable reason-code vocabulary for v1 so tests do not chase moving strings forever.

### 2. Build the smallest useful harness path
- [ ] Create a branch-aware startup profile for `master` first.
- [ ] Add harness-owned fixture save install/capture management.
- [ ] Launch the game automatically.
- [ ] Reach menu readiness reliably.
- [ ] Load a known save without manual keystrokes.
- [ ] Record success/failure with screenshot + harness log.

### 3. Add first real action smoke scenarios
- [ ] Scenario: targeted pickup success.
- [ ] Scenario: targeted pickup blocked.
- [ ] Scenario: `look_inventory` blocked/success case.
- [ ] Scenario: `attack=<target>` target-lost or no-viable-attack case.

## Josef / manual testing only when unavoidable
These are the things worth human eyes, because they are annoying to automate first.

- [ ] Verify blocked pickup bark feels sensible and not spammy.
- [ ] Verify blocked inventory bark feels sensible and not spammy.
- [ ] Verify attack waiting states (`aiming`, `advancing`, `reacquire_grace`) read sensibly in debug output.
- [ ] Verify action-status log lines are understandable during one real gameplay sequence.
- [ ] Verify no obviously absurd NPC chatter appears from the new terse refusal barks.

## Harness / automation work
- [ ] Decide where harness branch profiles live (`data` file vs script table).
- [ ] Pick artifact layout for screenshots + logs.
- [ ] Add one command to run a deterministic status-check fixture locally.
- [x] Add one command to run a branch startup smoke test locally.
- [ ] Consider menu/window visibility tooling for harness authoring.
- [ ] Capture the current `master` / `Sandy Creek` save as the first harness-owned gameplay fixture.
- [ ] Add an in-game smoke path for `Sandy Creek` using Ricky Broughton as the first talk target.
- [ ] Add an in-game smoke path for `C+b` utterance entry + submit and verify that logs / action output still look sane.
- [ ] Add a field probe for `Ricky, kill the cow.` and verify attack engagement from message-log/screenshot evidence.
- [ ] Add a field probe for `Ricky, pick up the gun and axe on the ground.` and verify partial/complete pickup behavior from message-log/screenshot evidence.
- [ ] Add a debug-menu helper path for spawning Rubik (`}`, `s`, `p`, `O`) once startup-to-gameplay is stable enough.

## Immediate in-game smoke ideas from the current save
- [ ] `C`, `t` → talk to nearby NPC Ricky Broughton.
- [ ] In Ricky's chat window, press `a`, then `a` again when the quest-first variant is present, to open the ruleset window and confirm normal chat UI still works.
- [ ] `C`, `b` → enter a player utterance, submit it, and confirm we still reach the expected conversation/log path.
- [ ] Figure out the minimum reliable precondition checks for those tests:
  - is Ricky nearby?
  - is the correct chat window focused?
  - do we need to scrape visible chat text before sending follow-up keys?
  - for `C+b`, which NPC actually receives the utterance in mixed ambient/follower situations?
- [ ] Decide whether chat-window contents should be read via screenshot/OCR or via a more direct observable signal.
- [ ] Investigate whether we can detect in-game menu/dialog/gameplay state more directly than screenshots (or at least with a stronger observable than blind keying).
- [ ] Keep `tools/openclaw_harness/CONTROL_LOOKUP.md` updated as more pragmatic automation key paths are discovered.
- [ ] Audit raw keybind vs typed-character semantics for harness input so gameplay keys do not misfire into things like throw mode.

## CI / repo hygiene
- [ ] Watch current `dev` CI after `content: format summary registry json`.
- [ ] If CodeQL is still red, inspect the new failure rather than assuming it is the same one.
- [ ] Decide whether the action-status checker should be wired into CI later, or remain a local dev tool first.

## Can wait a bit
- [ ] Bark cooldown tuning pass (`npc: tune action status barks and cooldowns`).
- [ ] Plumbing cleanup pass (`npc: clean up llm action status plumbing`).
- [ ] Broader curated summary coverage as side work.
- [ ] Richer harness scenario setup via debug menu automation.
