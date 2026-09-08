# R-033-smoke-dedup-repair-001

Repair the unchanged-source comparison defect and prove its narrow result. Owner relay4673b7453329
promotes R033-F002; the refrozen R-033 slice carries authority. Existing channel controls, accepted
same-OMT exception, handoff/reentry and original failed attempts retain their own evidence ceilings.

Current causal facts: `src/bandit_live_world.cpp::record_staffed_camp_signal_observations` builds a
lead keyed by camp, kind, target and source OMT. It compares fresh `read.summary` with the existing
lead after neutralizing first/last-seen and last-checked times. `bound_camp_map_lead_strings` bounds
stored summary to 256 characters. The unbounded fresh summary therefore fails payload equality
on repeated identical reads. `tests/bandit_live_world_test.cpp` contains staffed-camp signal cases.
Original native v7 evidence: `.de67/task-logs/r033-v7-witness.json`, event309 and
`.userdata/dev-harness/config/debug.log:3514`; retrieve exact record and bound artifact first because
the shared log may have changed. The original full finding is preserved in this review's baseline.

Reproduce a long summary through the real production compare/persist route in a focused regression.
Use equal content at later observation time, short/long boundaries and a meaningful changed source
control. Choose a canonical comparison that matches the durable semantics; merely discarding every
long-summary difference could hide real changes. Current code intentionally preserves the durable
record on unchanged reads (including its observation times); do not invent a requirement that all
metadata must always freeze or always advance. Separate source/content identity, record revision and
observation metadata in the evidence. Name any ambiguity that changes intended semantics.

For native verification, establish the actual eligible observer/camp, exact source/lead identity,
initial content/revision and opportunity to observe. `src/do_turn.cpp::live_bandit_staffed_camp_signal_reads`
uses actual range/LOS/sight; the prior observer18 needed binoculars. Preserve geometry and coordinate
units, exclude unrelated player-opportunity writers and the accepted same-OMT case. The staffed
cadence is five minutes while an ordinary smokebomb lasts about100 seconds: select a sustainable
unchanged source or align a justified native route, not repeated waits after the source expired.
Reobserve that same unchanged source across relevant native cadences and distinguish content/revision
from permitted metadata. Change one meaningful source property and verify its defined update. Remove
or expire it and check the separately relevant aging boundary; do not claim unrelated memory proof.
Use exact advertised item identities when two inventory labels match. Setup earns zero gameplay credit.

Aging continuation `f02117b55842456c933b1512428d411b4eef0412696d88d80de43bd5f79fa5b5` already created a
lead8225, saw no source8230 and waited to8592 without a final lead observation. Recover that saved
state when it answers the remaining question. Rebind changed source/executable/scenario and retain
native input, independent verdicts, original artifacts and exact runtime ownership/cleanup. Sol
coordinates any shared workbench/native instrumentation edits. Do not replay accepted campaigns for
adoption. Return the narrow result plus any still-open native boundary; source regression alone is
not native integration acceptance.
