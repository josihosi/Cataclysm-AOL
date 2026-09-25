# R-CAOL-FIRST-SMOKE native preflight

Coordinator source: `.userdata/reference-saves/josef-basecamp-closed-windows-20260924-122642/save/TestSetup00/`.
Owner archive README: the baseline was owner-created in closed-window/closed-door state after complete darkness and quicksave; original playable save was untouched; no game turns advanced during archive. The source contains world `TestSetup00`, character `Test00` (`config/lastworld.json`).

Source tree was read only. It contains 45 files; deterministic tree digest (SHA-256 of sorted relative paths and each file SHA-256) is `7df36c678a2ad0649d5724e18a12be4652ad20c96cd8844cbec9550b8c462ba3`. Player save SHA-256 is `037c9bf694be280cb19fe607ef691429593120245c2741d6b0906dfa8833d247`.

Two independent world-only copies were created, preserving source and avoiding the archive's configuration directory:

- `.userdata/first-smoke-exploration-001/smoke-first/TestSetup00/`
- `.userdata/first-smoke-exploration-001/first-light/TestSetup00/`

Immediately after copy, both had 45 files and tree digest `7df36c678a2ad0649d5724e18a12be4652ad20c96cd8844cbec9550b8c462ba3`, matching source. At that point no game process matching Cataclysm was running.

The first `runtime-status --full` after the reported build found executable `/Volumes/CodexBulk/Schanigarten/workspaces/Cataclysm-AOL/cataclysm-tiles`, SHA-256 `a99f455e69bea382401c6255f7fe763534ef42dc0c3e764882948df40efae5ac`, but returned `build_required`: the archived receipt set had no receipt matching the current product source/executable digest; runtime status named a malformed/unsupported receipt at the mutable legacy path `cataclysm-tiles-a0f057203d703846.json`. No native run started before a supported binding is confirmed.

No smoke, light, camp-site, or other world setup changes have yet been applied to either disposable copy.
