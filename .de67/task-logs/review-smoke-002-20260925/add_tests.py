from pathlib import Path
r=Path(__file__).resolve().parent;p=r/'method-candidate/tests/test_policy_kernel.py';s=p.read_text()
marker='    def test_successor_packet_uses_compact_receipt_and_reasoned_read_plan(self) -> None:\n'
new='''    def test_owner_assignment_context_is_scoped_at_dispatch_boundary(self) -> None:
        import coordinator_supervisor
        import worker_library
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            de67 = workspace / ".de67"
            de67.mkdir()
            (de67 / "work-ledger.md").write_text(
                "- [ ] R-SCOPE — Complete smoke ecology.\\n"
                "  - DFS slices: `R-SCOPE-S001`\\n"
                "  - Assignment scope: Implement and verify the assigned repair.\\n")
            (de67 / "FS.md").write_text(
                "<!-- DE67:DFS-SLICE:BEGIN id=R-SCOPE-S001 claim=R-SCOPE -->\\n"
                "- Acceptance: Preserve smoke ecology and real movement.\\n"
                "<!-- DE67:DFS-SLICE:END id=R-SCOPE-S001 claim=R-SCOPE -->\\n")
            source = de67 / "WEC.md"
            source.write_text("<!-- DE67:OWNER-CONTRACT:BEGIN -->\\n"
                "Astra owns implementation; Luna alone operates live harness playtests.\\n"
                "<!-- DE67:COORDINATOR-ONLY:BEGIN -->\\n"
                "Coordinator: assign Astra for this coupled implementation.\\n"
                "<!-- DE67:COORDINATOR-ONLY:END -->\\n"
                "Keep actual actors, save ownership and optional useful helpers.\\n"
                "<!-- DE67:OWNER-CONTRACT:END -->\\n")
            state = workspace / "state.sqlite3"
            with DeadlineHarness(state) as harness:
                harness.start_task("project", "scope", "R-SCOPE", 100, now=1)
            call = kernel.unbound_worker_spawns(workspace, state, "project")[0]
            packet = Path(call["dispatch_packet"]["path"])
            worker = packet.read_text()
            coordinator = coordinator_supervisor.coordinator_prompt(
                workspace, state, "project", "test-review", None)
            self.assertIn("Coordinator: assign Astra", coordinator)
            self.assertNotIn("Coordinator: assign Astra", worker)
            for text in (worker, coordinator):
                self.assertIn("Astra owns implementation", text)
                self.assertIn("Luna alone operates live harness", text)
                self.assertIn("optional useful helpers", text)
            self.assertIn(kernel.worker_helper_contract(), worker)
            digest = hashlib.sha256(packet.read_bytes()).hexdigest()
            worker_library._validate_packet(workspace, state, "project", "scope", packet, digest)
            source.write_text(source.read_text().replace("Keep actual actors", "Protect actual actors"))
            with self.assertRaisesRegex(worker_library.WorkerLibraryError, "Owner instructions changed"):
                worker_library._validate_packet(workspace, state, "project", "scope", packet, digest)

    def test_owner_audience_markers_fail_closed(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            (workspace / ".de67").mkdir()
            for section in (
                "<!-- DE67:COORDINATOR-ONLY:BEGIN -->missing end",
                "<!-- DE67:COORDINATOR-ONLY:END -->reversed<!-- DE67:COORDINATOR-ONLY:BEGIN -->",
                "<!-- DE67:COORDINATOR-ONLY:BEGIN --><!-- DE67:COORDINATOR-ONLY:END -->",
            ):
                (workspace / ".de67/WEC.md").write_text(
                    "<!-- DE67:OWNER-CONTRACT:BEGIN -->\\n" + section +
                    "\\n<!-- DE67:OWNER-CONTRACT:END -->")
                for audience in ("coordinator", "worker"):
                    with self.assertRaises(kernel.PolicyError):
                        kernel.current_owner_contract(workspace, audience=audience)

'''
assert marker in s;s=s.replace(marker,new+marker);p.write_text(s)
