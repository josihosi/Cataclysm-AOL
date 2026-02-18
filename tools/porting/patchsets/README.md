# Patchset Files

These files define the commit queues for patchset-based porting.

## Files
- `common.txt`: commits applied to every target.
- `cdda-master.txt`: optional fixups applied only for `cdda-master`.
- `cdda-0.H.txt`: optional fixups applied only for `cdda-0.H`.
- `cdda-0.I.txt`: optional fixups applied only for `cdda-0.I`.
- `ctlg-master.txt`: optional fixups applied only for `ctlg-master`.

## Format
- One commit SHA per line.
- Keep order oldest -> newest.
- Lines starting with `#` are comments.

## Workflow
1. Curate `common.txt` to only AOL/LLM commits.
2. Add branch-specific repair commits into target files.
3. Run `simulate_patchset.ps1` before real port runs.
