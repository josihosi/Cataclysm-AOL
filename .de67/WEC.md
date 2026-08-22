# WEC

*User intent and language brief*

## User outcome

Playtesting C-AOL should preserve proof that already exists, make failures easy to diagnose, and make the final integrated test easier to run. The parts may be tested separately while they are built, but final automated acceptance must prove one continuous round.

This applies to C-AOL as a whole. Hostile ecology is the first demanding vertical slice because actors cross between the overmap and the reality bubble, survive save and relaunch boundaries, and must still complete a long causal lifecycle.

## Intended experience

During development, agents can run small focused tests and diagnostic replays without repeating unrelated gates that are already proven on the same compatible binding. The system records what was proven, where the first divergence occurred, and which compatible diagnostic capsule is the best place to investigate from.

The agent chooses the latest compatible diagnostic capsule and records why it chose it. A diagnostic capsule or replay has zero final-certification credit. Checkpoint segments may not be spliced together to manufacture a passing round.

When the implementation reaches a proven certification boundary, the automated certification gate runs the entire required lifecycle as one bound, uninterrupted round. Ordinary save, quit, and relaunch are allowed when they are part of that same round. Checkpoint rollback, segment splicing, code or data changes, fixture or scenario changes, replacement worlds, and replacement player or actor identities are not allowed.

After automated certification passes, Josef performs a separate Windows feel pass in ordinary play. Exploratory free play may happen earlier, but it does not replace either final gate.

## Two final gates

- **Automated certification gate** — one continuous bound round proves the complete required lifecycle and all named proof gates.
- **Windows feel gate** — a separate ordinary-play pass on Windows establishes whether the result feels understandable, coherent, and enjoyable.

Neither gate substitutes for the other.

## Continuity and binding

One certification round has one scenario lineage and one compatible binding. The binding must cover the relevant code, data, executable, harness, fixture, scenario, world or save, player identity, and identity-bearing ecology actors.

Across every reality-bubble crossing and every permitted save, quit, and relaunch, each identity-bearing actor keeps the same durable identity and has exactly one authoritative simulation owner.

Offscreen aggregate simulation may represent populations, resources, pressure, or probability. It may not substitute for actor-level lifecycle evidence when the claim concerns a particular actor or group completing a transition.

Any relevant binding change invalidates the certification round and requires a fresh continuous round. It does not erase useful focused proof or diagnostic history; it only removes final-certification credit from incompatible evidence.

## Diagnostics

A failed run reports the first causal divergence, the last proven gate, the expected and observed states, the relevant actor identities and ownership state, the selected compatible diagnostic capsule, and the smallest next probe. It does not flood the report with repeated transport actions or identical log lines.

The system should distinguish setup support, build proof, synthetic proof, focused feature proof, automated continuous-round certification, and Windows feel evidence. A built binary, startup screenshot, helper result, or focused test earns only its own evidence class.

## Project language and terminology

Use these terms consistently:

- **Diagnostic capsule** — a bound preserved state used to investigate from a known point.
- **Diagnostic replay** — a run from a diagnostic capsule that earns no final-certification credit.
- **Continuous certification round** — the single bound execution used by the automated certification gate.
- **Automated certification gate** — the machine-verifiable final integrated gate.
- **Windows feel gate** — Josef's separate ordinary-play judgment gate.
- **First divergence** — the earliest failed causal expectation.
- **Binding** — the complete identity of the code, runtime, data, harness, scenario, world, and actors relevant to evidence compatibility.
- **Authoritative simulation owner** — the one layer currently allowed to advance an identity-bearing actor.

Avoid calling a diagnostic replay a resume of final certification. Avoid calling assembled segments a continuous round.

## Decisions

- Focused tests preserve useful development proof but never replace the final continuous round.
- Diagnostic capsules are agent-selected recommendations, not automatic authority.
- Checkpoint rollback and segment splicing receive zero final-certification credit.
- Normal save, quit, and relaunch may occur inside one continuous round when the binding and identities remain unchanged.
- Final acceptance has two separate gates: automated certification and Windows feel.
- Hostile ecology is the first vertical slice, but the resulting playtesting system is for all C-AOL.
- Identity-bearing actors must retain durable identity and exactly one authoritative owner across overmap, reality-bubble, and persistence transitions.

## Prototype or reaction questions

Concrete prototypes should resolve:

- How an agent sees already-proven focused gates without mistaking them for final certification.
- How the report presents the first divergence and recommends a compatible diagnostic capsule.
- How a continuous round proves identity and single-owner continuity across bubble crossings and save or relaunch boundaries.
- How the final Windows feel handoff stays ordinary and understandable rather than becoming another scripted checklist.

## Handoff to DE-67-2

Inspect the current harness, registry, transition-event stream, scenario tooling, save and relaunch paths, process ownership, evidence storage, worktree use, overmap actor state, local actor materialization and dematerialization, and current tests before specifying mechanisms.

Compare unfamiliar implementation patterns against live open-source game code instead of relying on memory. In particular, research overmap or world simulation plus a loaded reality bubble or scene projection. Useful comparison candidates include Veloren persistent simulation entities and loaded actors, Luanti active and stored objects, OpenMW durable actor or cell state and scene projection, and aggregate sector or wave simulations as a negative comparison. Use primary source repositories or official technical documentation, record precise links or commit references, and treat comparisons as design evidence rather than authority over C-AOL code.

The mechanistic specification should test the likely invariant that a durable actor record is authoritative while a local actor is a temporary projection or lease; transitions are explicit and idempotent; the source is retained until destination acknowledgement and persistence; load reconciliation repairs or rejects duplicates; and compact crossing receipts expose identity, generation, prior owner, next owner, and outcome. Accept, reject, or refine this direction from the inspected C-AOL code.

No product or test implementation belongs in Phase 2.
