# R-MAINT-COMPACT-IO-native-views-001

Provide a truly read-only Patrol inspection path for the shared workbench. In
`src/npc_inspection.cpp`, diagnostic_camp_patrol calls
`basecamp::get_current_patrol_shift_plan`; `src/basecamp.cpp` implements that accessor by calling
`refresh_patrol_shift_cache`. The emitted provenance admits that reading may refresh the plan.
This does not prove a historical actor-order mutation, but it cannot serve as a non-mutating view.

Own the narrow accessor/header/inspector/focused tests, coordinated with Sol. Return existing plan
and runtime facts with explicit unavailable/stale state, without refreshing cache, constructing
fixtures, advancing turns or changing the actor's order. Preserve the gameplay update accessor and
accepted Patrol behavior. Demonstrate that snapshots before/after observation have identical cache,
actor/order and time state, including an absent or stale cache. Current `get_current_patrol_runtime`
also reaches the refreshing accessor; inspect its use before asserting read-only behavior.

The observation assignment owns presentation/query interfaces and will consume these native fields.
Expose stable actor/camp identity and freshness through existing inspector schema, not a separate
framework. Retained J0166 shows roster character:3 while the selected actor was inactive; originals
and the accepted corrected persistence route remain valid at their recorded ceilings. No fresh
Patrol campaign is needed solely for adoption; source-bound native proof is only for a remaining
integration uncertainty. Tool delivery grants no new gameplay acceptance or repair promotion.
