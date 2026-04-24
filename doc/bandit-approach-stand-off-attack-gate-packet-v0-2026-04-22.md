# Bandit approach / stand-off / attack-gate packet v0 (2026-04-22)

Status: checkpointed / done for now.

## Why this exists

Once the current live bandit-owner seam is honest enough to send real groups into the world, the next missing product question is not yet the toll window itself.
It is the approach law right before that.

If bandits can notice value but have no explicit local pressure posture, the robbery chain will feel stupid in one of two directions:
- psychic instant dogpile nonsense
- decorative extortion UI that appears without an honest approach state

This packet exists to freeze one bounded local pressure law for bandit groups approaching a player, Basecamp, convoy, or similarly exposed target.
The point is to decide when they stalk, hold off, probe, open a shakedown, attack directly, or abort.

## Scope

1. Land one bounded local approach/pressure layer on top of the live bandit control seam.
2. Make the local posture explicit and reviewer-readable, using a small set such as `stalk`, `hold_off`, `probe`, `open_shakedown`, `attack_now`, or `abort` rather than haunted implicit behavior.
3. Base the posture change primarily on dispatch strength versus local threat/opportunity instead of on folklore or a fixed one-size-fits-all aggression toggle.
4. Keep ordinary pressure readable at a stand-off distance: the group should be allowed to shadow, watch, and look for weakness without crowding the player immediately or behaving like it knows every future visible tile.
5. Allow a bounded sight-avoid / recent-exposure heuristic if needed for stalking flavor, but keep it explicitly non-magical and subordinate to the real local scene.
6. Make convoy / vehicle / rolling-travel contexts more attack-forward by rule, so those scenes can skip the polite shakedown posture when they honestly read as a moving ambush opportunity.
7. Keep this packet focused on the gate law that decides `when a robbery scene should happen at all`, not yet on the payment UI or the later aftermath law.

## Non-goals

- the pay-or-fight trading window itself
- loot valuation or exact surrender-pool law
- giant tactical stealth doctrine or magical perfect sight avoidance
- radio deception, writhing-stalker pressure, or broader social-horror systems
- full local combat AI rewrite
- multi-camp coordination or faction grand strategy

## Success shape

This packet is good enough when:
1. one honest local approach/stand-off/attack-gate layer exists on top of the live bandit control seam
2. dispatched bandit groups can reviewer-readably choose among stalking, holding off, probing, opening a shakedown, attacking directly, or aborting
3. the gate law is explicit enough that later packet work can answer `why did this become a shakedown instead of a fight` without folklore reconstruction
4. ordinary Basecamp pressure does not feel like instant psychic tile collapse, because the group can keep bounded stand-off behavior while still looking dangerous
5. convoy or moving-travel contexts are allowed to skip the polite shakedown posture when they honestly read as rolling ambush opportunities
6. the slice stays bounded and does not widen into full stealth doctrine, radio play, singleton stalker work, or a broad combat-AI rewrite

## Validation expectations

- prefer live or multi-turn proof on real player-present contexts instead of a purely abstract state table
- cover at minimum one Basecamp or camp-adjacent scene and one travel / convoy-style scene so the attack-gate split is not only proven on one toy context
- reviewer-readable output should show the starting posture, the pressure inputs, and the final local gate decision clearly enough that later tuning is about taste rather than basic confusion
- if bounded sight-avoid behavior is introduced here at all, validation should prove it is heuristic/local rather than magical future-cone omniscience

## Dependency note

This queued packet sits behind the already-frozen hostile-site stack led by `doc/multi-site-hostile-owner-scheduler-packet-v0-2026-04-22.md`.
Radio warfare, `writhing stalker`, and the broader Arsenic-and-Old-Lace social-threat bank stay parked for later.
