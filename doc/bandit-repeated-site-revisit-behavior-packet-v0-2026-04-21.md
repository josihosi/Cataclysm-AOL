# Bandit repeated-site revisit behavior packet v0 (2026-04-21)

Status: active contract.

## Greenlight verdict

Josef explicitly wants the repeated-site follow-through to do two things and not wander into a third.

Do:
1. make scenario 9 produce a more interesting bounded behavior than plain confidence bookkeeping
2. make the reported metrics honestly match the benchmark/writeup burden

Do not do:
- broader site-type-sensitive branching or taxonomy work in this packet

The repeated-site reinforcement seam is already checkpointed closed.
The benchmark suite is already checkpointed closed.
What remains is a narrow behavioral follow-through on top of those landed seams: repeated same-site corroboration should produce one more deliberate bounded revisit posture, then cool back out, and the packet should report that behavior with the metrics it already claims to care about.

## Scope

1. Extend the current repeated-site scenario / evaluator footing so mixed repeated same-site corroboration can produce one more deliberate bounded revisit or cautious-watch behavior than plain early `scout`, while still staying clearly below free extraction or raid truth.
2. Keep that behavior reviewer-readable on the current playback / benchmark seam, so scenario 9 reads like actual bounded behavior instead of score bookkeeping.
3. Align the implemented metric/report surface for `repeated_site_interest_stays_bounded` with the benchmark packet's stated burden, specifically exposing:
   - `site_visit_count_500`
   - `site_revisit_count_500`
   - `cooldown_turn`
   - `endless_pressure_flag`
4. Keep the long-horizon proof explicit: the strengthened site can get a modest bounded revisit window, but it must still cool back out by the honest `500`-turn horizon.
5. Extend deterministic coverage so the new behavior and the richer metrics are both proven on the current tree instead of only implied by docs.

## Non-goals

- site-type-sensitive reaction trees
- shack/farmstead/fortified-homestead special-case taxonomy
- free `scavenge`, `steal`, or `raid` unlocks from repeated ordinary clues
- magical settlement identity truth
- broad visibility/concealment rewrites
- broad scoring or memory redesign
- z-level work

## Success shape

This item is good enough when:
1. repeated same-site corroboration can visibly progress from plain early scout pressure into one more deliberate bounded revisit / cautious-watch posture on the current seam
2. deterministic coverage proves the strengthened site still does **not** unlock free extraction truth or immortal pressure
3. scenario 9's reviewer-readable output exposes `site_visit_count_500`, `site_revisit_count_500`, `cooldown_turn`, and `endless_pressure_flag` honestly enough to judge whether the behavior stayed bounded
4. the `500`-turn proof shows the site pressure cooling back out instead of regrowing forever
5. the slice stays narrow and does not smuggle in site taxonomy or broader behavior design work

## Validation expectations

- prefer narrow deterministic work on the current playback / benchmark seam
- honest first pass should stay in the bandit test family, not broad harness theater
- the important proof is not just pass/fail, but that the rendered packet now shows the bounded revisit picture reviewer-cleanly
- if new metrics appear in docs or rendered reports, deterministic coverage should check those strings or fields directly instead of trusting them by vibes

## Notes

- This is a follow-through packet on top of the already checkpointed `Bandit repeated site activity reinforcement seam v0` and `Bandit overmap benchmark suite packet v0`.
- Keep `Bandit z-level visibility proof packet v0` ungreenlit. This packet is the active narrow next lane instead.
