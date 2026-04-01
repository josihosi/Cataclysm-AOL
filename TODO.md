# TODO

_Practical next steps after the v1 action-status pass._

## Right now / next according to Plan.md

### 1. Turn the status layer into repeatable checks
- [ ] Add 1-3 deterministic action-status fixtures beyond the first attack-target-missing example.
- [ ] Cover at least one pickup blocked case (`pickup.no_path` or `pickup.item_missing`).
- [ ] Cover at least one inventory blocked case (`inventory.item_missing` or `inventory.cannot_wield`).
- [ ] Decide whether `action_status_check.py` should also support ordered phase assertions.
- [ ] Document the intended stable reason-code vocabulary for v1 so tests do not chase moving strings forever.

### 2. Build the smallest useful harness path
- [ ] Create a branch-aware startup profile for `master` first.
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
- [ ] Add one command to run a branch startup smoke test locally.
- [ ] Consider menu/window visibility tooling for harness authoring.

## CI / repo hygiene
- [ ] Watch current `dev` CI after `content: format summary registry json`.
- [ ] If CodeQL is still red, inspect the new failure rather than assuming it is the same one.
- [ ] Decide whether the action-status checker should be wired into CI later, or remain a local dev tool first.

## Can wait a bit
- [ ] Bark cooldown tuning pass (`npc: tune action status barks and cooldowns`).
- [ ] Plumbing cleanup pass (`npc: clean up llm action status plumbing`).
- [ ] Broader curated summary coverage as side work.
- [ ] Richer harness scenario setup via debug menu automation.
