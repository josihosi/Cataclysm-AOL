# Cataclysm: Arsenic and Old Lace v0.1.0

First public release of the AOL fork.

Highlights:
- Local LLM intent runner integration (async, non-blocking).
- Initial follower NPC action set: follow, wait, equip, look_around, look_inventory, panic, attack.
- Snapshot logging for tuning and training data collection.
- OpenVINO runner support with optional API fallback.

Known limitations:
- This is an early release; expect rough edges and balance issues.
- LLM behavior depends heavily on model choice and hardware.
- This release has not been extensively tested yet; I can ship fixes quickly via PRs.
