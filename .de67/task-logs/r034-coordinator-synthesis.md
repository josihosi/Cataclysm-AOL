# R-034 coordinator synthesis

## Outcome

The assigned positive outcome is proved at a focused native evidence ceiling: after a real
save/quit and process replacement, the same McWilliams world exposed the saved camp, accessible
storage, and Katharina Leach's durable camp/follower state; later ordinary native turns were
accepted. The separately observed unaddressed free-text behavior is retained as an observation
only. It is not an authorized formal repair finding: R-034 did not require that invented control
property, no duplicate storage-item mutation was observed, and any gameplay repair requires
Josef's promotion after causal code proof.

## Bound lifecycle and source

- Scenario: `r026.living_npc_package_v001_mcw`.
- Source: `aeb8bbdb195a8cd10f3e9a36845464e02a9da4a012a457b44f3937bd2120e2cb`.
- Executable: `/Volumes/CodexBulk/Schanigarten/workspaces/Cataclysm-AOL-hostile-ecology-dev/cataclysm-tiles`, SHA-256 `aa25a0fc58fdb80db1898e4fb7b26df58b675a4c24e8f1bf3292ae1d47596ff7`.
- Native run/binding: `d7929b2e62413e227fde942eebe5e00bcab02f9c5bc8639c48b41be41ae8bdd8` / `3a5dbb5f23ae3a7334e22ae8233e67a96453fb9c29a72fd6cbc682ba86505dd2`.
- Original process: instance `1788807543644737`; generation 0. The native `world.quicksave`
  reported `saved`, then `world.save_quit` and `main_menu.quit` were accepted; the retained
  process-exit receipt reports code 0.
- Relaunch: instance `1788807628778897`; generation 1. It opened the same McWilliams save with no
  fixture reinstall or restaging. The first post-reload native frame was bound through the
  semantic world bootstrap.
- A request made from the prior frame was rejected as `unknown_or_stale_observation`; after new
  diagnostics, the stale UID was not attempted. See `r034-stale-frame-request.json` and
  `.userdata/openclaw_harness/bridge-sessions/selected-ebe80aaebe94445faeda26e4ef48777c/responses/play-28bdaece8ed24e9db9169d8a118f2c72.json`.

## State comparison and resumed behavior

The pre-save native segment and the post-reload journal bind the following values:

- World/camp/zone: McWilliams; current camp `Bugchaser central`; camp site `[140, 41, 0]`;
  accessible `Basecamp: Storage` zone.
- Storage boundary: seven original entries remained, including `novel_coa`,
  `money_strap_one`, `wallet`, `eyedrops`, `bag_plastic`, `can_aluminum_250ml`, and `box_small`;
  the cited first/last entries remained count 1.
- Actor: `character:2`, Katharina Leach; mission `CAMP_RESIDENT`; assigned camp `[140, 41, 0]`;
  `camp_patrol_order=false`; priority `8`; diagnostic inventory count `17`;
  `follow_close` enabled with `override_enabled=true`.
- Accepted post-reload ordinary actions include `world.pause` at J0010 and J0023. J0013 and the
  later world observation retain the actor/camp/storage boundary, demonstrating resumed ordinary
  behavior without claiming movement or separation.
- The later unaddressed free-text submission at J0021 was accepted. J0022 records a second
  Katharina craft response and the ordinary `R031 Route Ally 1` response; the storage boundary
  remained unchanged. This is preserved as a control observation with the structured native
  uncertainty noted in the witness: no structured camp-request ID is exposed for that utterance.

## Tracked scenario change and evidence limits

`tools/openclaw_harness/scenarios/r026.living_npc_package_v001_mcw.json` is the only relevant
tracked scenario change. The post-relaunch first step now uses
`native_semantic_bootstrap` (`world.wait`, required state `world`) and removes the former
screen-wait/OCR abort guard. Rebuild/query artifacts are `r034-rebuild.log` and `r034-query-2.json`;
the resulting manifest SHA is `1e85e6d1d16475ba43f23ebfbd813324ea2eff0001f33988657c9601f90e19b0`.

OCR, terminal bytes, and rendered text are presentation-only and cannot prove accepted input,
elapsed game time, gameplay state, or checkpoints. All credited claims above use native semantic
receipts, authoritative state/transition observations, and exact run/request/frame/process/
generation identities. Any OCR reading was corrected only as presentation context and receives no
proof credit.

The packaged probe at
`.userdata/dev-harness/harness_runs/20260907_210017_4c7d947c923f41368defec95f42300b9/probe.report.json`
is not the acceptance witness: registry ingest returned `invalid_report` because the playtest
witness was not bound to its immutable journal. It must not be edited or fabricated. The locally
validated native journal/post-witness bundle and archived cockpit evidence therefore define the
honest evidence ceiling.

The durable native records are retained in `r034-original-segment-witness-2.json`,
`r034-post-journal.json`, `r034-post-witness-bundle.json`, and the selected bridge-session
request/response journal; the post bundle's accepted witness is
`play-e9e2f76cd7be4b78ae5487926bfa9e01`.

## Cleanup

Scenario terminalization collected `safe_to_cleanup`; the relaunched process was already exited.
The cleanup report records status `already_exited`, with no remaining live game process claimed.
