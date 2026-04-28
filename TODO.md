# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active target: `Cannibal camp night-raid behavior packet v0`.

Current state: deterministic/code substrate for `Cannibal camp pack-size + smoke-light/darkness/sight-avoid substrate v0` has landed. This is deterministic support only, not live night-raid product proof.

Landed substrate:

- Cannibal stalk/attack-style pressure now requires pack pressure under the shared hostile-site/bandit-live-world substrate instead of sending a lone ordinary attacker.
- One-dispatchable-member cannibal camps stay on scout/probe pressure and do not become full attack pressure; explicitly manual/lone scout pressure still degrades to `probe` instead of `attack_now`.
- Smoke/light or nearby human-routine leads classify as scout/probe or pack-dispatch pressure rather than instant combat.
- `local_gate_input` has `darkness_or_concealment`, and cannibal local gates can shift from daylight/no-cover `hold_off` to dark/concealed `attack_now` only when pack size and pressure gates pass.
- Recent exposure around a camp/basecamp edge makes cannibals hold off instead of continuing a visible rush; high threat still aborts even in darkness.
- Cannibals remain blocked from shakedown/extortion; bandit shakedown/pay-fight local-gate coverage remains green.
- Reports expose `profile=cannibal_camp`, `pack_size`, `darkness_or_concealment`, `recent_exposure`, posture, shakedown state, and attack/no-extort notes.
- Save/load proof now preserves a multi-member cannibal active group.

Validated evidence:

- `git diff --check`
- `make -j4 obj/bandit_live_world.o tests/bandit_live_world_test.o LINTJSON=0 ASTYLE=0`
- `make -j4 tests LINTJSON=0 ASTYLE=0`
- `./tests/cata_test "[bandit][live_world][cannibal]" --success`
- `./tests/cata_test "[bandit][live_world][approach_gate]" --success`
- `./tests/cata_test "[bandit][live_world]"`

Remaining:

- Do not claim live night-raid product behavior from this deterministic slice.
- If Schani/Josef promote the later live/harness slice, wire the real time/light/visibility path into the local-gate input and prove a named cannibal night-raid scenario through the real dispatch/local-contact path under proof-freeze rules.
- Otherwise move to the next explicit Schani/Josef priority behind this lane.

Keep this file focused on the active lane only, ja, otherwise it turns into a junk drawer with headings.
