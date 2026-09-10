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

- Owner-authorized [trigger]: Josef requested “Also tell me what’s on the bug list, let’s get those going” after the open activation, sound and evidence choices and supervisor failure were explained. Promote R029-F004 attacker activation to implementation and native retesting: exact operation members must enter active local NPC/creature-tracker ownership so refusal, attack, casualty and aftermath can be exercised; inherited strategic routing must not compete with committed local contact, and authoritative return handoff must remain valid. Choose the smallest engineering approach from the existing activation plan rather than asking Josef to choose internal routing mechanics. Update the relevant FS outcome and ledger through exclusive review, then commission delivery to Sol. Also repair the reproduced DE67 supervisor mismatch that treats unchecked FS/ledger items as executable work and fails a valid owner-wait-only coordinator return; preserve true no-progress detection for executable work and honest owner-wait reporting. Reproduction: .de67/state/coordinator-runs/initial-ec2dd52b800a41f79cf6cd35264241e2/supervisor_error.txt, same coordinator session01a08a1c-22fb-7d90-abfa-ea8d8855f58b, and coordinator_supervisor.py final has_executable_work check. Reconcile stale active intake: R029-F005 night-departure/through-dawn behavior is repaired in receipt0892479ce584418db6ef41fc3ef4d594d82b16ca97d6dac9fb423f2c3687185f; retain the active-combat ceiling and existing proof without replay. Sound cooldown semantics and R031 historical-provenance acceptance versus fresh proof are awaiting explicit answers in Josef’s current conversation; do not infer acceptance of either recommendation from this repair-start request. Preserve all completed proofs and unrelated choices. Resolve this owner entry and request one supervisor-owned successor; do not create a competing coordinator.
