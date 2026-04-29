# C-AOL actual playtest verification stack v0 (2026-04-27)

Status: CLOSED / CHECKPOINTED READY FOR SCHANI REVIEW.

## Why this exists

Josef explicitly greenlit moving beyond paper/audit closure into actual playtests, but with the same proof discipline that the harness trust audit just made useful: every feature claim needs step-by-step screenshots, menu/UI trace or checked text when relevant, exact metadata, save/writeback gates when persistence matters, and honest non-green states when the live path cannot be proven.

This was not permission to wander around a live save and call vibes evidence. It was a small greenlit stack of actual product verification items with explicit proof boundaries; the current boundary is complete and ready for Schani review.

## Priority order

1. **Fuel normal-map entry primitive packet v0 — complete / green normal-map entry gate.**
   - Preserve `.userdata/dev-harness/harness_runs/20260429_140645/` as the scoped first normal-map entry proof for the current source-zone fuel path.
   - It proves feature-path startup/load, step-local ledger, charged wielded `lighter`, saved `f_brazier`, real saved `log`, saved `SOURCE_FIREWOOD`, and a guarded `normal_map_entry_gate_before_activation.png` screenshot/OCR fallback.
   - It does not send activation/targeting/fire/lighter keys and does not prove normal ignition, save mtime, `fd_fire`/`fd_smoke`, smoke/light, or bandit signal response.

2. **Fuel writeback repair via wood source zone packet v0 — complete / green normal player ignition + saved `fd_fire`.**
   - Josef diagnosed the old `fuel_writeback_source_zone_v0_2026-04-29` proof surface as a broken save/fixture; do not use it for more fire/lighter proof.
   - Preserve `.userdata/dev-harness/harness_runs/20260429_142257/` as prior non-proof action evidence: after normal apply-wielded `A`, the guarded screenshot/OCR boundary did not show `Light where?`.
   - The corrected product path is green at `.userdata/dev-harness/harness_runs/20260429_153253/`, scenario `bandit.live_world_nearby_camp_visible_brazier_source_zone_firestarter_action_mcw`: normal Apply inventory deploys `brazier`, normal Apply inventory selects charged `lighter`, exact UI trace proves `Light where?`, OCR proves source-firewood prompt and recognizable ignition, save mtime advances, and saved target tile contains `f_brazier` + `fd_fire`.
   - Detailed proof: `doc/fuel-visible-brazier-source-zone-firestarter-action-v0-2026-04-29.md`.

3. **Smart Zone Manager live layout verification packet v0 — honest boundary reached.**
   - Preserve this as implemented-but-unproven in Josef's playtest package.
   - Deterministic geometry/separation tests are supporting evidence only; they do not close the product claim.
   - Current-runtime rerun `.userdata/smart-zone-safe-clean-20260427/harness_runs/20260428_001347/` is startup/load red only (`blocked_clean_profile_no_loadable_character`; OCR `Dunn has no characters to load`), so no Smart Zone feature steps ran and no layout proof is credited.
   - Do not reuse the contaminated old McWilliams Smart Zone macro or run another blind clean-profile probe without an explicit fresh reopen/material loadable-profile or UI primitive repair.

4. **Player-lit fire and bandit signal verification packet v0 — complete / green real-fire bandit signal response.**
   - Green at `.userdata/dev-harness/harness_runs/20260429_162100/`, scenario `bandit.player_lit_fire_signal_wait_mcw`, starting from the real player-action fire proof `.userdata/dev-harness/harness_runs/20260429_153253/`.
   - Fixture `player_lit_fire_bandit_signal_wait_v0_2026-04-29` preserves saved `f_brazier` + `fd_fire` and adds mineral water plus an `AUTO_DRINK` support zone.
   - The run proves bounded 30-minute wait/time passage (`observed_delta_turns=1800`), live smoke/fire signal scan/maintenance with matched `bandit_camp`, dispatch plan `candidate_reason=live_signal`, and saved active scout response metadata with `known_recent_marks` including `live_smoke@140,41,0`.
   - Detailed proof: `doc/player-lit-fire-bandit-signal-verification-v0-2026-04-29.md`.

5. **Roof-fire horde detection proof packet v0 — complete / green split-run feature proof.**
   - Source run `.userdata/dev-harness/harness_runs/20260429_172847/` proves the normal player-created roof/elevated fire writeback (`t_tile_flat_roof` + `f_brazier` + `fd_fire`).
   - Split-run `.userdata/dev-harness/harness_runs/20260429_180239/`, scenario `bandit.roof_fire_horde_split_wait_from_player_fire_mcw`, fixture `roof_fire_horde_split_wait_from_player_fire_v0_2026-04-29`, loads that saved world back into gameplay and closes the horde proof: `feature-path`, `feature_proof=true`, `14/14` green step rows, green wait ledger, `observed_delta_turns=300`, same-run `bandit_live_world horde light signal:` with `horde_signal_power=20`, and saved staged-`mon_zombie` response metadata retargeting to roof-fire source submap `[3360,984,1]` with `moves=8400`.
   - Boundary: do not overclaim positive tracking-intensity; `tracking_intensity` remained `0`, so the credited horde response is retarget/movement-budget metadata after live roof-fire light signaling.
   - Detailed proof: `doc/roof-fire-horde-detection-proof-v0-2026-04-29.md`; prior partial boundary/postmortem: `doc/roof-fire-horde-player-action-boundary-v0-2026-04-29.md`.

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
- [x] Fuel writeback repair via wood source zone is green at `.userdata/dev-harness/harness_runs/20260429_153253/`: old broken fixture retired, visible deploy path proved, charged lighter selected through normal Apply inventory, `Light where?` was proven by UI trace before targeting, OCR captured source-firewood prompt plus recognizable ignition, save mtime advanced, and saved target tile contains `f_brazier` + `fd_fire`.
- [x] Smart Zone Manager live layout verification reached an honest agent-side boundary: deterministic geometry remains support only, the precise manual package is in `/Users/josefhorvath/.openclaw/workspace/runtime/josef-playtest-package.md`, and current-runtime rerun `.userdata/smart-zone-safe-clean-20260427/harness_runs/20260428_001347/` is startup/load red only (`Dunn has no characters to load`), with no feature/layout proof credited.
- [x] Player-lit fire/bandit signal verification is green at `.userdata/dev-harness/harness_runs/20260429_162100/`: survivable long-wait footing, bounded 30-minute elapsed-time proof, live signal/dispatch artifacts, guarded save/writeback, and saved bandit live-world signal/active-state metadata are all present without debug/synthetic fire shortcut.
- [x] Roof-fire horde detection proof is green at `.userdata/dev-harness/harness_runs/20260429_180239/`: split-run from the saved player-created roof-fire world proves roof/elevated `t_tile_flat_roof` + `f_brazier` + `fd_fire`, bounded time passage (`observed_delta_turns=300`), same-run roof-fire horde light signal (`horde_signal_power=20`), and saved horde retarget/movement-budget response metadata without treating source-zone fire/signal proof as roof proof.
- [x] Andi reports each item with evidence class boundaries intact: what is live product proof, what is deterministic support, what is startup/load, and what remains unproven.

State boundary: no further greenlit agent-side item remains in this stack. Natural multi-site pressure, zero-site idle baseline, or stricter roof-horde tracking-intensity proof require explicit future promotion.

## Testing expectations

- Every live step should have a proof row: precondition, action, expected state, screenshot or exact metadata, failure rule, artifact path, and verdict.
- Smart Zone proof is not current active work; any fresh reopen must first repair the loadable clean-profile/UI primitive and then include generated zone positions after creation/reopen.
- Fire proof must include the real in-game sequence, not synthetic loaded-map fire alone.
- Roof-horde proof may use debug for horde/distance staging only; fire/elevated-position setup must remain real player-action proof.
- If two materially similar attempts fail at the same primitive, consult Frau Knackal before attempt 3; after four total attempts, package implemented-but-unproven behavior instead of looping.
