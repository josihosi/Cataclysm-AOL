# C-AOL harness proof-freeze matrix v0 (2026-04-28)

Status: PROOF-FREEZE MATRIX DRAFT / READY FOR FRAU REVIEW.

Canonical contract: `doc/c-aol-harness-trust-audit-and-proof-freeze-packet-v0-2026-04-27.md`.
Inventory checkpoint: `doc/c-aol-harness-trust-audit-inventory-v0-2026-04-27.md`.

## Verdict

This is the compact reviewer matrix for the current harness trust-audit state. It freezes what the harness is currently allowed to claim, which artifacts make those claims reviewable, and which primitives remain red/yellow/blocked.

It is **not** a product-feature closure packet. Load/readiness remains startup/load evidence only. Feature proof requires a clean startup gate, green step-local ledger rows, same-run/same-save state proof where relevant, and no yellow/blocked wait ledger.

## Same-save / provenance policy

Default audit anchor remains `dev-harness` / `McWilliams` with copied disposable fixture/profile `mcwilliams_live_debug_2026-04-07` when the primitive can honestly run there.

Allowed exceptions must stay named in the scenario/report before they can support any claim:

- transformed bandit fixtures when the primitive genuinely needs overmap placement, sortie clocks, controlled contact, fire/smoke/light fields, or live-world ownership state;
- old `dev` locker/patrol fixtures only when auditing those legacy contracts directly;
- temporary `tmp.*` scenarios only as workbench evidence unless promoted with provenance.

Every feature-proof report must preserve: source/runtime commit evidence, scenario/profile/world, fixture/profile snapshot, run directory, applied save transforms, and whether the run used the canonical anchor or a justified exception.

## Current proof matrix

| Primitive / seam | Current verdict | Evidence / artifact | Frozen claim rule | Next honest action |
|---|---|---|---|---|
| Startup launch/load/focus/screenshot | green/yellow/red as startup only | `startup.step_ledger.json`; `.userdata/dev-harness/harness_runs/20260427_164639/` caught stale runtime as yellow | Startup/load can never be feature proof; stale runtime or screenshot failure blocks later feature proof | Keep as gate; no more load-only closure claims |
| Probe report classifier | green only when startup is clean, artifacts match, step ledger is all-green, and wait ledger is not yellow/blocked | `tools/openclaw_harness/startup_harness.py` report fields `proof_classification`, `evidence_class`, `feature_proof`, `step_ledger_summary` | `ok: true` means harness machinery completed, not necessarily feature proof | Frau review for hidden classification loopholes before final freeze |
| Step-local proof ledger | partially frozen / active | `<mode>.step_ledger.json`; deploy run `20260427_222635` all-green; fuel run `20260427_232220` red | Every live/GUI step needs precondition, action, expected state, artifact, failure rule, gate, verdict | Extend only by named primitive; do not create unguarded macro soup |
| Screenshot / OCR guard | yellow unless expected visible fact/text is named and checked | screen/OCR companion JSON in probe runs; GUI-as-text run `20260427_200919` blocked safely | Decorative screenshots do not prove a step; missing expected text before unsafe keypress is red/blocked | Add expected text/fact before trusting menu navigation |
| Saved-player item preflight | green only for live-accessible carried/worn/contained rows; legacy top-level `player.inv` is explanatory only | `audit_saved_player_items` metadata; direct updated audit after `20260427_203847` | Legacy inventory presence cannot prove live selector availability | Keep live-accessible distinction; repair fixtures instead of over-crediting |
| Saved-map target-tile audit | green only when required fields/items/furniture are present at the exact requested tile in the same run/save | `audit_saved_map_tile_near_player` metadata; `20260427_184319`, `191725`, `222635` | Empty requested tile is red evidence, not ambiguous absence | Keep aborting metadata gates before later feature steps |
| Normal player-action brazier deploy | green for scoped deploy primitive only | `.userdata/dev-harness/harness_runs/20260427_222635/`: 16/16 green, `feature_proof=true`, east tile `f_brazier` | Proves selected `brazier`, `Deploy where?`, east/right consumption, uppercase-`Y` save/writeback, saved east-tile `f_brazier`; not fuel/fire/bandit proof | Preserve as reusable audited primitive |
| Save/writeback mtime gate | green when save prompt is proven and mtime advances; red when prompt/confirmation is untrusted | `20260427_213850` lower-case `y` no mtime; `20260427_222635` uppercase `Y` mtime advance | Post-action map state cannot be credited until saved-player mtime/writeback gate advances | Require this before any saved-map post-action closure |
| Fuel Multidrop / exact `2x4` visibility | red / blocked | `.userdata/dev-harness/harness_runs/20260427_232220/`: filtered Multidrop has no visible/selectable `typeid="2x4"` row | No count selection, confirm-return, post-fuel save/writeback, lighter, or `fd_fire` credit after this gate | Repair fixture/live fuel availability or package/manual; do not retry lighter/fire blindly |
| `wait_action` / long-wait proof | strongest current wait primitive; scenario-specific audit still required | wait ledgers in live bandit/standoff runs, especially `.userdata/dev-harness/harness_runs/20260427_154309/` | Artifact matches cannot override yellow/blocked wait ledger | Reuse for long live-world proofs; do not downgrade to dot-spam for hours |
| Talk/dialog keypath primitive | red / blocked at talker selection | `.userdata/dev-harness/harness_runs/20260427_175051/`: source-backed `t` opens `Talk to whom`, alpha selector still visible after `a` | Source-backed key opens first menu, but recipient selection is not proven | Needs menu-entry/hotkey metadata or replacement recipe before closure use |
| Debug spawn item/monster/NPC/weather primitives | yellow/untrusted for product closure | inventory doc primitive table; no complete target-state proof matrix yet | Spawn macro completion is not target-tile/inventory/NPC identity proof | Next audit candidate after Frau review: one spawn path with metadata gate |
| Smart Zone live layout inspection | implemented-but-unproven / Josef package | `.userdata/smart-zone-safe-clean-20260427/harness_runs/20260428_001347/`: startup/load red, no feature steps | Deterministic geometry is support only; clean GUI layout proof is not captured | Do not rerun blind clean-profile probe unless reopened after a loadable-profile/UI primitive repair |
| Real player-lit fire / bandit signal chain | blocked behind fuel | fuel red run `20260427_232220`; deploy green run `20260427_222635` only covers brazier placement | Fire/smoke/bandit proof requires current-tile `f_brazier` + exact fuel writeback, then lighter/`fd_fire`/bandit response in the same disciplined chain | Stay blocked until fuel gate is honestly green or manual package is chosen |

## Frozen workflow rules

1. Classify the evidence class before running anything: startup/load, deterministic contract, live behavior, artifact/log visibility, or metadata/save inspection.
2. For live/GUI feature proof, each step must name the expected visible fact or exact metadata state before the next risky action runs.
3. A probe report is feature proof only when `proof_classification.feature_proof=true`, `evidence_class=feature-path`, startup is clean, all step-local ledger rows are green, matched artifacts exist where the scenario contract asks for them, and wait ledgers are not yellow/blocked.
4. Target-tile, inventory, NPC, clock, overmap, and save/writeback claims must be same-run/same-save evidence. Fixture transforms or logs alone are setup evidence.
5. Red/yellow/blocked rows stop credit for later dependent steps. The macro may mechanically continue only when explicitly marked safe; feature closure does not.
6. A deterministic test may close a deterministic contract. It cannot close a screen-visible/product claim unless the product contract explicitly names the deterministic seam as the target.
7. Failed harness proof does not prove feature failure by itself. If code is implemented but agent-side proof stays blocked after the escalation budget, package it as implemented-but-unproven for Josef and move on.

## Untrusted until reviewed/repaired

- Fuel Multidrop/live inventory availability for exact `2x4`/plank rows.
- Talker selector/recipient selection after `Talk to whom`.
- Debug spawn item/field/furniture/NPC/weather macros when no same-run target-state metadata follows them.
- Smart Zone live layout inspection from the current clean profile path.
- Any scenario that still relies on raw `press`/`type`, decorative screenshots, or artifact string matches without a green step-local ledger.

## Recommended review

Ask Frau Knackal to review this matrix before treating the proof-freeze workflow as settled. The review target is not product behavior; it is whether the matrix still hides a false-pass route, fixture bias, or claim that outruns its evidence.
