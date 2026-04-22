# Bandit extortion-at-camp restage + handoff packet v0 (2026-04-22)

Status: greenlit backlog contract.

## Why this exists

Once the robbery-chain packets are real, Andi still needs a usable stage, not just a nice idea.
The harness should be able to do more than spawn one hostile body nearby and shrug.
It should be able to attract a real controlled bandit group toward Basecamp and leave the scene alive at the interesting point so somebody can actually play through it.

This packet exists to make that playtest setup honest and repeatable.
The point is not generic scenario-authoring glory.
The point is one bounded restage/handoff surface for the real extortion-at-camp path.

## Scope

1. Land one bounded harness/restage packet for the player-present Basecamp extortion chain.
2. Let the harness intentionally attract a **real controlled bandit group** toward the current Basecamp/player footing through the live owner/dispatch path instead of through disconnected debug-spawn theater.
3. Provide one reviewer probe/capture path and one manual handoff path for that setup.
4. Leave the handoff session alive at a genuinely useful moment, such as:
   - bandits approaching the camp
   - bandits in stand-off / pressure posture
   - bandits opening the shakedown scene
5. Keep the setup tied to the honest current world footing rather than relocating the player/basecamp in a way that breaks `game::validate_camps()` or otherwise escapes the real product seam.
6. Make the generated reports/artifacts explicit enough that later packet work can tell whether the scene used the real live-control path, which setup mode ran, and what state the encounter reached before handoff.
7. Keep the helper surface named and reviewer-usable rather than living only as operator folklore.

## Non-goals

- generic world-authoring infrastructure for every imaginable hostile scene
- fake debug-spawn shortcuts that bypass the real controlled-bandit path
- closing the whole robbery chain by helper polish alone
- broad Basecamp AI redesign
- radio warfare, writhing-stalker pressure, or wider social-horror feature work
- silently treating a broken moved-player/basecamp relocation as honest product proof

## Success shape

This packet is good enough when:
1. one named restage path can attract a real controlled bandit group toward Basecamp through the actual live owner/dispatch seam
2. one reviewer probe/capture command and one manual handoff command exist for that same path
3. the handoff path leaves the session alive at a genuinely useful extortion-at-camp moment instead of cleaning up before play starts
4. the setup does not rely on fake debug-spawn theater or on moved-player/basecamp hacks that break current camp validity
5. reviewer-readable reports make it obvious which setup mode ran, which real controlled group was used, and whether the scene reached approach, stand-off, or shakedown state
6. the slice stays bounded and does not widen into a general harness empire

## Validation expectations

- prefer current-build live proof on the real controlled-bandit seam, not only dry-run scenario definitions
- validation should show both setup modes: probe/capture and manual handoff
- if the setup still depends on a tricky transform or nearby-restage helper, reviewer output should make that visible instead of hiding it as operator lore
- the important product question is whether Andi can reliably bring a real controlled bandit group to camp and then keep the session alive long enough to matter

## Current grounded note

The active lane already has the first nearby-restage helper surface (`bandit_live_world_nearby_camp_v0_2026-04-22` / `bandit.live_world_nearby_camp_mcw`), plus honest probe-versus-handoff split, but the current moved-player McWilliams attempt still trips `game::validate_camps()` when the original basecamp gets thrown out of bubble.
So this packet must consume the active lane's nearby-control work instead of papering over that failure with prettier helper lore.

## Dependency note

This queued packet sits behind the current robbery-chain stack, especially `doc/bandit-aftermath-renegotiation-writeback-packet-v0-2026-04-22.md`, and it also depends on the active `Bandit live-world control + playtest restage packet v0` lane becoming honest about nearby controlled camp setup first.
It is support work for the real extortion path, not a substitute for the path existing.
