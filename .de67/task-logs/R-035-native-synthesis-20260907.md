# R-035 native flesh-raptor evidence synthesis

Date: 2026-09-07 (Europe/Vienna)

## Scope and evidence rule

This closes the assigned fresh native test of flesh-raptor orbit, swoop,
blocked-corridor fallback, and encounter feel. Writhing stalkers and zombie
riders are out of scope. OCR, terminal bytes, and rendered text are not used
as proof of input, time, state, or gameplay. PTY receipts below prove only
that the run-bound terminal accepted the requested bytes; native semantic
frames and the source-emitted debug artifact prove the resulting game state
and behavior.

## Runtime identity

All three runs used the same source-bound curses executable:

* executable: `.userdata/openclaw_harness/r035-curses/cataclysm`
* executable SHA-256: `2c0cfe7e998d01783989e2faca6e08260bb34f37282302fa95649f8f8b387b53`
* runtime source SHA-256: `aeb8bbdb195a8cd10f3e9a36845464e02a9da4a012a457b44f3937bd2120e2cb`

The hashes are recorded in each run's `runtime.binding.json`.

The native event-stream SHA-256 values are, respectively, open
`131608150333f29203d088cd5efc5096c731714be48d47a91f5ef5700ebc1cc3`, crowded
`b0dd30f56d248dc7ae7132c7cfb877cff1e182e47c9ec05c3fb4f8e68f44c517`, and
blocked `715ca687561f6fdedb3620e9819c9aa9dd8a284294f78404488e820d2d9d365b`.
The corresponding debug-log SHA-256 values are open
`50e3e31c0ee796a7e9ac3e39492515addd0b9b9b0345534329d42f0482476c60`, crowded
`a2dcbf6842522d4fc1669cd893067df6fe9e88b1eeb4aa9dd72c29bd1e5b6728`, and
blocked `eab8ce4f478335a864ad69effe16b8879fcc24fe5fc2fec2f7c71bdb937d060f`.

## Run correlations

### Open field

* run: `21e5af319f1f49d88a29ce04c51ed86a`
* run directory: `.userdata/r035-open-transform2/harness_runs/20260907_201320_97bbd612ebf6440e894c3304e6c513dd`
* native frame range with visible `mon_spawn_raptor`: `...:5227200:1` through `...:5227301:266`; initial state is player `[3372,996,0]`, raptor `[3377,996,0]`, HP 45; final native frame `...:5227301:266` still reports the raptor at `[-1,-2]`, HP 45
* representative native log lines: `config/debug.log:1724-1728` (`best_open_orbit_arc`, distance 5, crowding 0), `:2922-2961` (cadence and committed swoop), `:2975` (melee hit, damage 10, target HP 90%, `run_after=yes`)
* representative accepted run-bound request: `terminal.input.dcb9be574efc494ca918a976b0be631b.receipt.json` (`!`, request `dcb9be574efc494ca918a976b0be631b`); the delayed native follow-up was accepted by the same endpoint and is correlated to the above native frame range, but its PTY receipt is not persisted as a run file

An earlier retained open-field attempt was also owned by this assignment and
is now closed: run `3df0b9a98468400dbf6873ded5b4c940`, directory
`.userdata/r035-open-transform/harness_runs/20260907_201211_cd031ccb3eaa4c48bb607218e868bcd5`,
game PID 83001, broker PID 83002, endpoint
`/tmp/caol-pty-c0af92b6ec955666af8a66c7.sock`. Native frames recorded the
save-and-quit prompt at `...:frame:143`, world return at `...:frame:144`,
main menu at `...:frame:147`, and quit confirmation at `...:frame:148`;
`...:frame:149` remained at the main menu after confirmation. The exact
run-bound cleanup requests were S `9ee0284299b240cfb2d2cf21540cf245`, y
`bb238f7e3b254dd3b772d0d66f9df172`, return
`d26f68177fbd4c469758c09d5fb47f1d`, Q
`3f3c5272df1b44b2951c472b4022d976`, return
`283a433c05504c8ca4107110db4b2995`, y
`12e44840048e4c529ce206ed5be84b56`, and return
`ab5a4b2b53e94e80ba53837926066dae`. A post-cleanup process check found both
PIDs absent and the endpoint removed. This attempt used the same executable
and runtime-source hashes above; its native event stream SHA-256 is
`fd9994a6d5b0d0ad97fe062b840160f594a155d743c76cb1c212fb5e59baf2f7`.

### Crowded north arc

* run: `48137afbec214894bc6b151916bb4c9d`
* run directory: `.userdata/r035-crowded/harness_runs/20260907_201814_b7f13b8da5c54d109d673d51876371e9`
* native frame range with visible `mon_spawn_raptor`: `...:5227200:1` through `...:5227219:163`; initial state is player `[3372,996,0]`, raptor `[3377,996,0]`, HP 45; final native frame `...:5227219:163` reports the raptor at `[2,2]`, HP 45
* delayed run-bound request receipts: `!` request `c994d4be76c04e229d951386b0a470a`; dot bundles request `994cc288fc4a41daa1b50c1be308f084` through `e059911d54c040ff96cdb4f4da42c91a` (all `ok=true`, same run and PID 83619)
* native log lines: `config/debug.log:1495-1616` (`best_open_orbit_arc`, selected `chosen_rel=-1,5`, distance 5, crowding 0); `:1642-1672` (cadence and committed swoop); `:1686` (melee hit, damage 7, target HP 98%, `run_after=yes`)
* result: the raptor avoided the zombie-occupied north side in favor of the under-occupied south/open arc, then swooped and produced a concrete combat outcome

### Blocked corridor

* run: `7e2fee73419a49de9af93c64ed32aa74`
* run directory: `.userdata/r035-blocked/harness_runs/20260907_202104_7cce4b2850564900a8eae0a4470126d4`
* native frame range with visible `mon_spawn_raptor`: `...:5227200:1` through `...:5227218:102`; initial state is player `[3372,996,0]`, raptor `[3377,996,0]`, HP 45; final native frame `...:5227218:102` reports the raptor at `[2,0]`, HP 45
* delayed run-bound request receipts: `!` request `7af4b5ad5ef043909f0d6d2d050b6b72`; dot bundles were accepted by the same endpoint (PID 83955, same run)
* native log lines: `config/debug.log:895` (`decision=fallback`, `reason=no_readable_lateral_orbit`, `candidates=3`, lane `[5,0]`); `:1033-1152` (cadence and committed swoop); `:959` (melee hit, damage 6, target HP 99%, `run_after=yes`)
* result: lateral orbit was rejected as unreadable, the raptor fell back to the straight open lane, and the encounter continued into swoop/melee pressure without orbit jitter

## Controls, setup, and limits

The three fixture manifests now explicitly clear the east approach tiles
`[3,0]..[6,0]` to `t_floor`/`f_null`, preserving the blocked fixture's rack
blockers elsewhere. Saved metadata audits prove noon turn 5227200, raptor
spawn at east offset `[5,0,0]`, crowded zombies where applicable, and blocked
rack/open-lane footing. These are setup controls, not credit for natural
behavior.

The packaged probe reports remain `blocked_*_live_plan_missing` because the
launcher finalized before delayed manual PTY input. That is a harness timing /
observation limitation, not a contradictory gameplay result. The authoritative
native artifacts above were collected after explicit run-bound input and are
the basis for this synthesis. Screen capture/OCR failures are retained as
presentation-observability ceilings only. A direct search of all three native
debug logs found no `writhing_stalker`, `zombie_rider`, or
`mon_zombie_rider` records, so the excluded controls did not enter these rows.

## Verification and cleanup

`./cata_test '[flesh_raptor]'` passed 7 cases / 61 assertions; complete output
is `.de67/task-logs/R-035-flesh-raptor-unit-20260907.log`. Fixture JSON parses
cleanly and `git diff --check` passes. PIDs 83139, 83619, and 83955 were
explicitly save-quit and quit through the native PTY flow; no process remains.

Changed repository files are the three fixture manifests above plus the
pre-existing unrelated `.de67/work-ledger.md`; no source file was modified in
this test pass.
