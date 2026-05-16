# CAOL-BANDIT-SCENIC-SHAKEDOWN-CHAT-OPENINGS-v0 (2026-05-01)

Status: CLOSED AS SCENIC-UI CONTRACT / SUPERSEDED BY `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0` FOR FINAL RESPONSE + PAYMENT CONTRACT

Source: Josef Discord, 2026-05-01: “the bandit shakedown. Can we make it in normal chat window and more scenic? A selection of bandit openings.”

## Intent

Turn the bandit shakedown from a proof-shaped pay/fight surface into a more scenic in-game encounter that uses the normal chat/dialogue window path where possible and can open with several bandit-flavored approaches.

The desired player experience is not just “a toll menu appears.” The bandits should feel like people/group-pressure entering the scene: watching, stepping out, surrounding, calling across the street, pressing a wounded camp, or using leverage before the pay/fight fork.

## Scope v0

- Route the visible shakedown/opening through the normal NPC chat/dialogue window path where feasible, or document exactly why a different UI is required.
- Add a small selection of bandit opening beats, chosen from situation/profile context rather than one generic line.
- Keep the mechanical fork clear. Historical wording here said pay/fight/refuse, but the active superseding contract requires visible Pay/Fight only; Esc/backout/refuse enters the hostile branch without a third visible response.
- Preserve scenic shakedown footing: demanded toll context, pay option, fight option, and aftermath/reopened-demand behavior. Historical hidden reachable-goods surrender is superseded by the active trade/debt-style payment contract.
- Add optical/screenshot proof for at least one normal-chat shakedown opening and one variant opening.

## Candidate openings

- **Roadblock / toll:** bandits step out and demand a cut before letting the player pass.
- **Basecamp pressure:** bandits use the camp, supplies, or workers as leverage.
- **Seen-you-before / reopened demand:** prior violence or defender loss makes the second demand colder and higher.
- **Weakness read:** bandits comment on visible vulnerability, wounds, overloaded travel, or poor odds.
- **Warning from cover:** bandits do not immediately stand adjacent; they open from a safer posture before closing the fork.

## Success state

- [ ] Shakedown opening reaches a normal chat/dialogue window path where feasible, or an explicit product-path reason says why not.
- [ ] At least three distinct opening beats exist and are selected from scenario context, not pure random decoration.
- [ ] The scenic shakedown fork remains legible. Superseded correction: final visible responses must be Pay/Fight only, and Pay must open the trade/debt-style surface required by `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`.
- [ ] First-demand and reopened/higher-demand shakedown paths both retain deterministic/harness proof.
- [ ] Snapshot proof includes at least one normal-chat shakedown opening and one scenic variant, with named optical facts.
- [ ] No cannibal/no-shakedown profile or unrelated bandit attack posture regresses into polite toll UI.

## Non-goals

- Do not redesign the whole bandit faction economy.
- Do not require release packaging.
- Do not reopen closed bandit map-risk/reward rows except where their shakedown surface is directly touched.
- Do not turn scenic openings into lore walls; the scene still needs to play.

## Initial proof targets

- Deterministic/UI test for opening selection context.
- Harness row for first-demand chat-window opening.
- Harness row for reopened higher-demand scenic opening.
- Screenshot suite update for Josef: normal chat window, scenic opener, and visible fork. Superseded correction: the final fork must be Pay/Fight only under `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`.
