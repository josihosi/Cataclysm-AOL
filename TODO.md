# TODO

## Current necessary claim

Run `20260809_065048` on exact checkpoint `2f916249a0` proves baseline, ordinary-NPC-only,
covert-only, and combined point routes from member 4 to far departure `(3935,947,0)` are all empty.
Neither avoidance layer causes the stall; the far boundary point is unreachable to the ordinary
local solver.

On that failure, fall back to the existing physical one-OMT-at-a-time homeward motor with the same
nonreentry predicate. Focused proof must show ordinary position progress toward the next persisted
OMT while ownership remains local and no boundary/dematerialization/return credit occurs early.
Then relink and rerun `bandit.scout_to_decision_observer_live_mcw` unchanged.
