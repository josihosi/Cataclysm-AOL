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
one longer wait is preferable to adjacent shorter waits with no useful decision between them;
combine their duration under the same interruption policy when advertised. Several supported
long waits remain appropriate when no single native duration covers the interval. Stop when the
requested outcome is established: extra waiting after saved payment/home proof adds no evidence
unless a later visit, persistence interval or another delayed behavior is the assigned question.
The [game manual](../GAME-MANUAL.md#wait-and-observe) covers duration merging and interrupted waits and [viewing setup](../GAME-MANUAL.md#viewing-setup), including the two
debug clairvoyance mutations and their local AI exposure caveat.

At a useful checkpoint, [save without closing the live session](live-operation.md#save-a-checkpoint). Inspect the saved camp roster,
signal/source lead, dispatch members, owner, phase, route cursor and positions, together with
exact native logs. A save can establish that smoke created a dispatch while leaving later movement
unproved. If the state answers the assignment, report the outcome. If it is still developing,
continue and check the next expected change. A temporary sleep/rest state is still developing: a missing wake timer is not a prerequisite
for a meaningful safe wait in the same writable session. Compare real elapsed time and actor states;
do not replace playable continuation with repeated read-only relaunches. If actors remain blocked
after the relevant condition changes, or an invoked gate prevents progress independently of time,
inspect that cause instead of repeating unchanged waits. Preserve the first meaningful divergence
without automatically ending the run. A short scripted interval ending is not a gameplay verdict. Preserve the same actor and dispatch identities across
checkpoints, and distinguish the player's basecamp from the hostile camp being tested.
