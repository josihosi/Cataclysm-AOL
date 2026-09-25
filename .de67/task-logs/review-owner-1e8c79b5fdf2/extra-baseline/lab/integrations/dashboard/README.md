# de67 dashboard

This optional package serves a small, read-only view of one de67 workspace. It reads
`.de67/FS.md`, `.de67/work-ledger.md`, the deadline SQLite database, and supervisor process state.
It does not import de67 core, write workspace state, or require OpenClaw.

Run on loopback:

```sh
python3 integrations/dashboard/de67_dashboard.py --workspace /path/to/project
```

Open `http://127.0.0.1:8767`. The visible tab updates in place every 30 seconds without a page reload. Updates pause while
the tab is hidden or text is selected. The last good view stays visible if the connection fails.
Use **Refresh snapshot** for an immediate update; `--refresh-seconds 0` disables automatic
updates. Set another interval, such as `--refresh-seconds 900`, for fifteen-minute updates.

The coordinator sun's corona settles close while it waits and expands gently while it
works, using its current session's execution signals at the same refresh interval.
Unavailable activity leaves the corona quiet; reduced-motion preferences disable the transition.

For the optional OpenClaw mutator conversation, add
`--mutator-activity-db /path/to/agents/MUTATOR/agent/openclaw-agent.sqlite`, pointing to the
dedicated mutator agent's store. The galaxy glows during mutation reviews, queued messages,
and conversation work, then fades when activity ends. Quick replies remain visible for one
refresh interval. The tooltip distinguishes reviewing, queued, working, and replied states.
This reads only current session activity metadata; it neither sends messages nor changes
OpenClaw, and message contents stay private. Archived sessions and old session windows do
not count. A missing or unavailable store leaves normal mutation-review indication intact.

Home-network exposure is explicit:

```sh
python3 integrations/dashboard/de67_dashboard.py \
  --workspace /path/to/project --bind 0.0.0.0 --port 8767
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

## Weekly subscription fuel

Add `--subscription-codex /path/to/codex` to show account-wide weekly allowance remaining,
percentage used, and the reset date in the dashboard host's time zone. This optional source
reads `account/rateLimits/read` over a private Codex App Server connection once per minute;
it makes no model requests and does not write the project workspace. The existing campaign
fresh-token chart remains separate from subscription quota.

The lowercase red `ngmi` means percentage used exceeds percentage of the weekly window elapsed:
continuing that weekly average would exhaust the allowance before reset. It is a pace estimate,
not a prediction of future activity. Missing reset data leaves pace unknown. A failed read
preserves the last good value marked stale and suppresses the pace verdict until a fresh read.

## Optional Fratbro status

The dashboard can ask a read-only Luna-low narrator to translate current agent activity into one
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
Luna low in a read-only sandbox, and writes only the configured external cache. It cannot steer
the coordinator, supervisor, ledger, or delivery loop.

Run focused tests:

```sh
python3 -m unittest integrations/dashboard/test_de67_dashboard.py
```

## Token use

The token panel counts observed campaign usage: input minus cached input plus output.
It includes coordinator, mutator, Terra-worker, and Luna-worker sessions, including completed
workers. Other or unknown worker models remain separately accounted for when present.
Missing session records make the total explicitly partial. The graph stacks the four roles into their combined total in one-hour
bins across the last 24 hours, with a linear scale that follows observed use.
Between the plots, a shared color legend identifies each role. A logarithmic dot plot
compares campaign totals in fixed mutator, coordinator, Terra, Luna order. Its axis rounds
around the lowest and highest positive totals, omitting the unused low end; zero totals
retain their numeric label without a dot. Hover for exact counts. The mutator uses the
lit galaxy's star color throughout both plots. The combined total sits at the bottom. Narrator and
unrelated sessions are excluded. Accounting uses the local Codex session index and logs;
unavailable accounting leaves only this panel unavailable.

The refresh script is bundled locally and only fetches this dashboard's own pages. No remote
JavaScript or new model calls are introduced. Narration remains tied to worker lifecycle changes.
For browser integration checks, install `playwright-core` in a test environment and run
`node integrations/dashboard/test_live_refresh.mjs`. Set `DE67_CHROMIUM` to an installed Chromium
executable and, if needed, `DE67_PLAYWRIGHT` to the local playwright-core module directory.
