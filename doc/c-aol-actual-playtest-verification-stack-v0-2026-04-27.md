# C-AOL actual playtest verification stack v0 (2026-04-27)

Status: GREENLIT / ACTUAL PLAYTEST STACK.

## Why this exists

Josef explicitly greenlit moving beyond paper/audit closure into actual playtests, but with the same proof discipline that the harness trust audit just made useful: every feature claim needs step-by-step screenshots, menu/UI trace or checked text when relevant, exact metadata, save/writeback gates when persistence matters, and honest non-green states when the live path cannot be proven.

This is not permission to wander around a live save and call vibes evidence. It is a small greenlit stack of actual product verification items for Andi to pull after the current active slice reaches its next honest boundary.

## Priority order

1. **Fuel normal-map entry primitive packet v0 — complete / green normal-map entry gate.**
   - Preserve `.userdata/dev-harness/harness_runs/20260429_140645/` as the scoped first normal-map entry proof for the current source-zone fuel path.
   - It proves feature-path startup/load, step-local ledger, charged wielded `lighter`, saved `f_brazier`, real saved `log`, saved `SOURCE_FIREWOOD`, and a guarded `normal_map_entry_gate_before_activation.png` screenshot/OCR fallback.
   - It does not send activation/targeting/fire/lighter keys and does not prove normal ignition, save mtime, `fd_fire`/`fd_smoke`, smoke/light, or bandit signal response.

2. **Fuel writeback repair via wood source zone packet v0 — implemented-but-unproven / Josef playtest package.**
   - Josef diagnosed the old `fuel_writeback_source_zone_v0_2026-04-29` proof surface as a broken save/fixture; do not use it for more fire/lighter proof.
   - Preserve `.userdata/dev-harness/harness_runs/20260429_142257/` as non-proof action evidence: after normal apply-wielded `A`, the guarded screenshot/OCR boundary did not show `Light where?`, so no targeting, ignition, mtime, `fd_fire`, smoke, or bandit signal proof is credited.
   - Clean replacement fixture `fuel_source_zone_clean_normal_map_v0_2026-04-29` and clean-gate probe `.userdata/dev-harness/harness_runs/20260429_143149/` prove the replacement first-load/normal-map footing only; use only this or a fresher first-load-green clean fixture if fire proof is explicitly reopened.
   - Manual package entry: `/Users/josefhorvath/.openclaw/workspace/runtime/josef-playtest-package.md#2026-04-29--fuel-writeback-source-zone-repair-v0`.

3. **Smart Zone Manager live layout verification packet v0 — honest boundary reached.**
   - Preserve this as implemented-but-unproven in Josef's playtest package.
   - Deterministic geometry/separation tests are supporting evidence only; they do not close the product claim.
   - Current-runtime rerun `.userdata/smart-zone-safe-clean-20260427/harness_runs/20260428_001347/` is startup/load red only (`blocked_clean_profile_no_loadable_character`; OCR `Dunn has no characters to load`), so no Smart Zone feature steps ran and no layout proof is credited.
   - Do not reuse the contaminated old McWilliams Smart Zone macro or run another blind clean-profile probe without an explicit fresh reopen/material loadable-profile or UI primitive repair.

4. **Player-lit fire and bandit signal verification packet v0 — blocked behind real/manual fuel proof.**
   - Continue only if Josef supplies manual evidence or Schani/Josef/Frau reopen a materially repaired firestarter/action primitive.
   - Reopened proof must include visible normal-map/fire-or-no-fire screenshot facts, player message/OCR, same-run save mtime, saved `fd_fire`/`fd_smoke`, bounded wait/time passage, and bandit signal response.
   - Do not credit debug map-editor fields, synthetic loaded-map fire, setup metadata, or clean first-load screenshots as player-lit fire.

5. **Roof-fire horde detection proof packet v0 — greenlit but blocked behind real player-lit fire.**
   - Josef/Schani greenlit this as a later verification item: debug may stage the horde/distance, but must not fake the fire.
   - Only after the real player-lit fire path is green, prove the player is actually on the shelter roof/elevated tile, the roof fire/light/smoke is real player-created, bounded time passes, and the horde before/after detection or response metadata is captured.
   - A clean proven negative (`horde does not detect/respond`) is valid if the setup is proven.

## Scope

- Use actual playtest-style live paths where the product question is visual/player-facing.
- For each feature path, capture before state, relevant menu/selector or prompt state, the exact action, post-action state, persistence/writeback if relevant, and final saved/game metadata.
- Prefer disposable/provenance-visible profiles and saves; do not mutate Josef's personal playtest state.
- Treat failed UI control, missing metadata, stale binary/profile, wrong screen, unproven save prompt, or load-and-close as non-green.

## Non-goals

- No release work.
- No Lacapult work.
- No broad debug-note stack reopening.
- No product closure from deterministic tests alone when Josef is asking about actual live behavior.
- No repeating known-bad macros unless the harness primitive or scenario setup has materially changed.

## Success state

- [x] Fuel normal-map entry primitive is green at `.userdata/dev-harness/harness_runs/20260429_140645/`: the source-zone fuel path reaches normal map UI with setup footing, but no activation/targeting/fire/lighter action key is sent or credited.
- [x] Fuel writeback repair via wood source zone reached an honest implemented-but-unproven boundary: old broken fixture retired, clean first-load replacement fixture proved at `.userdata/dev-harness/harness_runs/20260429_143149/`, and action diagnostic `.userdata/dev-harness/harness_runs/20260429_142257/` remains blocked before `Light where?` / targeting / ignition / writeback.
- [x] Smart Zone Manager live layout verification reached an honest agent-side boundary: deterministic geometry remains support only, the precise manual package is in `/Users/josefhorvath/.openclaw/workspace/runtime/josef-playtest-package.md`, and current-runtime rerun `.userdata/smart-zone-safe-clean-20260427/harness_runs/20260428_001347/` is startup/load red only (`Dunn has no characters to load`), with no feature/layout proof credited.
- [ ] Player-lit fire/bandit signal verification, if reopened/reached, proves real player-action ignition, actual `fd_fire`/smoke state, bounded wait/time passage, and bandit signal response with matching artifacts; otherwise it stays blocked behind real/manual fuel proof.
- [ ] Roof-fire horde detection proof, if reached after real player-lit fire is green, proves the horde/distance setup, player roof/elevated position, real player-created roof fire/light/smoke, bounded time passage, and horde before/after detection or response metadata; otherwise it stays blocked behind the real-fire gate.
- [ ] Andi reports each item with evidence class boundaries intact: what is live product proof, what is deterministic support, what is startup/load, and what remains unproven.

## Testing expectations

- Every live step should have a proof row: precondition, action, expected state, screenshot or exact metadata, failure rule, artifact path, and verdict.
- Smart Zone proof is not current active work; any fresh reopen must first repair the loadable clean-profile/UI primitive and then include generated zone positions after creation/reopen.
- Fire proof must include the real in-game sequence, not synthetic loaded-map fire alone.
- Roof-horde proof may use debug for horde/distance staging only; fire/elevated-position setup must remain real player-action proof.
- If two materially similar attempts fail at the same primitive, consult Frau Knackal before attempt 3; after four total attempts, package implemented-but-unproven behavior instead of looping.
