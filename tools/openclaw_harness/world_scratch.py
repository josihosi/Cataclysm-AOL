"""Select and launch an ordinary cockpit on a disposable copy of a prepared save."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time
import uuid

import startup_harness as harness


SCENARIO = "caol.scratch_world"
DECLARATION = Path(__file__).with_name("scenarios") / (SCENARIO + ".json")
REGISTRY_CLI = Path(__file__).with_name("scenario_registry_cli.py")
QUERY = {"requirements": [{"key": "capabilities.caol.scratch_world", "op": "eq",
                           "value": "prepared_native_session", "minimum_evidence": "declared"}],
         "preferences": []}


def selected_scratch(selection):
    """Recover the per-run inputs from the same brief sealed in the registry token."""
    if selection.scenario != SCENARIO:
        return None
    from scenario_registry_store import ScenarioRegistryStoreError
    brief = getattr(selection, "outcome_brief", {}) or {}
    binding = brief.get("scratch_world")
    if (not isinstance(binding, dict) or
            not re.fullmatch("[0-9a-f]{32}", str(binding.get("attempt_id", ""))) or
            not re.fullmatch("[0-9a-f]{64}", str(binding.get("sha256", ""))) or
            not isinstance(binding.get("source"), str)):
        raise ScenarioRegistryStoreError("Scratch launch needs a prepared world. Use world scratch WORLD.")
    source = Path(binding["source"])
    if not source.is_absolute() or not source.name or source.name in {".", ".."} or not source.is_dir():
        raise ScenarioRegistryStoreError(f"Scratch source world is unavailable: {source}")
    return source, "scratch-" + binding["attempt_id"][:16], binding["sha256"]


def local_runner_options(config_profile):
    """A fresh scratch profile can reuse the Windows installation's configured interpreter."""
    if sys.platform != "win32" or harness.load_game_options(config_profile).get("LLM_INTENT_PYTHON"):
        return {}
    try:
        rows = json.loads((harness.repo_root() / "config" / "options.json").read_bytes())
    except (OSError, ValueError):
        return {}
    if isinstance(rows, list):
        for row in rows:
            if isinstance(row, dict) and row.get("name") == "LLM_INTENT_PYTHON":
                value = str(row.get("value", "")).strip()
                if value and harness.resolve_configured_python_command(value)[0]:
                    return {"LLM_INTENT_PYTHON": value}
    return {}


def _registry(arguments, registry):
    command = [sys.executable, str(REGISTRY_CLI), "--json"]
    if registry:
        command.extend(["--registry", str(registry)])
    from bandit_live_world_audit import zzip_environment
    process = subprocess.run([*command, *arguments], cwd=harness.repo_root(), env=zzip_environment(harness.repo_root()),
                             capture_output=True, text=True, encoding="utf-8", errors="replace")
    try:
        value = json.loads(process.stdout if process.returncode == 0 else process.stderr)
    except ValueError:
        message = (process.stderr or process.stdout).strip()
        raise ValueError(message[-1600:] or f"Registry exited {process.returncode}") from None
    if not value.get("ok"):
        raise ValueError(str(value.get("error") or value.get("result") or value))
    return value


def _installed_tiles_receipts():
    """Newest installed publication for each local executable, without launching any games."""
    root = harness.repo_root() / ".userdata" / "openclaw_harness" / "source_bindings"
    seen = set()
    for path in sorted(root.glob("*.json"), key=lambda entry: entry.stat().st_mtime, reverse=True):
        try:
            receipt = json.loads(path.read_bytes())
            exe = Path(receipt["executable_path"])
            if (exe not in seen and exe.is_file() and receipt.get("schema") == harness.PRODUCT_BUILD_RECEIPT_SCHEMA
                    and receipt.get("build_configuration", {}).get("renderer") == "tiles"):
                seen.add(exe)
                yield path, receipt
        except (ValueError, KeyError, OSError, TypeError, AttributeError):
            continue


def _wait_for_bridge(session, binding_id, timeout=120.0):
    """Return only observed status. A slow startup retains its live session handle."""
    deadline = time.monotonic() + timeout
    while True:
        try:
            status = json.loads((session / "status.json").read_text(encoding="utf-8"))
        except (FileNotFoundError, json.JSONDecodeError):
            status = {}
        if status and status.get("binding_id") != binding_id:
            return {"status": "blocked", "reason": "scratch bridge identity changed"}
        state = status.get("state")
        if state == "ready":
            descriptor = status.get("session_descriptor") or {}
            if descriptor.get("run_id"):
                return {"status": "launched", "native_run_id": descriptor["run_id"]}
        if state in {"process_dead", "bridge_failed", "cleaned", "terminalization_failed", "reentry_failed"}:
            diagnostic = session / "child.stderr.log"
            reason = status.get("reason") or state
            try:
                with diagnostic.open("rb") as stream:
                    stream.seek(max(0, diagnostic.stat().st_size - 4096))
                    errors = stream.read().decode("utf-8", errors="replace").splitlines()
                cause = next((line for line in reversed(errors) if line.startswith("Cannot proceed: ")), "")
                if cause:
                    reason = cause.removeprefix("Cannot proceed: ")
            except OSError:
                pass
            return {"status": "blocked", "reason": reason, "diagnostic": str(diagnostic)}
        if time.monotonic() >= deadline:
            return {"status": "starting", "reason": "Startup is pending on the returned session."}
        time.sleep(0.25)


def launch_scratch(world: Path, *, executable: Path | None = None, build_receipt: Path | None = None,
                   dry_run: bool = False, question: str = "", family: str = "", registry=None) -> dict:
    """Check a build, select a single reusable declaration and open its bound disposable world."""
    import scenario_registry_cli as cli
    from scenario_registry_store import ScenarioRegistryStoreError
    from world_inspect import resolve_world

    attempt = uuid.uuid4().hex
    question = question.strip() or "Explore the prepared world and record what happens."
    profile = "scratch-" + attempt[:16]
    result = {"status": "blocked"}
    source = Path(world)
    build = {}
    recipe = {}
    source_binding = {}
    parent_source = ""
    selection_token = ""
    try:
        if executable and build_receipt:
            raise ValueError("Choose an executable or an immutable build receipt.")
        source = resolve_world(source).resolve(strict=True)
        player_files = sorted({p.name.removesuffix(".zzip") for pattern in ("*.sav", "*.sav.zzip")
                               for p in source.glob(pattern) if p.is_file()})
        if not (source / "worldoptions.json").is_file() or not player_files:
            raise ValueError(f"Scratch needs a saved player and world options in {source}")
        if len(player_files) == 1:
            player = player_files[0]
            source_binding["player_save"] = player if (source / player).is_file() else player + ".zzip"
        prep_file = source.parent.parent / "preparation.json" if source.parent.name == "save" else None
        if prep_file and prep_file.is_file():
            preparation = json.loads(prep_file.read_text(encoding="utf-8"))
            if preparation.get("schema") == "caol-world-preparation-v1":
                parent_source = str(preparation.get("source", ""))
                source_binding.update(parent=parent_source, parent_sha256=preparation.get("source_sha256"),
                                      prepared_sha256=preparation.get("world_sha256"),
                                      player_save=preparation.get("player_save"))
            if parent_source and isinstance(preparation.get("recipe"), dict):
                recipe = preparation["recipe"]
        args = argparse.Namespace(executable=str(executable or ""), run_build_receipt=str(build_receipt or ""))
        if build_receipt:
            binding = cli._resolve_run_build(args)
            candidate = binding["executable_path"]
            published = binding["selected_product_build"]
        else:
            candidate = str(executable or "")
            published = None
        readiness = cli._current_source_executable_readiness(executable=candidate, selected_product_build=published)
        if not executable and not build_receipt and readiness.get("status") == "build_required":
            alternatives = []
            product_digest = readiness.get("product_source_sha256")
            for path, receipt in _installed_tiles_receipts():
                if product_digest and receipt.get("product_source_sha256") == product_digest:
                    candidate = cli._current_source_executable_readiness(executable=receipt["executable_path"])
                    if candidate.get("status") == "ready":
                        readiness = candidate
                        break
                if len(alternatives) < 3:
                    alternatives.append({"captured_head": receipt.get("captured_head"), "receipt": str(path)})
            if readiness.get("status") == "build_required" and alternatives:
                result.update(available_builds=alternatives,
                              select_build="Choose an installed product with --build-receipt PATH.")
        build = {k: readiness.get(k) for k in ("executable_path", "executable_sha256", "captured_head")}
        if published:
            build.update(product_source_sha256=published["product_source_sha256"], receipt_path=published["receipt_path"])
        if readiness["status"] != "ready":
            result.update(reason=readiness.get("reason"), readiness=readiness["status"],
                          next_action=readiness.get("next_action"), build_entrypoint=readiness.get("build_entrypoint"))
        else:
            digest, error = harness.sha256_tree(source)
            if error:
                raise ValueError(error)
            source_binding["world_sha256"] = digest
            source_binding.setdefault("parent_sha256", digest)
            source_binding.setdefault("parent", str(source))
            brief = {"outcome": question, "scratch_world": {
                "attempt_id": attempt, "source": str(source), "sha256": digest}}
            if dry_run:
                return {"status": "preflight_passed", "source": str(source), "world_sha256": digest,
                        "build": build, "scenario": SCENARIO,
                        "scope": "Build and saved-world checks passed. Registry selection runs at launch."}
            _registry(["rebuild", "--source", str(DECLARATION)], registry)
            with tempfile.TemporaryDirectory(prefix="caol-scratch-brief-") as temp:
                brief_path = Path(temp) / "brief.json"
                brief_path.write_text(json.dumps(brief), encoding="utf-8")
                selection = ["registry-query", "--scenario-id", SCENARIO,
                             "--query-json", json.dumps(QUERY), "--coordinator-brief", str(brief_path)]
                if build_receipt:
                    selection.extend(["--run-build-receipt", str(Path(build_receipt).resolve())])
                elif build["executable_path"]:
                    selection.extend(["--executable", build["executable_path"]])
                query = _registry(selection, registry)["result"]
            action = query.get("next_action") or {}
            if action.get("kind") != "launch_selected_scenario":
                raise ValueError(str(action.get("action") or action.get("reason") or action))
            selection_token = query["token_id"]
            argv = action["command"]["argv"]
            index = argv.index("registry-detached-launch")
            launched = _registry(argv[index:], registry)
            session = Path(launched["session_dir"])
            result = {"session": str(session), "binding_id": launched["binding_id"],
                      "profile": profile, "world": source.name}
            print(f"Scratch session {session}", file=sys.stderr, flush=True)
            try:
                result.update(_wait_for_bridge(session, launched["binding_id"]))
            except KeyboardInterrupt:
                result.update(status="starting", reason="Launch continues on the returned session.")
    except (OSError, KeyError, ValueError, ScenarioRegistryStoreError, SystemExit) as error:
        result.update(status="blocked", reason=str(error))
    result.update(source=str(source), build=build)
    if not dry_run:
        from world_examples import record_attempt
        record = record_attempt(registry=registry, source_binding=source_binding, attempt_id=attempt, question=question, family=family,
                                source=parent_source or str(source), recipe=recipe, build=build, outcome=result["status"],
                                detail=str(result.get("reason", "")), native_run_id=result.get("native_run_id", ""),
                                selection_token=selection_token)
        result["run_id"] = record["run_id"]
    return result
