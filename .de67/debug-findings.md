# Current gameplay findings

Owner-promoted repairs and investigations are separated below. Accepted or repaired items leave this
active intake; original observations and dispositions remain in
`.de67/state/review-owner-d1cfc813605b/baseline/debug-findings.md` and durable receipts.

## Owner-promoted repairs

R029-F005 leaves active intake: receipt `0892479ce584418db6ef41fc3ef4d594d82b16ca97d6dac9fb423f2c3687185f` proves eligible night departure, exact-world reload, continuation after dawn and later local hostile contact. The earlier rallying observation and incorrect missing-caller attribution remain historical evidence. Actual active combat/aftermath is separately covered by the promoted R029-F004 repair.

R033-F002 is repaired and leaves active bug intake. The original observation and promotion remain in `.de67/state/review-owner-d1cfc813605b/baseline/debug-findings.md`. Receipt `df8d84806a3395b87181cf465a8bf57e955dbca534786b1ae86690e5b9a11494` proves the bounded comparison repair, focused boundary tests, native unchanged-source deduplication and a same-key refresh without duplication. The later R-033 closure receipts settle its required aging and boundary behavior; later pruning remains outside that accepted scope.

## Promoted active repairs

- R029-F004 — Bandit operation members are moved to the contact location but never become active NPCs. This matters because the player has nobody in the local game world to refuse, attack, kill or observe afterward. Fresh source-bound run `9f8137c9254c4d56b6671f7289551c9a` reached local committed contact at minute 8380 with exact members 18 and 19. Both members were found in overmap storage, but both remained inactive and absent from the active NPC registry before and after relocation. The semantic `visible_entities` set stayed empty. `src/creature.cpp:220` shows that `Creature::setpos` only changes position. `src/game.cpp:1166` shows the separate `game::load_npcs` path that activates nearby overmap NPCs and registers them with the creature tracker. No materialization-rejected event was present, so this result does not infer an absence rejection. Durable receipt `83459f43b78d9a86e4a8ab09ac3d3f26e0624b8526a8b56e4b34cbbe5c759fce` preserves the exact run, source evidence, cleanup and limits. OCR and terminal presentation were not used as gameplay proof. This blocks refusal, player attack, casualty and aftermath only. Disposition2026-09-07, owner70c4632e512e: targeted activation/arbitration plan is completed in receipt58e6879974cd7923a97f81904ed6fd4b6e24838434efb940e3a508e8d737667f and `build_logs/r029-activation-plan-001.md`. No gameplay fix is yet credited. Owner review 2a5bac92b69c promotes the concrete R-029-S002 approach: canonical activation and exact locally committed ID exclusion from generic travel until authoritative return handoff. Preserve ownership exceptions; no global AI rewrite is authorized. Independent R-029 tests continue.

- R029-F002 — Promoted sound timestamp/cooldown correction under owner review 2a5bac92b69c. The original receipt `01bbf939745acaba4fc874d500d88b669f15c2f528c717d6260372a892c9d87a` proves sound detection and aggregate drive347 rejection in run `4b7c5abbafc37adcf66a0865593798640999eafc61f64eaf6eaf5a550e70ba9f`. Its run-bound bootstrap records last_checked8225, so attempt8280 is inside the six-hour recent-check cooldown before candidate construction can admit the lead. Raising drive alone cannot create a missing candidate. The full component vector remains unavailable. See `.de67/state/review-owner-f918d28be953/sound-code-reproduction.md` and the source-predicate executable check; preserve the exact original observation and earlier attribution in this review’s `baseline/debug-findings.md`. The ledger carries the remaining distinction between distant sensing and physical investigation: current sensing sets last_checked while sound expires before the six-hour cooldown ends. The owner now authorizes R-029-S003: remote sound observation does not write or erase a physical-check timestamp; existing three-hour sound expiry and six-hour actual-investigation cooldown retain their distinct meanings. No threshold inflation or automatic scouting is implied.

R032-F001 is repaired and accepted through receipts `508f2d1a263a86dc61561a26e4634ceed303338e428b4de2f2539cd857d9e5e7` and `4a1ee0a32aac15442ec86e34b2f3581e8368528d9f721910aea827c3743f5a42`.
R029-F003 same-minute Pay is repaired in completed receipt `ab383ab50726156a5948882c5c74fefc0853945cd7c06f7cc44a963c380a34ee`: native payment, physical return and post-reload continuation are preserved. Broader R-029 refusal/aftermath and natural ecology remain open; the cross-run classifier limit is not an active same-minute Pay defect.
R033-F001 is the accepted same-OMT scope exception under owner70c4632e512e, not an active blocker.
Other smoke sensing, Pay, activation/arbitration and sound-investigation scopes remain separate.

An active gameplay defect needs an observed contradiction under valid conditions and its causal
implementation path. Unsettled observations stay executable investigation on the work ledger.
Only explicit owner promotion grants gameplay repair authority; harness and fixture repair remain
ordinary recoverable work. Reconcile this intake with accepted repairs and owner decisions when
settling the affected work, preserving exact evidence in existing artifacts.

NPC LLM playtest failures belong here as observed bugs with exact E4B request/response and native consequence, followed by the authorized same-case cheap OpenAI API comparison. Classify model, parser, execution and fixture failures from evidence; API success does not erase the local-model failure. Preserve complete evidence in task/run artifacts.
