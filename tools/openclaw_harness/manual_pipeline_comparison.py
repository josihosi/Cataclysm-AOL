#!/usr/bin/env python3
"""Own a finite, manual-input Tiles comparison arm from an isolated save.

The semantic-off arm deliberately cannot supply cockpit action or native turn
evidence. Both arms use the same manual input and external measurement route.
"""
import argparse
import json
import shutil
import sys
import uuid
from pathlib import Path

import startup_harness as harness
from writable_run_owner import own_writable_root


def run(args):
    profile = harness.resolve_profile_name(args.profile)
    root = harness.userdir_for_profile(profile)
    if root.exists() or root.is_symlink():
        raise ValueError("comparison profile must be fresh")
    executable = Path(args.executable).resolve(strict=True)
    receipt = Path(args.receipt).resolve(strict=True)
    receipt_hash, error = harness.sha256_file(receipt)
    if error or receipt_hash != args.receipt_sha256:
        raise ValueError("product receipt hash mismatch")
    product = json.loads(receipt.read_text(encoding="utf-8"))
    selected = {"schema": harness.SELECTED_PRODUCT_BUILD_SCHEMA,
                "receipt_path": str(receipt), "receipt_sha256": receipt_hash,
                "executable_sha256": product["executable_sha256"],
                "product_source_sha256": product["product_source_sha256"]}
    binding = harness.build_runtime_binding(executable, selected_product_build=selected)
    if not binding.get("ok"):
        raise ValueError("product binding unavailable: " + str(binding.get("error")))
    source = Path(args.saved_world).resolve(strict=True)
    source_hash, error = harness.sha256_tree(source)
    if error or source_hash != args.saved_world_sha256:
        raise ValueError("saved world hash mismatch")
    config = Path(args.config_source).resolve(strict=True)
    config_files = ("options.json", "keybindings.json", "fonts.json", "base_colors.json", "imgui_style.json")
    run_id = uuid.uuid4().hex
    with own_writable_root(root, run_id, harness.process_generation_snapshot,
                           harness.process_generation_matches):
        config_destination = harness.config_dir_for_profile(profile)
        config_destination.mkdir(parents=True, exist_ok=True)
        for name in config_files:
            source_file = config / name
            if source_file.is_file() and not source_file.is_symlink():
                shutil.copy2(source_file, config_destination / name)
        installed = harness.install_saved_world_snapshot(
            profile, args.world, source, disposable_run_id=run_id, require_fresh=True)
        run_dir = root / "harness_runs" / run_id
        run_dir.mkdir(parents=True, exist_ok=False)
        harness.write_json(run_dir / "runtime.binding.json", binding)
        harness.write_json(run_dir / "manual-comparison.json", {
            "schema": "caol-manual-pipeline-comparison-v1", "run_id": run_id,
            "profile": profile, "world": args.world, "mode": args.mode,
            "input": "manual GUI", "native_action_or_turn_proof": False,
            "saved_world": installed, "runtime_binding": binding,
        })
        # Neither arm inherits a caller's optional harness instrumentation.
        # launch_game adds only the selected run's own bindings afterward.
        environment = {key: value for key, value in harness.game_child_environment(profile).items()
                       if not key.startswith("OPENCLAW_HARNESS_")}
        config_binding = {}
        for name in config_files:
            path = config_destination / name
            if path.is_file():
                digest, error = harness.sha256_file(path)
                if error:
                    raise ValueError("comparison configuration hash unavailable")
                config_binding[name] = digest
        harness.write_json(run_dir / "manual-config.binding.json", config_binding)
        proc = harness.launch_game(
            profile, args.world, run_dir, executable=str(executable),
            transition_event_run_id=run_id, child_environment=environment,
            diagnostic_semantic_transport=(args.mode == "semantic-on"))
        print(json.dumps({"event": "launched", "run_id": run_id, "pid": proc.pid,
                          "run_dir": str(run_dir), "mode": args.mode}), flush=True)
        code = proc.wait()
        print(json.dumps({"event": "exited", "run_id": run_id,
                          "pid": proc.pid, "exit_code": code}), flush=True)
        return code


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for field in ("profile", "world", "executable", "receipt", "receipt-sha256",
                  "saved-world", "saved-world-sha256", "config-source"):
        parser.add_argument("--" + field, required=True)
    parser.add_argument("--mode", choices=("semantic-on", "semantic-off"), required=True)
    try:
        return run(parser.parse_args())
    except (ValueError, OSError, KeyError) as error:
        print(str(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
