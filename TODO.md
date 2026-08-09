# TODO

## Current necessary claim

Fix the exact T01 homeward placement rejection from run `20260809_035926`: the already-assembled
generation-1 pair must materialize at the first later safe non-camp route waypoint when that loaded
OMT has one complete adjacent pair but no second disjoint assembly-staging pair.

Relink the committed repair and rerun `bandit.scout_to_decision_observer_live_mcw` unchanged.
Preserve forward assembly, adjacency, rollback, and final-camp exclusion. Accept only a fresh
identity-bound incident pair that records the returning-home local handoff and camp
dematerialization before report and decision.
