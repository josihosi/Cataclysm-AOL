---
name: de-67-3
description: Explicit third phase of de67. Use when the user says `de67 3` to deliver a frozen FS through deadline-bound work, progressive role disclosure, honest proof, and evidence-driven method mutation. Do not run the discussion or FS-authoring phases.
---

# de67 3 — delivery router

Only GPT-6 Sol, GPT-6 Luna and GPT-6 Astra may run new work. Prefer Luna for workers;
use Sol for coordination. GPT-5.6 and Terra are retired. Live C-AOL harness playtests
are Luna-only; other workers delegate them to a GPT-6 Luna subagent.

This file routes Phase 3; it is not the coordinator procedure.

The invocation authorizes implementation of the frozen FS in the named working repository. Do not
read `de-67-1/` or `de-67-2/`. Never inventory, search, or read `.de67/no-go-zone/`.

This router is bootstrap material. A launcher may read it to initialize Phase 3. Runtime decisions
come from the compiled policy kernel, not packaged or workspace guideline prose. Packaged scripts
may be executed as tools.

Inspect `.de67/state/workspace.json` and verify that the canonical Functional Specification
(`.de67/FS.md`) records `Frozen` or `Refrozen` against an inspected source baseline. A missing
configuration, draft FS, unresolved material owner choice, or changed user outcome returns to de67 2.

Copy each missing environment artifact individually. Never overwrite an existing project file.
Machine state stays under `.de67/state/`; the FS and ledgers remain ordinary project artifacts.
The machine-canonical runtime policy is `.de67/phase3-policy.d67`, compiled from the branch's Phase-3
policy source and bound to its canonical digest. Route priorities, predicates, actions, reads,
obligations, fallback behavior, event vocabulary, temporal transitions, and keyed lifecycle rules
are mutable source data. The immutable kernel only authenticates and interprets that data. Guideline
Markdown is retained only as a legacy differential fixture during this experiment.

Only the Phase-3 coordinator choosing the next shared workflow transition executes
`scripts/policy_kernel.py decide` against the local compiled policy, workspace, and deadline
database. Ordinary workers own only their sealed task brief; they do not invoke coordinator policy
or mutate deadline state. The returned action names the starting context and applicable obligations.
The coordinator may inspect additional relevant evidence while preserving the action's ownership
and lifecycle requirements; worker briefs carry only the obligations that govern that worker's work.
Unknown, corrupt, ambiguous, or unsupported policy fails closed instead of falling back to prose.

Policy mutations operate on a candidate machine source and contract corpus under `.de67/state/`.
Promote the candidate source and compiled bytecode together only after `policy_kernel.py guard`
proves every contract, temporal invariant, mutation-ledger route, deadline wake, and rule necessity.
The successful promotion requests one fresh coordinator; failed candidates have no authority.
Compilation lowers object-shaped policy into positional instruction vectors, interns repeated
symbols, and compresses the resulting tape. `policy_kernel.py decompile` must recover the normalized
source exactly; lossy or noncanonical artifacts fail closed.

## Route locally

If you are explicitly starting, restarting, stopping, or diagnosing the external Phase-3
supervisor, first read [references/external-supervisor.md](references/external-supervisor.md).
Ordinary coordination, worker execution, and internal mutation handoff do not read that reference.

On macOS, start the external supervisor only through
`scripts/supervisor_service.py start --workspace <workspace>`. This creates one detached,
workspace-keyed tmux session whose lifetime is independent of the invoking terminal and which
retains the invoking user's workspace permissions. Use its `status` and `stop` commands for
observation and shutdown. The session has no automatic restart: a crash stays visible and requires
an explicit start instead of becoming an automatic restart loop. Direct
foreground execution of `coordinator_supervisor.py` is reserved for tests and attended diagnosis,
not an ordinary Phase-3 launch.

The supervisor launches an ordinary `gpt-6-sol` coordinator at low against the compiled workspace
policy. The coordinator asks the kernel for the next transition and supplies the relevant context
and role-specific obligations for that route. A due mutation blocks new dispatch. Once all
already-live worker turns have returned and are no longer editing, the coordinator exits without reviewing or changing guidance. The supervisor then runs one fresh
`gpt-6-astra` reviewer at medium with no coordinator or worker active. That reviewer reads the complete
mutation-suggestion ledger and treats each user-authored suggestion as explicit mutation-scoped
owner authority beneath system and developer instructions. Within the Phase-3 framework, that
authority supersedes conflicting ordinary skill, workspace, selected-lane, freeze, and
mutation-surface restrictions to the extent necessary to achieve the suggestion. It may, for
example, thaw and refreeze the FS or change a normally protected method artifact when the
suggestion requires that change. It does not authorize unrelated work, dishonest evidence, or a
different owner outcome. The reviewer follows every suggestion through, dispositions every entry,
resolves the durable mutation gate, and exits.
An `Owner-authorized [trigger]:` entry creates that exclusive gate as soon as workers are quiet. An
`Owner-authorized [defer]:` entry remains mandatory queued reviewer input but does not wake, retire,
or replace the ordinary coordinator; the next regularly due mutation review consumes it. Legacy
unlabelled entries retain trigger behavior.
Only then does the supervisor launch one fresh low coordinator. Product strategy and evidence
judgment remain model work inside the selected route.

## Terminal routing

Ordinary worker results, test failures, acceptance, ledger refill, and exploration-to-closure
transition stay with the same coordinator. A formal terminal finding also stays with that
coordinator; it is reserved for a contradicted assigned outcome, materially different owner outcome,
real external decision, unavailable capability, irreversible risk, or an exhausted authorized route.
A disproved strategy is nonterminal while recoverable repository work remains. Mutation completion
is the only planned fresh-coordinator boundary.
An abnormal process exit remains recoverable, but it is recorded as recovery rather than treated as
a policy restart. The external supervisor exclusively launches and acknowledges every process.
