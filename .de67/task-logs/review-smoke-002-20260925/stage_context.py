from pathlib import Path
import shutil
r=Path(__file__).resolve().parent; w=r.parents[2]
installed=Path('/Users/josefhorvath/.codex/skills/de67/de-67-3')
for name in ('method-baseline','method-candidate'):
 shutil.copytree(installed,r/name,ignore=shutil.ignore_patterns('__pycache__','.pytest_cache','.git'),dirs_exist_ok=True)
c=r/'method-candidate'; p=c/'scripts/policy_kernel.py';s=p.read_text()
s=s.replace('def current_owner_contract(workspace: Path) -> str:', 'def current_owner_contract(workspace: Path, *, audience: str = "coordinator") -> str:')
s=s.replace('    path = workspace / ".de67/WEC.md"\n', '    if audience not in {"coordinator", "worker"}:\n        raise PolicyError("Unknown owner-contract audience")\n    path = workspace / ".de67/WEC.md"\n',1)
needle='        body = body.strip()\n    return (\n'
replacement='''        body = body.strip()
    # Assignment instructions are coordinator context, not a worker delegation duty.
    # Preserve the original owner text; select only its explicitly marked audience.
    coord_begin = "<!-- DE67:COORDINATOR-ONLY:BEGIN -->"
    coord_end = "<!-- DE67:COORDINATOR-ONLY:END -->"
    if coord_begin in body or coord_end in body:
        if body.count(coord_begin) != 1 or body.count(coord_end) != 1:
            raise PolicyError("Coordinator owner context requires one complete marked section")
        shared_before, _, tail = body.partition(coord_begin)
        coordinator, marker, shared_after = tail.partition(coord_end)
        if not marker or coord_end in shared_before or not coordinator.strip():
            raise PolicyError("Coordinator owner context markers are empty or out of order")
        body = "\\n".join(part.strip() for part in (
            shared_before, coordinator if audience == "coordinator" else "", shared_after
        ) if part.strip())
    return (
'''
assert needle in s;s=s.replace(needle,replacement,1)
s=s.replace('owner_contract = current_owner_contract(workspace)', 'owner_contract = current_owner_contract(workspace, audience="worker")')
p.write_text(s)
p=c/'scripts/worker_library.py';s=p.read_text();assert 'current_owner_contract(workspace).encode' in s;s=s.replace('current_owner_contract(workspace).encode','current_owner_contract(workspace, audience="worker").encode');p.write_text(s)
p=c/'scripts/coordinator_supervisor.py';s=p.read_text();s=s.replace('        current_owner_contract(workspace),','        current_owner_contract(workspace, audience="coordinator"),');p.write_text(s)
# Retain original owner input in baseline, move only coordinator model-routing sentences.
s=(w/'.de67/WEC.md').read_text()
s=s.replace('Josef explicitly invites GPT-6 Astra at low effort for intricate performance work where preserving complex behavior matters. ', '')
s=s.replace('Improve demonstrated causes while preserving camp and NPC functionality; route NPC wait/sleep implementation to GPT-6 Astra.', 'Improve demonstrated causes while preserving camp and NPC functionality. NPC wait/sleep implementation remains assigned to GPT-6 Astra.')
s=s.replace('Coordinator may assign GPT-6 Astra for the coupled implementation (low recommended, effort chosen by judgment), preserve current shared-file/runtime ownership, and arrange GPT-6 Luna to repeat the full native Mac playtest on the corrected build.', 'Preserve current shared-file/runtime ownership; GPT-6 Luna performs the full native Mac playtest on the corrected build.')
s=s.replace('Josef also clarifies that selecting an Astra worker for intricate coding is a coordinator judgment, not a recursive rule telling Astra workers to spawn Astra. An Astra worker may choose an Astra helper when useful, but no standing instruction requires that pairing.', 'An assigned Astra worker owns its implementation outcome. An Astra helper remains an optional choice when useful; the assignment does not require one.')
addition='''
## Playtest observation and assignment — reviewed 2026-09-25

During playtests, monitor actual simulation-turn execution time against Josef's below-100-ms
 target. Investigate observed turns over 100 ms proportionately; reuse an established diagnosis
for repeated occurrences. Bind timings to the build/run and game state, separately from tool or
network latency, input waits and multi-turn action duration. Preserve gameplay and useful diagnostics.
At notable intervals and meaningful transitions, inspect native quicksaves and relevant metadata
for actor/job progress, ownership, targets and routes. Coordinate saves with the Luna input owner,
verify the saved turn, and use safe disposable snapshots. Compare identities and changes with native
events; distinguish stale saves or serialization limits from stalled simulation. Return concise
changes or anomalies, not repeated full-save dumps. These checks continue through substantive
play, not just closeout, and do not replace the complete agreed gameplay proof.

<!-- DE67:COORDINATOR-ONLY:BEGIN -->
Coordinator assignment authority: assign NPC wait/sleep performance implementation to GPT-6 Astra.
For other intricate performance or coupled smoke/light ecology repairs, Astra is available when
its judgment is useful; low effort is recommended, not enforced. Workers receive their assigned
outcome and owner constraints, not this model-selection instruction. Preserve Luna-only live
harness operation and useful optional helpers.
Ensure performance and saved-state observation during playtests. Create supported repair work
within the current task or the performance-improvement task according to its cause. A slowdown
linked to the current playtest is an immediate repair priority there; preserve the scene and
exclusive ownership for the handoff. Otherwise retain the diagnosis in the performance task.
Apply the same routing to supported saved-state anomalies. Compare equivalent gameplay after
repair and report remaining turns over 100 ms honestly. This is task-creation authority, not a
new acceptance gate, fixed sampling quota, background monitor or permission to suppress gameplay.
<!-- DE67:COORDINATOR-ONLY:END -->
'''.replace('\n target.', '\ntarget.')
s=s.replace('<!-- DE67:OWNER-CONTRACT:END -->',addition+'\n<!-- DE67:OWNER-CONTRACT:END -->')
(r/'WEC.candidate.md').write_text(s)
print('Staged role-scoped owner context and playtest observation guidance; live files unchanged')
