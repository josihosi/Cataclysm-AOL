# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active target: `Bandit local sight-avoid + scout return cadence packet v0`.

- Work from `/Users/josefhorvath/Schanigarten/Cataclysm-AOL` on `dev`; `josihosi/Cataclysm-AOL` is project/release truth and `josihosi/C-AOL-mirror` is green-dot-only.
- Use `doc/bandit-local-sight-avoid-and-scout-return-cadence-packet-v0-2026-04-26.md` as the active contract.
- Preserve the just-closed bridge caveat: `Bandit live-wiring audit + visible-light horde bridge correction v0` has loaded-map visible fire/light -> live horde signal bridge proof, not full player-lit brazier/wood/lighter product proof.
- Do **not** continue the parked smoke/fire site-refresh proof loop for `Bandit live signal + site bootstrap correction v0`; that item is in Josef review as `bandit-live-signal-smoke-source-site-refresh-proof` after attempt 5.
- Deterministic implementation checkpoint is landed in-tree: local sight-avoid selection, active scout sortie clocks, return-home/writeback cleanup, active job serialization, and reviewer-readable local-gate/sortie output.
- Current validation gate: `git diff --check`; `python3 -m py_compile tools/openclaw_harness/startup_harness.py`; `rm -f obj/tiles/do_turn.o obj/tiles/version.o cataclysm-tiles && make -j4 obj/tiles/do_turn.o cataclysm-tiles TILES=1 LINTJSON=0 ASTYLE=0`; `rm -f obj/tests/version.o tests/cata_test && make -j4 tests LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[bandit][live_world]"` -> 524 assertions in 22 test cases passed on 2026-04-27.
- Live checkpoint: Frau Knackal said not to rerun the 4500-turn standoff macro. The new pre-aged scout-return probe `bandit.local_scout_return_preaged_mcw` uses real nearby-owned-site local-contact footing plus a narrow `bandit_active_sortie_clock` fixture transform. Run `.userdata/dev-harness/harness_runs/20260427_051117/` proves the current runtime emits `scout_sortie: linger limit reached -> return_home` and then `returning_home -> local_gate skipped`; it is return-home decision proof, not full walked-home/writeback proof.
- Next target: either shape a short live sight-avoid proof for `sight_avoid: exposed -> repositioned`, or a bounded follow-through proof that the returning scout reaches home and writes back/clears the active group. Do not use the old long standoff macro unless the setup has materially changed.
- No Lacapult work, no release publication, no repo-role surgery, no user-data mutation, and no broad local AI rewrite unless this active slice truly requires it.

Keep this file focused on the active lane only, ja, otherwise it turns into a junk drawer with headings.
