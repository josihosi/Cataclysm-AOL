# DE-67 mutation suggestion ledger

This is a consumable mutation queue, not history. User-authored entries carry explicit
mutation-scoped owner authority; reviewer candidates remain evidence-backed proposals. A successful
review removes only completed entries; a failed or unproved review removes nothing. Preserve a
blocked entry with its exact conflict, impossibility, missing authority, risk, or uncertainty.

The pending section is machine-owned protocol as well as owner input. Encode each owner batch as
one top-level `- Owner-authorized [trigger]: ...` or `- Owner-authorized [defer]: ...` entry, and
encode a lesser-authority reviewer proposal as `- [defer] Reviewer-authored proposal: ...`. Indent
every continuation. A trigger requests review at the next durable quiet junction; a defer does not.
Accumulate a batch under one deferred entry and promote that entry once when ready. Any other
top-level syntax retains legacy trigger behavior. Delete completed entries; do not keep consumed-history sections in this queue. Durable receipts
and review artifacts retain the evidence.

For a miss, keep immediate recovery distinct from the smallest repeatable method correction and
state the counterexample that could falsify it. For random or universal review, preserve the
applicable policy's stored target, scope, authority, and evidence limits; a guard result never proves
more than its inputs.

## Pending suggestions

- Owner-authorized [trigger]: On 2026-09-20 Josef explicitly restarted delivery: "why dont you fix code right now, and then de67 3 the playtesting before i do? first harness fixing of course and then playtesting the light? harness also needs a bad performance alarm system btw". Incorporate this changed scope and order into the frozen FS and active ledger before ordinary dispatch; this supersedes the prior owner stop. First deliver the lean/reliable harness claims (including deletion of replaced machinery), adding actionable performance alarms, then perform source-bound native light/smoke/sound regression and performance playtests before the owner's manual play. Keep unrelated optional experiments deferred. Do not restart Phase 2 or demand a second permission for this explicitly authorized addition.
  Current precedence: Josef subsequently paused before playtesting to discuss status, then authorized only investigation and repair of the two camp-test failures before returning for discussion. Keep this broader request pending; do not launch Phase 3, harness work or native playtesting until he resumes.
  The direct performance checkpoint preceding launch spreads discovery of new stationary light/fire/smoke across 900 one-second turns, rechecks known sources, fuses field enumeration, and separates significant-sound observation from broad NPC maintenance. Josef permits up to fifteen in-game minutes to notice distant signals and explicitly answered "Yes—brief visual signals may be missed"; immediate nearby perception remains intact. Sound observations must survive until their subsequent AI decision, without duplicating events or renewing them from stale data. Reconcile prior per-turn discovery assertions with this new owner contract, preserving historical evidence ceilings rather than crediting old receipts to changed behavior.
  Alarm requirements come from the owner's request, not an invented universal speed target: expose stalled simulation progress, slow/spiking real turn execution, and sustained regressions against comparable workloads. Bind alarms to actual game-time progress and request/run/process identity with reason, measured values, baseline/configuration provenance and evidence handles. High CPU alone is not an alarm; transport timeout is not success or automatically native failure. Use measured baselines and explicit per-scenario expectations for thresholds, with discoverable configuration, so noisy alerts or automatic destructive recovery do not replace judgment. Exercise alarms through the real pending-operation driver and native waits as well as deterministic tests.
  The current first slice does not budget every known emitter/recipient each turn or change the existing sound-to-horde path. Measure populated/light-heavy and smoke/sound workloads before calling performance acceptable. If retained active-source work or another affected owner remains dominant, perform the smallest necessary follow-up fix and rerun its relevant proof. Compare average, tail/max turn latency, game-time throughput and first-use/15-minute boundary behavior; do not convert the empty-map collector benchmark into whole-game success. Include sector boundaries, brief visual exposure, light-off/removal, finite memory expiry, one-shot sound retention/idempotency, save/reload and real wait completion. Native harness reliability must precede gameplay acceptance.
