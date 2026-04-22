# Bandit extortion playthrough audit + harness-skill packet v0 (2026-04-22)

Status: greenlit backlog contract.

## Why this exists

Even with a real extortion scene and a real restage point, the job is still half-baked if only one person remembers the right incantations.
Andi should be able to drive the whole chain deliberately:
bring bandits in, hit the first demand, choose pay or fight, observe defender loss if it happens, and play through the reopened higher-demand scene with the second reconsideration fork.

This packet exists to package that full playthrough surface into named audits plus skill/docs updates so the knowledge does not stay trapped in chat or one operator's head.

## Scope

1. Land one named full-playthrough audit packet for the Basecamp extortion chain.
2. Package a reviewer-usable path that can exercise at minimum:
   - approach or stand-off arrival at camp
   - first `pay` versus `fight` demand
   - one fight-forward branch
   - one defender-loss / lowered-threat case that reopens the scene with a higher demand
   - the second explicit reconsideration fork where the player can again `pay` or `fight`
3. Keep screen proof, deterministic proof where applicable, and artifact/report proof clearly separated instead of flattening everything into one soup verdict.
4. Update the usable harness-facing documentation so later agents can actually run the path:
   - `skills/c-aol-harness/SKILL.md`
   - `tools/openclaw_harness/CONTROL_LOOKUP.md`
   - any relevant harness docs/readme surface if the packet lands new named scenarios or operating rules
5. Keep the packet focused on making the extortion chain playable/testable/teachable, not on reopening feature design arguments the earlier packets already settled.

## Non-goals

- inventing new robbery mechanics by stealth while writing the audit
- giant fully scripted automation that fakes every combat outcome
- hiding the whole method inside one brittle operator-only macro
- radio warfare, stalker pressure, or broader social-horror packaging
- replacing real gameplay proof with documentation-only theater
- treating missing deterministic hooks as proof that the live scene did not happen

## Success shape

This packet is good enough when:
1. one named audit/playthrough surface exists for the full Basecamp extortion chain rather than only for the first setup moment
2. the packaged flow can cover first demand, fight branch, defender-loss reopen, raised-price second demand, and second `pay` versus `fight` reconsideration clearly enough for review
3. reports separate screen, tests, and artifacts/logs cleanly enough that later debugging does not devolve into folklore
4. the relevant harness skill/docs are updated so another agent can discover the named paths and run them without archaeological guessing
5. the slice stays bounded and does not widen into a fake total-automation empire or a feature-redesign side quest

## Validation expectations

- prefer at least one honest live playthrough packet/report that reaches the reopened higher-demand scene, not just a first-demand smoke test
- if some branches still need human steering rather than full automation, the packet should say so plainly and package the steering steps instead of bluffing repeatability
- the updated harness skill/docs should name the exact scenario/fixture/report surfaces rather than waving toward `the harness somehow`
- the important reviewer question is whether Andi can now deliberately restage and audit the whole extortion chain instead of re-deriving the ritual each time

## Current grounded note

The current harness skill already distinguishes deterministic tests, live behavior, and artifact/log evidence, and it now knows about the nearby-restage helper names for the active live-world packet.
This packet should extend that same discipline to the robbery chain rather than spawning a second secret operator playbook.

## Dependency note

This queued packet sits directly behind `doc/bandit-extortion-at-camp-restage-handoff-packet-v0-2026-04-22.md` and depends on the earlier robbery-chain packets being real enough to play through.
It is the audit/teaching follow-through, not the first setup seam itself.
