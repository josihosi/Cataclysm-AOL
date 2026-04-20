# Bandit first 500-turn playback proof v0 (2026-04-20)

Status: active greenlit contract.

## Promotion audit verdict

The smoke bridge is checkpointed.
The light bridge is checkpointed.
The human / route bridge is checkpointed.
The repeated-site reinforcement bridge is checkpointed.
The next narrow bandit slice is the first honest 500-turn playback proof on the current abstract seams.

This should not become a fake world-sim milestone.
The point is to prove that the current bounded mark-generation / playback / evaluator footing can stay sane over a longer horizon, using named deterministic scenarios and reviewer-readable drift output instead of another pile of short-horizon vibes.

## Scope

1. Extend the current deterministic playback packet to include a first honest 500-turn proof horizon.
2. Reuse a small named scenario set on the existing abstract seam, initially the current smoke-cooling, city-edge peel-off, and repeated-site-boundedness cases unless later evidence proves a different trio is more honest.
3. Expose reviewer-readable checkpoints so long-horizon drift is legible without debugger soup.
4. Prove that the long-horizon packet stays bounded: no magical settlement truth, no immortal pressure theater, and no free extraction unlocks from ordinary signal reinforcement.

## Non-goals

- a broader overmap simulator
- new smoke/light/human-route adapter families
- live harness theater as the first answer
- persistence architecture rewrite
- raid/settlement AI expansion just because 500 turns sounds important

## Success shape

This item is good enough when:
1. at least one honest deterministic 500-turn playback packet exists on the current abstract bandit seams
2. deterministic coverage proves the chosen long-horizon scenarios stay bounded, including cooldown/peel-off/reinforcement behavior that does not magically harden into free raid truth
3. reviewer-readable report output exposes the long-horizon checkpoints and winner drift cleanly enough to inspect the 500-turn answers without spelunking through internals
4. the slice stays visibly bounded and does not smuggle in broader world-sim, persistence, or live-probe theater

## Validation expectations

- prefer the existing deterministic playback footing over new harness work
- the first honest code validation should stay `make -j4 tests` and `./tests/cata_test "[bandit]"`
- if the long-horizon packet gets slow or noisy, fix the reporting/probe shape before pretending broader evidence is needed

## Dependency note

This slice comes after:
- `doc/bandit-smoke-visibility-mark-seam-v0-2026-04-20.md`
- `doc/bandit-light-visibility-mark-seam-v0-2026-04-20.md`
- `doc/bandit-human-route-visibility-mark-seam-v0-2026-04-20.md`
- `doc/bandit-repeated-site-activity-reinforcement-seam-v0-2026-04-20.md`

It should reuse the already-checkpointed mark-generation, playback, evaluator, pursuit-handoff, and perf-budget seams rather than reopening them.