## Model choice

Current owner direction, 2026-10-28: choose GPT-6.1 Terra by default for coding with a clear outcome and testable constraints, including localized fixes, features, tests, fixtures, tooling and refactoring. Use GPT-6.1 Sol when the current work has concrete ambiguity, substantial cross-system coupling, an architectural decision or demonstrated difficulty that warrants it. A coding label, large file or historically complex assignment alone is not a reason to keep Sol. Reassess the next meaningful piece at a safe boundary; preserve useful context and avoid handoffs whose overhead outweighs the benefit. This is a default, not a quota or mandatory Terra trial before obviously difficult work. Do not add a mandatory Sol review layer to every Terra result; retain ordinary risk-based review.

Coordinator remains GPT-6.1 Sol; Astra owns mutation review. Terra also remains the default chaperone for direct GPT-6 Luna players; only Luna operates live harness input. Use supported same-assignment model handoffs with settled requests and source/build/input custody, preserving tasks, conversations, clocks and saves. A blocked model preference must not strand otherwise authorized delivery; retain the current capable owner and report the concrete obstruction. GPT-5.6 models remain retired. This direction supersedes historical dispatch guidance, not historical evidence.

## Thinking intensity

Choose worker effort separately from model, based on uncertainty, coupling and consequences within the model's available efforts. Clear bounded work can use low; raise effort when the task needs it. Do not automatically pair Terra with medium or Sol with high. Coordinator Sol low and mutation-reviewer Astra medium remain the role defaults. No specific quality, speed or cost saving is presumed without observed results.

## Mutator discussion and steering

**MUTATOR: DO NOT STEER SOL UNLESS JOSEF EXPLICITLY ASKS.**

This is Josef's standing workspace instruction and must survive context compression and restarts.
Discussion, an agent report, or permission to repair DE67 does not by itself authorize forwarding
instructions to Sol or workers. Do not turn Josef's discussion into mailbox steering. Keep this
boundary in mutator handoffs. Recovery repairs belong to the mutator, not a coordinator assignment.

Diagnose and implement authorized local repairs directly. Preserve conversation/task/clock and
source/save/input ownership. Installed-method activation still uses the existing exclusive
supervisor route; do not start a second coordinator or use old review bindings. Trace the failing
producer, delivered context, decision and runtime consumer before fixing it. Verify the affected
public behavior and useful continuation; service startup or an ACK alone does not prove recovery.
State the exact blocked operation and affected owner, and distinguish independent permitted work.
Do not infer a global stop from a local custody warning or repeat a blind restart.
The mutator owns semantic working-contract changes unless Josef explicitly delegates otherwise.

When Josef asks to recover broken de67 coordination, the mutator owns diagnosis and authorized repair directly. Do not send repair instructions to the failed coordinator or make its execution a prerequisite for restoring it. Use supported supervisor activation, watch startup through actual useful same-conversation work, and report any remaining blocker honestly; a running service or ACK alone is not recovery. Preserve tasks, clocks, pending input, saves and native ownership. This duty does not authorize unrelated service shutdowns, bypass exclusive activation, or override the no-unsolicited-steering boundary.


# Workspace command and evidence habits

Select evidence from the current question; these
query patterns are aids, not an investigation sequence. Record decisive findings and reproduction steps concisely; temporary evidence is for the
current operation, not indefinite retention or copied handoff archives.

## Harness playtesting model

Only GPT-6 Luna workers may launch or operate live playtest harness sessions. Terra, Sol and Astra
workers must delegate that work to a GPT-6 Luna subagent, even when the parent owns a complex
task. Give Luna the test outcome and relevant setup, and use its transcript and findings for
analysis. Code changes, builds, automated tests and reading existing evidence remain with the
appropriate worker. See `.agents/skills/caol-harness/SKILL.md` for playtest guidance.

## Shared development and publication

`origin/dev` is the shared development frontier and the sole DE67 checkpoint target.
`dev_w` is for parallel Windows work: integrate its useful changes into `dev` promptly,
then bring both branches to the same HEAD. Coordinate one owner for Git index, refs and
publication; preserve other workers' uncommitted files and active build/source bindings.

Publish coherent, reviewed checkpoints to `origin/dev` during delivery. Inspect the actual
push result and remote SHA: a local commit or successful hook wrapper does not prove publication.
On a rejected push, reconcile the remote and local content/history with their owners before
further publication. Keep useful independent work moving while resolving the shared frontier.
Do not discard remote commits, force-push over them, or create review/integration branches as
a substitute for reconciliation. Allowed remote branches are `master`, `dev`, `dev_w`,
`port/*` and purposeful `backup/*`; retire obsolete branches after preserving useful history.
GitHub's `.github/workflows/mirror-dev.yml` replicates `dev` to `C-AOL-mirror/dev` after
origin pushes. Agents publish to origin only; the mirror is not another development frontier
or DE67 checkpoint target. Check a failed mirror Actions run through GitHub rather than
adding a second local push target.

## Storage

Keep only dev and dev_w, usually on the same development HEAD. No Git worktrees, per-task
checkouts, or full source/build/data clones. Coordinate exclusive work in these existing locations.
Keep only the current necessary build and reusable cache. Playtest saves, logs, captures and proof
archives are disposable working files, not durable history: remove them when the active operation
ends. Record the commit, run identity, concise outcome and reproduction recipe in existing task context; Git keeps source
history. Cleanup is part of delivery, not optional follow-up. Do not create cleanup backups or
inventories of every artifact. Check space before large allocations and reuse existing outputs.
Protect user data, unrelated files and active owner operations; arrange their handoff before cleanup.
The chaperone closes completed or abandoned games and their owned brokers/helpers, verifies
process exit, then removes their generated run residue. Worker return or a hidden window is not
process closure. During long runs, watch free space and log growth and correct runaway output
at its producer. Keep concise results, not growing raw journals or duplicate proof directories.

## Commands

Keep task logs together in the existing `build_logs/` or `.de67/task-logs/` route used by that task;
the global implementation/evidence rules govern capture and result inspection.

Use `rg --files` with a filename clue to locate unknown source, or `rg -n -F` for a known symbol.
Read the enclosing function or section after locating it. Search generated output separately from
source and select the relevant run/session/artifact first. When output is truncated, narrow the
query or extract the required fields rather than increasing the dump.
Combine `rg` queries over the same targeted files, and retain and poll returned session IDs until
the searches finish or stop them before launching replacements.

## Structured evidence and handoff

For JSON, JSONL and databases, inspect schema when unknown, filter by exact identity, and project
needed fields. A line limit does not bound a large single-line JSON record. Preserve source paths,
identities and full-retrieval handles; omitted detail is not absent evidence.

Keep current task knowledge in the existing `.de67` context/ledger surfaces. Run artifacts under
`.userdata/*/harness_runs/` and session bindings under `.userdata/*/sessions/` or
`.userdata/openclaw_harness/bridge-sessions/` are evidence, not current owner instructions.

## Scheduled mutation review focus

For exclusive mutation reviews, retain Josef's latest coordinator conversation and inspect actual
playtest worker transcripts when diagnosing friction. Start from intended survival gameplay:
a cannibal raid killing the player can be success; passive committed attackers are a divergence.
Trace repeated fire/wait/movement mistakes to delivered instructions, current controls and game
visibility. Improve the existing harness knowledge base, compact observations or native-action
macros when evidence supports them; remove contradictory advice rather than accumulating gates.
Use the existing in-game LLM snapshot only as a reference and leave it unchanged. Respect actual
crashes, impassability and failed recovery; do not infer that every apparent blocker is a game bug
or that every blocker is imaginary. Reuse prior review findings and compare subsequent actual use;
written guidance alone is not proof of adoption. This standing focus adds no background review,
fixed audit quota or new orchestration layer.
