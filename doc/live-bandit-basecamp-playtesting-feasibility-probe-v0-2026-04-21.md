# Live bandit + Basecamp playtesting feasibility probe v0 (2026-04-21)

Status: greenlit live-probe packet.

## Greenlight verdict

Josef explicitly wants Andi to try starting the game and find out whether live playtesting is already practical with the new bandits plus the current Basecamp work in place.
The key uncertainty is not only feature logic, but setup reality: can the current tree be launched into a credible live probe path, and can overmap bandits be placed or induced on purpose without descending into debug-menu folklore.

This is a bounded feasibility probe, not a demand to prove the whole game is already playtest-perfect.

## Scope

1. Start the current game on a real live path and determine whether a credible playtesting setup for current bandits plus current Basecamp work can be reached.
2. Check whether current debug/harness/control paths can stand up the needed scenario footing, including whether overmap bandits can be spawned, placed, or otherwise made testable on purpose.
3. Produce one reviewer-readable packet that separates:
   - screen: what happened in the running game
   - tests: what deterministic checks already guarantee
   - artifacts/logs: what was or was not recorded
4. Classify the result honestly as something like feasible now, feasible with narrow restage/setup help, or blocked on a specific missing setup path.

## Non-goals

- claiming broad game readiness from one live packet
- replacing the active bandit visibility queue or the reopened Locker Zone V3 follow-through
- fresh bandit architecture or Basecamp feature work
- hand-waving setup ambiguity away just because debug menus are large and weird
- turning one inconclusive live run into fake product truth

## Success shape

This item is good enough when:
1. one bounded live playtesting feasibility packet exists for current bandits plus current Basecamp footing
2. the packet says plainly whether live playtesting is already practical on the current tree
3. the packet says plainly whether overmap-bandit setup/spawn control is currently available, narrowly restageable, or still missing
4. the packet separates screen/tests/artifacts cleanly instead of flattening them into one soup verdict
5. any blocker is stated as a concrete missing setup/control path rather than vague "playtesting hard" whining
6. the slice stays bounded and does not turn into open-ended live playtesting theater

## Validation expectations

- choose the evidence class deliberately: startup/load, live behavior, and artifacts are different claims
- use the smallest reliable layer first, then go live only where the live question is real
- prefer fresh launches for current-build proof instead of leaning on stale open sessions
- if a live probe is inconclusive, say why it is inconclusive instead of laundering it into success/failure

## Position in canon

This is a greenlit live-probe packet.
It does not replace the repo's single active `TODO.md` lane.
It exists to tell us whether current bandit-plus-Basecamp playtesting is already real, narrowly reachable with setup help, or still blocked by missing scenario/control footing.
