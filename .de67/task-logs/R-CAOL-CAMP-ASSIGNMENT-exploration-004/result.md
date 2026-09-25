# R-CAOL-CAMP-ASSIGNMENT-exploration-004

## Result and scope

Completed non-credit source and registry preparation for a fresh native R-032 camp-assignment run. No game was launched, no GUI input was sent, and PID 90668 was not inspected or operated in this assignment. It remains under coordinator ownership from exploration-003. No gameplay, native assignment, save/reload, ordinary-work, or cleanup claim is made here.

## Charter mismatch and alignment

Before alignment, `tools/openclaw_harness/charters/r032-fresh-camp-establishment-coordinator-brief-rev1.json` required establishing a camp, assigning the same follower, native save/quit/reload, and ordinary work. The witness charter `tools/openclaw_harness/charters/r032-fresh-camp-establishment-rev1.json` covered camp establishment and resulting actor/camp only. The exact pre-alignment charter is preserved at `witness-charter-before-alignment.json` (SHA-256 `1e18d4d3c03e2557ff283a83e873acac3335cd59c4a2275f29e27fc31c171ed5`). The first `registry-query` rejected the pair with: `Cannot proceed: coordinator brief outcome does not match witness charter claim`.

Aligned the witness charter claim to the exact brief outcome. Expanded its proof requirements, contradictions, uncertainties, and stop conditions to require native assignment of the same character ID, authoritative camp identity/owner/lookup OMT, persisted assignment through save/quit/process replacement, and ordinary camp work after reload. This was charter alignment to the existing scenario contract; the scenario was not changed by this task. The original mismatch is preserved above and in the first-query evidence recorded in this report.

The resulting charter claim and brief outcome are byte-for-byte equal as parsed JSON strings. A subsequent `registry-query` selected `r032.fresh_camp_establishment_v001_mcw` and returned a detached launch token. The query was validation/selection only; its launch command was not executed.

## Source-bound premise

Sources inspected: scenario `tools/openclaw_harness/scenarios/r032.fresh_camp_establishment_v001_mcw.json`; fixture manifests `tools/openclaw_harness/fixtures/saves/live-debug/r032_fresh_camp_establishment_follower_v1/manifest.json` and `.../r009_m059_preopportunity_bootstrap/manifest.json`; profile snapshot alias `bandit_basecamp_prepared_base_v1_2026-04-22`; and current source in `src/npctalk_funcs.cpp`, `src/faction_camp.cpp`, `src/overmapbuffer.cpp`, `src/condition.cpp`, `src/npc.h`, plus `data/json/npcs/common_chat/TALK_COMMON_ALLY.json` and the bare-bones camp recipe group/recipe JSON.

Reconstruction of the declared fixture transforms on a temporary copy of the saved payload (no game process) predicts:

- Setup actor: follower character ID `25`, display name `R032 Establishment Ally 1`, faction `your_followers`, alive, no assigned camp, initially guard-ally mission.
- Player and setup follower are both predicted at absolute OMT `[162,36,0]`; terrain there is `forest_thick`.
- That terrain is eligible for `faction_base_bare_bones_basecamp_0` (`om_terrains: ["ANY"]`). The nearby saved bandit special at `[164,39,0]` is terrain `bandit_camp_3_east`, not itself a player basecamp record. The source saved-world basecamp list has no camp in `find_camp`'s ±3 OMT lookup neighborhood for `[162,36,0]`.
- `talk_function::start_camp` evaluates recipes at the actor's current OMT, and `talk_function::assign_camp` calls `overmap_buffer.find_camp(npc.pos_abs_omt().xy())`, then assigns `NPC_MISSION_CAMP_RESIDENT`, adds the assignee, and opens `job_assignment_ui`. `TALK_CAMP` routes to “build a camp here” when `FACTION_CAMP_START` is offered, and to assignment when `FACTION_CAMP_ANY` is offered.
- Predicted camp site/lookup OMT is `[162,36,0]` and camp owner is expected to be `your_followers`; the actual camp identity, displayed name, live terrain/recipe offer, post-creation lookup OMT, resulting mission/assignment, and usable priority must all be read from the native run. These source predictions are setup guidance, not evidence of behavior.

## Historical default-profile launch selection (not safe to launch)

The first registry selection was for the scenario's default profile and is retained as historical prep only. It targeted `dev-harness/McWilliams`, where PID 90668's retained world resides, so its command must not be used.

Exact launch command returned by `registry-query`:

```sh
/Library/Developer/CommandLineTools/usr/bin/python3 /Volumes/CodexBulk/Schanigarten/workspaces/Cataclysm-AOL/tools/openclaw_harness/scenario_registry_cli.py --registry /Volumes/CodexBulk/Schanigarten/workspaces/Cataclysm-AOL/.userdata/openclaw_harness/scenario_registry.sqlite3 registry-detached-launch 2ec05b7a1796a3662f85a72e2a9db5288a6339798e851010b52219cc6181397c --witness-charter /Volumes/CodexBulk/Schanigarten/workspaces/Cataclysm-AOL/tools/openclaw_harness/charters/r032-fresh-camp-establishment-rev1.json --session-dir /Volumes/CodexBulk/Schanigarten/workspaces/Cataclysm-AOL/.userdata/openclaw_harness/bridge-sessions/selected-cc38345fd6a8419b897ffb3cfda34666
```

## Isolated profile route (staged and validated, not launched)

The existing contract supports a profile override on `registry-detached-launch` (accepted as a hidden CLI argument), forwards it through `registry-launch --profile`, and resolves the canonical probe's target profile from that override. `userdir_for_profile` scopes the game userdir to `.userdata/<profile>`; `install_fixture` copies the declared fixture/world into that profile's save directory; `replace_existing_worlds` replaces worlds only there. This scenario declares the fixture source profile and profile snapshot source profile explicitly as `live-debug`, so overriding the target profile preserves the exact saved source chain and does not retarget either source. The profile snapshot contributes config entries while its installer expressly skips `save`; the fresh fixture install supplies McWilliams in the isolated save directory.

I validated profile `r032-camp-assignment-exp004-20260924` in memory through `_registry_launch_probe_namespace(..., cockpit_live_session=True, profile_override=...)`; the canonical probe namespace retained the override and the declared fixture/world/source profiles. Its target world is `.userdata/r032-camp-assignment-exp004-20260924/save/McWilliams`; that userdir did not exist during validation, while `.userdata/dev-harness/save/McWilliams` remained present. A temporary, redirected fixture install staged and removed a complete copy outside the repository. It retained source chain `live-debug:r032_fresh_camp_establishment_follower_v1` → `live-debug:r009_m059_preopportunity_bootstrap`, installed only `McWilliams`, and applied the declared transforms, predicting character 25 at `[162,36,0]` with no assigned camp. Neither the temporary destination nor the existing dev-harness world was changed by this task.

A fresh registry query after charter alignment again selected the exact scenario and reported a ready source executable (`product_build_receipt: matched`). Registry query artifact: `.userdata/openclaw_harness/command-artifacts/registry-query/0e7ce8e2c9bbbe959bd7f8af1441a546cc2e3a6e034db3b18fb459ff41344f4e.json`; manifest ID `5cc04a1cba845c73080b318bc459e5131d6f0cd2b154262a2ef54c95fda890ef`, manifest SHA-256 `4b03860fb0c2a55b2183d1ec4a17fbe217138413d66ab5cfb3094bfdcb6ea879`, product source SHA-256 `3c2c7098eff480d056cb7b9a601a59e5e50cadc185833b6ed5d684307a02f1fa`. The artifact-backed candidate page is retained at `isolated-route-registry-page.json`.

The new query repeated token ID `2ec05b7a1796a3662f85a72e2a9db5288a6339798e851010b52219cc6181397c` and supplied a fresh session directory `.userdata/openclaw_harness/bridge-sessions/selected-435ff50c25214ced9d3f1009ecf2c680`. The token is technical registry state, not user permission or safe launch authorization. Re-query at the later authorized handoff; launch revalidates the token and bindings. The isolated launch command shape, with the fresh query's current token shown for reproducibility, is:

```sh
/Library/Developer/CommandLineTools/usr/bin/python3 /Volumes/CodexBulk/Schanigarten/workspaces/Cataclysm-AOL/tools/openclaw_harness/scenario_registry_cli.py --registry /Volumes/CodexBulk/Schanigarten/workspaces/Cataclysm-AOL/.userdata/openclaw_harness/scenario_registry.sqlite3 registry-detached-launch 2ec05b7a1796a3662f85a72e2a9db5288a6339798e851010b52219cc6181397c --witness-charter /Volumes/CodexBulk/Schanigarten/workspaces/Cataclysm-AOL/tools/openclaw_harness/charters/r032-fresh-camp-establishment-rev1.json --session-dir /Volumes/CodexBulk/Schanigarten/workspaces/Cataclysm-AOL/.userdata/openclaw_harness/bridge-sessions/selected-435ff50c25214ced9d3f1009ecf2c680 --profile r032-camp-assignment-exp004-20260924
```

This command has not been run and the session directory was not pre-created. At the later handoff, use a newly selected token and a fresh, absent session directory, retaining the explicit profile override. The remaining safe prerequisite is Sol's explicit return of exclusive Mac GUI input; PID 90668 can remain untouched because the isolated target save is separate.

## Later Luna observation route

After authorized launch, the scenario's first native action is the bootstrap-required advertised action `world.chat` while state is `world`. This binds the initial world frame and is bootstrap only. In that frame, follow the currently advertised chat/action owner and select `character:25` only if the live semantic selector confirms that exact actor ID and the expected eligibility. Do not infer the live actor from this fixture reconstruction.

Then follow the live `TALK_CAMP` route. Establish the camp through the offered native “build a camp here” path, verify the actual camp identity, owner and lookup OMT, and position the same eligible follower at that camp's actual lookup OMT through native actions if needed. Reopen `TALK_CAMP`, take the offered native assignment route, complete the job-priority UI, and independently observe that the same character ID has `CAMP_RESIDENT` and assigned-camp matching that OMT. Use the scenario's native save/quit and process-replacement steps; after reload verify the same actor/camp assignment and usable ordinary job-priority controls without reassignment.

First divergence branches to preserve: no current registry/source match; missing `world.chat` or actor selector; actor ID/faction/eligibility/position differs; `TALK_CAMP` or bare-bones camp route absent; a no-recipe, nearby-camp, or vehicle-collision rejection; no resulting camp or camp lookup at the actor's OMT; assignment UI fails to change the same actor to resident with matching camp; job priority unavailable; save/quit/process replacement fails; or post-reload actor, assignment, camp match, or ordinary-work controls diverge. Stop the affected proof at its first observed boundary; do not directly mutate state or restage the fixture.

## Checks, artifacts, and evidence ceiling

- `python3 tools/openclaw_harness/r032_fresh_camp_assignment_route_test.py`: 3 tests passed. This checks the scenario's proof route, bootstrap non-credit boundary, and manifest validity; it is not gameplay proof.
- Non-GUI route checks passed: parsed detached `--profile` override reached the canonical cockpit probe namespace; a temporary fixture-install smoke check confirmed the declared source chain and expected single `McWilliams` destination under the isolated profile. Temporary staging was removed automatically.
- `registry-query` rejected the mismatched brief/charter, then selected the exact scenario after charter alignment. The detached launch command/token above is its output.
- Task artifact: this `result.md`. Prior retained-session handoff evidence remains `.de67/task-logs/camp-assignment-recovery-003/recovery.md` and `pid-90668-window.png`, with prior receipt `66db63c582b276cc0d87c1c0282cef7ca91d71d2758fc59660f840ad425efbe8`.
- No native journal entries, game processes, or live bridge session artifacts were created by this task. No new cleanup applies. The detached registry selection is prepared but does not authorize launch; Sol should re-query at the later GUI handoff so launch revalidates current source and binding state.

First open boundary: Sol's explicit return of the exclusive Mac GUI input boundary. At that time select the scenario again and use the isolated profile override to preserve PID 90668's `dev-harness/McWilliams`. Fresh native establishment and same-follower assignment are the first gameplay proof boundary after launch.
