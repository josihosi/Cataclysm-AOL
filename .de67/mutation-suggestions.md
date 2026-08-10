# DE-67 mutation suggestion ledger

This is consumable scratch, not history. Independent reviews and manual suggestions use ordinary
Markdown. The current mutation transaction reads every pending entry. After a real guarded mutation
is applied, replace the whole ledger with this empty template; if guard or application fails, clear
nothing.

A random improvement review records one to three concrete inefficiencies ranked by causal
importance, direct evidence, the candidate target/section change, and proposed treatment of pending
suggestions. The accepted subset must correspond to the actual guarded file change. A guarded DFS
no-op leaves this scratch ledger intact.

## Pending suggestions

### T01 closeout — ledger cleared outside mutation

Source: watcher finding

**Short verdict:** mutation scratch consumed by ordinary task acceptance

The coordinator removed the pending T01-M16 workflow finding while accepting T01, although no
guarded guideline or DFS-expansion mutation consumed it. Keep pending suggestions until a real
mutation transaction succeeds; task fixes and DFS completion do not empty the ledger.

### T01 closeout — coordinator stopped instead of refilling

Source: watcher finding

**Short verdict:** coordinator treated one accepted claim as terminal

After accepting R-001, the coordinator exited with other red DFS claims still available instead of
refilling the work ledger and continuing. A batch completion is a refill boundary, not a workflow
completion, unless the DFS has no red work or an owner-set stop condition has fired.

### R-002 staffing audit — strong model used for every worker

Source: watcher finding

**Short verdict:** wrong worker or model / cheap lane unavailable

Mac thread state records sixteen spawned DE-67 workers from T01 through the current R-002 preflight,
all using `gpt-5.6-sol` at `high`, including understood build, operator, fixture, and focused-test
tasks. The current coordinator spawned Huygens on the same strong lane. Apply the existing weakest-
sufficient rule concretely when a cheaper model is available; reserve Sol/high for real causal
ambiguity and Sol/xhigh for independent mutation review. If no cheap lane is exposed, record that
availability fact once rather than pretending worker selection occurred.
