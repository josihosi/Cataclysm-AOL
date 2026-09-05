# Harness checkpoint and next discussion

The harness implementation and representative qualification fixtures are checkpointed on `dev`.
The comprehensive playtest package is a separate owner discussion followed by the appropriate DE67
steps; this checkpoint neither launches that package nor certifies all CAOL features.

The working source is the Mac mini checkout at
`/Volumes/CodexBulk/Schanigarten/workspaces/Cataclysm-AOL-hostile-ecology-dev`.
Windows remains behind and must not overwrite that checkout. Preserve the existing DE67 state.

## Use and evidence

Start with [the harness skill](../../.agents/skills/caol-harness/SKILL.md), then
[the capability map](QUALIFICATION.md). The latter separates native implementation tests,
fresh Luna usability observations, and remaining product questions. Exact local trial receipts
are under `.userdata/openclaw_harness/bridge-sessions/`; review, build, resource and failed-attempt
artifacts are under `build_logs/20260905-harness-ergonomics/`. These are retained local evidence,
not files included in this Git checkpoint.

The `harness.*_mcw` scenario definitions and `factory-*` charters are reusable disposable
qualification scenes. Query them through the registry and use its freshly bound launch authority.
They seed their runtime config from the existing profile snapshot. The prepared revolver fixture
is a declared transform of the prepared camp, with manufactured equipment receiving no gameplay
credit. The combat scene uses local Gemma 4 E4B; the focused NPC scene uses E2B; ordinary camp,
persistence and ecology scenes disable the model. Installed local model tags are prerequisites.

Failed input, transport, observation or cancellation leaves the game running. Refresh uncertain
ownership and choose from the current native surface. The player's explicit quit ends the whole
scenario; a declared save/reload continuation belongs to `finish`, and the persistent client
collects the new session generation. No automatic time/RSS kill guard should be restarted.

## Qualification still to settle

The current map is authoritative for the latest trial results. At this checkpoint, open checks
include completed live item trade and cancellation; post-order movement and new-process persistence;
actual melee/firearm damage, reload/unload and retreat; death/terminal handling; the actual E4B NPC
route; and sustained wait with live pending cancellation, state retrieval over time/distance,
and comparable resource/context observations. These are unfinished harness qualification checks,
not completed product verdicts. Historical failures and reporting corrections must remain visible.

Later package discussion should select and freeze the CAOL product trajectories against the
existing DFS: living NPC/camp routing, Locker/Patrol/Food/Storage behavior, faction stimulus and
contact/payment/refusal lifecycles, world boundaries and persistence, flesh raptors, and integrated
performance. It must not silently treat this harness checkpoint as acceptance of those features.

## Local validation and platform limits

Changed file classes include C++ semantic owners/tests, Python harness/tests, the player skill,
and disposable scenario documentation. Recent checks include the native Mac trade regression
(180 assertions), source-bound Mac game build, 32 Mac/WSL bridge and startup tests, and four native
Windows startup-flow tests. Linux curses compiles the changed trade owners and test translation
unit; its existing npc.h `changes-meaning` diagnostic was demoted from error for that isolated check.
The FIFO bridge subprocess suite is a POSIX route. No complete native Windows or Linux game run
is claimed for this frontier. The scoped trade/quit structured review is clean.

C++ formatting uses astyle 3.1 with the repository configuration. Remote CI results belong to the
pushed commit and must be checked separately; local checks do not establish a green hosted run.
