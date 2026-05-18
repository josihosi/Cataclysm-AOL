# Bandit overmap-to-bubble pursuit handoff seam v0 (2026-04-20)

Status: greenlit backlog contract.

## Why this exists

Once the bandit lane has a real writer-side mark model, the next honest question is how one bounded abstract group crosses into local play and comes back with consequences that matter.

The right first anchor is pursuit / investigation, not a full raid circus.
That keeps the first handoff narrow, explainable, and compatible with existing pursuit-style footing.

## Scope

1. Implement one bounded pursuit / investigation handoff from abstract overmap group state into local play.
2. Carry a small explicit entry payload such as source camp, group/job identity, confidence, current mark, return clock, and light continuity state.
3. Carry a small explicit return packet for updated threat/mark knowledge, cargo outcome, losses, panic, and retreat state.
4. Keep continuity group-first, with at most a tiny anchored-identity slice where that is honestly needed.
5. Prove the handoff and return path on deterministic or similarly controlled footing before asking for broader live drama.

## Non-goals

- full raid / ambush / extortion mode suite
- broad local tactical AI rewrite
- full per-bandit persistent biography across every step
- visibility / concealment implementation
- broad encounter variety or balance theater

## Success shape

This item is good enough when:
1. one honest pursuit / investigation handoff exists from abstract group state into local play
2. the return path preserves meaningful consequences instead of dropping them on the floor
3. continuity rules stay explicit, small, and reviewer-readable
4. the slice stays bounded and does not quietly turn into the whole bandit combat machine

## Validation expectations

- prefer deterministic or tightly controlled handoff proof first
- reuse current bandit evaluator / playback footing where practical for the overmap side
- keep the entry payload and return packet inspectable in tests or reviewer-readable summaries
- escalate to broader live proof only if deterministic or tightly controlled evidence cannot honestly answer the seam question

## Dependency note

This queued follow-up sits behind `doc/bandit-mark-generation-heatmap-seam-v0-2026-04-20.md`.
Do not pull it forward before the writer-side mark seam is real, or the handoff lane will end up faking the overmap state it is supposed to inherit.
