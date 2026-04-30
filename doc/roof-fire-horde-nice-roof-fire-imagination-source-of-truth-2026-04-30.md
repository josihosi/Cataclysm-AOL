# Roof-fire horde nice-fire playtest imagination source — 2026-04-30

## Lived scene

The player has made a proper, visible roof fire: not a debug sparkle, not a hidden metadata token, but a believable elevated blaze on a roof/elevated surface that a human would expect to draw attention. The horde is somewhere out there in the dark pressure field. Time passes. The important product question is simple: does the roof fire feel like it matters to the horde?

A good playtest feels like this:
- the roof fire is inspectable before the wait and visibly/metadata-honestly exists on elevated roof footing;
- a horde is present before the wait, close enough that response is plausible but not so close that the result is just collision noise;
- after bounded time passage, the horde state changes in a way attributable to the roof-fire signal: destination, processed turn, movement budget, tracking/interest metadata where available, and log/artifact evidence;
- the run reports cost and stability so we know the proof did not buy drama by tanking the game.

## Failure smells

- The fire is injected as `fd_fire` without preserving the player-created/elevated-fire chain.
- The horde is merely present, while no direct horde before/after response is captured.
- The result depends on rerunning the old mixed-hostile soup and calling setup proof behavior proof.
- The proof overclaims positive `tracking_intensity` if the game does not actually expose or update it.
- The wait is a one-turn poke instead of bounded visible time passage.
- The scene is technically green but product-dead: no visible or saved pressure, no useful report, no cost readout.

## Review questions

- Would Josef believe, from the artifacts, that there was a nice roof fire and a horde that reacted to it?
- Is the credited fire still tied to the real roof/elevated fire path from the previous green proof, or clearly rebuilt through an equally honest player-action path?
- Did the horde move/retarget/process because of the roof-fire signal, or did we only prove it existed nearby?
- Are any missing fields called out as missing instead of laundered into success?
