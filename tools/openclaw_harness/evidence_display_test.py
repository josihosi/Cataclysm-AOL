import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from evidence_display import bounded, encoded, page, recover, retain, DEFAULT_BYTES, exact_select
from gameplay_display import display
from cockpit_evidence import query, record_artifact
from evidence_events import parse, envelopes


class EvidenceDisplayTest(unittest.TestCase):
    def test_small_response_does_not_create_display_artifacts(self):
        value = {"ok": True, "state": "pending"}
        self.assertEqual(bounded(value, directory=self.root), value)
        self.assertEqual(list(self.root.iterdir()), [])

    def test_nested_omissions_retain_the_response_only_once(self):
        value = {"first": {"history": "a" * 100000}, "second": {"history": "b" * 100000}}
        output = bounded(value, directory=self.root)
        self.assertEqual(len(list(self.root.glob("*.json.gz"))), 1)
        self.assertLess(sum(p.stat().st_size for p in self.root.iterdir()), len(encoded(value)))
        for key in ("first", "second"):
            omitted = output[key]
            expected = value[key]
            if "omitted" not in omitted:
                omitted = omitted["history"]
                expected = expected["history"]
            self.assertTrue(omitted["omitted"])
            source = recover(omitted["evidence"]["sha256"], self.root)
            for part in omitted["evidence"]["path"]:
                source = source[part]
            self.assertEqual(source, expected)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def test_existing_uncompressed_snapshot_remains_readable_without_duplicate(self):
        value = {"history": "retained older evidence"}
        raw = encoded(value)
        digest = hashlib.sha256(raw).hexdigest()
        (self.root / (digest + ".json")).write_bytes(raw)
        self.assertEqual(recover(digest, self.root), value)
        self.assertEqual(retain(value, self.root)["sha256"], digest)
        self.assertEqual(len(list(self.root.iterdir())), 1)

    def test_compressed_snapshot_still_checks_content_integrity(self):
        import gzip
        digest = retain({"value": "original"}, self.root)["sha256"]
        (self.root / (digest + ".json.gz")).write_bytes(gzip.compress(b'{}'))
        with self.assertRaisesRegex(ValueError, "evidence_hash_mismatch"):
            recover(digest, self.root)

    def test_single_fields_objects_metadata_and_errors_are_bounded_and_exact(self):
        values = [{"ok": True, "fields": {"text": "雪\\\"" * 100000}},
                  {"error": "bad" * 100000}, {"x" * 100000: list(range(300))},
                  {"controls": [{"label": "x" * 100000}]},
                  {"avatar": {str(i): "v" * 1000 for i in range(1000)}}]
        for value in values:
            output = bounded(value, directory=self.root)
            self.assertLessEqual(len(encoded(output)) + 1, DEFAULT_BYTES)
            self.assertEqual(recover(output["presentation"]["full_evidence"]["sha256"], self.root), value)
        value = "雪\\\"" * 1000
        digest = retain(value, self.root)["sha256"]
        reconstructed, offset = "", 0
        while True:
            result = page(digest, offset=offset, limit=101, directory=self.root)
            reconstructed += result["value"]
            offset = result["page"]["next_offset"]
            if offset is None:
                break
        self.assertEqual(reconstructed, value)

    def test_exact_json_string_type_and_nested_change(self):
        self.assertEqual(exact_select({"text": '{"nested":1}'}, "text"), '{"nested":1}')
        first = self.response(facts={"avatar_status": {"observed_turn": 1, "health": {"hp": 100}}})
        _, state = display(first)
        after = self.response(facts={"avatar_status": {"observed_turn": 2, "health": {"hp": 100}}}, frame=2)
        shown, _ = display(after, state)
        self.assertEqual(shown["facts_changed"], {"avatar_status": {"observed_turn": 2}})

    def test_original_single_record_failure_and_stable_append_paging(self):
        path = self.root / "events.log"
        text = "x" * 100000
        raw = (json.dumps({"event": "test", "text": text}) + "\n").encode()
        path.write_bytes(raw + b'{"event":"test","text":"second"}\n')
        result = query([path], {"event": "test"}, ["text"], 0, 1)
        self.assertEqual(result["rows"][0]["record"]["text"], text)
        self.assertLessEqual(len(encoded(bounded(result, directory=self.root))) + 1, DEFAULT_BYTES)
        with path.open("ab") as stream:
            stream.write(b'{"event":"test","text":"third"}\n')
        second = query([path], {"event": "test"}, ["text"], 1, 1, snapshot=result["snapshot"])
        self.assertEqual(second["matched"], 2)
        self.assertIsNone(second["page"]["next_offset"])
        artifact = record_artifact(path, 0, len(raw), hashlib.sha256(raw).hexdigest(), [])
        self.assertEqual(artifact["record"]["text"], text)
        self.assertEqual(artifact["raw"].encode(), raw)
        self.assertLessEqual(len(encoded(bounded(artifact, directory=self.root))) + 1, DEFAULT_BYTES)
        path.write_bytes(b"replacement")
        self.assertFalse(query([path], {"event": "test"}, ["text"], 1, 1, snapshot=result["snapshot"])["ok"])

    def response(self, owner="world", facts=None, actions=None, frame=1):
        return {"ok": True, "observation": {"run_id": "run", "observation_id": str(frame),
                "surface_id": owner, "game_minutes": 42,
                "surface": {"kind": owner, "facts": facts or {"terrain": "floor"},
                            "actions": actions or [{"id": owner + ".cancel", "enabled": True}]}}}

    def test_owner_selection_removal_and_refresh_without_time(self):
        response = self.response()
        first, state = display(response)
        response["observation"]["observation_id"] = "2"
        same, state = display(response, state)
        self.assertEqual(same["facts_changed"], {})
        self.assertNotIn("actions", same["current_input"])
        menu, state = display(self.response("menu", {"selected_index": 0}), state)
        selected, state = display(self.response("menu", {"selected_index": 1}), state)
        self.assertEqual(selected["current_input"]["facts_changed"]["selected_index"], 1)
        closed, state = display(response, state)
        self.assertIn("actions", closed["current_input"])
        self.assertEqual(closed["facts_changed"], {})
        changed, state = display(self.response(facts={"new": 1}, actions=[{"id": "world.new"}]), state)
        self.assertEqual(changed["facts_removed"], ["terrain"])
        self.assertEqual(changed["current_input"]["actions_changed"], [{"id": "world.new"}])
        fresh, _ = display(self.response(), state, refresh=True)
        self.assertIn("terrain", fresh["facts_changed"])

    def test_display_keeps_selectors_and_changed_sixth_entity_ahead_of_unchanged_rows(self):
        entities = [{"id": str(index), "name": "unchanged", "hp": 10}
                    for index in range(6)]
        first = self.response(facts={"visible_entities": entities,
                                     "messages": [{"text": "old", "time": 1}]})
        shown, state = display(first)
        self.assertEqual(shown["current_input"]["source_selector"], "observation")
        self.assertEqual(shown["current_input"]["actions_selector"], "observation.surface.actions")
        updated = [dict(entity) for entity in entities]
        updated[5]["hp"] = 3
        second = self.response(facts={"visible_entities": updated,
                                      "messages": [{"text": "old", "time": 1},
                                                   {"text": "old", "time": 2, "actor": "Ada"}]}, frame=2)
        changed, _ = display(second, state)
        entities_delta = changed["facts_changed"]["visible_entities"]
        self.assertEqual(entities_delta["selector"], "observation.surface.facts.visible_entities")
        self.assertEqual(entities_delta["changed"][0]["identity"], {"id": "5"})
        self.assertEqual(entities_delta["changed"][0]["value"]["hp"], 3)
        messages = changed["facts_changed"]["messages"]
        self.assertEqual(messages["cursor"]["mode"], "append")
        self.assertEqual(messages["new_events"], [{"source_index": 1,
                                                     "value": {"text": "old", "time": 2, "actor": "Ada"}}])

    def test_display_pages_a_large_owner_catalog_without_dropping_navigation(self):
        actions = []
        for index in range(8):
            actions.append({"id": "inventory.select", "stable_id": str(index),
                            "label": f"item {index}", "enabled": True})
        actions.append({"id": "inventory.cancel", "enabled": True})
        shown, _ = display(self.response("inventory", {"title": "Inventory"}, actions))
        catalog = shown["current_input"]["actions"]
        self.assertEqual(catalog["selector"], "observation.surface.actions")
        self.assertEqual(catalog["target_count"], 8)
        self.assertEqual(catalog["controls"], [{"id": "inventory.cancel", "enabled": True}])
        self.assertEqual(catalog["next_offset"], 5)

    def test_chain_exposes_terminal_and_interruption_not_internal_observations(self):
        response = {"ok": False, "result": {"terminal_observation": self.response()["observation"],
                    "partial_progress": 3, "reason": "blocked", "native_receipts": list(range(100)),
                    "handled_interruptions": object()}}
        view, _ = display(response)
        self.assertEqual(view["outcome"]["chain"]["partial_progress"], 3)
        self.assertEqual(view["outcome"]["chain"]["reason"], "blocked")
        self.assertNotIn("native_receipts", view["outcome"]["chain"])
        self.assertNotIn("handled_interruptions", view["outcome"]["chain"])
        self.assertEqual(len(response["result"]["native_receipts"]), 100)

    def test_shared_npc_request_is_not_fabricated_process_correlation(self):
        record = parse(b'[CAOL_EVENT] action_status npc="A B" kind="follow" request="req_0"\n')
        source = {"producer": "npc", "sha256": "a" * 64, "offset": 0}
        event = next(envelopes(record, source))
        self.assertEqual(event["actor_name"], "A B")
        self.assertEqual(event["request_id"], "req_0")
        self.assertIsNone(event["process_instance"])
        self.assertIsNone(event["actor_id"])
        self.assertIsNone(event["run_id"])


if __name__ == "__main__":
    unittest.main()
