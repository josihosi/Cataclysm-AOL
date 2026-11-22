"""Search playtest results and keep selected examples alongside the scenario registry."""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import sqlite3
import tempfile
from collections.abc import Mapping
from contextlib import closing, nullcontext
from pathlib import Path

from scenario_registry_store import immediate_transaction, open_registry, resolve_registry_path


def report_context(connection, report, facts):
    """Capture the small historical fields that cannot be recovered after cleanup."""
    manifest = facts.get("manifest") or {}
    row = connection.execute(
        "SELECT declaration_json FROM manifest_current WHERE source_path=? AND current_sha256=?",
        (manifest.get("source_path"), manifest.get("source_sha256")),
    ).fetchone()
    declaration = json.loads(row[0]) if row else {}
    contract = report.get("contract")
    contract = contract if isinstance(contract, Mapping) else {}
    final = next((step["cockpit_live_session"]["final"] for step in reversed(report.get("steps") or [])
                  if isinstance(step, Mapping) and isinstance(step.get("cockpit_live_session"), Mapping)
                  and isinstance(step["cockpit_live_session"].get("final"), Mapping)), {})
    runtime = facts.get("runtime") or {}
    build = runtime.get("runtime_binding_observed") or {}
    stop = final.get("stop_detail")
    stop = stop if isinstance(stop, Mapping) else {}
    observed = stop.get("gameplay_outcome")
    observed = observed if isinstance(observed, Mapping) else {}
    return {
        "scenario": facts.get("scenario") or "",
        "native_run_id": final.get("run_id") or report.get("run_id") or "",
        "question": declaration.get("description", ""),
        "family": (declaration.get("catalogue") or {}).get("family_id", ""),
        "source": (declaration.get("saved_world_snapshot") or contract.get("saved_world_snapshot")
                   or declaration.get("fixture") or contract.get("fixture") or ""),
        "build": {key: build[key] for key in ("executable_path", "executable_sha256", "runtime_source_sha256")
                  if isinstance(build, Mapping) and build.get(key)},
        "recipe": {key: contract.get(key) or declaration[key] for key in
                   ("name", "world", "fixture_profile", "fixture", "saved_world_snapshot")
                   if contract.get(key) or declaration.get(key)} | {"manifest_sha256": manifest.get("source_sha256", "")},
        "reported_outcome": str(observed.get("verdict", ""))[:120],
        "detail": str(observed.get("summary") or stop.get("requested_stop_reason")
                      or final.get("stop_reason") or "")[:2000],
    }


def _bind_attempt_source(conn, attempt_id, source_binding):
    if not isinstance(source_binding, Mapping):
        raise ValueError("The scratch source binding must be an object")
    row = conn.execute("SELECT source_binding_json FROM world_scratch_attempt WHERE run_id=?", (attempt_id,)).fetchone()
    if row is None:
        raise ValueError("Unknown scratch attempt")
    bound = json.loads(row[0])
    for key, value in source_binding.items():
        if value is None or value == "":
            continue
        if key in bound and bound[key] != value:
            raise ValueError(f"This scratch attempt already records a different {key}")
        bound[key] = value
    conn.execute("UPDATE world_scratch_attempt SET source_binding_json=? WHERE run_id=?",
                 (json.dumps(bound, sort_keys=True), attempt_id))
    return bound


def bind_attempt_source(attempt_id, source_binding, *, registry=None):
    """Attach previously missing source facts recovered from this exact run."""
    with closing(open_registry(registry)) as conn, immediate_transaction(conn):
        return _bind_attempt_source(conn, attempt_id, source_binding)


def record_attempt(*, attempt_id, question, outcome, registry=None, family="", source="",
                   recipe=None, build=None, detail="", native_run_id="", selection_token="", source_binding=None):
    """Record a real scratch launch or refusal once, before its workspace is removed."""
    if not attempt_id.strip() or not question.strip() or outcome not in {"starting", "launched", "blocked"}:
        raise ValueError("A scratch attempt needs an ID, a question, and a launch outcome")
    details = (question.strip(), family.strip(), str(source), json.dumps(recipe or {}, sort_keys=True),
               json.dumps(build or {}, sort_keys=True), outcome, str(detail), native_run_id, selection_token)
    with closing(open_registry(registry)) as conn, immediate_transaction(conn):
        previous = conn.execute(
            "SELECT question,family,source,recipe_json,build_json,outcome,detail,native_run_id,selection_token "
            "FROM world_scratch_attempt WHERE run_id=?", (attempt_id,),
        ).fetchone()
        if previous is None:
            conn.execute(
                "INSERT INTO world_scratch_attempt(run_id,question,family,source,recipe_json,"
                "build_json,outcome,detail,native_run_id,selection_token) VALUES(?,?,?,?,?,?,?,?,?,?)", (attempt_id, *details),
            )
        elif tuple(previous) != details:
            raise ValueError("This scratch attempt already has a different recorded result")
        if source_binding is not None:
            _bind_attempt_source(conn, attempt_id, source_binding)
    return {"run_id": attempt_id, "outcome": outcome}


# Older registries retain report/verification/token and witness rows even after run files
# disappear. Use those identities directly. A later declaration can describe another world.
_REPORTS = """
WITH terminals AS (
  SELECT json_extract(details_json, '$.report_id') AS report_id, token_id
  FROM token_history WHERE event_kind IN ('verification_run','bootstrap_verification_run','repair_verification_run')
  AND json_type(details_json, '$.report_id')='text'
), witnesses AS (
  SELECT link.report_id, w.witness_id, w.verdict,
         json_extract(w.charter_json,'$.claim') AS claim,
         json_extract(w.statement_json,'$.smallest_supported_claim') AS summary,
         row_number() OVER(PARTITION BY link.report_id ORDER BY w.recorded_at DESC,w.rowid DESC) AS latest
  FROM playtest_witness_report_history AS link JOIN playtest_witness_history AS w USING(witness_id)
)
SELECT r.report_id,r.report_sha256,r.report_kind,r.ingestion_status,r.error_text,r.recorded_at,
       r.run_context_json,t.token_id,w.witness_id,w.verdict,w.claim,w.summary,
       v.outcome_kind,v.proof_status,
       json_extract(v.details_json,'$.scenario') AS scenario,
       json_extract(v.details_json,'$.manifest.source_sha256') AS scenario_sha256,
       json_extract(v.details_json,'$.runtime.runtime_binding_observed.executable_sha256') AS executable_sha256,
       json_extract(v.details_json,'$.runtime.runtime_binding_observed.executable_path') AS executable_path,
       json_extract(v.details_json,'$.runtime.runtime_binding_observed.runtime_source_sha256') AS runtime_source_sha256,
       json_extract(m.declaration_json,'$.description') AS description,
       json_extract(m.declaration_json,'$.catalogue.family_id') AS family,
       json_extract(m.declaration_json,'$.saved_world_snapshot') AS snapshot,
       json_extract(m.declaration_json,'$.fixture') AS fixture
FROM report_ingestion_history AS r
LEFT JOIN verification_history AS v ON v.report_id=r.report_id
LEFT JOIN manifest_current AS m ON m.manifest_id=r.manifest_id
     AND m.current_sha256=json_extract(v.details_json,'$.manifest.source_sha256')
LEFT JOIN witnesses AS w ON w.report_id=r.report_id AND w.latest=1
LEFT JOIN terminals AS t ON t.report_id=r.report_id
"""


def _history_rows(connection):
    has_attempts = connection.execute(
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name='world_scratch_attempt'"
    ).fetchone() is not None
    reports = {}
    tokens = {}
    query = _REPORTS if has_attempts else _REPORTS.replace("r.run_context_json", "NULL AS run_context_json")
    for row in connection.execute(query):
        ctx = json.loads(row["run_context_json"] or "{}")
        native = ctx.get("native_run_id") or row["token_id"]
        run_id = native or row["report_id"]
        scenario = ctx.get("scenario") or row["scenario"] or ""
        entry = {
            "run_id": run_id, "native_run_id": native or "", "scenario": scenario,
            "question": row["claim"] or ctx.get("question") or row["description"] or scenario,
            "family": ctx.get("family") or row["family"] or "",
            "source": ctx.get("source") or row["snapshot"] or row["fixture"] or "",
            "recipe": ctx.get("recipe") or {"name": scenario, "manifest_sha256": row["scenario_sha256"]},
            "build": ctx.get("build") or {key: row[key] for key in
                       ("executable_sha256", "executable_path", "runtime_source_sha256") if row[key]},
            "outcome": row["verdict"] or ctx.get("reported_outcome") or row["outcome_kind"] or row["report_kind"],
            "proof_status": row["proof_status"], "report_verdict": row["outcome_kind"],
            "detail": row["summary"] or ctx.get("detail") or row["error_text"] or "",
            "report_id": row["report_id"], "witness_id": row["witness_id"],
            "ingestion_status": row["ingestion_status"], "recorded_at": row["recorded_at"],
        }
        old = reports.get(run_id)
        if not old or (entry["recorded_at"], entry["report_id"]) > (old["recorded_at"], old["report_id"]):
            reports[run_id] = entry
            if row["token_id"]:
                tokens[row["token_id"]] = run_id
    for row in connection.execute("SELECT * FROM world_scratch_attempt") if has_attempts else ():
        native = row["native_run_id"]
        linked = reports.pop(native, None) or reports.pop(tokens.get(row["selection_token"]), {})
        yield {
            **linked, "run_id": row["run_id"], "native_run_id": linked.get("native_run_id") or native,
            "scenario": linked.get("scenario", ""),
            "selection_token": row["selection_token"],
            "source_binding": json.loads(row["source_binding_json"]) if "source_binding_json" in row.keys() else {},
            "question": row["question"], "family": row["family"], "source": row["source"],
            "build": json.loads(row["build_json"]), "recipe": json.loads(row["recipe_json"]),
            "outcome": linked.get("outcome") or row["outcome"], "detail": linked.get("detail") or row["detail"],
            "recorded_at": linked.get("recorded_at") or row["recorded_at"],
        }
    yield from reports.values()


def _store(registry):
    db = resolve_registry_path(registry)
    return db.with_name(db.name + ".examples")


def _checkpoint_path(registry, run_id, name):
    if not name or name in (".", "..") or Path(name).name != name:
        raise ValueError("Invalid stored checkpoint name")
    key = hashlib.sha256(run_id.encode()).hexdigest()
    path = _store(registry) / key / name
    if path.absolute() != path.resolve():
        raise ValueError("Checkpoint storage contains an alias")
    return path


def history(*, registry=None, search="", family="", kept=False, limit=20):
    if limit < 1:
        raise ValueError("History limit must be positive")
    if not resolve_registry_path(registry).is_file():
        return {"total": 0, "runs": []}
    with closing(open_registry(registry, writable=False)) as conn:
        # Read all projections from one WAL snapshot while concurrent games finish.
        conn.execute("BEGIN")
        has_pins = conn.execute("SELECT 1 FROM sqlite_master WHERE name='kept_world_example'").fetchone()
        pins = {r["run_id"]: r for r in conn.execute("SELECT * FROM kept_world_example WHERE reason<>''")} if has_pins else {}
        found = []
        words = search.casefold().split()
        for row in _history_rows(conn):
            pin = pins.get(row["run_id"])
            if pin and pin["family"]:
                row["family"] = pin["family"]
            if kept and pin is None or family and family != row["family"]:
                continue
            if pin:
                row["reason"] = pin["reason"]
                if pin["checkpoint_name"]:
                    row["checkpoint"] = str(_checkpoint_path(registry, row["run_id"], pin["checkpoint_name"]))
                    row["checkpoint_sha256"] = pin["checkpoint_sha256"]
            if words:
                text = json.dumps(row, ensure_ascii=False).casefold()
                if not all(word in text for word in words):
                    continue
            found.append(row)
        found.sort(key=lambda r: (r["recorded_at"], r["run_id"]), reverse=True)
        return {"total": len(found), "runs": found[:limit]}


def _assert_tree(path):
    for child in (path, *path.rglob("*")):
        if (child.is_symlink() or (os.name == "nt" and child.is_junction()) or
                not (child.is_file() or child.is_dir())):
            raise ValueError(f"Checkpoint contains an alias or special file: {child}")


def _copy_checkpoint(source, staging):
    from startup_harness import (own_writable_root, process_generation_matches, process_generation_snapshot,
                                 sha256_tree, _validate_closed_saved_world_owner)
    source = Path(source).absolute()
    if source.is_symlink() or (os.name == "nt" and source.is_junction()):
        raise ValueError("Checkpoint source cannot be an alias")
    source = source.resolve()
    players = (*source.glob("*.sav"), *source.glob("*.sav.zzip"))
    if not (source / "worldoptions.json").is_file() or not any(p.is_file() for p in players):
        raise ValueError("Checkpoint must be a saved world containing a character")
    if source == staging or source in staging.parents:
        raise ValueError("Checkpoint storage cannot be inside its source")
    owner = nullcontext()
    if source.parent.name == "save":
        profile = source.parent.parent
        if (profile / ".harness-owner.lock").exists():
            owner = own_writable_root(profile, "keep-world-example", process_generation_snapshot,
                                      process_generation_matches, prior_validator=_validate_closed_saved_world_owner,
                                      preserve_prior=True)
    with owner:
        _assert_tree(source)
        before, err = sha256_tree(source)
        if err:
            raise ValueError(err)
        copied = staging / source.name
        shutil.copytree(source, copied, symlinks=True)
        _assert_tree(copied)
        after, err = sha256_tree(copied)
        again, current_err = sha256_tree(source)
        if err or current_err or not before == after == again:
            raise ValueError("Checkpoint changed during capture")
        return copied, after


def keep(run_id, reason, *, registry=None, checkpoint=None, family=""):
    reason = reason.strip()
    if not reason:
        raise ValueError("Give a short reason for keeping this example")
    with closing(open_registry(registry)) as conn:
        if not any(row["run_id"] == run_id for row in _history_rows(conn)):
            raise ValueError("Unknown run ID; use world history to find the exact run")
        root = _store(registry)
        if checkpoint:
            if root.resolve() != root:
                raise ValueError("Checkpoint storage contains an alias")
            root.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=root, prefix=".saving-") if checkpoint else nullcontext() as tmp:
            copied, digest = _copy_checkpoint(checkpoint, Path(tmp)) if checkpoint else (None, None)
            placed = None
            try:
                with immediate_transaction(conn):
                    previous = conn.execute("SELECT * FROM kept_world_example WHERE run_id=?", (run_id,)).fetchone()
                    if previous and not previous["reason"]:
                        _release_checkpoint(registry, previous)
                        previous = None
                    name = previous["checkpoint_name"] if previous else None
                    old_digest = previous["checkpoint_sha256"] if previous else None
                    if copied and name and (digest != old_digest or name != copied.name):
                        raise ValueError("Example already has another checkpoint; unkeep it before replacing it")
                    if copied:
                        name = copied.name
                        dest = _checkpoint_path(registry, run_id, name)
                        if not dest.exists():
                            dest.parent.mkdir(exist_ok=True)
                            copied.rename(dest)
                            placed = dest
                        else:
                            from startup_harness import sha256_tree
                            _assert_tree(dest)
                            if sha256_tree(dest) != (digest, ""):
                                raise ValueError("Checkpoint storage is occupied or has changed")
                    conn.execute(
                        "INSERT INTO kept_world_example(run_id,reason,family,checkpoint_name,checkpoint_sha256) VALUES(?,?,?,?,?) "
                        "ON CONFLICT(run_id) DO UPDATE SET reason=excluded.reason,family=excluded.family,"
                        "checkpoint_name=excluded.checkpoint_name,checkpoint_sha256=excluded.checkpoint_sha256",
                        (run_id, reason, family or (previous["family"] if previous else ""), name, digest or old_digest),
                    )
            except BaseException:
                if placed:
                    placed.rename(copied)
                    placed.parent.rmdir()
                raise
    result = {"run_id": run_id, "reason": reason}
    if name:
        result["checkpoint"] = str(_checkpoint_path(registry, run_id, name))
    return result


def _release_checkpoint(registry, row):
    if row["checkpoint_name"]:
        path = _checkpoint_path(registry, row["run_id"], row["checkpoint_name"])
        if path.exists():
            _assert_tree(path)
            shutil.rmtree(path)
        if path.parent.is_dir() and not any(path.parent.iterdir()):
            path.parent.rmdir()


def unkeep(run_id, *, registry=None):
    with closing(open_registry(registry)) as conn:
        # Commit the release first so a partially removed save is never advertised.
        # The pending row keeps enough information for a retry after an interruption.
        with immediate_transaction(conn):
            conn.execute("UPDATE kept_world_example SET reason='' WHERE run_id=?", (run_id,))
        with immediate_transaction(conn):
            row = conn.execute("SELECT * FROM kept_world_example WHERE run_id=? AND reason=''", (run_id,)).fetchone()
            if row:
                _release_checkpoint(registry, row)
                conn.execute("DELETE FROM kept_world_example WHERE run_id=?", (run_id,))
    return {"run_id": run_id, "kept": False}


def add_commands(subparsers):
    query = subparsers.add_parser("history", help="Search recorded runs, blocked attempts, and kept examples")
    query.add_argument("search", nargs="*", help="Words to find in the same run")
    query.add_argument("--family", default="")
    query.add_argument("--kept", action="store_true")
    query.add_argument("--limit", type=int, default=20)
    query.add_argument("--json", action="store_true", help="Include full recipe, build, and evidence handles")
    pin = subparsers.add_parser("keep", help="Keep this example without adding a catalogue scenario")
    pin.add_argument("run_id")
    pin.add_argument("--reason", required=True)
    pin.add_argument("--family", default="", help="Group this example under an existing playtest question")
    pin.add_argument("--checkpoint", type=Path, help="Closed saved world to retain with this example")
    release = subparsers.add_parser("unkeep", help="Release an example and its retained copy; preserve the run result")
    release.add_argument("run_id")
    for command, action in ((query, "history"), (pin, "keep"), (release, "unkeep")):
        command.set_defaults(example_action=action)
        command.add_argument("--registry", help="Scenario registry SQLite file")


def run_command(args):
    if args.example_action == "history":
        value = history(registry=args.registry, search=" ".join(args.search), family=args.family,
                        kept=args.kept, limit=args.limit)
        if not args.json:
            print(f"{value['total']} matching playtest{'' if value['total'] == 1 else 's'}")
            for row in value["runs"]:
                summary = " ".join(str(row["question"] or row["scenario"]).split())
                print(f"{row['run_id']}  [{row['outcome']}]  {summary[:200]}")
                if row.get("reason") or row.get("detail"):
                    print("  " + " ".join((row.get("reason") or row["detail"]).split())[:220])
            return 0
    elif args.example_action == "keep":
        value = keep(args.run_id, args.reason, registry=args.registry, checkpoint=args.checkpoint, family=args.family)
    else:
        value = unkeep(args.run_id, registry=args.registry)
    print(json.dumps(value, indent=2, ensure_ascii=False))
    return 0
