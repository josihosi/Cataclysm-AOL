# Andi handoff

Current state: `CAOL-CAMP-LOCKER-EQUIPMENT-API-REDUCTION-v0` is closed/checkpointed green v0.

Closure summary: camp locker now preserves explicit camp policy while deferring the audited item, wearability, body coverage/layer, reload/ammo, scoring, carried-cleanup, live-state, medical-readiness, and zone-boundary truth to existing APIs where a real equivalent exists. Last code checkpoint: `6a0f003dfe Reduce camp locker armor resistance scoring`.

Evidence: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[camp][locker]"` green at closure.

Boundary: no more open-ended locker ontology archaeology. Any future locker cleanup needs a newly promoted concrete seam. Do not drift into bandits/riders/stalkers from this closure packet.
