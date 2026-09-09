# Script usage audit

Scope: the 22 files in the stable method-candidate `scripts/` tree, cross-referenced against the installed de67 phase-3 tree, imports, subprocess/path construction, direct CLI documentation, integration READMEs, and tests. This was read-only; no candidate scripts were changed.

The inventory finds 16 active runtime/support files, 4 operator CLIs, 1 comparison/test utility, and 1 dependency artifact. It finds no confidently dead script and therefore no safe removal candidate. The most sparsely referenced files are still consumed: `instruction_context.py` is imported by policy generation/supervision; `mutator_session.py` is dynamically imported by the app-server runner and direct-input relay; `usage_projection.py` is dynamically imported by `work_context.py`; and `blocker_adapter.py`, `repository_checkpoint.py`, and `symbol_codec.py` are imported by production runtime modules.

The four operator entrypoints have explicit use evidence: `supervisor_service.py` is the documented start/status/stop lifecycle; `trajectory_sidecar.py` is a dashboard `--sidecar-script`; `mutation_guard.py` and `method_provenance.py` are guard/audit CLIs. `policy_compare.py` has no production caller, but is an intentional legacy-versus-compiled comparison utility covered by its test. `app-server-requirements.txt` is an installation artifact referenced by the direct-input README.

See the machine-readable per-file evidence in [script-usage-audit.json](script-usage-audit.json).
