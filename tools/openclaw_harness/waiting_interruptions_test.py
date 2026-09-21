import re
import unittest
from pathlib import Path

from waiting_interruptions import continuation


class InterruptionPolicyTest(unittest.TestCase):
    def test_all_native_distraction_types_have_a_policy(self):
        expected = {"noise", "pain", "attacked", "hostile_spotted_far", "hostile_spotted_near",
                    "talked_to", "asthma", "motion_alarm", "weather_change", "portal_storm_popup",
                    "eoc", "dangerous_field", "hunger", "thirst", "temperature", "mutation",
                    "oxygen", "withdrawal", "craft_step_complete"}
        source = (Path(__file__).resolve().parents[2] / "src/enums.h").read_text()
        body = re.search(r"enum class distraction_type : int \{(.*?)\};", source, re.S).group(1)
        self.assertEqual(set(re.findall(r"\b(\w+)\s*,", body)) - {"last"}, expected)
        for distraction in sorted(expected):
            with self.subTest(distraction=distraction):
                portal = distraction == "portal_storm_popup"
                actions = [{"id": "prompt.choose", "label": label, "stable_id": str(i)}
                           for i, label in enumerate(["YES0", "YES1", "YES2"] if portal else
                                                     ["YES", "NO", "IGNORE", "MANAGER"])]
                surface = {"kind": "prompt", "facts": {
                    "title": "YES_QUERY" if portal else "CANCEL_ACTIVITY_OR_IGNORE_QUERY",
                    "text": "A translated message without English classification keywords"}, "actions": actions}
                self.assertEqual(continuation(surface, "ignore")["label"], "YES0" if portal else "IGNORE")
                self.assertIsNone(continuation(surface, "safe"))
                self.assertIsNone(continuation(surface, "stop"))

    def test_native_activity_owner_and_disabled_choice(self):
        surface = {"kind": "activity_distraction", "actions": [{"id": "activity.ignore"}]}
        self.assertEqual(continuation(surface, "ignore")["id"], "activity.ignore")
        surface["actions"][0]["enabled"] = False
        self.assertIsNone(continuation(surface, "ignore"))

    def test_wait_stop_query_does_not_authorize_unrelated_yes_no(self):
        surface = {"kind": "prompt", "facts": {"title": "YESNO"}, "actions": [
            {"id": "prompt.choose", "label": "YES"}, {"id": "prompt.choose", "label": "NO"}]}
        self.assertIsNone(continuation(surface, "ignore"))
        surface["breadcrumbs"] = ["Activity in progress", "YESNO"]
        self.assertEqual(continuation(surface, "ignore")["label"], "NO")

    def test_notification_and_unknown_choice(self):
        for action in ("prompt.cancel", "prompt.acknowledge", "modal.acknowledge"):
            surface = {"kind": "prompt", "actions": [{"id": action}]}
            self.assertIsNotNone(continuation(surface, "safe"))
            self.assertIsNone(continuation(surface, "stop"))
            surface["actions"].append({"id": "prompt.choose", "label": "Buy"})
            self.assertIsNone(continuation(surface, "ignore"))


if __name__ == "__main__":
    unittest.main()
