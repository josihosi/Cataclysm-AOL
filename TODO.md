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
   - turn the current locker probe from “load + wait + report honestly” into a stronger locker-specific signal once setup helpers exist
2. stabilize the packaged chat probe and package the next named scenario
   - `chat.nearby_npc_basic` now installs the captured `dev` profile snapshot plus the save fixture so `dev-harness` inherits the LLM/chat options and keybindings it actually needs
   - rerun it on a current tiles binary/runtime path until recipient / `llm_intent.log` proof is honest, or demote the path again if artifacts still refuse to appear
   - `ambient.weird_item_reaction`
3. add the first useful scenario-setup helpers so repeated probes stop depending on debug-menu folklore
   - debug spawn item
   - debug spawn monster
   - debug spawn follower NPC
   - assign NPC to camp
   - assign NPC to follower
4. package one compact Josef-facing testing packet before the pre-holiday active-testing window gets chewed up by setup friction
5. only after the footing is stable, move toward the distinguished hackathon runways
   - chat interface over dialogue branches
   - tiny ambient-trigger NPC model

Still true:
- Locker Zone V1 and V2 stay closed only because their bundled task sets are checked in `SUCCESS.md`
- if current V3/harness work disproves any bundled V1/V2 claim, reopen the affected slice first
- keep V3 from turning into giant NPC-personality soup
- keep the finished Basecamp follow-through closed unless Josef explicitly reopens it
