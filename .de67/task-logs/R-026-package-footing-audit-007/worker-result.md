# R-026 package footing audit — worker result draft

## Outcome

Current package footing is reusable as declarations and historical evidence only; no current-source gameplay witness is established. The smallest next executable route is: build a fresh source-bound `cataclysm-tiles` from current HEAD, retain the disposable `dev-harness`/McWilliams binding, then issue a new exact registry query and run the claim-bound living-NPC route (`r026.living_npc_package_v001_mcw.json`) or Zone Manager route as the selected question requires. No game was launched and no fixture/source was changed.

## Binding and limits

- HEAD: `3ae2efab8830ac49c15d6a6c29163640bc186224`.
- Dirty state at audit: `.de67/work-ledger.md` modified and unrelated untracked `.de67/task-logs/R-MAINT-CHECKPOINT-CONTEXT-closure-integrity-001/`; no binary-relevant worktree changes (`runtime-status` reports none). The committed binary-relevant inputs `src/do_turn.cpp` and `src/handle_action.cpp` differ from the last product build, so the existing executable is not current-source-ready.
- `runtime-status --executable ./cataclysm-tiles --full`: `status=build_required`, runtime source SHA-256 `aeb8bbdb195a8cd10f3e9a36845464e02a9da4a012a457b44f3937bd2120e2cb`, executable SHA-256 `89a724fd5999ae27fa82116e324af5164ca12ad143e0cd962f32471875e9a95a`; old receipt `.userdata/openclaw_harness/source_bindings/cataclysm-tiles-9c12cd9305daf8f1.json` is unmatched/malformed for current status and records product source `4f6921e9...`.
- Historical source-bound receipts/builds remain available, but none is proven against current source. Present executable inventory includes `./cataclysm-tiles` plus retained curses/tiles binaries under `r_surface_*` and `build/r_surface_*`; their receipt source hashes are historical (or missing receipt binding), not a usable current R-026 executable.

## Fixtures, transforms, seeds

- Established-base save manifest is the prepared alias, source fixture `tmp_bandit_live_world_local_contact_raw_2026-04-23`, source profile `live-debug`, transform `repair_basecamp_npc_assignments` for player `#Wm9yYWlkYSBWaWNr.sav.zzip`, NPC IDs 2 and 3, assigned camp OMT `[140,41,0]`. This is setup footing and cannot prove establishment.
- Profile snapshot manifest aliases source snapshot `mcwilliams_live_debug_2026-04-07` / `live-debug`; runtime profile is `dev-harness` (`post_lastworld_wait_seconds=15`, no continue keys).
- `r026.living_npc_package_v001_mcw.json`: fixture `r031_living_non_camp_follower_v1`, derived transform adds nearby follower and clears camp assignment; world/profile `McWilliams`/`dev-harness`; `grants_gameplay_proof=true`; native semantic route includes free text, ambient, basecamp context and save/reload only when state changes.
- `r026.camp_zone_manager_v001_mcw.json`: prepared-base fixture, isolated profile `r026-zone-manager-isolated-20260903`; `grants_gameplay_proof=false`; native Zone Manager enable/disable plus persistence route; copy provenance is explicitly zero credit.
- `harness.living_camp_freeplay_mcw.json`: fixture `harness_basecamp_no_giuseppe_2026-09-05` (removes NPC 4 only), profile `harness-camp-freeplay`, `run_class=non_combat`; `grants_gameplay_proof=false`; exploratory only.

## Existing evidence handles and verdict

Package guide `tools/openclaw_harness/R026_PACKAGE_GUIDE.md` records prepared-base audit run `99528d25e8da4263a1e64337df14e770` (startup/load/inconclusive, not behavior proof), and living-NPC token `5899e6957e17aef803a3848660c8b2b9ecfe6ff0b7110c7e0ea0dc26bf1c735b`, session binding `6c95cfed31958cf53bd9fe5f451e8d9a3845424c07501816c0dcc7359df008ad`, with contradictory recipient/basecamp identity observations (R026-F004). These remain valid at their stated ceilings and must not be relabeled. No prepared-camp establishment is claimed.

## Exact hashes

- Guide `7cc418f5360ad10697dace87767add1b947de18da8b69cea5be9841b8efc5e7e`.
- Seeds: living `1e85e6d1d16475ba43f23ebfbd813324ea2eff0001f33988657c9601f90e19b0`; zone `9e9a98eb8cd38caed854307a91144d8fb523d3ad925836438925eddf11c2b1eb`; freeplay `f357ef88d898beae2b66953038c9fa1df7afb3d221efa37df64c3883cfc21438`.
- Prepared save/profile manifests: `e93757578eee384de460e11be95141e29cb8cb2f2d4089fcbe5d52b810d16649` / `8351ff9f14ed3b40657b57cde0fd41b4f03a30e8b96c827476bac79fda4115af`.
- Build receipt `.userdata/openclaw_harness/source_bindings/cataclysm-tiles-9c12cd9305daf8f1.json`: `785edad70809f8dfbe5c5b67fb491f68b96d370872764b33ff32a41f781fd72b`.

Material uncertainty: runtime source identity is computed by the registry and currently differs from all retained R-026 build receipts; the next build must produce and record a fresh matching receipt before any launch or gameplay credit.
