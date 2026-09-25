# Failed launch preflight and destination reconciliation

- Smoke-first selected token `b0db8d6d1c4d11118eea527f93ff84241c6677ac563bb0f861001f06fbfac1db` was consumed once by detached launch. Record it as failed preflight, not gameplay.
- Session: `.userdata/openclaw_harness/bridge-sessions/selected-5d1ced6b4e2d41c18b74f8dc83676bfb/`.
- Session binding `32b8fd1c68fd6568f4c06b3e521551e7f7f01ae43473295068e3a3fa1c85c874`; bridge PID `17399`; child exit `1`; status `process_dead`, `request_count=0`, `cleanup.status=accepted`. `ps -p 17399` returned no process. Native game did not reach gameplay. PID birth time was not retained by the session status.
- Exact first failure: `Fixture player-items transform target not found: .../.userdata/dev-harness/save/#VGVzdDAw.mm1/#VGVzdDAw.sav.zzip`. The installed-fixture `save/` hierarchy had been staged one level too shallow; transforms require `save/<world>/#VGVzdDAw.sav.zzip`.
- Transcript: session `playtest.txt`; stderr: `child.stderr.log`; state: `status.json`; binding/launch command: `bridge.manifest.json`.

## Destination audit

After the failure, `find .userdata/dev-harness/save -cmin 20` found only the save root, `#VGVzdDAw.mm1/`, and its `.cold/.hot/.warm.zzip` files with recent ctime. All three file hashes match both the archived owner source and the staged copy byte-for-byte:

- cold `515b4556e7101f6021fe7c2ae186a46b63de70d6e8b10d765406c549ce40361b`
- hot `99a4ae49e4abe243388d5454172ad203ab813077b0f342a93d27e854850caff0`
- warm `0f76ffb0fe52acdfc313d06d8dd5468d6e15246b39c873bcd4c20bcbc43306ed`

Their birth and modification times match the owner archive; ctime is 2026-09-24 22:34:13. `maps/` and `overmaps/` retained their prior ctime (2026-08-24 09:25:26; 42 and 2 files), and no other save files had recent ctime. No harness-run or task backup was found. There is no recorded pre-attempt profile snapshot, so prior presence/identity of those same-byte avatar files is unresolved; no restore/delete was attempted. The current destination files equal the archived source bytes and no unequal overwritten data is evidenced.

No further install or launch was run after the failure.

## Corrected isolated launch: first observed native failure and cleanup

- Exact scenario: `cannibal.r_caol_first_smoke_smoke-first_testsetup00`; profile `first-smoke-r-caol-001-smoke`; run ID `efcc2e5f5f194c660f7165de4fd4a5c09ab22cdfc9ea9dc2c62a99dd218ae6e9`.
- Selected token: `efcc2e5f5f194c660f7165de4fd4a5c09ab22cdfc9ea9dc2c62a99dd218ae6e9` (the bridge command run identity); selected scenario ID `6f7f15041e5bb316ce3e95f3558c67f02a29e07fa9bdd8422f3381e6a6d5965b`.
- Binding `2be10f13f03436da2865e4a6bd671de66e8afd87ad77a9c9aba5a351a8cba6db`; executable SHA-256 `a99f455e69bea382401c6255f7fe763534ef42dc0c3e764882948df40efae5ac`; receipt product-source SHA-256 `7e65390983c57c76210659e02b763e77c48ee072e6058494c6c19580b8be1ca5`.
- Process mapping from `ps -p 18460,18461,18488 -o pid= -o ppid= -o lstart= -o command=` and session records: bridge PID 18460, PPID 1, born Thu Sep 24 22:43:12 2026; CLI child PID 18461, PPID 18460, same birth time; game PID 18488, PPID 18461, born Thu Sep 24 22:43:13 2026. Game command is `/Volumes/CodexBulk/Schanigarten/workspaces/Cataclysm-AOL/cataclysm-tiles --userdir .userdata/first-smoke-r-caol-001-smoke/ --world TestSetup00`. GUI window 45773 belonged to PID 18488.
- The current build receipt matched before launch; fixture installation reached the app. There was no HUD or native session descriptor: bridge `request_count=0`, state `starting`, phase `awaiting_startup_hud_or_session_descriptor`.
- Exact exception: `src/item_location.cpp:388 ... Failed to find item_location owner with character_id 2`. `debug.log` stack includes `item_location::impl::item_on_person::ensure_who_unpacked`, `zone_sort_activity_actor::deserialize`, `npc::load`, and `overmap::unserialize`.
- Startup display said press space to continue. A single exact-PID Peekaboo `press space` returned success, but a fresh screenshot showed the same error again. Then non-force `peekaboo app quit --pid 18488` returned success. PID 18488 was absent on subsequent `ps` checks.
- Bridge and CLI child then ended naturally. Final session state in `status.json`: `process_dead`, child exit code 1, request_count 0, reason `pre_descriptor_no_progress`. The bridge first finalized while the game PID was already absent. The explicit cleanup CLI attempted while bridge was still `starting` blocked on the FIFO; it was interrupted after status changed to `process_dead`. A second documented cleanup against terminal state returned `ok:false` with ownership `process_exited_or_identity_changed`; no game or bridge PID remained (`smoke-first-final-ps.txt` is empty). See `smoke-first-bridge-cleanup.json`, final `status.json`, and `peekaboo-bridge-status-after-quit.json`.
- No native smoke or light input occurred. The separate first-light arm was not launched.
- NPC/source comparison is preserved in `smoke-first-npc-reference-audit.json` (SHA-256 `7d87972ae61c25a53b2beb6eb13e5c030ef6062ed108b8779b93c1cbe6ac9475`) and its reproducible command is `audit_smoke_first_npc_refs.py`. It decoded copies of source and fixture `overmaps/o.0.0.zzip` with `startup_harness.extract_overmap_payload`; canonical NPC subtree SHA is identical (`cecf1bda92e4f162cab90d751fb2ae0fb58d550773bc83041096f8168603ed59`). NPC id 2, Tilda Wray, stores ACT_MOVE_LOOT picked-up item locations parented to character id 2. This implicates a saved source-world NPC activity/reference during overmap load; the evidence does not distinguish stale source data from loader ordering behavior.
