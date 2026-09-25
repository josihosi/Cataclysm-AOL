# Isolated profile and route readiness (no launch/install)

The task-owned fixture payloads now have the expected `save/<world>/...` layout. Their contained world is `TestSetup00` (45 files). The staged copy tree hashes remain the values recorded in `camp-transplant.json`; no source archive files were edited.

Both scenario declarations use unique profiles. The exact resolved save destinations from `save_dir_for_profile(profile)` are listed below; both profile roots were absent before this readiness query and remain absent afterward. `TestSetup00` is therefore absent at either install destination. The declarations currently say `replace_existing_worlds=true`; if a launch is later authorized, the installer will replace only a same-named world beneath the respective isolated profile. No install was run.

## smoke-first

- Scenario: `cannibal.r_caol_first_smoke_smoke-first_testsetup00`; query selected id `6f7f15041e5bb316ce3e95f3558c67f02a29e07fa9bdd8422f3381e6a6d5965b`.
- Profile: `first-smoke-r-caol-001-smoke`; isolated destination `/Volumes/CodexBulk/Schanigarten/workspaces/Cataclysm-AOL/.userdata/first-smoke-r-caol-001-smoke`; profile exists: `False`; destination world exists: `False`.
- Fixture payload: `/Volumes/CodexBulk/Schanigarten/workspaces/Cataclysm-AOL/tools/openclaw_harness/fixtures/saves/live-debug/first_smoke_smoke-first_testsetup00_v1/save/TestSetup00`; manifest SHA-256 `d39ea1437aa2709eda07aed13ad54771c77c8ffb9186fd99039d1f6283dba366`.
- Registry query artifact SHA-256 `86946a3a33f5e0f8fdb771d3b79e14f10167b61f6c1ec724a6eac86ce6332ea8`; next action is `launch_selected_scenario`; fresh launch token remains unused.

## first-light

- Scenario: `cannibal.r_caol_first_smoke_first-light_testsetup00`; query selected id `4148b8a0a145af509f48a994f19e6ceeea222eccf8e7128df531765add74b333`.
- Profile: `first-smoke-r-caol-001-light`; isolated destination `/Volumes/CodexBulk/Schanigarten/workspaces/Cataclysm-AOL/.userdata/first-smoke-r-caol-001-light`; profile exists: `False`; destination world exists: `False`.
- Fixture payload: `/Volumes/CodexBulk/Schanigarten/workspaces/Cataclysm-AOL/tools/openclaw_harness/fixtures/saves/live-debug/first_smoke_first-light_testsetup00_v1/save/TestSetup00`; manifest SHA-256 `a5b729f79486578a24a6e19bc4092f9f7f3ef90b9ee6ee4b647a6a356eb8dfca`.
- Registry query artifact SHA-256 `c53861f90f27b47e723fd5f9b4ef8af88d7f94a7f27a16e496a8f54107dc3678`; next action is `launch_selected_scenario`; fresh launch token remains unused.

Current runtime-status is `ready` with reason `receipt_bound_dirty_product_build`; executable SHA-256 `a99f455e69bea382401c6255f7fe763534ef42dc0c3e764882948df40efae5ac`. Full runtime receipt is `runtime-status-isolated.json`.

No game process is running. No launch or fixture install occurred after the failed shared-profile attempt.
