# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

- Trace the real `show_board` / board-handoff path end-to-end.
  - Confirm where the structured / LLM-side path consumes `basecamp_board_handoff_snapshot.txt`.
  - Keep the human-facing spoken board bark concise instead of dumping the full handoff snapshot at the player.
- Capture the first honest evidence packet for the board-snapshot lane.
  - Deterministic baseline already passed: `./tests/cata_test "[camp][basecamp_ai]"`
  - The shipped board handoff template now includes `{{planning_snapshot}}`
  - The missing piece is proof of the actual prompt/artifact path, not another broad compile ritual

## If blocked

- If the blocker is harness/probe setup or save state, improve the probe path, restore a pre-service fixture, or add the smallest temporary re-arm hook needed.
- Do **not** burn another run just repeating startup/load smoke unless the binary actually changed in a way that justifies it.
- Do **not** sit on a Josef-waiting item when an agent-side target is available.
- If there is no good unblocked target left, send Josef a short parked-options menu from `Plan.md` instead of spinning.

## Waiting on Josef

Keep this parked without drama until Josef is available:
- Basecamp bark feel pass on the current dirty tree:
  - `show me the board`
  - `status of bandages`
  - `craft 5 bandages`
  - `clear request #...`

Do not treat this as a stop-work order.

## Later

- If the routing proof lands cleanly, expand the LLM-side board snapshot work only as far as needed to make the prompt/artifact path legible and testable.
