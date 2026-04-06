# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Hackathon runway — stabilization + harness is the active lane.

Execution queue:
1. extend the landed harness slice from `doc/harness-first-slice-plan-2026-04-06.md`
   - keep the reusable `probe` contract path honest on the current binary
   - keep explicit separation of **screen**, **tests**, and **artifacts/logs**
   - keep startup readiness honest after the `lastworld.json` flip; the chat probe already exposed that a plain autoload success signal can still be on the loading splash
   - keep runtime blockers explicit instead of burning more runs on fake-mysterious no-artifact reports
2. extend scenario-setup helpers now that the first one landed
   - keep `debug_spawn_follower_npc` usable/documented on the current debug-menu path (`}`, `s`, `f`)
   - add debug spawn item
   - add debug spawn monster
   - add assign-NPC helper(s) for camp/follower setup
3. strengthen the locker probe into a more direct locker-trigger/setup path
   - turn the current locker probe from “load + wait + report honestly” into a stronger locker-specific signal once setup helpers exist
4. keep the packaged chat probe parked honestly until runner prerequisites exist, then resume it
   - current blocker packet is explicit on current HEAD: `LLM_INTENT_PYTHON` is empty in `dev`/`dev-harness`, and `CATA_API_KEY` is absent for the harness process
   - do not keep rerunning `chat.nearby_npc_basic` blindly until those prerequisites are real
   - after that, close recipient / `llm_intent.log` proof honestly
5. package one compact Josef-facing testing packet before the pre-holiday active-testing window gets chewed up by setup friction
6. only after the footing is stable, move toward the distinguished hackathon runways
   - chat interface over dialogue branches
   - tiny ambient-trigger NPC model

Still true:
- Locker Zone V1 and V2 stay closed only because their bundled task sets are checked in `SUCCESS.md`
- if current V3/harness work disproves any bundled V1/V2 claim, reopen the affected slice first
- keep V3 from turning into giant NPC-personality soup
- keep the finished Basecamp follow-through closed unless Josef explicitly reopens it
