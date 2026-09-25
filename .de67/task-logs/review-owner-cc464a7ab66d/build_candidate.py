from pathlib import Path
p=Path(__file__).parent;c=p/'candidate'
fs=(p/'baseline/FS.md').read_text()
fs+='''

## Owner crash correction — S-WINDOWS-DISPATCH-CRASH

Refrozen 2026-09-24 under owner suggestion WINDOWS-CAMP-ITEM-CRASH-20260924. Inspected
Mac dev baseline `224d7dc9779fe3e871a6ec13de918226338e6751` with existing dirty work preserved.
Windows launch reports the same base plus dirty SDL3 changes; the exact executable SHA and
original dump are retained in `.de67/task-logs/owner-scout-crash-20260924/triage.md`.
Matching HEADs do not establish identical sources or binaries. This addition retains every
existing slice, accepted proof and unfinished outcome; it does not reopen harness cleanup.

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-WINDOWS-DISPATCH-CRASH-S001 claim=R-CAOL-WINDOWS-DISPATCH-CRASH -->
### Ordinary camp dispatch must preserve valid character and item references
- [ ] 🔴 R-CAOL-WINDOWS-DISPATCH-CRASH — Waiting normally through camp dispatch and subsequent local observation in the owner's Windows TestSetup00 world no longer crashes on an invalid item reference, with inventory and ecology behavior preserved.
  - Code gap: The original Windows dump confirms a read access violation at0x2100000020 with RBX0x2100000000 in the retained executable. The reported stack traverses item::has_flag and Character::cache_has_item_with during local bandit observation; the exact symbolized build and originating invalidation remain unproved. A three-member scout dispatch shortly beforehand is correlated, not yet proven to be the faulting two-member structural-sortie observation. A pre-crash save is not a runtime snapshot.
  - Required mechanism: Bind the actual executable, symbols/configuration and dirty source; trace the item/reference from creation or deserialization through inventory/container/cache and character/NPC ownership to the faulting use. Use the original dump/log/setup audit and preserved save as evidence, disposable copies for reproduction, and the ordinary wait-through-dispatch sequence reported by Josef. Distinguish invalid serialized state, loader behavior, runtime movement/destruction/cache invalidation and a faulty caller. Repair the earliest demonstrated lifetime boundary. Preserve dispatch, perception, threat observation, inventory contents and normal simulation; do not bypass the caller, suppress the exception or treat a null check on a nonzero invalid address as a repair. Cannibal ecology and imported-save corruption require causal evidence, not proximity.
  - Proof: Establish a failing-before/passing-after regression through the identified owner and observed trigger, with relevant item/location/cache and character/outing controls. Validate the actual corrected Windows ordinary-wait sequence through dispatch/materialization and the implicated subsequent observation, binding executable/source, world/actor/item identity and native events. Verify inventory validity/conservation and save/reload where the diagnosis implicates them. A compile, Mac-only pass, forced encounter or screenshot cannot close Windows native proof. If the exact original trigger cannot be reproduced, report the precise missing state/symbol boundary and use causal evidence proportionate to the claim; do not invent a successful native run.

Existing camp patrol/Locker/zombie acceptance is retained at its original scope. Compare the
closest earlier agent dispatch attempts and their actual received briefs/build/world/actor state
against this ordinary-player trigger; identify what differed and why our proof did not expose
it. An earlier wait or scheduler dispatch is not proof of equivalent materialized actors/items
and subsequent observation. Preserve contrary evidence, including attempts that did cross a
natural dispatch boundary. Correct a demonstrated scenario, stop-condition, handoff or acceptance
gap at its original owner; do not impose a new broad matrix or assume all prior work was invalid.
<!-- DE67:DFS-SLICE:END id=R-CAOL-WINDOWS-DISPATCH-CRASH-S001 claim=R-CAOL-WINDOWS-DISPATCH-CRASH -->
'''
(c/'FS.md').write_text(fs)
wec=(p/'baseline/WEC.md').read_text();marker='<!-- DE67:OWNER-CONTRACT:END -->'
wec=wec.replace(marker,'''Owner crash priority, 2026-09-24, pending delivery evidence: WINDOWS-CAMP-ITEM-CRASH-20260924 is the first delivery priority after this mutation, before unrelated game features, optional cleanup and launcher work. Josef only waited until dispatch; preserve that ordinary-player trigger. Sol owns source/dump/lifetime diagnosis and the smallest supported repair; GPT-6 Luna owns any live reproduction. Deliver R-CAOL-WINDOWS-DISPATCH-CRASH-S001 and compare the closest prior agent proof/attempts against the actual Windows build, world and actor/item state to explain the escape. Do not delay a supported crash fix for speculative attribution. Preserve current work/ownership and all scoped acceptances. Current Windows/WSL reservation remains in force: use already retrieved artifacts and Mac-side diagnosis now, retain the exact Windows native verification boundary for authorized access, and do not silently waive it or demote the crash. Keep this priority pending until responsible workers acknowledge it and return applied evidence or the exact remaining boundary.
'''+marker)
(c/'WEC.md').write_text(wec)
ledger=(p/'baseline/work-ledger.md').read_text();first='- [ ] R-CAOL-REFERENCE-CAMP —';i=ledger.index(first)
block='''- [ ] R-CAOL-WINDOWS-DISPATCH-CRASH — FIRST DELIVERY PRIORITY: fix the owner's ordinary-wait Windows camp crash and explain the missed coverage.
  - DFS slices: `R-CAOL-WINDOWS-DISPATCH-CRASH-S001`
  - Assignment R-CAOL-WINDOWS-DISPATCH-CRASH-exploration-001: Sol owns the urgent causal diagnosis and smallest supported lifetime repair. Begin with `.de67/task-logs/owner-scout-crash-20260924/triage.md`, original owner report, verified dump/register/module facts and source/setup snapshots; use `.de67/task-logs/review-owner-cc464a7ab66d/report.md` for prior-proof comparison and current limits. Identify the invalid item/reference owner, bind the dirty Windows build to symbols/source, and distinguish cached location, container, character/NPC lifetime, loader or caller defects. Preserve the original dump, executable, archives and pre-crash save; inspect or instrument disposable copies only. Josef did nothing except ordinary waiting until dispatch. Correlate exact site/job/actor identity rather than equating the three-member scout log with the reported two-member structural-sortie stack. Repair only a supported defect and prove failing-before/passing-after behavior through the real owner; preserve item contents, dispatch and perception. Delegate every live harness operation to GPT-6 Luna. The current Windows/WSL owner reservation still blocks DE67 execution there: proceed with Mac code/evidence diagnosis and tests, record missing remote source/symbol/runtime evidence precisely, and coordinate authorized Windows reproduction without disturbing Josef. Do not declare the crash fixed from a Mac-only pass. Compare earlier natural-dispatch attempts, not just the narrow accepted zombie witness, and report the first concrete build/world/branch or proof-boundary difference; no generic test-more recommendation. Return a source-bound diagnosis, patch/regression evidence, exact Windows proof or unresolved boundary, and retained process/evidence ownership. Do not mutate installed DE67 tooling or acceptance records.
  - Priority and recovery: This assignment precedes unrelated work. Preserve returned bulk-item and Smartzone work for normal ingress; they do not inherit failure or lose acceptance because this crash is urgent. A genuine host/evidence blocker permits independent useful work after the exact crash next step is retained. Do not restart the old harness campaign or silently classify the original save as corrupt.
  - Subtasks:
    - [open] bind-crash-and-reference-owner :: Correlate executable/symbols, reported stack, exact actor/item lifetime and ordinary wait trigger using original artifacts.
    - [open] repair-proven-lifetime-boundary :: Implement the smallest supported correction with failing-before/passing-after regression and relevant conservation controls.
    - [open] prove-windows-wait-through-observation :: On authorized Windows access, validate source-bound native dispatch and subsequent implicated observation; retain exact limits until proved.
    - [open] explain-prior-proof-difference :: Compare original agent context/runs with Josef's path, preserve contrary natural-dispatch evidence, and correct the earliest demonstrated coverage or handoff gap.

'''
ledger=ledger[:i]+block+ledger[i:]
ledger='''Current owner priority, 2026-09-24: the Windows camp dispatch crash is first after mutation. The marked WEC owner contract and R-CAOL-WINDOWS-DISPATCH-CRASH assignment below govern; preserve existing returned work and accepted proof. Windows/WSL reservation remains explicit, so start the authorized Mac-side diagnosis immediately and retain Windows verification as open until access is authorized.

'''+ledger
(c/'work-ledger.md').write_text(ledger)
queue=(p/'baseline/mutation-suggestions.md').read_text();(c/'mutation-suggestions.md').write_text(queue.split('## Pending suggestions')[0]+'## Pending suggestions\n')
