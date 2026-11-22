# Working on Cataclysm: Arsenic and Old Lace

Start with a short imagination round. Picture someone using the change in a real game.
What would they do next? Where could a test pass while the game still feels wrong?
Use that case to choose what to inspect. For a small change, one good example is enough.

## Finding your way

- [src/](src/) holds the C++ simulation and UI. [data/](data/) holds game definitions and mods.
- [tests/](tests/) contains the C++ regression tests. The [build guide](doc/SETUP.md) and [developer tooling guide](doc/c++/DEVELOPER_TOOLING.md) cover building and checking a change.
- [tools/openclaw_harness/](tools/openclaw_harness/) drives native playtests. For a playtest assignment, use the [harness guide](.agents/skills/caol-harness/SKILL.md).
- For DE67 delivery, the relevant section of the [functional specification](.de67/FS.md) describes the game logic. The [work ledger](.de67/work-ledger.md) identifies the assignment and its evidence. If you are acting as Josef’s DE67 mutator, read the [mutator guide](.de67/mutator-guidance.md) for direct discussions and reviews.

## Making a change

Trace the production caller and the inputs it actually reads. Check the branch you changed and any independent path that can produce the same visible result. For a gameplay finding, identify the build, starting conditions and first observable difference so someone else can reproduce it.

Use the existing Mac `dev` or Windows `dev_w` checkout and build cache. Coordinate shared files and builds with their current owner. Preserve personal saves and active work. When a run ends, keep the needed build, continuation and concise result, and remove your disposable output.

Publish development checkpoints to `origin/dev`. Bring useful Windows changes back from `dev_w` and verify the remote commit after pushing. GitHub [mirrors dev](.github/workflows/mirror-dev.yml) automatically. Release work uses `master`; the port and intentional backup branches serve their existing purposes.
