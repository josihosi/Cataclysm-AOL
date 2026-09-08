# R-MAINT-CONSULT-001: supported transport and missing advisory adapter

Observed locally 2026-09-08: `/opt/homebrew/bin/openclaw agent --help` exposes Gateway agent turns,
`--agent`, `--session-key`, `--message-file`, `--json`, and optional delivery. The configured target is
Josef's persistent conversational `astra-mutator-relay`, session key
`agent:astra-mutator-relay:discord:channel:1546286063335510147`. It is not this exclusive reviewer or
a raw Codex thread. Inspect current host binding before use; a model/persona string is not identity.
The return-to-caller transport shape (not yet an authenticated task adapter) is:

```text
/opt/homebrew/bin/openclaw agent --agent astra-mutator-relay --session-key agent:astra-mutator-relay:discord:channel:1546286063335510147 --message-file REQUEST_PATH --json
```

Run as an argument array without a shell. Omit `--deliver`: the requested advisory reply belongs to
originating Sol, not an unsolicited Discord post. Retain the structured Gateway run/result identity
and payload. CLI availability and a healthy endpoint do not prove a role/task-bound round trip.
No host credential or access change is currently shown necessary.

Implement the smallest repository adapter over this transport. Bind the caller to the current
coordinator supervisor/task ownership in `.de67/state/deadlines.sqlite3`, not self-asserted environment
or packet role alone. Inspect its schema and existing run records. Bind workspace/lineage/run/task,
assignment revision and request identity, configured recipient role, original evidence references
and reply. Retain pending/unavailable states; deduplicate retries without silently sending twice,
reject stale/rebound tasks and unauthorized sender/workspace, and prevent reply-to-request echoes.
Use existing task context and retained artifacts rather than full transcripts or a new report system.
Do not launch another coordinator. Validate the host execution identity available to the CLI; if it
cannot support the required sender validation, return that precise capability/setup gap.

Sol consults after local diagnosis for consequential cross-task obstruction, uncertain responsibility,
contradictory specification/evidence, architectural decisions, exhausted tactic versus method issue,
or scope/ownership conflict. Packet: task/run/revision, intended outcome, current state/first
divergence, exact evidence, tried/learned, constraints/live owner and decision needed. Routine repair
and allocation stay with Sol/workers. Advice states evidence limits and change/no-change recommendation;
it never changes owner intent, repair authority, DFS, gates, live ownership or queue provenance.

Recipient role source is `/Users/josefhorvath/.openclaw/workspaces/astra-mutator-relay/AGENTS.md`.
Its `mutator_mailbox.py::_format_entry` unconditionally labels enqueued text Owner-authorized and
its receipt identity does not include task/revision: it is an owner submission tool, not an advisory
channel. Never feed agent requests/replies into that owner path. Clearly agent-authored method
recommendations may follow the existing non-forcing review mechanism under current authority.
Preserve/merge existing host guidance; no plugins, secret collection, global access change or raw
thread UUID integration. Host config stays host-owned.

Proof obligation: deterministic wrong sender/workspace, stale revision, duplicate requests/replies,
unavailable endpoint, mixed writers and no-authority-escalation; then one harmless diagnostic request
from the next ordinary Sol run and correlated conversational reply, with no gameplay input or owner
queue mutation. Reviewer did not launch Sol or claim this test. Sending/running/timeout is not done;
keep task ownership and concrete next work. This named assignment is the continuation for the missing
adapter, not a claim the channel is delivered.

Transport docs: https://docs.openclaw.ai/cli/agent . Codex thread/turn docs:
https://learn.chatgpt.com/docs/app-server ; raw Codex IDs do not authenticate this role.
