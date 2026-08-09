# TODO

## Current necessary claim

Run `20260809_063218` on exact checkpoint `959f3c0e96` proves member 4's combined route solve to
departure `(3935,947,0)` repeatedly returns `route_found=no`, `path_before=0`, then pauses without
moving. The pair cannot reach the boundary transaction while one required member has no route.

On that existing hourly/member receipt, add debug-observer-only read-only comparison solves to the
same departure: no extra avoidance, ordinary NPC avoidance only, covert nonreentry avoidance only,
and the actual combined result. Relink and rerun `bandit.scout_to_decision_observer_live_mcw`
unchanged. Change no production constraint until the exact artifact selects it.
