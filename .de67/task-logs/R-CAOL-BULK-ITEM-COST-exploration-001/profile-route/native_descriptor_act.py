import json
import sys
from pathlib import Path

sys.path.insert(0, 'tools/openclaw_harness')
import startup_harness

run_dir = Path(sys.argv[1]).resolve()
run_id, profile, pid, session_id, frame_id, action_id = sys.argv[2:8]
stable_id = sys.argv[8] if len(sys.argv) > 8 else None
trace = run_dir / 'semantic.native.events.jsonl'
descriptor = None
for line in trace.read_text(encoding='utf-8').splitlines():
    try:
        record = json.loads(line.split(': ', 1)[1])
    except (IndexError, json.JSONDecodeError):
        continue
    if record.get('event') == 'surface_descriptor' and record.get('frame_id') == frame_id:
        descriptor = record
if descriptor is None:
    raise SystemExit(f'no native descriptor for frame {frame_id}')
result = startup_harness.execute_semantic_act(
    run_dir=run_dir, profile=profile, run_id=run_id,
    trace_start_offset=255063, pid=int(pid), session_id=session_id,
    frame_id=frame_id, action_id=action_id,
    stable_id=stable_id, observed_frame=descriptor,
    transition_timeout_seconds=90, observe_interval_seconds=0.1,
)
print(json.dumps(result, ensure_ascii=False, indent=2))
