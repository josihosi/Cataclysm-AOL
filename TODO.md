# TODO

## Current necessary claim

Run `20260809_045732` on exact checkpoint `8ab8fcb84b` did not reach local homeward ownership, but
it credited two returned members after failed materialization and without camp dematerialization.
Require an exact same-operation abstract-resume receipt physically at the owning camp before the
schema-10 structural return reconciler can credit returned members, a report, or a decision.

Prove no-resume and off-camp-resume states retain physical ownership with zero return credit, while
an at-camp dematerialization receipt reconciles exactly once. Then relink and rerun
`bandit.scout_to_decision_observer_live_mcw` unchanged. Keep the installed motor receipt until the
exact pair's physical return and camp dematerialization are proved.
