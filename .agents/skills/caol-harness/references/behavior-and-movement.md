# Exercise the behavior and choose movement

Before calling a playtest failed, check its primary physical circumstances: did the intended
order apply to the right actor and establish actual follow eligibility (an affirmative reply or
`follow_close` flag alone does not establish it), did the player actually move, and did enough game
time and separation develop for the behavior to become observable? Give the mechanic room to work.
For following, move away far enough to require pursuit under the actual follow setting; two
nearby steps may prove nothing. For staying/guarding, move away from the NPC and allow enough
time to distinguish holding position from following; walking toward it is a weak test. Choose
distance and duration from the mechanic and observed state, not a universal step count.

Compare actual before/after positions and game time. An unchanged position may mean a wall,
closed door, occupied tile or intervening prompt: inspect it and find a usable door, window or
route around the obstruction before judging the NPC. A game turn is one second; an action can
cost a different amount of game time. Wall-clock waiting and repeated observations do not advance
the simulation. Measure distance in map squares and elapsed time in game timestamps rather than
assuming a few tool calls gave the behavior a meaningful opportunity.

Prefer a movement or wait macro when the next useful decision is at a destination or after an
interval. For many separation tests, native overmap autotravel to a tile one OMT away is a useful
first choice: it uses the game's pathfinder and gives the behavior room without a tool response
per footstep. Choose a destination away from the NPC, use the current Overmap actions
`overmap.move_cursor` and `overmap.choose_destination` with their advertised targets, and handle
the native travel confirmation. Inspect actual arrival/displacement and elapsed time; selecting
a destination alone is not completed travel, and one OMT is a useful test choice, not a universal
success threshold. Adjust if the behavior or an interruption requires it.

For local movement or waiting, use the existing `game.move_relative` / `game.wait` requests
advertised by `controls`, submitted through `call --request`. `game.move_relative` batches cardinal
steps; it does not pathfind around walls. Choose a passable route, collect once, and inspect terminal
state, actual progress and interruptions
instead of reading a full response after every tile. Single steps remain useful when the next
step itself matters. Macro completion proves movement/time passage, not the tested feature.

Treat inadequate separation, blocked movement or insufficient elapsed time as a test-design
problem to resolve in the current run where possible. Preserve the observation, adapt the test,
and distinguish a setup limitation or inconclusive result from a demonstrated gameplay failure.

## Long camp and dispatch observation

For fire/smoke/light and bandit/cannibal response, wait in meaningful in-game intervals and
intermittently inspect the camp, nearby movement and the real fire. Distant notice can need many
game minutes; a dispatch or return may need a full day or more. A short clean callback or a few
tool responses does not settle the player's observed journey. The native wait menu may offer
five minutes, one hour or multi-hour choices; inspect its current advertised controls instead of
assuming a maximum or repeatedly choosing short waits. With a verified fire and active test,
several long waits can be the useful action. Use the owner's clairvoyance
mutation as a viewing aid when requested, and record whether it was already present or added to a
disposable save. It changes what the player can see, not the physical source or camp cause.

At a useful checkpoint, quicksave without closing the live session. Inspect the saved camp roster,
signal/source lead, dispatch members, owner, phase, route cursor and positions, together with
exact native logs. A save can establish that smoke created a dispatch while leaving later movement
unproved. If the state is conclusive, report the outcome; if it raises a concrete question, keep
the game running, wait longer and check that question again. Do not close an inconclusive run just
because a short scripted interval ended. Preserve the same actor and dispatch identities across
checkpoints, and distinguish the player's basecamp from the hostile camp being tested.
