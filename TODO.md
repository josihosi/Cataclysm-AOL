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
2. keep the scenario-setup helper wave honest now that the first slice is landing
   - keep `debug_spawn_follower_npc` usable/documented on the current debug-menu path (`}`, `s`, `f`)
   - keep `debug_spawn_item` honest on the current wish path (`}`, `s`, `w`) and its filter/amount prompt behavior
   - keep `debug_spawn_monster` honest on the current wish path (`}`, `s`, `m`) and its look-around target confirm behavior
   - keep `drop_item(...)` honest on the normal inventory drop path (`d`) for both filtered visible-text selection and raw one-slot selection
   - keep `ambient.weird_item_reaction` honest as runnable on the shipped fixture; do not relabel it blocked just because stronger restaging helpers would be nice
   - only add assign-NPC helper(s) if they unlock a concrete fixture-restaging or stronger-probe need
3. repair the locker probe contract before spending another live run on it
   - the shipped `basecamp_dev_manual_2026-04-02` fixture does **not** contain a `CAMP_LOCKER` zone, so `locker.weather_wait` is currently blocked on a locker-capable fixture/restaging path
   - keep that blocker explicit instead of burning more runs on honest-but-useless `no_artifact` reports
4. move the packaged ambient probe from “honest but empty” to first real reaction proof
   - `ambient.weird_item_reaction` now tails the correct repo-level `config/llm_intent.log`; the latest packaged run at `.userdata/dev-harness/harness_runs/20260406_092532/probe.report.json` is an honest `inconclusive_no_new_artifacts`
   - do not regress this contract back to `debug.log` or fake repo-HEAD blocker logic
   - next step is live trigger/staging work: improve the weird-item setup, wait window, or nearby-NPC positioning until ambient target/response artifacts actually appear
5. package one compact Josef-facing testing packet before the pre-holiday active-testing window gets chewed up by setup friction
   - `chat.nearby_npc_basic` is now good packet material via `.userdata/dev-harness/harness_runs/20260406_092352/probe.report.json`
6. only after the footing is stable, move toward the distinguished hackathon runways
   - chat interface over dialogue branches
   - tiny ambient-trigger NPC model

Still true:
- Locker Zone V1 and V2 stay closed only because their bundled task sets are checked in `SUCCESS.md`
- if current V3/harness work disproves any bundled V1/V2 claim, reopen the affected slice first
- keep V3 from turning into giant NPC-personality soup
- keep the finished Basecamp follow-through closed unless Josef explicitly reopens it
