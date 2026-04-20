# Bandit first 500-turn playback proof v0 (2026-04-20)

Status: checkpointed contract.

## Promotion audit verdict

The smoke bridge is checkpointed.
The light bridge is checkpointed.
The human / route bridge is checkpointed.
The repeated-site reinforcement bridge is checkpointed.
The first honest 500-turn playback proof now exists on the current abstract seams.

This did not become a fake world-sim milestone.
The current bounded mark-generation / playback / evaluator footing now has a reviewer-readable long-horizon proof packet using named deterministic scenarios and explicit drift checkpoints instead of another pile of short-horizon vibes.

## Scope

1. Extend the current deterministic playback packet to include a first honest 500-turn proof horizon.
2. Reuse a small named scenario set on the existing abstract seam, specifically `smoke_only_distant_clue`, `city_edge_moving_hordes`, and `generated_repeated_site_reinforcement_stays_bounded`.
3. Expose reviewer-readable checkpoints so long-horizon drift is legible without debugger soup.
4. Prove that the long-horizon packet stays bounded: no magical settlement truth, no immortal pressure theater, and no free extraction unlocks from ordinary signal reinforcement.

## Non-goals

- a broader overmap simulator
- new smoke/light/human-route adapter families
- live harness theater as the first answer
- persistence architecture rewrite
- raid/settlement AI expansion just because 500 turns sounds important

## Landed shape

This item is now good enough because:
1. an honest deterministic 500-turn playback packet exists on the current abstract bandit seams through `proof_packet_result`, `run_first_500_turn_playback_proof()`, and `render_first_500_turn_playback_proof( const proof_packet_result &result )`
2. deterministic coverage proves the chosen long-horizon scenarios stay bounded, including cooldown, city-edge peel-off, and repeated-site reinforcement behavior that eventually cools back out instead of hardening into free raid truth
3. reviewer-readable report output exposes the long-horizon checkpoints and winner drift cleanly enough to inspect the 500-turn answers without spelunking through internals
4. the slice stayed visibly bounded and did not smuggle in broader world-sim, persistence, or live-probe theater

## Validation results

- the implementation stayed on the existing deterministic playback footing instead of detouring into new harness work
- narrow deterministic validation passed via `make -j4 tests` and `./tests/cata_test "[bandit]"`
- if later evidence says the long-horizon packet is too noisy or too optimistic, fix the reporting or scenario shape before pretending broader evidence is needed

## Dependency note

This slice comes after:
- `doc/bandit-smoke-visibility-mark-seam-v0-2026-04-20.md`
- `doc/bandit-light-visibility-mark-seam-v0-2026-04-20.md`
- `doc/bandit-human-route-visibility-mark-seam-v0-2026-04-20.md`
- `doc/bandit-repeated-site-activity-reinforcement-seam-v0-2026-04-20.md`

It reuses the already-checkpointed mark-generation, playback, evaluator, pursuit-handoff, and perf-budget seams rather than reopening them.

## Implementation notes

- the first proof packet reuses `smoke_only_distant_clue`, `city_edge_moving_hordes`, and `generated_repeated_site_reinforcement_stays_bounded`
- the proof checkpoints are `0`, `20`, `100`, and `500`
- the repeated-site scenario now carries extra idle-horizon frames on ordinary inactive cadence so the long-horizon result cools honestly instead of leaving one reinforced clue as immortal scout pressure
