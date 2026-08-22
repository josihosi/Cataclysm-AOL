import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from identity_binding import (  # noqa: E402
    IdentityBindingError,
    RoundManifestError,
    byte_tree_digest,
    complete_identity_binding,
    git_worktree_identity,
    authoritative_identity_binding,
    file_identity,
    seal_complete_round_manifest,
    compare_round_manifest,
    first_certification_mismatch,
    order_certification_mismatches,
)


class IdentityBindingTests(unittest.TestCase):
    def test_certification_mismatch_order_is_stable_across_mapping_order(self) -> None:
        cases = (
            ({"actors", "worktree", "round_id", "lease_ownership"}, "round_id"),
            ({"binding_id", "world_save", "event_stream", "executable"}, "executable"),
            ({"world_save_sequence", "world_save_progression", "event_stream"}, "world_save_progression"),
            ({"lease_ownership", "event_stream", "world_save_sequence"}, "world_save_sequence"),
        )
        for values, expected in cases:
            self.assertEqual(first_certification_mismatch(iter(values)), expected)
            self.assertEqual(first_certification_mismatch(reversed(tuple(values))), expected)
        self.assertEqual(order_certification_mismatches(["actors", "actors", "world_save"]),
                         ["world_save", "actors"])

    def test_tree_digest_ignores_mtime_and_enumeration_order(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            (root / "b").write_bytes(b"B")
            (root / "a").write_bytes(b"A")
            first = byte_tree_digest(root, paths=[root / "b", root / "a"], domain="data")
            os.utime(root / "a", (1, 1))
            second = byte_tree_digest(root, paths=[root / "a", root / "b"], domain="data")
            self.assertEqual(first, second)

    def test_complete_binding_changes_independently_for_each_family(self) -> None:
        base = dict(
            worktree={"files": [{"path": "src/a", "sha256": "1"}]},
            data_config={"files": [{"path": "data/a", "sha256": "2"}]},
            world_save={"world": "w", "save": "s"},
            player={"id": "p", "save": "s"},
            actors=[{"actor_id": 1, "kind": "bandit"}],
        )
        original = complete_identity_binding(**base)["sha256"]
        for name in ("worktree", "data_config", "world_save", "player"):
            changed = dict(base)
            changed[name] = dict(changed[name], mutation="changed")
            self.assertNotEqual(original, complete_identity_binding(**changed)["sha256"], name)
        changed = dict(base, actors=[{"actor_id": 1, "kind": "changed"}])
        self.assertNotEqual(original, complete_identity_binding(**changed)["sha256"])

    def test_actor_order_is_canonical_and_duplicate_or_missing_ids_fail_closed(self) -> None:
        base = dict(worktree={"x": 1}, data_config={"x": 1}, world_save={"x": 1}, player={"id": "p"})
        left = complete_identity_binding(**base, actors=[{"actor_id": 2}, {"actor_id": 1}])
        right = complete_identity_binding(**base, actors=[{"actor_id": 1}, {"actor_id": 2}])
        self.assertEqual(left, right)
        with self.assertRaises(IdentityBindingError):
            complete_identity_binding(**base, actors=[{"actor_id": 1}, {"actor_id": 1}])
        with self.assertRaises(IdentityBindingError):
            complete_identity_binding(**base, actors=[{"kind": "bandit"}])

    def test_excluded_run_outputs_do_not_change_tree_identity(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            (root / "payload.json").write_bytes(b"payload")
            (root / "harness_runs").mkdir()
            (root / "harness_runs" / "report.json").write_bytes(b"run output")
            first = byte_tree_digest(root, domain="world")
            (root / "harness_runs" / "report.json").write_bytes(b"different output")
            self.assertEqual(first, byte_tree_digest(root, domain="world"))

    def test_git_worktree_identity_is_content_based(self) -> None:
        root = Path(__file__).resolve().parents[2]
        identity = git_worktree_identity(root, ["tools/openclaw_harness/identity_binding.py"])
        self.assertEqual(len(identity["files"]), 1)
        self.assertEqual(identity["files"][0]["path"], "tools/openclaw_harness/identity_binding.py")
        self.assertTrue(identity["sha256"])

    def test_authoritative_producer_binds_all_required_inputs(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            for name, content in (("game", b"exe"), ("scenario", b"scenario"), ("fixture", b"fixture"), ("profile", b"profile")):
                (root / name).write_bytes(content)
            data = root / "data"; data.mkdir(); (data / "config.json").write_bytes(b"config")
            harness = root / "harness"; harness.mkdir(); (harness / "runner.py").write_bytes(b"runner")
            world = root / "world"; world.mkdir(); (world / "player.sav").write_bytes(b"save")
            kwargs = dict(
                repo_root=Path(__file__).resolve().parents[2], executable=root / "game",
                runtime_paths=["tools/openclaw_harness/identity_binding.py"], data_config_roots=[data],
                harness_roots=[harness], scenario_path=root / "scenario", fixture_path=root / "fixture",
                profile_path=root / "profile", world_dir=world, player_save="player.sav",
                saved_player_payload={"player": {"id": "p", "name": "Player"}},
                ecology_audit={"actors": [{"actor_id": 2}, {"actor_id": 1}]},
            )
            first = authoritative_identity_binding(**kwargs)
            (root / "world" / "player.sav").write_bytes(b"replacement")
            second = authoritative_identity_binding(**kwargs)
            self.assertNotEqual(first["sha256"], second["sha256"])
            self.assertEqual(first["authoritative_components"]["actors"], [{"actor_id": 1}, {"actor_id": 2}])

    def test_authoritative_producer_rejects_missing_actor_identity(self) -> None:
        with self.assertRaises(IdentityBindingError):
            from identity_binding import ecology_actor_identity
            ecology_actor_identity({"actors": [{"kind": "bandit"}]})

    def test_each_authoritative_component_has_independent_mutation_sensitivity(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            for name, content in (("game", b"exe"), ("scenario", b"scenario"), ("fixture", b"fixture"), ("profile", b"profile")):
                (root / name).write_bytes(content)
            data = root / "data"; data.mkdir(); (data / "config.json").write_bytes(b"config")
            harness = root / "harness"; harness.mkdir(); (harness / "runner.py").write_bytes(b"runner")
            world = root / "world"; world.mkdir(); (world / "player.sav").write_bytes(b"save")
            repo = root / "repo"; repo.mkdir(); (repo / "tracked.txt").write_bytes(b"tracked")
            subprocess.run(["git", "init", "-q", str(repo)], check=True)
            subprocess.run(["git", "-C", str(repo), "add", "tracked.txt"], check=True)
            subprocess.run(["git", "-C", str(repo), "-c", "user.name=test", "-c", "user.email=test@example.invalid", "commit", "-qm", "base"], check=True)
            kwargs = dict(
                repo_root=repo, executable=root / "game", runtime_paths=["tracked.txt"],
                data_config_roots=[data], harness_roots=[harness], scenario_path=root / "scenario",
                fixture_path=root / "fixture", profile_path=root / "profile", world_dir=world,
                player_save="player.sav", saved_player_payload={"player": {"id": "p"}},
                ecology_audit={"actors": [{"actor_id": 1}]},
            )
            baseline = authoritative_identity_binding(**kwargs)["sha256"]
            mutations = {
                "worktree": lambda: (repo / "tracked.txt").write_bytes(b"changed"),
                "executable": lambda: (root / "game").write_bytes(b"changed"),
                "data_config": lambda: (data / "config.json").write_bytes(b"changed"),
                "harness": lambda: (harness / "runner.py").write_bytes(b"changed"),
                "scenario": lambda: (root / "scenario").write_bytes(b"changed"),
                "fixture": lambda: (root / "fixture").write_bytes(b"changed"),
                "profile": lambda: (root / "profile").write_bytes(b"changed"),
                "world_save": lambda: (world / "player.sav").write_bytes(b"changed"),
                "player": lambda: kwargs.update(saved_player_payload={"player": {"id": "replacement"}}),
                "actors": lambda: kwargs.update(ecology_audit={"actors": [{"actor_id": 2}]}),
            }
            for name, mutate in mutations.items():
                mutate()
                self.assertNotEqual(baseline, authoritative_identity_binding(**kwargs)["sha256"], name)
                # Restore the exact baseline inputs for the next independent control.
                if name == "worktree": (repo / "tracked.txt").write_bytes(b"tracked")
                elif name == "executable": (root / "game").write_bytes(b"exe")
                elif name == "data_config": (data / "config.json").write_bytes(b"config")
                elif name == "harness": (harness / "runner.py").write_bytes(b"runner")
                elif name == "scenario": (root / "scenario").write_bytes(b"scenario")
                elif name == "fixture": (root / "fixture").write_bytes(b"fixture")
                elif name == "profile": (root / "profile").write_bytes(b"profile")
                elif name == "world_save": (world / "player.sav").write_bytes(b"save")
                elif name == "player": kwargs["saved_player_payload"] = {"player": {"id": "p"}}
                elif name == "actors": kwargs["ecology_audit"] = {"actors": [{"actor_id": 1}]}

    def test_authoritative_producer_rejects_missing_roots_files_and_deleted_tracked_path(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw); repo = root / "repo"; repo.mkdir(); (repo / "tracked").write_bytes(b"x")
            subprocess.run(["git", "init", "-q", str(repo)], check=True)
            subprocess.run(["git", "-C", str(repo), "add", "tracked"], check=True)
            subprocess.run(["git", "-C", str(repo), "-c", "user.name=test", "-c", "user.email=test@example.invalid", "commit", "-qm", "base"], check=True)
            (repo / "tracked").unlink()
            with self.assertRaises(IdentityBindingError):
                git_worktree_identity(repo, ["tracked"])
            with self.assertRaises(IdentityBindingError):
                file_identity(root / "missing", domain="missing")
            with self.assertRaises(IdentityBindingError):
                byte_tree_digest(root / "missing-root", domain="missing")

    def test_sealed_round_manifest_rechecks_all_identity_families_and_metadata(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw); repo = root / "repo"; repo.mkdir(); (repo / "tracked").write_bytes(b"base")
            subprocess.run(["git", "init", "-q", str(repo)], check=True)
            subprocess.run(["git", "-C", str(repo), "add", "tracked"], check=True)
            subprocess.run(["git", "-C", str(repo), "-c", "user.name=test", "-c", "user.email=test@example.invalid", "commit", "-qm", "base"], check=True)
            for name in ("game", "scenario", "fixture", "profile"):
                (root / name).write_bytes(name.encode())
            for name in ("data", "harness"):
                (root / name).mkdir(); (root / name / "input").write_bytes(name.encode())
            world = root / "world"; world.mkdir(); (world / "save").write_bytes(b"save")
            kwargs = dict(repo_root=repo, executable=root / "game", runtime_paths=["tracked"],
                          data_config_roots=[root / "data"], harness_roots=[root / "harness"],
                          scenario_path=root / "scenario", fixture_path=root / "fixture", profile_path=root / "profile",
                          world_dir=world, player_save="save", saved_player_payload={"player": {"id": "p"}},
                          ecology_audit={"actors": [{"actor_id": 1}]})
            ids = dict(round_id="round", scenario_lineage_id="lineage", authority_id="auth",
                       authority_kind="certification", event_stream_id="events")
            manifest = seal_complete_round_manifest(**ids, **kwargs)
            before = dict(manifest)
            self.assertEqual(compare_round_manifest(manifest, **ids, **kwargs), {"ok": True, "mismatches": []})
            for field, value in (("round_id", "other-round"), ("scenario_lineage_id", "other-lineage"),
                                 ("authority_id", "other-auth"), ("authority_kind", "diagnostic"),
                                 ("event_stream_id", "other-events")):
                changed = dict(ids, **{field: value})
                self.assertIn(field, compare_round_manifest(manifest, **changed, **kwargs)["mismatches"])
            for family, mutate in {
                "worktree": lambda: (repo / "tracked").write_bytes(b"changed"),
                "executable": lambda: (root / "game").write_bytes(b"changed"),
                "data_config": lambda: (root / "data" / "input").write_bytes(b"changed"),
                "harness": lambda: (root / "harness" / "input").write_bytes(b"changed"),
                "scenario": lambda: (root / "scenario").write_bytes(b"changed"),
                "fixture": lambda: (root / "fixture").write_bytes(b"changed"),
                "profile": lambda: (root / "profile").write_bytes(b"changed"),
                "world_save": lambda: (world / "save").write_bytes(b"changed"),
                "player": lambda: kwargs.update(saved_player_payload={"player": {"id": "other"}}),
                "actors": lambda: kwargs.update(ecology_audit={"actors": [{"actor_id": 2}]}),
            }.items():
                mutate(); self.assertIn(family, compare_round_manifest(manifest, **ids, **kwargs)["mismatches"])
                kwargs["saved_player_payload"] = {"player": {"id": "p"}}; kwargs["ecology_audit"] = {"actors": [{"actor_id": 1}]}
                (repo / "tracked").write_bytes(b"base"); (root / "game").write_bytes(b"game")
                for name in ("data", "harness"): (root / name / "input").write_bytes(name.encode())
                for name in ("scenario", "fixture", "profile"): (root / name).write_bytes(name.encode())
                (world / "save").write_bytes(b"save")
            self.assertEqual(dict(manifest), before)
            forged = dict(manifest, binding_id="forged")
            with self.assertRaises(RoundManifestError):
                compare_round_manifest(forged, **ids, **kwargs)
            with self.assertRaises(RoundManifestError):
                compare_round_manifest(dict(manifest, unexpected="field"), **ids, **kwargs)
            missing = dict(manifest); del missing["event_stream_id"]
            with self.assertRaises(RoundManifestError):
                compare_round_manifest(missing, **ids, **kwargs)
            with self.assertRaises(TypeError):
                seal_complete_round_manifest(**ids, binding_id="caller-forged", **kwargs)
            with self.assertRaises(TypeError):
                manifest["round_id"] = "forged"


if __name__ == "__main__":
    unittest.main()
