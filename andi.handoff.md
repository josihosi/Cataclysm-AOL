# Andi handoff: Smart Zone Manager v1 Josef playtest corrections

## Classification and position

- classification: ACTIVE / GREENLIT NOW
- authoritative canon: `Plan.md`
- active queue: `TODO.md`
- success ledger: `SUCCESS.md`
- validation policy: `TESTING.md`
- packet doc: `doc/smart-zone-manager-v1-josef-playtest-followup-2026-04-26.md`
- just-closed prior packet: `doc/bandit-local-sight-avoid-and-scout-return-cadence-packet-v0-2026-04-26.md`
- repo: `/Users/josefhorvath/Schanigarten/Cataclysm-AOL` on `dev`

## Why this is active

`Bandit local sight-avoid + scout return cadence packet v0` now has deterministic proof plus bounded live/harness proof for both scout return/writeback and `sight_avoid: exposed -> repositioned`. The next greenlit stack item is the narrow Smart Zone Manager correction Josef identified after checking generated zones in the UI.

## Prior packet closure to preserve

Do not reopen the local-stalking packet by drift. The stable evidence packet is:

```sh
python3 tools/openclaw_harness/startup_harness.py probe bandit.local_sight_avoid_exposed_mcw
```

Run `.userdata/dev-harness/harness_runs/20260427_061344/` uses `bandit_local_sight_avoid_exposed_v0_2026-04-27` on equivalent nearby-owned-site local-contact footing, disables safe mode, advances 20 turns, and logs `bandit_live_world sight_avoid: exposed -> repositioned npc=4 from=(60,23,0) to=(59,22,0) reason=repositioning because exposed`.

Scout return/writeback proof remains `.userdata/dev-harness/harness_runs/20260427_054353/`, with `scout_report: returned -> pressure refreshed`, active outing fields cleared, and member `4` back home. These prove the local packet; they do not claim later redispatch tuning beyond explicit camp re-evaluation.

## Active implementation target

Read `doc/smart-zone-manager-v1-josef-playtest-followup-2026-04-26.md`, then inspect the Smart Zone Manager generation code/tests. Desired narrow corrections:

- add `LOOT_MANUALS` on/near the Basecamp books cluster
- preserve ordinary `LOOT_BOOKS`
- preserve gun-magazine `LOOT_MAGAZINES`, with clearer label if current text can be confused with readable magazines/manuals
- add full Basecamp storage `AUTO_EAT` and `AUTO_DRINK`
- ensure both auto-consume zones have `ignore_contents == false`

## Validation plan

Start with deterministic zone-id/type/option tests for the generated layout. Use one focused harness/save inspection only after the static shape is honest and only if it answers persistence/reopen risk. No broad Basecamp automation rewrite, no Lacapult, no release publication, and no user-data mutation.
