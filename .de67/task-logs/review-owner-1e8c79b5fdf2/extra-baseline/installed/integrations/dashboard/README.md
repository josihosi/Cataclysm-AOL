# de67 dashboard

This optional package serves a small, read-only view of one de67 workspace. It reads
`.de67/FS.md`, `.de67/work-ledger.md`, the deadline SQLite database, and supervisor process state.
It does not import de67 core, write workspace state, or require OpenClaw.

Run on loopback:

```sh
python3 integrations/dashboard/de67_dashboard.py --workspace /path/to/project
```

Open `http://127.0.0.1:8767`. Add `--refresh-seconds 5` for browser-driven live refresh. The
default is manual refresh, so an idle browser causes no filesystem or database reads.

Home-network exposure is explicit:

```sh
python3 integrations/dashboard/de67_dashboard.py \
  --workspace /path/to/project --bind 0.0.0.0 --port 8767 --refresh-seconds 5
```

Direct LAN binding has no authentication or TLS. Prefer loopback behind an authenticated Tailscale
or HTTPS proxy when the workspace contents are sensitive.

The dashboard uses only Python's standard library. It keeps the last good Markdown and clock panel
in memory when a source becomes unavailable. A missing workspace, locked database, malformed text,
unknown schema, failed process probe, or dashboard crash cannot stop or mutate de67.

The overview shows active Luna, Terra, and Sol workers by reasoning effort from Codex's existing local
session records. It does not add worker fields or coordinator reporting. Use `--codex-sessions PATH`
when the session root is not `~/.codex/sessions`; an unavailable source leaves only that table
unavailable. The mutation tile combines completed deadline, integrity, and random mutations and
shows how many more worker results remain before the pending random mutation.

The optional trajectory spider runs the existing read-only sidecar when the clock state changes:

```sh
python3 integrations/dashboard/de67_dashboard.py \
  --workspace /path/to/project \
  --sidecar-script de-67-3/scripts/trajectory_sidecar.py
```

The report stays in memory until the clock changes. The dashboard does not create sidecar
snapshots, history files, or refresh artifacts. A missing or failed sidecar cannot stop de67.
The plot keeps current closure gaps around the claim, draws product and test cosine similarity
along each spoke, and shows the sidecar's categorical trajectory observations without scoring them.

## Optional Fratbro status

The dashboard can ask a read-only Luna-medium narrator to translate current agent activity into one
short, natural paragraph directly below the trajectory plot. Each update starts from first
principles, assumes no prior project knowledge, and plainly explains the concrete work, observed
result, whether it is progress or churn, and what happens next.

This feature is off by default. Core de67 never starts it, and running the dashboard without these
flags creates no narrator process or model usage:

```sh
python3 integrations/dashboard/de67_dashboard.py \
  --workspace /path/to/project \
  --sidecar-script de-67-3/scripts/trajectory_sidecar.py \
  --fratbro-script integrations/dashboard/fratbro_narrator.py \
  --fratbro-cache "/path/outside/project/fratbro-status.json" \
  --fratbro-codex /path/to/codex
```

The dashboard starts one narrator when a worker becomes durably active and once more when that
worker finishes. Tool calls, commentary, ledger edits, and page refreshes reuse the cached summary
instead of spending another model call. The narrator reads the workspace and session records, runs
Luna medium in a read-only sandbox, and writes only the configured external cache. It cannot steer
the coordinator, supervisor, ledger, or delivery loop.

Run focused tests:

```sh
python3 -m unittest integrations/dashboard/test_de67_dashboard.py
```
