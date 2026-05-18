#### Summary
Interface "Make spoken camp craft requests deterministic"

#### Purpose of change

Basecamp spoken craft handling was doing too much loose guessing for a patch that should stay small and upstreamable.
The goal of this change is to make the spoken `craft ...` path explicit, deterministic, and easier to review.

In particular, this narrows craft intake to the exact standalone trigger `craft`, avoids substring false positives such as `witchcraft`, prefers whole-word / ordered-phrase matches over looser noun fallback, and reports ambiguity instead of silently guessing.
It also keeps blocked spoken craft requests in the deterministic path so they explain themselves instead of falling through to unrelated generic chatter.

#### Describe the solution

The spoken camp-craft path now works as a small deterministic command route:

- only the exact standalone verb `craft` enters the spoken camp-craft path
- craft query matching prefers exact product names and ordered whole-word phrases
- partial-word substring matches are rejected
- if the best spoken target is genuinely ambiguous, camp asks for clarification instead of picking one silently
- if the request resolves to a blocked craft, the blocker is preserved and reported deterministically
- a shared deterministic craft-resolution helper is used so spoken craft intake and later structured craft handling rely on the same matching/evaluation path
- conservative cleanup for obvious plural phrasing helps requests like `makeshift bandages` resolve without broad fuzzy matching
- bark / request summaries prefer the heard request text and keep request ids as trailing detail instead of leading bookkeeping noise

#### Describe alternatives you've considered

The main alternative would be broader fuzzy natural-language matching, broader trigger verbs such as `make` / `build`, or simply letting ambiguous / blocked cases fall through to a more open-ended interpretation layer.

That approach has a larger review surface and makes it harder to reason about what spoken craft input is legally supposed to do.
For the upstream-facing slice, the narrower explicit deterministic route is the better trade: smaller patch, clearer behavior, easier regression coverage, less accidental nonsense.

#### Testing

Automated / deterministic checks:
- `make -j4 tests`
- `./tests/cata_test "[camp][basecamp_ai]"`

Runtime validation:
- `make version TILES=1 -j4 cataclysm-tiles`
- `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'`

Live smoke packet used for close-out:
- `craft 5 makeshift bandages` -> positive deterministic path / request pins cleanly
- `craft boiled` -> deterministic clarification instead of guessing
- `craft 5 bandages` -> deterministic blocked path / no crash / blocker reported clearly

#### Additional context

This PR is intentionally only the deterministic spoken camp-craft cleanup.
It does not include broader Basecamp AI handoff work, prompt plumbing, movement-contract changes, or other later `dev`-branch experiments.
