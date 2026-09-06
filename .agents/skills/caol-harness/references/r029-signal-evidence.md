# R-029 signal evidence boundary

For the current investigation, identify the intended live site and observer before interpreting the
adapter callback. An overmap-special footprint is setup, not proof of its eligible roster. Keep
camp `tripoint_abs_omt` separate from member `home_spawn_tile` (`tripoint_abs_ms`); one OMT spans
24 map squares when SEEX=12. Use the source/event location, not a nearby-looking sprite.

`record_staffed_camp_signal_observations` in `src/bandit_live_world.cpp` skips retired sites, active
outings/hostile operations, invalid rosters, and camps without a ready at-home member. Its adapter
`live_bandit_staffed_camp_signal_reads` in `src/do_turn.cpp` also needs the same camp identity and a
living NPC. Sound checks supported kind, volume, source age (up to 180 game minutes), deafness,
hearing, weather attenuation and geometry. Required effective volume is
`(OMT_distance + 1) * 2 * SEEX - 1 + abs(delta_z) * 10 * SEEX`.
`no_signal_source` means no adapter reads survived, not necessarily no emitted sound.

The live owner separates discovery/memory (five-minute cadence), dispatch eligibility (thirty-minute
cadence) and structural maintenance (sixty-minute cadence). Newly bootstrapped sites defer the
observation/maintenance pass as specified by `bootstrapped_sites`. An observed callback proves that
callback ran, not that dispatch or maintenance ran or that every site was eligible.

The 2026-09-06 sound experiment has callbacks at 8225 and 8230 for site
`overmap_special:bandit_camp@140,51,0`, observer 4 home `(3371,1230,0)` map squares. Run-bound player
position was `(3372,996,1)`, OMT `(140,41,1)`: distance 10 and vertical difference 1 require effective
volume 383. The intended `[0,-1,-1]` footprint's hypothetical threshold 59 is not this callback's
geometry. The retained debug reports two sites but one eligible callback. Why the other site was
not eligible is still unresolved; inspect its actual roster/state rather than inventing a reason.

Exact run, callback records, surrounding World frames, source pointers, original durable receipt
and corrected interpretation are in `.de67/state/review-owner-ffc9168450f6/` and
`.de67/debug-findings.md`. Retain native reload/shot/callback proof. A new positive experiment first
needs an identified eligible recipient and sufficient stimulus; a negative result without those
conditions remains setup/eligibility uncertainty. Do not auto-promote or repair gameplay.

The separate Fight scenario is a scripted sample. Its 120 ordinary turns and absence of a matching
terminal log settle that sample only. Inspect actual operation/member state before expecting a
casualty or return, and choose a live observation/continuation route if the scripted pipeline stops.
Save/relaunch and natural discovery are independent claims; neither follows from choosing Fight.
