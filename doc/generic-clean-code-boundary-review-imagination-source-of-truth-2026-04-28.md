# Generic clean-code boundary review imagination source of truth (2026-04-28)

## Finished scene

The current feature lane reaches a real boundary, the repo is not mid-surgery, and a fresh clean Code/agent session walks in like a second pair of eyes rather than another cook grabbing the soup spoon.

It reads the current canon, the active/queued stack, the recent diff/commit boundary, and the build/test posture. It produces a short practical report: what looks contradictory, stale, risky, overgrown, or quietly broken; what is safe to fix now; what should be queued; and what should be ignored because it is only aesthetic fuss.

The useful outcome is not a grand refactor. The useful outcome is confidence that the next lane starts from a tidy enough floor: no stale TODOs misleading Andi, no obvious build/test landmine, no drift between `Plan.md` and execution reality, no hidden “this is probably fine” rot.

## Product/operator experience

- Schani/Josef can read one compact report and decide what, if anything, to apply.
- Andi is not interrupted mid-lane by generic cleanup energy.
- The review names exact files/sections/tests when it finds something real.
- Small fixes are proposed as bounded patches, not silently applied by default.
- Findings are sorted by priority: fix-now, queue, ignore/watch.

## Boundaries

- This is a boundary review after a checkpoint, not an active feature lane.
- First pass is report-only unless explicitly promoted to edits.
- It must not invent new product direction.
- It must not reorder current canon priorities by vibes.
- It must not convert broad “cleanliness” into a refactor spree.

## Failure smells

- The run produces generic advice with no file/path/test anchors.
- The run starts changing code before the report is reviewed.
- It reopens closed lanes because it noticed old TODO text without understanding canon.
- It churns formatting or style while missing actual build/test/canon risks.
- It treats private Schani/Josef workflow files as public product cleanup.

## Review questions

1. Did the review start from current `Plan.md`, `TODO.md`, `SUCCESS.md`, `TESTING.md`, and `andi.handoff.md` rather than stale memory?
2. Are findings concrete enough that another agent could verify them?
3. Did it preserve active-lane and greenlit-order boundaries?
4. Does each recommendation say fix now / queue / ignore-watch?
5. Did it keep code cleanup subordinate to product continuity?
