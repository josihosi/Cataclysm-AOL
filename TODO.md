# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active lane: **Locker Package 5, basecamp carried-item dump lane**.

Current target:
1. reopen the corrected Robbie-only packet at the post-pickup camp-assignment seam on the real McWilliams / `CAMP_LOCKER` footing
   - the upstream four-item pickup-completion seam is now live-screen closed at `.userdata/dev-harness/harness_runs/20260419_090351/`: the shorter named `say` order submits, only Robbie answers, and the visible log shows him picking up `adhesive bandage`, `9x19mm JHP, reloaded`, `Glock 9x19mm 15-round magazine`, and `small plastic bag`
   - `config/llm_intent.log` now records the repaired four-item selection `item_8, item_6, item_7, item_9`; the remaining mismatch is observability for the bandage pickup line, not the old three-item queue cap
2. get Robbie assigned back to camp on that same corrected packet before reading any locker artifact
   - the earlier camp-branch reference at `.userdata/dev-harness/harness_runs/20260419_055401/` still matters, but the packaged e2e seam was unstable before the upstream pickup fix and now needs one fresh honest rerun on the repaired footing
3. only after Robbie is shown both completing the four-item packet and getting assigned back to camp should any locker artifact be read as kept-vs-dump proof
   - keep this narrow on the real McWilliams / `CAMP_LOCKER` path; do not drift into generic pickup behavior, Package 4 cleanup, or the side observability mismatch unless it blocks the camp/locker readout

Out of scope right now:
- finishing Package 4 in the same patch
- grenades or broader consumable logic
- bandit / overmap-threat design
- hackathon feature lanes
