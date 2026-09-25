# DE-67 work ledger

FS: `.de67/FS.md`
Lineage: `<one stable project identity; the deadline database binds it on first task>`

Keep the currently actionable red FS claims. Refill this projection whenever executable red FS
work remains; an empty batch is not completion. Do not impose a batch-size limit. Each item names
one mode:

- `Exploration` — one learning goal, the unknown premise, and evidence that will yield a strategy
  plus proof route.
- `Closure` — the known strategy and proof route, stable ids and current revisions for finite frozen
  gaps, and the next action expected to close or falsify exactly one active gap.

Use `  - Current handoff:` for the transient continuation: proved footing, active work with exact
handles/status queries, the first unresolved step, and evidence links. Replace it when the frontier
changes; historical receipts retain proof without making old process state current.

Keep only the present causal frontier, active route, material boundaries, claim-level item clock,
and current attempt id after dispatch. A retry receives a new task or attempt id without rebasing the
lineage, claim, item start, estimate, or deadline. A finding may reopen exploration but never
replaces that item identity or erases an attempt record. Delete an item when the outcome is accepted;
do not duplicate transcripts, chronological task history, or batch summaries here.

Every active item has exactly one indented pointer line containing one or more stable ids allocated
by `mutation_guard.py mark-dfs-slices`. Multiple ids preserve non-contiguous claim, acceptance, and
proof sections without loading the text between them. The markers remain in the FS after
acceptance; deleting the active item removes only these live pointers.

## Active work

<!-- Example item: - [ ] R-001 — Closure: make the named owner perform the FS transition. -->
<!-- Example pointer:   - DFS slices: `R-001-S001`, `R-001-S002` -->
