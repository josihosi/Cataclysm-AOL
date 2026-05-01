# CAOL-VISIONS-PLAYTEST-SAMPLER-v0 (2026-05-01)

Status: GREENLIT / QUEUED / PRODUCT-TASTE PLAYTEST PACKET

Owner when promoted: Andi for harness/package prep; Schani for sampler framing and Josef handoff.

Imagination source: `doc/caol-visions-playtest-imagination-source-2026-05-01.md`.

## Intent

Create a small C-AOL “visions” playtest sampler: a handful of prepared, labelled live/handoff scenes that let Josef judge whether the current C-AOL direction feels alive, fair, fun, and readable.

This is not a demand to reprove every closed lane. It is a product-feel sampler over already-built or nearly-built visions.

## Scope v0

Prepare 3-5 labelled postcards, choosing the smallest useful set from:

1. Zombie rider late-world terror / counterplay.
2. Writhing stalker zombie-shadow predator.
3. Flesh raptor circling skirmisher.
4. Basecamp/camp locker practical usefulness.
5. Bandit/basecamp pressure and living-world risk.

For each postcard, provide:

- scenario/handoff name or save/setup path;
- one-sentence intended vision;
- what Josef should try for 2-5 minutes;
- 3-5 taste questions;
- at least one screenshot checkpoint with a named expected visible/optical fact;
- known caveats / staged-footing warning;
- artifact/log path if agent-side proof already exists;
- “do not overinterpret” boundary.

## Optical / screenshot condition

Josef clarified that “vision” also means optical/visual playtest evidence. The sampler must therefore include screenshot-visible criteria, not only logs, tests, or debug artifacts.

For each visual postcard:

- define the intended visible read, e.g. “rider pressure is legible at distance”, “stalker pressure is visible without omniscient beeline nonsense”, “raptor orbit reads as circling/skirmishing”, “camp usefulness is visible in the UI or NPC/equipment state”;
- capture or request screenshots at the decisive moment;
- label each screenshot with the expected visible fact;
- record whether the screenshot actually supports the optical read, is ambiguous, or shows the wrong thing;
- include a gnostic taste check: whether the scene carries occult/inner C-AOL meaning and not only visible mechanics.

## Evidence classes

Use C-AOL harness discipline:

- **Agent-side footing:** existing green scenario rows, startup/load, deterministic tests, or fresh handoff setup.
- **Josef taste:** fun, fairness, readability, optical/screenshot read, gnostic/occult inner meaning, annoyance, vibe, and whether the thing feels like the intended C-AOL vision.
- **Not claimed:** natural random discovery, release readiness, full playthrough balance, or regression closure for unrelated lanes.

## Success state

- [ ] The sampler chooses a bounded v0 set of 3-5 postcards instead of becoming open-ended “play the whole mod”.
- [ ] Each postcard has a named vision, a prepared scenario/handoff or exact setup recipe, and a short Josef-facing play instruction.
- [ ] Each visual postcard has at least one screenshot checkpoint with a named expected optical/visible fact.
- [ ] Each postcard has focused taste questions that distinguish fun/readable/fair/alive/optically-legible/gnostic from annoying/fake/unfair/invisible/visually-confusing/hollow.
- [ ] Existing closed proof rows are cited as footing where used, without laundering staged rows into natural discovery claims.
- [ ] If fresh handoff runs are created, each run records artifact dir, cleanup/handoff status, and one visible fact or explicit reason visual proof is not the evidence class.
- [ ] The final Josef handoff is short enough to actually use.

## Non-goals

- Do not reopen closed v0 lanes merely because they appear in the sampler.
- Do not interrupt the active camp-locker API-reduction lane unless Schani/Josef explicitly promote this sampler as next active execution work.
- Do not create a release package.
- Do not ask Josef to inspect logs as the primary playtest activity.
- Do not turn this into a hidden tuning pass; taste findings become follow-up packets if needed.

## Recommended v0 postcard order

1. **Writhing stalker** — fastest to answer “cheating goblin or fair dread?”
2. **Zombie rider** — strongest late-world fantasy and counterplay check.
3. **Flesh raptor** — annoyance-vs-fun comparator.
4. **Basecamp/camp locker** — include only if active cleanup has reached a stable checkpoint.
5. **Bandit/basecamp pressure** — include if the sampler wants a living-world scene rather than just creature behavior.

## First implementation step when promoted

Build `doc/caol-visions-josef-playtest-card-v0-2026-05-01.md` from existing green scenario evidence, including screenshot/optical checkpoints, then run only the missing `handoff` commands needed to leave live sessions playable. Stop after the smallest usable sampler; future postcards can be v1. Treat Josef's “gnostic” axis as a taste question: occult/inner meaning, omen-like wrongness, or hidden-world intelligibility; record it separately from optical and mechanical proof.
