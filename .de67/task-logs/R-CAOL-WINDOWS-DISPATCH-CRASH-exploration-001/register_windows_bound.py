"""Build an isolated registry projection and query the fixed Windows witness."""

import contextlib
import io
import json
import os
from pathlib import Path
import sys
import types

task = Path(__file__).resolve().parent
root = task.parents[1]
os.environ["PATH"] = os.pathsep.join(
    [str(Path("C:/Users/josef/dev/msys64/ucrt64/bin")), os.environ["PATH"]]
)
sys.modules.setdefault("resource", types.ModuleType("resource"))
sys.path.insert(0, str(root / "tools/openclaw_harness"))
import scenario_registry_cli

registry = task / "dispatch-crash-registry.sqlite3"
scenarios = task / "scenarios"
charter = task / "windows-dispatch-witness-charter.json"
brief = task / "windows-dispatch-coordinator-brief.json"

def invoke(name, arguments):
    sys.argv = ["scenario_registry_cli.py", "--json", "--registry", str(registry),
                name, *arguments]
    output = io.StringIO()
    with contextlib.redirect_stdout(output):
        code = scenario_registry_cli.main()
    payload = output.getvalue()
    (task / f"{name}.json").write_text(payload, encoding="utf-8")
    if code:
        raise RuntimeError(f"{name} exit {code}: {payload[:1000]}")
    return json.loads(payload)

rebuild = invoke("rebuild", ["--scenarios-root", str(scenarios)])
typed_query = json.loads(brief.read_text(encoding="utf-8"))["query"]
query = invoke("registry-query", [
    "--query-json", json.dumps(typed_query),
    "--coordinator-brief", str(brief), "--witness-charter", str(charter), "--full",
])
print(json.dumps({"registry": str(registry), "rebuild": rebuild,
                  "query": query}, indent=2))
